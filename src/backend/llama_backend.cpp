#include "prisminfer/backend.h"
#include "prisminfer/flat_json.h"
#include "prisminfer/native_worker.h"
#include "prisminfer/model_sidecar.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <optional>
#include <vector>

namespace prisminfer {
namespace {

bool path_has_directory(const std::filesystem::path& path) {
  return path.has_parent_path() || path.is_absolute();
}

std::filesystem::path resolve_llama_executable(const RuntimeConfig& config) {
  return config.llama_executable_path;
}

std::filesystem::path resolve_workspace_artifact(
    const std::filesystem::path& relative_path) {
  auto cursor = std::filesystem::path(PRISMINFER_SOURCE_DIR);
  for (;;) {
    const auto candidate = cursor / relative_path;
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error) && !error) {
      return candidate;
    }
    const auto parent = cursor.parent_path();
    if (parent == cursor || parent.empty()) return {};
    cursor = parent;
  }
}

std::optional<NativeWorkerTrustCatalog> load_llama_approval_catalog() {
  const auto approval_path = std::filesystem::path(PRISMINFER_SOURCE_DIR) /
      "third_party" / "llama.cpp-executable-approval.json";
  const auto approval = read_flat_json_file(approval_path, 64U * 1024U);
  const auto schema = approval.fields.find("schema_version");
  const auto status = approval.fields.find("approval_status");
  if (!approval.ok || schema == approval.fields.end() ||
      schema->second.text != "0.1" || status == approval.fields.end() ||
      status->second.text != "approved") {
    return std::nullopt;
  }
  const auto executable = approval.fields.find("workspace_relative_executable");
  const auto hash = approval.fields.find("sha256");
  const auto identity = approval.fields.find("approval_identity");
  const auto dependency_count = approval.fields.find("dependency_count");
  const auto shared_libraries = approval.fields.find("shared_libraries");
  const auto dynamic_loading =
      approval.fields.find("dynamic_backend_loading");
  const auto cuda = approval.fields.find("cuda");
  const auto openssl = approval.fields.find("openssl");
  const auto import_policy = approval.fields.find("pe_import_policy");
  if (executable == approval.fields.end() || hash == approval.fields.end() ||
      identity == approval.fields.end() ||
      dependency_count == approval.fields.end() ||
      shared_libraries == approval.fields.end() ||
      shared_libraries->second.text != "disabled" ||
      dynamic_loading == approval.fields.end() ||
      dynamic_loading->second.text != "disabled" ||
      cuda == approval.fields.end() || cuda->second.text != "disabled" ||
      openssl == approval.fields.end() || openssl->second.text != "disabled" ||
      import_policy == approval.fields.end() ||
      import_policy->second.text != "system-dlls-only" ||
      identity->second.text.empty()) {
    return std::nullopt;
  }
  const auto executable_path =
      resolve_workspace_artifact(executable->second.text);
  if (executable_path.empty()) return std::nullopt;
  if (dependency_count->second.text != "0") return std::nullopt;
  return NativeWorkerTrustCatalog({{
      executable_path, executable_path.parent_path(), hash->second.text,
      identity->second.text}});
}

struct PacketAArtifactApproval {
  std::string filename;
  std::string sha256;
  std::uint64_t bytes{0};
  std::string identity;
};

std::optional<PacketAArtifactApproval> load_packet_a_artifact_approval() {
  const auto record_path = std::filesystem::path(PRISMINFER_SOURCE_DIR) /
      "configs" / "model-artifacts" /
      "llama-3.1-8b-instruct-q4-k-m" / "artifact-record.json";
  const auto record = read_flat_json_file(record_path, 64U * 1024U);
  const auto version = record.fields.find("record_version");
  const auto kind = record.fields.find("artifact_kind");
  const auto filename = record.fields.find("artifact_filename");
  const auto sha256 = record.fields.find("artifact_sha256");
  const auto bytes = record.fields.find("artifact_bytes");
  const auto license = record.fields.find("license_status");
  const auto claim = record.fields.find("claim_boundary");
  if (!record.ok || version == record.fields.end() ||
      version->second.text != "packet-a-artifact-v2" ||
      kind == record.fields.end() || kind->second.text != "gguf" ||
      filename == record.fields.end() || filename->second.text.empty() ||
      sha256 == record.fields.end() || sha256->second.text.size() != 64U ||
      bytes == record.fields.end() ||
      bytes->second.kind != FlatJsonValue::Kind::Number ||
      license == record.fields.end() || license->second.text != "accepted" ||
      claim == record.fields.end() ||
      claim->second.text !=
          "CPU/offline artifact admission only; no inference, CUDA, calibration, benchmark, or performance claim") {
    return std::nullopt;
  }
  try {
    const auto approved_bytes = std::stoull(bytes->second.text);
    if (approved_bytes == 0U) return std::nullopt;
    return PacketAArtifactApproval{
        filename->second.text, sha256->second.text, approved_bytes,
        "packet-a:llama-3.1-8b-instruct-q4-k-m:" + sha256->second.text};
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::uint64_t parse_bytes(double value, const std::string& unit) {
  double multiplier = 1.0;
  if (unit == "KiB") {
    multiplier = 1024.0;
  } else if (unit == "MiB") {
    multiplier = 1024.0 * 1024.0;
  } else if (unit == "GiB") {
    multiplier = 1024.0 * 1024.0 * 1024.0;
  }
  if (value <= 0.0) {
    return 0;
  }
  return static_cast<std::uint64_t>(value * multiplier);
}

std::uint64_t extract_llama_allocation_bytes(
    const std::string& captured_output) {
  try {
  std::istringstream in(captured_output);

  std::uint64_t total = 0;
  std::string line;
  const std::regex size_pattern(
      R"(([0-9]+(?:\.[0-9]+)?)\s*(B|KiB|MiB|GiB))");
  while (std::getline(in, line)) {
    const bool allocation_line =
        line.find("buffer size") != std::string::npos ||
        line.find("llama_kv_cache: size =") != std::string::npos;
    if (!allocation_line) {
      continue;
    }
    auto begin = std::sregex_iterator(line.begin(), line.end(), size_pattern);
    const auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
      const double value = std::stod((*it)[1].str());
      total += parse_bytes(value, (*it)[2].str());
    }
  }
  return total;
  } catch (const std::exception&) {
    return 0;
  }
}

bool extract_llama_kv_profile(const std::string& captured_output,
                              const RuntimeConfig& config,
                              KvCacheProfile* profile) {
  try {
  if (profile == nullptr) {
    return false;
  }
  std::istringstream in(captured_output);
  std::uint32_t layer_count = 0;
  std::uint32_t kv_head_count = 0;
  std::uint32_t head_dim = 0;
  std::string line;
  while (std::getline(in, line)) {
    const auto equals = line.rfind('=');
    if (equals == std::string::npos) {
      continue;
    }
    std::string value_text = line.substr(equals + 1);
    const auto first_digit = value_text.find_first_of("0123456789");
    if (first_digit == std::string::npos) {
      continue;
    }
    value_text = value_text.substr(first_digit);
    const auto end_digit = value_text.find_first_not_of("0123456789");
    if (end_digit != std::string::npos) {
      value_text = value_text.substr(0, end_digit);
    }
    const auto value = static_cast<std::uint32_t>(std::stoul(value_text));
    if (line.find("llama.block_count") != std::string::npos ||
        line.find("n_layer               =") != std::string::npos) {
      layer_count = value;
    } else if (line.find("llama.attention.head_count_kv") !=
                   std::string::npos ||
               line.find("n_head_kv") != std::string::npos) {
      kv_head_count = value;
    } else if (line.find("llama.rope.dimension_count") != std::string::npos ||
               line.find("n_embd_head_k") != std::string::npos) {
      head_dim = value;
    }
  }
  KvCacheProfile extracted;
  const auto result = make_kv_cache_profile(
      layer_count, kv_head_count, head_dim, config.kv_block_tokens, 16, 16,
      &extracted);
  if (!result.ok) {
    return false;
  }
  *profile = extracted;
  return true;
  } catch (const std::exception&) {
    return false;
  }
}

struct WorkerOutputCleanup {
  std::filesystem::path path;
  ~WorkerOutputCleanup() {
    if (!path.empty()) {
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  }
};

class LlamaBackendAdapter final : public BackendAdapter {
 public:
  std::string name() const override { return "llama"; }
  std::string version() const override { return kBackendContractVersion; }

  BackendWarmupResult warmup(const RuntimeConfig& config,
                             CappedAllocatorTracker& allocator) override {
    BackendWarmupResult result;
    result.memory_sample = allocator.sample();
    const auto executable = resolve_llama_executable(config);
    if (executable.empty()) {
      result.ok = false;
      result.failure_reason = "llama_executable_required";
      return result;
    }
    if (config.model_path.empty()) {
      result.ok = false;
      result.failure_reason = "llama_model_required";
      return result;
    }
    if (!std::filesystem::exists(config.model_path)) {
      result.ok = false;
      result.failure_reason = "llama_model_not_found";
      return result;
    }
    const auto artifact_approval = load_packet_a_artifact_approval();
    if (!artifact_approval) {
      result.ok = false;
      result.failure_reason = "packet_a_artifact_approval_unavailable";
      return result;
    }
    const ModelSidecarValidationRequest model_validation_request{
        config.model_path, config.sidecar_path, config.max_model_bytes,
        config.max_sidecar_bytes};
    const auto model_validation =
        validate_model_sidecar(model_validation_request);
    if (!model_validation.valid || model_validation.skipped ||
        model_validation.model_sha256 != artifact_approval->sha256 ||
        model_validation.model_size_bytes != artifact_approval->bytes ||
        model_validation.normalized_model_path.filename().string() !=
            artifact_approval->filename) {
      result.ok = false;
      result.failure_reason = "packet_a_artifact_identity_mismatch";
      return result;
    }
    if (path_has_directory(executable) && !std::filesystem::exists(executable)) {
      result.ok = false;
      result.failure_reason = "llama_executable_not_found";
      return result;
    }
    const auto catalog = load_llama_approval_catalog();
    if (!catalog.has_value()) {
      result.ok = false;
      result.failure_reason = "llama_executable_approval_unavailable";
      return result;
    }

    std::vector<std::wstring> arguments{
        L"cli", L"--model", config.model_path.wstring(),
        L"--ctx-size", std::to_wstring(config.context_tokens),
        L"--n-predict", std::to_wstring(config.warmup_tokens),
        L"--prompt", L"PrismInfer backend warmup.",
        L"--no-display-prompt", L"--no-conversation", L"--single-turn",
        L"--log-verbose", L"--gpu-layers", std::to_wstring(config.gpu_layers)};
    if (!config.mmap_enabled) {
      arguments.emplace_back(L"--no-mmap");
    }
    NativeWorkerRequest request;
    request.executable_path = executable;
    request.arguments = std::move(arguments);
    request.timeout_ms = 60'000U;
    request.approved_read_only_inputs.push_back({
        model_validation.normalized_model_path,
        model_validation.normalized_model_path.parent_path(),
        artifact_approval->sha256, artifact_approval->identity});
    const auto worker = run_native_worker(*catalog, request);
    result.worker_evidence_available = worker.evidence_available;
    result.worker_executable_sha256 = worker.executable_sha256;
    result.worker_approval_identity = worker.approval_identity;
    result.worker_exit_code = worker.exit_code;
    result.worker_timed_out = worker.timed_out;
    result.worker_root_process_id = worker.root_process_id;
    result.worker_job_identity = worker.job_identity;
    result.worker_job_total_processes = worker.job_total_processes;
    result.worker_job_peak_active_processes = worker.job_peak_active_processes;
    result.worker_job_total_terminated_processes =
        worker.job_total_terminated_processes;
    result.worker_root_peak_working_set_bytes =
        worker.root_peak_working_set_bytes;
    result.worker_root_peak_private_commit_bytes =
        worker.root_peak_private_commit_bytes;
    result.worker_tree_peak_working_set_bytes =
        worker.tree_peak_working_set_bytes;
    result.worker_tree_peak_private_commit_bytes =
        worker.tree_peak_private_commit_bytes;
    result.worker_tree_sample_interval_milliseconds =
        worker.tree_sample_interval_milliseconds;
    result.worker_read_bytes = worker.read_bytes;
    result.worker_write_bytes = worker.write_bytes;
    result.worker_output_bytes = worker.output_bytes;
    result.worker_output_limit_bytes = worker.output_limit_bytes;
    if (!worker.ok) {
      result.ok = false;
      result.failure_reason = "llama_" + worker.failure_reason;
      return result;
    }
    WorkerOutputCleanup output_cleanup{worker.output_path};

    result.backend_external_peak_bytes =
        extract_llama_allocation_bytes(worker.captured_output);
    result.kv_profile_available =
        extract_llama_kv_profile(worker.captured_output, config,
                                 &result.kv_profile);
    if (result.backend_external_peak_bytes == 0) {
      result.ok = false;
      result.failure_reason = "llama_allocation_evidence_missing";
      return result;
    }
    return result;
  }
};

}  // namespace

std::unique_ptr<BackendAdapter> make_llama_backend_adapter() {
  return std::make_unique<LlamaBackendAdapter>();
}

}  // namespace prisminfer

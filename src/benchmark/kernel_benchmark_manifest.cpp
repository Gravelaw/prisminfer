#include "prisminfer/kernel_benchmark_manifest.h"

#include <charconv>
#include <climits>
#include <cmath>
#include <sstream>

#include "prisminfer/flat_json.h"
#include "prisminfer/kernel_benchmark_manifest_schema.h"

namespace prisminfer {

namespace {

constexpr std::uint64_t kMaxKernelManifestBytes = 64ULL * 1024ULL;

KernelBenchmarkManifestResult fail(const std::string& reason) {
  KernelBenchmarkManifestResult result;
  result.error = reason;
  return result;
}

bool parse_u64(const FlatJsonValue& value, std::uint64_t* output) {
  if (value.kind != FlatJsonValue::Kind::Number || value.text.empty() ||
      value.text.find_first_of(".-+eE") != std::string::npos) {
    return false;
  }
  const char* begin = value.text.data();
  const char* end = value.text.data() + value.text.size();
  const auto result = std::from_chars(begin, end, *output);
  return result.ec == std::errc{} && result.ptr == end;
}

bool parse_u32(const FlatJsonValue& value, std::uint32_t* output) {
  std::uint64_t parsed = 0;
  if (!parse_u64(value, &parsed) || parsed > UINT32_MAX) {
    return false;
  }
  *output = static_cast<std::uint32_t>(parsed);
  return true;
}

bool parse_double(const FlatJsonValue& value, double* output) {
  if (value.kind != FlatJsonValue::Kind::Number) {
    return false;
  }
  std::istringstream in(value.text);
  in >> *output;
  return in && in.eof() && std::isfinite(*output);
}

bool parse_bool(const FlatJsonValue& value, bool* output) {
  if (value.kind != FlatJsonValue::Kind::Boolean) {
    return false;
  }
  *output = value.text == "true";
  return true;
}

bool parse_string(const FlatJsonValue& value, std::string* output) {
  if (value.kind != FlatJsonValue::Kind::String) {
    return false;
  }
  *output = value.text;
  return true;
}

bool parse_non_empty_string(const FlatJsonValue& value, std::string* output) {
  return parse_string(value, output) && !output->empty();
}

template <typename T, typename Parser, typename Constraint>
bool validate_optional_field(const std::map<std::string, FlatJsonValue>& fields,
                             const char* key,
                             Parser parser,
                             Constraint constraint,
                             std::string* error) {
  const auto found = fields.find(key);
  if (found == fields.end()) {
    return true;
  }
  T value{};
  if (!parser(found->second, &value) || !constraint(value)) {
    *error = std::string("invalid_field:") + key;
    return false;
  }
  return true;
}

bool validate_optional_schema_fields(
    const std::map<std::string, FlatJsonValue>& fields, std::string* error) {
  const auto any_bool = [](bool) { return true; };
  const auto any_string = [](const std::string&) { return true; };
  const auto nonnegative_u64 = [](std::uint64_t) { return true; };
  const auto nonnegative_double = [](double value) { return value >= 0.0; };
  const auto unit_interval = [](double value) {
    return value >= 0.0 && value <= 1.0;
  };

  for (const char* key : {"dequant_fused", "tensor_core_claimed",
                          "tensor_core_used"}) {
    if (!validate_optional_field<bool>(fields, key, parse_bool, any_bool,
                                       error)) {
      return false;
    }
  }
  for (const char* key : {"workspace_bytes", "transfer_h2d_bytes",
                          "transfer_d2h_bytes", "group_size",
                          "residual_fp_window_tokens", "rotation_seed",
                          "qjl_residual_bits", "dequant_workspace_peak_bytes"}) {
    if (!validate_optional_field<std::uint64_t>(fields, key, parse_u64,
                                                nonnegative_u64, error)) {
      return false;
    }
  }
  for (const char* key : {"kernel_ms", "launch_ms", "sync_ms",
                          "payload_bits_per_value"}) {
    if (!validate_optional_field<double>(fields, key, parse_double,
                                         nonnegative_double, error)) {
      return false;
    }
  }
  if (!validate_optional_field<double>(fields, "attention_topk_overlap",
                                       parse_double, unit_interval, error)) {
    return false;
  }
  for (const char* key : {"profiler_artifact_path", "profiler_artifact_sha256",
                          "quantization_scope", "key_quant_axis",
                          "value_quant_axis", "pre_rope_or_post_rope",
                          "outlier_policy", "rotation_policy",
                          "projection_policy"}) {
    if (!validate_optional_field<std::string>(fields, key, parse_string,
                                              any_string, error)) {
      return false;
    }
  }
  return true;
}

template <typename T, typename Parser>
bool read_required(const std::map<std::string, FlatJsonValue>& fields,
                   const std::string& key,
                   T* output,
                   Parser parser,
                   std::string* error) {
  const auto found = fields.find(key);
  if (found == fields.end()) {
    *error = "missing_required_field:" + key;
    return false;
  }
  if (!parser(found->second, output)) {
    *error = "invalid_field:" + key;
    return false;
  }
  return true;
}

template <typename T, typename Parser>
bool read_optional(const std::map<std::string, FlatJsonValue>& fields,
                   const std::string& key,
                   T* output,
                   Parser parser,
                   std::string* error) {
  const auto found = fields.find(key);
  if (found == fields.end()) {
    return true;
  }
  if (!parser(found->second, output)) {
    *error = "invalid_field:" + key;
    return false;
  }
  return true;
}

}  // namespace

KernelBenchmarkManifestResult read_kernel_benchmark_manifest(
    const std::filesystem::path& path) {
  const auto json = read_flat_json_file(path, kMaxKernelManifestBytes);
  if (!json.ok) {
    return fail(json.error);
  }
  const auto& fields = json.fields;
  for (const auto& [key, value] : fields) {
    (void)value;
    if (kernel_manifest_allowed_keys().find(key) ==
        kernel_manifest_allowed_keys().end()) {
      return fail("unknown_field:" + key);
    }
  }
  for (const auto& key : kernel_manifest_required_keys()) {
    if (fields.find(key) == fields.end()) {
      return fail("missing_required_field:" + key);
    }
  }

  std::string error;
  if (!validate_optional_schema_fields(fields, &error)) {
    return fail(error);
  }

  KernelBenchmarkManifest manifest;
  if (!read_required(fields, "manifest_version", &manifest.manifest_version,
                     parse_non_empty_string, &error) ||
      manifest.manifest_version != "0.1") {
    return fail(error.empty() ? "invalid_field:manifest_version" : error);
  }
  if (!read_required(fields, "validation_cell_id", &manifest.validation_cell_id,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "model_hash", &manifest.cell.model_hash,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "quantization_format",
                     &manifest.cell.quantization_format,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "quant_artifact_sha256",
                     &manifest.cell.quant_artifact_sha256,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "context_tokens", &manifest.cell.context_tokens,
                     parse_u64, &error) ||
      !read_required(fields, "batch_size", &manifest.cell.batch_size,
                     parse_u32, &error) ||
      !read_required(fields, "prompt_fixture_hash",
                     &manifest.cell.prompt_fixture_hash,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "backend", &manifest.cell.backend,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "os", &manifest.cell.os,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "gpu_name", &manifest.cell.gpu_name,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "driver_mode", &manifest.cell.driver_mode,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "cuda_driver_version",
                     &manifest.cell.cuda_driver_version, parse_u32, &error) ||
      !read_required(fields, "cuda_runtime_version",
                     &manifest.cell.cuda_runtime_version, parse_u32, &error) ||
      !read_required(fields, "vram_tier_gib", &manifest.cell.vram_tier_gib,
                     parse_u32, &error) ||
      !read_required(fields, "hard_cap_bytes", &manifest.cell.hard_cap_bytes,
                     parse_u64, &error) ||
      !read_required(fields, "op_type", &manifest.cell.op_type,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "sequence_phase", &manifest.cell.sequence_phase,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "kernel_backend", &manifest.cell.kernel_backend,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "kernel_name", &manifest.cell.kernel_name,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "kernel_version", &manifest.cell.kernel_version,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "baseline_backend", &manifest.baseline_backend,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "baseline_manifest_hash",
                     &manifest.baseline_manifest_hash,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "correctness_fixture_hash",
                     &manifest.correctness_fixture_hash,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "quality_fixture_hash",
                     &manifest.quality_fixture_hash, parse_non_empty_string,
                     &error) ||
      !read_required(fields, "full_dequant_materialized",
                     &manifest.full_dequant_materialized, parse_bool, &error) ||
      !read_required(fields, "workspace_peak_bytes",
                     &manifest.workspace_peak_bytes, parse_u64, &error) ||
      !read_required(fields, "speedup_ratio", &manifest.speedup_ratio,
                     parse_double, &error) ||
      !read_required(fields, "claim_status", &manifest.claim_status,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "compression_status", &manifest.compression_status,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "quality_gate_id", &manifest.quality_gate_id,
                     parse_non_empty_string, &error) ||
      !read_required(fields, "cap_certification_status",
                     &manifest.cap_certification_status,
                     parse_non_empty_string, &error)) {
    return fail(error);
  }

  if (!kernel_manifest_identity_constraints_ok(manifest)) {
    return fail("manifest_schema_constraint_failed");
  }
  if (manifest.full_dequant_materialized) {
    return fail("full_dequant_materialized");
  }

  if (!read_optional(fields, "tdr_status", &manifest.tdr_status,
                     parse_non_empty_string, &error) ||
      !read_optional(fields, "application_control_status",
                     &manifest.application_control_status,
                     parse_non_empty_string, &error) ||
      !read_optional(fields, "compression_profile_id",
                     &manifest.compression_profile_id, parse_string, &error) ||
      !read_optional(fields, "algorithm_family", &manifest.algorithm_family,
                     parse_string, &error) ||
      !read_optional(fields, "effective_bits_per_value",
                     &manifest.effective_bits_per_value, parse_double,
                     &error) ||
      !read_optional(fields, "metadata_bits_per_value",
                     &manifest.metadata_bits_per_value, parse_double, &error) ||
      !read_optional(fields, "kv_payload_bytes", &manifest.kv_payload_bytes,
                     parse_u64, &error) ||
      !read_optional(fields, "kv_metadata_bytes", &manifest.kv_metadata_bytes,
                     parse_u64, &error) ||
      !read_optional(fields, "kv_residual_or_sketch_bytes",
                     &manifest.kv_residual_or_sketch_bytes, parse_u64,
                     &error) ||
      !read_optional(fields, "compression_decode_overhead_ms",
                     &manifest.compression_decode_overhead_ms, parse_double,
                     &error) ||
      !read_optional(fields, "attention_logit_error_p95",
                     &manifest.attention_logit_error_p95, parse_double,
                     &error) ||
      !read_optional(fields, "attention_logit_error_p99",
                     &manifest.attention_logit_error_p99, parse_double,
                     &error) ||
      !read_optional(fields, "attention_topk_overlap",
                     &manifest.attention_topk_overlap, parse_double, &error) ||
      !read_optional(fields, "quality_result_path",
                     &manifest.quality_result_path, parse_string, &error)) {
    return fail(error);
  }

  if (!kernel_manifest_optional_constraints_ok(manifest)) {
    return fail("manifest_schema_constraint_failed");
  }
  if ((manifest.compression_status == "reference" ||
       manifest.compression_status == "experimental") &&
      (manifest.effective_bits_per_value <= 0.0 ||
       manifest.metadata_bits_per_value <= 0.0 ||
       manifest.kv_payload_bytes == 0 || manifest.kv_metadata_bytes == 0 ||
       manifest.compression_decode_overhead_ms <= 0.0 ||
       manifest.quality_result_path.empty())) {
    return fail("compression_evidence_required");
  }

  KernelBenchmarkManifestResult result;
  result.ok = true;
  result.manifest = manifest;
  return result;
}

}  // namespace prisminfer

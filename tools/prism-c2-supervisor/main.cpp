#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <system_error>
#include <vector>

#include "prisminfer/c2_clearance_receipt.h"
#include "prisminfer/c2_case_policy.h"
#include "prisminfer/gpu_admission_session.h"
#include "prisminfer/gpu_thermal.h"
#include "prisminfer/host_memory_tracker.h"
#include "prisminfer/sha256.h"
#include "prisminfer/telemetry.h"
#include "prisminfer/windows_evidence.h"

namespace {

constexpr std::uint64_t kGiB = 1ULL << 30U;
constexpr std::uint32_t kWorkerTimeoutMilliseconds = 10'000U;
constexpr std::uint64_t kCleanupWddmToleranceBytes = 16ULL << 20U;

struct Options {
  std::filesystem::path worker;
  std::filesystem::path output_root;
  std::string case_name;
  std::string workflow_run_id;
  std::string authorization_id;
  std::string gpu_uuid;
  std::uint32_t adapter_index{0};
  std::uint64_t payload_bytes{0};
};

template <typename Integer>
bool parse_integer(const std::string& text, Integer* value) {
  if (text.empty()) return false;
  const auto* begin = text.data();
  const auto* end = begin + text.size();
  const auto parsed = std::from_chars(begin, end, *value);
  return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool parse_options(int argc, char** argv, Options* options) {
  if (argc != 17) return false;
  std::map<std::string, std::string> fields;
  for (int index = 1; index + 1 < argc; index += 2) {
    if (std::string(argv[index]).rfind("--", 0U) != 0U ||
        !fields.emplace(argv[index], argv[index + 1]).second) {
      return false;
    }
  }
  if (fields.size() != 8U || !fields.contains("--worker") ||
      !fields.contains("--output-root") || !fields.contains("--case") ||
      !fields.contains("--workflow-run-id") ||
      !fields.contains("--authorization-id") ||
      !fields.contains("--gpu-uuid") ||
      !fields.contains("--adapter-index") ||
      !fields.contains("--payload-bytes")) {
    return false;
  }
  options->worker = fields["--worker"];
  options->output_root = fields["--output-root"];
  options->case_name = fields["--case"];
  options->workflow_run_id = fields["--workflow-run-id"];
  options->authorization_id = fields["--authorization-id"];
  options->gpu_uuid = fields["--gpu-uuid"];
  const auto authorization_valid =
      !options->authorization_id.empty() &&
      options->authorization_id.size() <= 128U &&
      options->authorization_id.find_first_not_of(
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.:-") ==
          std::string::npos;
  bool gpu_uuid_valid = options->gpu_uuid.size() == 40U &&
                        options->gpu_uuid.rfind("GPU-", 0U) == 0U;
  for (std::size_t index = 4U; gpu_uuid_valid && index < 40U; ++index) {
    const bool separator = index == 12U || index == 17U || index == 22U ||
                           index == 27U;
    gpu_uuid_valid = separator
                         ? options->gpu_uuid[index] == '-'
                         : ((options->gpu_uuid[index] >= '0' &&
                             options->gpu_uuid[index] <= '9') ||
                            (options->gpu_uuid[index] >= 'a' &&
                             options->gpu_uuid[index] <= 'f'));
  }
  return parse_integer(fields["--adapter-index"], &options->adapter_index) &&
         options->adapter_index < 64U &&
         parse_integer(fields["--payload-bytes"], &options->payload_bytes) &&
         options->payload_bytes != 0U &&
         options->payload_bytes <= prisminfer::kC2MaximumPayloadBytes &&
         !options->workflow_run_id.empty() &&
         options->workflow_run_id.size() <= 128U &&
         authorization_valid && gpu_uuid_valid &&
         (options->case_name == "success" ||
          options->case_name == "post-context-telemetry-loss" ||
          options->case_name == "heartbeat-loss" ||
          options->case_name == "watchdog-cancel");
}

prisminfer::AdmissionDigest digest(const std::string& value) {
  std::string hexadecimal;
  prisminfer::AdmissionDigest result{};
  if (!prisminfer::sha256_text(value, &hexadecimal) ||
      hexadecimal.size() != 64U) {
    return result;
  }
  for (std::size_t index = 0U; index < result.size(); ++index) {
    unsigned int byte = 0U;
    const auto* begin = hexadecimal.data() + index * 2U;
    const auto parsed = std::from_chars(begin, begin + 2U, byte, 16);
    if (parsed.ec != std::errc{}) return {};
    result[index] = static_cast<std::uint8_t>(byte);
  }
  return result;
}

prisminfer::PreContextGpuSample gpu_sample(
    const prisminfer::WddmMemorySample& wddm) {
  prisminfer::PreContextGpuSample sample;
  sample.available = wddm.available;
  sample.adapter_identity_available = wddm.available;
  sample.adapter_luid_high = wddm.adapter_luid_high;
  sample.adapter_luid_low = wddm.adapter_luid_low;
  sample.captured_monotonic_milliseconds =
      wddm.captured_monotonic_milliseconds;
  sample.physical_or_reportable_local_bytes = wddm.local_budget_bytes;
  sample.dxgi_local_budget_bytes = wddm.local_budget_bytes;
  sample.dxgi_local_current_usage_bytes = wddm.local_current_usage_bytes;
  return sample;
}

prisminfer::PredictedBytes predicted(
    std::uint64_t bytes, prisminfer::PredictionProvenance provenance) {
  return {bytes, provenance};
}

prisminfer::AdmissionCellIdentity make_cell(
    const Options& options, const prisminfer::WddmMemorySample& wddm,
    const std::string& worker_sha256) {
  prisminfer::AdmissionCellIdentity cell;
  cell.run_sequence = 1U;
  cell.run_contract_hash = digest("prisminfer-c2-synthetic-contract-v1");
  cell.threshold_registry_hash = digest(
      "prisminfer-t100-t105-v2:c2-max-payload=67108864:worker=10000ms");
  cell.hardware_identity_hash =
      digest(std::to_string(wddm.adapter_luid_high) + ":" +
             std::to_string(wddm.adapter_luid_low) + ":" +
             wddm.adapter_description);
  cell.runtime_identity_hash =
      digest(std::string(PRISMINFER_C2_REVIEWED_SHA) + ":" +
             PRISMINFER_C2_SOURCE_TREE_SHA);
  cell.artifact_identity_hash = digest(worker_sha256);
  cell.service_profile_hash =
      digest(options.case_name + ":" +
             std::to_string(options.payload_bytes) + ":" +
             std::to_string(kWorkerTimeoutMilliseconds));
  return cell;
}

prisminfer::PreContextAdmissionRequest make_pre_request(
    const Options& options, const prisminfer::WddmMemorySample& wddm,
    const prisminfer::AdmissionCellIdentity& cell) {
  const auto now = prisminfer::monotonic_time_milliseconds();
  prisminfer::PreContextAdmissionRequest request;
  request.requested_tier_bytes = 2U * kGiB;
  request.context_tokens = 1U;
  request.run_deadline_milliseconds = 60'000U;
  request.evaluation_monotonic_milliseconds = now;
  request.cell = cell;
  request.predicted_gpu.context_runtime = predicted(
      512ULL << 20U,
      prisminfer::PredictionProvenance::PinnedRuntimeEstimate);
  request.predicted_gpu.backend = predicted(
      options.payload_bytes,
      prisminfer::PredictionProvenance::PinnedRuntimeEstimate);
  request.predicted_gpu.model = predicted(
      0U, prisminfer::PredictionProvenance::ExactZeroNotApplicable);
  request.predicted_gpu.state = predicted(
      0U, prisminfer::PredictionProvenance::ExactZeroNotApplicable);
  request.predicted_gpu.workspace = predicted(
      0U, prisminfer::PredictionProvenance::ExactZeroNotApplicable);
  request.predicted_gpu.fragmentation = predicted(
      0U, prisminfer::PredictionProvenance::ExactZeroNotApplicable);
  request.gpu = gpu_sample(wddm);
  request.thermal =
      prisminfer::sample_nvml_gpu_thermal(options.gpu_uuid);
  request.host = prisminfer::sample_host_telemetry();
  request.host_policy = prisminfer::default_host_reserve_policy(
      prisminfer::HostAdmissionLane::EvidencePromotable);
  request.host_request.planned_incremental_resident_bytes = 512ULL << 20U;
  request.host_request.planned_incremental_commit_bytes = 512ULL << 20U;
  request.host_request.resident_uncertainty_bytes = 128ULL << 20U;
  request.host_request.commit_uncertainty_bytes = 128ULL << 20U;
  request.host_request.promotion_requested = true;
  request.storage.available = false;
  std::error_code space_error;
  const auto space = std::filesystem::space(options.output_root, space_error);
  if (!space_error) {
    request.storage.available = true;
    request.storage.free_bytes = space.available;
    request.storage.reserve_bytes = 1U * kGiB;
    request.storage.required_incremental_bytes = 4ULL << 20U;
  }
  return request;
}

prisminfer::PostContextAdmissionRequest make_post_request(
    const Options& options, const prisminfer::AdmissionCellIdentity& cell,
    const prisminfer::HostReservePolicy& host_policy,
    const prisminfer::HostAdmissionRequest& host_request,
    std::uint32_t process_id, bool lose_telemetry) {
  prisminfer::PostContextAdmissionRequest request;
  request.requested_tier_bytes = 2U * kGiB;
  request.cell = cell;
  request.gpu = gpu_sample(
      prisminfer::sample_wddm_memory(options.adapter_index));
  request.thermal =
      prisminfer::sample_nvml_gpu_thermal(options.gpu_uuid);
  request.host = prisminfer::sample_host_telemetry();
  request.host_policy = host_policy;
  request.host_request = host_request;
  request.owned_gpu.process_id = process_id;
  if (lose_telemetry) request.gpu.available = false;
  return request;
}

prisminfer::SupervisorWatchdogSample make_watchdog_sample(
    const Options& options, const prisminfer::HostReservePolicy& host_policy,
    const prisminfer::HostAdmissionRequest& host_request,
    std::uint32_t process_id, bool force_cancel) {
  prisminfer::SupervisorWatchdogSample sample;
  sample.gpu = gpu_sample(
      prisminfer::sample_wddm_memory(options.adapter_index));
  sample.thermal =
      prisminfer::sample_nvml_gpu_thermal(options.gpu_uuid);
  sample.host = prisminfer::sample_host_telemetry();
  sample.host_policy = host_policy;
  sample.host_request = host_request;
  sample.owned_gpu.process_id = process_id;
  if (force_cancel) sample.thermal.available = false;
  return sample;
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  if (!parse_options(argc, argv, &options)) {
    std::cerr << "invalid arguments\n";
    return 2;
  }
  std::error_code path_error;
  options.worker = std::filesystem::absolute(options.worker, path_error);
  if (path_error) return 3;
  options.output_root =
      std::filesystem::weakly_canonical(options.output_root, path_error);
  if (path_error || !std::filesystem::is_directory(options.output_root)) {
    return 4;
  }
  const auto worker_sha256 =
      prisminfer::sha256_regular_file(options.worker);
  if (worker_sha256.size() != 64U) return 5;
  const auto pre_wddm =
      prisminfer::sample_wddm_memory(options.adapter_index);
  if (!pre_wddm.available) return 6;
  const auto cell = make_cell(options, pre_wddm, worker_sha256);
  auto acquired = prisminfer::acquire_gpu_admission_session(
      cell, pre_wddm.adapter_luid_high, pre_wddm.adapter_luid_low);
  if (!acquired.session) return 7;
  const auto lease_id = acquired.session->lease_id();
  const auto pre = make_pre_request(options, pre_wddm, cell);
  const auto host_policy = pre.host_policy;
  const auto host_request = pre.host_request;
  const auto pre_decision = acquired.session->admit_pre_context(pre);
  if (!pre_decision.admitted) {
    std::cerr << pre_decision.reason << "\n";
    return 8;
  }

  constexpr const char* kApprovalIdentity = "c2-synthetic-worker-v1";
  prisminfer::NativeWorkerTrustCatalog catalog(
      {{options.worker, options.worker.parent_path(), worker_sha256,
        kApprovalIdentity}});
  prisminfer::NativeWorkerRequest worker_request;
  worker_request.executable_path = options.worker;
  worker_request.arguments = {
      L"--case", std::wstring(options.case_name.begin(), options.case_name.end()),
      L"--device-index", std::to_wstring(options.adapter_index),
      L"--luid-high", std::to_wstring(pre_wddm.adapter_luid_high),
      L"--luid-low", std::to_wstring(pre_wddm.adapter_luid_low),
      L"--gpu-uuid", std::wstring(options.gpu_uuid.begin(), options.gpu_uuid.end()),
      L"--payload-bytes", std::to_wstring(options.payload_bytes)};
  worker_request.timeout_ms = kWorkerTimeoutMilliseconds;
  worker_request.max_output_bytes = 1U * 1024U * 1024U;
  worker_request.max_active_processes = 1U;
  worker_request.max_job_memory_bytes = 1U * kGiB;
  worker_request.require_gpu_protocol_evidence = true;
  worker_request.expected_post_admission_payload_bytes = options.payload_bytes;
  worker_request.maximum_post_admission_payload_bytes =
      prisminfer::kC2MaximumPayloadBytes;

  const bool lose_post_context =
      options.case_name == "post-context-telemetry-loss";
  const bool force_watchdog_cancel =
      options.case_name == "watchdog-cancel";
  auto result = acquired.session->run_contained_worker(
      catalog, worker_request, 250U,
      [&](std::stop_token stop, std::uint32_t pid, std::uint64_t) {
        if (stop.stop_requested()) return prisminfer::PostContextAdmissionRequest{};
        return make_post_request(options, cell, host_policy, host_request, pid,
                                 lose_post_context);
      },
      [&](std::stop_token stop, std::uint32_t pid, std::uint64_t,
          std::uint64_t) {
        if (stop.stop_requested()) return prisminfer::SupervisorWatchdogSample{};
        return make_watchdog_sample(options, host_policy, host_request, pid,
                                    force_watchdog_cancel);
      });

  const auto final_wddm =
      prisminfer::sample_wddm_memory(options.adapter_index);
  const auto final_host = prisminfer::sample_host_telemetry();
  const auto final_thermal =
      prisminfer::sample_nvml_gpu_thermal(options.gpu_uuid);
  const auto cleanup_wddm_positive_delta =
      final_wddm.local_current_usage_bytes > pre_wddm.local_current_usage_bytes
          ? final_wddm.local_current_usage_bytes -
                pre_wddm.local_current_usage_bytes
          : 0U;
  const bool device_reconciled = result.worker_exit_observed &&
                                 result.job_tree_empty && final_wddm.available &&
                                 final_host.available && final_thermal.available &&
                                 cleanup_wddm_positive_delta <=
                                     kCleanupWddmToleranceBytes;
  std::string last_good_hash;
  const std::string last_good_material =
      worker_sha256 + ":" + std::to_string(result.root_process_id) + ":" +
      std::to_string(result.heartbeat_sequence) + ":" +
      std::to_string(final_wddm.local_current_usage_bytes);
  if (!prisminfer::sha256_text(last_good_material, &last_good_hash)) return 9;
  std::string bundle_hash;
  if (!prisminfer::sha256_text(last_good_hash + ":" + result.failure_reason +
                                   ":" + options.case_name,
                               &bundle_hash)) {
    return 10;
  }
  bool output_removed = result.output_path.empty();
  if (!result.output_path.empty()) {
    std::error_code remove_error;
    output_removed = std::filesystem::remove(result.output_path, remove_error) &&
                     !remove_error;
  }
  result.temporary_files_reconciled =
      result.temporary_files_reconciled && output_removed;

  prisminfer::DeviceCleanupEvidence cleanup_evidence;
  cleanup_evidence.device_resources_reconciled = device_reconciled;
  cleanup_evidence.terminal_reason =
      result.ok ? "synthetic_c2_candidate_completed" : result.failure_reason;
  cleanup_evidence.last_good_sample_sha256 = last_good_hash;
  cleanup_evidence.evidence_bundle_sha256 = bundle_hash;
  const auto cleanup = acquired.session->finalize_cleanup(
      cleanup_evidence, prisminfer::monotonic_time_milliseconds());

  prisminfer::C2ClearanceReceipt receipt;
  receipt.repository = "Gravelaw/prisminfer";
  receipt.reviewed_sha = PRISMINFER_C2_REVIEWED_SHA;
  receipt.source_tree_sha = PRISMINFER_C2_SOURCE_TREE_SHA;
  receipt.worker_sha256 = worker_sha256;
  receipt.worker_approval_identity = kApprovalIdentity;
  receipt.workflow_run_id = options.workflow_run_id;
  receipt.authorization_id = options.authorization_id;
  receipt.gpu_uuid = options.gpu_uuid;
  receipt.case_name = options.case_name;
  receipt.status = result.ok ? "candidate-complete" : "rejected";
  receipt.failure_reason = result.failure_reason;
  receipt.cleanup_status =
      cleanup == prisminfer::GpuAdmissionSessionState::Cleaned ? "cleaned"
                                                               : "quarantined";
  receipt.lease_id = lease_id;
  receipt.job_identity = result.job_identity;
  receipt.last_good_sample_sha256 = last_good_hash;
  receipt.evidence_bundle_sha256 = bundle_hash;
  receipt.adapter_luid_high = pre_wddm.adapter_luid_high;
  receipt.adapter_luid_low = pre_wddm.adapter_luid_low;
  receipt.adapter_index = options.adapter_index;
  receipt.worker_timeout_milliseconds = kWorkerTimeoutMilliseconds;
  receipt.post_admission_payload_bytes =
      result.last_heartbeat_evidence.available
          ? result.last_heartbeat_evidence.post_admission_payload_bytes
          : 0U;
  receipt.context_cuda_free_bytes =
      result.context_evidence.cuda_mem_info_free_bytes;
  receipt.context_cuda_total_bytes =
      result.context_evidence.cuda_mem_info_total_bytes;
  receipt.last_heartbeat_cuda_free_bytes =
      result.last_heartbeat_evidence.cuda_mem_info_free_bytes;
  receipt.last_heartbeat_cuda_total_bytes =
      result.last_heartbeat_evidence.cuda_mem_info_total_bytes;
  receipt.pre_wddm_local_usage_bytes = pre_wddm.local_current_usage_bytes;
  receipt.final_wddm_local_usage_bytes = final_wddm.local_current_usage_bytes;
  receipt.cleanup_wddm_positive_delta_bytes = cleanup_wddm_positive_delta;
  receipt.cleanup_wddm_tolerance_bytes = kCleanupWddmToleranceBytes;
  receipt.pre_host_memory_available_bytes =
      pre.host.system_memory_available_bytes;
  receipt.final_host_memory_available_bytes =
      final_host.system_memory_available_bytes;
  receipt.pre_host_commit_available_bytes =
      pre.host.system_commit_available_bytes;
  receipt.final_host_commit_available_bytes =
      final_host.system_commit_available_bytes;
  receipt.pre_gpu_temperature_celsius = pre.thermal.current_celsius;
  receipt.final_gpu_temperature_celsius = final_thermal.current_celsius;
  receipt.root_process_id = result.root_process_id;
  receipt.heartbeat_count =
      static_cast<std::uint32_t>(result.heartbeat_sequence);
  receipt.context_ready_observed = result.context_evidence.available;
  receipt.token_consumed_observed = result.token_consumed_observed;
  receipt.worker_exit_observed = result.worker_exit_observed;
  receipt.job_tree_empty = result.job_tree_empty;
  receipt.job_accounting_reconciled = result.job_accounting_reconciled;
  receipt.device_resources_reconciled = device_reconciled;
  receipt.artifact_handles_closed = result.artifact_handles_closed;
  receipt.temporary_files_reconciled = result.temporary_files_reconciled;
  const auto receipt_path =
      options.output_root /
      ("c2-clearance-" + options.case_name + ".json");
  std::string receipt_error;
  if (!prisminfer::write_c2_clearance_receipt(
          options.output_root, receipt_path, receipt, &receipt_error)) {
    std::cerr << receipt_error << "\n";
    return 11;
  }
  if (!prisminfer::c2_case_result_matches_contract(
          options.case_name, result, cleanup, output_removed)) {
    std::cerr << "unexpected case result: " << result.failure_reason << "\n";
    return 12;
  }
  return 0;
}

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace prisminfer {

inline constexpr std::uint64_t kC2ReceiptMaximumBytes = 32U * 1024U;
inline constexpr std::uint64_t kC2MaximumPayloadBytes =
    64ULL * 1024ULL * 1024ULL;

struct C2ClearanceReceipt {
  std::string repository;
  std::string reviewed_sha;
  std::string source_tree_sha;
  std::string worker_sha256;
  std::string worker_approval_identity;
  std::string workflow_run_id;
  std::string authorization_id;
  std::string gpu_uuid;
  std::string case_name;
  std::string status;
  std::string failure_reason;
  std::string cleanup_status;
  std::string lease_id;
  std::string job_identity;
  std::string process_wddm_source;
  std::string last_good_sample_sha256;
  std::string pre_cleanup_evidence_sha256;
  std::string evidence_bundle_sha256;
  std::int32_t adapter_luid_high{0};
  std::uint32_t adapter_luid_low{0};
  std::uint32_t adapter_index{0};
  std::uint32_t worker_timeout_milliseconds{0};
  std::uint64_t post_admission_payload_bytes{0};
  std::uint64_t maximum_payload_bytes{kC2MaximumPayloadBytes};
  std::uint64_t context_cuda_free_bytes{0};
  std::uint64_t context_cuda_total_bytes{0};
  std::uint64_t last_heartbeat_cuda_free_bytes{0};
  std::uint64_t last_heartbeat_cuda_total_bytes{0};
  std::uint64_t pre_wddm_local_usage_bytes{0};
  std::uint64_t final_wddm_local_usage_bytes{0};
  std::uint64_t cleanup_wddm_positive_delta_bytes{0};
  std::uint64_t cleanup_wddm_tolerance_bytes{16ULL * 1024ULL * 1024ULL};
  std::uint64_t last_sample_monotonic_milliseconds{0};
  std::uint64_t last_sample_wddm_local_budget_bytes{0};
  std::uint64_t last_sample_wddm_local_usage_bytes{0};
  std::uint64_t process_wddm_sample_monotonic_milliseconds{0};
  std::uint64_t process_wddm_current_bytes{0};
  std::uint64_t process_wddm_peak_bytes{0};
  std::uint64_t last_host_sample_monotonic_milliseconds{0};
  std::uint64_t last_host_memory_available_bytes{0};
  std::uint64_t last_host_commit_available_bytes{0};
  std::uint64_t last_thermal_sample_monotonic_milliseconds{0};
  std::int32_t last_gpu_temperature_celsius{0};
  std::int32_t last_gpu_slowdown_celsius{0};
  std::uint64_t pre_host_memory_available_bytes{0};
  std::uint64_t final_host_memory_available_bytes{0};
  std::uint64_t pre_host_commit_available_bytes{0};
  std::uint64_t final_host_commit_available_bytes{0};
  std::int32_t pre_gpu_temperature_celsius{0};
  std::int32_t final_gpu_temperature_celsius{0};
  std::uint32_t root_process_id{0};
  std::uint32_t heartbeat_count{0};
  bool context_ready_observed{false};
  bool token_consumed_observed{false};
  bool worker_exit_observed{false};
  bool job_tree_empty{false};
  bool job_accounting_reconciled{false};
  bool device_resources_reconciled{false};
  bool artifact_handles_closed{false};
  bool temporary_files_reconciled{false};
  bool last_gpu_thermal_throttling{false};
  bool last_gpu_power_brake_slowdown{false};
  bool last_wddm_available{false};
  bool last_process_wddm_available{false};
  bool last_host_available{false};
  bool last_thermal_available{false};
};

[[nodiscard]] bool compute_c2_clearance_evidence_hashes(
    C2ClearanceReceipt* receipt, std::string* error);

[[nodiscard]] std::string serialize_c2_clearance_receipt(
    const C2ClearanceReceipt& receipt);
[[nodiscard]] bool validate_c2_clearance_receipt_file(
    const std::filesystem::path& path, std::string* error);
[[nodiscard]] bool write_c2_clearance_receipt(
    const std::filesystem::path& trusted_output_root,
    const std::filesystem::path& path, const C2ClearanceReceipt& receipt,
    std::string* error);

}  // namespace prisminfer

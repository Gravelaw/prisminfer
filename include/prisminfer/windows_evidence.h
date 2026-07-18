#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "prisminfer/host_memory_tracker.h"
#include "prisminfer/transfer_metrics.h"

namespace prisminfer {

struct WddmMemorySample {
  bool available{false};
  std::string unavailable_reason;
  std::uint64_t captured_monotonic_milliseconds{0};
  std::uint32_t adapter_index{0};
  std::int32_t adapter_luid_high{0};
  std::uint32_t adapter_luid_low{0};
  std::string adapter_description;
  std::uint64_t local_budget_bytes{0};
  std::uint64_t local_current_usage_bytes{0};
  std::uint64_t local_available_for_reservation_bytes{0};
  std::uint64_t nonlocal_budget_bytes{0};
  std::uint64_t nonlocal_current_usage_bytes{0};
  std::uint64_t nonlocal_available_for_reservation_bytes{0};
};

WddmMemorySample sample_wddm_memory(std::uint32_t adapter_index = 0U);

struct FileIoEvidence {
  bool identity_available{false};
  std::string unavailable_reason;
  std::string role;
  std::string final_path;
  std::string volume_serial_hex;
  std::string file_id_hex;
  std::uint32_t hard_link_count{0};
  std::uint64_t size_bytes{0};
  std::uint64_t mapped_bytes{0};
  bool resident_proxy_available{false};
  std::uint64_t resident_proxy_bytes{0};
  bool identity_aware_io_available{false};
  std::uint64_t observed_read_bytes{0};
  std::uint64_t observed_write_bytes{0};
  bool pagefile_io_available{false};
  std::uint64_t observed_pagefile_read_bytes{0};
  std::uint64_t observed_pagefile_write_bytes{0};
  bool hard_faults_available{false};
  std::uint64_t hard_fault_count{0};
  std::string hard_fault_source{"unavailable"};
  std::string cache_state{"unknown"};
  bool cache_state_verified{false};
  std::uint64_t dropped_trace_records{0};
};

FileIoEvidence sample_file_identity(const std::filesystem::path& path,
                                    std::string role);

struct FileEvidenceValidationResult {
  bool ok{false};
  std::string failure_reason;
};

FileEvidenceValidationResult validate_file_io_evidence(
    const FileIoEvidence& evidence, bool cold_cache_required);

struct OwnedGpuMemoryEvidence {
  bool available{false};
  std::uint64_t captured_monotonic_milliseconds{0};
  bool reconciled{false};
  bool process_device_corroboration_available{false};
  std::string process_device_source;
  std::uint32_t process_id{0};
  std::uint64_t process_device_captured_monotonic_milliseconds{0};
  std::uint64_t process_device_current_bytes{0};
  bool adapter_identity_available{false};
  std::int32_t adapter_luid_high{0};
  std::uint32_t adapter_luid_low{0};
  std::uint64_t hard_cap_bytes{0};
  std::uint64_t owned_current_bytes{0};
  std::uint64_t owned_peak_bytes{0};
  std::uint64_t rejected_attempted_peak_bytes{0};
  std::uint64_t backend_buffer_current_bytes{0};
  std::uint64_t backend_buffer_at_owned_peak_bytes{0};
  std::uint64_t backend_pool_current_bytes{0};
  std::uint64_t backend_pool_at_owned_peak_bytes{0};
  std::uint64_t kernel_workspace_current_bytes{0};
  std::uint64_t kernel_workspace_at_owned_peak_bytes{0};
  std::uint64_t activation_current_bytes{0};
  std::uint64_t activation_at_owned_peak_bytes{0};
  std::uint64_t kv_current_bytes{0};
  std::uint64_t kv_at_owned_peak_bytes{0};
  std::uint64_t graph_current_bytes{0};
  std::uint64_t graph_at_owned_peak_bytes{0};
  std::uint64_t profiler_workspace_current_bytes{0};
  std::uint64_t profiler_workspace_at_owned_peak_bytes{0};
  std::uint64_t cuda_context_runtime_current_bytes{0};
  std::uint64_t cuda_context_runtime_at_owned_peak_bytes{0};
  std::uint64_t cuda_mem_info_free_bytes{0};
  std::uint64_t cuda_mem_info_total_bytes{0};
  std::uint64_t unknown_unreconciled_bytes{0};
};

struct ProcessTreeHostEvidence {
  bool available{false};
  std::string parent_identity;
  std::string job_identity;
  std::uint64_t parent_working_set_current_bytes{0};
  std::uint64_t parent_working_set_peak_bytes{0};
  std::uint64_t parent_private_commit_current_bytes{0};
  std::uint64_t parent_private_commit_peak_bytes{0};
  std::uint64_t tree_working_set_current_bytes{0};
  std::uint64_t tree_working_set_peak_bytes{0};
  std::uint64_t tree_private_commit_current_bytes{0};
  std::uint64_t tree_private_commit_peak_bytes{0};
  std::uint64_t tree_io_read_bytes{0};
  std::uint64_t tree_io_write_bytes{0};
};

struct WindowsEvidenceBundle {
  std::string claim_scope{"owned-allocation"};
  std::string instrumentation_mode{"ordinary"};
  bool real_execution{false};
  std::uint64_t evaluation_monotonic_milliseconds{0};
  std::uint64_t maximum_host_sample_age_milliseconds{0};
  std::uint64_t maximum_owned_gpu_sample_age_milliseconds{0};
  std::uint64_t maximum_wddm_sample_age_milliseconds{0};
  OwnedGpuMemoryEvidence gpu;
  WddmMemorySample wddm;
  bool eviction_residency_trace_available{false};
  bool eviction_or_nonlocal_residency_observed{false};
  std::uint64_t residency_trace_dropped_records{0};
  ProcessTreeHostEvidence process_tree;
  HostTelemetrySample system_host_pre;
  HostTelemetrySample system_host_post;
  bool offload_file_claim_requested{false};
  bool cold_cache_claim_requested{false};
  std::vector<FileIoEvidence> files;
  bool transfer_claim_requested{false};
  TransferSample transfers;
};

struct WindowsEvidenceDecision {
  bool promotable{false};
  std::string classification{"measured-non-certified"};
  std::string reason;
};

WindowsEvidenceDecision classify_windows_evidence(
    const WindowsEvidenceBundle& evidence);

}  // namespace prisminfer

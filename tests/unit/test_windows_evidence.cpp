#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

#include "prisminfer/windows_evidence.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

prisminfer::WindowsEvidenceBundle valid_owned() {
  prisminfer::WindowsEvidenceBundle evidence;
  evidence.real_execution = true;
  evidence.evaluation_monotonic_milliseconds = 250;
  evidence.maximum_host_sample_age_milliseconds = 500;
  evidence.gpu.available = true;
  evidence.gpu.reconciled = true;
  evidence.gpu.process_device_corroboration_available = true;
  evidence.gpu.hard_cap_bytes = 1024;
  evidence.gpu.owned_current_bytes = 512;
  evidence.gpu.owned_peak_bytes = 768;
  evidence.gpu.backend_buffer_current_bytes = 512;
  evidence.gpu.backend_buffer_at_owned_peak_bytes = 768;
  evidence.process_tree.available = true;
  evidence.process_tree.parent_identity = "parent-1";
  evidence.process_tree.job_identity = "job-1";
  evidence.process_tree.parent_working_set_current_bytes = 128;
  evidence.process_tree.parent_working_set_peak_bytes = 256;
  evidence.process_tree.parent_private_commit_current_bytes = 192;
  evidence.process_tree.parent_private_commit_peak_bytes = 384;
  evidence.process_tree.tree_working_set_peak_bytes = 512;
  evidence.process_tree.tree_private_commit_peak_bytes = 768;
  evidence.system_host_pre.available = true;
  evidence.system_host_pre.system_commit_source = "get_performance_info";
  evidence.system_host_pre.pagefile_configuration_available = true;
  evidence.system_host_pre.process_id = 42;
  evidence.system_host_pre.process_image_path = R"(C:\prism-probe.exe)";
  evidence.system_host_pre.captured_monotonic_milliseconds = 100;
  evidence.system_host_post = evidence.system_host_pre;
  evidence.system_host_post.captured_monotonic_milliseconds = 200;
  return evidence;
}

prisminfer::FileIoEvidence valid_source_file() {
  prisminfer::FileIoEvidence file;
  file.identity_available = true;
  file.role = "source-gguf";
  file.final_path = R"(C:\model.gguf)";
  file.volume_serial_hex = "0123456789abcdef";
  file.file_id_hex = "0123456789abcdef0123456789abcdef";
  file.hard_link_count = 1;
  file.size_bytes = 4096;
  file.identity_aware_io_available = true;
  file.observed_read_bytes = 4096;
  file.pagefile_io_available = true;
  file.hard_faults_available = true;
  file.hard_fault_count = 1;
  file.hard_fault_source = "etw-file-identity";
  return file;
}

int expect_downgrade(const prisminfer::WindowsEvidenceBundle& evidence,
                     const char* reason, const char* message) {
  const auto decision = prisminfer::classify_windows_evidence(evidence);
  return expect(!decision.promotable && decision.reason == reason, message);
}

}  // namespace

int main() {
  const auto owned = prisminfer::classify_windows_evidence(valid_owned());
  if (expect(owned.promotable &&
                 owned.classification == "owned-allocation-cap-certified",
             "reconciled owned allocation certifies independently"))
    return 1;

  auto unknown = valid_owned();
  unknown.gpu.unknown_unreconciled_bytes = 1;
  const auto unknown_result = prisminfer::classify_windows_evidence(unknown);
  if (expect(!unknown_result.promotable &&
                 unknown_result.reason == "unknown_gpu_bytes_present",
             "unknown GPU bytes downgrade owned claim"))
    return 1;

  auto mismatched_gpu = valid_owned();
  mismatched_gpu.gpu.backend_buffer_at_owned_peak_bytes = 767;
  const auto mismatched_gpu_result =
      prisminfer::classify_windows_evidence(mismatched_gpu);
  if (expect(!mismatched_gpu_result.promotable &&
                 mismatched_gpu_result.reason ==
                     "owned_gpu_category_reconciliation_failed",
             "owned GPU categories must arithmetically reconcile at peak")) {
    return 1;
  }

  auto mismatched_current_gpu = valid_owned();
  mismatched_current_gpu.gpu.backend_buffer_current_bytes = 511;
  const auto mismatched_current_gpu_result =
      prisminfer::classify_windows_evidence(mismatched_current_gpu);
  if (expect(!mismatched_current_gpu_result.promotable &&
                 mismatched_current_gpu_result.reason ==
                     "owned_gpu_category_reconciliation_failed",
             "owned GPU current categories must arithmetically reconcile")) {
    return 1;
  }

  auto overflowing_gpu = valid_owned();
  overflowing_gpu.gpu.backend_buffer_at_owned_peak_bytes =
      std::numeric_limits<std::uint64_t>::max();
  overflowing_gpu.gpu.backend_pool_at_owned_peak_bytes = 1;
  const auto overflowing_gpu_result =
      prisminfer::classify_windows_evidence(overflowing_gpu);
  if (expect(!overflowing_gpu_result.promotable &&
                 overflowing_gpu_result.reason ==
                     "owned_gpu_category_reconciliation_failed",
             "owned GPU category overflow fails closed")) {
    return 1;
  }

  auto mismatched_host = valid_owned();
  mismatched_host.system_host_post.process_id = 43;
  const auto mismatched_host_result =
      prisminfer::classify_windows_evidence(mismatched_host);
  if (expect(!mismatched_host_result.promotable &&
                 mismatched_host_result.reason ==
                     "authoritative_system_host_evidence_required",
             "pre/post host lifecycle samples require one process identity")) {
    return 1;
  }

  auto stale_host = valid_owned();
  stale_host.evaluation_monotonic_milliseconds = 701;
  if (expect_downgrade(stale_host,
                       "authoritative_system_host_evidence_required",
                       "stale host evidence fails closed")) {
    return 1;
  }

  auto invalid_host_age_policy = valid_owned();
  invalid_host_age_policy.maximum_host_sample_age_milliseconds = 501;
  if (expect_downgrade(invalid_host_age_policy,
                       "authoritative_system_host_evidence_required",
                       "host freshness cannot exceed T-103")) {
    return 1;
  }

  auto invalid_tree = valid_owned();
  invalid_tree.process_tree.parent_working_set_peak_bytes = 1;
  if (expect_downgrade(invalid_tree, "process_tree_host_counters_invalid",
                       "contradictory process-tree counters fail closed")) {
    return 1;
  }

  auto missing_pagefile = valid_owned();
  missing_pagefile.system_host_post.pagefile_configuration_available = false;
  if (expect_downgrade(missing_pagefile,
                       "authoritative_system_host_evidence_required",
                       "missing pagefile configuration blocks promotion")) {
    return 1;
  }

  auto cap_exceeded = valid_owned();
  cap_exceeded.gpu.hard_cap_bytes = 700;
  if (expect_downgrade(cap_exceeded, "owned_gpu_cap_exceeded",
                       "owned GPU peak above the hard cap is rejected")) {
    return 1;
  }

  auto physical = valid_owned();
  physical.claim_scope = "physical-residency";
  physical.evaluation_monotonic_milliseconds = 1000;
  physical.system_host_pre.captured_monotonic_milliseconds = 900;
  physical.system_host_post.captured_monotonic_milliseconds = 950;
  physical.maximum_wddm_sample_age_milliseconds = 100;
  physical.wddm.available = true;
  physical.wddm.captured_monotonic_milliseconds = 950;
  physical.wddm.local_budget_bytes = 1024;
  physical.wddm.local_current_usage_bytes = 512;
  physical.wddm.adapter_luid_high = 1;
  physical.wddm.adapter_luid_low = 2;
  physical.gpu.adapter_identity_available = true;
  physical.gpu.adapter_luid_high = 1;
  physical.gpu.adapter_luid_low = 2;
  physical.eviction_residency_trace_available = true;
  const auto physical_result = prisminfer::classify_windows_evidence(physical);
  if (expect(
          physical_result.promotable && physical_result.classification ==
                                            "physical-residency-cap-certified",
          "fresh ordinary WDDM and trace evidence certifies residency")) {
    return 1;
  }

  auto adapter_mismatch = physical;
  adapter_mismatch.gpu.adapter_luid_low = 3;
  if (expect_downgrade(adapter_mismatch, "wddm_adapter_identity_mismatch",
                       "WDDM and owned-GPU adapter identity must match")) {
    return 1;
  }

  auto nonlocal = physical;
  nonlocal.wddm.nonlocal_current_usage_bytes = 1;
  if (expect_downgrade(nonlocal, "wddm_oversubscription_observed",
                       "nonlocal WDDM use blocks residency certification")) {
    return 1;
  }

  auto trace_drop = physical;
  trace_drop.residency_trace_dropped_records = 1;
  if (expect_downgrade(trace_drop, "complete_residency_trace_required",
                       "dropped residency records fail closed")) {
    return 1;
  }

  auto eviction = physical;
  eviction.eviction_or_nonlocal_residency_observed = true;
  if (expect_downgrade(eviction,
                       "eviction_or_nonlocal_residency_observed",
                       "observed eviction blocks residency certification")) {
    return 1;
  }

  physical.instrumentation_mode = "profiler";
  const auto profiler = prisminfer::classify_windows_evidence(physical);
  if (expect(!profiler.promotable &&
                 profiler.reason == "ordinary_run_required_for_residency_claim",
             "profiler and ordinary residency cells cannot alias"))
    return 1;

  physical.instrumentation_mode = "ordinary";
  physical.evaluation_monotonic_milliseconds = 1100;
  const auto stale = prisminfer::classify_windows_evidence(physical);
  if (expect(
          !stale.promotable && stale.reason == "fresh_wddm_evidence_required",
          "stale WDDM evidence downgrades the residency claim"))
    return 1;

  auto storage = valid_owned();
  storage.offload_file_claim_requested = true;
  storage.files.push_back(valid_source_file());
  storage.files.front().mapped_bytes = 4096;
  const auto ambiguous = prisminfer::classify_windows_evidence(storage);
  if (expect(!ambiguous.promotable &&
                 ambiguous.reason == "mapped_residency_ambiguous" &&
                 ambiguous.classification == "storage-ambiguous",
             "mapped bytes without residency evidence are storage ambiguous")) {
    return 1;
  }

  storage.files.front().mapped_bytes = 0;
  const auto traced_storage = prisminfer::classify_windows_evidence(storage);
  if (expect(traced_storage.promotable,
             "identity-aware file and pagefile evidence preserves the claim")) {
    return 1;
  }

  storage.files.front().observed_read_bytes = 0;
  const auto no_file_activity = prisminfer::classify_windows_evidence(storage);
  if (expect(!no_file_activity.promotable &&
                 no_file_activity.reason ==
                     "actual_identity_linked_file_io_required",
             "instrumentation availability cannot replace observed file IO")) {
    return 1;
  }
  storage.files.front().observed_read_bytes = 4096;

  storage.files.front().hard_link_count = 2;
  if (expect_downgrade(storage, "opened_handle_file_identity_required",
                       "multi-link file identity remains ambiguous")) {
    return 1;
  }
  storage.files.front().hard_link_count = 1;

  storage.files.front().mapped_bytes = 4096;
  storage.files.front().resident_proxy_available = true;
  storage.files.front().resident_proxy_bytes = 4097;
  const auto invalid_resident_proxy =
      prisminfer::classify_windows_evidence(storage);
  if (expect(!invalid_resident_proxy.promotable &&
                 invalid_resident_proxy.reason ==
                     "file_mapping_or_resident_proxy_invalid",
             "resident proxy bytes cannot exceed mapped bytes")) {
    return 1;
  }
  storage.files.front().mapped_bytes = 0;
  storage.files.front().resident_proxy_available = false;
  storage.files.front().resident_proxy_bytes = 0;

  storage.cold_cache_claim_requested = true;
  const auto unknown_cache = prisminfer::classify_windows_evidence(storage);
  if (expect(!unknown_cache.promotable &&
                 unknown_cache.reason == "cold_cache_state_unverified",
             "a cold-cache claim requires verified cache state")) {
    return 1;
  }

  storage.cold_cache_claim_requested = false;
  storage.files.front().dropped_trace_records = 1;
  const auto dropped_file = prisminfer::classify_windows_evidence(storage);
  if (expect(!dropped_file.promotable &&
                 dropped_file.reason == "file_trace_dropped_records",
             "dropped file trace records fail closed")) {
    return 1;
  }

  storage.files.front() = valid_source_file();
  storage.files.front().pagefile_io_available = false;
  if (expect_downgrade(storage, "file_and_pagefile_trace_required",
                       "missing pagefile trace keeps storage ambiguous")) {
    return 1;
  }

  storage.files.front() = valid_source_file();
  storage.files.front().hard_fault_source = "ambiguous";
  if (expect_downgrade(storage, "hard_fault_source_ambiguous",
                       "ambiguous hard-fault attribution fails closed")) {
    return 1;
  }

  auto transfer = valid_owned();
  transfer.transfer_claim_requested = true;
  const auto missing_transfer = prisminfer::classify_windows_evidence(transfer);
  if (expect(
          !missing_transfer.promotable &&
              missing_transfer.reason == "actual_transfer_evidence_required",
          "logical or declared bytes cannot substitute for transfer events")) {
    return 1;
  }

  auto actual_transfer = valid_owned();
  actual_transfer.transfer_claim_requested = true;
  actual_transfer.transfers.events.push_back(
      {"transfer-1", "plan-1", "h2d", "host-1", "gpu-1", "pinned", "stream-1",
       "copy-1", "tensor-1", 64, 64, 1, 2, 3, 1, 0, false, false, false});
  const auto actual_transfer_result =
      prisminfer::classify_windows_evidence(actual_transfer);
  if (expect(actual_transfer_result.promotable,
             "validated actual transfer events preserve owned claim")) {
    return 1;
  }

  actual_transfer.transfers.observed_h2d_completed_bytes = 999999;
  actual_transfer.transfers.events.push_back(
      actual_transfer.transfers.events.front());
  const auto duplicate_transfer_result =
      prisminfer::classify_windows_evidence(actual_transfer);
  if (expect(!duplicate_transfer_result.promotable &&
                 duplicate_transfer_result.reason ==
                     "actual_transfer_evidence_required",
             "classification independently rejects duplicate transfer IDs")) {
    return 1;
  }

  actual_transfer.transfers.events.pop_back();
  actual_transfer.transfers.dropped_records = 1;
  if (expect_downgrade(actual_transfer, "actual_transfer_evidence_required",
                       "dropped transfer instrumentation fails closed")) {
    return 1;
  }

#if defined(_WIN32)
  const auto temporary = std::filesystem::temp_directory_path() /
                         "prisminfer-file-identity-test.gguf";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output << "GGUF";
  }
  const auto identity =
      prisminfer::sample_file_identity(temporary, "source-gguf");
  std::error_code remove_error;
  std::filesystem::remove(temporary, remove_error);
  if (expect(identity.identity_available && identity.size_bytes == 4U &&
                 identity.file_id_hex.size() == 32U &&
                 identity.hard_link_count == 1U &&
                 identity.volume_serial_hex.size() == 16U &&
                 !identity.final_path.empty(),
             "opened-handle file identity is stable and complete")) {
    return 1;
  }
#endif

  const auto live = prisminfer::sample_wddm_memory();
  if (live.available) {
    if (expect(live.local_budget_bytes > 0U &&
                   live.local_current_usage_bytes <= live.local_budget_bytes &&
                   live.captured_monotonic_milliseconds > 0U,
               "live DXGI sample is internally consistent"))
      return 1;
  } else if (expect(!live.unavailable_reason.empty(),
                    "unavailable WDDM sample has a typed reason")) {
    return 1;
  }
  return 0;
}

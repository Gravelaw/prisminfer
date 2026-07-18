#include <cstdint>
#include <iostream>
#include <limits>
#include <type_traits>

#include "prisminfer/supervisor_admission.h"

namespace {

constexpr std::uint64_t kGiB = 1ULL << 30;

static_assert(
    !std::is_default_constructible_v<prisminfer::PreContextAdmissionReceipt>);

int expect(bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

prisminfer::PredictedBytes prediction(
    std::uint64_t bytes, prisminfer::PredictionProvenance provenance) {
  return {bytes, provenance};
}

prisminfer::AdmissionCellIdentity valid_cell(std::uint8_t seed = 1) {
  prisminfer::AdmissionCellIdentity identity;
  identity.run_sequence = seed;
  identity.run_contract_hash.fill(seed);
  identity.threshold_registry_hash.fill(static_cast<std::uint8_t>(seed + 1));
  identity.hardware_identity_hash.fill(static_cast<std::uint8_t>(seed + 2));
  identity.runtime_identity_hash.fill(static_cast<std::uint8_t>(seed + 3));
  identity.artifact_identity_hash.fill(static_cast<std::uint8_t>(seed + 4));
  identity.service_profile_hash.fill(static_cast<std::uint8_t>(seed + 5));
  return identity;
}

prisminfer::PreContextAdmissionRequest valid_request() {
  prisminfer::PreContextAdmissionRequest request;
  request.requested_tier_bytes = 8 * kGiB;
  request.context_tokens = 8'192;
  request.run_deadline_milliseconds = 60'000;
  request.evaluation_monotonic_milliseconds = 10'100;
  request.exclusive_gpu_lease_held = true;

  using prisminfer::PredictionProvenance;
  request.predicted_gpu.context_runtime = prediction(
      512 * (1ULL << 20), PredictionProvenance::PinnedRuntimeEstimate);
  request.predicted_gpu.backend = prediction(
      256 * (1ULL << 20), PredictionProvenance::PinnedRuntimeEstimate);
  request.predicted_gpu.model =
      prediction(5 * kGiB, PredictionProvenance::ApprovedArtifactCensus);
  request.predicted_gpu.state = prediction(
      256 * (1ULL << 20), PredictionProvenance::ApprovedArtifactCensus);
  request.predicted_gpu.workspace = prediction(
      256 * (1ULL << 20), PredictionProvenance::PinnedRuntimeEstimate);
  request.predicted_gpu.fragmentation = prediction(
      256 * (1ULL << 20), PredictionProvenance::PinnedRuntimeEstimate);

  request.gpu.available = true;
  request.gpu.adapter_identity_available = true;
  request.gpu.adapter_luid_high = 7;
  request.gpu.adapter_luid_low = 11;
  request.gpu.captured_monotonic_milliseconds = 10'000;
  request.gpu.physical_or_reportable_local_bytes = 16 * kGiB;
  request.gpu.dxgi_local_budget_bytes = 15 * kGiB;
  request.gpu.dxgi_local_current_usage_bytes = 1 * kGiB;

  request.thermal.available = true;
  request.thermal.captured_monotonic_milliseconds = 10'000;
  request.thermal.current_celsius = 55;
  request.thermal.reported_target_celsius = 75;
  request.thermal.reported_slowdown_celsius = 80;

  request.storage.available = true;
  request.storage.free_bytes = 100 * kGiB;
  request.storage.reserve_bytes = 10 * kGiB;
  request.storage.required_incremental_bytes = 20 * kGiB;

  request.host.available = true;
  request.host.system_commit_source = "get_performance_info";
  request.host.captured_monotonic_milliseconds = 10'000;
  request.host.system_memory_total_bytes = 32 * kGiB;
  request.host.system_memory_available_bytes = 16 * kGiB;
  request.host.system_commit_total_bytes = 16 * kGiB;
  request.host.system_commit_limit_bytes = 64 * kGiB;
  request.host.system_commit_available_bytes = 48 * kGiB;
  request.host_policy = prisminfer::default_host_reserve_policy(
      prisminfer::HostAdmissionLane::DevelopmentNonPromotable);
  request.host_request.planned_incremental_resident_bytes = 4 * kGiB;
  request.host_request.planned_incremental_commit_bytes = 4 * kGiB;
  request.cell = valid_cell();
  return request;
}

prisminfer::PostContextAdmissionRequest valid_post_request() {
  prisminfer::PostContextAdmissionRequest request;
  const auto pre_request = valid_request();
  const auto pre = prisminfer::evaluate_pre_context_admission(pre_request);
  request.pre_context_receipt = pre.receipt;
  request.policy_ceiling_bytes = pre_request.policy_ceiling_bytes;
  request.requested_tier_bytes = pre_request.requested_tier_bytes;
  request.evaluation_monotonic_milliseconds = 10'300;
  request.timing = pre_request.timing;

  request.gpu = pre_request.gpu;
  request.gpu.captured_monotonic_milliseconds = 10'200;
  request.gpu.dxgi_local_budget_bytes = 14 * kGiB;
  request.gpu.dxgi_local_current_usage_bytes = 2 * kGiB;
  request.thermal = pre_request.thermal;
  request.thermal.captured_monotonic_milliseconds = 10'200;

  request.owned_gpu.available = true;
  request.owned_gpu.captured_monotonic_milliseconds =
      request.gpu.captured_monotonic_milliseconds;
  request.owned_gpu.reconciled = true;
  request.owned_gpu.process_device_corroboration_available = true;
  request.owned_gpu.process_device_source = "wddm-process";
  request.owned_gpu.process_id = 42;
  request.owned_gpu.process_device_captured_monotonic_milliseconds =
      request.owned_gpu.captured_monotonic_milliseconds;
  request.owned_gpu.adapter_identity_available = true;
  request.owned_gpu.adapter_luid_high = request.gpu.adapter_luid_high;
  request.owned_gpu.adapter_luid_low = request.gpu.adapter_luid_low;
  request.owned_gpu.hard_cap_bytes = pre.pre_context_effective_cap_bytes;
  request.owned_gpu.owned_current_bytes = 512 * (1ULL << 20);
  request.owned_gpu.process_device_current_bytes =
      request.owned_gpu.owned_current_bytes;
  request.owned_gpu.owned_peak_bytes = 512 * (1ULL << 20);
  request.owned_gpu.cuda_context_runtime_current_bytes = 512 * (1ULL << 20);
  request.owned_gpu.cuda_context_runtime_at_owned_peak_bytes =
      512 * (1ULL << 20);
  request.owned_gpu.cuda_mem_info_free_bytes = 12 * kGiB;
  request.owned_gpu.cuda_mem_info_total_bytes = 16 * kGiB;

  request.host = pre_request.host;
  request.host.captured_monotonic_milliseconds = 10'200;
  request.host_policy = pre_request.host_policy;
  request.host_request = pre_request.host_request;
  request.cell = valid_cell();
  return request;
}

}  // namespace

int main() {
  {
    auto request = valid_request();
    request.cell.runtime_identity_hash.fill(0);
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_cell_identity_invalid",
               "Stage-A rejects an incomplete exact-cell identity")) {
      return 1;
    }
  }

  {
    const auto decision =
        prisminfer::evaluate_pre_context_admission(valid_request());
    if (expect(decision.admitted, "complete bounded pre-context cell admits") ||
        expect(decision.predicted_gpu_peak_bytes == 6'979'321'856ULL,
               "predicted categories reconcile exactly") ||
        expect(decision.pre_context_live_capacity_bytes == 15 * kGiB,
               "live capacity is the physical and DXGI minimum") ||
        expect(decision.dxgi_local_headroom_bytes == 14 * kGiB,
               "current DXGI usage is removed from headroom") ||
        expect(decision.pre_context_gpu_reserve_bytes == 1'610'612'736ULL,
               "GPU reserve is exact ten-percent ceiling") ||
        expect(decision.pre_context_effective_cap_bytes == 8 * kGiB,
               "requested tier bounds the live cap") ||
        expect(decision.gpu_warning_celsius == 67 &&
                   decision.gpu_stop_celsius == 70,
               "thermal limits apply all available bounds") ||
        expect(decision.storage_payload_bytes == 90 * kGiB,
               "storage reserve is not payload") ||
        expect(decision.host_decision.admitted,
               "T-101 host decision is integrated")) {
      return 1;
    }
  }

  {
    const auto decision =
        prisminfer::evaluate_post_context_admission(valid_post_request());
    if (expect(decision.admitted,
               "complete context-only reconciliation admits") ||
        expect(decision.reconciled_owned_current_bytes == 512 * (1ULL << 20),
               "owned context categories reconcile") ||
        expect(decision.cuda_owned_capacity_bytes == 13'421'772'800ULL,
               "owned plus CUDA free capacity is checked") ||
        expect(decision.dxgi_owned_capacity_bytes == 13'421'772'800ULL,
               "owned plus DXGI headroom capacity is checked") ||
        expect(decision.post_context_live_capacity_bytes == 13'421'772'800ULL,
               "post-context live capacity uses every source") ||
        expect(decision.gpu_reserve_bytes == 1'610'612'736ULL,
               "post-context reserve cannot shrink below pre-context") ||
        expect(decision.post_context_effective_cap_bytes == 8 * kGiB,
               "post-context cap cannot grow above pre-context") ||
        expect(decision.receipt && decision.receipt->cell() == valid_cell(),
               "Stage-B receipt binds the exact admission cell")) {
      return 1;
    }
  }

  {
    auto request = valid_post_request();
    request.cell.artifact_identity_hash.fill(0);
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_cell_identity_invalid",
               "Stage-B rejects an incomplete exact-cell identity")) {
      return 1;
    }
  }

  {
    auto request = valid_post_request();
    request.cell = valid_cell(2);
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_pre_admission_cell_mismatched",
               "Stage-B cannot pair Stage-A evidence with another valid cell")) {
      return 1;
    }
  }

  {
    auto request = valid_post_request();
    request.pre_context_receipt.reset();
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_pre_admission_receipt_missing",
               "post-context cannot bypass a missing Stage-A receipt")) {
      return 1;
    }
    request = valid_post_request();
    request.policy_ceiling_bytes = 17'179'869'185ULL;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_policy_ceiling_invalid",
               "post-context independently rejects a forged hard ceiling")) {
      return 1;
    }
    request = valid_post_request();
    request.requested_tier_bytes -= 1;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_pre_admission_invalid_or_mismatched",
               "post-context requested tier is bound to pre-context")) {
      return 1;
    }
    request = valid_post_request();
    request.timing.maximum_guard_age_milliseconds -= 1;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_pre_admission_invalid_or_mismatched",
               "post-context timing is bound to pre-context")) {
      return 1;
    }
  }

  {
    auto request = valid_post_request();
    request.gpu.adapter_luid_low += 1;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_gpu_telemetry_invalid_or_stale",
               "adapter identity drift rejects")) {
      return 1;
    }
    request = valid_post_request();
    request.gpu.captured_monotonic_milliseconds = 9'799;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_gpu_telemetry_invalid_or_stale",
               "stale post-context DXGI telemetry rejects")) {
      return 1;
    }
  }

  {
    auto request = valid_post_request();
    request.owned_gpu.reconciled = false;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_owned_gpu_evidence_invalid",
               "unreconciled owned evidence rejects")) {
      return 1;
    }
    request = valid_post_request();
    request.owned_gpu.unknown_unreconciled_bytes = 1;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_owned_gpu_evidence_invalid",
               "unknown GPU bytes reject")) {
      return 1;
    }
    request = valid_post_request();
    request.owned_gpu.process_device_corroboration_available = false;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_owned_gpu_evidence_invalid",
               "missing process/device corroboration rejects")) {
      return 1;
    }
    request = valid_post_request();
    request.owned_gpu.backend_pool_current_bytes = 1;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_owned_gpu_evidence_invalid",
               "owned category mismatch rejects")) {
      return 1;
    }
    request = valid_post_request();
    request.owned_gpu.cuda_context_runtime_current_bytes = 0;
    request.owned_gpu.owned_current_bytes = 0;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_owned_gpu_evidence_invalid",
               "missing actual context bytes rejects")) {
      return 1;
    }
    request = valid_post_request();
    request.owned_gpu.hard_cap_bytes -= 1;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_owned_gpu_evidence_invalid",
               "allocator hard cap must match pre-admitted cap")) {
      return 1;
    }
  }

  {
    auto request = valid_post_request();
    request.owned_gpu.cuda_mem_info_free_bytes =
        std::numeric_limits<std::uint64_t>::max();
    request.owned_gpu.cuda_mem_info_total_bytes =
        std::numeric_limits<std::uint64_t>::max();
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_live_capacity_overflowed_or_contradictory",
               "owned plus CUDA free overflow rejects")) {
      return 1;
    }
    request = valid_post_request();
    request.owned_gpu.cuda_mem_info_free_bytes = 5 * kGiB;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_gpu_peak_exceeds_effective_cap",
               "context capacity surprise rejects before model load")) {
      return 1;
    }
  }

  {
    auto request = valid_post_request();
    request.thermal.current_celsius = 78;
    if (expect(prisminfer::evaluate_post_context_admission(request).reason ==
                   "post_context_gpu_thermal_state_unsafe",
               "post-context thermal warning blocks progression")) {
      return 1;
    }
    request = valid_post_request();
    request.host.system_commit_source = "memory_status_ex";
    const auto decision = prisminfer::evaluate_post_context_admission(request);
    if (expect(!decision.admitted &&
                   decision.reason == "post_context_host_admission_rejected" &&
                   decision.host_decision.reason ==
                       "system_commit_source_not_authoritative",
               "post-context host rejection preserves typed cause")) {
      return 1;
    }
  }

  {
    auto request = valid_request();
    request.policy_ceiling_bytes += 1;
    const auto decision = prisminfer::evaluate_pre_context_admission(request);
    if (expect(!decision.admitted &&
                   decision.reason == "pre_context_policy_ceiling_invalid",
               "policy ceiling above 16 GiB rejects")) {
      return 1;
    }
    request = valid_request();
    request.requested_tier_bytes = request.policy_ceiling_bytes + 1;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_requested_tier_invalid",
               "requested tier cannot exceed policy")) {
      return 1;
    }
  }

  {
    auto request = valid_request();
    request.context_tokens = 0;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_context_tokens_invalid",
               "zero context rejects")) {
      return 1;
    }
    request = valid_request();
    request.context_tokens =
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) +
        1;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_context_tokens_invalid",
               "context narrowing rejects truncation")) {
      return 1;
    }
    request = valid_request();
    request.run_deadline_milliseconds = 0;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_run_deadline_invalid",
               "zero run deadline rejects")) {
      return 1;
    }
    request = valid_request();
    request.run_deadline_milliseconds =
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) +
        1;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_run_deadline_invalid",
               "run deadline narrowing rejects truncation")) {
      return 1;
    }
  }

  {
    auto request = valid_request();
    request.timing.supervisor_sample_period_milliseconds = 101;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_timing_policy_invalid",
               "T-103 sample period rejects above 100 ms")) {
      return 1;
    }
    request = valid_request();
    request.timing.worker_heartbeat_period_milliseconds = 0;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_timing_policy_invalid",
               "zero heartbeat period rejects")) {
      return 1;
    }
    request = valid_request();
    request.timing.automatic_same_context_retry_count = 1;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_timing_policy_invalid",
               "T-105 automatic retry remains zero")) {
      return 1;
    }
    request = valid_request();
    request.timing.cleanup_milliseconds = 5'001;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_timing_policy_invalid",
               "T-105 cleanup deadline rejects above five seconds")) {
      return 1;
    }
    request = valid_request();
    request.timing.maximum_guard_age_milliseconds = 50;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_timing_policy_invalid",
               "guard age must cover sample and heartbeat periods")) {
      return 1;
    }
    request = valid_request();
    request.timing.worker_exit_milliseconds = 400;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_timing_policy_invalid",
               "worker exit deadline must cover cancel acknowledgement")) {
      return 1;
    }
  }

  {
    auto request = valid_request();
    request.exclusive_gpu_lease_held = false;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_lease_not_held",
               "missing exclusive lease rejects")) {
      return 1;
    }
    request = valid_request();
    request.competing_gpu_work_detected = true;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_competing_gpu_work_detected",
               "competing GPU work rejects")) {
      return 1;
    }
  }

  {
    auto request = valid_request();
    request.predicted_gpu.backend.bytes.reset();
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_prediction_incomplete",
               "a missing prediction category rejects")) {
      return 1;
    }
    request = valid_request();
    request.predicted_gpu.model =
        prediction(1, prisminfer::PredictionProvenance::ExactZeroNotApplicable);
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_prediction_incomplete",
               "exact-zero provenance cannot carry nonzero bytes")) {
      return 1;
    }
    request = valid_request();
    request.predicted_gpu.context_runtime.bytes =
        std::numeric_limits<std::uint64_t>::max();
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_prediction_overflowed",
               "predicted category overflow rejects")) {
      return 1;
    }
    request = valid_request();
    request.predicted_gpu.model.bytes = 8 * kGiB;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_peak_exceeds_effective_cap",
               "predicted peak above effective cap rejects")) {
      return 1;
    }
  }

  {
    auto request = valid_request();
    request.gpu.available = false;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_telemetry_invalid_or_stale",
               "missing GPU telemetry rejects")) {
      return 1;
    }
    request = valid_request();
    request.gpu.captured_monotonic_milliseconds = 9'599;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_telemetry_invalid_or_stale",
               "stale GPU telemetry rejects")) {
      return 1;
    }
    request = valid_request();
    request.gpu.adapter_identity_available = false;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_telemetry_invalid_or_stale",
               "missing pre-context adapter identity rejects")) {
      return 1;
    }
    request = valid_request();
    request.gpu.dxgi_local_current_usage_bytes = 16 * kGiB;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_telemetry_invalid_or_stale",
               "contradictory DXGI telemetry rejects")) {
      return 1;
    }
    request = valid_request();
    request.requested_tier_bytes = 16 * kGiB;
    request.predicted_gpu.model.bytes = 14 * kGiB;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_peak_exceeds_effective_cap",
               "nominal 16 GiB is not an allocation target")) {
      return 1;
    }
    request = valid_request();
    request.gpu.dxgi_local_current_usage_bytes = 14 * kGiB;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_peak_exceeds_effective_cap",
               "high valid DXGI usage consumes admission headroom")) {
      return 1;
    }
  }

  {
    auto request = valid_request();
    request.thermal.available = false;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_thermal_telemetry_invalid_or_stale",
               "missing thermal telemetry rejects")) {
      return 1;
    }
    request = valid_request();
    request.thermal.available = false;
    request.thermal.unavailable_reason =
        prisminfer::GpuThermalUnavailableReason::SlowdownTlimitTypeInvalid;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_thermal_telemetry_invalid_or_stale:"
                   "nvml_slowdown_tlimit_type_invalid",
               "typed thermal acquisition failure is retained")) {
      return 1;
    }
    request = valid_request();
    request.thermal.current_celsius = 78;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_thermal_state_unsafe",
               "warning temperature blocks admission scale-up")) {
      return 1;
    }
    request = valid_request();
    request.thermal.power_brake_slowdown = true;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_thermal_state_unsafe",
               "power-brake slowdown rejects")) {
      return 1;
    }
    request = valid_request();
    request.thermal.reported_target_celsius = 4;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_gpu_thermal_bounds_invalid",
               "invalid device thermal bounds reject without underflow")) {
      return 1;
    }
  }

  {
    auto request = valid_request();
    request.storage.available = false;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_storage_telemetry_unavailable",
               "missing storage telemetry rejects")) {
      return 1;
    }
    request = valid_request();
    request.storage.reserve_bytes = request.storage.free_bytes + 1;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_storage_reserve_unavailable",
               "storage reserve is mandatory")) {
      return 1;
    }
    request = valid_request();
    request.storage.required_incremental_bytes = 91 * kGiB;
    if (expect(prisminfer::evaluate_pre_context_admission(request).reason ==
                   "pre_context_storage_payload_exceeded",
               "storage requirement above payload rejects")) {
      return 1;
    }
  }

  {
    auto request = valid_request();
    request.host.system_commit_source = "memory_status_ex";
    const auto decision = prisminfer::evaluate_pre_context_admission(request);
    if (expect(!decision.admitted &&
                   decision.reason == "pre_context_host_admission_rejected" &&
                   decision.host_decision.reason ==
                       "system_commit_source_not_authoritative",
               "host rejection preserves its typed cause")) {
      return 1;
    }
    request = valid_request();
    request.host_request.planned_incremental_commit_bytes = 60 * kGiB;
    const auto pressure = prisminfer::evaluate_pre_context_admission(request);
    if (expect(!pressure.admitted && pressure.host_decision.reason ==
                                         "planned_commit_peak_exceeds_payload",
               "commit pressure rejects independently")) {
      return 1;
    }
  }

  {
    const auto pre_input = valid_request();
    const auto pre = prisminfer::evaluate_pre_context_admission(pre_input);
    if (expect(pre.receipt &&
                   pre.receipt->run_started_monotonic_milliseconds() ==
                       pre_input.evaluation_monotonic_milliseconds &&
                   pre.receipt->run_deadline_monotonic_milliseconds() ==
                       70'100,
               "Stage A binds one absolute run deadline")) {
      return 1;
    }
    auto post = valid_post_request();
    post.host_request.planned_incremental_commit_bytes += 1;
    if (expect(prisminfer::evaluate_post_context_admission(post).reason ==
                   "post_context_pre_admission_invalid_or_mismatched",
               "Stage B cannot loosen or replace the admitted host workload")) {
      return 1;
    }
    post = valid_post_request();
    post.host_policy.lane =
        prisminfer::HostAdmissionLane::EvidencePromotable;
    if (expect(prisminfer::evaluate_post_context_admission(post).reason ==
                   "post_context_pre_admission_invalid_or_mismatched",
               "Stage B cannot replace the admitted host policy")) {
      return 1;
    }
    post = valid_post_request();
    post.thermal.reported_target_celsius.reset();
    post.thermal.reported_slowdown_celsius.reset();
    if (expect(prisminfer::evaluate_post_context_admission(post).reason ==
                   "post_context_gpu_thermal_state_unsafe",
               "Stage B cannot discard tighter device thermal bounds")) {
      return 1;
    }
  }

  return 0;
}

#include "prisminfer/supervisor_admission.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <utility>

#include "prisminfer/checked_arithmetic.h"
#include "prisminfer/gpu_cap_policy.h"

namespace prisminfer {

PreContextAdmissionReceipt::PreContextAdmissionReceipt(
    std::uint64_t policy_ceiling_bytes, std::uint64_t requested_tier_bytes,
    SupervisorTimingPolicy timing, std::int32_t adapter_luid_high,
    std::uint32_t adapter_luid_low, std::uint64_t predicted_gpu_peak_bytes,
    std::uint64_t pre_context_gpu_reserve_bytes,
    std::uint64_t pre_context_effective_cap_bytes,
    AdmissionCellIdentity cell)
    : policy_ceiling_bytes_(policy_ceiling_bytes),
      requested_tier_bytes_(requested_tier_bytes),
      timing_(timing),
      adapter_luid_high_(adapter_luid_high),
      adapter_luid_low_(adapter_luid_low),
      predicted_gpu_peak_bytes_(predicted_gpu_peak_bytes),
      pre_context_gpu_reserve_bytes_(pre_context_gpu_reserve_bytes),
      pre_context_effective_cap_bytes_(pre_context_effective_cap_bytes),
      cell_(std::move(cell)) {}

std::uint64_t PreContextAdmissionReceipt::policy_ceiling_bytes() const {
  return policy_ceiling_bytes_;
}

std::uint64_t PreContextAdmissionReceipt::requested_tier_bytes() const {
  return requested_tier_bytes_;
}

const SupervisorTimingPolicy& PreContextAdmissionReceipt::timing() const {
  return timing_;
}

std::int32_t PreContextAdmissionReceipt::adapter_luid_high() const {
  return adapter_luid_high_;
}

std::uint32_t PreContextAdmissionReceipt::adapter_luid_low() const {
  return adapter_luid_low_;
}

std::uint64_t PreContextAdmissionReceipt::predicted_gpu_peak_bytes() const {
  return predicted_gpu_peak_bytes_;
}

std::uint64_t PreContextAdmissionReceipt::pre_context_gpu_reserve_bytes()
    const {
  return pre_context_gpu_reserve_bytes_;
}

std::uint64_t PreContextAdmissionReceipt::pre_context_effective_cap_bytes()
    const {
  return pre_context_effective_cap_bytes_;
}

const AdmissionCellIdentity& PreContextAdmissionReceipt::cell() const {
  return cell_;
}

PostContextAdmissionReceipt::PostContextAdmissionReceipt(
    std::uint64_t receipt_id, std::uint64_t policy_ceiling_bytes,
    std::uint64_t requested_tier_bytes, SupervisorTimingPolicy timing,
    std::int32_t adapter_luid_high, std::uint32_t adapter_luid_low,
    std::uint64_t predicted_gpu_peak_bytes, std::uint64_t gpu_reserve_bytes,
    std::uint64_t effective_cap_bytes,
    std::uint64_t evaluation_monotonic_milliseconds,
    AdmissionCellIdentity cell)
    : receipt_id_(receipt_id),
      policy_ceiling_bytes_(policy_ceiling_bytes),
      requested_tier_bytes_(requested_tier_bytes),
      timing_(timing),
      adapter_luid_high_(adapter_luid_high),
      adapter_luid_low_(adapter_luid_low),
      predicted_gpu_peak_bytes_(predicted_gpu_peak_bytes),
      gpu_reserve_bytes_(gpu_reserve_bytes),
      effective_cap_bytes_(effective_cap_bytes),
      evaluation_monotonic_milliseconds_(evaluation_monotonic_milliseconds),
      cell_(std::move(cell)),
      token_consumed_(std::make_shared<std::atomic<bool>>(false)) {}

std::uint64_t PostContextAdmissionReceipt::receipt_id() const {
  return receipt_id_;
}

std::uint64_t PostContextAdmissionReceipt::policy_ceiling_bytes() const {
  return policy_ceiling_bytes_;
}

std::uint64_t PostContextAdmissionReceipt::requested_tier_bytes() const {
  return requested_tier_bytes_;
}

const SupervisorTimingPolicy& PostContextAdmissionReceipt::timing() const {
  return timing_;
}

std::int32_t PostContextAdmissionReceipt::adapter_luid_high() const {
  return adapter_luid_high_;
}

std::uint32_t PostContextAdmissionReceipt::adapter_luid_low() const {
  return adapter_luid_low_;
}

std::uint64_t PostContextAdmissionReceipt::predicted_gpu_peak_bytes() const {
  return predicted_gpu_peak_bytes_;
}

std::uint64_t PostContextAdmissionReceipt::gpu_reserve_bytes() const {
  return gpu_reserve_bytes_;
}

std::uint64_t PostContextAdmissionReceipt::effective_cap_bytes() const {
  return effective_cap_bytes_;
}

std::uint64_t PostContextAdmissionReceipt::evaluation_monotonic_milliseconds()
    const {
  return evaluation_monotonic_milliseconds_;
}

const AdmissionCellIdentity& PostContextAdmissionReceipt::cell() const {
  return cell_;
}

bool PostContextAdmissionReceipt::consume_for_token() const {
  bool expected = false;
  return token_consumed_ && token_consumed_->compare_exchange_strong(
                                expected, true, std::memory_order_acq_rel,
                                std::memory_order_acquire);
}

namespace {

std::atomic<std::uint64_t> g_post_context_receipt_sequence{1};

std::optional<std::uint64_t> next_post_context_receipt_id() {
  auto current =
      g_post_context_receipt_sequence.load(std::memory_order_relaxed);
  for (;;) {
    if (current == 0 || current == std::numeric_limits<std::uint64_t>::max()) {
      return std::nullopt;
    }
    if (g_post_context_receipt_sequence.compare_exchange_weak(
            current, current + 1, std::memory_order_relaxed,
            std::memory_order_relaxed)) {
      return current;
    }
  }
}

constexpr std::uint64_t kOneGiB = 1ULL << 30;
constexpr std::uint64_t kMaximumSupervisorSampleMilliseconds = 100;
constexpr std::uint64_t kMaximumHeartbeatMilliseconds = 250;
constexpr std::uint64_t kMaximumGuardAgeMilliseconds = 500;
constexpr std::uint64_t kMaximumCancelAckMilliseconds = 500;
constexpr std::uint64_t kMaximumWorkerExitMilliseconds = 2'000;
constexpr std::uint64_t kMaximumCleanupMilliseconds = 5'000;
constexpr std::uint64_t kMaximumDispatchBoundMilliseconds = 500;

PreContextAdmissionDecision reject(PreContextAdmissionDecision decision,
                                   const char* reason) {
  decision.reason = reason;
  return decision;
}

bool valid_bounded_period(std::uint64_t value, std::uint64_t maximum) {
  return value > 0 && value <= maximum;
}

bool same_timing_policy(const SupervisorTimingPolicy& left,
                        const SupervisorTimingPolicy& right) {
  return left.supervisor_sample_period_milliseconds ==
             right.supervisor_sample_period_milliseconds &&
         left.worker_heartbeat_period_milliseconds ==
             right.worker_heartbeat_period_milliseconds &&
         left.maximum_guard_age_milliseconds ==
             right.maximum_guard_age_milliseconds &&
         left.cooperative_cancel_ack_milliseconds ==
             right.cooperative_cancel_ack_milliseconds &&
         left.worker_exit_milliseconds == right.worker_exit_milliseconds &&
         left.cleanup_milliseconds == right.cleanup_milliseconds &&
         left.declared_dispatch_bound_milliseconds ==
             right.declared_dispatch_bound_milliseconds &&
         left.automatic_same_context_retry_count ==
             right.automatic_same_context_retry_count;
}

bool sample_is_fresh(std::uint64_t captured, std::uint64_t evaluated,
                     std::uint64_t maximum_age) {
  return captured > 0 && evaluated > 0 && evaluated >= captured &&
         evaluated - captured <= maximum_age;
}

bool valid_temperature(std::int32_t value) {
  return value >= -100 && value <= 200;
}

std::optional<std::uint64_t> predicted_gpu_peak(
    const PredictedGpuFootprint& prediction) {
  auto total = checked_add_u64(*prediction.context_runtime.bytes,
                               *prediction.backend.bytes);
  if (total) total = checked_add_u64(*total, *prediction.model.bytes);
  if (total) total = checked_add_u64(*total, *prediction.state.bytes);
  if (total) total = checked_add_u64(*total, *prediction.workspace.bytes);
  if (total) {
    total = checked_add_u64(*total, *prediction.fragmentation.bytes);
  }
  return total;
}

bool prediction_category_is_valid(const PredictedBytes& category) {
  if (!category.bytes ||
      category.provenance == PredictionProvenance::Unavailable) {
    return false;
  }
  if (category.provenance == PredictionProvenance::ExactZeroNotApplicable) {
    return *category.bytes == 0;
  }
  return category.provenance == PredictionProvenance::ApprovedArtifactCensus ||
         category.provenance == PredictionProvenance::PinnedRuntimeEstimate;
}

bool prediction_is_complete(const PredictedGpuFootprint& prediction) {
  return prediction_category_is_valid(prediction.context_runtime) &&
         prediction_category_is_valid(prediction.backend) &&
         prediction_category_is_valid(prediction.model) &&
         prediction_category_is_valid(prediction.state) &&
         prediction_category_is_valid(prediction.workspace) &&
         prediction_category_is_valid(prediction.fragmentation) &&
         *prediction.context_runtime.bytes > 0;
}

struct ThermalDecision {
  std::int32_t warning_celsius{78};
  std::int32_t stop_celsius{82};
  bool safe_for_admission{false};
};

std::optional<ThermalDecision> evaluate_thermal(const GpuThermalSample& thermal,
                                                std::uint64_t evaluated,
                                                std::uint64_t maximum_age) {
  if (!thermal.available ||
      !sample_is_fresh(thermal.captured_monotonic_milliseconds, evaluated,
                       maximum_age) ||
      !valid_temperature(thermal.current_celsius) ||
      (thermal.reported_target_celsius &&
       !valid_temperature(*thermal.reported_target_celsius)) ||
      (thermal.reported_slowdown_celsius &&
       !valid_temperature(*thermal.reported_slowdown_celsius))) {
    return std::nullopt;
  }
  ThermalDecision decision;
  if (thermal.reported_target_celsius) {
    decision.warning_celsius = std::min(decision.warning_celsius,
                                        *thermal.reported_target_celsius - 8);
    decision.stop_celsius =
        std::min(decision.stop_celsius, *thermal.reported_target_celsius - 5);
  }
  if (thermal.reported_slowdown_celsius) {
    decision.stop_celsius =
        std::min(decision.stop_celsius, *thermal.reported_slowdown_celsius - 5);
  }
  if (decision.warning_celsius <= 0 || decision.stop_celsius <= 0 ||
      decision.warning_celsius >= decision.stop_celsius) {
    return ThermalDecision{decision.warning_celsius, decision.stop_celsius,
                           false};
  }
  decision.safe_for_admission =
      !thermal.thermal_throttling && !thermal.power_brake_slowdown &&
      thermal.current_celsius < decision.warning_celsius;
  return decision;
}

std::optional<std::uint64_t> reconcile_owned_current(
    const OwnedGpuMemoryEvidence& gpu) {
  const std::uint64_t categories[] = {gpu.backend_buffer_current_bytes,
                                      gpu.backend_pool_current_bytes,
                                      gpu.kernel_workspace_current_bytes,
                                      gpu.activation_current_bytes,
                                      gpu.kv_current_bytes,
                                      gpu.graph_current_bytes,
                                      gpu.profiler_workspace_current_bytes,
                                      gpu.cuda_context_runtime_current_bytes};
  std::optional<std::uint64_t> total{0};
  for (const auto category : categories) {
    total = checked_add_u64(*total, category);
    if (!total) return std::nullopt;
  }
  return total;
}

}  // namespace

PreContextAdmissionDecision evaluate_pre_context_admission(
    const PreContextAdmissionRequest& request) {
  PreContextAdmissionDecision decision;
  decision.policy_ceiling_bytes = request.policy_ceiling_bytes;
  decision.requested_tier_bytes = request.requested_tier_bytes;
  decision.timing = request.timing;

  if (!admission_cell_identity_valid(request.cell)) {
    return reject(decision, "pre_context_cell_identity_invalid");
  }

  const auto cap_policy = validate_gpu_hard_cap(request.policy_ceiling_bytes);
  if (!cap_policy.accepted) {
    return reject(decision, "pre_context_policy_ceiling_invalid");
  }
  if (request.requested_tier_bytes == 0 ||
      request.requested_tier_bytes > request.policy_ceiling_bytes) {
    return reject(decision, "pre_context_requested_tier_invalid");
  }
  if (!checked_narrow_u32(request.context_tokens) ||
      request.context_tokens == 0) {
    return reject(decision, "pre_context_context_tokens_invalid");
  }
  if (!checked_narrow_u32(request.run_deadline_milliseconds) ||
      request.run_deadline_milliseconds == 0) {
    return reject(decision, "pre_context_run_deadline_invalid");
  }

  const auto& timing = request.timing;
  if (!valid_bounded_period(timing.supervisor_sample_period_milliseconds,
                            kMaximumSupervisorSampleMilliseconds) ||
      !valid_bounded_period(timing.worker_heartbeat_period_milliseconds,
                            kMaximumHeartbeatMilliseconds) ||
      !valid_bounded_period(timing.maximum_guard_age_milliseconds,
                            kMaximumGuardAgeMilliseconds) ||
      !valid_bounded_period(timing.cooperative_cancel_ack_milliseconds,
                            kMaximumCancelAckMilliseconds) ||
      !valid_bounded_period(timing.worker_exit_milliseconds,
                            kMaximumWorkerExitMilliseconds) ||
      !valid_bounded_period(timing.cleanup_milliseconds,
                            kMaximumCleanupMilliseconds) ||
      !valid_bounded_period(timing.declared_dispatch_bound_milliseconds,
                            kMaximumDispatchBoundMilliseconds) ||
      timing.maximum_guard_age_milliseconds <
          timing.supervisor_sample_period_milliseconds ||
      timing.maximum_guard_age_milliseconds <
          timing.worker_heartbeat_period_milliseconds ||
      timing.worker_exit_milliseconds <
          timing.cooperative_cancel_ack_milliseconds ||
      timing.cleanup_milliseconds < timing.worker_exit_milliseconds ||
      timing.automatic_same_context_retry_count != 0) {
    return reject(decision, "pre_context_timing_policy_invalid");
  }
  if (!request.exclusive_gpu_lease_held) {
    return reject(decision, "pre_context_gpu_lease_not_held");
  }
  if (request.competing_gpu_work_detected) {
    return reject(decision, "pre_context_competing_gpu_work_detected");
  }

  if (!prediction_is_complete(request.predicted_gpu)) {
    return reject(decision, "pre_context_gpu_prediction_incomplete");
  }
  const auto predicted_peak = predicted_gpu_peak(request.predicted_gpu);
  if (!predicted_peak) {
    return reject(decision, "pre_context_gpu_prediction_overflowed");
  }
  decision.predicted_gpu_peak_bytes = *predicted_peak;

  if (!request.gpu.available || !request.gpu.adapter_identity_available ||
      !sample_is_fresh(request.gpu.captured_monotonic_milliseconds,
                       request.evaluation_monotonic_milliseconds,
                       timing.maximum_guard_age_milliseconds) ||
      request.gpu.physical_or_reportable_local_bytes == 0 ||
      request.gpu.dxgi_local_budget_bytes == 0 ||
      request.gpu.dxgi_local_current_usage_bytes >
          request.gpu.dxgi_local_budget_bytes) {
    return reject(decision, "pre_context_gpu_telemetry_invalid_or_stale");
  }
  decision.pre_context_live_capacity_bytes =
      std::min(request.gpu.physical_or_reportable_local_bytes,
               request.gpu.dxgi_local_budget_bytes);
  decision.adapter_luid_high = request.gpu.adapter_luid_high;
  decision.adapter_luid_low = request.gpu.adapter_luid_low;
  decision.dxgi_local_headroom_bytes =
      request.gpu.dxgi_local_budget_bytes -
      request.gpu.dxgi_local_current_usage_bytes;
  const auto fractional_reserve = checked_basis_points_u64(
      decision.pre_context_live_capacity_bytes, 1'000, RatioRounding::Up);
  if (!fractional_reserve) {
    return reject(decision, "pre_context_gpu_reserve_overflowed");
  }
  decision.pre_context_gpu_reserve_bytes =
      std::max(kOneGiB, *fractional_reserve);
  const auto bounded_live = std::min(request.policy_ceiling_bytes,
                                     decision.pre_context_live_capacity_bytes);
  const auto reserve_adjusted =
      bounded_live > decision.pre_context_gpu_reserve_bytes
          ? bounded_live - decision.pre_context_gpu_reserve_bytes
          : 0;
  const auto dxgi_reserve_adjusted =
      decision.dxgi_local_headroom_bytes >
              decision.pre_context_gpu_reserve_bytes
          ? decision.dxgi_local_headroom_bytes -
                decision.pre_context_gpu_reserve_bytes
          : 0;
  decision.pre_context_effective_cap_bytes = std::min(
      {request.requested_tier_bytes, reserve_adjusted, dxgi_reserve_adjusted});
  if (decision.predicted_gpu_peak_bytes >
      decision.pre_context_effective_cap_bytes) {
    return reject(decision, "pre_context_gpu_peak_exceeds_effective_cap");
  }

  const auto thermal = evaluate_thermal(
      request.thermal, request.evaluation_monotonic_milliseconds,
      timing.maximum_guard_age_milliseconds);
  if (!thermal) {
    return reject(decision,
                  "pre_context_gpu_thermal_telemetry_invalid_or_stale");
  }
  decision.gpu_warning_celsius = thermal->warning_celsius;
  decision.gpu_stop_celsius = thermal->stop_celsius;
  if (decision.gpu_warning_celsius <= 0 || decision.gpu_stop_celsius <= 0 ||
      decision.gpu_warning_celsius >= decision.gpu_stop_celsius) {
    return reject(decision, "pre_context_gpu_thermal_bounds_invalid");
  }
  if (!thermal->safe_for_admission) {
    return reject(decision, "pre_context_gpu_thermal_state_unsafe");
  }

  if (!request.storage.available) {
    return reject(decision, "pre_context_storage_telemetry_unavailable");
  }
  if (request.storage.free_bytes < request.storage.reserve_bytes) {
    return reject(decision, "pre_context_storage_reserve_unavailable");
  }
  decision.storage_payload_bytes =
      request.storage.free_bytes - request.storage.reserve_bytes;
  if (request.storage.required_incremental_bytes >
      decision.storage_payload_bytes) {
    return reject(decision, "pre_context_storage_payload_exceeded");
  }

  auto host_request = request.host_request;
  host_request.evaluation_monotonic_milliseconds =
      request.evaluation_monotonic_milliseconds;
  host_request.max_telemetry_age_milliseconds =
      timing.maximum_guard_age_milliseconds;
  decision.host_decision =
      evaluate_host_admission(request.host, request.host_policy, host_request);
  if (!decision.host_decision.admitted) {
    return reject(decision, "pre_context_host_admission_rejected");
  }

  decision.admitted = true;
  decision.reason = "admitted";
  decision.receipt = PreContextAdmissionReceipt(
      decision.policy_ceiling_bytes, decision.requested_tier_bytes,
      decision.timing, decision.adapter_luid_high, decision.adapter_luid_low,
      decision.predicted_gpu_peak_bytes, decision.pre_context_gpu_reserve_bytes,
      decision.pre_context_effective_cap_bytes, request.cell);
  return decision;
}

PostContextAdmissionDecision evaluate_post_context_admission(
    const PostContextAdmissionRequest& request) {
  PostContextAdmissionDecision decision;
  if (!admission_cell_identity_valid(request.cell)) {
    decision.reason = "post_context_cell_identity_invalid";
    return decision;
  }
  const auto cap_policy = validate_gpu_hard_cap(request.policy_ceiling_bytes);
  if (!cap_policy.accepted) {
    decision.reason = "post_context_policy_ceiling_invalid";
    return decision;
  }
  if (!request.pre_context_receipt) {
    decision.reason = "post_context_pre_admission_receipt_missing";
    return decision;
  }
  const auto& pre = *request.pre_context_receipt;
  if (pre.cell() != request.cell) {
    decision.reason = "post_context_pre_admission_cell_mismatched";
    return decision;
  }
  if (request.policy_ceiling_bytes != pre.policy_ceiling_bytes() ||
      request.requested_tier_bytes != pre.requested_tier_bytes() ||
      !same_timing_policy(request.timing, pre.timing()) ||
      pre.pre_context_gpu_reserve_bytes() < kOneGiB ||
      pre.pre_context_effective_cap_bytes() == 0 ||
      pre.pre_context_effective_cap_bytes() > pre.requested_tier_bytes() ||
      pre.requested_tier_bytes() > pre.policy_ceiling_bytes() ||
      pre.predicted_gpu_peak_bytes() > pre.pre_context_effective_cap_bytes()) {
    decision.reason = "post_context_pre_admission_invalid_or_mismatched";
    return decision;
  }

  if (!request.gpu.available || !request.gpu.adapter_identity_available ||
      request.gpu.adapter_luid_high != pre.adapter_luid_high() ||
      request.gpu.adapter_luid_low != pre.adapter_luid_low() ||
      !sample_is_fresh(request.gpu.captured_monotonic_milliseconds,
                       request.evaluation_monotonic_milliseconds,
                       request.timing.maximum_guard_age_milliseconds) ||
      request.gpu.physical_or_reportable_local_bytes == 0 ||
      request.gpu.dxgi_local_budget_bytes == 0 ||
      request.gpu.dxgi_local_current_usage_bytes >
          request.gpu.dxgi_local_budget_bytes) {
    decision.reason = "post_context_gpu_telemetry_invalid_or_stale";
    return decision;
  }

  const auto& owned = request.owned_gpu;
  const auto reconciled = reconcile_owned_current(owned);
  if (!owned.available || !owned.reconciled ||
      !owned.process_device_corroboration_available ||
      !owned.adapter_identity_available ||
      owned.adapter_luid_high != request.gpu.adapter_luid_high ||
      owned.adapter_luid_low != request.gpu.adapter_luid_low ||
      owned.unknown_unreconciled_bytes != 0 || !reconciled ||
      *reconciled != owned.owned_current_bytes ||
      owned.cuda_context_runtime_current_bytes == 0 ||
      owned.owned_current_bytes > owned.owned_peak_bytes ||
      owned.hard_cap_bytes != pre.pre_context_effective_cap_bytes() ||
      owned.owned_peak_bytes > owned.hard_cap_bytes ||
      owned.cuda_mem_info_total_bytes == 0 ||
      owned.cuda_mem_info_free_bytes > owned.cuda_mem_info_total_bytes ||
      request.gpu.dxgi_local_current_usage_bytes < owned.owned_current_bytes) {
    decision.reason = "post_context_owned_gpu_evidence_invalid";
    return decision;
  }
  decision.reconciled_owned_current_bytes = *reconciled;

  const auto cuda_owned_capacity = checked_add_u64(
      owned.owned_current_bytes, owned.cuda_mem_info_free_bytes);
  const auto dxgi_headroom = request.gpu.dxgi_local_budget_bytes -
                             request.gpu.dxgi_local_current_usage_bytes;
  const auto dxgi_owned_capacity =
      checked_add_u64(owned.owned_current_bytes, dxgi_headroom);
  if (!cuda_owned_capacity || !dxgi_owned_capacity ||
      *cuda_owned_capacity > owned.cuda_mem_info_total_bytes) {
    decision.reason = "post_context_live_capacity_overflowed_or_contradictory";
    return decision;
  }
  decision.cuda_owned_capacity_bytes = *cuda_owned_capacity;
  decision.dxgi_owned_capacity_bytes = *dxgi_owned_capacity;
  decision.post_context_live_capacity_bytes =
      std::min({request.gpu.physical_or_reportable_local_bytes,
                request.gpu.dxgi_local_budget_bytes, *cuda_owned_capacity,
                *dxgi_owned_capacity});

  const auto fractional_reserve = checked_basis_points_u64(
      decision.post_context_live_capacity_bytes, 1'000, RatioRounding::Up);
  if (!fractional_reserve) {
    decision.reason = "post_context_gpu_reserve_overflowed";
    return decision;
  }
  decision.gpu_reserve_bytes = std::max(
      {pre.pre_context_gpu_reserve_bytes(), kOneGiB, *fractional_reserve});
  const auto bounded_live = std::min(request.policy_ceiling_bytes,
                                     decision.post_context_live_capacity_bytes);
  const auto reserve_adjusted = bounded_live > decision.gpu_reserve_bytes
                                    ? bounded_live - decision.gpu_reserve_bytes
                                    : 0;
  decision.post_context_effective_cap_bytes =
      std::min({pre.pre_context_effective_cap_bytes(),
                request.requested_tier_bytes, reserve_adjusted});
  if (owned.owned_current_bytes > decision.post_context_effective_cap_bytes ||
      pre.predicted_gpu_peak_bytes() >
          decision.post_context_effective_cap_bytes) {
    decision.reason = "post_context_gpu_peak_exceeds_effective_cap";
    return decision;
  }

  const auto thermal = evaluate_thermal(
      request.thermal, request.evaluation_monotonic_milliseconds,
      request.timing.maximum_guard_age_milliseconds);
  if (!thermal) {
    decision.reason = "post_context_gpu_thermal_telemetry_invalid_or_stale";
    return decision;
  }
  if (thermal->warning_celsius <= 0 || thermal->stop_celsius <= 0 ||
      thermal->warning_celsius >= thermal->stop_celsius ||
      !thermal->safe_for_admission) {
    decision.reason = "post_context_gpu_thermal_state_unsafe";
    return decision;
  }

  auto host_request = request.host_request;
  host_request.evaluation_monotonic_milliseconds =
      request.evaluation_monotonic_milliseconds;
  host_request.max_telemetry_age_milliseconds =
      request.timing.maximum_guard_age_milliseconds;
  decision.host_decision =
      evaluate_host_admission(request.host, request.host_policy, host_request);
  if (!decision.host_decision.admitted) {
    decision.reason = "post_context_host_admission_rejected";
    return decision;
  }

  const auto receipt_id = next_post_context_receipt_id();
  if (!receipt_id) {
    decision.reason = "post_context_receipt_sequence_exhausted";
    return decision;
  }
  decision.admitted = true;
  decision.reason = "admitted";
  decision.receipt = PostContextAdmissionReceipt(
      *receipt_id, request.policy_ceiling_bytes, request.requested_tier_bytes,
      request.timing, request.gpu.adapter_luid_high,
      request.gpu.adapter_luid_low, pre.predicted_gpu_peak_bytes(),
      decision.gpu_reserve_bytes, decision.post_context_effective_cap_bytes,
      request.evaluation_monotonic_milliseconds, request.cell);
  return decision;
}

}  // namespace prisminfer

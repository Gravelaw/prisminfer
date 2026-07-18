#include "prisminfer/supervisor_watchdog.h"

#include <algorithm>
#include <optional>

#include "prisminfer/checked_arithmetic.h"

namespace prisminfer {
namespace {

bool fresh(std::uint64_t captured, std::uint64_t evaluated,
           std::uint64_t maximum_age) {
  return captured != 0 && evaluated >= captured &&
         evaluated - captured <= maximum_age;
}

bool valid_temperature(std::int32_t value) {
  return value > -100 && value < 200;
}

struct ThermalBounds {
  std::int32_t warning{78};
  std::int32_t stop{82};
};

std::optional<ThermalBounds> thermal_bounds(
    const GpuThermalSample& sample) {
  if (!valid_temperature(sample.current_celsius) ||
      (sample.reported_target_celsius &&
       !valid_temperature(*sample.reported_target_celsius)) ||
      (sample.reported_slowdown_celsius &&
       !valid_temperature(*sample.reported_slowdown_celsius))) {
    return std::nullopt;
  }
  ThermalBounds bounds;
  if (sample.reported_target_celsius) {
    bounds.warning =
        std::min(bounds.warning, *sample.reported_target_celsius - 8);
    bounds.stop = std::min(bounds.stop, *sample.reported_target_celsius - 5);
  }
  if (sample.reported_slowdown_celsius) {
    bounds.stop =
        std::min(bounds.stop, *sample.reported_slowdown_celsius - 5);
  }
  return bounds.warning > 0 && bounds.stop > 0 &&
                 bounds.warning < bounds.stop
             ? std::optional<ThermalBounds>(bounds)
             : std::nullopt;
}

std::optional<std::uint64_t> reconciled_owned_current(
    const OwnedGpuMemoryEvidence& owned) {
  const std::uint64_t categories[] = {
      owned.backend_buffer_current_bytes,
      owned.backend_pool_current_bytes,
      owned.kernel_workspace_current_bytes,
      owned.activation_current_bytes,
      owned.kv_current_bytes,
      owned.graph_current_bytes,
      owned.profiler_workspace_current_bytes,
      owned.cuda_context_runtime_current_bytes};
  std::optional<std::uint64_t> total{0};
  for (const auto category : categories) {
    total = checked_add_u64(*total, category);
    if (!total) return std::nullopt;
  }
  return total;
}

SupervisorWatchdogDecision cancel(WatchdogReason reason) {
  SupervisorWatchdogDecision decision;
  decision.reason = reason;
  return decision;
}

}  // namespace

SupervisorWatchdogDecision evaluate_supervisor_watchdog(
    const PostContextAdmissionReceipt& admission,
    const SupervisorWatchdogSample& sample) {
  const auto maximum_age = admission.timing().maximum_guard_age_milliseconds;
  if (!sample.worker_alive) return cancel(WatchdogReason::WorkerUnavailable);
  if (sample.context_fatal_error) return cancel(WatchdogReason::ContextFatal);
  if (sample.evaluated_monotonic_milliseconds == 0 ||
      sample.run_deadline_monotonic_milliseconds == 0 ||
      sample.evaluated_monotonic_milliseconds >=
          sample.run_deadline_monotonic_milliseconds) {
    return cancel(WatchdogReason::DeadlineReached);
  }
  if (!fresh(sample.worker_heartbeat_monotonic_milliseconds,
             sample.evaluated_monotonic_milliseconds, maximum_age)) {
    return cancel(WatchdogReason::HeartbeatInvalidOrStale);
  }
  if (!sample.gpu.available || !sample.gpu.adapter_identity_available ||
      !fresh(sample.gpu.captured_monotonic_milliseconds,
             sample.evaluated_monotonic_milliseconds, maximum_age) ||
      sample.gpu.physical_or_reportable_local_bytes == 0 ||
      sample.gpu.dxgi_local_budget_bytes == 0 ||
      sample.gpu.dxgi_local_current_usage_bytes >
          sample.gpu.dxgi_local_budget_bytes) {
    return cancel(WatchdogReason::GpuTelemetryInvalidOrStale);
  }
  if (sample.gpu.adapter_luid_high != admission.adapter_luid_high() ||
      sample.gpu.adapter_luid_low != admission.adapter_luid_low()) {
    return cancel(WatchdogReason::AdapterIdentityMismatch);
  }
  const auto owned_total = reconciled_owned_current(sample.owned_gpu);
  if (!sample.owned_gpu.available || !sample.owned_gpu.reconciled ||
      !sample.owned_gpu.process_device_corroboration_available ||
      !sample.owned_gpu.adapter_identity_available || !owned_total ||
      *owned_total != sample.owned_gpu.owned_current_bytes ||
      sample.owned_gpu.unknown_unreconciled_bytes != 0 ||
      sample.owned_gpu.cuda_context_runtime_current_bytes == 0 ||
      sample.owned_gpu.adapter_luid_high != admission.adapter_luid_high() ||
      sample.owned_gpu.adapter_luid_low != admission.adapter_luid_low() ||
      sample.owned_gpu.hard_cap_bytes != admission.effective_cap_bytes() ||
      sample.owned_gpu.owned_current_bytes >
          sample.owned_gpu.owned_peak_bytes ||
      sample.owned_gpu.owned_peak_bytes > admission.effective_cap_bytes() ||
      sample.owned_gpu.cuda_mem_info_total_bytes == 0 ||
      sample.owned_gpu.cuda_mem_info_free_bytes >
          sample.owned_gpu.cuda_mem_info_total_bytes ||
      sample.gpu.dxgi_local_current_usage_bytes <
          sample.owned_gpu.owned_current_bytes) {
    return cancel(WatchdogReason::OwnedGpuEvidenceInvalid);
  }
  const auto owned_plus_free = checked_add_u64(
      sample.owned_gpu.owned_current_bytes,
      sample.owned_gpu.cuda_mem_info_free_bytes);
  if (!owned_plus_free) return cancel(WatchdogReason::LiveCapacityOverflow);
  const auto dxgi_headroom = sample.gpu.dxgi_local_budget_bytes -
                             sample.gpu.dxgi_local_current_usage_bytes;
  const auto dxgi_owned_capacity = checked_add_u64(
      sample.owned_gpu.owned_current_bytes, dxgi_headroom);
  if (!dxgi_owned_capacity) {
    return cancel(WatchdogReason::LiveCapacityOverflow);
  }
  if (*owned_plus_free > sample.owned_gpu.cuda_mem_info_total_bytes) {
    return cancel(WatchdogReason::OwnedGpuEvidenceInvalid);
  }

  SupervisorWatchdogDecision decision;
  decision.current_live_capacity_bytes =
      std::min({sample.gpu.physical_or_reportable_local_bytes,
                sample.gpu.dxgi_local_budget_bytes, *owned_plus_free,
                *dxgi_owned_capacity});
  const auto bounded_live =
      std::min(admission.policy_ceiling_bytes(),
               decision.current_live_capacity_bytes);
  const auto reserve_adjusted =
      bounded_live > admission.gpu_reserve_bytes()
          ? bounded_live - admission.gpu_reserve_bytes()
          : 0;
  decision.effective_cap_bytes =
      std::min(admission.effective_cap_bytes(), reserve_adjusted);
  if (sample.owned_gpu.owned_current_bytes > decision.effective_cap_bytes ||
      admission.predicted_gpu_peak_bytes() > decision.effective_cap_bytes) {
    decision.reason = WatchdogReason::EffectiveCapBreached;
    return decision;
  }
  if (!sample.thermal.available ||
      !fresh(sample.thermal.captured_monotonic_milliseconds,
             sample.evaluated_monotonic_milliseconds, maximum_age)) {
    decision.reason = WatchdogReason::ThermalTelemetryInvalidOrStale;
    return decision;
  }
  const auto bounds = thermal_bounds(sample.thermal);
  if (!bounds) {
    decision.reason = WatchdogReason::ThermalTelemetryInvalidOrStale;
    return decision;
  }
  if (sample.thermal.current_celsius >= bounds->stop ||
      sample.thermal.thermal_throttling ||
      sample.thermal.power_brake_slowdown) {
    decision.reason = WatchdogReason::ThermalStop;
    return decision;
  }
  auto host_request = sample.host_request;
  host_request.evaluation_monotonic_milliseconds =
      sample.evaluated_monotonic_milliseconds;
  host_request.max_telemetry_age_milliseconds = maximum_age;
  decision.host_decision =
      evaluate_host_admission(sample.host, sample.host_policy, host_request);
  if (!decision.host_decision.admitted) {
    decision.reason = WatchdogReason::HostReserveBreached;
    return decision;
  }
  decision.continue_work = true;
  decision.submissions_blocked = false;
  decision.reason = WatchdogReason::None;
  return decision;
}

}  // namespace prisminfer

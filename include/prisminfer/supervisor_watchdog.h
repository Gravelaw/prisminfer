#pragma once

#include <cstdint>

#include "prisminfer/supervisor_admission.h"

namespace prisminfer {

enum class WatchdogReason {
  None,
  WorkerUnavailable,
  ContextFatal,
  DeadlineReached,
  HeartbeatInvalidOrStale,
  GpuTelemetryInvalidOrStale,
  AdapterIdentityMismatch,
  OwnedGpuEvidenceInvalid,
  LiveCapacityOverflow,
  EffectiveCapBreached,
  ThermalTelemetryInvalidOrStale,
  ThermalStop,
  HostReserveBreached,
};

struct SupervisorWatchdogSample {
  std::uint64_t evaluated_monotonic_milliseconds{0};
  std::uint64_t run_deadline_monotonic_milliseconds{0};
  std::uint64_t worker_heartbeat_monotonic_milliseconds{0};
  bool worker_alive{false};
  bool context_fatal_error{false};
  PreContextGpuSample gpu;
  OwnedGpuMemoryEvidence owned_gpu;
  GpuThermalSample thermal;
  HostTelemetrySample host;
  HostReservePolicy host_policy;
  HostAdmissionRequest host_request;
};

struct SupervisorWatchdogDecision {
  bool continue_work{false};
  bool submissions_blocked{true};
  WatchdogReason reason{WatchdogReason::GpuTelemetryInvalidOrStale};
  std::uint64_t current_live_capacity_bytes{0};
  std::uint64_t effective_cap_bytes{0};
  HostAdmissionDecision host_decision;
};

[[nodiscard]] SupervisorWatchdogDecision evaluate_supervisor_watchdog(
    const PostContextAdmissionReceipt& admission,
    const SupervisorWatchdogSample& sample);

}  // namespace prisminfer

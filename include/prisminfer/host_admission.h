#pragma once

#include <cstdint>
#include <string>

#include "prisminfer/host_memory_tracker.h"

namespace prisminfer {

enum class HostAdmissionLane {
  DevelopmentNonPromotable,
  EvidencePromotable,
};

struct HostReservePolicy {
  // The evaluator always applies the canonical floor for lane. Nonzero values
  // supplied here are tightening overrides and can never lower that floor.
  HostAdmissionLane lane{HostAdmissionLane::DevelopmentNonPromotable};
  std::uint64_t physical_reserve_floor_bytes{0};
  std::uint32_t physical_reserve_basis_points{0};
  std::uint64_t commit_reserve_floor_bytes{0};
  std::uint32_t commit_limit_reserve_basis_points{0};
  std::uint32_t commit_physical_reserve_basis_points{0};
};

struct HostAdmissionRequest {
  // Planned incremental peaks exclude pinned_host_bytes, which is charged to
  // both physical and commit requirements by evaluate_host_admission().
  std::uint64_t planned_incremental_resident_bytes{0};
  std::uint64_t planned_incremental_commit_bytes{0};
  std::uint64_t resident_uncertainty_bytes{0};
  std::uint64_t commit_uncertainty_bytes{0};
  std::uint64_t pinned_host_bytes{0};
  std::uint64_t explicit_physical_reserve_bytes{0};
  std::uint64_t explicit_commit_reserve_bytes{0};
  std::uint64_t evaluation_monotonic_milliseconds{0};
  std::uint64_t max_telemetry_age_milliseconds{0};
  bool promotion_requested{false};
};

struct HostAdmissionDecision {
  bool admitted{false};
  // This means the host decision used the evidence lane and promotion was
  // requested. It does not replace the remaining #103/evidence gates.
  bool promotable{false};
  std::string reason;
  HostAdmissionLane lane{HostAdmissionLane::DevelopmentNonPromotable};
  std::uint64_t physical_reserve_bytes{0};
  std::uint64_t commit_reserve_bytes{0};
  std::uint64_t commit_headroom_bytes{0};
  std::uint64_t physical_payload_bytes{0};
  std::uint64_t commit_payload_bytes{0};
  std::uint64_t effective_payload_bytes{0};
  std::uint64_t required_resident_bytes{0};
  std::uint64_t required_commit_bytes{0};
  std::uint64_t pinned_host_cap_bytes{0};
};

HostReservePolicy default_host_reserve_policy(HostAdmissionLane lane);
std::string to_string(HostAdmissionLane lane);

HostAdmissionDecision evaluate_host_admission(
    const HostTelemetrySample& sample, const HostReservePolicy& policy,
    const HostAdmissionRequest& request);

}  // namespace prisminfer

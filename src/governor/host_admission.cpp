#include "prisminfer/host_admission.h"

#include <algorithm>
#include <limits>

namespace prisminfer {

namespace {

constexpr std::uint64_t kGiB = 1ULL << 30;
constexpr std::uint64_t kMiB = 1ULL << 20;
constexpr std::uint32_t kBasisPointDenominator = 10'000;
constexpr std::uint64_t kMaximumTelemetryAgeMilliseconds = 500;

bool checked_add(std::uint64_t left, std::uint64_t right,
                 std::uint64_t* result) {
  if (result == nullptr ||
      left > std::numeric_limits<std::uint64_t>::max() - right) {
    return false;
  }
  *result = left + right;
  return true;
}

bool checked_ratio(std::uint64_t value, std::uint32_t basis_points,
                   bool round_up, std::uint64_t* result) {
  if (result == nullptr || basis_points > kBasisPointDenominator) {
    return false;
  }
  const auto quotient = value / kBasisPointDenominator;
  const auto remainder = value % kBasisPointDenominator;
  if (basis_points != 0 &&
      quotient > std::numeric_limits<std::uint64_t>::max() / basis_points) {
    return false;
  }
  const auto base = quotient * basis_points;
  const auto remainder_product = remainder * basis_points;
  const auto extra = round_up
                         ? (remainder_product + kBasisPointDenominator - 1) /
                               kBasisPointDenominator
                         : remainder_product / kBasisPointDenominator;
  return checked_add(base, extra, result);
}

HostAdmissionDecision reject(HostAdmissionDecision decision,
                             const char* reason) {
  decision.reason = reason;
  return decision;
}

}  // namespace

HostReservePolicy default_host_reserve_policy(HostAdmissionLane lane) {
  if (lane == HostAdmissionLane::EvidencePromotable) {
    return {lane, 4 * kGiB, 1'500, 4 * kGiB, 1'000, 1'500};
  }
  return {lane, 2 * kGiB, 800, 2 * kGiB, 500, 0};
}

std::string to_string(HostAdmissionLane lane) {
  if (lane == HostAdmissionLane::EvidencePromotable) {
    return "evidence_promotable";
  }
  if (lane == HostAdmissionLane::DevelopmentNonPromotable) {
    return "development_nonpromotable";
  }
  return "invalid";
}

HostAdmissionDecision evaluate_host_admission(
    const HostTelemetrySample& sample, const HostReservePolicy& policy,
    const HostAdmissionRequest& request) {
  HostAdmissionDecision decision;
  decision.lane = policy.lane;

  if (policy.lane != HostAdmissionLane::DevelopmentNonPromotable &&
      policy.lane != HostAdmissionLane::EvidencePromotable) {
    return reject(decision, "host_admission_lane_invalid");
  }

  if (!sample.available) {
    return reject(decision, "host_telemetry_unavailable");
  }
  if (sample.captured_monotonic_milliseconds == 0 ||
      request.evaluation_monotonic_milliseconds == 0) {
    return reject(decision, "host_telemetry_timestamp_unavailable");
  }
  if (request.max_telemetry_age_milliseconds == 0 ||
      request.max_telemetry_age_milliseconds >
          kMaximumTelemetryAgeMilliseconds) {
    return reject(decision, "host_telemetry_freshness_policy_invalid");
  }
  if (request.evaluation_monotonic_milliseconds <
      sample.captured_monotonic_milliseconds) {
    return reject(decision, "host_telemetry_clock_anomaly");
  }
  if (request.evaluation_monotonic_milliseconds -
          sample.captured_monotonic_milliseconds >
      request.max_telemetry_age_milliseconds) {
    return reject(decision, "host_telemetry_stale");
  }
  if (sample.system_commit_source != "get_performance_info") {
    return reject(decision, "system_commit_source_not_authoritative");
  }
  if (sample.system_memory_total_bytes == 0 ||
      sample.system_memory_available_bytes > sample.system_memory_total_bytes ||
      sample.system_commit_total_bytes > sample.system_commit_limit_bytes) {
    return reject(decision, "host_telemetry_contradictory");
  }

  const auto commit_headroom =
      sample.system_commit_limit_bytes - sample.system_commit_total_bytes;
  if (sample.system_commit_available_bytes != commit_headroom) {
    return reject(decision, "host_commit_headroom_mismatch");
  }
  decision.commit_headroom_bytes = commit_headroom;

  const auto canonical_policy = default_host_reserve_policy(policy.lane);
  const auto physical_basis_points =
      std::max(canonical_policy.physical_reserve_basis_points,
               policy.physical_reserve_basis_points);
  const auto commit_limit_basis_points =
      std::max(canonical_policy.commit_limit_reserve_basis_points,
               policy.commit_limit_reserve_basis_points);
  const auto commit_physical_basis_points =
      std::max(canonical_policy.commit_physical_reserve_basis_points,
               policy.commit_physical_reserve_basis_points);

  std::uint64_t physical_fraction = 0;
  std::uint64_t commit_limit_fraction = 0;
  std::uint64_t commit_physical_fraction = 0;
  if (!checked_ratio(sample.system_memory_total_bytes, physical_basis_points,
                     true, &physical_fraction) ||
      !checked_ratio(sample.system_commit_limit_bytes,
                     commit_limit_basis_points, true, &commit_limit_fraction) ||
      !checked_ratio(sample.system_memory_total_bytes,
                     commit_physical_basis_points, true,
                     &commit_physical_fraction)) {
    return reject(decision, "host_reserve_policy_invalid_or_overflowed");
  }

  decision.physical_reserve_bytes =
      std::max({canonical_policy.physical_reserve_floor_bytes,
                policy.physical_reserve_floor_bytes, physical_fraction,
                request.explicit_physical_reserve_bytes});
  decision.commit_reserve_bytes = std::max(
      {canonical_policy.commit_reserve_floor_bytes,
       policy.commit_reserve_floor_bytes, commit_limit_fraction,
       commit_physical_fraction, request.explicit_commit_reserve_bytes});

  if (sample.system_memory_available_bytes < decision.physical_reserve_bytes) {
    return reject(decision, "host_physical_reserve_unavailable");
  }
  if (commit_headroom < decision.commit_reserve_bytes) {
    return reject(decision, "host_commit_reserve_unavailable");
  }

  decision.physical_payload_bytes =
      sample.system_memory_available_bytes - decision.physical_reserve_bytes;
  decision.commit_payload_bytes =
      commit_headroom - decision.commit_reserve_bytes;
  decision.effective_payload_bytes =
      std::min(decision.physical_payload_bytes, decision.commit_payload_bytes);

  std::uint64_t pinned_cap = 0;
  if (!checked_ratio(sample.system_memory_total_bytes, 200, false,
                     &pinned_cap)) {
    return reject(decision, "pinned_host_cap_overflowed");
  }
  decision.pinned_host_cap_bytes = std::min(512 * kMiB, pinned_cap);
  if (request.pinned_host_bytes > decision.pinned_host_cap_bytes) {
    return reject(decision, "pinned_host_cap_exceeded");
  }

  std::uint64_t resident_without_pinned = 0;
  if (!checked_add(request.planned_incremental_resident_bytes,
                   request.resident_uncertainty_bytes,
                   &resident_without_pinned) ||
      !checked_add(resident_without_pinned, request.pinned_host_bytes,
                   &decision.required_resident_bytes)) {
    return reject(decision, "required_resident_bytes_overflowed");
  }

  std::uint64_t commit_without_pinned = 0;
  if (!checked_add(request.planned_incremental_commit_bytes,
                   request.commit_uncertainty_bytes, &commit_without_pinned) ||
      !checked_add(commit_without_pinned, request.pinned_host_bytes,
                   &decision.required_commit_bytes)) {
    return reject(decision, "required_commit_bytes_overflowed");
  }

  if (decision.required_resident_bytes > decision.physical_payload_bytes) {
    return reject(decision, "planned_resident_peak_exceeds_payload");
  }
  if (decision.required_commit_bytes > decision.commit_payload_bytes) {
    return reject(decision, "planned_commit_peak_exceeds_payload");
  }
  if (request.promotion_requested &&
      policy.lane == HostAdmissionLane::DevelopmentNonPromotable) {
    return reject(decision, "development_lane_is_nonpromotable");
  }

  decision.admitted = true;
  decision.promotable = policy.lane == HostAdmissionLane::EvidencePromotable &&
                        request.promotion_requested;
  decision.reason = "admitted";
  return decision;
}

}  // namespace prisminfer

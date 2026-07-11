#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>

#include "prisminfer/host_admission.h"

namespace {

constexpr std::uint64_t kGiB = 1ULL << 30;
constexpr std::uint64_t kMiB = 1ULL << 20;
constexpr std::uint64_t kDevPhysicalReserve32GiB = 2'748'779'070ULL;
constexpr std::uint64_t kEvidencePhysicalReserve32GiB = 5'153'960'756ULL;
constexpr std::uint64_t kDevCommitReserve64GiB = 3'435'973'837ULL;
constexpr std::uint64_t kEvidenceCommitReserve64GiB = 6'871'947'674ULL;
constexpr std::uint64_t kDevPhysicalPayload8GiB = 5'841'155'522ULL;

int expect(bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

prisminfer::HostTelemetrySample sample_with_available(
    std::uint64_t available_bytes) {
  prisminfer::HostTelemetrySample sample;
  sample.available = true;
  sample.system_commit_source = "get_performance_info";
  sample.captured_monotonic_milliseconds = 10'000;
  sample.system_memory_total_bytes = 32 * kGiB;
  sample.system_memory_available_bytes = available_bytes;
  sample.system_commit_total_bytes = 16 * kGiB;
  sample.system_commit_limit_bytes = 64 * kGiB;
  sample.system_commit_available_bytes = 48 * kGiB;
  return sample;
}

prisminfer::HostAdmissionRequest request_for(std::uint64_t resident_bytes,
                                             std::uint64_t commit_bytes) {
  prisminfer::HostAdmissionRequest request;
  request.planned_incremental_resident_bytes = resident_bytes;
  request.planned_incremental_commit_bytes = commit_bytes;
  request.evaluation_monotonic_milliseconds = 10'100;
  request.max_telemetry_age_milliseconds = 500;
  return request;
}

}  // namespace

int main() {
  using prisminfer::HostAdmissionLane;

  const auto development = prisminfer::default_host_reserve_policy(
      HostAdmissionLane::DevelopmentNonPromotable);
  const auto evidence = prisminfer::default_host_reserve_policy(
      HostAdmissionLane::EvidencePromotable);

  {
    const auto sample = sample_with_available(8 * kGiB);
    const auto accepted = prisminfer::evaluate_host_admission(
        sample, development, request_for(5 * kGiB, 5 * kGiB));
    if (expect(accepted.admitted,
               "development lane admits bounded 5 GiB "
               "request with 8 GiB available") ||
        expect(!accepted.promotable,
               "development lane is always non-promotable") ||
        expect(accepted.physical_reserve_bytes ==
                   kDevPhysicalReserve32GiB,
               "development lane uses the exact eight-percent reserve") ||
        expect(accepted.commit_reserve_bytes == kDevCommitReserve64GiB,
               "development lane uses the exact five-percent commit reserve") ||
        expect(accepted.physical_payload_bytes == kDevPhysicalPayload8GiB,
               "development physical payload uses the exact reserve")) {
      return 1;
    }

    const auto rejected = prisminfer::evaluate_host_admission(
        sample, development, request_for(6 * kGiB, 6 * kGiB));
    if (expect(!rejected.admitted,
               "development lane rejects request above live payload") ||
        expect(rejected.reason == "planned_resident_peak_exceeds_payload",
               "physical payload rejection is specific")) {
      return 1;
    }
  }

  {
    const auto sample = sample_with_available(8 * kGiB);
    const auto nonpromotion = prisminfer::evaluate_host_admission(
        sample, evidence, request_for(3 * kGiB, 3 * kGiB));
    if (expect(nonpromotion.admitted && !nonpromotion.promotable,
               "evidence lane does not imply a promotion request")) {
      return 1;
    }

    auto request = request_for(3 * kGiB, 3 * kGiB);
    request.promotion_requested = true;
    const auto accepted =
        prisminfer::evaluate_host_admission(sample, evidence, request);
    if (expect(accepted.admitted && accepted.promotable,
               "evidence lane admits promotable request inside payload") ||
        expect(accepted.physical_reserve_bytes ==
                   kEvidencePhysicalReserve32GiB,
               "evidence lane uses the exact fifteen-percent reserve") ||
        expect(accepted.commit_reserve_bytes == kEvidenceCommitReserve64GiB,
               "evidence lane uses the exact ten-percent commit reserve")) {
      return 1;
    }

    request.planned_incremental_resident_bytes = 4 * kGiB;
    const auto rejected =
        prisminfer::evaluate_host_admission(sample, evidence, request);
    if (expect(!rejected.admitted,
               "evidence lane rejects 4 GiB resident "
               "request at 8 GiB available")) {
      return 1;
    }
  }

  {
    auto sample = sample_with_available(8 * kGiB);
    sample.system_memory_total_bytes = 33'752'915'968ULL;
    sample.system_commit_total_bytes = 15'391'395'840ULL;
    sample.system_commit_limit_bytes = 47'711'559'680ULL;
    sample.system_commit_available_bytes =
        sample.system_commit_limit_bytes - sample.system_commit_total_bytes;
    const auto dev = prisminfer::evaluate_host_admission(
        sample, development, request_for(0, 0));
    const auto promotable = prisminfer::evaluate_host_admission(
        sample, evidence, request_for(0, 0));
    if (expect(dev.admitted &&
                   dev.physical_reserve_bytes == 2'700'233'278ULL &&
                   dev.commit_reserve_bytes == 2'385'577'984ULL,
               "31.43 GiB development reserves are exact") ||
        expect(promotable.admitted &&
                   promotable.physical_reserve_bytes == 5'062'937'396ULL &&
                   promotable.commit_reserve_bytes == 5'062'937'396ULL,
               "31.43 GiB evidence reserves are exact")) {
      return 1;
    }
  }

  {
    for (const auto scenario : {std::pair{12ULL * kGiB, 7ULL * kGiB},
                                std::pair{15ULL * kGiB, 10ULL * kGiB}}) {
      auto request = request_for(scenario.second, scenario.second);
      request.promotion_requested = true;
      const auto decision = prisminfer::evaluate_host_admission(
          sample_with_available(scenario.first), evidence, request);
      if (expect(decision.admitted,
                 "12/15 GiB availability admits "
                 "workload-relative evidence request")) {
        return 1;
      }
    }
  }

  {
    auto sample = sample_with_available(15 * kGiB);
    sample.system_commit_total_bytes = 60 * kGiB;
    sample.system_commit_available_bytes = 4 * kGiB;
    const auto decision = prisminfer::evaluate_host_admission(
        sample, evidence, request_for(1 * kGiB, 1 * kGiB));
    if (expect(
            !decision.admitted,
            "commit pressure rejects independently of physical availability") ||
        expect(decision.reason == "host_commit_reserve_unavailable",
               "commit reserve rejection is specific")) {
      return 1;
    }
  }

  {
    prisminfer::HostTelemetrySample missing;
    const auto decision = prisminfer::evaluate_host_admission(
        missing, development, request_for(0, 0));
    if (expect(!decision.admitted &&
                   decision.reason == "host_telemetry_unavailable",
               "missing telemetry fails closed")) {
      return 1;
    }
  }

  {
    auto sample = sample_with_available(8 * kGiB);
    sample.system_commit_total_bytes = 65 * kGiB;
    const auto decision = prisminfer::evaluate_host_admission(
        sample, development, request_for(0, 0));
    if (expect(!decision.admitted &&
                   decision.reason == "host_telemetry_contradictory",
               "contradictory commit telemetry fails closed")) {
      return 1;
    }
  }

  {
    auto sample = sample_with_available(8 * kGiB);
    sample.system_commit_available_bytes -= 1;
    const auto decision = prisminfer::evaluate_host_admission(
        sample, development, request_for(0, 0));
    if (expect(!decision.admitted &&
                   decision.reason == "host_commit_headroom_mismatch",
               "inconsistent derived commit headroom fails closed")) {
      return 1;
    }
  }

  {
    auto sample = sample_with_available(8 * kGiB);
    sample.system_commit_source = "memory_status_ex";
    const auto decision = prisminfer::evaluate_host_admission(
        sample, development, request_for(0, 0));
    if (expect(!decision.admitted &&
                   decision.reason ==
                       "system_commit_source_not_authoritative",
               "process-bounded commit source fails closed")) {
      return 1;
    }
  }

  {
    auto invalid_policy = development;
    invalid_policy.physical_reserve_basis_points = 10'001;
    const auto decision = prisminfer::evaluate_host_admission(
        sample_with_available(8 * kGiB), invalid_policy, request_for(0, 0));
    if (expect(
            !decision.admitted &&
                decision.reason == "host_reserve_policy_invalid_or_overflowed",
            "invalid reserve basis points fail closed")) {
      return 1;
    }
  }

  {
    prisminfer::HostReservePolicy zeroed_evidence;
    zeroed_evidence.lane = HostAdmissionLane::EvidencePromotable;
    auto request = request_for(4 * kGiB, 1 * kGiB);
    request.promotion_requested = true;
    const auto decision = prisminfer::evaluate_host_admission(
        sample_with_available(8 * kGiB), zeroed_evidence, request);
    if (expect(!decision.admitted &&
                   decision.reason == "planned_resident_peak_exceeds_payload",
               "mutable policy cannot lower canonical evidence reserve") ||
        expect(decision.physical_reserve_bytes > 4 * kGiB,
               "canonical evidence reserve survives a zeroed override")) {
      return 1;
    }
  }

  {
    auto sample = sample_with_available(8 * kGiB);
    auto request = request_for(0, 0);
    request.evaluation_monotonic_milliseconds = 10'501;
    const auto stale =
        prisminfer::evaluate_host_admission(sample, development, request);
    if (expect(!stale.admitted && stale.reason == "host_telemetry_stale",
               "stale telemetry fails closed")) {
      return 1;
    }

    request.evaluation_monotonic_milliseconds = 9'999;
    const auto clock_anomaly =
        prisminfer::evaluate_host_admission(sample, development, request);
    if (expect(!clock_anomaly.admitted &&
                   clock_anomaly.reason == "host_telemetry_clock_anomaly",
               "monotonic clock anomaly fails closed")) {
      return 1;
    }

    request.evaluation_monotonic_milliseconds = 10'100;
    request.max_telemetry_age_milliseconds = 501;
    const auto unsafe_age =
        prisminfer::evaluate_host_admission(sample, development, request);
    if (expect(
            !unsafe_age.admitted &&
                unsafe_age.reason == "host_telemetry_freshness_policy_invalid",
            "freshness policy cannot exceed the supervisor bound")) {
      return 1;
    }

    request.evaluation_monotonic_milliseconds = 10'500;
    request.max_telemetry_age_milliseconds = 500;
    const auto boundary =
        prisminfer::evaluate_host_admission(sample, development, request);
    if (expect(boundary.admitted,
               "exact telemetry freshness boundary remains admissible")) {
      return 1;
    }

    request.max_telemetry_age_milliseconds = 0;
    const auto zero_age =
        prisminfer::evaluate_host_admission(sample, development, request);
    if (expect(!zero_age.admitted &&
                   zero_age.reason ==
                       "host_telemetry_freshness_policy_invalid",
               "zero freshness bound fails closed")) {
      return 1;
    }

    request = request_for(0, 0);
    request.evaluation_monotonic_milliseconds = 0;
    const auto missing_evaluation =
        prisminfer::evaluate_host_admission(sample, development, request);
    sample.captured_monotonic_milliseconds = 0;
    request = request_for(0, 0);
    const auto missing_capture =
        prisminfer::evaluate_host_admission(sample, development, request);
    if (expect(!missing_evaluation.admitted &&
                   missing_evaluation.reason ==
                       "host_telemetry_timestamp_unavailable",
               "missing evaluation timestamp fails closed") ||
        expect(!missing_capture.admitted &&
                   missing_capture.reason ==
                       "host_telemetry_timestamp_unavailable",
               "missing capture timestamp fails closed")) {
      return 1;
    }
  }

  {
    auto invalid_policy = development;
    invalid_policy.lane = static_cast<HostAdmissionLane>(99);
    const auto decision = prisminfer::evaluate_host_admission(
        sample_with_available(8 * kGiB), invalid_policy, request_for(0, 0));
    if (expect(!decision.admitted &&
                   decision.reason == "host_admission_lane_invalid",
               "unknown admission lane fails closed")) {
      return 1;
    }
  }

  {
    auto request = request_for(0, 0);
    request.pinned_host_bytes = 512 * kMiB + 1;
    const auto decision = prisminfer::evaluate_host_admission(
        sample_with_available(15 * kGiB), development, request);
    if (expect(
            !decision.admitted && decision.reason == "pinned_host_cap_exceeded",
            "pinned host cap remains separate and fail-closed")) {
      return 1;
    }
  }

  {
    auto request = request_for(1 * kGiB, 2 * kGiB);
    request.pinned_host_bytes = 512 * kMiB;
    const auto decision = prisminfer::evaluate_host_admission(
        sample_with_available(15 * kGiB), development, request);
    if (expect(decision.admitted &&
                   decision.required_resident_bytes == 1'610'612'736ULL &&
                   decision.required_commit_bytes == 2'684'354'560ULL,
               "pinned bytes are charged to resident and commit requirements")) {
      return 1;
    }
  }

  {
    auto request = request_for(0, 0);
    request.pinned_host_bytes = 512 * kMiB;
    const auto decision = prisminfer::evaluate_host_admission(
        sample_with_available(8 * kGiB), development, request);
    if (expect(decision.admitted &&
                   decision.pinned_host_cap_bytes == 512 * kMiB,
               "exact pinned-host cap boundary remains admissible")) {
      return 1;
    }
  }

  {
    auto request = request_for(std::numeric_limits<std::uint64_t>::max(), 0);
    request.resident_uncertainty_bytes = 1;
    const auto decision = prisminfer::evaluate_host_admission(
        sample_with_available(15 * kGiB), development, request);
    if (expect(!decision.admitted &&
                   decision.reason == "required_resident_bytes_overflowed",
               "resident requirement overflow fails closed")) {
      return 1;
    }
  }

  {
    auto request = request_for(0, std::numeric_limits<std::uint64_t>::max());
    request.commit_uncertainty_bytes = 1;
    const auto decision = prisminfer::evaluate_host_admission(
        sample_with_available(15 * kGiB), development, request);
    if (expect(!decision.admitted &&
                   decision.reason == "required_commit_bytes_overflowed",
               "commit requirement overflow fails closed")) {
      return 1;
    }
  }

  {
    const auto decision = prisminfer::evaluate_host_admission(
        sample_with_available(15 * kGiB), development,
        request_for(0, 45 * kGiB));
    if (expect(!decision.admitted &&
                   decision.reason == "planned_commit_peak_exceeds_payload",
               "planned commit peak is checked independently")) {
      return 1;
    }
  }

  {
    auto request = request_for(1 * kGiB, 1 * kGiB);
    request.promotion_requested = true;
    const auto decision = prisminfer::evaluate_host_admission(
        sample_with_available(15 * kGiB), development, request);
    if (expect(!decision.admitted &&
                   decision.reason == "development_lane_is_nonpromotable",
               "development receipt cannot be relabeled promotable")) {
      return 1;
    }
  }

  {
    auto request = request_for(2 * kGiB, 1 * kGiB);
    request.explicit_physical_reserve_bytes = 7 * kGiB;
    const auto decision = prisminfer::evaluate_host_admission(
        sample_with_available(8 * kGiB), development, request);
    if (expect(
            !decision.admitted && decision.physical_reserve_bytes == 7 * kGiB,
            "explicit reserve can raise but not lower the lane reserve")) {
      return 1;
    }
  }

  {
    auto request = request_for(0, 0);
    request.explicit_commit_reserve_bytes = 49 * kGiB;
    const auto decision = prisminfer::evaluate_host_admission(
        sample_with_available(15 * kGiB), development, request);
    if (expect(!decision.admitted &&
                   decision.reason == "host_commit_reserve_unavailable" &&
                   decision.commit_reserve_bytes == 49 * kGiB,
               "explicit commit reserve can only raise the lane floor")) {
      return 1;
    }
  }

  {
    const auto exact = prisminfer::evaluate_host_admission(
        sample_with_available(8 * kGiB), development,
        request_for(kDevPhysicalPayload8GiB, 0));
    if (expect(exact.admitted &&
                   exact.physical_payload_bytes == kDevPhysicalPayload8GiB,
               "exact physical payload boundary remains admissible")) {
      return 1;
    }
  }

  return 0;
}

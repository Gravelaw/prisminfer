#include <algorithm>
#include <cstdint>
#include <atomic>
#include <array>
#include <iostream>
#include <thread>
#include <type_traits>
#include <utility>

#include "prisminfer/admission_token.h"
#include "prisminfer/supervisor_watchdog.h"

namespace {

constexpr std::uint64_t kGiB = 1ULL << 30;

static_assert(
    !std::is_default_constructible_v<prisminfer::PostContextAdmissionReceipt>);
static_assert(!std::is_default_constructible_v<prisminfer::AdmissionToken>);
static_assert(!std::is_copy_constructible_v<prisminfer::AdmissionToken>);

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

prisminfer::PredictedBytes prediction(
    std::uint64_t bytes, prisminfer::PredictionProvenance provenance) {
  return {bytes, provenance};
}

prisminfer::AdmissionCellIdentity cell(std::uint8_t seed = 1);

prisminfer::PreContextAdmissionRequest pre_request() {
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
  request.thermal.reported_target_celsius = 90;
  request.thermal.reported_slowdown_celsius = 92;
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
  request.cell = cell();
  return request;
}

prisminfer::PostContextAdmissionReceipt post_receipt() {
  const auto pre_input = pre_request();
  const auto pre = prisminfer::evaluate_pre_context_admission(pre_input);
  prisminfer::PostContextAdmissionRequest request;
  request.pre_context_receipt = pre.receipt;
  request.policy_ceiling_bytes = pre_input.policy_ceiling_bytes;
  request.requested_tier_bytes = pre_input.requested_tier_bytes;
  request.evaluation_monotonic_milliseconds = 10'300;
  request.timing = pre_input.timing;
  request.gpu = pre_input.gpu;
  request.gpu.captured_monotonic_milliseconds = 10'200;
  request.gpu.dxgi_local_budget_bytes = 14 * kGiB;
  request.gpu.dxgi_local_current_usage_bytes = 2 * kGiB;
  request.thermal = pre_input.thermal;
  request.thermal.captured_monotonic_milliseconds = 10'200;
  request.owned_gpu.available = true;
  request.owned_gpu.reconciled = true;
  request.owned_gpu.process_device_corroboration_available = true;
  request.owned_gpu.adapter_identity_available = true;
  request.owned_gpu.adapter_luid_high = request.gpu.adapter_luid_high;
  request.owned_gpu.adapter_luid_low = request.gpu.adapter_luid_low;
  request.owned_gpu.hard_cap_bytes = pre.pre_context_effective_cap_bytes;
  request.owned_gpu.owned_current_bytes = 512 * (1ULL << 20);
  request.owned_gpu.owned_peak_bytes = 512 * (1ULL << 20);
  request.owned_gpu.cuda_context_runtime_current_bytes = 512 * (1ULL << 20);
  request.owned_gpu.cuda_context_runtime_at_owned_peak_bytes =
      512 * (1ULL << 20);
  request.owned_gpu.cuda_mem_info_free_bytes = 12 * kGiB;
  request.owned_gpu.cuda_mem_info_total_bytes = 16 * kGiB;
  request.host = pre_input.host;
  request.host.captured_monotonic_milliseconds = 10'200;
  request.host_policy = pre_input.host_policy;
  request.host_request = pre_input.host_request;
  request.cell = cell();
  const auto post = prisminfer::evaluate_post_context_admission(request);
  return *post.receipt;
}

prisminfer::AdmissionCellIdentity cell(std::uint8_t seed) {
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

prisminfer::SupervisorWatchdogSample watchdog_sample() {
  const auto base = pre_request();
  prisminfer::SupervisorWatchdogSample sample;
  sample.evaluated_monotonic_milliseconds = 10'400;
  sample.run_deadline_monotonic_milliseconds = 20'000;
  sample.worker_heartbeat_monotonic_milliseconds = 10'300;
  sample.worker_alive = true;
  sample.gpu = base.gpu;
  sample.gpu.captured_monotonic_milliseconds = 10'300;
  sample.gpu.dxgi_local_budget_bytes = 14 * kGiB;
  sample.gpu.dxgi_local_current_usage_bytes = 2 * kGiB;
  sample.owned_gpu.available = true;
  sample.owned_gpu.reconciled = true;
  sample.owned_gpu.process_device_corroboration_available = true;
  sample.owned_gpu.adapter_identity_available = true;
  sample.owned_gpu.adapter_luid_high = 7;
  sample.owned_gpu.adapter_luid_low = 11;
  sample.owned_gpu.hard_cap_bytes = 8 * kGiB;
  sample.owned_gpu.owned_current_bytes = 512ULL << 20;
  sample.owned_gpu.owned_peak_bytes = 512ULL << 20;
  sample.owned_gpu.cuda_context_runtime_current_bytes = 512ULL << 20;
  sample.owned_gpu.cuda_context_runtime_at_owned_peak_bytes = 512ULL << 20;
  sample.owned_gpu.cuda_mem_info_free_bytes = 12 * kGiB;
  sample.owned_gpu.cuda_mem_info_total_bytes = 16 * kGiB;
  sample.thermal = base.thermal;
  sample.thermal.captured_monotonic_milliseconds = 10'300;
  sample.host = base.host;
  sample.host.captured_monotonic_milliseconds = 10'300;
  sample.host_policy = base.host_policy;
  sample.host_request = base.host_request;
  return sample;
}

}  // namespace

int main() {
  using prisminfer::AdmissionTokenStatus;

  {
    const auto receipt = post_receipt();
    prisminfer::AdmissionTokenIssuer issuer;
    auto issued = issuer.issue(receipt, cell(), 10'400, 100);
    if (expect(issued.status == AdmissionTokenStatus::Issued && issued.token &&
                   issued.token->active(),
               "valid exact cell receives an active token") ||
        expect(issued.token->effective_cap_bytes() == 8 * kGiB,
               "token carries the post-context cap")) {
      return 1;
    }
    const auto consumed = issuer.consume(*issued.token, cell(), 10'450);
    if (expect(consumed.status == AdmissionTokenStatus::Consumed &&
                   consumed.effective_cap_bytes == 8 * kGiB &&
                   !issued.token->active(),
               "token is consumed exactly once")) {
      return 1;
    }
    if (expect(issuer.consume(*issued.token, cell(), 10'450).status ==
                   AdmissionTokenStatus::TokenAlreadyConsumed,
               "consumed token cannot replay") ||
        expect(issuer.issue(receipt, cell(), 10'450, 50).status ==
                   AdmissionTokenStatus::ReceiptAlreadyUsed,
               "one Stage-B receipt cannot mint multiple tokens")) {
      return 1;
    }
  }

  {
    auto invalid_cell = cell();
    invalid_cell.artifact_identity_hash.fill(0);
    prisminfer::AdmissionTokenIssuer issuer;
    if (expect(issuer.issue(post_receipt(), invalid_cell, 10'400, 100).status ==
                   AdmissionTokenStatus::InvalidCellIdentity,
               "missing exact-cell hash rejects")) {
      return 1;
    }
  }

  {
    prisminfer::AdmissionTokenIssuer issuer;
    if (expect(issuer.issue(post_receipt(), cell(2), 10'400, 100).status ==
                   AdmissionTokenStatus::InvalidCellIdentity,
               "Stage-B receipt cannot mint for another valid cell")) {
      return 1;
    }
  }

  {
    const auto receipt = post_receipt();
    const auto decision =
        prisminfer::evaluate_supervisor_watchdog(receipt, watchdog_sample());
    if (expect(decision.continue_work && !decision.submissions_blocked &&
                   decision.reason == prisminfer::WatchdogReason::None &&
                   decision.effective_cap_bytes == 8 * kGiB,
               "fresh safe watchdog sample continues at admitted cap")) {
      return 1;
    }
  }

  {
    const auto receipt = post_receipt();
    auto sample = watchdog_sample();
    sample.gpu.dxgi_local_budget_bytes = 10 * kGiB;
    const auto decision =
        prisminfer::evaluate_supervisor_watchdog(receipt, sample);
    if (expect(decision.continue_work &&
                   decision.effective_cap_bytes < receipt.effective_cap_bytes(),
               "watchdog cap can shrink but never grow")) {
      return 1;
    }
  }

  {
    const auto receipt = post_receipt();
    auto sample = watchdog_sample();
    sample.gpu.dxgi_local_budget_bytes = 8 * kGiB;
    sample.gpu.dxgi_local_current_usage_bytes = 7 * kGiB;
    sample.owned_gpu.owned_current_bytes = 7 * kGiB;
    sample.owned_gpu.owned_peak_bytes = 7 * kGiB;
    sample.owned_gpu.cuda_context_runtime_current_bytes = 7 * kGiB;
    sample.owned_gpu.cuda_context_runtime_at_owned_peak_bytes = 7 * kGiB;
    sample.owned_gpu.cuda_mem_info_free_bytes = kGiB;
    const auto decision =
        prisminfer::evaluate_supervisor_watchdog(receipt, sample);
    if (expect(!decision.continue_work && decision.submissions_blocked &&
                   decision.reason ==
                       prisminfer::WatchdogReason::EffectiveCapBreached,
               "live budget shrink below owned bytes cancels")) {
      return 1;
    }
  }

  {
    const auto receipt = post_receipt();
    auto sample = watchdog_sample();
    sample.gpu.dxgi_local_budget_bytes = 8 * kGiB;
    sample.gpu.dxgi_local_current_usage_bytes = 2 * kGiB;
    const auto decision =
        prisminfer::evaluate_supervisor_watchdog(receipt, sample);
    if (expect(!decision.continue_work && decision.reason ==
                   prisminfer::WatchdogReason::EffectiveCapBreached,
               "cap below admitted predicted peak blocks future work")) {
      return 1;
    }
  }

  {
    const auto receipt = post_receipt();
    auto sample = watchdog_sample();
    sample.gpu.dxgi_local_budget_bytes = 14 * kGiB;
    sample.gpu.dxgi_local_current_usage_bytes = 13 * kGiB;
    if (expect(prisminfer::evaluate_supervisor_watchdog(receipt, sample).reason ==
                   prisminfer::WatchdogReason::EffectiveCapBreached,
               "external DXGI usage reduces owned live capacity")) {
      return 1;
    }
    sample = watchdog_sample();
    sample.gpu.dxgi_local_current_usage_bytes = 0;
    if (expect(prisminfer::evaluate_supervisor_watchdog(receipt, sample).reason ==
                   prisminfer::WatchdogReason::OwnedGpuEvidenceInvalid,
               "DXGI usage below owned bytes is contradictory")) {
      return 1;
    }
    sample = watchdog_sample();
    sample.owned_gpu.owned_current_bytes = 0;
    sample.owned_gpu.owned_peak_bytes = 0;
    sample.owned_gpu.cuda_context_runtime_current_bytes = 0;
    sample.owned_gpu.cuda_context_runtime_at_owned_peak_bytes = 0;
    if (expect(prisminfer::evaluate_supervisor_watchdog(receipt, sample).reason ==
                   prisminfer::WatchdogReason::OwnedGpuEvidenceInvalid,
               "zero context ledger cannot continue a GPU run")) {
      return 1;
    }
  }

  {
    const auto receipt = post_receipt();
    auto sample = watchdog_sample();
    sample.worker_heartbeat_monotonic_milliseconds = 9'899;
    if (expect(prisminfer::evaluate_supervisor_watchdog(receipt, sample).reason ==
                   prisminfer::WatchdogReason::HeartbeatInvalidOrStale,
               "stale worker heartbeat cancels")) {
      return 1;
    }
    sample = watchdog_sample();
    sample.thermal.current_celsius = 82;
    if (expect(prisminfer::evaluate_supervisor_watchdog(receipt, sample).reason ==
                   prisminfer::WatchdogReason::ThermalStop,
               "thermal stop blocks submissions")) {
      return 1;
    }
    sample = watchdog_sample();
    sample.host.system_memory_available_bytes = kGiB;
    if (expect(prisminfer::evaluate_supervisor_watchdog(receipt, sample).reason ==
                   prisminfer::WatchdogReason::HostReserveBreached,
               "host hard-reserve breach cancels")) {
      return 1;
    }
    sample = watchdog_sample();
    sample.context_fatal_error = true;
    if (expect(prisminfer::evaluate_supervisor_watchdog(receipt, sample).reason ==
                   prisminfer::WatchdogReason::ContextFatal,
               "context-fatal report cancels without retry")) {
      return 1;
    }
    sample = watchdog_sample();
    sample.evaluated_monotonic_milliseconds =
        sample.run_deadline_monotonic_milliseconds;
    if (expect(prisminfer::evaluate_supervisor_watchdog(receipt, sample).reason ==
                   prisminfer::WatchdogReason::DeadlineReached,
               "exact run deadline cancels")) {
      return 1;
    }
  }

  {
    const auto receipt = post_receipt();
    prisminfer::AdmissionTokenIssuer first;
    prisminfer::AdmissionTokenIssuer second;
    std::atomic<unsigned> ready{0};
    std::atomic<bool> go{false};
    std::array<AdmissionTokenStatus, 2> statuses{};
    std::thread first_issue([&] {
      ready.fetch_add(1, std::memory_order_release);
      while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
      statuses[0] = first.issue(receipt, cell(), 10'400, 100).status;
    });
    std::thread second_issue([&] {
      ready.fetch_add(1, std::memory_order_release);
      while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
      statuses[1] = second.issue(receipt, cell(), 10'400, 100).status;
    });
    while (ready.load(std::memory_order_acquire) != 2) {
      std::this_thread::yield();
    }
    go.store(true, std::memory_order_release);
    first_issue.join();
    second_issue.join();
    const auto issued = static_cast<unsigned>(std::count(
        statuses.begin(), statuses.end(), AdmissionTokenStatus::Issued));
    const auto reused = static_cast<unsigned>(std::count(
        statuses.begin(), statuses.end(),
        AdmissionTokenStatus::ReceiptAlreadyUsed));
    if (expect(issued == 1 && reused == 1,
               "receipt copies mint exactly once across concurrent issuers")) {
      return 1;
    }
  }

  {
    const auto receipt = post_receipt();
    prisminfer::AdmissionTokenIssuer issuer;
    if (expect(issuer.issue(receipt, cell(), 10'801, 100).status ==
                   AdmissionTokenStatus::InvalidTime,
               "stale Stage-B receipt cannot mint a token") ||
        expect(issuer.issue(receipt, cell(), 10'800, 1).status ==
                   AdmissionTokenStatus::InvalidTime,
               "receipt cannot mint at its guard-age deadline") ||
        expect(issuer.issue(receipt, cell(), 10'400, 501).status ==
                   AdmissionTokenStatus::InvalidTime,
               "token validity cannot exceed the guard age")) {
      return 1;
    }
  }

  {
    prisminfer::AdmissionTokenIssuer issuer;
    auto issued = issuer.issue(post_receipt(), cell(), 10'750, 100);
    if (expect(issued.status == AdmissionTokenStatus::Issued,
               "late issue is bounded by remaining evidence lifetime") ||
        expect(issuer.consume(*issued.token, cell(), 10'801).status ==
                   AdmissionTokenStatus::TokenExpired,
               "token cannot outlive its Stage-B guard evidence")) {
      return 1;
    }
  }

  {
    prisminfer::AdmissionTokenIssuer issuer;
    auto issued = issuer.issue(post_receipt(), cell(), 10'400, 100);
    std::atomic<unsigned> ready{0};
    std::atomic<bool> go{false};
    std::array<AdmissionTokenStatus, 8> statuses{};
    std::array<std::thread, 8> consumers;
    for (std::size_t index = 0; index < consumers.size(); ++index) {
      consumers[index] = std::thread([&, index] {
        ready.fetch_add(1, std::memory_order_release);
        while (!go.load(std::memory_order_acquire)) {
          std::this_thread::yield();
        }
        statuses[index] =
            issuer.consume(*issued.token, cell(), 10'450).status;
      });
    }
    while (ready.load(std::memory_order_acquire) != consumers.size()) {
      std::this_thread::yield();
    }
    go.store(true, std::memory_order_release);
    for (auto& consumer : consumers) consumer.join();
    const auto consumed = static_cast<unsigned>(std::count(
        statuses.begin(), statuses.end(), AdmissionTokenStatus::Consumed));
    const auto rejected = static_cast<unsigned>(std::count(
        statuses.begin(), statuses.end(),
        AdmissionTokenStatus::TokenAlreadyConsumed));
    if (expect(consumed == 1 && rejected == consumers.size() - 1,
               "concurrent consumption has exactly one winner")) {
      return 1;
    }
  }

  {
    prisminfer::AdmissionTokenIssuer issuer;
    auto issued = issuer.issue(post_receipt(), cell(), 10'400, 100);
    const auto mismatch = issuer.consume(*issued.token, cell(2), 10'450);
    if (expect(mismatch.status == AdmissionTokenStatus::TokenCellMismatch &&
                   !issued.token->active(),
               "cell mismatch invalidates the token") ||
        expect(issuer.consume(*issued.token, cell(), 10'450).status ==
                   AdmissionTokenStatus::TokenAlreadyConsumed,
               "mismatched token cannot be retried with another cell")) {
      return 1;
    }
  }

  {
    prisminfer::AdmissionTokenIssuer issuer;
    auto issued = issuer.issue(post_receipt(), cell(), 10'400, 100);
    if (expect(issuer.consume(*issued.token, cell(), 10'501).status ==
                   AdmissionTokenStatus::TokenExpired,
               "expired token rejects and invalidates")) {
      return 1;
    }
  }

  {
    prisminfer::AdmissionTokenIssuer issuer;
    prisminfer::AdmissionTokenIssuer other;
    auto issued = issuer.issue(post_receipt(), cell(), 10'400, 100);
    if (expect(other.consume(*issued.token, cell(), 10'450).status ==
                   AdmissionTokenStatus::ForeignIssuer,
               "another supervisor cannot consume the token") ||
        expect(issuer.consume(*issued.token, cell(), 10'500).status ==
                   AdmissionTokenStatus::Consumed,
               "exact expiry boundary remains valid")) {
      return 1;
    }
  }

  {
    prisminfer::AdmissionTokenIssuer issuer;
    auto issued = issuer.issue(post_receipt(), cell(), 10'400, 100);
    auto token = std::move(*issued.token);
    if (expect(!issued.token->active() && token.active(),
               "move transfers token authority and invalidates the source") ||
        expect(issuer.consume(token, cell(), 10'450).status ==
                   AdmissionTokenStatus::Consumed,
               "moved token remains single-use")) {
      return 1;
    }
  }

  {
    prisminfer::AdmissionTokenIssuer issuer;
    for (std::size_t index = 0;
         index < prisminfer::AdmissionTokenIssuer::kMaximumReceipts; ++index) {
      const auto issued = issuer.issue(post_receipt(), cell(), 10'400, 100);
      if (expect(issued.status == AdmissionTokenStatus::Issued,
                 "bounded issuer slot accepts a unique receipt")) {
        return 1;
      }
    }
    if (expect(issuer.issue(post_receipt(), cell(), 10'400, 100).status ==
                   AdmissionTokenStatus::IssuerCapacityExhausted,
               "bounded issuer fails closed at fixed capacity")) {
      return 1;
    }
  }

  return 0;
}

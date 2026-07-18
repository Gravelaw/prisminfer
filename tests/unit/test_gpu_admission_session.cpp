#include <cstdint>
#include <iostream>
#include <utility>

#include "prisminfer/gpu_admission_session.h"

namespace {

constexpr std::uint64_t kGiB = 1ULL << 30;

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

prisminfer::AdmissionCellIdentity cell(std::uint8_t seed = 1) {
  prisminfer::AdmissionCellIdentity identity;
  identity.run_sequence = seed;
  identity.run_contract_hash.fill(seed);
  identity.threshold_registry_hash.fill(seed + 1);
  identity.hardware_identity_hash.fill(seed + 2);
  identity.runtime_identity_hash.fill(seed + 3);
  identity.artifact_identity_hash.fill(seed + 4);
  identity.service_profile_hash.fill(seed + 5);
  return identity;
}

prisminfer::PredictedBytes prediction(
    std::uint64_t bytes, prisminfer::PredictionProvenance provenance) {
  return {bytes, provenance};
}

prisminfer::PreContextAdmissionRequest pre_request(
    const prisminfer::AdmissionCellIdentity& exact_cell = cell()) {
  prisminfer::PreContextAdmissionRequest request;
  request.requested_tier_bytes = 8 * kGiB;
  request.context_tokens = 8'192;
  request.run_deadline_milliseconds = 60'000;
  request.evaluation_monotonic_milliseconds = 10'100;
  request.cell = exact_cell;
  using prisminfer::PredictionProvenance;
  request.predicted_gpu.context_runtime =
      prediction(512ULL << 20, PredictionProvenance::PinnedRuntimeEstimate);
  request.predicted_gpu.backend =
      prediction(256ULL << 20, PredictionProvenance::PinnedRuntimeEstimate);
  request.predicted_gpu.model =
      prediction(5 * kGiB, PredictionProvenance::ApprovedArtifactCensus);
  request.predicted_gpu.state =
      prediction(256ULL << 20, PredictionProvenance::ApprovedArtifactCensus);
  request.predicted_gpu.workspace =
      prediction(256ULL << 20, PredictionProvenance::PinnedRuntimeEstimate);
  request.predicted_gpu.fragmentation =
      prediction(256ULL << 20, PredictionProvenance::PinnedRuntimeEstimate);
  request.gpu.available = true;
  request.gpu.adapter_identity_available = true;
  request.gpu.adapter_luid_high = 7;
  request.gpu.adapter_luid_low = 11;
  request.gpu.captured_monotonic_milliseconds = 10'000;
  request.gpu.physical_or_reportable_local_bytes = 16 * kGiB;
  request.gpu.dxgi_local_budget_bytes = 15 * kGiB;
  request.gpu.dxgi_local_current_usage_bytes = kGiB;
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
  return request;
}

prisminfer::PostContextAdmissionRequest post_request(
    const prisminfer::AdmissionCellIdentity& exact_cell = cell()) {
  const auto pre = pre_request(exact_cell);
  prisminfer::PostContextAdmissionRequest request;
  request.policy_ceiling_bytes = pre.policy_ceiling_bytes;
  request.requested_tier_bytes = pre.requested_tier_bytes;
  request.evaluation_monotonic_milliseconds = 10'300;
  request.timing = pre.timing;
  request.cell = exact_cell;
  request.gpu = pre.gpu;
  request.gpu.captured_monotonic_milliseconds = 10'200;
  request.gpu.dxgi_local_budget_bytes = 14 * kGiB;
  request.gpu.dxgi_local_current_usage_bytes = 2 * kGiB;
  request.thermal = pre.thermal;
  request.thermal.captured_monotonic_milliseconds = 10'200;
  request.owned_gpu.available = true;
  request.owned_gpu.reconciled = true;
  request.owned_gpu.process_device_corroboration_available = true;
  request.owned_gpu.adapter_identity_available = true;
  request.owned_gpu.adapter_luid_high = 7;
  request.owned_gpu.adapter_luid_low = 11;
  request.owned_gpu.hard_cap_bytes = 8 * kGiB;
  request.owned_gpu.owned_current_bytes = 512ULL << 20;
  request.owned_gpu.owned_peak_bytes = 512ULL << 20;
  request.owned_gpu.cuda_context_runtime_current_bytes = 512ULL << 20;
  request.owned_gpu.cuda_context_runtime_at_owned_peak_bytes = 512ULL << 20;
  request.owned_gpu.cuda_mem_info_free_bytes = 12 * kGiB;
  request.owned_gpu.cuda_mem_info_total_bytes = 16 * kGiB;
  request.host = pre.host;
  request.host.captured_monotonic_milliseconds = 10'200;
  request.host_policy = pre.host_policy;
  request.host_request = pre.host_request;
  return request;
}

}  // namespace

int main() {
  using prisminfer::ExclusiveGpuLeaseStatus;
  using prisminfer::GpuAdmissionSessionState;

  {
    auto invalid = cell();
    invalid.run_contract_hash.fill(0);
    if (expect(prisminfer::acquire_gpu_admission_session(invalid, 7, 11)
                       .status == prisminfer::GpuAdmissionSessionAcquireResult::
                                      Status::InvalidCellIdentity,
               "invalid run cell cannot acquire a session")) {
      return 1;
    }
  }

  {
    auto out_of_order =
        prisminfer::acquire_gpu_admission_session(cell(), 7, 11);
    if (expect(out_of_order.session &&
                   out_of_order.session->issue_token(10'400, 100).status ==
                       prisminfer::AdmissionTokenStatus::
                           SupervisorStateInvalid &&
                   out_of_order.session->state() ==
                       GpuAdmissionSessionState::FailedClosed,
               "out-of-order token issuance fails closed")) {
      return 1;
    }
  }

  {
    auto acquired = prisminfer::acquire_gpu_admission_session(cell(), 7, 11);
    if (expect(acquired.lease_status == ExclusiveGpuLeaseStatus::Acquired &&
                   acquired.session && !acquired.session->lease_id().empty(),
               "session owns a real adapter lease") ||
        expect(prisminfer::acquire_exclusive_gpu_lease(7, 11).status ==
                   ExclusiveGpuLeaseStatus::AlreadyHeldInProcess,
               "session prevents a competing lease")) {
      return 1;
    }

    auto pre = pre_request();
    pre.exclusive_gpu_lease_held = false;
    const auto pre_decision = acquired.session->admit_pre_context(pre);
    if (expect(pre_decision.admitted &&
                   acquired.session->state() ==
                       GpuAdmissionSessionState::PreContextAdmitted,
               "owned lease, not caller boolean, authorizes Stage A")) {
      return 1;
    }
    auto post = acquired.session->admit_post_context(post_request());
    if (expect(post.admitted &&
                   acquired.session->state() ==
                       GpuAdmissionSessionState::PostContextAdmitted,
               "session supplies its own Stage-A receipt to Stage B")) {
      return 1;
    }
    auto token = acquired.session->issue_token(10'400, 100);
    if (expect(token.status == prisminfer::AdmissionTokenStatus::Issued &&
                   token.token && acquired.session->state() ==
                                      GpuAdmissionSessionState::TokenIssued,
               "ordered exact session issues one token")) {
      return 1;
    }
    auto authority = std::move(*token.token);
    if (expect(acquired.session->consume_token(authority, 10'450).status ==
                   prisminfer::AdmissionTokenStatus::Consumed &&
                   acquired.session->state() ==
                       GpuAdmissionSessionState::TokenConsumed,
               "session consumes its exact token once") ||
        expect(acquired.session->issue_token(10'400, 100).status ==
                   prisminfer::AdmissionTokenStatus::SupervisorStateInvalid &&
                   acquired.session->state() ==
                       GpuAdmissionSessionState::FailedClosed,
               "duplicate token transition fails closed") ||
        expect(acquired.session->cleanup(true) ==
                   GpuAdmissionSessionState::Cleaned,
               "reconciled cleanup releases session resources")) {
      return 1;
    }
  }

  if (expect(prisminfer::acquire_exclusive_gpu_lease(7, 11).status ==
                 ExclusiveGpuLeaseStatus::Acquired,
             "cleanup releases the OS-wide lease")) {
    return 1;
  }

  {
    auto acquired = prisminfer::acquire_gpu_admission_session(cell(), 7, 11);
    const auto rejected = acquired.session->admit_pre_context(pre_request(cell(2)));
    if (expect(!rejected.admitted && acquired.session->state() ==
                                        GpuAdmissionSessionState::FailedClosed,
               "session rejects another valid run cell") ||
        expect(acquired.session->cleanup(false) ==
                   GpuAdmissionSessionState::Quarantined,
               "unreconciled cleanup quarantines")) {
      return 1;
    }
  }
  return 0;
}

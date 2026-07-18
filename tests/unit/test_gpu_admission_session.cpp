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

prisminfer::ContainedWorkerEvidence contained_worker() {
  prisminfer::ContainedWorkerEvidence evidence;
  evidence.created_suspended = true;
  evidence.job_assigned_before_resume = true;
  evidence.non_breakaway_job = true;
  evidence.kill_on_close = true;
  evidence.controlled_environment = true;
  evidence.controlled_inherited_handles = true;
  evidence.root_process_id = 1234;
  evidence.maximum_active_processes = 1;
  evidence.job_memory_limit_bytes = 8 * kGiB;
  evidence.job_cpu_time_limit_milliseconds = 60'000;
  evidence.job_identity = "job-test";
  evidence.executable_identity = "approved-test-worker";
  return evidence;
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
    if (expect(acquired.session->bind_contained_worker(contained_worker()),
               "secure worker containment precedes Stage B")) {
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
        expect(acquired.session->begin_cleanup(10'500),
               "failed-closed session enters bounded cleanup") ||
        expect(acquired.session->cleanup(true, 10'600) ==
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
        expect(acquired.session->begin_cleanup(10'500),
               "rejected session enters cleanup") ||
        expect(acquired.session->cleanup(false, 10'600) ==
                   GpuAdmissionSessionState::Quarantined,
               "unreconciled cleanup quarantines")) {
      return 1;
    }
  }


  {
    auto acquired = prisminfer::acquire_gpu_admission_session(cell(), 7, 11);
    auto pre = pre_request();
    if (expect(acquired.session->admit_pre_context(pre).admitted,
               "cancel test Stage A admits") ||
        expect(acquired.session->bind_contained_worker(contained_worker()),
               "cancel test worker is contained") ||
        expect(acquired.session->admit_post_context(post_request()).admitted,
               "cancel test Stage B admits")) {
      return 1;
    }
    auto issued = acquired.session->issue_token(10'400, 100);
    auto token = std::move(*issued.token);
    if (expect(acquired.session->consume_token(token, 10'450).status ==
                   prisminfer::AdmissionTokenStatus::Consumed,
               "cancel test consumes exact token")) {
      return 1;
    }
    auto safe = watchdog_sample();
    if (expect(acquired.session->evaluate_watchdog(safe).continue_work &&
                   acquired.session->state() ==
                       GpuAdmissionSessionState::WatchdogActive,
               "fresh watchdog activates supervised work")) {
      return 1;
    }
    auto stale = safe;
    stale.evaluated_monotonic_milliseconds = 10'900;
    if (expect(!acquired.session->evaluate_watchdog(stale).continue_work &&
                   acquired.session->state() ==
                       GpuAdmissionSessionState::CancelRequested,
               "telemetry loss blocks submissions and requests cancel")) {
      return 1;
    }
    const auto abort = acquired.session->advance_cancellation(
        11'400, false, false, false);
    if (expect(abort.action == prisminfer::CancellationAction::AbortJob &&
                   abort.state == GpuAdmissionSessionState::JobAbortRequired,
               "missed acknowledgement deadline requires Job abort") ||
        expect(acquired.session->record_job_abort(11'500, true).action ==
                   prisminfer::CancellationAction::BeginCleanup,
               "empty terminated Job tree permits cleanup") ||
        expect(acquired.session->begin_cleanup(11'600),
               "aborted run starts bounded cleanup") ||
        expect(acquired.session->cleanup(true, 11'700) ==
                   GpuAdmissionSessionState::Cleaned,
               "reconciled aborted run releases the lease")) {
      return 1;
    }
  }


  {
    auto acquired = prisminfer::acquire_gpu_admission_session(cell(), 7, 11);
    auto pre = pre_request();
    if (expect(acquired.session->admit_pre_context(pre).admitted,
               "cooperative test Stage A admits") ||
        expect(acquired.session->bind_contained_worker(contained_worker()),
               "cooperative test worker is contained") ||
        expect(acquired.session->admit_post_context(post_request()).admitted,
               "cooperative test Stage B admits")) {
      return 1;
    }
    auto issued = acquired.session->issue_token(10'400, 100);
    auto token = std::move(*issued.token);
    if (expect(acquired.session->consume_token(token, 10'450).status ==
                   prisminfer::AdmissionTokenStatus::Consumed,
               "cooperative test consumes token")) {
      return 1;
    }
    auto stale = watchdog_sample();
    stale.evaluated_monotonic_milliseconds = 10'900;
    (void)acquired.session->evaluate_watchdog(stale);
    if (expect(acquired.session->advance_cancellation(
                   11'100, true, false, false).state ==
                   GpuAdmissionSessionState::CooperativeCancelAcknowledged,
               "timely cooperative acknowledgement is retained") ||
        expect(acquired.session->advance_cancellation(
                   11'600, false, false, false).action ==
                   prisminfer::CancellationAction::AwaitCooperativeExit,
               "acknowledged worker receives its bounded exit window") ||
        expect(acquired.session->advance_cancellation(
                   12'901, false, true, true).action ==
                   prisminfer::CancellationAction::AbortJob,
               "late worker exit cannot erase a missed exit deadline") ||
        expect(acquired.session->record_job_abort(13'000, true).action ==
                   prisminfer::CancellationAction::BeginCleanup,
               "late exit still requires supervisor Job reconciliation") ||
        expect(acquired.session->begin_cleanup(13'100),
               "cooperative timeout enters cleanup") ||
        expect(acquired.session->cleanup(true, 16'001) ==
                   GpuAdmissionSessionState::Quarantined,
               "cleanup beyond the cancel-relative deadline quarantines")) {
      return 1;
    }
  }

  {
    auto acquired = prisminfer::acquire_gpu_admission_session(cell(), 7, 11);
    auto pre = pre_request();
    auto invalid_worker = contained_worker();
    invalid_worker.kill_on_close = false;
    if (expect(acquired.session->admit_pre_context(pre).admitted,
               "invalid worker test Stage A admits") ||
        expect(!acquired.session->bind_contained_worker(invalid_worker) &&
                   acquired.session->state() ==
                       GpuAdmissionSessionState::FailedClosed,
               "worker without kill-on-close containment fails closed") ||
        expect(acquired.session->begin_cleanup(12'000),
               "invalid worker enters zero-resource cleanup") ||
        expect(acquired.session->cleanup(false, 12'100) ==
                   GpuAdmissionSessionState::Quarantined,
               "unreconciled containment failure quarantines")) {
      return 1;
    }
  }
  return 0;
}

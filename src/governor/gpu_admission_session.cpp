#include "prisminfer/gpu_admission_session.h"

#include <cerrno>
#include <mutex>
#include <utility>

namespace prisminfer {

namespace {

bool is_sha256(const std::string& value) {
  if (value.size() != 64U) return false;
  for (const char character : value) {
    const bool digit = character >= '0' && character <= '9';
    const bool lower = character >= 'a' && character <= 'f';
    if (!digit && !lower) return false;
  }
  return true;
}

}  // namespace

struct GpuAdmissionSession::Impl {
  AdmissionCellIdentity cell;
  std::int32_t adapter_luid_high{0};
  std::uint32_t adapter_luid_low{0};
  std::optional<ExclusiveGpuLease> lease;
  GpuAdmissionSessionState state{GpuAdmissionSessionState::LeaseHeld};
  std::optional<PreContextAdmissionReceipt> pre_receipt;
  std::optional<PostContextAdmissionReceipt> post_receipt;
  std::optional<ContainedWorkerEvidence> worker;
  std::uint64_t cancel_requested_monotonic_milliseconds{0};
  std::uint64_t cleanup_started_monotonic_milliseconds{0};
  std::optional<SupervisorCleanupEvidence> terminal_evidence;
  AdmissionTokenIssuer token_issuer;
  mutable std::mutex mutex;
};

GpuAdmissionSession::GpuAdmissionSession(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

GpuAdmissionSession::GpuAdmissionSession(GpuAdmissionSession&&) noexcept =
    default;
GpuAdmissionSession::~GpuAdmissionSession() {
  if (!impl_) return;
  std::lock_guard lock(impl_->mutex);
  if (impl_->lease && impl_->state != GpuAdmissionSessionState::Cleaned) {
    impl_->lease->quarantine_for_process_lifetime();
  }
}

GpuAdmissionSessionState GpuAdmissionSession::state() const {
  if (!impl_) return GpuAdmissionSessionState::FailedClosed;
  std::lock_guard lock(impl_->mutex);
  return impl_->state;
}

bool GpuAdmissionSession::bind_contained_worker(
    const ContainedWorkerEvidence& evidence) {
  if (!impl_) return false;
  std::lock_guard lock(impl_->mutex);
  const bool valid =
      impl_->state == GpuAdmissionSessionState::PreContextAdmitted &&
      evidence.created_suspended && evidence.job_assigned_before_resume &&
      evidence.non_breakaway_job && evidence.kill_on_close &&
      evidence.controlled_environment &&
      evidence.controlled_inherited_handles && evidence.root_process_id != 0 &&
      evidence.maximum_active_processes != 0 &&
      evidence.job_memory_limit_bytes != 0 &&
      evidence.job_cpu_time_limit_milliseconds != 0 &&
      !evidence.job_identity.empty() && !evidence.executable_identity.empty();
  if (!valid) {
    impl_->state = GpuAdmissionSessionState::FailedClosed;
    return false;
  }
  impl_->worker = evidence;
  impl_->state = GpuAdmissionSessionState::WorkerContained;
  return true;
}

std::string GpuAdmissionSession::lease_id() const {
  if (!impl_) return {};
  std::lock_guard lock(impl_->mutex);
  return impl_->lease ? impl_->lease->lease_id() : std::string{};
}

PreContextAdmissionDecision GpuAdmissionSession::admit_pre_context(
    PreContextAdmissionRequest request) {
  PreContextAdmissionDecision rejected;
  if (!impl_) {
    rejected.reason = "supervisor_pre_context_state_or_identity_mismatch";
    return rejected;
  }
  std::lock_guard lock(impl_->mutex);
  if (impl_->state != GpuAdmissionSessionState::LeaseHeld ||
      request.cell != impl_->cell ||
      request.gpu.adapter_luid_high != impl_->adapter_luid_high ||
      request.gpu.adapter_luid_low != impl_->adapter_luid_low) {
    rejected.reason = "supervisor_pre_context_state_or_identity_mismatch";
    if (impl_) impl_->state = GpuAdmissionSessionState::FailedClosed;
    return rejected;
  }
  request.exclusive_gpu_lease_held = impl_->lease && impl_->lease->active();
  auto decision = evaluate_pre_context_admission(request);
  if (!decision.admitted || !decision.receipt) {
    impl_->state = GpuAdmissionSessionState::FailedClosed;
    return decision;
  }
  impl_->pre_receipt = decision.receipt;
  impl_->state = GpuAdmissionSessionState::PreContextAdmitted;
  return decision;
}

PostContextAdmissionDecision GpuAdmissionSession::admit_post_context(
    PostContextAdmissionRequest request) {
  PostContextAdmissionDecision rejected;
  if (!impl_) {
    rejected.reason = "supervisor_post_context_state_or_identity_mismatch";
    return rejected;
  }
  std::lock_guard lock(impl_->mutex);
  if (impl_->state != GpuAdmissionSessionState::WorkerContained ||
      !impl_->worker ||
      !impl_->pre_receipt || request.cell != impl_->cell ||
      request.gpu.adapter_luid_high != impl_->adapter_luid_high ||
      request.gpu.adapter_luid_low != impl_->adapter_luid_low) {
    rejected.reason = "supervisor_post_context_state_or_identity_mismatch";
    if (impl_) impl_->state = GpuAdmissionSessionState::FailedClosed;
    return rejected;
  }
  request.pre_context_receipt = impl_->pre_receipt;
  auto decision = evaluate_post_context_admission(request);
  if (!decision.admitted || !decision.receipt) {
    impl_->state = GpuAdmissionSessionState::FailedClosed;
    return decision;
  }
  impl_->post_receipt = decision.receipt;
  impl_->state = GpuAdmissionSessionState::PostContextAdmitted;
  return decision;
}

AdmissionTokenIssueResult GpuAdmissionSession::issue_token(
    std::uint64_t now_monotonic_milliseconds,
    std::uint64_t validity_milliseconds) {
  AdmissionTokenIssueResult rejected;
  rejected.status = AdmissionTokenStatus::SupervisorStateInvalid;
  if (!impl_) return rejected;
  std::lock_guard lock(impl_->mutex);
  if (impl_->state != GpuAdmissionSessionState::PostContextAdmitted ||
      !impl_->post_receipt) {
    if (impl_) impl_->state = GpuAdmissionSessionState::FailedClosed;
    return rejected;
  }
  const AdmissionWorkerIdentity worker_identity{
      impl_->worker->root_process_id, impl_->worker->job_identity,
      impl_->worker->executable_identity};
  auto issued = impl_->token_issuer.issue(*impl_->post_receipt, impl_->cell,
                                          worker_identity,
                                          now_monotonic_milliseconds,
                                          validity_milliseconds);
  if (issued.status != AdmissionTokenStatus::Issued || !issued.token) {
    impl_->state = GpuAdmissionSessionState::FailedClosed;
    return issued;
  }
  impl_->state = GpuAdmissionSessionState::TokenIssued;
  return issued;
}

AdmissionTokenConsumeResult GpuAdmissionSession::consume_token(
    AdmissionToken& token, std::uint64_t now_monotonic_milliseconds) {
  AdmissionTokenConsumeResult rejected;
  rejected.status = AdmissionTokenStatus::SupervisorStateInvalid;
  if (!impl_) return rejected;
  std::lock_guard lock(impl_->mutex);
  if (impl_->state != GpuAdmissionSessionState::TokenIssued) {
    if (impl_) impl_->state = GpuAdmissionSessionState::FailedClosed;
    return rejected;
  }
  if (!impl_->worker) {
    impl_->state = GpuAdmissionSessionState::FailedClosed;
    return rejected;
  }
  const AdmissionWorkerIdentity worker_identity{
      impl_->worker->root_process_id, impl_->worker->job_identity,
      impl_->worker->executable_identity};
  auto consumed =
      impl_->token_issuer.consume(token, impl_->cell, worker_identity,
                                  now_monotonic_milliseconds);
  if (consumed.status != AdmissionTokenStatus::Consumed) {
    impl_->state = GpuAdmissionSessionState::FailedClosed;
    return consumed;
  }
  impl_->state = GpuAdmissionSessionState::TokenConsumed;
  return consumed;
}

SupervisorWatchdogDecision GpuAdmissionSession::evaluate_watchdog(
    const SupervisorWatchdogSample& sample) {
  SupervisorWatchdogDecision rejected;
  if (!impl_) return rejected;
  std::lock_guard lock(impl_->mutex);
  if ((impl_->state != GpuAdmissionSessionState::TokenConsumed &&
       impl_->state != GpuAdmissionSessionState::WatchdogActive) ||
      !impl_->post_receipt) {
    impl_->state = GpuAdmissionSessionState::FailedClosed;
    return rejected;
  }
  auto decision = evaluate_supervisor_watchdog(*impl_->post_receipt, sample);
  if (decision.continue_work) {
    impl_->state = GpuAdmissionSessionState::WatchdogActive;
  } else {
    impl_->state = GpuAdmissionSessionState::CancelRequested;
    impl_->cancel_requested_monotonic_milliseconds =
        sample.evaluated_monotonic_milliseconds;
  }
  return decision;
}

CancellationDecision GpuAdmissionSession::advance_cancellation(
    std::uint64_t now, bool acknowledged, bool worker_exited,
    bool job_tree_empty) {
  CancellationDecision decision;
  if (!impl_) return decision;
  std::lock_guard lock(impl_->mutex);
  if ((impl_->state != GpuAdmissionSessionState::CancelRequested &&
       impl_->state !=
           GpuAdmissionSessionState::CooperativeCancelAcknowledged) ||
      !impl_->post_receipt ||
      now < impl_->cancel_requested_monotonic_milliseconds) {
    impl_->state = GpuAdmissionSessionState::Quarantined;
    decision.state = impl_->state;
    return decision;
  }
  const auto elapsed = now - impl_->cancel_requested_monotonic_milliseconds;
  const auto& timing = impl_->post_receipt->timing();
  const bool cancellation_was_acknowledged =
      impl_->state ==
      GpuAdmissionSessionState::CooperativeCancelAcknowledged;
  if (elapsed > timing.worker_exit_milliseconds) {
    impl_->state = GpuAdmissionSessionState::JobAbortRequired;
    decision.action = CancellationAction::AbortJob;
  } else if (worker_exited) {
    if (!job_tree_empty) {
      impl_->state = GpuAdmissionSessionState::JobAbortRequired;
      decision.action = CancellationAction::AbortJob;
    } else {
      impl_->state = GpuAdmissionSessionState::WorkerExited;
      decision.action = CancellationAction::BeginCleanup;
    }
  } else if (elapsed == timing.worker_exit_milliseconds) {
    impl_->state = GpuAdmissionSessionState::JobAbortRequired;
    decision.action = CancellationAction::AbortJob;
  } else if (acknowledged &&
             elapsed <= timing.cooperative_cancel_ack_milliseconds) {
    impl_->state = GpuAdmissionSessionState::CooperativeCancelAcknowledged;
    decision.action = CancellationAction::AwaitCooperativeExit;
  } else if (!cancellation_was_acknowledged &&
             elapsed >= timing.cooperative_cancel_ack_milliseconds) {
    impl_->state = GpuAdmissionSessionState::JobAbortRequired;
    decision.action = CancellationAction::AbortJob;
  } else {
    decision.action = CancellationAction::AwaitCooperativeExit;
  }
  decision.state = impl_->state;
  return decision;
}

CancellationDecision GpuAdmissionSession::record_job_abort(
    std::uint64_t now, bool job_tree_empty) {
  CancellationDecision decision;
  if (!impl_) return decision;
  std::lock_guard lock(impl_->mutex);
  if (impl_->state != GpuAdmissionSessionState::JobAbortRequired ||
      !impl_->post_receipt ||
      now < impl_->cancel_requested_monotonic_milliseconds) {
    impl_->state = GpuAdmissionSessionState::Quarantined;
    decision.state = impl_->state;
    return decision;
  }
  const auto elapsed = now - impl_->cancel_requested_monotonic_milliseconds;
  if (!job_tree_empty ||
      elapsed > impl_->post_receipt->timing().cleanup_milliseconds) {
    impl_->state = GpuAdmissionSessionState::Quarantined;
    decision.action = CancellationAction::Quarantine;
  } else {
    impl_->state = GpuAdmissionSessionState::WorkerExited;
    decision.action = CancellationAction::BeginCleanup;
  }
  decision.state = impl_->state;
  return decision;
}

bool GpuAdmissionSession::record_normal_worker_exit(std::uint64_t now,
                                                    bool job_tree_empty) {
  if (!impl_ || now == 0U) return false;
  std::lock_guard lock(impl_->mutex);
  if (impl_->state != GpuAdmissionSessionState::WatchdogActive ||
      !impl_->post_receipt || !job_tree_empty) {
    impl_->state = GpuAdmissionSessionState::Quarantined;
    if (impl_->lease) impl_->lease->quarantine_for_process_lifetime();
    return false;
  }
  impl_->state = GpuAdmissionSessionState::WorkerExited;
  return true;
}

bool GpuAdmissionSession::begin_cleanup(std::uint64_t now) {
  if (!impl_ || now == 0) return false;
  std::lock_guard lock(impl_->mutex);
  if (impl_->state != GpuAdmissionSessionState::WorkerExited &&
      (impl_->state != GpuAdmissionSessionState::FailedClosed ||
       impl_->worker.has_value())) {
    impl_->state = GpuAdmissionSessionState::Quarantined;
    if (impl_->lease) impl_->lease->quarantine_for_process_lifetime();
    return false;
  }
  impl_->cleanup_started_monotonic_milliseconds = now;
  impl_->state = GpuAdmissionSessionState::CleanupStarted;
  return true;
}

GpuAdmissionSessionState GpuAdmissionSession::cleanup(
    const SupervisorCleanupEvidence& evidence, std::uint64_t now) {
  if (!impl_) return GpuAdmissionSessionState::FailedClosed;
  std::lock_guard lock(impl_->mutex);
  if (impl_->state == GpuAdmissionSessionState::Cleaned ||
      impl_->state == GpuAdmissionSessionState::Quarantined) {
    impl_->state = GpuAdmissionSessionState::Quarantined;
    return impl_->state;
  }
  const bool deadline_met =
      impl_->state == GpuAdmissionSessionState::CleanupStarted &&
      impl_->cleanup_started_monotonic_milliseconds != 0 &&
      now >= impl_->cleanup_started_monotonic_milliseconds &&
      impl_->post_receipt &&
      now - impl_->cleanup_started_monotonic_milliseconds <=
          impl_->post_receipt->timing().cleanup_milliseconds &&
      (impl_->cancel_requested_monotonic_milliseconds == 0 ||
       (now >= impl_->cancel_requested_monotonic_milliseconds &&
        now - impl_->cancel_requested_monotonic_milliseconds <=
            impl_->post_receipt->timing().cleanup_milliseconds));
  impl_->pre_receipt.reset();
  impl_->post_receipt.reset();
  impl_->worker.reset();
  const bool evidence_complete =
      evidence.worker_exit_observed && evidence.job_tree_empty &&
      evidence.job_accounting_reconciled &&
      evidence.device_resources_reconciled &&
      evidence.artifact_handles_closed &&
      evidence.temporary_files_reconciled &&
      !evidence.terminal_reason.empty() &&
      is_sha256(evidence.last_good_sample_sha256) &&
      is_sha256(evidence.evidence_bundle_sha256);
  impl_->terminal_evidence = evidence;
  const bool cleaned = evidence_complete && deadline_met;
  if (!cleaned && impl_->lease) {
    impl_->lease->quarantine_for_process_lifetime();
  }
  impl_->lease.reset();
  impl_->state = cleaned ? GpuAdmissionSessionState::Cleaned
                         : GpuAdmissionSessionState::Quarantined;
  return impl_->state;
}

std::optional<SupervisorCleanupEvidence>
GpuAdmissionSession::terminal_evidence() const {
  if (!impl_) return std::nullopt;
  std::lock_guard lock(impl_->mutex);
  return impl_->terminal_evidence;
}

GpuAdmissionSessionAcquireResult acquire_gpu_admission_session(
    const AdmissionCellIdentity& cell, std::int32_t adapter_luid_high,
    std::uint32_t adapter_luid_low) {
  GpuAdmissionSessionAcquireResult result;
  if (!admission_cell_identity_valid(cell)) {
    result.status =
        GpuAdmissionSessionAcquireResult::Status::InvalidCellIdentity;
    return result;
  }
  auto acquired =
      acquire_exclusive_gpu_lease(adapter_luid_high, adapter_luid_low);
  result.lease_status = acquired.status;
  result.system_error = acquired.system_error;
  if (acquired.status != ExclusiveGpuLeaseStatus::Acquired ||
      !acquired.lease) {
    return result;
  }
  std::unique_ptr<GpuAdmissionSession::Impl> impl;
  try {
    impl = std::make_unique<GpuAdmissionSession::Impl>();
  } catch (const std::bad_alloc&) {
    result.system_error = static_cast<std::uint32_t>(ENOMEM);
    return result;
  }
  impl->cell = cell;
  impl->adapter_luid_high = adapter_luid_high;
  impl->adapter_luid_low = adapter_luid_low;
  impl->lease = std::move(acquired.lease);
  result.session.emplace(GpuAdmissionSession(std::move(impl)));
  result.status = GpuAdmissionSessionAcquireResult::Status::Acquired;
  return result;
}

}  // namespace prisminfer

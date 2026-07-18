#include "prisminfer/gpu_admission_session.h"

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

#include "prisminfer/windows_evidence.h"
#include "prisminfer/telemetry.h"

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

template <typename Result, typename Callable>
std::optional<Result> bounded_evidence_call(Callable callable,
                                            std::uint32_t timeout_ms) {
  struct SharedState {
    std::mutex mutex;
    std::condition_variable ready;
    std::optional<Result> result;
    bool completed{false};
  };
  if (timeout_ms == 0U) return std::nullopt;
  auto shared = std::make_shared<SharedState>();
  std::jthread producer([shared, callable = std::move(callable)](
                            std::stop_token stop) mutable {
    std::optional<Result> result;
    try {
      result = callable(stop);
    } catch (...) {
      result.reset();
    }
    {
      std::lock_guard lock(shared->mutex);
      shared->result = std::move(result);
      shared->completed = true;
    }
    shared->ready.notify_one();
  });
  std::unique_lock lock(shared->mutex);
  if (!shared->ready.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                              [&] { return shared->completed; })) {
    producer.request_stop();
    constexpr std::uint32_t kStopGraceMilliseconds = 100U;
    if (!shared->ready.wait_for(
            lock, std::chrono::milliseconds(kStopGraceMilliseconds),
            [&] { return shared->completed; })) {
      std::_Exit(static_cast<int>(kEvidenceProviderFailStopExitCode));
    }
    lock.unlock();
    producer.join();
    return std::nullopt;
  }
  auto result = std::move(shared->result);
  lock.unlock();
  producer.join();
  return result;
}

struct LivePostContextEvidence {
  PostContextAdmissionRequest request;
  ProcessDeviceMemorySample independent;
};

struct LiveWatchdogEvidence {
  SupervisorWatchdogSample sample;
  ProcessDeviceMemorySample independent;
};

class CallbackProtocolSupervisor final
    : public NativeWorkerProtocolSupervisor {
 public:
  std::function<bool(const NativeWorkerContainmentIdentity&)> bind;
  std::function<NativeWorkerAdmissionGrant(std::uint64_t)> ready;
  std::function<bool(std::uint64_t, std::uint64_t)> consumed;
  std::function<bool(std::uint64_t, std::uint64_t)> beat;
  std::function<void(std::uint64_t)> cancel_ack;

  bool bind_owned_worker(
      const NativeWorkerContainmentIdentity& identity) override {
    return bind(identity);
  }
  NativeWorkerAdmissionGrant context_ready(std::uint64_t now) override {
    return ready(now);
  }
  bool token_consumed(std::uint64_t token_id, std::uint64_t now) override {
    return consumed(token_id, now);
  }
  bool heartbeat(std::uint64_t sequence, std::uint64_t now) override {
    return beat(sequence, now);
  }
  void cooperative_cancel_acknowledged(std::uint64_t now) override {
    cancel_ack(now);
  }
};

}  // namespace

struct GpuAdmissionSession::Impl {
  AdmissionCellIdentity cell;
  std::int32_t adapter_luid_high{0};
  std::uint32_t adapter_luid_low{0};
  std::optional<ExclusiveGpuLease> lease;
  GpuAdmissionSessionState state{GpuAdmissionSessionState::LeaseHeld};
  std::optional<PreContextAdmissionReceipt> pre_receipt;
  std::optional<PostContextAdmissionReceipt> post_receipt;
  std::optional<NativeWorkerContainmentIdentity> worker;
  std::optional<AdmissionToken> pending_token;
  bool owned_worker_exit_observed{false};
  bool owned_job_tree_empty{false};
  bool owned_job_accounting_reconciled{false};
  bool owned_artifact_handles_closed{false};
  bool owned_temporary_files_reconciled{false};
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

bool GpuAdmissionSession::bind_owned_worker(
    const NativeWorkerContainmentIdentity& evidence) {
  if (!impl_) return false;
  std::lock_guard lock(impl_->mutex);
  const bool valid =
      impl_->state == GpuAdmissionSessionState::PreContextAdmitted &&
      evidence.root_process_id != 0 &&
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
      request.owned_gpu.process_id != impl_->worker->root_process_id ||
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
      !impl_->post_receipt || !impl_->worker ||
      sample.owned_gpu.process_id != impl_->worker->root_process_id) {
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

NativeWorkerResult GpuAdmissionSession::run_contained_worker(
    const NativeWorkerTrustCatalog& catalog,
    const NativeWorkerRequest& request,
    std::uint64_t token_validity_milliseconds,
    PostContextEvidenceProducer post_context_evidence,
    WatchdogEvidenceProducer watchdog_evidence,
    ProcessDeviceEvidenceProducer process_device_evidence) {
  if (!impl_ || token_validity_milliseconds == 0U ||
      !post_context_evidence || !watchdog_evidence) {
    NativeWorkerResult rejected;
    rejected.failure_reason = "supervisor_worker_request_invalid";
    return rejected;
  }
  if (!process_device_evidence) {
    process_device_evidence =
        [](std::stop_token stop, std::uint32_t process_id,
           std::int32_t luid_high, std::uint32_t luid_low) {
          if (stop.stop_requested()) return ProcessDeviceMemorySample{};
          auto sample = sample_process_device_memory(process_id, luid_high,
                                                     luid_low);
          if (stop.stop_requested()) return ProcessDeviceMemorySample{};
          return sample;
        };
  }
  CallbackProtocolSupervisor protocol;
  NativeWorkerProtocolPolicy protocol_policy;
  {
    std::lock_guard lock(impl_->mutex);
    if (!impl_->pre_receipt ||
        impl_->state != GpuAdmissionSessionState::PreContextAdmitted) {
      NativeWorkerResult rejected;
      rejected.failure_reason = "supervisor_pre_context_receipt_required";
      return rejected;
    }
    const auto& timing = impl_->pre_receipt->timing();
    protocol_policy.context_ready_timeout_ms =
        static_cast<std::uint32_t>(timing.maximum_guard_age_milliseconds);
    protocol_policy.heartbeat_timeout_ms =
        static_cast<std::uint32_t>(timing.maximum_guard_age_milliseconds);
    protocol_policy.cooperative_cancel_ack_timeout_ms =
        static_cast<std::uint32_t>(
            timing.cooperative_cancel_ack_milliseconds);
    protocol_policy.worker_exit_timeout_ms =
        static_cast<std::uint32_t>(timing.worker_exit_milliseconds);
  }
  protocol.bind = [this](const NativeWorkerContainmentIdentity& identity) {
    return bind_owned_worker(identity);
  };
  protocol.ready = [this, &post_context_evidence, process_device_evidence,
                    protocol_policy,
                    token_validity_milliseconds](std::uint64_t now) {
    NativeWorkerAdmissionGrant grant;
    std::uint32_t process_id = 0U;
    {
      std::lock_guard lock(impl_->mutex);
      if (!impl_->worker) return grant;
      process_id = impl_->worker->root_process_id;
    }
    std::int32_t luid_high = 0;
    std::uint32_t luid_low = 0U;
    {
      std::lock_guard lock(impl_->mutex);
      luid_high = impl_->adapter_luid_high;
      luid_low = impl_->adapter_luid_low;
    }
    const auto live = bounded_evidence_call<LivePostContextEvidence>(
        [producer = post_context_evidence,
         independent_producer = process_device_evidence, process_id, now,
         luid_high, luid_low](std::stop_token stop) {
          return LivePostContextEvidence{
              producer(stop, process_id, now),
              independent_producer(stop, process_id, luid_high, luid_low)};
        },
        protocol_policy.heartbeat_timeout_ms);
    if (!live) {
      std::lock_guard lock(impl_->mutex);
      impl_->state = GpuAdmissionSessionState::FailedClosed;
      grant.failure_reason = "post_context_evidence_timeout";
      return grant;
    }
    const auto evaluated_at = monotonic_time_milliseconds();
    if (evaluated_at == 0U) {
      std::lock_guard lock(impl_->mutex);
      impl_->state = GpuAdmissionSessionState::FailedClosed;
      grant.failure_reason = "monotonic_clock_invalid";
      return grant;
    }
    auto request = std::move(live->request);
    request.evaluation_monotonic_milliseconds = evaluated_at;
    const auto& independent = live->independent;
    if (!bind_process_device_memory_evidence(
            &request.owned_gpu, independent, process_id, luid_high,
            luid_low, evaluated_at,
            protocol_policy.heartbeat_timeout_ms)) {
      {
        std::lock_guard lock(impl_->mutex);
        impl_->state = GpuAdmissionSessionState::FailedClosed;
      }
      grant.failure_reason = independent.unavailable_reason.empty()
                                 ? "process_device_evidence_rejected"
                                 : independent.unavailable_reason;
      return grant;
    }
    const auto admitted = admit_post_context(std::move(request));
    if (!admitted.admitted) {
      grant.failure_reason = admitted.reason;
      return grant;
    }
    auto issued = issue_token(evaluated_at, token_validity_milliseconds);
    if (issued.status != AdmissionTokenStatus::Issued || !issued.token) {
      grant.failure_reason = "token_issue_failed";
      return grant;
    }
    grant.admitted = true;
    grant.token_id = issued.token->token_id();
    grant.effective_cap_bytes = issued.token->effective_cap_bytes();
    grant.expires_monotonic_milliseconds =
        issued.token->expires_monotonic_milliseconds();
    {
      std::lock_guard lock(impl_->mutex);
      impl_->pending_token = std::move(issued.token);
    }
    return grant;
  };
  protocol.consumed = [this](std::uint64_t token_id, std::uint64_t now) {
    std::optional<AdmissionToken> token;
    {
      std::lock_guard lock(impl_->mutex);
      if (!impl_->pending_token ||
          impl_->pending_token->token_id() != token_id) {
        impl_->state = GpuAdmissionSessionState::FailedClosed;
        return false;
      }
      token = std::move(impl_->pending_token);
      impl_->pending_token.reset();
    }
    return consume_token(*token, now).status == AdmissionTokenStatus::Consumed;
  };
  protocol.beat = [this, &watchdog_evidence, process_device_evidence,
                   protocol_policy](
                      std::uint64_t sequence, std::uint64_t now) {
    std::uint32_t process_id = 0U;
    {
      std::lock_guard lock(impl_->mutex);
      if (!impl_->worker) return false;
      process_id = impl_->worker->root_process_id;
    }
    std::int32_t luid_high = 0;
    std::uint32_t luid_low = 0U;
    {
      std::lock_guard lock(impl_->mutex);
      if (!impl_->post_receipt) return false;
      luid_high = impl_->adapter_luid_high;
      luid_low = impl_->adapter_luid_low;
    }
    const auto live = bounded_evidence_call<LiveWatchdogEvidence>(
        [producer = watchdog_evidence,
         independent_producer = process_device_evidence, process_id, sequence,
         now, luid_high, luid_low](std::stop_token stop) {
          return LiveWatchdogEvidence{
              producer(stop, process_id, sequence, now),
              independent_producer(stop, process_id, luid_high, luid_low)};
        },
        protocol_policy.heartbeat_timeout_ms);
    if (!live) return false;
    const auto evaluated_at = monotonic_time_milliseconds();
    if (evaluated_at == 0U) return false;
    auto sample = std::move(live->sample);
    sample.evaluated_monotonic_milliseconds = evaluated_at;
    sample.worker_heartbeat_monotonic_milliseconds = now;
    sample.worker_alive = true;
    {
      std::lock_guard lock(impl_->mutex);
      if (!impl_->post_receipt) return false;
      sample.run_deadline_monotonic_milliseconds =
          impl_->post_receipt->run_deadline_monotonic_milliseconds();
      sample.owned_gpu.hard_cap_bytes =
          impl_->post_receipt->effective_cap_bytes();
    }
    const auto& independent = live->independent;
    if (!bind_process_device_memory_evidence(
            &sample.owned_gpu, independent, process_id, luid_high,
            luid_low, evaluated_at,
            protocol_policy.heartbeat_timeout_ms)) {
      return false;
    }
    return evaluate_watchdog(sample).continue_work;
  };
  protocol.cancel_ack = [this](std::uint64_t now) {
    (void)advance_cancellation(now, true, false, false);
  };
  auto result = run_supervised_native_worker(catalog, request,
                                             protocol_policy, protocol,
                                             NativeWorkerSupervisorAccess{});
  {
    std::lock_guard lock(impl_->mutex);
    impl_->pending_token.reset();
    impl_->owned_worker_exit_observed = result.worker_exit_observed;
    impl_->owned_job_tree_empty = result.job_tree_empty;
    impl_->owned_job_accounting_reconciled =
        result.job_accounting_reconciled;
    impl_->owned_artifact_handles_closed = result.artifact_handles_closed;
    impl_->owned_temporary_files_reconciled =
        result.temporary_files_reconciled;
    const bool exit_state =
        impl_->state == GpuAdmissionSessionState::WatchdogActive ||
        impl_->state == GpuAdmissionSessionState::CancelRequested ||
        impl_->state ==
            GpuAdmissionSessionState::CooperativeCancelAcknowledged ||
        impl_->state == GpuAdmissionSessionState::JobAbortRequired;
    if (result.worker_exit_observed && result.job_tree_empty && exit_state) {
      impl_->state = GpuAdmissionSessionState::WorkerExited;
    } else if (!result.worker_exit_observed || !result.job_tree_empty) {
      impl_->state = GpuAdmissionSessionState::Quarantined;
    }
    if (impl_->state == GpuAdmissionSessionState::Quarantined && impl_->lease) {
      impl_->lease->quarantine_for_process_lifetime();
    }
  }
  return result;
}

GpuAdmissionSessionState GpuAdmissionSession::finalize_cleanup(
    const DeviceCleanupEvidence& evidence, std::uint64_t now) {
  if (!impl_ || now == 0U) return GpuAdmissionSessionState::FailedClosed;
  SupervisorCleanupEvidence owned;
  {
    std::lock_guard lock(impl_->mutex);
    owned.worker_exit_observed = impl_->owned_worker_exit_observed;
    owned.job_tree_empty = impl_->owned_job_tree_empty;
    owned.job_accounting_reconciled =
        impl_->owned_job_accounting_reconciled;
    owned.artifact_handles_closed = impl_->owned_artifact_handles_closed;
    owned.temporary_files_reconciled =
        impl_->owned_temporary_files_reconciled;
  }
  owned.device_resources_reconciled = evidence.device_resources_reconciled;
  owned.terminal_reason = evidence.terminal_reason;
  owned.last_good_sample_sha256 = evidence.last_good_sample_sha256;
  owned.evidence_bundle_sha256 = evidence.evidence_bundle_sha256;
  if (!begin_cleanup(now)) return state();
  return cleanup_owned(owned, now);
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

GpuAdmissionSessionState GpuAdmissionSession::cleanup_owned(
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

EvidenceProviderFailStopReceipt record_evidence_provider_fail_stop(
    const NativeWorkerResult& supervisor_result,
    std::int32_t adapter_luid_high, std::uint32_t adapter_luid_low,
    std::uint64_t observed_elapsed_milliseconds,
    std::uint64_t maximum_elapsed_milliseconds) {
  EvidenceProviderFailStopReceipt receipt;
  receipt.reason = "evidence_provider_fail_stop_unverified";
  auto acquired =
      acquire_exclusive_gpu_lease(adapter_luid_high, adapter_luid_low);
  if (acquired.status != ExclusiveGpuLeaseStatus::Acquired ||
      !acquired.lease) {
    receipt.reason = "evidence_provider_fail_stop_quarantine_failed";
    return receipt;
  }
  acquired.lease->quarantine_for_process_lifetime();
  receipt.quarantined = true;
  receipt.retry_prohibited = true;
  const bool exact_outer_evidence =
      maximum_elapsed_milliseconds != 0U &&
      observed_elapsed_milliseconds <= maximum_elapsed_milliseconds &&
      !supervisor_result.ok && !supervisor_result.timed_out &&
      supervisor_result.exit_code == kEvidenceProviderFailStopExitCode &&
      supervisor_result.worker_exit_observed &&
      supervisor_result.job_tree_empty &&
      supervisor_result.job_accounting_reconciled &&
      supervisor_result.artifact_handles_closed &&
      supervisor_result.temporary_files_reconciled &&
      supervisor_result.output_path.empty();
  if (!exact_outer_evidence) return receipt;
  receipt.accepted = true;
  receipt.reason = "evidence_provider_timeout_fail_stop";
  return receipt;
}

}  // namespace prisminfer

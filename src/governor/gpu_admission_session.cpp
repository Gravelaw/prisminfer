#include "prisminfer/gpu_admission_session.h"

#include <cerrno>
#include <mutex>
#include <utility>

namespace prisminfer {

struct GpuAdmissionSession::Impl {
  AdmissionCellIdentity cell;
  std::int32_t adapter_luid_high{0};
  std::uint32_t adapter_luid_low{0};
  std::optional<ExclusiveGpuLease> lease;
  GpuAdmissionSessionState state{GpuAdmissionSessionState::LeaseHeld};
  std::optional<PreContextAdmissionReceipt> pre_receipt;
  std::optional<PostContextAdmissionReceipt> post_receipt;
  AdmissionTokenIssuer token_issuer;
  mutable std::mutex mutex;
};

GpuAdmissionSession::GpuAdmissionSession(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

GpuAdmissionSession::GpuAdmissionSession(GpuAdmissionSession&&) noexcept =
    default;
GpuAdmissionSession::~GpuAdmissionSession() = default;

GpuAdmissionSessionState GpuAdmissionSession::state() const {
  if (!impl_) return GpuAdmissionSessionState::FailedClosed;
  std::lock_guard lock(impl_->mutex);
  return impl_->state;
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
  if (impl_->state != GpuAdmissionSessionState::PreContextAdmitted ||
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
  auto issued = impl_->token_issuer.issue(*impl_->post_receipt, impl_->cell,
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
  auto consumed =
      impl_->token_issuer.consume(token, impl_->cell,
                                  now_monotonic_milliseconds);
  if (consumed.status != AdmissionTokenStatus::Consumed) {
    impl_->state = GpuAdmissionSessionState::FailedClosed;
    return consumed;
  }
  impl_->state = GpuAdmissionSessionState::TokenConsumed;
  return consumed;
}

GpuAdmissionSessionState GpuAdmissionSession::cleanup(
    bool resources_reconciled) {
  if (!impl_) return GpuAdmissionSessionState::FailedClosed;
  std::lock_guard lock(impl_->mutex);
  if (impl_->state == GpuAdmissionSessionState::Cleaned ||
      impl_->state == GpuAdmissionSessionState::Quarantined) {
    impl_->state = GpuAdmissionSessionState::Quarantined;
    return impl_->state;
  }
  impl_->pre_receipt.reset();
  impl_->post_receipt.reset();
  impl_->lease.reset();
  impl_->state = resources_reconciled ? GpuAdmissionSessionState::Cleaned
                                      : GpuAdmissionSessionState::Quarantined;
  return impl_->state;
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

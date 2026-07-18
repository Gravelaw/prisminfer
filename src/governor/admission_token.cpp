#include "prisminfer/admission_token.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <utility>

#include "prisminfer/checked_arithmetic.h"
#include "prisminfer/gpu_cap_policy.h"

namespace prisminfer {

namespace {

std::atomic<std::uint64_t> g_issuer_sequence{1};

std::uint64_t next_issuer_id() {
  auto current = g_issuer_sequence.load(std::memory_order_relaxed);
  for (;;) {
    if (current == 0 || current == std::numeric_limits<std::uint64_t>::max()) {
      return 0;
    }
    if (g_issuer_sequence.compare_exchange_weak(current, current + 1,
                                                std::memory_order_relaxed,
                                                std::memory_order_relaxed)) {
      return current;
    }
  }
}

}  // namespace

AdmissionToken::AdmissionToken(std::uint64_t issuer_id, std::uint64_t token_id,
                               std::uint64_t receipt_id,
                               AdmissionCellIdentity cell,
                               AdmissionWorkerIdentity worker,
                               std::uint64_t issued_monotonic_milliseconds,
                               std::uint64_t expires_monotonic_milliseconds,
                               std::uint64_t effective_cap_bytes)
    : issuer_id_(issuer_id),
      token_id_(token_id),
      receipt_id_(receipt_id),
      cell_(std::move(cell)),
      worker_(std::move(worker)),
      issued_monotonic_milliseconds_(issued_monotonic_milliseconds),
      expires_monotonic_milliseconds_(expires_monotonic_milliseconds),
      effective_cap_bytes_(effective_cap_bytes),
      active_(true) {}

AdmissionToken::AdmissionToken(AdmissionToken&& other) noexcept {
  *this = std::move(other);
}

AdmissionToken& AdmissionToken::operator=(AdmissionToken&& other) noexcept {
  if (this == &other) return *this;
  issuer_id_ = other.issuer_id_;
  token_id_ = other.token_id_;
  receipt_id_ = other.receipt_id_;
  cell_ = other.cell_;
  worker_ = other.worker_;
  issued_monotonic_milliseconds_ = other.issued_monotonic_milliseconds_;
  expires_monotonic_milliseconds_ = other.expires_monotonic_milliseconds_;
  effective_cap_bytes_ = other.effective_cap_bytes_;
  active_.store(other.active_.exchange(false, std::memory_order_acq_rel),
                std::memory_order_release);
  other.invalidate();
  return *this;
}

std::uint64_t AdmissionToken::token_id() const { return token_id_; }

std::uint64_t AdmissionToken::effective_cap_bytes() const {
  return effective_cap_bytes_;
}

bool AdmissionToken::active() const {
  return active_.load(std::memory_order_acquire);
}

void AdmissionToken::invalidate() {
  issuer_id_ = 0;
  token_id_ = 0;
  receipt_id_ = 0;
  cell_ = {};
  worker_ = {};
  issued_monotonic_milliseconds_ = 0;
  expires_monotonic_milliseconds_ = 0;
  effective_cap_bytes_ = 0;
  active_.store(false, std::memory_order_release);
}

AdmissionTokenIssuer::AdmissionTokenIssuer() : issuer_id_(next_issuer_id()) {}

AdmissionTokenIssueResult AdmissionTokenIssuer::issue(
    const PostContextAdmissionReceipt& receipt,
    const AdmissionCellIdentity& cell, const AdmissionWorkerIdentity& worker,
    std::uint64_t now_monotonic_milliseconds,
    std::uint64_t validity_milliseconds) {
  AdmissionTokenIssueResult result;
  std::lock_guard lock(mutex_);
  if (issuer_id_ == 0 || receipt.receipt_id() == 0 ||
      !validate_gpu_hard_cap(receipt.policy_ceiling_bytes()).accepted ||
      receipt.requested_tier_bytes() == 0 ||
      receipt.requested_tier_bytes() > receipt.policy_ceiling_bytes() ||
      receipt.effective_cap_bytes() == 0 ||
      receipt.effective_cap_bytes() > receipt.requested_tier_bytes() ||
      receipt.predicted_gpu_peak_bytes() > receipt.effective_cap_bytes()) {
    return result;
  }
  if (!admission_cell_identity_valid(cell) || receipt.cell() != cell) {
    result.status = AdmissionTokenStatus::InvalidCellIdentity;
    return result;
  }
  if (worker.root_process_id == 0 || worker.job_identity.empty() ||
      worker.executable_identity.empty()) {
    result.status = AdmissionTokenStatus::InvalidWorkerIdentity;
    return result;
  }
  if (now_monotonic_milliseconds == 0 ||
      now_monotonic_milliseconds <
          receipt.evaluation_monotonic_milliseconds() ||
      validity_milliseconds == 0 ||
      validity_milliseconds > receipt.timing().maximum_guard_age_milliseconds) {
    result.status = AdmissionTokenStatus::InvalidTime;
    return result;
  }
  const auto requested_expires =
      checked_add_u64(now_monotonic_milliseconds, validity_milliseconds);
  const auto receipt_expires =
      checked_add_u64(receipt.evaluation_monotonic_milliseconds(),
                      receipt.timing().maximum_guard_age_milliseconds);
  if (!requested_expires || !receipt_expires ||
      now_monotonic_milliseconds >= *receipt_expires) {
    result.status = AdmissionTokenStatus::InvalidTime;
    return result;
  }
  const auto expires = std::min(*requested_expires, *receipt_expires);
  if (std::find(used_receipt_ids_.begin(),
                used_receipt_ids_.begin() + used_receipt_count_,
                receipt.receipt_id()) !=
      used_receipt_ids_.begin() + used_receipt_count_) {
    result.status = AdmissionTokenStatus::ReceiptAlreadyUsed;
    return result;
  }
  if (used_receipt_count_ == used_receipt_ids_.size()) {
    result.status = AdmissionTokenStatus::IssuerCapacityExhausted;
    return result;
  }
  if (next_token_id_ == 0 ||
      next_token_id_ == std::numeric_limits<std::uint64_t>::max()) {
    result.status = AdmissionTokenStatus::TokenSequenceExhausted;
    return result;
  }

  if (!receipt.consume_for_token()) {
    result.status = AdmissionTokenStatus::ReceiptAlreadyUsed;
    return result;
  }

  const auto token_id = next_token_id_++;
  used_receipt_ids_[used_receipt_count_++] = receipt.receipt_id();
  result.token = AdmissionToken(issuer_id_, token_id, receipt.receipt_id(),
                                cell, worker, now_monotonic_milliseconds, expires,
                                receipt.effective_cap_bytes());
  result.status = AdmissionTokenStatus::Issued;
  return result;
}

AdmissionTokenConsumeResult AdmissionTokenIssuer::consume(
    AdmissionToken& token, const AdmissionCellIdentity& cell,
    const AdmissionWorkerIdentity& worker,
    std::uint64_t now_monotonic_milliseconds) {
  AdmissionTokenConsumeResult result;
  if (!token.active_.load(std::memory_order_acquire)) {
    result.status = AdmissionTokenStatus::TokenAlreadyConsumed;
    return result;
  }
  if (token.issuer_id_ != issuer_id_) {
    result.status = AdmissionTokenStatus::ForeignIssuer;
    return result;
  }
  AdmissionTokenStatus terminal_status = AdmissionTokenStatus::Consumed;
  if (!admission_cell_identity_valid(cell) || token.cell_ != cell) {
    terminal_status = AdmissionTokenStatus::TokenCellMismatch;
  } else if (worker.root_process_id == 0 || worker.job_identity.empty() ||
             worker.executable_identity.empty() ||
             token.worker_ != worker) {
    terminal_status = AdmissionTokenStatus::TokenWorkerMismatch;
  } else if (now_monotonic_milliseconds <
                 token.issued_monotonic_milliseconds_ ||
             now_monotonic_milliseconds >
                 token.expires_monotonic_milliseconds_) {
    terminal_status = AdmissionTokenStatus::TokenExpired;
  }
  bool expected = true;
  if (!token.active_.compare_exchange_strong(
          expected, false, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    result.status = AdmissionTokenStatus::TokenAlreadyConsumed;
    return result;
  }
  if (terminal_status == AdmissionTokenStatus::TokenCellMismatch) {
    result.status = AdmissionTokenStatus::TokenCellMismatch;
    return result;
  }
  if (terminal_status == AdmissionTokenStatus::TokenWorkerMismatch) {
    result.status = AdmissionTokenStatus::TokenWorkerMismatch;
    return result;
  }
  if (terminal_status == AdmissionTokenStatus::TokenExpired) {
    result.status = AdmissionTokenStatus::TokenExpired;
    return result;
  }

  result.effective_cap_bytes = token.effective_cap_bytes_;
  result.status = AdmissionTokenStatus::Consumed;
  return result;
}

}  // namespace prisminfer

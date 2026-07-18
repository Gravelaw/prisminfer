#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include "prisminfer/admission_cell_identity.h"
#include "prisminfer/supervisor_admission.h"

namespace prisminfer {

enum class AdmissionTokenStatus {
  Issued,
  Consumed,
  InvalidReceipt,
  InvalidCellIdentity,
  InvalidTime,
  ReceiptAlreadyUsed,
  IssuerCapacityExhausted,
  TokenSequenceExhausted,
  ForeignIssuer,
  TokenAlreadyConsumed,
  TokenCellMismatch,
  InvalidWorkerIdentity,
  TokenWorkerMismatch,
  TokenExpired,
  SupervisorStateInvalid,
};

struct AdmissionWorkerIdentity {
  std::uint32_t root_process_id{0};
  std::string job_identity;
  std::string executable_identity;

  bool operator==(const AdmissionWorkerIdentity&) const = default;
};

class AdmissionToken {
 public:
  AdmissionToken(const AdmissionToken&) = delete;
  AdmissionToken& operator=(const AdmissionToken&) = delete;
  AdmissionToken(AdmissionToken&& other) noexcept;
  AdmissionToken& operator=(AdmissionToken&& other) noexcept;

  [[nodiscard]] std::uint64_t token_id() const;
  [[nodiscard]] std::uint64_t effective_cap_bytes() const;
  [[nodiscard]] std::uint64_t expires_monotonic_milliseconds() const;
  [[nodiscard]] bool active() const;

 private:
  friend class AdmissionTokenIssuer;

  AdmissionToken(std::uint64_t issuer_id, std::uint64_t token_id,
                 std::uint64_t receipt_id, AdmissionCellIdentity cell,
                 AdmissionWorkerIdentity worker,
                 std::uint64_t issued_monotonic_milliseconds,
                 std::uint64_t expires_monotonic_milliseconds,
                 std::uint64_t effective_cap_bytes);
  void invalidate();

  std::uint64_t issuer_id_{0};
  std::uint64_t token_id_{0};
  std::uint64_t receipt_id_{0};
  AdmissionCellIdentity cell_;
  AdmissionWorkerIdentity worker_;
  std::uint64_t issued_monotonic_milliseconds_{0};
  std::uint64_t expires_monotonic_milliseconds_{0};
  std::uint64_t effective_cap_bytes_{0};
  std::atomic<bool> active_{false};
};

struct AdmissionTokenIssueResult {
  AdmissionTokenStatus status{AdmissionTokenStatus::InvalidReceipt};
  std::optional<AdmissionToken> token;
};

struct AdmissionTokenConsumeResult {
  AdmissionTokenStatus status{AdmissionTokenStatus::TokenAlreadyConsumed};
  std::uint64_t effective_cap_bytes{0};
};

class AdmissionTokenIssuer {
 public:
  static constexpr std::size_t kMaximumReceipts = 64;

  AdmissionTokenIssuer();
  AdmissionTokenIssuer(const AdmissionTokenIssuer&) = delete;
  AdmissionTokenIssuer& operator=(const AdmissionTokenIssuer&) = delete;

  [[nodiscard]] AdmissionTokenIssueResult issue(
      const PostContextAdmissionReceipt& receipt,
      const AdmissionCellIdentity& cell,
      const AdmissionWorkerIdentity& worker,
      std::uint64_t now_monotonic_milliseconds,
      std::uint64_t validity_milliseconds);
  [[nodiscard]] AdmissionTokenConsumeResult consume(
      AdmissionToken& token, const AdmissionCellIdentity& cell,
      const AdmissionWorkerIdentity& worker,
      std::uint64_t now_monotonic_milliseconds);

 private:
  std::uint64_t issuer_id_{0};
  std::uint64_t next_token_id_{1};
  std::array<std::uint64_t, kMaximumReceipts> used_receipt_ids_{};
  std::size_t used_receipt_count_{0};
  mutable std::mutex mutex_;
};

}  // namespace prisminfer

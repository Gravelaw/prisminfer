#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "prisminfer/admission_token.h"
#include "prisminfer/exclusive_gpu_lease.h"

namespace prisminfer {

enum class GpuAdmissionSessionState {
  LeaseHeld,
  PreContextAdmitted,
  PostContextAdmitted,
  TokenIssued,
  TokenConsumed,
  FailedClosed,
  Cleaned,
  Quarantined,
};

struct GpuAdmissionSessionAcquireResult;

class GpuAdmissionSession {
 public:
  GpuAdmissionSession(const GpuAdmissionSession&) = delete;
  GpuAdmissionSession& operator=(const GpuAdmissionSession&) = delete;
  GpuAdmissionSession(GpuAdmissionSession&&) noexcept;
  GpuAdmissionSession& operator=(GpuAdmissionSession&&) noexcept = delete;
  ~GpuAdmissionSession();

  [[nodiscard]] GpuAdmissionSessionState state() const;
  [[nodiscard]] std::string lease_id() const;
  [[nodiscard]] PreContextAdmissionDecision admit_pre_context(
      PreContextAdmissionRequest request);
  [[nodiscard]] PostContextAdmissionDecision admit_post_context(
      PostContextAdmissionRequest request);
  [[nodiscard]] AdmissionTokenIssueResult issue_token(
      std::uint64_t now_monotonic_milliseconds,
      std::uint64_t validity_milliseconds);
  [[nodiscard]] AdmissionTokenConsumeResult consume_token(
      AdmissionToken& token, std::uint64_t now_monotonic_milliseconds);
  [[nodiscard]] GpuAdmissionSessionState cleanup(bool resources_reconciled);

 private:
  struct Impl;
  friend struct GpuAdmissionSessionAcquireResult;
  friend GpuAdmissionSessionAcquireResult acquire_gpu_admission_session(
      const AdmissionCellIdentity& cell, std::int32_t adapter_luid_high,
      std::uint32_t adapter_luid_low);
  explicit GpuAdmissionSession(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

struct GpuAdmissionSessionAcquireResult {
  enum class Status { Acquired, InvalidCellIdentity, LeaseUnavailable };
  Status status{Status::LeaseUnavailable};
  ExclusiveGpuLeaseStatus lease_status{ExclusiveGpuLeaseStatus::SystemError};
  std::optional<GpuAdmissionSession> session;
  std::uint32_t system_error{0};
};

[[nodiscard]] GpuAdmissionSessionAcquireResult acquire_gpu_admission_session(
    const AdmissionCellIdentity& cell, std::int32_t adapter_luid_high,
    std::uint32_t adapter_luid_low);

}  // namespace prisminfer

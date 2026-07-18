#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "prisminfer/admission_token.h"
#include "prisminfer/exclusive_gpu_lease.h"
#include "prisminfer/supervisor_watchdog.h"

namespace prisminfer {

enum class GpuAdmissionSessionState {
  LeaseHeld,
  PreContextAdmitted,
  WorkerContained,
  PostContextAdmitted,
  TokenIssued,
  TokenConsumed,
  WatchdogActive,
  CancelRequested,
  CooperativeCancelAcknowledged,
  JobAbortRequired,
  WorkerExited,
  CleanupStarted,
  FailedClosed,
  Cleaned,
  Quarantined,
};

struct ContainedWorkerEvidence {
  bool created_suspended{false};
  bool job_assigned_before_resume{false};
  bool non_breakaway_job{false};
  bool kill_on_close{false};
  bool controlled_environment{false};
  bool controlled_inherited_handles{false};
  std::uint32_t root_process_id{0};
  std::uint32_t maximum_active_processes{0};
  std::uint64_t job_memory_limit_bytes{0};
  std::uint32_t job_cpu_time_limit_milliseconds{0};
  std::string job_identity;
  std::string executable_identity;
};

enum class CancellationAction {
  AwaitCooperativeExit,
  AbortJob,
  BeginCleanup,
  Quarantine,
};

struct CancellationDecision {
  CancellationAction action{CancellationAction::Quarantine};
  GpuAdmissionSessionState state{GpuAdmissionSessionState::FailedClosed};
};

struct SupervisorCleanupEvidence {
  bool worker_exit_observed{false};
  bool job_tree_empty{false};
  bool job_accounting_reconciled{false};
  bool device_resources_reconciled{false};
  bool artifact_handles_closed{false};
  bool temporary_files_reconciled{false};
  std::string terminal_reason;
  std::string last_good_sample_sha256;
  std::string evidence_bundle_sha256;
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
  [[nodiscard]] bool bind_contained_worker(
      const ContainedWorkerEvidence& evidence);
  [[nodiscard]] PostContextAdmissionDecision admit_post_context(
      PostContextAdmissionRequest request);
  [[nodiscard]] AdmissionTokenIssueResult issue_token(
      std::uint64_t now_monotonic_milliseconds,
      std::uint64_t validity_milliseconds);
  [[nodiscard]] AdmissionTokenConsumeResult consume_token(
      AdmissionToken& token, std::uint64_t now_monotonic_milliseconds);
  [[nodiscard]] SupervisorWatchdogDecision evaluate_watchdog(
      const SupervisorWatchdogSample& sample);
  [[nodiscard]] CancellationDecision advance_cancellation(
      std::uint64_t now_monotonic_milliseconds,
      bool cooperative_cancel_acknowledged, bool worker_exited,
      bool job_tree_empty);
  [[nodiscard]] CancellationDecision record_job_abort(
      std::uint64_t now_monotonic_milliseconds, bool job_tree_empty);
  [[nodiscard]] bool record_normal_worker_exit(
      std::uint64_t now_monotonic_milliseconds, bool job_tree_empty);
  [[nodiscard]] bool begin_cleanup(
      std::uint64_t now_monotonic_milliseconds);
  [[nodiscard]] GpuAdmissionSessionState cleanup(
      const SupervisorCleanupEvidence& evidence,
      std::uint64_t now_monotonic_milliseconds);
  [[nodiscard]] std::optional<SupervisorCleanupEvidence> terminal_evidence()
      const;

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

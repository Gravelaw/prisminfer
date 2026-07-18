#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

#include "prisminfer/admission_token.h"
#include "prisminfer/exclusive_gpu_lease.h"
#include "prisminfer/native_worker.h"
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

struct DeviceCleanupEvidence {
  bool device_resources_reconciled{false};
  std::string terminal_reason;
  std::string last_good_sample_sha256;
  std::string evidence_bundle_sha256;
};

// Evidence producers are supervisor-owned and must observe the stop token.
// The session requests stop on deadline and joins every producer before
// returning, so producer activity and captured state cannot escape cleanup.
using PostContextEvidenceProducer = std::function<PostContextAdmissionRequest(
    std::stop_token stop, std::uint32_t worker_process_id,
    std::uint64_t context_ready_monotonic_milliseconds)>;
using WatchdogEvidenceProducer = std::function<SupervisorWatchdogSample(
    std::stop_token stop, std::uint32_t worker_process_id,
    std::uint64_t heartbeat_sequence,
    std::uint64_t heartbeat_monotonic_milliseconds)>;
using ProcessDeviceEvidenceProducer = std::function<ProcessDeviceMemorySample(
    std::stop_token stop, std::uint32_t worker_process_id,
    std::int32_t adapter_luid_high, std::uint32_t adapter_luid_low)>;

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
  // This is the only production worker entry point. The session invokes the
  // secure launcher itself and therefore owns the live process, Job,
  // bidirectional control channel, cancellation, and fail-closed teardown.
  [[nodiscard]] NativeWorkerResult run_contained_worker(
      const NativeWorkerTrustCatalog& catalog,
      const NativeWorkerRequest& request,
      std::uint64_t token_validity_milliseconds,
      PostContextEvidenceProducer post_context_evidence,
      WatchdogEvidenceProducer watchdog_evidence,
      ProcessDeviceEvidenceProducer process_device_evidence = {});
  [[nodiscard]] GpuAdmissionSessionState finalize_cleanup(
      const DeviceCleanupEvidence& evidence,
      std::uint64_t now_monotonic_milliseconds);
  [[nodiscard]] std::optional<SupervisorCleanupEvidence> terminal_evidence()
      const;

 private:
  [[nodiscard]] PostContextAdmissionDecision admit_post_context(
      PostContextAdmissionRequest request);
  [[nodiscard]] AdmissionTokenIssueResult issue_token(
      std::uint64_t now_monotonic_milliseconds,
      std::uint64_t validity_milliseconds);
  [[nodiscard]] AdmissionTokenConsumeResult consume_token(
      AdmissionToken& token, std::uint64_t now_monotonic_milliseconds);
  [[nodiscard]] SupervisorWatchdogDecision evaluate_watchdog(
      const SupervisorWatchdogSample& sample);
  [[nodiscard]] bool bind_owned_worker(
      const NativeWorkerContainmentIdentity& identity);
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
  [[nodiscard]] GpuAdmissionSessionState cleanup_owned(
      const SupervisorCleanupEvidence& evidence,
      std::uint64_t now_monotonic_milliseconds);
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

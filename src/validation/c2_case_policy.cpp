#include "prisminfer/c2_case_policy.h"

namespace prisminfer {

bool c2_case_result_matches_contract(
    const std::string& case_name, const NativeWorkerResult& result,
    GpuAdmissionSessionState cleanup, bool output_removed) {
  if (!output_removed || !result.artifact_handles_closed ||
      !result.temporary_files_reconciled ||
      !result.job_accounting_reconciled) {
    return false;
  }
  if (case_name == "success") {
    return result.ok && cleanup == GpuAdmissionSessionState::Cleaned;
  }
  if (case_name == "watchdog-cancel") {
    return !result.ok &&
           result.failure_reason.find("watchdog_rejected") != std::string::npos &&
           cleanup == GpuAdmissionSessionState::Cleaned;
  }
  if (case_name == "heartbeat-loss") {
    return !result.ok &&
           result.failure_reason.find("heartbeat_timeout") != std::string::npos &&
           cleanup == GpuAdmissionSessionState::Cleaned;
  }
  if (case_name == "post-context-telemetry-loss") {
    return !result.ok &&
           result.failure_reason.find("admission_rejected") != std::string::npos &&
           cleanup == GpuAdmissionSessionState::Quarantined;
  }
  return false;
}

}  // namespace prisminfer

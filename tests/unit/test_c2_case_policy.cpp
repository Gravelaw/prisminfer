#include <iostream>
#include <string>

#include "prisminfer/c2_case_policy.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

prisminfer::NativeWorkerResult contained_result(std::string reason = {}) {
  prisminfer::NativeWorkerResult result;
  result.ok = reason.empty();
  result.failure_reason = std::move(reason);
  result.worker_exit_observed = true;
  result.job_tree_empty = true;
  result.job_accounting_reconciled = true;
  result.artifact_handles_closed = true;
  result.temporary_files_reconciled = true;
  return result;
}

}  // namespace

int main() {
  using State = prisminfer::GpuAdmissionSessionState;
  if (expect(prisminfer::c2_case_result_matches_contract(
                 "success", contained_result(), State::Cleaned, true),
             "success requires cleaned contained result") ||
      expect(prisminfer::c2_case_result_matches_contract(
                 "watchdog-cancel",
                 contained_result("native_worker_protocol_watchdog_rejected"),
                 State::Cleaned, true),
             "watchdog cancellation requires reconciled cleanup") ||
      expect(prisminfer::c2_case_result_matches_contract(
                 "heartbeat-loss",
                 contained_result("native_worker_protocol_heartbeat_timeout"),
                 State::Cleaned, true),
             "heartbeat loss requires reconciled cleanup") ||
      expect(prisminfer::c2_case_result_matches_contract(
                 "post-context-telemetry-loss",
                 contained_result("native_worker_protocol_admission_rejected:telemetry"),
                 State::Quarantined, true),
             "telemetry loss has the typed fail-closed result")) {
    return 1;
  }
  auto missing_accounting =
      contained_result("native_worker_protocol_watchdog_rejected");
  missing_accounting.job_accounting_reconciled = false;
  if (expect(!prisminfer::c2_case_result_matches_contract(
                 "watchdog-cancel", missing_accounting, State::Cleaned, true),
             "missing containment evidence always rejects")) {
    return 1;
  }
  prisminfer::ProcessDeviceMemorySample wddm;
  wddm.available = true;
  wddm.source = "wddm-process";
  const auto accepted_wddm =
      prisminfer::require_c2_wddm_process_sample(wddm);
  auto nvml = wddm;
  nvml.source = "nvml-process";
  const auto rejected_nvml =
      prisminfer::require_c2_wddm_process_sample(nvml);
  return expect(accepted_wddm.available && !rejected_nvml.available &&
                    rejected_nvml.unavailable_reason ==
                        "c2_requires_wddm_process_source:nvml-process",
                "C2 accepts only the native WDDM process source");
}

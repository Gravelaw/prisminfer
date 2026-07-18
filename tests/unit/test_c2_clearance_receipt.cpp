#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "prisminfer/c2_clearance_receipt.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

prisminfer::C2ClearanceReceipt valid_receipt() {
  prisminfer::C2ClearanceReceipt receipt;
  receipt.repository = "Gravelaw/prisminfer";
  receipt.reviewed_sha = std::string(40U, '1');
  receipt.source_tree_sha = std::string(40U, '2');
  receipt.worker_sha256 = std::string(64U, '3');
  receipt.worker_approval_identity = "c2-synthetic-worker-v1";
  receipt.workflow_run_id = "unit-test";
  receipt.authorization_id = "C2-AUTH-unit-test";
  receipt.gpu_uuid = "GPU-11111111-2222-3333-4444-555555555555";
  receipt.case_name = "success";
  receipt.status = "candidate-complete";
  receipt.cleanup_status = "cleaned";
  receipt.lease_id = "prisminfer-gpu-00000001-00000002";
  receipt.job_identity = "job:1:2:3";
  receipt.last_good_sample_sha256 = std::string(64U, '4');
  receipt.evidence_bundle_sha256 = std::string(64U, '5');
  receipt.adapter_luid_high = 1;
  receipt.adapter_luid_low = 2U;
  receipt.worker_timeout_milliseconds = 10'000U;
  receipt.post_admission_payload_bytes = 64ULL * 1024ULL * 1024ULL;
  receipt.context_cuda_free_bytes = 8ULL << 30U;
  receipt.context_cuda_total_bytes = 16ULL << 30U;
  receipt.last_heartbeat_cuda_free_bytes =
      (8ULL << 30U) - receipt.post_admission_payload_bytes;
  receipt.last_heartbeat_cuda_total_bytes = 16ULL << 30U;
  receipt.pre_wddm_local_usage_bytes = 256ULL << 20U;
  receipt.final_wddm_local_usage_bytes = 256ULL << 20U;
  receipt.cleanup_wddm_positive_delta_bytes = 0U;
  receipt.pre_host_memory_available_bytes = 16ULL << 30U;
  receipt.final_host_memory_available_bytes = 15ULL << 30U;
  receipt.pre_host_commit_available_bytes = 24ULL << 30U;
  receipt.final_host_commit_available_bytes = 23ULL << 30U;
  receipt.pre_gpu_temperature_celsius = 45;
  receipt.final_gpu_temperature_celsius = 46;
  receipt.root_process_id = 7U;
  receipt.heartbeat_count = 2U;
  receipt.context_ready_observed = true;
  receipt.token_consumed_observed = true;
  receipt.worker_exit_observed = true;
  receipt.job_tree_empty = true;
  receipt.job_accounting_reconciled = true;
  receipt.device_resources_reconciled = true;
  receipt.artifact_handles_closed = true;
  receipt.temporary_files_reconciled = true;
  return receipt;
}

void write_text(const std::filesystem::path& path, const std::string& value) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << value;
}

}  // namespace

int main() {
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root = std::filesystem::temp_directory_path() /
                    ("prisminfer-c2-receipt-test-" + std::to_string(nonce));
  std::filesystem::create_directory(root);
  const auto cleanup = [&] {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  };

  const auto path = root / "receipt.json";
  std::string error;
  const auto receipt = valid_receipt();
  if (expect(prisminfer::write_c2_clearance_receipt(root, path, receipt, &error),
             "valid receipt publishes") ||
      expect(prisminfer::validate_c2_clearance_receipt_file(path, &error),
             "published receipt validates") ||
      expect(prisminfer::write_c2_clearance_receipt(root, path, receipt, &error),
             "same receipt is idempotent")) {
    cleanup();
    return 1;
  }

  auto oversized = receipt;
  oversized.post_admission_payload_bytes =
      prisminfer::kC2MaximumPayloadBytes + 1U;
  const auto oversized_path = root / "oversized.json";
  if (expect(!prisminfer::write_c2_clearance_receipt(
                 root, oversized_path, oversized, &error),
             "payload above 64 MiB rejects")) {
    cleanup();
    return 1;
  }

  auto leaked = receipt;
  leaked.cleanup_wddm_positive_delta_bytes =
      leaked.cleanup_wddm_tolerance_bytes + 1U;
  const auto leaked_path = root / "leaked.json";
  if (expect(!prisminfer::write_c2_clearance_receipt(
                 root, leaked_path, leaked, &error),
             "cleanup WDDM delta above tolerance rejects")) {
    cleanup();
    return 1;
  }

  auto tampered = prisminfer::serialize_c2_clearance_receipt(receipt);
  const auto classification = tampered.find("\"promotable\":false");
  tampered.replace(classification, std::string("\"promotable\":false").size(),
                   "\"promotable\":true");
  const auto tampered_path = root / "tampered.json";
  write_text(tampered_path, tampered);
  if (expect(!prisminfer::validate_c2_clearance_receipt_file(tampered_path,
                                                             &error),
             "promotable tampering rejects")) {
    cleanup();
    return 1;
  }

  auto unknown = prisminfer::serialize_c2_clearance_receipt(receipt);
  unknown.replace(unknown.rfind("\n}"), 2U, ",\n  \"unexpected\":1\n}");
  const auto unknown_path = root / "unknown.json";
  write_text(unknown_path, unknown);
  if (expect(!prisminfer::validate_c2_clearance_receipt_file(unknown_path,
                                                             &error),
             "unknown field rejects")) {
    cleanup();
    return 1;
  }

  auto escaped = root / "nested" / "receipt.json";
  std::filesystem::create_directory(root / "nested");
  if (expect(!prisminfer::write_c2_clearance_receipt(root, escaped, receipt,
                                                     &error),
             "nested output escapes direct-child policy")) {
    cleanup();
    return 1;
  }
  cleanup();
  return 0;
}

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "prisminfer/kernel_benchmark_manifest.h"
#include "prisminfer/sha256.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

std::string valid_manifest() {
  return R"json({
  "manifest_version": "0.1",
  "validation_cell_id": "phase6-cell",
  "model_hash": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  "quantization_format": "Q4_K_M",
  "quant_artifact_sha256": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
  "context_tokens": 2048,
  "batch_size": 1,
  "prompt_fixture_hash": "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
  "backend": "llama.cpp",
  "os": "windows",
  "gpu_name": "gpu",
  "driver_mode": "wddm",
  "cuda_driver_version": 13030,
  "cuda_runtime_version": 13030,
  "vram_tier_gib": 8,
  "hard_cap_bytes": 8589934592,
  "op_type": "matmul",
  "sequence_phase": "decode",
  "kernel_backend": "ggml-cuda-mmq",
  "kernel_name": "baseline",
  "kernel_version": "1",
  "baseline_backend": "llama.cpp",
  "baseline_manifest_hash": "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
  "correctness_fixture_hash": "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
  "quality_fixture_hash": "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
  "full_dequant_materialized": false,
  "workspace_peak_bytes": 1024,
  "device_resident_bytes": 0,
  "host_commit_peak_bytes": 0,
  "unknown_owned_bytes": 0,
  "ttft_ms": 0.0,
  "prefill_ms": 0.0,
  "decode_tokens_per_second": 0.0,
  "request_tail_ms": 0.0,
  "speedup_ratio": 1.0,
  "compression_status": "none",
  "quality_gate_id": "phase6-retrieval-gate",
  "cap_certification_status": "research-only",
  "run_outcome": "completed",
  "supervisor_status": "not-attempted",
  "admission_status": "not-attempted",
  "requested_execution_path": "upstream-baseline",
  "actual_execution_path": "upstream-baseline",
  "raw_trial_count": 3,
  "raw_trial_sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  "failure_record_sha256": "",
  "claim_status": "research-only"
})json";
}

std::filesystem::path write_manifest(const std::string& name,
                                     const std::string& content) {
  const auto path = std::filesystem::temp_directory_path() / name;
  std::ofstream out(path, std::ios::out | std::ios::trunc);
  out << content;
  return path;
}

std::string replace_once(std::string value,
                         const std::string& needle,
                         const std::string& replacement) {
  const auto position = value.find(needle);
  if (position != std::string::npos) {
    value.replace(position, needle.size(), replacement);
  }
  return value;
}

}  // namespace

int main() {
  std::error_code remove_error;
  const auto sha_path =
      std::filesystem::temp_directory_path() / "prisminfer-sha256.txt";
  {
    std::ofstream sha_input(sha_path, std::ios::out | std::ios::trunc);
    sha_input << "abc";
  }
  std::string digest;
  std::string digest_error;
  if (expect(prisminfer::sha256_file(sha_path, &digest, &digest_error),
             digest_error.c_str())) {
    return 1;
  }
  if (expect(digest ==
                 "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
             "sha256 known vector")) {
    return 1;
  }
  std::filesystem::remove(sha_path, remove_error);
  const auto valid_path = write_manifest("prisminfer-kernel-valid.json",
                                         valid_manifest());
  const auto parsed = prisminfer::read_kernel_benchmark_manifest(valid_path);
  if (expect(parsed.ok, parsed.error.c_str())) return 1;
  if (expect(parsed.manifest.cell.model_hash ==
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
             "model hash parsed")) return 1;
  if (expect(parsed.manifest.cell.hard_cap_bytes == 8589934592ULL,
             "hard cap parsed")) return 1;
  if (expect(parsed.manifest.raw_trial_count == 3,
             "raw trial count parsed")) return 1;
  if (expect(parsed.manifest.raw_trial_sha256 ==
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
             "raw trial hash parsed")) return 1;
  if (expect(parsed.manifest.compression_status == "none",
             "compression status parsed")) return 1;
  if (expect(parsed.manifest.unknown_owned_bytes == 0 &&
                 parsed.manifest.ttft_ms == 0.0 &&
                 parsed.manifest.supervisor_status == "not-attempted" &&
                 parsed.manifest.admission_status == "not-attempted",
             "required runner evidence parsed")) return 1;
  const auto emitted_path = std::filesystem::temp_directory_path() /
                            "prisminfer-kernel-emitted.json";
  std::filesystem::remove(emitted_path, remove_error);
  std::string write_error;
  if (expect(prisminfer::write_kernel_benchmark_manifest(
                 emitted_path, parsed.manifest, &write_error),
             write_error.c_str())) {
    return 1;
  }
  const auto emitted =
      prisminfer::read_kernel_benchmark_manifest(emitted_path);
  if (expect(emitted.ok, emitted.error.c_str())) return 1;
  if (expect(emitted.manifest.cell.model_hash ==
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
             "emitted manifest preserves identity")) {
    return 1;
  }
  std::filesystem::remove(emitted_path, remove_error);
  std::filesystem::remove(valid_path, remove_error);

  const auto malformed_hash_path = write_manifest(
      "prisminfer-kernel-malformed-raw-hash.json",
      replace_once(valid_manifest(), "\"raw_trial_sha256\": "
                   "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"",
                   "\"raw_trial_sha256\": \"not-a-hash\""));
  const auto malformed_hash =
      prisminfer::read_kernel_benchmark_manifest(malformed_hash_path);
  if (expect(!malformed_hash.ok, "malformed raw hash rejected")) return 1;
  std::filesystem::remove(malformed_hash_path, remove_error);

  const auto malformed_identity_path = write_manifest(
      "prisminfer-kernel-malformed-identity.json",
      replace_once(
          valid_manifest(),
          "\"quant_artifact_sha256\": \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"",
          "\"quant_artifact_sha256\": \"not-a-hash\""));
  const auto malformed_identity =
      prisminfer::read_kernel_benchmark_manifest(malformed_identity_path);
  if (expect(!malformed_identity.ok, "malformed identity hash rejected")) {
    return 1;
  }
  if (expect(malformed_identity.error == "invalid_field:identity_sha256",
             "malformed identity hash reason")) return 1;
  std::filesystem::remove(malformed_identity_path, remove_error);

  const auto missing_evidence_path = write_manifest(
      "prisminfer-kernel-missing-evidence.json",
      replace_once(valid_manifest(), "  \"host_commit_peak_bytes\": 0,\n", ""));
  const auto missing_evidence =
      prisminfer::read_kernel_benchmark_manifest(missing_evidence_path);
  if (expect(!missing_evidence.ok, "missing runner evidence rejected")) return 1;
  if (expect(missing_evidence.error ==
                 "missing_required_field:host_commit_peak_bytes",
             "missing runner evidence reason")) return 1;
  std::filesystem::remove(missing_evidence_path, remove_error);

  const auto unknown_owned_path = write_manifest(
      "prisminfer-kernel-unknown-owned.json",
      replace_once(valid_manifest(), "\"unknown_owned_bytes\": 0",
                   "\"unknown_owned_bytes\": 1"));
  const auto unknown_owned =
      prisminfer::read_kernel_benchmark_manifest(unknown_owned_path);
  if (expect(!unknown_owned.ok, "nonzero unknown owned bytes rejected")) return 1;
  if (expect(unknown_owned.error == "manifest_schema_constraint_failed",
             "unknown owned bytes reason")) return 1;
  std::filesystem::remove(unknown_owned_path, remove_error);

  const auto promoted_without_supervision_path = write_manifest(
      "prisminfer-kernel-promoted-without-supervision.json",
      replace_once(valid_manifest(),
                   "\"cap_certification_status\": \"research-only\"",
                   "\"cap_certification_status\": \"validated-benchmark\""));
  const auto promoted_without_supervision =
      prisminfer::read_kernel_benchmark_manifest(
          promoted_without_supervision_path);
  if (expect(!promoted_without_supervision.ok,
             "promoted cap state requires active supervision and admission")) {
    return 1;
  }
  if (expect(promoted_without_supervision.error ==
                 "manifest_schema_constraint_failed",
             "promoted cap supervision reason")) {
    return 1;
  }
  std::filesystem::remove(promoted_without_supervision_path, remove_error);

  const auto negative_timing_path = write_manifest(
      "prisminfer-kernel-negative-timing.json",
      replace_once(valid_manifest(), "\"ttft_ms\": 0.0", "\"ttft_ms\": -1.0"));
  const auto negative_timing =
      prisminfer::read_kernel_benchmark_manifest(negative_timing_path);
  if (expect(!negative_timing.ok, "negative timing rejected")) return 1;
  if (expect(negative_timing.error == "invalid_field:run_timing",
             "negative timing reason")) return 1;
  std::filesystem::remove(negative_timing_path, remove_error);

  const auto incomplete_path = write_manifest(
      "prisminfer-kernel-incomplete-trials.json",
      replace_once(valid_manifest(), "  \"raw_trial_count\": 3,\n", ""));
  const auto incomplete =
      prisminfer::read_kernel_benchmark_manifest(incomplete_path);
  if (expect(!incomplete.ok, "completed manifest requires raw trials")) return 1;
  if (expect(incomplete.error == "completed_raw_evidence_required",
             "missing raw trial reason")) return 1;
  std::filesystem::remove(incomplete_path, remove_error);

  std::string aborted = valid_manifest();
  aborted = replace_once(aborted, "\"run_outcome\": \"completed\",",
                         "\"run_outcome\": \"aborted\",\n"
                         "  \"failure_reason\": \"watchdog_timeout\",");
  aborted = replace_once(
      aborted, "\"failure_record_sha256\": \"\"",
      "\"failure_record_sha256\": "
      "\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"");
  aborted = replace_once(aborted, "  \"raw_trial_count\": 3,\n", "");
  aborted = replace_once(
      aborted,
      "  \"raw_trial_sha256\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\n",
      "");
  aborted = replace_once(aborted,
                         "\"cap_certification_status\": \"research-only\"",
                         "\"cap_certification_status\": \"rejected\"");
  aborted = replace_once(aborted, "\"claim_status\": \"research-only\"",
                         "\"claim_status\": \"rejected\"");
  const auto aborted_path =
      write_manifest("prisminfer-kernel-aborted.json", aborted);
  const auto aborted_result =
      prisminfer::read_kernel_benchmark_manifest(aborted_path);
  if (expect(aborted_result.ok, aborted_result.error.c_str())) return 1;
  if (expect(aborted_result.manifest.run_outcome == "aborted",
             "aborted outcome parsed")) return 1;
  std::filesystem::remove(aborted_path, remove_error);

  const auto aborted_with_trials_path = write_manifest(
      "prisminfer-kernel-aborted-with-trials.json",
      replace_once(
          aborted, "  \"failure_reason\": \"watchdog_timeout\",\n",
          "  \"failure_reason\": \"watchdog_timeout\",\n"
          "  \"raw_trial_count\": 1,\n"
          "  \"raw_trial_sha256\": "
          "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\n"));
  const auto aborted_with_trials =
      prisminfer::read_kernel_benchmark_manifest(aborted_with_trials_path);
  if (expect(!aborted_with_trials.ok,
             "aborted evidence cannot retain promotable raw trials")) return 1;
  if (expect(aborted_with_trials.error == "failure_evidence_required",
             "aborted raw trial rejection reason")) return 1;
  std::filesystem::remove(aborted_with_trials_path, remove_error);

  const auto mismatched_promoted_path = write_manifest(
      "prisminfer-kernel-mismatched-promoted.json",
      replace_once(
          replace_once(valid_manifest(),
                       "\"actual_execution_path\": \"upstream-baseline\"",
                       "\"actual_execution_path\": \"fallback\""),
          "\"claim_status\": \"research-only\"",
          "\"claim_status\": \"measured\""));
  const auto mismatched_promoted =
      prisminfer::read_kernel_benchmark_manifest(mismatched_promoted_path);
  if (expect(!mismatched_promoted.ok,
             "requested and actual path mismatch cannot be promoted")) return 1;
  if (expect(mismatched_promoted.error == "manifest_schema_constraint_failed",
             "promoted path mismatch reason")) return 1;
  std::filesystem::remove(mismatched_promoted_path, remove_error);

  const auto missing_path = write_manifest(
      "prisminfer-kernel-missing.json",
      replace_once(
          valid_manifest(),
          "  \"model_hash\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\n",
          ""));
  const auto missing = prisminfer::read_kernel_benchmark_manifest(missing_path);
  if (expect(!missing.ok, "missing required field rejected")) return 1;
  if (expect(missing.error == "missing_required_field:model_hash",
             "missing field reason")) return 1;
  std::filesystem::remove(missing_path, remove_error);

  const auto unknown_path = write_manifest(
      "prisminfer-kernel-unknown.json",
      replace_once(valid_manifest(), "  \"claim_status\"",
                   "  \"surprise\": true,\n  \"claim_status\""));
  const auto unknown = prisminfer::read_kernel_benchmark_manifest(unknown_path);
  if (expect(!unknown.ok, "unknown field rejected")) return 1;
  if (expect(unknown.error == "unknown_field:surprise",
             "unknown field reason")) return 1;
  std::filesystem::remove(unknown_path, remove_error);

  const auto invalid_path = write_manifest(
      "prisminfer-kernel-invalid.json",
      replace_once(valid_manifest(), "8589934592", "17179869185"));
  const auto invalid = prisminfer::read_kernel_benchmark_manifest(invalid_path);
  if (expect(!invalid.ok, "cap above project maximum rejected")) return 1;
  if (expect(invalid.error == "manifest_schema_constraint_failed",
             "invalid hard cap reason")) return 1;
  std::filesystem::remove(invalid_path, remove_error);

  const auto compression_path = write_manifest(
      "prisminfer-kernel-compression-missing.json",
      replace_once(valid_manifest(), "\"compression_status\": \"none\"",
                   "\"compression_status\": \"reference\""));
  const auto compression =
      prisminfer::read_kernel_benchmark_manifest(compression_path);
  if (expect(!compression.ok, "compression evidence required")) return 1;
  if (expect(compression.error == "compression_evidence_required",
             "compression evidence reason")) return 1;
  std::filesystem::remove(compression_path, remove_error);

  const auto empty_path = write_manifest(
      "prisminfer-kernel-empty.json",
      replace_once(
          valid_manifest(),
          "\"model_hash\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"",
                   "\"model_hash\": \"\""));
  const auto empty = prisminfer::read_kernel_benchmark_manifest(empty_path);
  if (expect(!empty.ok, "empty required string rejected")) return 1;
  if (expect(empty.error == "invalid_field:model_hash",
             "empty field reason")) return 1;
  std::filesystem::remove(empty_path, remove_error);

  const auto enum_path = write_manifest(
      "prisminfer-kernel-enum.json",
      replace_once(valid_manifest(), "\"driver_mode\": \"wddm\"",
                   "\"driver_mode\": \"bogus\""));
  const auto enum_result = prisminfer::read_kernel_benchmark_manifest(enum_path);
  if (expect(!enum_result.ok, "invalid enum rejected")) return 1;
  if (expect(enum_result.error == "manifest_schema_constraint_failed",
             "invalid enum reason")) return 1;
  std::filesystem::remove(enum_path, remove_error);

  const auto optional_type_path = write_manifest(
      "prisminfer-kernel-optional-type.json",
      replace_once(valid_manifest(), "  \"workspace_peak_bytes\":",
                   "  \"dequant_fused\": \"not-a-bool\",\n"
                   "  \"workspace_peak_bytes\":"));
  const auto optional_type =
      prisminfer::read_kernel_benchmark_manifest(optional_type_path);
  if (expect(!optional_type.ok, "optional schema type rejected")) return 1;
  if (expect(optional_type.error == "invalid_field:dequant_fused",
             "optional type reason")) return 1;
  std::filesystem::remove(optional_type_path, remove_error);

  const auto duplicate_path = write_manifest(
      "prisminfer-kernel-duplicate.json",
      replace_once(valid_manifest(), "  \"model_hash\":",
                   "  \"model_hash\": \"first\",\n  \"model_hash\":"));
  const auto duplicate =
      prisminfer::read_kernel_benchmark_manifest(duplicate_path);
  if (expect(!duplicate.ok, "duplicate field rejected")) return 1;
  if (expect(duplicate.error == "duplicate_field:model_hash",
             "duplicate field reason")) return 1;
  std::filesystem::remove(duplicate_path, remove_error);

  const auto directory = prisminfer::read_kernel_benchmark_manifest(
      std::filesystem::temp_directory_path());
  if (expect(!directory.ok, "directory manifest rejected")) return 1;
  if (expect(directory.error == "manifest_not_regular_file",
             "directory rejection reason")) return 1;

  const auto large_path = std::filesystem::temp_directory_path() /
                          "prisminfer-kernel-large.json";
  {
    std::ofstream large(large_path, std::ios::out | std::ios::trunc);
    large << "{\"manifest_version\":\"0.1\",\"padding\":\"";
    large << std::string(70 * 1024, 'x');
    large << "\"}";
  }
  const auto large = prisminfer::read_kernel_benchmark_manifest(large_path);
  if (expect(!large.ok, "oversized manifest rejected")) return 1;
  if (expect(large.error == "manifest_size_exceeds_limit",
             "oversized manifest reason")) return 1;
  std::filesystem::remove(large_path, remove_error);

  std::string compressed = valid_manifest();
  compressed = replace_once(compressed, "\"compression_status\": \"none\"",
                            "\"compression_status\": \"reference\"");
  compressed = replace_once(
      compressed, "  \"quality_gate_id\":",
      "  \"effective_bits_per_value\": 4.25,\n"
      "  \"metadata_bits_per_value\": 0.25,\n"
      "  \"kv_payload_bytes\": 4096,\n"
      "  \"kv_metadata_bytes\": 128,\n"
      "  \"compression_decode_overhead_ms\": 0.5,\n"
      "  \"quality_result_path\": \"quality.json\",\n"
      "  \"quality_gate_id\":");
  const auto compressed_path = write_manifest(
      "prisminfer-kernel-compression-valid.json", compressed);
  const auto compressed_result =
      prisminfer::read_kernel_benchmark_manifest(compressed_path);
  if (expect(compressed_result.ok, compressed_result.error.c_str())) return 1;
  if (expect(compressed_result.manifest.kv_metadata_bytes == 128,
             "compression metadata bytes parsed")) return 1;
  std::filesystem::remove(compressed_path, remove_error);

  const auto accounting_only_path = write_manifest(
      "prisminfer-kernel-accounting-only.json",
      replace_once(valid_manifest(), "\"compression_status\": \"none\"",
                   "\"compression_status\": \"accounting-only\""));
  const auto accounting_only =
      prisminfer::read_kernel_benchmark_manifest(accounting_only_path);
  if (expect(accounting_only.ok, accounting_only.error.c_str())) return 1;
  std::filesystem::remove(accounting_only_path, remove_error);

  return 0;
}

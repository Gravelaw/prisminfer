#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "prisminfer/kernel_benchmark_manifest.h"

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
  "model_hash": "model",
  "quantization_format": "Q4_K_M",
  "quant_artifact_sha256": "quant",
  "context_tokens": 2048,
  "batch_size": 1,
  "prompt_fixture_hash": "prompt",
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
  "baseline_manifest_hash": "baseline",
  "correctness_fixture_hash": "correctness",
  "quality_fixture_hash": "quality",
  "full_dequant_materialized": false,
  "workspace_peak_bytes": 1024,
  "speedup_ratio": 1.0,
  "claim_status": "research-only",
  "compression_status": "none",
  "quality_gate_id": "phase6-retrieval-gate",
  "cap_certification_status": "research-only"
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
  const auto valid_path = write_manifest("prisminfer-kernel-valid.json",
                                         valid_manifest());
  const auto parsed = prisminfer::read_kernel_benchmark_manifest(valid_path);
  if (expect(parsed.ok, parsed.error.c_str())) return 1;
  if (expect(parsed.manifest.cell.model_hash == "model",
             "model hash parsed")) return 1;
  if (expect(parsed.manifest.cell.hard_cap_bytes == 8589934592ULL,
             "hard cap parsed")) return 1;
  if (expect(parsed.manifest.compression_status == "none",
             "compression status parsed")) return 1;
  std::filesystem::remove(valid_path, remove_error);

  const auto missing_path = write_manifest(
      "prisminfer-kernel-missing.json",
      replace_once(valid_manifest(), "  \"model_hash\": \"model\",\n", ""));
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
      replace_once(valid_manifest(), "\"model_hash\": \"model\"",
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

  return 0;
}

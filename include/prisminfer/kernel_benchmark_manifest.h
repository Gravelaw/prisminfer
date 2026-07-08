#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "prisminfer/benchmark_comparator.h"

namespace prisminfer {

struct KernelBenchmarkManifest {
  BenchmarkCell cell;
  std::string manifest_version;
  std::string validation_cell_id;
  std::string tdr_status;
  std::string application_control_status;
  std::string baseline_backend;
  std::string baseline_manifest_hash;
  std::string correctness_fixture_hash;
  std::string quality_fixture_hash;
  std::string compression_status{"none"};
  std::string compression_profile_id;
  std::string algorithm_family;
  std::string quality_gate_id;
  std::string quality_result_path;
  std::string cap_certification_status{"research-only"};
  bool full_dequant_materialized{false};
  std::uint64_t workspace_peak_bytes{0};
  std::uint64_t kv_payload_bytes{0};
  std::uint64_t kv_metadata_bytes{0};
  std::uint64_t kv_residual_or_sketch_bytes{0};
  double speedup_ratio{0.0};
  double effective_bits_per_value{0.0};
  double metadata_bits_per_value{0.0};
  double compression_decode_overhead_ms{0.0};
  double attention_logit_error_p95{0.0};
  double attention_logit_error_p99{0.0};
  double attention_topk_overlap{0.0};
  std::string claim_status;
};

struct KernelBenchmarkManifestResult {
  bool ok{false};
  KernelBenchmarkManifest manifest;
  std::string error;
};

KernelBenchmarkManifestResult read_kernel_benchmark_manifest(
    const std::filesystem::path& path);

}  // namespace prisminfer

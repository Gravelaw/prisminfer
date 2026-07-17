#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

#include "prisminfer/benchmark_comparator.h"
#include "prisminfer/flat_json.h"

namespace prisminfer {

struct KernelBenchmarkManifest {
  std::map<std::string, FlatJsonValue> fields;
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
  std::string supervisor_status;
  std::string admission_status;
  std::string run_outcome;
  std::string requested_execution_path;
  std::string actual_execution_path;
  std::string failure_reason;
  std::uint64_t raw_trial_count{0};
  std::string raw_trial_sha256;
  std::string failure_record_sha256;
  bool full_dequant_materialized{false};
  std::uint64_t workspace_peak_bytes{0};
  std::uint64_t device_resident_bytes{0};
  std::uint64_t host_commit_peak_bytes{0};
  std::uint64_t unknown_owned_bytes{0};
  std::uint64_t kv_payload_bytes{0};
  std::uint64_t kv_metadata_bytes{0};
  std::uint64_t kv_residual_or_sketch_bytes{0};
  double speedup_ratio{0.0};
  double ttft_ms{0.0};
  double prefill_ms{0.0};
  double decode_tokens_per_second{0.0};
  double request_tail_ms{0.0};
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

std::string canonical_kernel_benchmark_manifest_json(
    const KernelBenchmarkManifest& manifest);

bool write_kernel_benchmark_manifest(
    const std::filesystem::path& path,
    const KernelBenchmarkManifest& manifest,
    std::string* error);

}  // namespace prisminfer

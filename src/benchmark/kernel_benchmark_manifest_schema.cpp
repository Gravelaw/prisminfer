#include "prisminfer/kernel_benchmark_manifest_schema.h"

namespace prisminfer {

namespace {

bool one_of(const std::string& value, const std::set<std::string>& allowed) {
  return allowed.find(value) != allowed.end();
}

bool valid_vram_tier(std::uint32_t value) {
  return value == 4 || value == 6 || value == 8 || value == 12 || value == 16;
}

}  // namespace

const std::set<std::string>& kernel_manifest_allowed_keys() {
  static const std::set<std::string> keys{
      "manifest_version", "validation_cell_id", "model_hash",
      "quantization_format", "quant_artifact_sha256", "context_tokens",
      "batch_size", "prompt_fixture_hash", "backend", "os", "gpu_name",
      "driver_mode", "tdr_status", "application_control_status",
      "cuda_driver_version", "cuda_runtime_version", "vram_tier_gib",
      "hard_cap_bytes", "op_type", "sequence_phase", "kernel_backend",
      "kernel_name", "kernel_version", "baseline_backend",
      "baseline_manifest_hash", "correctness_fixture_hash",
      "quality_fixture_hash", "dequant_fused", "full_dequant_materialized",
      "workspace_bytes", "workspace_peak_bytes", "kernel_ms", "launch_ms",
      "sync_ms", "transfer_h2d_bytes", "transfer_d2h_bytes",
      "tensor_core_claimed", "tensor_core_used", "profiler_artifact_path",
      "profiler_artifact_sha256", "speedup_ratio", "claim_status",
      "compression_status", "compression_profile_id", "quantization_scope",
      "algorithm_family", "payload_bits_per_value",
      "effective_bits_per_value", "metadata_bits_per_value",
      "key_quant_axis", "value_quant_axis", "pre_rope_or_post_rope",
      "group_size", "residual_fp_window_tokens", "outlier_policy",
      "rotation_policy", "rotation_seed", "projection_policy",
      "qjl_residual_bits", "dequant_workspace_peak_bytes",
      "kv_payload_bytes", "kv_metadata_bytes",
      "kv_residual_or_sketch_bytes", "compression_decode_overhead_ms",
      "attention_logit_error_p95", "attention_logit_error_p99",
      "attention_topk_overlap", "quality_gate_id", "quality_result_path",
      "cap_certification_status", "run_outcome", "supervisor_status",
      "admission_status", "requested_execution_path", "actual_execution_path",
      "raw_trial_count", "raw_trial_sha256", "failure_record_sha256",
      "failure_reason", "device_resident_bytes", "host_commit_peak_bytes",
      "unknown_owned_bytes", "ttft_ms", "prefill_ms",
      "decode_tokens_per_second", "request_tail_ms"};
  return keys;
}

const std::vector<std::string>& kernel_manifest_required_keys() {
  static const std::vector<std::string> keys{
      "manifest_version", "validation_cell_id", "model_hash",
      "quantization_format", "quant_artifact_sha256", "context_tokens",
      "batch_size", "prompt_fixture_hash", "backend", "os", "gpu_name",
      "driver_mode", "cuda_driver_version", "cuda_runtime_version",
      "vram_tier_gib", "hard_cap_bytes", "op_type", "sequence_phase",
      "kernel_backend", "kernel_name", "kernel_version", "baseline_backend",
      "baseline_manifest_hash", "correctness_fixture_hash",
      "quality_fixture_hash", "full_dequant_materialized",
      "workspace_peak_bytes", "device_resident_bytes", "host_commit_peak_bytes",
      "unknown_owned_bytes", "ttft_ms", "prefill_ms",
      "decode_tokens_per_second", "request_tail_ms", "speedup_ratio",
      "claim_status", "compression_status", "quality_gate_id",
      "cap_certification_status", "run_outcome", "supervisor_status",
      "admission_status", "requested_execution_path", "actual_execution_path"};
  return keys;
}

bool kernel_manifest_identity_constraints_ok(
    const KernelBenchmarkManifest& manifest) {
  const bool promoted_cap_status =
      manifest.cap_certification_status == "quality-gated" ||
      manifest.cap_certification_status == "validated-benchmark";
  const bool completed = manifest.run_outcome == "completed";
  const bool promotable_claim = manifest.claim_status == "measured" ||
                                manifest.claim_status ==
                                    "measured-non-certified";
  return manifest.cell.context_tokens != 0 && manifest.cell.batch_size != 0 &&
         valid_vram_tier(manifest.cell.vram_tier_gib) &&
         manifest.cell.hard_cap_bytes != 0 &&
         manifest.cell.hard_cap_bytes <= 17'179'869'184ULL &&
         one_of(manifest.cell.driver_mode,
                {"wddm", "tcc", "linux", "unavailable"}) &&
         manifest.cell.op_type == "matmul" &&
         one_of(manifest.cell.sequence_phase, {"decode", "prefill"}) &&
         one_of(manifest.claim_status,
                {"measured", "measured-non-certified", "rejected",
                 "research-only"}) &&
         one_of(manifest.compression_status,
                {"none", "accounting-only", "reference", "experimental"}) &&
         one_of(manifest.cap_certification_status,
                {"research-only", "measured-non-certified", "quality-gated",
                 "validated-benchmark", "rejected"}) &&
         one_of(manifest.supervisor_status,
                {"not-attempted", "unavailable", "active", "rejected", "aborted"}) &&
         one_of(manifest.admission_status,
                {"not-attempted", "unavailable", "admitted", "rejected", "aborted"}) &&
         one_of(manifest.run_outcome,
                {"completed", "skipped", "unsupported", "rejected",
                 "aborted"}) &&
         manifest.unknown_owned_bytes == 0 &&
         (completed ||
          (!promotable_claim && manifest.cap_certification_status == "rejected")) &&
         (manifest.requested_execution_path == manifest.actual_execution_path ||
          (!promotable_claim && !promoted_cap_status)) &&
         (!promoted_cap_status ||
          (manifest.run_outcome == "completed" &&
           manifest.supervisor_status == "active" &&
           manifest.admission_status == "admitted")) &&
         !manifest.requested_execution_path.empty() &&
         !manifest.actual_execution_path.empty();
}

bool kernel_manifest_optional_constraints_ok(
    const KernelBenchmarkManifest& manifest) {
  const bool tdr_ok = manifest.tdr_status.empty() ||
                      one_of(manifest.tdr_status,
                             {"enabled", "disabled", "unavailable"});
  const bool app_control_ok =
      manifest.application_control_status.empty() ||
      one_of(manifest.application_control_status,
             {"allowed", "blocked", "unavailable"});
  return tdr_ok && app_control_ok;
}

}  // namespace prisminfer

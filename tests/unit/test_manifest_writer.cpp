#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "prisminfer/benchmark.h"
#include "prisminfer/kv_cache_ledger.h"
#include "prisminfer/kv_compression_policy.h"
#include "prisminfer/quality_gate.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

}  // namespace

int main() {
  const auto path =
      std::filesystem::temp_directory_path() / "prisminfer_manifest_test.json";
  std::error_code remove_error;
  std::filesystem::remove(path, remove_error);

  prisminfer::RuntimeConfig config;
  config.run_id = "manifest-run";
  config.telemetry_path = "manifest-run.jsonl";
  config.model_parameter_bucket = "<=2B";
  config.parameter_count = 123456;
  config.vram_tier_gib = 1;
  config.validation_cell_id = "cell-manifest";
  config.validation_cell_status = "metadata-only";
  config.llama_executable_path = "llama-cli";
  config.worker_evidence_available = true;
  config.worker_executable_sha256 = "worker-sha256";
  config.worker_approval_identity = "approval-1";
  config.worker_exit_code = 0;
  config.worker_root_process_id = 1234;
  config.worker_job_identity = "job:1:1234:99";
  config.worker_job_total_processes = 1;
  config.worker_job_peak_active_processes = 1;
  config.worker_root_peak_working_set_bytes = 3072;
  config.worker_root_peak_private_commit_bytes = 2048;
  config.worker_tree_peak_working_set_bytes = 3072;
  config.worker_tree_peak_private_commit_bytes = 4096;
  config.worker_tree_sample_interval_milliseconds = 10;
  config.worker_output_bytes = 128;
  config.worker_output_limit_bytes = 1024;
  config.kv_accounting = true;
  config.kv_placement = "host";
  config.kv_compression = "accounting-only";
  config.quality_gate = "smoke";
  config.profitability_policy = "fail-closed";
  config.offload_policy = "gpu";
  config.baseline_manifest = "cpu-baseline.json";
  config.min_speedup_ratio = 1.1;
  config.cpu_baseline_ttft_ms = 100.0;
  config.cpu_baseline_decode_tps = 10.0;

  prisminfer::MemorySample sample;
  sample.allocator_peak_bytes = 123;
  sample.process_gpu_peak_bytes = 123;
  sample.device_delta_bytes = 12;
  sample.warmup_peak_bytes = 99;
  sample.unknown_post_warmup_bytes = 7;
  sample.kv_host_peak_bytes = 4096;
  sample.kv_metadata_peak_bytes = 128;

  prisminfer::CudaProbeResult cuda;
  cuda.gpu_name = "test gpu";
  cuda.driver_version = 13030;
  cuda.runtime_version = 13030;

  prisminfer::HostTelemetrySample host;
  host.available = true;
  host.system_commit_source = "get_performance_info";
  host.captured_monotonic_milliseconds = 12345;
  host.process_id = 77;
  host.process_image_path = R"(C:\prism-probe.exe)";
  host.process_working_set_current_bytes = 111;
  host.process_working_set_peak_bytes = 333;
  host.process_private_commit_current_bytes = 222;
  host.process_private_commit_peak_bytes = 444;
  host.system_memory_total_bytes = 1000;
  host.system_memory_available_bytes = 700;
  host.system_commit_total_bytes = 400;
  host.system_commit_limit_bytes = 900;
  host.system_commit_available_bytes = 500;
  host.pagefile_configuration_available = true;
  host.pagefile_count = 1;
  host.pagefile_total_bytes = 800;
  host.pagefile_current_usage_bytes = 200;
  host.pagefile_peak_usage_bytes = 300;
  host.process_io_read_bytes = 333;
  host.process_io_write_bytes = 444;

  prisminfer::WddmMemorySample wddm;
  wddm.available = true;
  wddm.captured_monotonic_milliseconds = 12346;
  wddm.adapter_description = "test adapter";
  wddm.local_budget_bytes = 8192;
  wddm.local_current_usage_bytes = 4096;
  prisminfer::OwnedGpuMemoryEvidence owned_gpu;
  owned_gpu.available = true;
  owned_gpu.reconciled = true;
  owned_gpu.hard_cap_bytes = 8192;
  owned_gpu.owned_current_bytes = 2048;
  owned_gpu.owned_peak_bytes = 4096;
  owned_gpu.backend_buffer_current_bytes = 2048;
  owned_gpu.backend_buffer_at_owned_peak_bytes = 4096;
  prisminfer::WindowsEvidenceDecision windows_evidence;
  windows_evidence.classification = "measured-non-certified";
  windows_evidence.reason = "complete_residency_trace_required";
  prisminfer::FileIoEvidence file;
  file.identity_available = true;
  file.role = "source-gguf";
  file.final_path = R"(C:\model.gguf)";
  file.volume_serial_hex = "0123456789abcdef";
  file.file_id_hex = "0123456789abcdef0123456789abcdef";
  file.size_bytes = 4096;
  file.mapped_bytes = 1024;

  std::string error;
  prisminfer::KvCacheProfile profile;
  profile.layer_count = 2;
  profile.kv_head_count = 4;
  profile.head_dim = 8;
  profile.block_tokens = 16;
  profile.key_bits = 16;
  profile.value_bits = 16;
  profile.bytes_per_token = 256;
  profile.bytes_per_block = 4096;
  prisminfer::KvCacheSample kv_sample;
  kv_sample.logical_tokens = 16;
  kv_sample.active_blocks = 1;
  kv_sample.host_bytes = 4096;
  kv_sample.metadata_bytes = 128;
  kv_sample.evidence_status = "reconciled";
  prisminfer::QualityGateResult quality;
  quality.passed = true;
  quality.status = "passed";
  prisminfer::KvCompressionResult compression;
  compression.accepted = true;
  compression.status = "accounting-only";
  prisminfer::OffloadPlan offload_plan;
  offload_plan.policy = "gpu";
  offload_plan.h2d_bytes = 1024;
  offload_plan.staging_peak_bytes = 2048;
  offload_plan.evidence_label = "measured-offload";
  prisminfer::TransferSample transfer;
  transfer.declared_h2d_bytes = 1024;
  transfer.events.push_back({"transfer-1", "plan-1", "h2d", "host-1", "gpu-1",
                             "pinned", "stream-1", "copy-1", "tensor-1", 1024,
                             1024, 1, 2, 3, 1, 0, false, false, false});
  if (expect(prisminfer::reconcile_transfer_sample(&transfer).ok,
             "manifest transfer fixture reconciles"))
    return 1;
  transfer.time_to_first_token_ms = 80.0;
  transfer.decode_tokens_per_second = 12.0;
  prisminfer::ProfitabilityDecision profitability;
  profitability.accepted = true;
  profitability.status = "accepted";
  profitability.speedup_ratio = 1.2;
  profitability.required_speedup_ratio = 1.1;
  prisminfer::HybridPlanResult hybrid_plan;
  hybrid_plan.ok = true;
  hybrid_plan.resident_gpu_total_bytes = 1234;
  prisminfer::UsabilityResult usability;
  usability.accepted = true;
  usability.status = "accepted";
  prisminfer::ClaimDecision claim;
  claim.accepted = true;
  claim.status = "accepted";

  const prisminfer::ManifestInputs inputs{
      config, sample, host, host, wddm, owned_gpu, {file}, windows_evidence,
      cuda, profile, kv_sample, quality, compression, offload_plan, transfer,
      profitability, hybrid_plan, usability, claim, "ok", ""};
  if (expect(prisminfer::write_probe_manifest(path, inputs, &error),
             error.c_str()))
    return 1;

  std::ifstream in(path);
  if (expect(in.good(), "manifest file opens")) return 1;
  std::stringstream buffer;
  buffer << in.rdbuf();
  in.close();
  const auto content = buffer.str();

  if (expect(content.find("\"manifest_version\": \"0.6\"") != std::string::npos,
             "manifest version written"))
    return 1;
  if (expect(content.find("\"run_id\": \"manifest-run\"") != std::string::npos,
             "run id written"))
    return 1;
  if (expect(
          content.find("\"worker_executable_sha256\": \"worker-sha256\"") !=
                  std::string::npos &&
              content.find("\"worker_job_identity\": "
                           "\"job:1:1234:99\"") != std::string::npos &&
              content.find("\"worker_tree_peak_private_commit_bytes\": 4096") !=
                  std::string::npos &&
              content.find("\"worker_output_limit_bytes\": 1024") !=
                  std::string::npos,
          "worker identity and process-tree Job evidence written"))
    return 1;
  if (expect(content.find("\"allocator_peak_bytes\": 123") != std::string::npos,
             "allocator peak written"))
    return 1;
  if (expect(content.find("\"model_parameter_bucket\": \"<=2B\"") !=
                 std::string::npos,
             "model parameter bucket written"))
    return 1;
  if (expect(content.find("\"parameter_count\": 123456") != std::string::npos,
             "parameter count written"))
    return 1;
  if (expect(content.find("\"validation_cell_id\": \"cell-manifest\"") !=
                 std::string::npos,
             "validation cell id written"))
    return 1;
  if (expect(content.find("\"backend\": \"null\"") != std::string::npos,
             "backend written"))
    return 1;
  if (expect(content.find("\"backend_adapter_contract_version\": \"0.2\"") !=
                 std::string::npos,
             "backend contract version written"))
    return 1;
  if (expect(content.find("\"llama_executable\": \"llama-cli\"") !=
                 std::string::npos,
             "llama executable written"))
    return 1;
  if (expect(content.find("\"host_pre_telemetry_available\": true") !=
                     std::string::npos &&
                 content.find("\"host_post_telemetry_available\": true") !=
                     std::string::npos,
             "host pre/post telemetry availability written"))
    return 1;
  if (expect(
          content.find("\"host_post_process_id\": 77") != std::string::npos &&
              content.find(
                  "\"host_post_process_working_set_current_bytes\": 111") !=
                  std::string::npos,
          "host process identity and current working set written"))
    return 1;
  if (expect(content.find("\"host_post_system_memory_total_bytes\": 1000") !=
                 std::string::npos,
             "host physical total written") ||
      expect(content.find("\"host_post_system_commit_available_bytes\": 500") !=
                 std::string::npos,
             "host system commit headroom written") ||
      expect(content.find("\"host_post_system_commit_source\": "
                          "\"get_performance_info\"") != std::string::npos,
             "host system commit source written") ||
      expect(content.find(
                 "\"host_post_captured_monotonic_milliseconds\": 12345") !=
                 std::string::npos,
             "host monotonic capture time written")) {
    return 1;
  }
  if (expect(
          content.find(
              "\"host_post_pagefile_configuration_available\": true") !=
                  std::string::npos &&
              content.find("\"host_post_pagefile_current_usage_bytes\": 200") !=
                  std::string::npos,
          "pagefile configuration and usage state are written")) {
    return 1;
  }
  if (expect(content.find("\"build_config\":") != std::string::npos,
             "build config written"))
    return 1;
  if (expect(
          content.find("\"wddm_local_budget_bytes\": 8192") !=
                  std::string::npos &&
              content.find("\"windows_evidence_classification\": "
                           "\"measured-non-certified\"") != std::string::npos,
          "WDDM sample and downgraded classification written"))
    return 1;
  if (expect(content.find("\"owned_gpu_peak_bytes\": 4096") !=
                     std::string::npos &&
                 content.find(
                     "\"owned_gpu_backend_buffer_current_bytes\": 2048") !=
                     std::string::npos &&
                 content.find(
                     "\"owned_gpu_backend_buffer_at_peak_bytes\": 4096") !=
                     std::string::npos &&
                 content.find(
                     "\"owned_gpu_unknown_unreconciled_bytes\": 0") !=
                     std::string::npos,
             "owned GPU peak categories and unknown bytes are retained")) {
    return 1;
  }
  if (expect(content.find("\"file_evidence\": [") != std::string::npos &&
                 content.find("\"file_id_hex\": "
                              "\"0123456789abcdef0123456789abcdef\"") !=
                     std::string::npos &&
                 content.find("\"mapped_bytes\": 1024") != std::string::npos &&
                 content.find("\"identity_aware_io_available\": false") !=
                     std::string::npos &&
                 content.find("\"pagefile_io_available\": false") !=
                     std::string::npos,
             "file identity, mapping, file IO, and pagefile fields stay "
             "separate")) {
    return 1;
  }
  if (expect(
          content.find("\"kernel_build_enabled\": false") != std::string::npos,
          "kernel build flag written"))
    return 1;
  if (expect(content.find("\"kernel_cuda_archs\":") != std::string::npos,
             "kernel arch list written"))
    return 1;
  if (expect(content.find("\"warmup_peak_bytes\": 99") != std::string::npos,
             "warmup peak written"))
    return 1;
  if (expect(
          content.find("\"unknown_post_warmup_bytes\": 7") != std::string::npos,
          "unknown post-warmup bytes written"))
    return 1;
  if (expect(content.find("\"kv_bytes_per_token\": 256") != std::string::npos,
             "kv bytes per token written"))
    return 1;
  if (expect(content.find("\"kv_host_bytes\": 4096") != std::string::npos,
             "kv host bytes written"))
    return 1;
  if (expect(
          content.find("\"quality_status\": \"passed\"") != std::string::npos,
          "quality status written"))
    return 1;
  if (expect(content.find("\"profitability_status\": \"accepted\"") !=
                 std::string::npos,
             "profitability status written"))
    return 1;
  if (expect(content.find("\"compression_status\": \"accounting-only\"") !=
                 std::string::npos,
             "phase6 compression status written"))
    return 1;
  if (expect(
          content.find("\"effective_bits_per_value\": 0") != std::string::npos,
          "phase6 effective bits default written"))
    return 1;
  if (expect(content.find("\"kv_payload_bytes\": 0") != std::string::npos,
             "phase6 kv payload bytes written"))
    return 1;
  if (expect(
          content.find("\"quality_gate_id\": \"smoke\"") != std::string::npos,
          "phase6 quality gate id written"))
    return 1;
  if (expect(content.find("\"cap_certification_status\": \"research-only\"") !=
                 std::string::npos,
             "phase6 cap status remains research-only"))
    return 1;
  if (expect(content.find("\"claim_validation_status\": \"accepted\"") !=
                 std::string::npos,
             "claim validation status written"))
    return 1;
  if (expect(
          content.find("\"h2d_declared_bytes\": 1024") != std::string::npos &&
              content.find("\"h2d_observed_completed_bytes\": 1024") !=
                  std::string::npos &&
              content.find("\"transfer_event_count\": 1") != std::string::npos,
          "declared and observed transfer evidence stay separate")) {
    return 1;
  }
  if (expect(content.find("\"gpu_name\": \"test gpu\"") != std::string::npos,
             "gpu name written"))
    return 1;

  remove_error.clear();
  std::filesystem::remove(path, remove_error);
  return 0;
}

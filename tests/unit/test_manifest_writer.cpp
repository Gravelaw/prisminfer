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
  host.process_working_set_bytes = 111;
  host.process_private_bytes = 222;
  host.process_io_read_bytes = 333;
  host.process_io_write_bytes = 444;

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
  transfer.h2d_bytes = 1024;
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

  const prisminfer::ManifestInputs inputs{config, sample, host, cuda, profile,
                                          kv_sample, quality, compression,
                                          offload_plan, transfer,
                                          profitability, hybrid_plan,
                                          usability, claim, "ok", ""};
  if (expect(prisminfer::write_probe_manifest(path, inputs, &error),
             error.c_str())) return 1;

  std::ifstream in(path);
  if (expect(in.good(), "manifest file opens")) return 1;
  std::stringstream buffer;
  buffer << in.rdbuf();
  in.close();
  const auto content = buffer.str();

  if (expect(content.find("\"manifest_version\": \"0.5\"") !=
                 std::string::npos,
             "manifest version written")) return 1;
  if (expect(content.find("\"run_id\": \"manifest-run\"") !=
                 std::string::npos,
             "run id written")) return 1;
  if (expect(content.find("\"allocator_peak_bytes\": 123") !=
                 std::string::npos,
             "allocator peak written")) return 1;
  if (expect(content.find("\"model_parameter_bucket\": \"<=2B\"") !=
                 std::string::npos,
             "model parameter bucket written")) return 1;
  if (expect(content.find("\"parameter_count\": 123456") !=
                 std::string::npos,
             "parameter count written")) return 1;
  if (expect(content.find("\"validation_cell_id\": \"cell-manifest\"") !=
                 std::string::npos,
             "validation cell id written")) return 1;
  if (expect(content.find("\"backend\": \"null\"") != std::string::npos,
             "backend written")) return 1;
  if (expect(content.find("\"backend_adapter_contract_version\": \"0.2\"") !=
                 std::string::npos,
             "backend contract version written")) return 1;
  if (expect(content.find("\"llama_executable\": \"llama-cli\"") !=
                 std::string::npos,
             "llama executable written")) return 1;
  if (expect(content.find("\"host_telemetry_available\": true") !=
                 std::string::npos,
             "host telemetry availability written")) return 1;
  if (expect(content.find("\"process_working_set_bytes\": 111") !=
                 std::string::npos,
             "host working set written")) return 1;
  if (expect(content.find("\"build_config\":") != std::string::npos,
             "build config written")) return 1;
  if (expect(content.find("\"kernel_build_enabled\": false") !=
                 std::string::npos,
             "kernel build flag written")) return 1;
  if (expect(content.find("\"kernel_cuda_archs\":") != std::string::npos,
             "kernel arch list written")) return 1;
  if (expect(content.find("\"warmup_peak_bytes\": 99") !=
                 std::string::npos,
             "warmup peak written")) return 1;
  if (expect(content.find("\"unknown_post_warmup_bytes\": 7") !=
                 std::string::npos,
             "unknown post-warmup bytes written")) return 1;
  if (expect(content.find("\"kv_bytes_per_token\": 256") !=
                 std::string::npos,
             "kv bytes per token written")) return 1;
  if (expect(content.find("\"kv_host_bytes\": 4096") !=
                 std::string::npos,
             "kv host bytes written")) return 1;
  if (expect(content.find("\"quality_status\": \"passed\"") !=
                 std::string::npos,
             "quality status written")) return 1;
  if (expect(content.find("\"profitability_status\": \"accepted\"") !=
                  std::string::npos,
              "profitability status written")) return 1;
  if (expect(content.find("\"compression_status\": \"accounting-only\"") !=
                 std::string::npos,
             "phase6 compression status written")) return 1;
  if (expect(content.find("\"effective_bits_per_value\": 0") !=
                 std::string::npos,
             "phase6 effective bits default written")) return 1;
  if (expect(content.find("\"kv_payload_bytes\": 0") != std::string::npos,
             "phase6 kv payload bytes written")) return 1;
  if (expect(content.find("\"quality_gate_id\": \"smoke\"") !=
                 std::string::npos,
             "phase6 quality gate id written")) return 1;
  if (expect(content.find("\"cap_certification_status\": \"research-only\"") !=
                 std::string::npos,
             "phase6 cap status remains research-only")) return 1;
  if (expect(content.find("\"claim_validation_status\": \"accepted\"") !=
                  std::string::npos,
              "claim validation status written")) return 1;
  if (expect(content.find("\"h2d_bytes\": 1024") != std::string::npos,
             "transfer bytes written")) return 1;
  if (expect(content.find("\"gpu_name\": \"test gpu\"") !=
                 std::string::npos,
             "gpu name written")) return 1;

  remove_error.clear();
  std::filesystem::remove(path, remove_error);
  return 0;
}

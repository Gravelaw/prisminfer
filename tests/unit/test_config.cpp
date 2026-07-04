#include <iostream>
#include <string>
#include <vector>

#include "prisminfer/config.h"

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
  {
    const auto parsed = prisminfer::parse_args({});
    if (expect(parsed.config.has_value(), "default args parse")) return 1;
    if (expect(parsed.config->mode == prisminfer::RunMode::OneGbSafeCpu,
               "default mode is CPU safe")) return 1;
    if (expect(parsed.config->hard_cap_bytes == prisminfer::kOneGiB,
               "default hard cap is one GiB")) return 1;
  }
  {
    const std::vector<std::string> args = {
        "--mode", "1gb-safe-gpu-probed", "--hard-cap-bytes", "2048",
        "--telemetry", "out.jsonl", "--manifest", "manifest.json",
        "--run-id", "run-123", "--model", "model.gguf", "--sidecar",
        "model.gguf.prism.json", "--max-model-bytes", "8192",
        "--max-sidecar-bytes", "512", "--model-parameter-bucket", "<=2B",
        "--parameter-count", "123456", "--vram-tier-gib", "2",
        "--validation-cell-id", "cell-1", "--validation-cell-status",
        "metadata-only", "--backend", "fake", "--backend-required",
        "--dependency-pin-file", "pins.json", "--llama-executable",
        "llama-cli", "--context-tokens", "2048", "--gpu-layers", "4",
        "--mmap", "off", "--warmup-tokens", "8", "--kv-accounting", "on",
        "--kv-layer-count", "12", "--kv-head-count", "4", "--kv-head-dim",
        "64", "--kv-block-tokens", "32", "--kv-placement", "mixed",
        "--kv-compression", "reference", "--kv-key-bits", "8",
        "--kv-value-bits", "8", "--kv-metadata-budget-bytes", "1024",
        "--quality-gate", "retrieval", "--quality-baseline-manifest",
        "baseline.json", "--quality-baseline-score", "1.0",
        "--quality-observed-score", "1.01", "--quality-max-delta", "0.02",
        "--quality-retrieval-passed", "true",
        "--quality-deterministic-match", "true",
        "--simulate-allocator-peak-bytes", "4096",
        "--simulate-process-gpu-peak-bytes", "4096",
        "--simulate-warmup-peak-bytes", "1024",
        "--simulate-unknown-post-warmup-bytes", "1"};
    const auto parsed = prisminfer::parse_args(args);
    if (expect(parsed.config.has_value(), "explicit args parse")) return 1;
    if (expect(parsed.config->mode == prisminfer::RunMode::OneGbSafeGpuProbed,
               "explicit GPU-probed mode")) return 1;
    if (expect(parsed.config->hard_cap_bytes == 2048,
               "explicit hard cap")) return 1;
    if (expect(parsed.config->telemetry_path == "out.jsonl",
               "explicit telemetry path")) return 1;
    if (expect(parsed.config->manifest_path == "manifest.json",
               "explicit manifest path")) return 1;
    if (expect(parsed.config->run_id == "run-123", "explicit run id")) {
      return 1;
    }
    if (expect(parsed.config->model_path == "model.gguf",
               "explicit model path")) return 1;
    if (expect(parsed.config->sidecar_path == "model.gguf.prism.json",
               "explicit sidecar path")) return 1;
    if (expect(parsed.config->max_model_bytes == 8192,
               "explicit model size limit")) return 1;
    if (expect(parsed.config->max_sidecar_bytes == 512,
               "explicit sidecar size limit")) return 1;
    if (expect(parsed.config->model_parameter_bucket == "<=2B",
               "explicit model parameter bucket")) return 1;
    if (expect(parsed.config->parameter_count == 123456,
               "explicit parameter count")) return 1;
    if (expect(parsed.config->vram_tier_gib == 2,
               "explicit vram tier")) return 1;
    if (expect(parsed.config->validation_cell_id == "cell-1",
               "explicit validation cell id")) return 1;
    if (expect(parsed.config->validation_cell_status == "metadata-only",
               "explicit validation cell status")) return 1;
    if (expect(parsed.config->backend == prisminfer::BackendKind::Fake,
               "explicit backend")) return 1;
    if (expect(parsed.config->backend_required, "backend required")) return 1;
    if (expect(parsed.config->dependency_pin_file == "pins.json",
               "explicit dependency pin file")) return 1;
    if (expect(parsed.config->llama_executable_path == "llama-cli",
               "explicit llama executable")) return 1;
    if (expect(parsed.config->context_tokens == 2048,
               "explicit context tokens")) return 1;
    if (expect(parsed.config->gpu_layers == 4, "explicit gpu layers")) {
      return 1;
    }
    if (expect(!parsed.config->mmap_enabled, "explicit mmap off")) return 1;
    if (expect(parsed.config->warmup_tokens == 8,
               "explicit warmup tokens")) return 1;
    if (expect(parsed.config->kv_accounting, "kv accounting enabled")) {
      return 1;
    }
    if (expect(parsed.config->kv_layer_count == 12,
               "explicit kv layer count")) return 1;
    if (expect(parsed.config->kv_head_count == 4,
               "explicit kv head count")) return 1;
    if (expect(parsed.config->kv_head_dim == 64,
               "explicit kv head dim")) return 1;
    if (expect(parsed.config->kv_block_tokens == 32,
               "explicit kv block tokens")) return 1;
    if (expect(parsed.config->kv_placement == "mixed",
               "explicit kv placement")) return 1;
    if (expect(parsed.config->kv_compression == "reference",
               "explicit kv compression")) return 1;
    if (expect(parsed.config->kv_key_bits == 8,
               "explicit kv key bits")) return 1;
    if (expect(parsed.config->kv_value_bits == 8,
               "explicit kv value bits")) return 1;
    if (expect(parsed.config->kv_metadata_budget_bytes == 1024,
               "explicit kv metadata budget")) return 1;
    if (expect(parsed.config->quality_gate == "retrieval",
               "explicit quality gate")) return 1;
    if (expect(parsed.config->quality_baseline_manifest == "baseline.json",
               "explicit quality baseline manifest")) return 1;
    if (expect(parsed.config->quality_observed_score > 1.0,
               "explicit quality score")) return 1;
    if (expect(parsed.config->simulate_allocator_peak_bytes == 4096,
               "explicit allocator simulation")) return 1;
    if (expect(parsed.config->simulate_process_gpu_peak_bytes == 4096,
               "explicit process simulation")) return 1;
    if (expect(parsed.config->simulate_warmup_peak_bytes == 1024,
               "explicit warmup simulation")) return 1;
    if (expect(parsed.config->simulate_unknown_post_warmup_bytes == 1,
               "explicit unknown post-warmup simulation")) return 1;
  }
  {
    const auto parsed = prisminfer::parse_args({"--mode", "invalid"});
    if (expect(!parsed.config.has_value(), "invalid mode rejected")) return 1;
    if (expect(parsed.error.find("unsupported mode") != std::string::npos,
               "invalid mode error is specific")) return 1;
  }
  {
    const auto parsed =
        prisminfer::parse_args({"--hard-cap-bytes", "17179869185"});
    if (expect(!parsed.config.has_value(), "cap above 16 GiB rejected")) {
      return 1;
    }
    if (expect(parsed.error == "hard_cap_exceeds_max_gpu_cap",
               "cap policy error is specific")) return 1;
  }
  {
    const auto parsed = prisminfer::parse_args({"--vram-tier-gib", "32"});
    if (expect(!parsed.config.has_value(), "tier above 16 GiB rejected")) {
      return 1;
    }
    if (expect(parsed.error == "vram_tier_exceeds_max_gpu_cap",
               "tier policy error is specific")) return 1;
  }
  return 0;
}

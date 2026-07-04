#include <iostream>
#include <string>
#include <vector>

#include "prisminfer/allocator_tracker.h"
#include "prisminfer/backend.h"
#include "prisminfer/backend_memory_observer.h"
#include "prisminfer/benchmark.h"
#include "prisminfer/config.h"
#include "prisminfer/cuda_context_probe.h"
#include "prisminfer/errors.h"
#include "prisminfer/host_memory_tracker.h"
#include "prisminfer/kv_cache_ledger.h"
#include "prisminfer/kv_compression_policy.h"
#include "prisminfer/memory_cap.h"
#include "prisminfer/memory_ledger.h"
#include "prisminfer/model_sidecar.h"
#include "prisminfer/offload_planner.h"
#include "prisminfer/profitability_policy.h"
#include "prisminfer/quality_gate.h"
#include "prisminfer/runtime_state.h"
#include "prisminfer/telemetry.h"
#include "prisminfer/transfer_metrics.h"

namespace {

std::vector<std::string> collect_args(int argc, char** argv) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  return args;
}

void print_summary(const prisminfer::RuntimeConfig& config,
                   const std::string& status,
                   const std::string& reason) {
  std::cout << "{"
            << "\"status\":\"" << prisminfer::json_escape(status) << "\","
            << "\"mode\":\""
            << prisminfer::json_escape(prisminfer::to_string(config.mode))
            << "\","
            << "\"hard_cap_bytes\":" << config.hard_cap_bytes << ","
            << "\"failure_reason\":\"" << prisminfer::json_escape(reason)
            << "\""
            << "}\n";
}

void apply_simulated_breaches(const prisminfer::RuntimeConfig& config,
                              prisminfer::CappedAllocatorTracker* allocator,
                              prisminfer::MemorySample* sample) {
  if (config.simulate_allocator_peak_bytes > 0) {
    if (config.simulate_allocator_peak_bytes > allocator->current_bytes()) {
      const auto additional =
          config.simulate_allocator_peak_bytes - allocator->current_bytes();
      const auto result = allocator->reserve(additional);
      if (!result.accepted) {
        allocator->observe_rejected_peak(config.simulate_allocator_peak_bytes);
      }
    } else {
      allocator->observe_rejected_peak(config.simulate_allocator_peak_bytes);
    }
    const auto allocator_sample = allocator->sample();
    sample->allocator_bytes = allocator_sample.allocator_bytes;
    sample->allocator_peak_bytes = allocator_sample.allocator_peak_bytes;
  }
  if (config.simulate_process_gpu_peak_bytes > 0) {
    sample->process_gpu_peak_bytes = config.simulate_process_gpu_peak_bytes;
  }
  if (config.simulate_warmup_peak_bytes > 0) {
    sample->warmup_peak_bytes = config.simulate_warmup_peak_bytes;
  }
  if (config.simulate_unknown_post_warmup_bytes > 0) {
    sample->unknown_post_warmup_bytes =
        config.simulate_unknown_post_warmup_bytes;
  }
}

std::uint64_t ceil_div(std::uint64_t lhs, std::uint64_t rhs) {
  return (lhs + rhs - 1) / rhs;
}

prisminfer::KvCacheSample make_kv_sample(
    const prisminfer::RuntimeConfig& config,
    const prisminfer::KvCacheProfile& profile) {
  prisminfer::KvCacheSample sample;
  if (!config.kv_accounting || profile.bytes_per_token == 0) {
    return sample;
  }
  sample.logical_tokens = config.context_tokens + config.warmup_tokens;
  sample.active_blocks = ceil_div(sample.logical_tokens, profile.block_tokens);
  sample.metadata_bytes = sample.active_blocks * 64;
  const std::uint64_t baseline_bytes =
      sample.logical_tokens * profile.bytes_per_token;
  sample.evidence_status =
      config.backend == prisminfer::BackendKind::Fake ? "reconciled"
                                                      : "estimated";

  if (config.kv_compression == "reference" ||
      config.kv_compression == "experimental") {
    const std::uint64_t compressed_per_token =
        profile.layer_count * profile.kv_head_count * profile.head_dim *
        static_cast<std::uint64_t>(config.kv_key_bits + config.kv_value_bits) /
        8ULL;
    sample.compressed_bytes = compressed_per_token * sample.logical_tokens;
  }

  const std::uint64_t payload_bytes =
      sample.compressed_bytes > 0 ? sample.compressed_bytes : baseline_bytes;
  if (config.kv_placement == "gpu") {
    sample.gpu_bytes = payload_bytes;
  } else if (config.kv_placement == "host") {
    sample.host_bytes = payload_bytes;
  } else if (config.kv_placement == "mixed") {
    sample.gpu_bytes = payload_bytes / 2;
    sample.host_bytes = payload_bytes - sample.gpu_bytes;
  } else {
    sample.unknown_bytes =
        sample.evidence_status == "reconciled" ? 0 : payload_bytes;
    if (sample.evidence_status == "reconciled") {
      sample.host_bytes = payload_bytes;
    }
  }
  return sample;
}

void apply_kv_sample_to_memory(const prisminfer::KvCacheSample& kv_sample,
                               prisminfer::MemorySample* sample) {
  sample->kv_gpu_peak_bytes = kv_sample.gpu_bytes;
  sample->kv_host_peak_bytes = kv_sample.host_bytes;
  sample->kv_compressed_peak_bytes = kv_sample.compressed_bytes;
  sample->kv_metadata_peak_bytes = kv_sample.metadata_bytes;
  sample->kv_unknown_peak_bytes = kv_sample.unknown_bytes;
  if (sample->warmup_peak_bytes < kv_sample.gpu_bytes) {
    sample->warmup_peak_bytes = kv_sample.gpu_bytes;
  }
  sample->unknown_post_warmup_bytes += kv_sample.unknown_bytes;
}

prisminfer::TransferSample make_transfer_sample(
    const prisminfer::RuntimeConfig& config,
    const prisminfer::OffloadPlan& plan) {
  prisminfer::TransferSample sample;
  sample.h2d_bytes = plan.h2d_bytes;
  sample.d2h_bytes = plan.d2h_bytes;
  sample.nvme_read_bytes = plan.nvme_read_bytes;
  sample.nvme_write_bytes = plan.nvme_write_bytes;
  sample.pinned_host_peak_bytes = plan.pinned_host_peak_bytes;
  sample.staging_peak_bytes = plan.staging_peak_bytes;
  sample.h2d_ms = config.transfer_h2d_ms;
  sample.d2h_ms = config.transfer_d2h_ms;
  sample.io_ms = config.transfer_io_ms;
  sample.wait_ms = config.transfer_wait_ms;
  sample.prefill_ms = config.prefill_ms;
  sample.decode_ms = config.decode_ms;
  sample.time_to_first_token_ms = config.observed_ttft_ms;
  sample.decode_tokens_per_second = config.observed_decode_tps;
  sample.token_latency_p50_ms = config.token_latency_p50_ms;
  sample.token_latency_p95_ms = config.token_latency_p95_ms;
  return sample;
}

int finish_probe(const prisminfer::RuntimeConfig& config,
                 const prisminfer::MemorySample& sample,
                 const prisminfer::CudaProbeResult& cuda,
                 const prisminfer::KvCacheProfile& kv_profile,
                 const prisminfer::KvCacheSample& kv_sample,
                 const prisminfer::QualityGateResult& quality,
                 const prisminfer::KvCompressionResult& compression,
                 const prisminfer::OffloadPlan& offload_plan,
                 const prisminfer::TransferSample& transfer,
                 const prisminfer::ProfitabilityDecision& profitability,
                 const std::string& status,
                 const std::string& reason,
                 prisminfer::ExitCode exit_code) {
  std::string manifest_error;
  const auto host = prisminfer::sample_host_telemetry();
  const prisminfer::ManifestInputs manifest{
      config, sample, host, cuda, kv_profile, kv_sample,
      quality, compression, offload_plan, transfer, profitability, status,
      reason};
  if (!prisminfer::write_probe_manifest(config.manifest_path, manifest,
                                        &manifest_error)) {
    std::cerr << manifest_error << "\n";
    return static_cast<int>(prisminfer::ExitCode::IoError);
  }
  print_summary(config, status, reason);
  return static_cast<int>(exit_code);
}

}  // namespace

int main(int argc, char** argv) {
  const auto parsed = prisminfer::parse_args(collect_args(argc, argv));
  if (!parsed.config.has_value()) {
    std::cerr << parsed.error << "\n\n" << prisminfer::usage_text();
    return static_cast<int>(prisminfer::ExitCode::Usage);
  }

  const prisminfer::RuntimeConfig config = *parsed.config;
  if (config.show_help) {
    std::cout << prisminfer::usage_text();
    return static_cast<int>(prisminfer::ExitCode::Ok);
  }

  prisminfer::JsonlTelemetry telemetry(config.telemetry_path);
  if (!telemetry.ok()) {
    std::cerr << telemetry.error() << "\n";
    return static_cast<int>(prisminfer::ExitCode::IoError);
  }

  telemetry.emit(prisminfer::event::RunStart, config);
  telemetry.emit(prisminfer::event::ConfigValidated, config);
  telemetry.emit(prisminfer::event::TelemetryProbe, config);

  prisminfer::CappedAllocatorTracker allocator(config.hard_cap_bytes);
  prisminfer::MemorySample sample;
  prisminfer::CudaProbeResult cuda;
  prisminfer::KvCacheProfile kv_profile;
  prisminfer::KvCacheSample kv_sample;
  prisminfer::QualityGateResult quality;
  prisminfer::KvCompressionResult compression;
  prisminfer::OffloadPlan offload_plan;
  prisminfer::TransferSample transfer;
  prisminfer::ProfitabilityDecision profitability;

  const prisminfer::ModelSidecarValidationRequest validation_request{
      config.model_path, config.sidecar_path, config.max_model_bytes,
      config.max_sidecar_bytes};
  const auto validation =
      prisminfer::validate_model_sidecar(validation_request);
  telemetry.emit(prisminfer::event::ModelSidecarValidated, config,
                 {{"skipped", validation.skipped ? "true" : "false"},
                  {"valid", validation.valid ? "true" : "false"},
                  {"failure_reason", validation.failure_reason},
                  {"model_path",
                   validation.normalized_model_path.generic_string()},
                  {"sidecar_path",
                   validation.normalized_sidecar_path.generic_string()},
                  {"model_size_bytes",
                   std::to_string(validation.model_size_bytes)},
                  {"sidecar_size_bytes",
                   std::to_string(validation.sidecar_size_bytes)},
                  {"model_sha256", validation.model_sha256}});

  if (!validation.valid) {
    telemetry.emit_memory_sample(config, sample, sample.telemetry_agreement);
    telemetry.emit(prisminfer::event::FailedClosed, config,
                   {{"failure_reason", validation.failure_reason}});
    telemetry.emit(prisminfer::event::RunEnd, config,
                   {{"status", "failed_closed"}});
    return finish_probe(config, sample, cuda, kv_profile, kv_sample, quality,
                        compression, offload_plan, transfer, profitability,
                        "failed_closed", validation.failure_reason,
                        prisminfer::ExitCode::FailedClosed);
  }

  const auto backend_plan = prisminfer::make_backend_plan(config);
  telemetry.emit(prisminfer::event::BackendSelected, config,
                 {{"backend", backend_plan.backend_name},
                  {"backend_version", backend_plan.backend_version},
                  {"backend_contract_version",
                   prisminfer::kBackendContractVersion}});
  if (config.backend == prisminfer::BackendKind::Llama &&
      config.dependency_pin_file.empty()) {
    const std::string reason = "dependency_pins_required";
    telemetry.emit_memory_sample(config, sample, sample.telemetry_agreement);
    telemetry.emit(prisminfer::event::FailedClosed, config,
                   {{"failure_reason", reason}});
    telemetry.emit(prisminfer::event::RunEnd, config,
                   {{"status", "failed_closed"}});
    return finish_probe(config, sample, cuda, kv_profile, kv_sample, quality,
                        compression, offload_plan, transfer, profitability,
                        "failed_closed", reason,
                        prisminfer::ExitCode::FailedClosed);
  }
  telemetry.emit(prisminfer::event::DependencyPinsResolved, config,
                 {{"dependency_pin_file",
                   config.dependency_pin_file.generic_string()},
                  {"required",
                   config.backend == prisminfer::BackendKind::Llama ? "true"
                                                                    : "false"}});

  if (prisminfer::gpu_requested(config.mode)) {
    cuda = prisminfer::probe_cuda_context();
    telemetry.emit(prisminfer::event::CudaContextProbe, config,
                   {{"available", cuda.available ? "true" : "false"},
                    {"context_created",
                     cuda.context_created ? "true" : "false"},
                    {"context_bytes", std::to_string(cuda.context_bytes)},
                    {"device_used_before_bytes",
                     std::to_string(cuda.device_used_before_bytes)},
                    {"device_used_after_bytes",
                     std::to_string(cuda.device_used_after_bytes)},
                    {"device_total_bytes",
                     std::to_string(cuda.device_total_bytes)},
                    {"driver_version", std::to_string(cuda.driver_version)},
                    {"runtime_version", std::to_string(cuda.runtime_version)},
                    {"gpu_name", cuda.gpu_name},
                    {"failure_reason", cuda.failure_reason}});

    if (!cuda.available || !cuda.context_created) {
      sample.required_telemetry_present = false;
      apply_simulated_breaches(config, &allocator, &sample);
      telemetry.emit_memory_sample(config, sample, false);
      telemetry.emit(prisminfer::event::FailedClosed, config,
                     {{"failure_reason", cuda.failure_reason}});
      telemetry.emit(prisminfer::event::RunEnd, config,
                     {{"status", "failed_closed"}});
      return finish_probe(config, sample, cuda, kv_profile, kv_sample, quality,
                          compression, offload_plan, transfer, profitability,
                          "failed_closed", cuda.failure_reason,
                          prisminfer::ExitCode::FailedClosed);
    }

    const auto allocation = allocator.reserve(cuda.context_bytes);
    if (!allocation.accepted) {
      allocator.observe_rejected_peak(cuda.context_bytes);
    }
    const auto allocator_sample = allocator.sample();
    sample.allocator_bytes = allocator_sample.allocator_bytes;
    sample.allocator_peak_bytes = allocator_sample.allocator_peak_bytes;
    sample.process_gpu_bytes = cuda.context_bytes;
    sample.process_gpu_peak_bytes = cuda.context_bytes;
    sample.warmup_peak_bytes = cuda.context_bytes;
    sample.device_used_bytes = cuda.device_used_after_bytes;
    sample.device_delta_bytes = cuda.context_bytes;
  }

  telemetry.emit(prisminfer::event::CapSemanticsResolved, config);
  telemetry.emit(prisminfer::event::HostPrepare, config);
  telemetry.emit(prisminfer::event::ModelLoadPlan, config,
                 {{"backend", backend_plan.backend_name},
                  {"model_path", backend_plan.model_path.generic_string()},
                  {"context_tokens",
                   std::to_string(backend_plan.requested_context_tokens)},
                  {"gpu_layers", std::to_string(backend_plan.gpu_layers)},
                  {"mmap_enabled",
                   backend_plan.mmap_enabled ? "true" : "false"}});
  auto backend = prisminfer::make_backend_adapter(config.backend);
  if (backend == nullptr) {
    const std::string reason =
        config.backend == prisminfer::BackendKind::Llama
            ? "llama_backend_not_compiled"
            : "backend_adapter_unavailable";
    telemetry.emit(prisminfer::event::BackendInit, config,
                   {{"backend", backend_plan.backend_name},
                    {"backend_version", backend_plan.backend_version},
                    {"status", "failed"},
                    {"failure_reason", reason}});
    telemetry.emit(prisminfer::event::BackendWarmup, config,
                   {{"status", "failed"},
                    {"backend", backend_plan.backend_name},
                    {"backend_owned_peak_bytes", "0"},
                    {"backend_external_peak_bytes", "0"},
                    {"retained_pool_bytes", "0"},
                    {"evidence_status", "adapter_unavailable"},
                    {"failure_reason", reason}});
    telemetry.emit_memory_sample(config, sample, sample.telemetry_agreement);
    telemetry.emit(prisminfer::event::FailedClosed, config,
                   {{"failure_reason", reason}});
    telemetry.emit(prisminfer::event::RunEnd, config,
                   {{"status", "failed_closed"}});
    return finish_probe(config, sample, cuda, kv_profile, kv_sample, quality,
                        compression, offload_plan, transfer, profitability,
                        "failed_closed", reason,
                        prisminfer::ExitCode::FailedClosed);
  }
  telemetry.emit(prisminfer::event::BackendInit, config,
                 {{"backend", backend->name()},
                  {"backend_version", backend->version()}});
  const auto warmup = backend->warmup(config, allocator);
  if (warmup.memory_sample.allocator_peak_bytes > sample.allocator_peak_bytes) {
    sample.allocator_bytes = warmup.memory_sample.allocator_bytes;
    sample.allocator_peak_bytes = warmup.memory_sample.allocator_peak_bytes;
  }
  sample.retained_pool_bytes = warmup.retained_pool_bytes;
  sample.unknown_post_warmup_bytes += warmup.backend_external_peak_bytes;
  if (sample.warmup_peak_bytes < warmup.backend_owned_peak_bytes) {
    sample.warmup_peak_bytes = warmup.backend_owned_peak_bytes;
  }
  const auto observation =
      prisminfer::observe_backend_memory(allocator, warmup);
  prisminfer::MemoryLedger ledger;
  ledger.record_backend_observation(observation);
  telemetry.emit(prisminfer::event::BackendWarmup, config,
                 {{"status", warmup.ok ? "ok" : "failed"},
                  {"backend", backend->name()},
                  {"backend_owned_peak_bytes",
                   std::to_string(warmup.backend_owned_peak_bytes)},
                  {"backend_external_peak_bytes",
                   std::to_string(warmup.backend_external_peak_bytes)},
                  {"retained_pool_bytes",
                   std::to_string(warmup.retained_pool_bytes)},
                  {"evidence_status", observation.evidence_status},
                  {"failure_reason", warmup.failure_reason}});
  if (!warmup.ok) {
    telemetry.emit_memory_sample(config, sample, sample.telemetry_agreement);
    telemetry.emit(prisminfer::event::FailedClosed, config,
                   {{"failure_reason", warmup.failure_reason}});
    telemetry.emit(prisminfer::event::RunEnd, config,
                   {{"status", "failed_closed"}});
    return finish_probe(config, sample, cuda, kv_profile, kv_sample, quality,
                        compression, offload_plan, transfer, profitability,
                        "failed_closed", warmup.failure_reason,
                        prisminfer::ExitCode::FailedClosed);
  }

  if (config.kv_accounting) {
    auto profile_result =
        warmup.kv_profile_available
            ? prisminfer::KvCacheLedgerResult{true, ""}
            : prisminfer::make_kv_cache_profile(
                  config.kv_layer_count, config.kv_head_count,
                  config.kv_head_dim, config.kv_block_tokens, 16, 16,
                  &kv_profile);
    if (warmup.kv_profile_available) {
      kv_profile = warmup.kv_profile;
    }
    if (!profile_result.ok) {
      telemetry.emit_memory_sample(config, sample, sample.telemetry_agreement);
      telemetry.emit(prisminfer::event::FailedClosed, config,
                     {{"failure_reason", profile_result.failure_reason}});
      telemetry.emit(prisminfer::event::RunEnd, config,
                     {{"status", "failed_closed"}});
      return finish_probe(config, sample, cuda, kv_profile, kv_sample, quality,
                          compression, offload_plan, transfer, profitability,
                          "failed_closed",
                          profile_result.failure_reason,
                          prisminfer::ExitCode::FailedClosed);
    }
    telemetry.emit(prisminfer::event::KvCacheProfile, config,
                   {{"layer_count", std::to_string(kv_profile.layer_count)},
                    {"kv_head_count",
                     std::to_string(kv_profile.kv_head_count)},
                    {"head_dim", std::to_string(kv_profile.head_dim)},
                    {"block_tokens", std::to_string(kv_profile.block_tokens)},
                    {"key_bits", std::to_string(kv_profile.key_bits)},
                    {"value_bits", std::to_string(kv_profile.value_bits)},
                    {"bytes_per_token",
                     std::to_string(kv_profile.bytes_per_token)},
                    {"bytes_per_block",
                     std::to_string(kv_profile.bytes_per_block)}});
    telemetry.emit(prisminfer::event::PrefillStart, config);
    kv_sample = make_kv_sample(config, kv_profile);
    prisminfer::KvCacheLedger kv_ledger;
    const auto record_result = kv_ledger.record_profile(kv_profile);
    const auto sample_result =
        record_result.ok
            ? kv_ledger.sample(kv_sample, config.kv_metadata_budget_bytes)
            : record_result;
    telemetry.emit(prisminfer::event::KvCacheSample, config,
                   {{"logical_tokens",
                     std::to_string(kv_sample.logical_tokens)},
                    {"active_blocks",
                     std::to_string(kv_sample.active_blocks)},
                    {"gpu_bytes", std::to_string(kv_sample.gpu_bytes)},
                    {"host_bytes", std::to_string(kv_sample.host_bytes)},
                    {"compressed_bytes",
                     std::to_string(kv_sample.compressed_bytes)},
                    {"metadata_bytes",
                     std::to_string(kv_sample.metadata_bytes)},
                    {"unknown_bytes",
                     std::to_string(kv_sample.unknown_bytes)},
                    {"evidence_status", kv_sample.evidence_status},
                    {"failure_reason", sample_result.failure_reason}});
    telemetry.emit(prisminfer::event::PrefillEnd, config);
    telemetry.emit(prisminfer::event::DecodeStart, config);
    for (std::uint64_t token = 0; token < config.warmup_tokens; ++token) {
      telemetry.emit(prisminfer::event::DecodeToken, config,
                     {{"token_index", std::to_string(token)}});
    }
    telemetry.emit(prisminfer::event::DecodeEnd, config);
    apply_kv_sample_to_memory(kv_sample, &sample);

    quality = prisminfer::evaluate_quality_gate(
        prisminfer::QualityGateInput{config.quality_gate,
                                     config.quality_baseline_score,
                                     config.quality_observed_score,
                                     config.quality_max_delta,
                                     config.quality_deterministic_match,
                                     config.quality_retrieval_passed});
    const std::uint64_t baseline_kv_bytes =
        kv_profile.bytes_per_token * kv_sample.logical_tokens;
    compression = prisminfer::evaluate_kv_compression_claim(
        prisminfer::KvCompressionInput{config.kv_compression,
                                       baseline_kv_bytes,
                                       kv_sample.compressed_bytes,
                                       kv_sample.metadata_bytes,
                                       quality});
    telemetry.emit(prisminfer::event::QualityGateResult, config,
                   {{"status", quality.status},
                    {"passed", quality.passed ? "true" : "false"},
                    {"failure_reason", quality.failure_reason},
                    {"observed_delta",
                     std::to_string(quality.observed_delta)},
                    {"compression_status", compression.status},
                    {"compression_accepted",
                     compression.accepted ? "true" : "false"},
                    {"compression_failure_reason",
                     compression.failure_reason},
                    {"kv_saved_bytes",
                     std::to_string(compression.saved_bytes)}});
    if (!sample_result.ok || !quality.passed || !compression.accepted) {
      const std::string reason =
          !sample_result.ok
              ? sample_result.failure_reason
              : (!quality.passed ? quality.failure_reason
                                 : compression.failure_reason);
      telemetry.emit_memory_sample(config, sample, sample.telemetry_agreement);
      telemetry.emit(prisminfer::event::FailedClosed, config,
                     {{"failure_reason", reason}});
      telemetry.emit(prisminfer::event::RunEnd, config,
                     {{"status", "failed_closed"}});
      return finish_probe(config, sample, cuda, kv_profile, kv_sample, quality,
                          compression, offload_plan, transfer, profitability,
                          "failed_closed", reason,
                          prisminfer::ExitCode::FailedClosed);
    }
  }
  if (config.profitability_policy != "off") {
    telemetry.emit(prisminfer::event::BaselineSelected, config,
                   {{"baseline_manifest",
                     config.baseline_manifest.generic_string()},
                    {"cpu_time_to_first_token_ms",
                     std::to_string(config.cpu_baseline_ttft_ms)},
                    {"cpu_decode_tokens_per_second",
                     std::to_string(config.cpu_baseline_decode_tps)},
                    {"cpu_peak_bytes",
                     std::to_string(config.cpu_baseline_peak_bytes)}});

    const auto plan_result = prisminfer::make_offload_plan(
        config.offload_policy, config.pinned_host_budget_bytes,
        config.staging_buffer_bytes, config.transfer_h2d_bytes,
        config.transfer_d2h_bytes, config.nvme_read_bytes,
        config.nvme_write_bytes, config.cold_cache_run);
    offload_plan = plan_result.plan;
    telemetry.emit(prisminfer::event::TransferPlan, config,
                   {{"policy", offload_plan.policy},
                    {"h2d_bytes", std::to_string(offload_plan.h2d_bytes)},
                    {"d2h_bytes", std::to_string(offload_plan.d2h_bytes)},
                    {"nvme_read_bytes",
                     std::to_string(offload_plan.nvme_read_bytes)},
                    {"nvme_write_bytes",
                     std::to_string(offload_plan.nvme_write_bytes)},
                    {"pinned_host_peak_bytes",
                     std::to_string(offload_plan.pinned_host_peak_bytes)},
                    {"staging_peak_bytes",
                     std::to_string(offload_plan.staging_peak_bytes)},
                    {"cold_cache_required",
                     offload_plan.cold_cache_required ? "true" : "false"},
                    {"failure_reason", plan_result.failure_reason}});
    transfer = make_transfer_sample(config, offload_plan);
    const auto transfer_result = prisminfer::validate_transfer_sample(
        config.offload_policy, transfer, config.transfer_metrics,
        config.cold_cache_run);
    telemetry.emit(prisminfer::event::TransferSample, config,
                   {{"h2d_bytes", std::to_string(transfer.h2d_bytes)},
                    {"d2h_bytes", std::to_string(transfer.d2h_bytes)},
                    {"nvme_read_bytes",
                     std::to_string(transfer.nvme_read_bytes)},
                    {"nvme_write_bytes",
                     std::to_string(transfer.nvme_write_bytes)},
                    {"h2d_ms", std::to_string(transfer.h2d_ms)},
                    {"d2h_ms", std::to_string(transfer.d2h_ms)},
                    {"io_ms", std::to_string(transfer.io_ms)},
                    {"wait_ms", std::to_string(transfer.wait_ms)},
                    {"prefill_ms", std::to_string(transfer.prefill_ms)},
                    {"decode_ms", std::to_string(transfer.decode_ms)},
                    {"time_to_first_token_ms",
                     std::to_string(transfer.time_to_first_token_ms)},
                    {"decode_tokens_per_second",
                     std::to_string(transfer.decode_tokens_per_second)},
                    {"failure_reason", transfer_result.failure_reason}});
    telemetry.emit(prisminfer::event::OffloadSample, config,
                   {{"policy", offload_plan.policy},
                    {"evidence_label", offload_plan.evidence_label}});
    profitability = prisminfer::evaluate_profitability(
        prisminfer::ProfitabilityInput{
            config.profitability_policy,
            config.offload_policy,
            config.min_speedup_ratio,
            prisminfer::ProfitabilityBaseline{
                config.cpu_baseline_ttft_ms,
                config.cpu_baseline_decode_tps,
                config.cpu_baseline_peak_bytes,
                config.validation_cell_id},
            transfer,
            config.validation_cell_id,
            true,
            quality.passed,
            transfer_result.ok});
    std::string phase3_reason =
        !plan_result.ok
            ? plan_result.failure_reason
            : (!transfer_result.ok ? transfer_result.failure_reason
                                   : profitability.reason);
    telemetry.emit(prisminfer::event::ProfitabilityResult, config,
                   {{"status", profitability.status},
                    {"accepted", profitability.accepted ? "true" : "false"},
                    {"reason", phase3_reason},
                    {"speedup_ratio",
                     std::to_string(profitability.speedup_ratio)},
                    {"required_speedup_ratio",
                     std::to_string(profitability.required_speedup_ratio)}});
    if (!plan_result.ok || !transfer_result.ok || !profitability.accepted) {
      telemetry.emit_memory_sample(config, sample, sample.telemetry_agreement);
      telemetry.emit(prisminfer::event::FailedClosed, config,
                     {{"failure_reason", phase3_reason}});
      telemetry.emit(prisminfer::event::RunEnd, config,
                     {{"status", "failed_closed"}});
      return finish_probe(config, sample, cuda, kv_profile, kv_sample, quality,
                          compression, offload_plan, transfer, profitability,
                          "failed_closed", phase3_reason,
                          prisminfer::ExitCode::FailedClosed);
    }
  }
  apply_simulated_breaches(config, &allocator, &sample);
  telemetry.emit_memory_sample(config, sample, sample.telemetry_agreement);

  const auto certification =
      prisminfer::certify_cap(sample, config.hard_cap_bytes);
  telemetry.emit(prisminfer::event::CapCertificationResult, config,
                 {{"certified", certification.certified ? "true" : "false"},
                  {"failure_reason", certification.failure_reason}});

  if (!certification.certified) {
    telemetry.emit(prisminfer::event::FailedClosed, config,
                   {{"failure_reason", certification.failure_reason}});
    telemetry.emit(prisminfer::event::RunEnd, config,
                   {{"status", "failed_closed"}});
    return finish_probe(config, sample, cuda, kv_profile, kv_sample, quality,
                        compression, offload_plan, transfer, profitability,
                        "failed_closed",
                        certification.failure_reason,
                        prisminfer::ExitCode::FailedClosed);
  }

  telemetry.emit(prisminfer::event::Completed, config);
  telemetry.emit(prisminfer::event::RunEnd, config, {{"status", "ok"}});
  return finish_probe(config, sample, cuda, kv_profile, kv_sample, quality,
                      compression, offload_plan, transfer, profitability, "ok", "",
                      prisminfer::ExitCode::Ok);
}

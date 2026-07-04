#include <iostream>
#include <string>
#include <vector>

#include "prisminfer/allocator_tracker.h"
#include "prisminfer/benchmark.h"
#include "prisminfer/config.h"
#include "prisminfer/cuda_context_probe.h"
#include "prisminfer/errors.h"
#include "prisminfer/memory_cap.h"
#include "prisminfer/model_sidecar.h"
#include "prisminfer/runtime_state.h"
#include "prisminfer/telemetry.h"

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

int finish_probe(const prisminfer::RuntimeConfig& config,
                 const prisminfer::MemorySample& sample,
                 const prisminfer::CudaProbeResult& cuda,
                 const std::string& status,
                 const std::string& reason,
                 prisminfer::ExitCode exit_code) {
  std::string manifest_error;
  const prisminfer::ManifestInputs manifest{
      config, sample, cuda, status, reason};
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
    return finish_probe(config, sample, cuda, "failed_closed",
                        validation.failure_reason,
                        prisminfer::ExitCode::FailedClosed);
  }

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
      return finish_probe(config, sample, cuda, "failed_closed",
                          cuda.failure_reason,
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
  telemetry.emit(prisminfer::event::BackendWarmup, config,
                 {{"status", "placeholder"},
                  {"llama_backend_attached", "false"},
                  {"warmup_peak_bytes", std::to_string(sample.warmup_peak_bytes)}});
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
    return finish_probe(config, sample, cuda, "failed_closed",
                        certification.failure_reason,
                        prisminfer::ExitCode::FailedClosed);
  }

  telemetry.emit(prisminfer::event::Completed, config);
  telemetry.emit(prisminfer::event::RunEnd, config, {{"status", "ok"}});
  return finish_probe(config, sample, cuda, "ok", "",
                      prisminfer::ExitCode::Ok);
}

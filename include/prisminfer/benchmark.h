#pragma once

#include <filesystem>
#include <string>

#include "prisminfer/config.h"
#include "prisminfer/cuda_context_probe.h"
#include "prisminfer/host_memory_tracker.h"
#include "prisminfer/kv_cache_ledger.h"
#include "prisminfer/kv_compression_policy.h"
#include "prisminfer/memory_cap.h"
#include "prisminfer/offload_planner.h"
#include "prisminfer/profitability_policy.h"
#include "prisminfer/quality_gate.h"
#include "prisminfer/transfer_metrics.h"

namespace prisminfer {

struct ManifestInputs {
  RuntimeConfig config;
  MemorySample sample;
  HostTelemetrySample host;
  CudaProbeResult cuda_probe;
  KvCacheProfile kv_profile;
  KvCacheSample kv_sample;
  QualityGateResult quality;
  KvCompressionResult compression;
  OffloadPlan offload_plan;
  TransferSample transfer;
  ProfitabilityDecision profitability;
  std::string status;
  std::string failure_reason;
};

bool write_probe_manifest(const std::filesystem::path& path,
                          const ManifestInputs& inputs,
                          std::string* error);

bool validate_phase0_lifecycle(const std::filesystem::path& telemetry_path,
                               std::string* error);

}  // namespace prisminfer

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace prisminfer {

struct TransferEvent {
  std::string transfer_event_id;
  std::string plan_entry_id;
  std::string direction;
  std::string source_allocation_id;
  std::string destination_allocation_id;
  std::string host_memory_kind;
  std::string stream_id;
  std::string copy_engine;
  std::string logical_object_id;
  std::uint64_t submitted_bytes{0};
  std::uint64_t completed_bytes{0};
  std::uint64_t submit_monotonic_nanoseconds{0};
  std::uint64_t start_monotonic_nanoseconds{0};
  std::uint64_t finish_monotonic_nanoseconds{0};
  std::uint64_t exposed_wait_nanoseconds{0};
  std::uint64_t overlap_nanoseconds{0};
  bool retry{false};
  bool fallback{false};
  bool partial{false};
};

struct TransferSample {
  // Plan/configuration values are never serialized as observed measurements.
  std::uint64_t declared_h2d_bytes{0};
  std::uint64_t declared_d2h_bytes{0};
  std::uint64_t declared_nvme_read_bytes{0};
  std::uint64_t declared_nvme_write_bytes{0};
  std::uint64_t pinned_host_budget_bytes{0};
  std::uint64_t staging_budget_bytes{0};
  double declared_h2d_ms{0.0};
  double declared_d2h_ms{0.0};
  double declared_io_ms{0.0};
  double declared_wait_ms{0.0};

  // These values are derived only from validated actual events.
  std::uint64_t observed_h2d_submitted_bytes{0};
  std::uint64_t observed_h2d_completed_bytes{0};
  std::uint64_t observed_d2h_submitted_bytes{0};
  std::uint64_t observed_d2h_completed_bytes{0};
  std::uint64_t observed_pinned_host_peak_bytes{0};
  std::uint64_t observed_staging_peak_bytes{0};
  std::string instrumentation_mode{"ordinary"};
  std::uint64_t dropped_records{0};
  std::vector<TransferEvent> events;

  // Request performance values remain separately named performance evidence.
  double prefill_ms{0.0};
  double decode_ms{0.0};
  double time_to_first_token_ms{0.0};
  double decode_tokens_per_second{0.0};
  double token_latency_p50_ms{0.0};
  double token_latency_p95_ms{0.0};
};

struct TransferValidationResult {
  bool ok{true};
  std::string failure_reason;
};

// Validates every event, performs checked aggregation, and populates only the
// observed totals. On failure all observed totals remain zero.
TransferValidationResult reconcile_transfer_sample(TransferSample* sample);

TransferValidationResult validate_transfer_sample(
    const std::string& offload_policy,
    const TransferSample& sample,
    bool transfer_metrics_enabled,
    bool cold_cache_run);

}  // namespace prisminfer

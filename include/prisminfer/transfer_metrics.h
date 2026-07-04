#pragma once

#include <cstdint>
#include <string>

namespace prisminfer {

struct TransferSample {
  std::uint64_t h2d_bytes{0};
  std::uint64_t d2h_bytes{0};
  std::uint64_t nvme_read_bytes{0};
  std::uint64_t nvme_write_bytes{0};
  std::uint64_t pinned_host_peak_bytes{0};
  std::uint64_t staging_peak_bytes{0};
  double h2d_ms{0.0};
  double d2h_ms{0.0};
  double io_ms{0.0};
  double wait_ms{0.0};
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

TransferValidationResult validate_transfer_sample(
    const std::string& offload_policy,
    const TransferSample& sample,
    bool transfer_metrics_enabled,
    bool cold_cache_run);

}  // namespace prisminfer


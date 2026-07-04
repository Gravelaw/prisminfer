#pragma once

#include <cstdint>
#include <string>

namespace prisminfer {

struct HybridPlan {
  std::string model_id;
  std::string validation_cell_id;
  std::string model_parameter_bucket{">85B-90B"};
  std::uint64_t parameter_count{90'000'000'000ULL};
  std::uint64_t hard_vram_cap_bytes{17'179'869'184ULL};
  std::uint64_t resident_weight_bytes{0};
  std::uint64_t weight_payload_bytes{0};
  std::uint64_t weight_metadata_bytes{0};
  std::uint64_t resident_kv_bytes{0};
  std::uint64_t workspace_bytes{0};
  std::uint64_t runtime_overhead_bytes{0};
  std::uint64_t safety_margin_bytes{0};
  std::uint64_t host_budget_bytes{0};
  std::uint64_t nvme_budget_bytes{0};
  double expected_time_to_first_token_ms{0.0};
  double expected_decode_tokens_per_second{0.0};
  std::string claim_label{"simulated"};
};

struct HybridPlanResult {
  bool ok{false};
  std::string failure_reason;
  std::uint64_t resident_gpu_total_bytes{0};
  HybridPlan plan;
};

HybridPlanResult validate_hybrid_plan(const HybridPlan& plan);
HybridPlan make_90b_simulated_plan(std::uint64_t hard_vram_cap_bytes);

}  // namespace prisminfer


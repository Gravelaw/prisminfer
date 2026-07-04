#include "prisminfer/hybrid_plan.h"

#include <limits>

namespace prisminfer {
namespace {

constexpr std::uint64_t kMaxGpuCapBytes = 17'179'869'184ULL;

bool add_u64(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t* out) {
  if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs) {
    return false;
  }
  *out = lhs + rhs;
  return true;
}

}  // namespace

HybridPlanResult validate_hybrid_plan(const HybridPlan& plan) {
  HybridPlanResult result;
  result.plan = plan;
  if (plan.hard_vram_cap_bytes == 0 ||
      plan.hard_vram_cap_bytes > kMaxGpuCapBytes) {
    result.failure_reason = "hard_vram_cap_out_of_scope";
    return result;
  }
  std::uint64_t total = 0;
  if (!add_u64(total, plan.resident_weight_bytes, &total) ||
      !add_u64(total, plan.weight_metadata_bytes, &total) ||
      !add_u64(total, plan.resident_kv_bytes, &total) ||
      !add_u64(total, plan.workspace_bytes, &total) ||
      !add_u64(total, plan.runtime_overhead_bytes, &total) ||
      !add_u64(total, plan.safety_margin_bytes, &total)) {
    result.failure_reason = "resident_gpu_total_overflow";
    return result;
  }
  result.resident_gpu_total_bytes = total;
  if (total > plan.hard_vram_cap_bytes) {
    result.failure_reason = "resident_gpu_budget_exceeded";
    return result;
  }
  result.ok = true;
  return result;
}

HybridPlan make_90b_simulated_plan(std::uint64_t hard_vram_cap_bytes) {
  HybridPlan plan;
  plan.model_id = "90b-hybrid-simulated";
  plan.validation_cell_id = "simulated-90b-under-16gib";
  plan.hard_vram_cap_bytes = hard_vram_cap_bytes;
  plan.resident_weight_bytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
  plan.weight_payload_bytes = 48ULL * 1024ULL * 1024ULL * 1024ULL;
  plan.weight_metadata_bytes = 512ULL * 1024ULL * 1024ULL;
  plan.resident_kv_bytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
  plan.workspace_bytes = 1024ULL * 1024ULL * 1024ULL;
  plan.runtime_overhead_bytes = 1024ULL * 1024ULL * 1024ULL;
  plan.safety_margin_bytes = 512ULL * 1024ULL * 1024ULL;
  plan.host_budget_bytes = 128ULL * 1024ULL * 1024ULL * 1024ULL;
  plan.nvme_budget_bytes = 512ULL * 1024ULL * 1024ULL * 1024ULL;
  plan.expected_time_to_first_token_ms = 0.0;
  plan.expected_decode_tokens_per_second = 0.0;
  plan.claim_label = "simulated";
  return plan;
}

}  // namespace prisminfer


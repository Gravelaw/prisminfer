#include "prisminfer/offload_planner.h"

namespace prisminfer {

bool valid_offload_policy(const std::string& policy) {
  return policy == "none" || policy == "gpu" || policy == "host-kv" ||
         policy == "nvme-simulated" || policy == "nvme-experimental";
}

OffloadPlanResult make_offload_plan(const std::string& policy,
                                    std::uint64_t pinned_host_budget_bytes,
                                    std::uint64_t staging_buffer_bytes,
                                    std::uint64_t h2d_bytes,
                                    std::uint64_t d2h_bytes,
                                    std::uint64_t nvme_read_bytes,
                                    std::uint64_t nvme_write_bytes,
                                    bool cold_cache_run) {
  if (!valid_offload_policy(policy)) {
    return {false, "offload_policy_invalid", {}};
  }

  OffloadPlan plan;
  plan.policy = policy;
  plan.h2d_bytes = h2d_bytes;
  plan.d2h_bytes = d2h_bytes;
  plan.nvme_read_bytes = nvme_read_bytes;
  plan.nvme_write_bytes = nvme_write_bytes;
  plan.pinned_host_peak_bytes = pinned_host_budget_bytes;
  plan.staging_peak_bytes = staging_buffer_bytes;
  plan.cold_cache_required =
      policy == "nvme-simulated" || policy == "nvme-experimental";
  plan.evidence_label = policy == "none" ? "no-offload" : "measured-offload";

  if ((policy == "host-kv" || policy == "nvme-simulated" ||
       policy == "nvme-experimental") &&
      pinned_host_budget_bytes == 0) {
    return {false, "pinned_host_budget_required", plan};
  }
  if (policy != "none" && staging_buffer_bytes == 0) {
    return {false, "staging_buffer_required", plan};
  }
  if (plan.cold_cache_required && !cold_cache_run) {
    return {false, "cold_cache_required_for_nvme", plan};
  }
  return {true, "", plan};
}

}  // namespace prisminfer


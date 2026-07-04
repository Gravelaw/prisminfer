#pragma once

#include <cstdint>
#include <string>

namespace prisminfer {

struct OffloadPlan {
  std::string policy{"none"};
  std::uint64_t h2d_bytes{0};
  std::uint64_t d2h_bytes{0};
  std::uint64_t nvme_read_bytes{0};
  std::uint64_t nvme_write_bytes{0};
  std::uint64_t pinned_host_peak_bytes{0};
  std::uint64_t staging_peak_bytes{0};
  bool cold_cache_required{false};
  std::string evidence_label{"no-offload"};
};

struct OffloadPlanResult {
  bool ok{true};
  std::string failure_reason;
  OffloadPlan plan;
};

bool valid_offload_policy(const std::string& policy);
OffloadPlanResult make_offload_plan(const std::string& policy,
                                    std::uint64_t pinned_host_budget_bytes,
                                    std::uint64_t staging_buffer_bytes,
                                    std::uint64_t h2d_bytes,
                                    std::uint64_t d2h_bytes,
                                    std::uint64_t nvme_read_bytes,
                                    std::uint64_t nvme_write_bytes,
                                    bool cold_cache_run);

}  // namespace prisminfer


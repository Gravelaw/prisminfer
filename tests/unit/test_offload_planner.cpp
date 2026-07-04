#include <iostream>

#include "prisminfer/offload_planner.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

}  // namespace

int main() {
  const auto none =
      prisminfer::make_offload_plan("none", 0, 0, 0, 0, 0, 0, false);
  if (expect(none.ok, "none policy accepted")) return 1;
  if (expect(none.plan.evidence_label == "no-offload",
             "none evidence label")) return 1;

  const auto host =
      prisminfer::make_offload_plan("host-kv", 1024, 2048, 512, 0, 0, 0,
                                    false);
  if (expect(host.ok, "host kv policy accepted")) return 1;

  const auto missing_pinned =
      prisminfer::make_offload_plan("host-kv", 0, 2048, 512, 0, 0, 0, false);
  if (expect(!missing_pinned.ok, "missing pinned budget rejected")) return 1;
  if (expect(missing_pinned.failure_reason == "pinned_host_budget_required",
             "missing pinned reason")) return 1;

  const auto nvme =
      prisminfer::make_offload_plan("nvme-simulated", 1024, 2048, 0, 0, 4096,
                                    0, false);
  if (expect(!nvme.ok, "nvme without cold cache rejected")) return 1;
  if (expect(nvme.failure_reason == "cold_cache_required_for_nvme",
             "nvme reason")) return 1;
  return 0;
}


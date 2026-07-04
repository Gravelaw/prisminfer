#include <iostream>

#include "prisminfer/hybrid_plan.h"

namespace {
int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}
}  // namespace

int main() {
  auto plan = prisminfer::make_90b_simulated_plan(17179869184ULL);
  const auto ok = prisminfer::validate_hybrid_plan(plan);
  if (expect(ok.ok, "simulated 90b plan fits declared resident budget")) {
    return 1;
  }
  plan.hard_vram_cap_bytes = 17179869185ULL;
  const auto cap = prisminfer::validate_hybrid_plan(plan);
  if (expect(!cap.ok, "cap above 16 GiB rejected")) return 1;
  if (expect(cap.failure_reason == "hard_vram_cap_out_of_scope",
             "cap rejection reason")) return 1;
  plan.hard_vram_cap_bytes = 1024;
  const auto budget = prisminfer::validate_hybrid_plan(plan);
  if (expect(!budget.ok, "resident budget breach rejected")) return 1;
  if (expect(budget.failure_reason == "resident_gpu_budget_exceeded",
             "budget rejection reason")) return 1;
  return 0;
}


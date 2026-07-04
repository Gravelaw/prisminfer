#include <iostream>
#include <string>
#include <vector>

#include "prisminfer/errors.h"
#include "prisminfer/hybrid_plan.h"
#include "prisminfer/telemetry.h"

namespace {

std::uint64_t parse_u64_or_default(const std::vector<std::string>& args,
                                   const std::string& flag,
                                   std::uint64_t fallback) {
  for (std::size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == flag) {
      try {
        return std::stoull(args[i + 1]);
      } catch (...) {
        return fallback;
      }
    }
  }
  return fallback;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  const std::uint64_t cap =
      parse_u64_or_default(args, "--hard-cap-bytes", 17179869184ULL);
  const auto plan = prisminfer::make_90b_simulated_plan(cap);
  const auto result = prisminfer::validate_hybrid_plan(plan);
  std::cout << "{"
            << "\"claim_label\":\"simulated\","
            << "\"deployable\":false,"
            << "\"model_id\":\"" << prisminfer::json_escape(plan.model_id)
            << "\","
            << "\"validation_cell_id\":\""
            << prisminfer::json_escape(plan.validation_cell_id) << "\","
            << "\"hard_vram_cap_bytes\":" << plan.hard_vram_cap_bytes << ","
            << "\"resident_gpu_total_bytes\":"
            << result.resident_gpu_total_bytes << ","
            << "\"ok\":" << (result.ok ? "true" : "false") << ","
            << "\"failure_reason\":\""
            << prisminfer::json_escape(result.failure_reason) << "\""
            << "}\n";
  return result.ok ? static_cast<int>(prisminfer::ExitCode::Ok)
                   : static_cast<int>(prisminfer::ExitCode::FailedClosed);
}


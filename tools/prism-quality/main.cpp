#include <iostream>
#include <string>
#include <vector>

#include "prisminfer/errors.h"
#include "prisminfer/quality_gate.h"
#include "prisminfer/telemetry.h"

namespace {

std::string value_after(const std::vector<std::string>& args,
                        const std::string& flag,
                        const std::string& fallback) {
  for (std::size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == flag) {
      return args[i + 1];
    }
  }
  return fallback;
}

bool flag_bool(const std::vector<std::string>& args,
               const std::string& flag,
               bool fallback) {
  const auto value = value_after(args, flag, fallback ? "true" : "false");
  return value == "true";
}

double flag_double(const std::vector<std::string>& args,
                   const std::string& flag,
                   double fallback) {
  const auto value = value_after(args, flag, "");
  if (value.empty()) {
    return fallback;
  }
  try {
    return std::stod(value);
  } catch (...) {
    return fallback;
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  if (args.empty() || value_after(args, "--help", "") == "true") {
    std::cout << "Usage: prism-quality --quality-gate none|smoke|retrieval|long-context "
                 "[--baseline-score N] [--observed-score N] "
                 "[--max-delta N] [--retrieval-passed true|false] "
                 "[--deterministic-match true|false]\n";
    return static_cast<int>(prisminfer::ExitCode::Ok);
  }

  const prisminfer::QualityGateInput input{
      value_after(args, "--quality-gate", "none"),
      flag_double(args, "--baseline-score", 1.0),
      flag_double(args, "--observed-score", 1.0),
      flag_double(args, "--max-delta", 0.0),
      flag_bool(args, "--deterministic-match", true),
      flag_bool(args, "--retrieval-passed", true)};
  const auto result = prisminfer::evaluate_quality_gate(input);
  std::cout << "{\"status\":\"" << prisminfer::json_escape(result.status)
            << "\",\"passed\":" << (result.passed ? "true" : "false")
            << ",\"failure_reason\":\""
            << prisminfer::json_escape(result.failure_reason)
            << "\",\"observed_delta\":" << result.observed_delta << "}\n";
  return result.passed ? static_cast<int>(prisminfer::ExitCode::Ok)
                       : static_cast<int>(prisminfer::ExitCode::FailedClosed);
}

#include <iostream>
#include <string>
#include <vector>

#include "prisminfer/claim_taxonomy.h"
#include "prisminfer/errors.h"
#include "prisminfer/telemetry.h"

namespace {

bool has_flag(const std::vector<std::string>& args, const std::string& flag) {
  for (const auto& arg : args) {
    if (arg == flag) return true;
  }
  return false;
}

std::string value_of(const std::vector<std::string>& args,
                     const std::string& flag) {
  for (std::size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == flag) return args[i + 1];
  }
  return "";
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  prisminfer::ClaimEvidence evidence;
  evidence.claim_label = value_of(args, "--claim-label");
  evidence.validation_cell_id = value_of(args, "--validation-cell-id");
  evidence.model_hash = value_of(args, "--model-hash");
  evidence.quantization_format = value_of(args, "--quantization-format");
  evidence.quant_artifact_sha256 = value_of(args, "--quant-artifact-sha256");
  evidence.simulated_evidence = has_flag(args, "--simulated-evidence");
  evidence.measured_evidence = has_flag(args, "--measured-evidence");
  evidence.cap_passed = has_flag(args, "--cap-passed");
  evidence.quality_passed = has_flag(args, "--quality-passed");
  evidence.profitability_passed = has_flag(args, "--profitability-passed");
  evidence.usability_passed = has_flag(args, "--usability-passed");
  evidence.repeatability_passed = has_flag(args, "--repeatability-passed");
  evidence.runbook_present = has_flag(args, "--runbook-present");

  const auto decision = prisminfer::validate_claim_evidence(evidence);
  std::cout << "{\"accepted\":" << (decision.accepted ? "true" : "false")
            << ",\"status\":\"" << prisminfer::json_escape(decision.status)
            << "\",\"reason\":\"" << prisminfer::json_escape(decision.reason)
            << "\"}\n";
  return decision.accepted ? static_cast<int>(prisminfer::ExitCode::Ok)
                           : static_cast<int>(prisminfer::ExitCode::FailedClosed);
}


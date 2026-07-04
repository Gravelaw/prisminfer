#pragma once

#include <cstdint>
#include <string>

#include "prisminfer/quality_gate.h"

namespace prisminfer {

struct KvCompressionInput {
  std::string policy{"none"};
  std::uint64_t baseline_bytes{0};
  std::uint64_t compressed_bytes{0};
  std::uint64_t metadata_bytes{0};
  QualityGateResult quality;
};

struct KvCompressionResult {
  bool accepted{true};
  std::string status{"none"};
  std::string failure_reason;
  std::uint64_t saved_bytes{0};
};

bool valid_kv_compression_policy(const std::string& policy);
KvCompressionResult evaluate_kv_compression_claim(
    const KvCompressionInput& input);

}  // namespace prisminfer

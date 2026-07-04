#pragma once

#include <cstdint>
#include <string>

#include "prisminfer/claim_taxonomy.h"

namespace prisminfer {

struct EvidenceBundle {
  std::string claim_label{"metadata-only"};
  std::string validation_cell_id;
  std::uint64_t hard_vram_cap_bytes{0};
  bool manifest_present{false};
  bool telemetry_present{false};
  bool model_hash_present{false};
  bool quant_hash_present{false};
  bool quality_present{false};
  bool profitability_present{false};
  bool repeatability_present{false};
  bool runbook_present{false};
  bool simulated_evidence{false};
  bool measured_evidence{false};
};

ClaimDecision validate_evidence_bundle(const EvidenceBundle& bundle);

}  // namespace prisminfer


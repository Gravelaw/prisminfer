#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace prisminfer {

using AdmissionDigest = std::array<std::uint8_t, 32>;

struct AdmissionCellIdentity {
  std::uint64_t run_sequence{0};
  AdmissionDigest run_contract_hash{};
  AdmissionDigest threshold_registry_hash{};
  AdmissionDigest hardware_identity_hash{};
  AdmissionDigest runtime_identity_hash{};
  AdmissionDigest artifact_identity_hash{};
  AdmissionDigest service_profile_hash{};

  bool operator==(const AdmissionCellIdentity&) const = default;
};

inline bool admission_cell_identity_valid(const AdmissionCellIdentity& cell) {
  const auto nonzero = [](const AdmissionDigest& digest) {
    return std::any_of(digest.begin(), digest.end(),
                       [](std::uint8_t byte) { return byte != 0; });
  };
  return cell.run_sequence != 0 && nonzero(cell.run_contract_hash) &&
         nonzero(cell.threshold_registry_hash) &&
         nonzero(cell.hardware_identity_hash) &&
         nonzero(cell.runtime_identity_hash) &&
         nonzero(cell.artifact_identity_hash) &&
         nonzero(cell.service_profile_hash);
}

}  // namespace prisminfer

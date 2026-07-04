#include "prisminfer/memory_ledger.h"

namespace prisminfer {

void MemoryLedger::record_backend_observation(
    const BackendMemoryObservation& observation) {
  snapshot_.prisminfer_owned_bytes = observation.prisminfer_owned_bytes;
  snapshot_.backend_owned_bytes = observation.backend_owned_bytes;
  snapshot_.retained_pool_bytes = observation.retained_pool_bytes;
  snapshot_.unknown_bytes = observation.unknown_bytes;
}

MemoryLedgerSnapshot MemoryLedger::snapshot() const {
  return snapshot_;
}

bool MemoryLedger::certifiable() const {
  return snapshot_.unknown_bytes == 0;
}

}  // namespace prisminfer

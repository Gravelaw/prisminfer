#pragma once

#include <cstdint>

#include "prisminfer/backend_memory_observer.h"

namespace prisminfer {

struct MemoryLedgerSnapshot {
  std::uint64_t prisminfer_owned_bytes{0};
  std::uint64_t backend_owned_bytes{0};
  std::uint64_t retained_pool_bytes{0};
  std::uint64_t unknown_bytes{0};
};

class MemoryLedger {
 public:
  void record_backend_observation(
      const BackendMemoryObservation& observation);
  MemoryLedgerSnapshot snapshot() const;
  bool certifiable() const;

 private:
  MemoryLedgerSnapshot snapshot_;
};

}  // namespace prisminfer

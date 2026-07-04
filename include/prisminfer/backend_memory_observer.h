#pragma once

#include <cstdint>
#include <string>

#include "prisminfer/allocator_tracker.h"
#include "prisminfer/backend.h"

namespace prisminfer {

struct BackendMemoryObservation {
  std::uint64_t prisminfer_owned_bytes{0};
  std::uint64_t backend_owned_bytes{0};
  std::uint64_t cuda_context_bytes{0};
  std::uint64_t kv_estimated_bytes{0};
  std::uint64_t workspace_estimated_bytes{0};
  std::uint64_t retained_pool_bytes{0};
  std::uint64_t unknown_bytes{0};
  std::string evidence_status{"unknown"};
};

BackendMemoryObservation observe_backend_memory(
    const CappedAllocatorTracker& allocator,
    const BackendWarmupResult& warmup);

}  // namespace prisminfer

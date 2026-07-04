#include "prisminfer/backend_memory_observer.h"

namespace prisminfer {

BackendMemoryObservation observe_backend_memory(
    const CappedAllocatorTracker& allocator,
    const BackendWarmupResult& warmup) {
  BackendMemoryObservation observation;
  observation.prisminfer_owned_bytes = allocator.current_bytes();
  observation.backend_owned_bytes = warmup.backend_owned_peak_bytes;
  observation.retained_pool_bytes = warmup.retained_pool_bytes;
  observation.unknown_bytes = warmup.backend_external_peak_bytes;
  observation.evidence_status =
      observation.unknown_bytes == 0 ? "observed" : "unknown";
  return observation;
}

}  // namespace prisminfer

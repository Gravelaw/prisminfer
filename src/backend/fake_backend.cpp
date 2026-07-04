#include "prisminfer/backend.h"

namespace prisminfer {
namespace {

class FakeBackendAdapter final : public BackendAdapter {
 public:
  std::string name() const override { return "fake"; }
  std::string version() const override { return kBackendContractVersion; }

  BackendWarmupResult warmup(const RuntimeConfig& config,
                             CappedAllocatorTracker& allocator) override {
    BackendWarmupResult result;
    const auto allocation = allocator.reserve(config.warmup_tokens);
    if (!allocation.accepted) {
      result.ok = false;
      result.failure_reason = allocation.failure_reason;
      allocator.observe_rejected_peak(allocator.current_bytes() +
                                      config.warmup_tokens);
    }
    result.memory_sample = allocator.sample();
    result.backend_owned_peak_bytes = result.memory_sample.allocator_peak_bytes;
    result.retained_pool_bytes = result.memory_sample.allocator_bytes;
    return result;
  }
};

}  // namespace

std::unique_ptr<BackendAdapter> make_fake_backend_adapter() {
  return std::make_unique<FakeBackendAdapter>();
}

}  // namespace prisminfer

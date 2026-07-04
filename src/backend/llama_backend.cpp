#include "prisminfer/backend.h"

namespace prisminfer {
namespace {

class LlamaBackendAdapter final : public BackendAdapter {
 public:
  std::string name() const override { return "llama"; }
  std::string version() const override { return kBackendContractVersion; }

  BackendWarmupResult warmup(const RuntimeConfig&,
                             CappedAllocatorTracker& allocator) override {
    BackendWarmupResult result;
    result.ok = false;
    result.failure_reason = "llama_backend_not_implemented";
    result.memory_sample = allocator.sample();
    return result;
  }
};

}  // namespace

std::unique_ptr<BackendAdapter> make_llama_backend_adapter() {
  return std::make_unique<LlamaBackendAdapter>();
}

}  // namespace prisminfer

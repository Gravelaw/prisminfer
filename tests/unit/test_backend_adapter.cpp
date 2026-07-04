#include <iostream>

#include "prisminfer/backend.h"
#include "prisminfer/backend_memory_observer.h"
#include "prisminfer/memory_ledger.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

}  // namespace

int main() {
  {
    prisminfer::RuntimeConfig config;
    config.backend = prisminfer::BackendKind::Null;
    config.context_tokens = 128;
    const auto plan = prisminfer::make_backend_plan(config);
    if (expect(plan.backend_name == "null", "null backend plan name")) {
      return 1;
    }
    if (expect(plan.requested_context_tokens == 128,
               "context token plan field")) {
      return 1;
    }
    auto backend = prisminfer::make_backend_adapter(config.backend);
    if (expect(backend != nullptr, "null backend factory")) return 1;
    prisminfer::CappedAllocatorTracker allocator(config.hard_cap_bytes);
    const auto warmup = backend->warmup(config, allocator);
    if (expect(warmup.ok, "null backend warmup succeeds")) return 1;
    if (expect(warmup.memory_sample.allocator_peak_bytes == 0,
               "null backend does not allocate")) {
      return 1;
    }
  }
  {
    prisminfer::RuntimeConfig config;
    config.backend = prisminfer::BackendKind::Fake;
    config.hard_cap_bytes = 4;
    config.warmup_tokens = 3;
    auto backend = prisminfer::make_backend_adapter(config.backend);
    if (expect(backend != nullptr, "fake backend factory")) return 1;
    prisminfer::CappedAllocatorTracker allocator(config.hard_cap_bytes);
    const auto warmup = backend->warmup(config, allocator);
    if (expect(warmup.ok, "fake backend warmup succeeds under cap")) return 1;
    if (expect(warmup.backend_owned_peak_bytes == 3,
               "fake backend records owned peak")) {
      return 1;
    }
    const auto observation =
        prisminfer::observe_backend_memory(allocator, warmup);
    if (expect(observation.evidence_status == "observed",
               "observed backend memory has no unknown bytes")) {
      return 1;
    }
    prisminfer::MemoryLedger ledger;
    ledger.record_backend_observation(observation);
    if (expect(ledger.certifiable(), "observed ledger is certifiable")) {
      return 1;
    }
  }
  {
    prisminfer::RuntimeConfig config;
    config.backend = prisminfer::BackendKind::Fake;
    config.hard_cap_bytes = 2;
    config.warmup_tokens = 3;
    auto backend = prisminfer::make_backend_adapter(config.backend);
    prisminfer::CappedAllocatorTracker allocator(config.hard_cap_bytes);
    const auto warmup = backend->warmup(config, allocator);
    if (expect(!warmup.ok, "fake backend warmup fails over cap")) return 1;
    if (expect(warmup.failure_reason == "allocator_cap_would_be_exceeded",
               "fake backend failure reason")) {
      return 1;
    }
  }
  {
    auto backend =
        prisminfer::make_backend_adapter(prisminfer::BackendKind::Llama);
#if defined(PRISMINFER_ENABLE_LLAMA_BACKEND)
    if (expect(backend != nullptr, "llama backend opt-in factory")) return 1;
    prisminfer::RuntimeConfig config;
    config.backend = prisminfer::BackendKind::Llama;
    prisminfer::CappedAllocatorTracker allocator(config.hard_cap_bytes);
    const auto warmup = backend->warmup(config, allocator);
    if (expect(!warmup.ok, "llama stub fails closed")) return 1;
    if (expect(warmup.failure_reason == "llama_backend_not_implemented",
               "llama stub reason")) {
      return 1;
    }
#else
    if (expect(backend == nullptr, "llama backend disabled by default")) {
      return 1;
    }
#endif
  }
  return 0;
}

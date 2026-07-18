#include <iostream>
#include <filesystem>
#include <fstream>

#include "prisminfer/backend.h"
#include "prisminfer/native_worker.h"
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

int main(int argc, char** argv) {
#if defined(_WIN32)
  if (argc > 2 && std::string(argv[1]) == "cli" &&
      std::string(argv[2]) == "--model") {
    std::cout << "llama_kv_cache: size = 1.00 MiB\n"
              << "llama_context: CPU compute buffer size = 2.00 MiB\n";
    return 0;
  }
#endif
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
    config.gpu_layers = 1U;
    const auto unsupervised_gpu = backend->warmup(config, allocator);
    if (expect(!unsupervised_gpu.ok &&
                   unsupervised_gpu.failure_reason ==
                       "llama_gpu_requires_supervised_context_protocol",
               "GPU-capable llama cannot bypass staged supervision")) {
      return 1;
    }
    config.gpu_layers = 0U;
    const auto warmup = backend->warmup(config, allocator);
    if (expect(!warmup.ok, "llama backend requires executable")) return 1;
    if (expect(warmup.failure_reason == "llama_executable_required",
               "llama executable required reason")) {
      return 1;
    }
    config.llama_executable_path =
        std::filesystem::temp_directory_path() /
        "definitely-missing-prisminfer-llama-cli";
    config.model_path =
        std::filesystem::temp_directory_path() / "prisminfer-test.gguf";
    {
      std::ofstream out(config.model_path, std::ios::out | std::ios::binary);
      out << "GGUF";
    }
    const auto missing_executable = backend->warmup(config, allocator);
    std::error_code remove_error;
    std::filesystem::remove(config.model_path, remove_error);
    if (expect(!missing_executable.ok,
               "llama backend fails closed on missing executable")) {
      return 1;
    }
    if (expect(missing_executable.failure_reason ==
                   "llama_executable_not_found",
               "llama missing executable reason")) {
      return 1;
    }
    config.run_id = "backend-adapter-test";
    config.llama_executable_path = std::filesystem::absolute(argv[0]);
    {
      std::ofstream out(config.model_path, std::ios::out | std::ios::binary);
      out << "GGUF";
    }
    const auto scripted_warmup = backend->warmup(config, allocator);
    std::filesystem::remove(config.model_path, remove_error);
    if (expect(!scripted_warmup.ok,
               "unapproved executable cannot cross native worker boundary")) return 1;
    if (expect(scripted_warmup.failure_reason ==
                   "llama_native_worker_executable_not_approved",
               "runtime executable outside immutable approval fails closed")) return 1;
#else
    if (expect(backend == nullptr, "llama backend disabled by default")) {
      return 1;
    }
#endif
  }
  return 0;
}

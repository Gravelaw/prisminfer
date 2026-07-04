#include <iostream>
#include <filesystem>
#include <fstream>

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
    const auto script_path =
        std::filesystem::temp_directory_path()
#if defined(_WIN32)
        / "prisminfer-llama-test.cmd";
#else
        / "prisminfer-llama-test.sh";
#endif
    {
      std::ofstream script(script_path, std::ios::out | std::ios::trunc);
#if defined(_WIN32)
      script << "@echo off\n"
             << "echo llama_kv_cache: size = 1.00 MiB\n"
             << "echo llama_context: CPU compute buffer size = 2.00 MiB\n"
             << "exit /b 0\n";
#else
      script << "#!/usr/bin/env sh\n"
             << "echo 'llama_kv_cache: size = 1.00 MiB'\n"
             << "echo 'llama_context: CPU compute buffer size = 2.00 MiB'\n";
#endif
    }
#if !defined(_WIN32)
    std::filesystem::permissions(
        script_path, std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::add);
#endif
    config.run_id = "backend-adapter-test";
    config.llama_executable_path = script_path;
    {
      std::ofstream out(config.model_path, std::ios::out | std::ios::binary);
      out << "GGUF";
    }
    const auto scripted_warmup = backend->warmup(config, allocator);
    std::filesystem::remove(config.model_path, remove_error);
    std::filesystem::remove(script_path, remove_error);
    if (expect(scripted_warmup.ok, "llama script warmup succeeds")) return 1;
    if (expect(scripted_warmup.backend_external_peak_bytes ==
                   3ULL * 1024ULL * 1024ULL,
               "llama script allocation evidence parsed")) {
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

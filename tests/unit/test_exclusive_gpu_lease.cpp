#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#include "prisminfer/exclusive_gpu_lease.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

int child_exit_code(int status) {
#ifdef _WIN32
  return status;
#else
  return WIFEXITED(status) ? WEXITSTATUS(status) : 255;
#endif
}

int run_child(const char* executable, const char* mode = "--child") {
  const std::string command =
      "\"" + std::string(executable) + "\" " + mode;
  return child_exit_code(std::system(command.c_str()));
}

}  // namespace

int main(int argc, char** argv) {
  using prisminfer::ExclusiveGpuLeaseStatus;
  constexpr std::int32_t kHigh = 7;
  constexpr std::uint32_t kLow = 11;

  if (argc == 2 && std::string(argv[1]) == "--child") {
    const auto acquired = prisminfer::acquire_exclusive_gpu_lease(kHigh, kLow);
    return acquired.status == ExclusiveGpuLeaseStatus::Acquired ? 0 : 17;
  }
  if (argc == 2 && std::string(argv[1]) == "--crash-child") {
    const auto acquired = prisminfer::acquire_exclusive_gpu_lease(kHigh, kLow);
    if (acquired.status != ExclusiveGpuLeaseStatus::Acquired) return 18;
    std::_Exit(23);
  }
#ifdef __linux__
  if (argc == 2 && std::string(argv[1]) == "--replace-path-child") {
    constexpr const char* kOldPath =
        "/tmp/prisminfer-gpu-00000007-0000000b.lock";
    std::remove(kOldPath);
    if (auto* replacement = std::fopen(kOldPath, "w")) {
      std::fputs("replacement", replacement);
      std::fclose(replacement);
    }
    const auto acquired = prisminfer::acquire_exclusive_gpu_lease(kHigh, kLow);
    std::remove(kOldPath);
    return acquired.status == ExclusiveGpuLeaseStatus::HeldByAnotherProcess
               ? 0
               : 19;
  }
#endif

  static_assert(!std::is_copy_constructible_v<prisminfer::ExclusiveGpuLease>);
  static_assert(std::is_move_constructible_v<prisminfer::ExclusiveGpuLease>);

  if (expect(prisminfer::acquire_exclusive_gpu_lease(0, 0).status ==
                 ExclusiveGpuLeaseStatus::InvalidAdapterIdentity,
             "zero adapter identity rejects")) {
    return 1;
  }

  {
    auto first = prisminfer::acquire_exclusive_gpu_lease(kHigh, kLow);
    if (expect(first.status == ExclusiveGpuLeaseStatus::Acquired &&
                   first.lease && first.lease->active(),
               "first owner acquires the adapter lease") ||
        expect(first.lease->lease_id() == "prisminfer-gpu-00000007-0000000b",
               "lease id is stable and adapter-scoped") ||
        expect(prisminfer::acquire_exclusive_gpu_lease(kHigh, kLow).status ==
                   ExclusiveGpuLeaseStatus::AlreadyHeldInProcess,
               "same process cannot recursively acquire") ||
        expect(run_child(argv[0]) == 17,
               "another process fails closed while lease is held")) {
      return 1;
    }
#ifdef __linux__
    if (expect(run_child(argv[0], "--replace-path-child") == 0,
               "pathname replacement cannot bypass the abstract lease")) {
      return 1;
    }
#endif
    auto moved = std::move(*first.lease);
    if (expect(moved.active() && !first.lease->active(),
               "move transfers exclusive lease ownership")) {
      return 1;
    }
  }

  if (expect(run_child(argv[0]) == 0,
             "another process acquires after deterministic release")) {
    return 1;
  }

  if (expect(run_child(argv[0], "--crash-child") == 23,
             "crash fixture exits while holding the OS lease")) {
    return 1;
  }
  {
    auto recovered = prisminfer::acquire_exclusive_gpu_lease(kHigh, kLow);
    if (expect(recovered.status == ExclusiveGpuLeaseStatus::Acquired,
               "OS releases an abandoned lease after process death")) {
      return 1;
    }
    auto lease = std::move(*recovered.lease);
    std::thread release_on_another_thread(
        [lease = std::move(lease)]() mutable {});
    release_on_another_thread.join();
  }
  if (expect(prisminfer::acquire_exclusive_gpu_lease(kHigh, kLow).status ==
                 ExclusiveGpuLeaseStatus::Acquired,
             "lease ownership can be released from another supervisor thread")) {
    return 1;
  }

  auto first_adapter = prisminfer::acquire_exclusive_gpu_lease(kHigh, kLow);
  auto second_adapter = prisminfer::acquire_exclusive_gpu_lease(kHigh, kLow + 1);
  if (expect(first_adapter.status == ExclusiveGpuLeaseStatus::Acquired &&
                 second_adapter.status == ExclusiveGpuLeaseStatus::Acquired,
             "distinct adapter identities have distinct leases")) {
    return 1;
  }
  return 0;
}

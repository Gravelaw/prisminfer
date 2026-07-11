#include "prisminfer/host_memory_tracker.h"

#include <chrono>
#include <limits>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

namespace prisminfer {

namespace {

#if defined(_WIN32)
bool checked_pages_to_bytes(SIZE_T pages, SIZE_T page_size,
                            std::uint64_t* bytes) {
  if (bytes == nullptr || page_size == 0) {
    return false;
  }
  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
  const auto pages_u64 = static_cast<std::uint64_t>(pages);
  const auto page_size_u64 = static_cast<std::uint64_t>(page_size);
  if (pages_u64 > maximum / page_size_u64) {
    return false;
  }
  *bytes = pages_u64 * page_size_u64;
  return true;
}
#endif

}  // namespace

HostTelemetrySample sample_host_telemetry() {
  HostTelemetrySample sample;
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS_EX memory{};
  memory.cb = sizeof(memory);
  if (GetProcessMemoryInfo(GetCurrentProcess(),
                           reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                           sizeof(memory)) == 0) {
    sample.unavailable_reason = "process_memory_counters_unavailable";
    return sample;
  }

  PERFORMANCE_INFORMATION performance{};
  performance.cb = sizeof(performance);
  if (GetPerformanceInfo(&performance,
                         static_cast<DWORD>(sizeof(performance))) == 0) {
    sample.unavailable_reason = "system_performance_info_unavailable";
    return sample;
  }

  if (!checked_pages_to_bytes(performance.PhysicalTotal, performance.PageSize,
                              &sample.system_memory_total_bytes) ||
      !checked_pages_to_bytes(performance.PhysicalAvailable,
                              performance.PageSize,
                              &sample.system_memory_available_bytes) ||
      !checked_pages_to_bytes(performance.CommitTotal, performance.PageSize,
                              &sample.system_commit_total_bytes) ||
      !checked_pages_to_bytes(performance.CommitLimit, performance.PageSize,
                              &sample.system_commit_limit_bytes)) {
    sample.unavailable_reason = "system_memory_counter_overflow";
    return sample;
  }

  if (sample.system_memory_total_bytes == 0 ||
      sample.system_memory_available_bytes > sample.system_memory_total_bytes ||
      sample.system_commit_total_bytes > sample.system_commit_limit_bytes) {
    sample.unavailable_reason = "system_memory_counters_contradictory";
    return sample;
  }
  sample.system_commit_available_bytes =
      sample.system_commit_limit_bytes - sample.system_commit_total_bytes;
  sample.system_commit_source = "get_performance_info";

  const auto captured = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
  if (captured < 0) {
    sample.unavailable_reason = "monotonic_clock_invalid";
    return sample;
  }
  sample.captured_monotonic_milliseconds = static_cast<std::uint64_t>(captured);

  IO_COUNTERS io{};
  if (GetProcessIoCounters(GetCurrentProcess(), &io) == 0) {
    sample.unavailable_reason = "process_io_counters_unavailable";
    return sample;
  }

  sample.available = true;
  sample.process_working_set_bytes =
      static_cast<std::uint64_t>(memory.WorkingSetSize);
  sample.process_private_bytes =
      static_cast<std::uint64_t>(memory.PrivateUsage);
  sample.process_io_read_bytes =
      static_cast<std::uint64_t>(io.ReadTransferCount);
  sample.process_io_write_bytes =
      static_cast<std::uint64_t>(io.WriteTransferCount);
  return sample;
#else
  sample.unavailable_reason = "host_telemetry_not_implemented_for_platform";
  return sample;
#endif
}

}  // namespace prisminfer

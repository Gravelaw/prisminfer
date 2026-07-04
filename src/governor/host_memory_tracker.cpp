#include "prisminfer/host_memory_tracker.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

namespace prisminfer {

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

  MEMORYSTATUSEX status{};
  status.dwLength = sizeof(status);
  if (GlobalMemoryStatusEx(&status) == 0) {
    sample.unavailable_reason = "global_memory_status_unavailable";
    return sample;
  }

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
  sample.system_memory_available_bytes =
      static_cast<std::uint64_t>(status.ullAvailPhys);
  sample.system_commit_total_bytes =
      static_cast<std::uint64_t>(status.ullTotalPageFile -
                                 status.ullAvailPageFile);
  sample.system_commit_limit_bytes =
      static_cast<std::uint64_t>(status.ullTotalPageFile);
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

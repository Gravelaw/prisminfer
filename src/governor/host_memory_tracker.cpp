#include "prisminfer/host_memory_tracker.h"

#include <chrono>
#include <limits>

#if defined(_WIN32)
#define NOMINMAX
// clang-format off: Windows SDK headers require windows.h first.
#include <windows.h>
#include <psapi.h>
// clang-format on
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

std::string utf8(const wchar_t* value, int length) {
  if (value == nullptr || length <= 0) return {};
  const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value,
                                        length, nullptr, 0, nullptr, nullptr);
  if (bytes <= 0) return {};
  std::string converted(static_cast<std::size_t>(bytes), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length,
                          converted.data(), bytes, nullptr, nullptr) == 0) {
    return {};
  }
  return converted;
}

struct PagefileAccumulator {
  SIZE_T page_size{0};
  bool overflow{false};
  std::uint32_t count{0};
  std::uint64_t total_bytes{0};
  std::uint64_t current_usage_bytes{0};
  std::uint64_t peak_usage_bytes{0};
};

BOOL CALLBACK accumulate_pagefile(LPVOID context,
                                  PENUM_PAGE_FILE_INFORMATION information,
                                  LPCWSTR) {
  auto* accumulator = static_cast<PagefileAccumulator*>(context);
  if (accumulator == nullptr || information == nullptr ||
      accumulator->page_size == 0U ||
      accumulator->count == std::numeric_limits<std::uint32_t>::max()) {
    if (accumulator != nullptr) accumulator->overflow = true;
    return FALSE;
  }
  std::uint64_t total = 0U;
  std::uint64_t current = 0U;
  std::uint64_t peak = 0U;
  if (!checked_pages_to_bytes(information->TotalSize, accumulator->page_size,
                              &total) ||
      !checked_pages_to_bytes(information->TotalInUse, accumulator->page_size,
                              &current) ||
      !checked_pages_to_bytes(information->PeakUsage, accumulator->page_size,
                              &peak) ||
      total > std::numeric_limits<std::uint64_t>::max() -
                  accumulator->total_bytes ||
      current > std::numeric_limits<std::uint64_t>::max() -
                    accumulator->current_usage_bytes ||
      peak > std::numeric_limits<std::uint64_t>::max() -
                 accumulator->peak_usage_bytes) {
    accumulator->overflow = true;
    return FALSE;
  }
  ++accumulator->count;
  accumulator->total_bytes += total;
  accumulator->current_usage_bytes += current;
  accumulator->peak_usage_bytes += peak;
  return TRUE;
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

  SYSTEM_INFO system_info{};
  GetSystemInfo(&system_info);
  PagefileAccumulator pagefiles;
  pagefiles.page_size = system_info.dwPageSize;
  if (pagefiles.page_size == 0U ||
      !EnumPageFilesW(accumulate_pagefile, &pagefiles) || pagefiles.overflow) {
    sample.pagefile_configuration_unavailable_reason =
        pagefiles.overflow ? "pagefile_counter_overflow"
                           : "pagefile_configuration_unavailable";
  } else if (pagefiles.current_usage_bytes > pagefiles.total_bytes ||
             pagefiles.peak_usage_bytes > pagefiles.total_bytes ||
             pagefiles.current_usage_bytes > pagefiles.peak_usage_bytes) {
    sample.pagefile_configuration_unavailable_reason =
        "pagefile_counters_contradictory";
  } else {
    sample.pagefile_configuration_available = true;
    sample.pagefile_count = pagefiles.count;
    sample.pagefile_total_bytes = pagefiles.total_bytes;
    sample.pagefile_current_usage_bytes = pagefiles.current_usage_bytes;
    sample.pagefile_peak_usage_bytes = pagefiles.peak_usage_bytes;
  }

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

  wchar_t process_image[32768]{};
  const DWORD process_image_length =
      GetModuleFileNameW(nullptr, process_image, 32768U);
  if (process_image_length == 0U || process_image_length >= 32768U) {
    sample.unavailable_reason = "process_image_identity_unavailable";
    return sample;
  }
  sample.process_image_path =
      utf8(process_image, static_cast<int>(process_image_length));
  if (sample.process_image_path.empty()) {
    sample.unavailable_reason = "process_image_identity_encoding_failed";
    return sample;
  }

  sample.available = true;
  sample.process_id = GetCurrentProcessId();
  sample.process_working_set_current_bytes =
      static_cast<std::uint64_t>(memory.WorkingSetSize);
  sample.process_working_set_peak_bytes =
      static_cast<std::uint64_t>(memory.PeakWorkingSetSize);
  sample.process_private_commit_current_bytes =
      static_cast<std::uint64_t>(memory.PrivateUsage);
  sample.process_private_commit_peak_bytes =
      static_cast<std::uint64_t>(memory.PeakPagefileUsage);
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

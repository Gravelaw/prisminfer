#include "prisminfer/windows_evidence.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "prisminfer/gpu_cap_policy.h"

#if defined(_WIN32)
#define NOMINMAX
#include <dxgi1_4.h>
#include <windows.h>
#endif

namespace prisminfer {
namespace {

WindowsEvidenceDecision downgrade(
    std::string reason, std::string classification = "measured-non-certified") {
  return {false, std::move(classification), std::move(reason)};
}

bool exact_lower_hex(const std::string& value, std::size_t length) {
  return value.size() == length &&
         std::all_of(value.begin(), value.end(), [](unsigned char character) {
           return (character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f');
         });
}

bool checked_category_sum(const std::uint64_t* categories, std::size_t count,
                          std::uint64_t* reconciled_bytes) {
  if (categories == nullptr || reconciled_bytes == nullptr) return false;
  std::uint64_t total = 0U;
  for (std::size_t index = 0; index < count; ++index) {
    if (categories[index] >
        std::numeric_limits<std::uint64_t>::max() - total) {
      return false;
    }
    total += categories[index];
  }
  *reconciled_bytes = total;
  return true;
}

bool reconcile_owned_categories(const OwnedGpuMemoryEvidence& gpu,
                                std::uint64_t* reconciled_current_bytes,
                                std::uint64_t* reconciled_peak_bytes) {
  const std::uint64_t current_categories[] = {
      gpu.backend_buffer_current_bytes,
      gpu.backend_pool_current_bytes,
      gpu.kernel_workspace_current_bytes,
      gpu.activation_current_bytes,
      gpu.kv_current_bytes,
      gpu.graph_current_bytes,
      gpu.profiler_workspace_current_bytes,
      gpu.cuda_context_runtime_current_bytes};
  const std::uint64_t categories[] = {
      gpu.backend_buffer_at_owned_peak_bytes,
      gpu.backend_pool_at_owned_peak_bytes,
      gpu.kernel_workspace_at_owned_peak_bytes,
      gpu.activation_at_owned_peak_bytes,
      gpu.kv_at_owned_peak_bytes,
      gpu.graph_at_owned_peak_bytes,
      gpu.profiler_workspace_at_owned_peak_bytes,
      gpu.cuda_context_runtime_at_owned_peak_bytes};
  return checked_category_sum(current_categories,
                              std::size(current_categories),
                              reconciled_current_bytes) &&
         checked_category_sum(categories, std::size(categories),
                              reconciled_peak_bytes);
}

#if defined(_WIN32)
std::string utf8(const wchar_t* value) {
  if (value == nullptr || *value == L'\0') return {};
  const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value,
                                         -1, nullptr, 0, nullptr, nullptr);
  if (length <= 1) return {};
  std::string converted(static_cast<std::size_t>(length), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                          converted.data(), length, nullptr, nullptr) == 0) {
    return {};
  }
  converted.resize(static_cast<std::size_t>(length - 1));
  return converted;
}

template <typename Interface>
class ComPtr {
 public:
  ~ComPtr() {
    if (value_ != nullptr) value_->Release();
  }
  Interface* get() const { return value_; }
  Interface** put() { return &value_; }

 private:
  Interface* value_{nullptr};
};

class UniqueHandle {
 public:
  explicit UniqueHandle(HANDLE value) : value_(value) {}
  ~UniqueHandle() {
    if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
      CloseHandle(value_);
    }
  }
  UniqueHandle(const UniqueHandle&) = delete;
  UniqueHandle& operator=(const UniqueHandle&) = delete;
  HANDLE get() const { return value_; }

 private:
  HANDLE value_{nullptr};
};

std::string hex_bytes(const unsigned char* bytes, std::size_t count) {
  if (bytes == nullptr) return {};
  std::ostringstream encoded;
  encoded << std::hex << std::setfill('0');
  for (std::size_t index = 0; index < count; ++index) {
    encoded << std::setw(2) << static_cast<unsigned int>(bytes[index]);
  }
  return encoded.str();
}
#endif

}  // namespace

ProcessDeviceMemorySample reconcile_process_device_memory_candidates(
    std::uint32_t process_id, std::int32_t adapter_luid_high,
    std::uint32_t adapter_luid_low,
    std::uint64_t captured_monotonic_milliseconds,
    const std::vector<ProcessDeviceMemoryCandidate>& candidates) {
  ProcessDeviceMemorySample sample;
  sample.process_id = process_id;
  sample.adapter_luid_high = adapter_luid_high;
  sample.adapter_luid_low = adapter_luid_low;
  sample.captured_monotonic_milliseconds = captured_monotonic_milliseconds;
  if (process_id == 0U || captured_monotonic_milliseconds == 0U) {
    sample.unavailable_reason = "process_device_identity_invalid";
    return sample;
  }
  if (candidates.empty()) {
    sample.unavailable_reason = "process_device_pid_not_reported";
    return sample;
  }
  std::optional<std::uint64_t> exact_bytes;
  std::unordered_set<std::string> sources;
  for (const auto& candidate : candidates) {
    if ((candidate.source != "nvml-compute" &&
         candidate.source != "nvml-graphics") ||
        !sources.insert(candidate.source).second) {
      sample.unavailable_reason = "process_device_source_invalid";
      return sample;
    }
    if (exact_bytes && *exact_bytes != candidate.bytes) {
      sample.unavailable_reason = "process_device_reports_contradictory";
      return sample;
    }
    exact_bytes = candidate.bytes;
  }
  sample.available = true;
  sample.source = "nvml-process";
  sample.current_bytes = *exact_bytes;
  return sample;
}

ProcessDeviceMemorySample sample_process_device_memory(
    std::uint32_t process_id, std::int32_t adapter_luid_high,
    std::uint32_t adapter_luid_low) {
  ProcessDeviceMemorySample unavailable;
  unavailable.process_id = process_id;
  unavailable.adapter_luid_high = adapter_luid_high;
  unavailable.adapter_luid_low = adapter_luid_low;
#if !defined(_WIN32)
  unavailable.unavailable_reason =
      "process_device_telemetry_not_available_for_platform";
  return unavailable;
#else
  if (process_id == 0U) {
    unavailable.unavailable_reason = "process_device_pid_required";
    return unavailable;
  }
  using NvmlReturn = int;
  using NvmlDevice = void*;
  struct NvmlProcessInfo {
    unsigned int pid;
    unsigned long long used_gpu_memory;
    unsigned int gpu_instance_id;
    unsigned int compute_instance_id;
  };
  using Init = NvmlReturn(__cdecl*)();
  using Shutdown = NvmlReturn(__cdecl*)();
  using DeviceCount = NvmlReturn(__cdecl*)(unsigned int*);
  using DeviceByIndex = NvmlReturn(__cdecl*)(unsigned int, NvmlDevice*);
  using DeviceLuid = NvmlReturn(__cdecl*)(NvmlDevice, char*, unsigned int*);
  using RunningProcesses =
      NvmlReturn(__cdecl*)(NvmlDevice, unsigned int*, NvmlProcessInfo*);
  constexpr NvmlReturn kSuccess = 0;
  constexpr NvmlReturn kInsufficientSize = 7;
  constexpr unsigned long long kValueNotAvailable = ~0ULL;
  struct ModuleGuard {
    HMODULE value{nullptr};
    ~ModuleGuard() {
      if (value != nullptr) FreeLibrary(value);
    }
  } module{LoadLibraryExW(L"nvml.dll", nullptr,
                          LOAD_LIBRARY_SEARCH_SYSTEM32)};
  if (module.value == nullptr) {
    unavailable.unavailable_reason = "nvml_provider_unavailable";
    return unavailable;
  }
  const auto symbol = [&](const char* name) {
    return GetProcAddress(module.value, name);
  };
  const auto init = reinterpret_cast<Init>(symbol("nvmlInit_v2"));
  const auto shutdown = reinterpret_cast<Shutdown>(symbol("nvmlShutdown"));
  const auto count_devices =
      reinterpret_cast<DeviceCount>(symbol("nvmlDeviceGetCount_v2"));
  const auto device_by_index = reinterpret_cast<DeviceByIndex>(
      symbol("nvmlDeviceGetHandleByIndex_v2"));
  const auto device_luid =
      reinterpret_cast<DeviceLuid>(symbol("nvmlDeviceGetLuid"));
  const auto compute = reinterpret_cast<RunningProcesses>(
      symbol("nvmlDeviceGetComputeRunningProcesses_v3"));
  const auto graphics = reinterpret_cast<RunningProcesses>(
      symbol("nvmlDeviceGetGraphicsRunningProcesses_v3"));
  if (!init || !shutdown || !count_devices || !device_by_index ||
      !device_luid || !compute || !graphics || init() != kSuccess) {
    unavailable.unavailable_reason = "nvml_contract_unavailable";
    return unavailable;
  }
  struct ShutdownGuard {
    Shutdown shutdown;
    ~ShutdownGuard() { (void)shutdown(); }
  } guard{shutdown};
  unsigned int count = 0U;
  if (count_devices(&count) != kSuccess || count == 0U || count > 64U) {
    unavailable.unavailable_reason = "nvml_device_enumeration_failed";
    return unavailable;
  }
  NvmlDevice matched = nullptr;
  for (unsigned int index = 0U; index < count; ++index) {
    NvmlDevice device = nullptr;
    std::array<char, 8> luid{};
    unsigned int node_mask = 0U;
    if (device_by_index(index, &device) != kSuccess || device == nullptr ||
        device_luid(device, luid.data(), &node_mask) != kSuccess) {
      continue;
    }
    LUID expected{};
    expected.HighPart = adapter_luid_high;
    expected.LowPart = adapter_luid_low;
    if (std::equal(luid.begin(), luid.end(),
                   reinterpret_cast<const char*>(&expected))) {
      if (matched != nullptr) {
        unavailable.unavailable_reason = "nvml_adapter_luid_ambiguous";
        return unavailable;
      }
      matched = device;
    }
  }
  if (matched == nullptr) {
    unavailable.unavailable_reason = "nvml_adapter_luid_not_found";
    return unavailable;
  }
  std::vector<ProcessDeviceMemoryCandidate> candidates;
  const auto collect = [&](RunningProcesses query, const char* source) {
    unsigned int entries = 0U;
    NvmlReturn status = query(matched, &entries, nullptr);
    if (status == kSuccess && entries == 0U) return true;
    if (status != kInsufficientSize || entries == 0U || entries > 65536U) {
      return false;
    }
    std::vector<NvmlProcessInfo> processes(entries);
    status = query(matched, &entries, processes.data());
    if (status != kSuccess || entries > processes.size()) return false;
    for (unsigned int index = 0U; index < entries; ++index) {
      if (processes[index].pid == process_id) {
        if (processes[index].used_gpu_memory == kValueNotAvailable) {
          return false;
        }
        candidates.push_back({source, processes[index].used_gpu_memory});
        break;
      }
    }
    return true;
  };
  if (!collect(compute, "nvml-compute") ||
      !collect(graphics, "nvml-graphics")) {
    unavailable.unavailable_reason = "nvml_process_query_failed";
    return unavailable;
  }
  const auto captured = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
  if (captured <= 0) {
    unavailable.unavailable_reason = "monotonic_clock_invalid";
    return unavailable;
  }
  return reconcile_process_device_memory_candidates(
      process_id, adapter_luid_high, adapter_luid_low,
      static_cast<std::uint64_t>(captured), candidates);
#endif
}

WddmMemorySample sample_wddm_memory(std::uint32_t adapter_index) {
  WddmMemorySample sample;
  sample.adapter_index = adapter_index;
#if defined(_WIN32)
  ComPtr<IDXGIFactory4> factory;
  if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory4),
                                reinterpret_cast<void**>(factory.put())))) {
    sample.unavailable_reason = "dxgi_factory_unavailable";
    return sample;
  }
  ComPtr<IDXGIAdapter1> adapter;
  if (factory.get()->EnumAdapters1(adapter_index, adapter.put()) != S_OK) {
    sample.unavailable_reason = "dxgi_adapter_unavailable";
    return sample;
  }
  DXGI_ADAPTER_DESC1 description{};
  if (FAILED(adapter.get()->GetDesc1(&description))) {
    sample.unavailable_reason = "dxgi_adapter_description_unavailable";
    return sample;
  }
  if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0U) {
    sample.unavailable_reason = "dxgi_software_adapter_rejected";
    return sample;
  }
  ComPtr<IDXGIAdapter3> adapter3;
  if (FAILED(adapter.get()->QueryInterface(
          __uuidof(IDXGIAdapter3), reinterpret_cast<void**>(adapter3.put())))) {
    sample.unavailable_reason = "dxgi_adapter3_unavailable";
    return sample;
  }
  DXGI_QUERY_VIDEO_MEMORY_INFO local{};
  DXGI_QUERY_VIDEO_MEMORY_INFO nonlocal{};
  if (FAILED(adapter3.get()->QueryVideoMemoryInfo(
          0U, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local)) ||
      FAILED(adapter3.get()->QueryVideoMemoryInfo(
          0U, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonlocal))) {
    sample.unavailable_reason = "dxgi_video_memory_query_unavailable";
    return sample;
  }
  const auto captured = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
  if (captured < 0) {
    sample.unavailable_reason = "monotonic_clock_invalid";
    return sample;
  }
  sample.captured_monotonic_milliseconds = static_cast<std::uint64_t>(captured);
  sample.adapter_luid_high = description.AdapterLuid.HighPart;
  sample.adapter_luid_low = description.AdapterLuid.LowPart;
  sample.adapter_description = utf8(description.Description);
  sample.local_budget_bytes = local.Budget;
  sample.local_current_usage_bytes = local.CurrentUsage;
  sample.local_available_for_reservation_bytes = local.AvailableForReservation;
  sample.nonlocal_budget_bytes = nonlocal.Budget;
  sample.nonlocal_current_usage_bytes = nonlocal.CurrentUsage;
  sample.nonlocal_available_for_reservation_bytes =
      nonlocal.AvailableForReservation;
  if (sample.local_budget_bytes == 0U ||
      sample.local_current_usage_bytes > sample.local_budget_bytes ||
      sample.nonlocal_current_usage_bytes > sample.nonlocal_budget_bytes) {
    sample.unavailable_reason = "dxgi_video_memory_counters_contradictory";
    return sample;
  }
  sample.available = true;
  return sample;
#else
  sample.unavailable_reason = "wddm_telemetry_not_available_for_platform";
  return sample;
#endif
}

FileIoEvidence sample_file_identity(const std::filesystem::path& path,
                                    std::string role) {
  FileIoEvidence evidence;
  evidence.role = std::move(role);
#if defined(_WIN32)
  if (path.empty()) {
    evidence.unavailable_reason = "file_path_required";
    return evidence;
  }
  UniqueHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING,
                                FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (file.get() == INVALID_HANDLE_VALUE) {
    evidence.unavailable_reason = "file_identity_open_failed";
    return evidence;
  }
  BY_HANDLE_FILE_INFORMATION basic{};
  FILE_ID_INFO identity{};
  LARGE_INTEGER size{};
  if (!GetFileInformationByHandle(file.get(), &basic) ||
      (basic.dwFileAttributes &
       (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0U ||
      !GetFileInformationByHandleEx(file.get(), FileIdInfo, &identity,
                                    sizeof(identity)) ||
      !GetFileSizeEx(file.get(), &size) || size.QuadPart < 0) {
    evidence.unavailable_reason = "file_identity_query_failed";
    return evidence;
  }
  constexpr DWORD kMaximumFinalPathCharacters = 32U * 1024U;
  std::vector<wchar_t> final_path(kMaximumFinalPathCharacters, L'\0');
  const DWORD final_length = GetFinalPathNameByHandleW(
      file.get(), final_path.data(), kMaximumFinalPathCharacters,
      FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (final_length == 0U || final_length >= kMaximumFinalPathCharacters) {
    evidence.unavailable_reason = "file_final_path_unavailable";
    return evidence;
  }
  std::ostringstream volume;
  volume << std::hex << std::setfill('0') << std::setw(16)
         << identity.VolumeSerialNumber;
  evidence.final_path = utf8(final_path.data());
  evidence.volume_serial_hex = volume.str();
  evidence.file_id_hex =
      hex_bytes(identity.FileId.Identifier, sizeof(identity.FileId.Identifier));
  evidence.hard_link_count = basic.nNumberOfLinks;
  evidence.size_bytes = static_cast<std::uint64_t>(size.QuadPart);
  if (evidence.final_path.empty() || evidence.file_id_hex.size() != 32U) {
    evidence.unavailable_reason = "file_identity_encoding_failed";
    return evidence;
  }
  evidence.identity_available = true;
  return evidence;
#else
  (void)path;
  evidence.unavailable_reason = "file_identity_not_available_for_platform";
  return evidence;
#endif
}

FileEvidenceValidationResult validate_file_io_evidence(
    const FileIoEvidence& evidence, bool cold_cache_required) {
  const bool role_valid =
      evidence.role == "source-gguf" || evidence.role == "mmproj" ||
      evidence.role == "derived-artifact" || evidence.role == "log";
  if (!role_valid) return {false, "file_role_invalid"};
  if (!evidence.identity_available || evidence.final_path.empty() ||
      !exact_lower_hex(evidence.volume_serial_hex, 16U) ||
      !exact_lower_hex(evidence.file_id_hex, 32U) ||
      evidence.hard_link_count != 1U) {
    return {false, "opened_handle_file_identity_required"};
  }
  if (evidence.dropped_trace_records != 0U) {
    return {false, "file_trace_dropped_records"};
  }
  if (!evidence.identity_aware_io_available ||
      !evidence.pagefile_io_available) {
    return {false, "file_and_pagefile_trace_required"};
  }
  if (((evidence.role == "source-gguf" || evidence.role == "mmproj") &&
       evidence.observed_read_bytes == 0U) ||
      ((evidence.role == "derived-artifact" || evidence.role == "log") &&
       evidence.observed_read_bytes == 0U &&
       evidence.observed_write_bytes == 0U)) {
    return {false, "actual_identity_linked_file_io_required"};
  }
  if (evidence.mapped_bytes > evidence.size_bytes ||
      (evidence.resident_proxy_available &&
       evidence.resident_proxy_bytes > evidence.mapped_bytes)) {
    return {false, "file_mapping_or_resident_proxy_invalid"};
  }
  if (evidence.mapped_bytes != 0U && !evidence.resident_proxy_available) {
    return {false, "mapped_residency_ambiguous"};
  }
  const bool hard_fault_source_valid =
      evidence.hard_fault_source == "etw-file-identity" ||
      evidence.hard_fault_source == "etw-pagefile" ||
      evidence.hard_fault_source == "etw-classified-none";
  if (!evidence.hard_faults_available || !hard_fault_source_valid ||
      (evidence.hard_fault_count != 0U &&
       evidence.hard_fault_source == "etw-classified-none")) {
    return {false, "hard_fault_source_ambiguous"};
  }
  if (cold_cache_required &&
      (!evidence.cache_state_verified || evidence.cache_state != "cold")) {
    return {false, "cold_cache_state_unverified"};
  }
  return {true, ""};
}

WindowsEvidenceDecision classify_windows_evidence(
    const WindowsEvidenceBundle& evidence) {
  if (evidence.claim_scope != "owned-allocation" &&
      evidence.claim_scope != "physical-residency") {
    return downgrade("claim_scope_invalid");
  }
  if (evidence.instrumentation_mode != "ordinary" &&
      evidence.instrumentation_mode != "profiler") {
    return downgrade("instrumentation_mode_invalid");
  }
  if (!evidence.real_execution) {
    return downgrade("real_execution_required", "simulated/policy-only");
  }
  if (!evidence.process_tree.available ||
      evidence.process_tree.parent_identity.empty() ||
      evidence.process_tree.job_identity.empty()) {
    return downgrade("process_tree_host_evidence_required");
  }
  if (evidence.process_tree.parent_working_set_current_bytes == 0U ||
      evidence.process_tree.parent_working_set_peak_bytes <
          evidence.process_tree.parent_working_set_current_bytes ||
      evidence.process_tree.parent_private_commit_current_bytes == 0U ||
      evidence.process_tree.parent_private_commit_peak_bytes <
          evidence.process_tree.parent_private_commit_current_bytes ||
      evidence.process_tree.tree_working_set_peak_bytes == 0U ||
      evidence.process_tree.tree_private_commit_peak_bytes == 0U) {
    return downgrade("process_tree_host_counters_invalid");
  }
  if (!evidence.system_host_pre.available ||
      !evidence.system_host_post.available ||
      evidence.system_host_pre.system_commit_source != "get_performance_info" ||
      evidence.system_host_post.system_commit_source !=
          "get_performance_info" ||
      !evidence.system_host_pre.pagefile_configuration_available ||
      !evidence.system_host_post.pagefile_configuration_available ||
      evidence.system_host_pre.process_id == 0U ||
      evidence.system_host_pre.process_id !=
          evidence.system_host_post.process_id ||
      evidence.system_host_pre.process_image_path.empty() ||
      evidence.system_host_pre.process_image_path !=
          evidence.system_host_post.process_image_path ||
      evidence.system_host_pre.captured_monotonic_milliseconds == 0U ||
      evidence.system_host_pre.captured_monotonic_milliseconds >
          evidence.system_host_post.captured_monotonic_milliseconds ||
      evidence.maximum_host_sample_age_milliseconds == 0U ||
      evidence.maximum_host_sample_age_milliseconds > 500U ||
      evidence.evaluation_monotonic_milliseconds <
          evidence.system_host_post.captured_monotonic_milliseconds ||
      evidence.evaluation_monotonic_milliseconds -
              evidence.system_host_post.captured_monotonic_milliseconds >
          evidence.maximum_host_sample_age_milliseconds) {
    return downgrade("authoritative_system_host_evidence_required");
  }
  if (!evidence.gpu.available || !evidence.gpu.reconciled ||
      !evidence.gpu.process_device_corroboration_available ||
      (evidence.gpu.process_device_source != "nvml-process" &&
       evidence.gpu.process_device_source != "wddm-process") ||
      evidence.gpu.process_id == 0U ||
      evidence.gpu.process_device_captured_monotonic_milliseconds !=
          evidence.gpu.captured_monotonic_milliseconds ||
      evidence.gpu.process_device_current_bytes !=
          evidence.gpu.owned_current_bytes ||
      !evidence.gpu.adapter_identity_available ||
      evidence.gpu.captured_monotonic_milliseconds == 0U ||
      evidence.maximum_owned_gpu_sample_age_milliseconds == 0U ||
      evidence.maximum_owned_gpu_sample_age_milliseconds > 500U ||
      evidence.evaluation_monotonic_milliseconds <
          evidence.gpu.captured_monotonic_milliseconds ||
      evidence.evaluation_monotonic_milliseconds -
              evidence.gpu.captured_monotonic_milliseconds >
          evidence.maximum_owned_gpu_sample_age_milliseconds) {
    return downgrade("owned_gpu_reconciliation_required");
  }
  if (evidence.gpu.unknown_unreconciled_bytes != 0U) {
    return downgrade("unknown_gpu_bytes_present");
  }
  std::uint64_t reconciled_owned_current_bytes = 0U;
  std::uint64_t reconciled_owned_peak_bytes = 0U;
  if (!reconcile_owned_categories(evidence.gpu,
                                  &reconciled_owned_current_bytes,
                                  &reconciled_owned_peak_bytes) ||
      reconciled_owned_current_bytes != evidence.gpu.owned_current_bytes ||
      reconciled_owned_peak_bytes != evidence.gpu.owned_peak_bytes ||
      evidence.gpu.owned_current_bytes > evidence.gpu.owned_peak_bytes) {
    return downgrade("owned_gpu_category_reconciliation_failed");
  }
  if (!validate_gpu_hard_cap(evidence.gpu.hard_cap_bytes).accepted ||
      evidence.gpu.owned_current_bytes > evidence.gpu.hard_cap_bytes ||
      evidence.gpu.owned_peak_bytes > evidence.gpu.hard_cap_bytes) {
    return downgrade("owned_gpu_cap_exceeded", "rejected");
  }
  if (evidence.transfer_claim_requested) {
    auto transfers = evidence.transfers;
    const auto reconciliation = reconcile_transfer_sample(&transfers);
    if (!reconciliation.ok || transfers.dropped_records != 0U ||
        transfers.events.empty() ||
        (transfers.observed_h2d_completed_bytes == 0U &&
         transfers.observed_d2h_completed_bytes == 0U)) {
      return downgrade("actual_transfer_evidence_required");
    }
  }
  if (evidence.offload_file_claim_requested) {
    if (evidence.files.empty()) {
      return downgrade("file_identity_evidence_required", "storage-ambiguous");
    }
    bool source_seen = false;
    for (const auto& file : evidence.files) {
      source_seen = source_seen || file.role == "source-gguf";
      const auto validation =
          validate_file_io_evidence(file, evidence.cold_cache_claim_requested);
      if (!validation.ok) {
        return downgrade(validation.failure_reason, "storage-ambiguous");
      }
    }
    if (!source_seen) {
      return downgrade("source_file_identity_required", "storage-ambiguous");
    }
  }
  if (evidence.claim_scope == "owned-allocation") {
    return {true, "owned-allocation-cap-certified", ""};
  }
  if (evidence.instrumentation_mode != "ordinary") {
    return downgrade("ordinary_run_required_for_residency_claim");
  }
  if (!evidence.wddm.available ||
      evidence.wddm.captured_monotonic_milliseconds == 0U ||
      evidence.maximum_wddm_sample_age_milliseconds == 0U ||
      evidence.maximum_wddm_sample_age_milliseconds > 500U ||
      evidence.wddm.adapter_description.empty() ||
      evidence.wddm.local_budget_bytes == 0U ||
      evidence.wddm.local_available_for_reservation_bytes >
          evidence.wddm.local_budget_bytes ||
      evidence.wddm.nonlocal_available_for_reservation_bytes >
          evidence.wddm.nonlocal_budget_bytes ||
      evidence.evaluation_monotonic_milliseconds <
          evidence.wddm.captured_monotonic_milliseconds ||
      evidence.evaluation_monotonic_milliseconds -
              evidence.wddm.captured_monotonic_milliseconds >
          evidence.maximum_wddm_sample_age_milliseconds) {
    return downgrade("fresh_wddm_evidence_required");
  }
  if (!evidence.gpu.adapter_identity_available ||
      evidence.gpu.adapter_luid_high != evidence.wddm.adapter_luid_high ||
      evidence.gpu.adapter_luid_low != evidence.wddm.adapter_luid_low) {
    return downgrade("wddm_adapter_identity_mismatch");
  }
  if (evidence.wddm.local_current_usage_bytes >
          evidence.wddm.local_budget_bytes ||
      evidence.wddm.nonlocal_current_usage_bytes != 0U) {
    return downgrade("wddm_oversubscription_observed");
  }
  if (!evidence.eviction_residency_trace_available ||
      evidence.residency_trace_dropped_records != 0U) {
    return downgrade("complete_residency_trace_required");
  }
  if (evidence.eviction_or_nonlocal_residency_observed) {
    return downgrade("eviction_or_nonlocal_residency_observed");
  }
  return {true, "physical-residency-cap-certified", ""};
}

}  // namespace prisminfer

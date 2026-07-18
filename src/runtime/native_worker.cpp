#include "prisminfer/native_worker.h"

#include "prisminfer/checked_arithmetic.h"


#include <cstddef>
#include <array>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <psapi.h>
#endif

namespace prisminfer {

NativeWorkerTrustCatalog::NativeWorkerTrustCatalog(
    std::vector<NativeWorkerExecutableApproval> approvals)
    : approvals_(std::move(approvals)) {}

const NativeWorkerExecutableApproval* NativeWorkerTrustCatalog::find(
    const std::filesystem::path& executable_path) const {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(executable_path, error);
  if (error || canonical.empty()) return nullptr;
  for (const auto& approval : approvals_) {
    const auto approved = std::filesystem::weakly_canonical(
        approval.executable_path, error);
    if (!error && approved == canonical) return &approval;
    error.clear();
  }
  return nullptr;
}

std::wstring quote_windows_argument(const std::wstring& argument) {
  if (argument.empty()) {
    return L"\"\"";
  }
  if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
    return argument;
  }

  std::wstring quoted;
  quoted.push_back(L'\"');
  std::size_t slash_count = 0;
  for (const wchar_t character : argument) {
    if (character == L'\\') {
      ++slash_count;
      continue;
    }
    if (character == L'\"') {
      quoted.append((slash_count * 2U) + 1U, L'\\');
      quoted.push_back(L'\"');
      slash_count = 0;
      continue;
    }
    quoted.append(slash_count, L'\\');
    slash_count = 0;
    quoted.push_back(character);
  }
  quoted.append(slash_count * 2U, L'\\');
  quoted.push_back(L'\"');
  return quoted;
}

std::string sha256_regular_file(const std::filesystem::path& path) {
#if !defined(_WIN32)
  (void)path;
  return {};
#else
  const DWORD attributes = GetFileAttributesW(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES ||
      (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0U) {
    return {};
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                  nullptr, 0U) != 0) return {};
  DWORD object_size = 0;
  DWORD written = 0;
  if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                        reinterpret_cast<PUCHAR>(&object_size),
                        sizeof(object_size), &written, 0U) != 0) {
    BCryptCloseAlgorithmProvider(algorithm, 0U);
    return {};
  }
  std::vector<unsigned char> hash_object(object_size);
  if (BCryptCreateHash(algorithm, &hash, hash_object.data(),
                       static_cast<ULONG>(hash_object.size()), nullptr, 0U,
                       0U) != 0) {
    BCryptCloseAlgorithmProvider(algorithm, 0U);
    return {};
  }
  std::array<char, 64 * 1024> buffer{};
  while (input.good()) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto count = input.gcount();
    if (count > 0 && BCryptHashData(hash,
                                   reinterpret_cast<PUCHAR>(buffer.data()),
                                   static_cast<ULONG>(count), 0U) != 0) {
      BCryptDestroyHash(hash);
      BCryptCloseAlgorithmProvider(algorithm, 0U);
      return {};
    }
  }
  if (!input.eof()) {
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0U);
    return {};
  }
  std::array<unsigned char, 32> digest{};
  const bool finished = BCryptFinishHash(hash, digest.data(),
                                         static_cast<ULONG>(digest.size()),
                                         0U) == 0;
  BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(algorithm, 0U);
  if (!finished) return {};
  std::ostringstream result;
  result << std::hex << std::setfill('0');
  for (const unsigned char byte : digest) {
    result << std::setw(2) << static_cast<unsigned int>(byte);
  }
  return result.str();
#endif
}

#if defined(_WIN32)
namespace {

class UniqueHandle {
 public:
  UniqueHandle() = default;
  explicit UniqueHandle(HANDLE value) : value_(value) {}
  ~UniqueHandle() { reset(); }
  UniqueHandle(const UniqueHandle&) = delete;
  UniqueHandle& operator=(const UniqueHandle&) = delete;
  UniqueHandle(UniqueHandle&& other) noexcept : value_(other.release()) {}
  UniqueHandle& operator=(UniqueHandle&& other) noexcept {
    if (this != &other) reset(other.release());
    return *this;
  }

  HANDLE get() const { return value_; }
  HANDLE release() {
    HANDLE value = value_;
    value_ = nullptr;
    return value;
  }
  void reset(HANDLE value = nullptr) {
    if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
      CloseHandle(value_);
    }
    value_ = value;
  }

 private:
  HANDLE value_{nullptr};
};

struct TemporaryOutputArtifact {
  std::filesystem::path path;
  UniqueHandle handle;
  bool released{false};

  ~TemporaryOutputArtifact() {
    handle.reset();
    if (!released && !path.empty()) {
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  }
};

NativeWorkerResult fail(std::string reason) {
  NativeWorkerResult result;
  result.failure_reason = std::move(reason);
  return result;
}

bool is_sha256(const std::string& value) {
  return value.size() == 64U &&
         std::all_of(value.begin(), value.end(), [](unsigned char character) {
           return (character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f') ||
                  (character >= 'A' && character <= 'F');
         });
}

bool is_approval_identity(const std::string& value) {
  return !value.empty() && value.size() <= 256U &&
         std::all_of(value.begin(), value.end(), [](unsigned char character) {
           return std::isalnum(character) != 0 || character == '-' ||
                  character == '_' || character == '.' || character == ':' ||
                  character == '/' || character == '@';
         });
}

struct TreeMemoryAccumulator {
  std::uint64_t root_peak_working_set_bytes{0};
  std::uint64_t root_peak_private_commit_bytes{0};
  std::uint64_t tree_peak_working_set_bytes{0};
  std::uint32_t peak_active_processes{0};
  std::unordered_set<ULONG_PTR> observed_process_ids;
  std::vector<UniqueHandle> observed_process_handles;
};

bool checked_add_size(std::uint64_t left, SIZE_T right,
                      std::uint64_t* output) {
  if (output == nullptr ||
      static_cast<std::uint64_t>(right) >
          std::numeric_limits<std::uint64_t>::max() - left) {
    return false;
  }
  *output = left + static_cast<std::uint64_t>(right);
  return true;
}

bool sample_retained_root(HANDLE process, DWORD root_process_id,
                          TreeMemoryAccumulator* accumulator) {
  if (process == nullptr || root_process_id == 0U || accumulator == nullptr) {
    return false;
  }
  PROCESS_MEMORY_COUNTERS_EX memory{};
  memory.cb = sizeof(memory);
  if (!GetProcessMemoryInfo(
          process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
          sizeof(memory))) {
    return false;
  }
  accumulator->observed_process_ids.insert(root_process_id);
  accumulator->root_peak_working_set_bytes =
      std::max(accumulator->root_peak_working_set_bytes,
               static_cast<std::uint64_t>(memory.PeakWorkingSetSize));
  accumulator->root_peak_private_commit_bytes =
      std::max(accumulator->root_peak_private_commit_bytes,
               static_cast<std::uint64_t>(memory.PeakPagefileUsage));
  accumulator->tree_peak_working_set_bytes =
      std::max(accumulator->tree_peak_working_set_bytes,
               accumulator->root_peak_working_set_bytes);
  return true;
}

bool sample_job_tree(HANDLE job, DWORD root_process_id,
                     std::uint32_t maximum_processes,
                     TreeMemoryAccumulator* accumulator) {
  if (job == nullptr || accumulator == nullptr || maximum_processes == 0U) {
    return false;
  }
  const std::size_t bytes =
      offsetof(JOBOBJECT_BASIC_PROCESS_ID_LIST, ProcessIdList) +
      static_cast<std::size_t>(maximum_processes) * sizeof(ULONG_PTR);
  std::vector<std::byte> storage(bytes);
  auto* process_ids =
      reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(storage.data());
  if (!QueryInformationJobObject(job, JobObjectBasicProcessIdList,
                                 process_ids, static_cast<DWORD>(bytes),
                                 nullptr) ||
      process_ids->NumberOfProcessIdsInList !=
          process_ids->NumberOfAssignedProcesses) {
    return false;
  }
  accumulator->peak_active_processes =
      std::max(accumulator->peak_active_processes,
               static_cast<std::uint32_t>(
                   process_ids->NumberOfAssignedProcesses));
  std::uint64_t tree_working_set = 0U;
  for (DWORD index = 0; index < process_ids->NumberOfProcessIdsInList;
       ++index) {
    const ULONG_PTR process_id = process_ids->ProcessIdList[index];
    if (process_id == 0U || process_id > MAXDWORD) return false;
    UniqueHandle process(OpenProcess(PROCESS_QUERY_INFORMATION |
                                         PROCESS_VM_READ | SYNCHRONIZE,
                                     FALSE,
                                     static_cast<DWORD>(process_id)));
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    if (process.get() == nullptr ||
        !GetProcessMemoryInfo(
            process.get(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
            sizeof(memory)) ||
        !checked_add_size(tree_working_set, memory.WorkingSetSize,
                          &tree_working_set)) {
      return false;
    }
    const bool first_observation =
        accumulator->observed_process_ids.insert(process_id).second;
    if (process_id == root_process_id) {
      accumulator->root_peak_working_set_bytes =
          std::max(accumulator->root_peak_working_set_bytes,
                   static_cast<std::uint64_t>(memory.PeakWorkingSetSize));
      accumulator->root_peak_private_commit_bytes =
          std::max(accumulator->root_peak_private_commit_bytes,
                   static_cast<std::uint64_t>(memory.PeakPagefileUsage));
    }
    if (first_observation) {
      accumulator->observed_process_handles.push_back(std::move(process));
    }
  }
  accumulator->tree_peak_working_set_bytes =
      std::max(accumulator->tree_peak_working_set_bytes, tree_working_set);
  return true;
}

bool wait_for_job_empty(HANDLE job, std::uint32_t timeout_ms) {
  if (job == nullptr || timeout_ms == 0U) return false;
  const ULONGLONG started = GetTickCount64();
  for (;;) {
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
    if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                                   &accounting, sizeof(accounting), nullptr)) {
      return false;
    }
    if (accounting.ActiveProcesses == 0U) return true;
    const ULONGLONG elapsed = GetTickCount64() - started;
    if (elapsed >= timeout_ms) return false;
    const auto remaining = static_cast<DWORD>(timeout_ms - elapsed);
    Sleep(std::min<DWORD>(10U, remaining));
  }
}

bool terminate_job_tree(HANDLE job, HANDLE root_process,
                        const TreeMemoryAccumulator& accumulator,
                        std::uint32_t timeout_ms) {
  if (job == nullptr || root_process == nullptr || timeout_ms == 0U ||
      !TerminateJobObject(job, 1U)) {
    return false;
  }
  const ULONGLONG started = GetTickCount64();
  if (WaitForSingleObject(root_process, timeout_ms) != WAIT_OBJECT_0 ||
      !wait_for_job_empty(job, timeout_ms)) {
    return false;
  }
  for (const auto& process : accumulator.observed_process_handles) {
    const ULONGLONG elapsed = GetTickCount64() - started;
    if (elapsed >= timeout_ms ||
        WaitForSingleObject(process.get(),
                            static_cast<DWORD>(timeout_ms - elapsed)) !=
            WAIT_OBJECT_0) {
      return false;
    }
  }
  return true;
}

bool has_disallowed_windows_path_syntax(const std::filesystem::path& path) {
  const auto text = path.native();
  if (text.rfind(L"\\\\.\\", 0U) == 0U) return true;
  if (text.size() >= 14U &&
      CompareStringOrdinal(text.c_str(), 14, L"\\\\?\\GLOBALROOT", 14,
                           TRUE) == CSTR_EQUAL) {
    return true;
  }
  std::size_t colon_search_start = 0U;
  if (text.size() >= 2U && text[1] == L':') {
    colon_search_start = 2U;
  } else if (text.rfind(L"\\\\?\\", 0U) == 0U && text.size() >= 6U &&
             text[5] == L':') {
    colon_search_start = 6U;
  }
  const auto extra_colon = text.find(L':', colon_search_start);
  return extra_colon != std::wstring::npos;
}

std::filesystem::path canonical_policy_path(
    const std::filesystem::path& path, std::error_code& error) {
  if (path.native().rfind(L"\\\\?\\", 0U) == 0U) {
    error.clear();
    return path.lexically_normal();
  }
  return std::filesystem::weakly_canonical(path, error);
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return value;
}

bool is_under_root(const std::filesystem::path& path,
                   const std::filesystem::path& root) {
  const auto path_text = path.native();
  auto root_text = root.native();
  if (!root_text.empty() && root_text.back() != L'\\') root_text.push_back(L'\\');
  return path_text.size() > root_text.size() &&
         CompareStringOrdinal(path_text.c_str(),
                              static_cast<int>(root_text.size()),
                              root_text.c_str(),
                              static_cast<int>(root_text.size()), TRUE) == CSTR_EQUAL;
}

bool hash_open_file(HANDLE file, std::string* digest) {
  if (digest == nullptr || file == nullptr || file == INVALID_HANDLE_VALUE) {
    return false;
  }
  LARGE_INTEGER zero{};
  if (!SetFilePointerEx(file, zero, nullptr, FILE_BEGIN)) return false;
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                  nullptr, 0U) != 0) return false;
  DWORD object_size = 0;
  DWORD written = 0;
  if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                        reinterpret_cast<PUCHAR>(&object_size),
                        sizeof(object_size), &written, 0U) != 0) {
    BCryptCloseAlgorithmProvider(algorithm, 0U);
    return false;
  }
  std::vector<unsigned char> hash_object(object_size);
  if (BCryptCreateHash(algorithm, &hash, hash_object.data(),
                       static_cast<ULONG>(hash_object.size()), nullptr, 0U,
                       0U) != 0) {
    BCryptCloseAlgorithmProvider(algorithm, 0U);
    return false;
  }
  std::array<unsigned char, 64 * 1024> buffer{};
  DWORD read = 0;
  for (;;) {
    if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()),
                  &read, nullptr)) {
      BCryptDestroyHash(hash);
      BCryptCloseAlgorithmProvider(algorithm, 0U);
      return false;
    }
    if (read == 0U) break;
    if (BCryptHashData(hash, buffer.data(), read, 0U) != 0) {
      BCryptDestroyHash(hash);
      BCryptCloseAlgorithmProvider(algorithm, 0U);
      return false;
    }
  }
  std::array<unsigned char, 32> bytes{};
  const bool finished = BCryptFinishHash(hash, bytes.data(),
                                         static_cast<ULONG>(bytes.size()),
                                         0U) == 0;
  BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(algorithm, 0U);
  if (!finished || !SetFilePointerEx(file, zero, nullptr, FILE_BEGIN)) {
    return false;
  }
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const unsigned char byte : bytes) {
    output << std::setw(2) << static_cast<unsigned int>(byte);
  }
  *digest = output.str();
  return true;
}

std::filesystem::path final_path_from_handle(HANDLE handle) {
  const DWORD flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
  const DWORD required =
      GetFinalPathNameByHandleW(handle, nullptr, 0U, flags);
  if (required == 0U) return {};
  std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1U);
  const DWORD written = GetFinalPathNameByHandleW(
      handle, buffer.data(), static_cast<DWORD>(buffer.size()), flags);
  if (written == 0U || written >= buffer.size()) return {};
  return std::filesystem::path(std::wstring(buffer.data(), written));
}

std::vector<wchar_t> minimal_environment() {
  wchar_t system_root[MAX_PATH + 1]{};
  const DWORD length = GetEnvironmentVariableW(L"SystemRoot", system_root,
                                                MAX_PATH + 1);
  if (length == 0U || length >= MAX_PATH) return {};
  std::wstring block = L"SystemRoot=";
  block += system_root;
  block.push_back(L'\0');
  block += L"SystemDrive=";
  block += std::filesystem::path(system_root).root_name().wstring();
  block.push_back(L'\0');
  block.push_back(L'\0');
  return std::vector<wchar_t>(block.begin(), block.end());
}

bool create_opaque_output_artifact(
    const std::filesystem::path& temp_directory,
    TemporaryOutputArtifact* artifact) {
  if (artifact == nullptr) return false;
  for (int attempt = 0; attempt < 16; ++attempt) {
    std::array<unsigned char, 16> random_bytes{};
    if (BCryptGenRandom(nullptr, random_bytes.data(),
                        static_cast<ULONG>(random_bytes.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
      return false;
    }
    std::wostringstream name;
    name << L"prisminfer-" << std::hex << std::setfill(L'0');
    for (const unsigned char byte : random_bytes) {
      name << std::setw(2) << static_cast<unsigned int>(byte);
    }
    name << L".log";
    const auto candidate = temp_directory / name.str();
    UniqueHandle created(CreateFileW(
        candidate.c_str(), GENERIC_WRITE, 0U, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY, nullptr));
    if (created.get() != INVALID_HANDLE_VALUE) {
      artifact->path = candidate;
      artifact->handle = std::move(created);
      return true;
    }
    if (GetLastError() != ERROR_FILE_EXISTS &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
      return false;
    }
  }
  return false;
}

std::wstring build_command_line(const std::filesystem::path& executable,
                                const std::vector<std::wstring>& arguments) {
  std::wstring command = quote_windows_argument(executable.wstring());
  for (const auto& argument : arguments) {
    command.push_back(L' ');
    command += quote_windows_argument(argument);
  }
  return command;
}

}  // namespace
#endif

NativeWorkerResult run_native_worker(const NativeWorkerTrustCatalog& catalog,
                                     const NativeWorkerRequest& request) {
#if !defined(_WIN32)
  (void)catalog;
  (void)request;
  NativeWorkerResult result;
  result.failure_reason = "native_worker_windows_required";
  return result;
#else
  if (request.timeout_ms == 0U) {
    return fail("native_worker_timeout_required");
  }
  constexpr std::uint64_t kMaximumWorkerOutputBytes = 16U * 1024U * 1024U;
  if (request.max_output_bytes == 0U ||
      request.max_output_bytes > kMaximumWorkerOutputBytes) {
    return fail("native_worker_output_limit_required");
  }
  if (request.max_active_processes == 0U ||
      request.max_active_processes > 8U) {
    return fail("native_worker_process_limit_rejected");
  }
  const auto job_time_100ns =
      checked_multiply_u64(request.timeout_ms, 10'000U);
  if (request.max_job_memory_bytes == 0U ||
      request.max_job_memory_bytes >
          static_cast<std::uint64_t>(std::numeric_limits<SIZE_T>::max()) ||
      !job_time_100ns ||
      *job_time_100ns >
          static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max())) {
    return fail("native_worker_job_resource_limit_rejected");
  }
  if (has_disallowed_windows_path_syntax(request.executable_path)) {
    return fail("native_worker_executable_path_syntax_rejected");
  }
  const DWORD requested_attributes =
      GetFileAttributesW(request.executable_path.c_str());
  if (requested_attributes != INVALID_FILE_ATTRIBUTES &&
      (requested_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
    return fail("native_worker_executable_reparse_rejected");
  }
  const auto* approval = catalog.find(request.executable_path);
  if (approval == nullptr) {
    return fail("native_worker_executable_not_approved");
  }
  if (approval->trusted_executable_root.empty() ||
      !is_sha256(approval->expected_executable_sha256) ||
      !is_approval_identity(approval->approval_identity)) {
    return fail("native_worker_approval_invalid");
  }
  std::error_code canonical_error;
  const auto trusted_root = canonical_policy_path(
      approval->trusted_executable_root, canonical_error);
  const DWORD trusted_root_attributes =
      trusted_root.empty() ? INVALID_FILE_ATTRIBUTES :
      GetFileAttributesW(trusted_root.c_str());
  if (canonical_error || trusted_root.empty() ||
      trusted_root_attributes == INVALID_FILE_ATTRIBUTES ||
      (trusted_root_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0U ||
      (trusted_root_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
    return fail("native_worker_trusted_root_rejected");
  }
  UniqueHandle trusted_root_handle(CreateFileW(
      trusted_root.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
      nullptr));
  BY_HANDLE_FILE_INFORMATION trusted_root_info{};
  const auto final_trusted_root =
      trusted_root_handle.get() == INVALID_HANDLE_VALUE
          ? std::filesystem::path{}
          : final_path_from_handle(trusted_root_handle.get());
  if (trusted_root_handle.get() == INVALID_HANDLE_VALUE ||
      !GetFileInformationByHandle(trusted_root_handle.get(),
                                  &trusted_root_info) ||
      (trusted_root_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U ||
      (trusted_root_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U ||
      final_trusted_root.empty()) {
    return fail("native_worker_trusted_root_identity_rejected");
  }
  const auto executable = canonical_policy_path(
      request.executable_path, canonical_error);
  if (canonical_error || executable.empty()) {
    return fail("native_worker_executable_rejected");
  }
  UniqueHandle executable_handle(CreateFileW(
      executable.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  BY_HANDLE_FILE_INFORMATION executable_info{};
  std::string executable_hash;
  const auto final_executable =
      executable_handle.get() == INVALID_HANDLE_VALUE
          ? std::filesystem::path{}
          : final_path_from_handle(executable_handle.get());
  if (executable_handle.get() == INVALID_HANDLE_VALUE ||
      !GetFileInformationByHandle(executable_handle.get(), &executable_info) ||
       (executable_info.dwFileAttributes &
        (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0U ||
      executable_info.nNumberOfLinks != 1U ||
      final_executable.empty() ||
      !is_under_root(final_executable, final_trusted_root) ||
      final_executable.parent_path() != final_trusted_root ||
      !hash_open_file(executable_handle.get(), &executable_hash) ||
      lowercase(executable_hash) != lowercase(approval->expected_executable_sha256)) {
    return fail("native_worker_executable_identity_rejected");
  }
  if (request.approved_read_only_inputs.size() > 8U) {
    return fail("native_worker_input_approval_count_rejected");
  }
  std::vector<UniqueHandle> retained_input_roots;
  std::vector<UniqueHandle> retained_inputs;
  retained_input_roots.reserve(request.approved_read_only_inputs.size());
  retained_inputs.reserve(request.approved_read_only_inputs.size());
  for (const auto& input : request.approved_read_only_inputs) {
    if (input.path.empty() || input.trusted_root.empty() ||
        !is_sha256(input.expected_sha256) ||
        !is_approval_identity(input.approval_identity) ||
        has_disallowed_windows_path_syntax(input.path)) {
      return fail("native_worker_input_approval_invalid");
    }
    auto input_root = canonical_policy_path(input.trusted_root, canonical_error);
    auto input_path = canonical_policy_path(input.path, canonical_error);
    if (canonical_error || input_root.empty() || input_path.empty() ||
        input_path.parent_path() != input_root) {
      return fail("native_worker_input_path_rejected");
    }
    UniqueHandle input_root_handle(CreateFileW(
        input_root.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    UniqueHandle input_handle(CreateFileW(
        input_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION input_root_info{};
    BY_HANDLE_FILE_INFORMATION input_info{};
    const auto final_input_root =
        input_root_handle.get() == INVALID_HANDLE_VALUE
            ? std::filesystem::path{}
            : final_path_from_handle(input_root_handle.get());
    const auto final_input =
        input_handle.get() == INVALID_HANDLE_VALUE
            ? std::filesystem::path{}
            : final_path_from_handle(input_handle.get());
    std::string input_hash;
    if (input_root_handle.get() == INVALID_HANDLE_VALUE ||
        input_handle.get() == INVALID_HANDLE_VALUE ||
        !GetFileInformationByHandle(input_root_handle.get(),
                                    &input_root_info) ||
        !GetFileInformationByHandle(input_handle.get(), &input_info) ||
        (input_root_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U ||
        (input_root_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) !=
            0U ||
        (input_info.dwFileAttributes &
         (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0U ||
        input_info.nNumberOfLinks != 1U || final_input_root.empty() ||
        final_input.empty() || final_input.parent_path() != final_input_root ||
        !hash_open_file(input_handle.get(), &input_hash) ||
        lowercase(input_hash) != lowercase(input.expected_sha256)) {
      return fail("native_worker_input_identity_rejected");
    }
    retained_input_roots.push_back(std::move(input_root_handle));
    retained_inputs.push_back(std::move(input_handle));
  }
  const auto environment = minimal_environment();
  if (environment.empty()) {
    return fail("native_worker_environment_or_dll_policy_failed");
  }

  wchar_t temp_directory[MAX_PATH + 1]{};
  if (GetTempPathW(MAX_PATH, temp_directory) == 0U) {
    return fail("native_worker_temp_directory_unavailable");
  }
  TemporaryOutputArtifact temporary_output;
  if (!create_opaque_output_artifact(std::filesystem::path(temp_directory),
                                     &temporary_output)) {
    return fail("native_worker_output_create_failed");
  }
  NativeWorkerResult result;
  result.output_path = temporary_output.path;
  result.executable_sha256 = executable_hash;
  result.approval_identity = approval->approval_identity;
  result.output_limit_bytes = request.max_output_bytes;
  result.job_memory_limit_bytes = request.max_job_memory_bytes;
  result.job_cpu_time_limit_milliseconds = request.timeout_ms;
  SECURITY_ATTRIBUTES pipe_security{};
  pipe_security.nLength = sizeof(pipe_security);
  pipe_security.bInheritHandle = TRUE;
  HANDLE output_read_raw = nullptr;
  HANDLE output_write_raw = nullptr;
  if (!CreatePipe(&output_read_raw, &output_write_raw, &pipe_security, 4096U)) {
    return fail("native_worker_output_pipe_failed");
  }
  UniqueHandle output_read(output_read_raw);
  UniqueHandle output_write(output_write_raw);
  if (!SetHandleInformation(output_read.get(), HANDLE_FLAG_INHERIT, 0U)) {
    return fail("native_worker_output_inherit_failed");
  }
  UniqueHandle null_input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ |
                                               FILE_SHARE_WRITE,
                                      nullptr, OPEN_EXISTING, 0, nullptr));
  if (null_input.get() == INVALID_HANDLE_VALUE ||
      !SetHandleInformation(null_input.get(), HANDLE_FLAG_INHERIT,
                            HANDLE_FLAG_INHERIT)) {
    return fail("native_worker_stdio_setup_failed");
  }

  UniqueHandle job(CreateJobObjectW(nullptr, nullptr));
  if (job.get() == nullptr) {
    return fail("native_worker_job_create_failed");
  }
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags =
      JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION |
      JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_JOB_MEMORY |
      JOB_OBJECT_LIMIT_JOB_TIME;
  limits.BasicLimitInformation.ActiveProcessLimit = request.max_active_processes;
  limits.BasicLimitInformation.PerJobUserTimeLimit.QuadPart =
      static_cast<LONGLONG>(*job_time_100ns);
  limits.JobMemoryLimit = static_cast<SIZE_T>(request.max_job_memory_bytes);
  if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                               &limits, sizeof(limits))) {
    return fail("native_worker_job_limit_failed");
  }

  SIZE_T attribute_size = 0;
  InitializeProcThreadAttributeList(nullptr, 2U, 0U, &attribute_size);
  std::vector<std::byte> attribute_storage(attribute_size);
  auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
      attribute_storage.data());
  if (!InitializeProcThreadAttributeList(attributes, 2U, 0U,
                                         &attribute_size)) {
    return fail("native_worker_attribute_list_failed");
  }
  struct AttributeListCleanup {
    LPPROC_THREAD_ATTRIBUTE_LIST attributes;
    ~AttributeListCleanup() { DeleteProcThreadAttributeList(attributes); }
  } cleanup{attributes};
  HANDLE inherited_handles[] = {null_input.get(), output_write.get()};
  if (!UpdateProcThreadAttribute(attributes, 0U,
                                 PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                 inherited_handles, sizeof(inherited_handles),
                                 nullptr, nullptr)) {
    return fail("native_worker_handle_restriction_failed");
  }
  const std::uint64_t mitigation_policy =
      PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_REMOTE_ALWAYS_ON |
      PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_LOW_LABEL_ALWAYS_ON |
      PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON;
  if (!UpdateProcThreadAttribute(
          attributes, 0U, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
          const_cast<std::uint64_t*>(&mitigation_policy), sizeof(mitigation_policy),
          nullptr, nullptr)) {
    return fail("native_worker_child_dll_policy_failed");
  }

  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = null_input.get();
  startup.StartupInfo.hStdOutput = output_write.get();
  startup.StartupInfo.hStdError = output_write.get();
  startup.lpAttributeList = attributes;
  PROCESS_INFORMATION process{};
  const ULONGLONG launch_identity_ticks = GetTickCount64();
  auto command_line = build_command_line(final_executable, request.arguments);
  command_line.push_back(L'\0');
  std::filesystem::path working_directory = final_executable.parent_path();
  if (working_directory.native().size() >= MAX_PATH) {
    wchar_t system_directory[MAX_PATH + 1]{};
    const UINT system_directory_length =
        GetSystemDirectoryW(system_directory, MAX_PATH + 1);
    if (system_directory_length == 0U || system_directory_length > MAX_PATH) {
      return fail("native_worker_working_directory_unavailable");
    }
    working_directory = system_directory;
  }
  const DWORD creation_flags = CREATE_SUSPENDED |
                               EXTENDED_STARTUPINFO_PRESENT |
                               CREATE_UNICODE_ENVIRONMENT;
  if (!CreateProcessW(final_executable.c_str(), command_line.data(), nullptr,
                      nullptr, TRUE, creation_flags,
                      const_cast<wchar_t*>(environment.data()),
                      working_directory.c_str(), &startup.StartupInfo,
                      &process)) {
    return fail("native_worker_create_failed");
  }
  UniqueHandle process_handle(process.hProcess);
  UniqueHandle thread_handle(process.hThread);
  result.root_process_id = process.dwProcessId;
  result.job_identity =
      "job:" + std::to_string(GetCurrentProcessId()) + ":" +
      std::to_string(process.dwProcessId) + ":" +
      std::to_string(launch_identity_ticks);
  output_write.reset();
  if (!AssignProcessToJobObject(job.get(), process_handle.get())) {
    if (!TerminateProcess(process_handle.get(), 1U) ||
        WaitForSingleObject(process_handle.get(), request.timeout_ms) !=
            WAIT_OBJECT_0) {
      return fail("native_worker_job_assign_cleanup_failed");
    }
    return fail("native_worker_job_assign_failed");
  }
  constexpr std::uint32_t kTreeSampleIntervalMilliseconds = 10U;
  TreeMemoryAccumulator tree_memory;
  // The suspended root is the only race-free point at which the supervisor
  // can guarantee that even a one-shot worker is represented in tree evidence.
  if (!sample_job_tree(job.get(), process.dwProcessId,
                       request.max_active_processes, &tree_memory)) {
    if (!terminate_job_tree(job.get(), process_handle.get(), tree_memory,
                            request.timeout_ms)) {
      return fail("native_worker_tree_sample_cleanup_failed");
    }
    return fail("native_worker_tree_sample_failed");
  }
  if (ResumeThread(thread_handle.get()) == static_cast<DWORD>(-1)) {
    if (!terminate_job_tree(job.get(), process_handle.get(), tree_memory,
                            request.timeout_ms)) {
      return fail("native_worker_resume_cleanup_failed");
    }
    return fail("native_worker_resume_failed");
  }
  const ULONGLONG deadline = GetTickCount64() + request.timeout_ms;
  DWORD wait = WAIT_TIMEOUT;
  bool tree_complete = false;
  bool job_query_failed = false;
  bool output_limit_exceeded = false;
  std::string captured_output;
  captured_output.reserve(static_cast<std::size_t>(request.max_output_bytes));
  const auto drain_output = [&]() {
    for (;;) {
      DWORD available = 0U;
      if (!PeekNamedPipe(output_read.get(), nullptr, 0U, nullptr, &available,
                         nullptr)) {
        return GetLastError() == ERROR_BROKEN_PIPE;
      }
      if (available == 0U) return true;
      std::array<char, 64U * 1024U> buffer{};
      const DWORD requested =
          std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
      DWORD read = 0U;
      if (!ReadFile(output_read.get(), buffer.data(), requested, &read,
                    nullptr)) {
        return GetLastError() == ERROR_BROKEN_PIPE;
      }
      if (captured_output.size() + read > request.max_output_bytes) {
        output_limit_exceeded = true;
        return true;
      }
      captured_output.insert(captured_output.end(), buffer.data(),
                             buffer.data() + read);
    }
  };
  while (GetTickCount64() < deadline) {
    wait = WaitForSingleObject(process_handle.get(), 10U);
    if (!drain_output()) return fail("native_worker_output_read_failed");
    if (output_limit_exceeded) break;
    if (!sample_job_tree(job.get(), process.dwProcessId,
                         request.max_active_processes, &tree_memory)) {
      job_query_failed = true;
      break;
    }
    if (wait == WAIT_OBJECT_0) {
      JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
      if (!QueryInformationJobObject(job.get(),
                                     JobObjectBasicAccountingInformation,
                                     &accounting, sizeof(accounting), nullptr)) {
        job_query_failed = true;
        break;
      }
      if (accounting.ActiveProcesses == 0U) {
        tree_complete = true;
        break;
      }
      Sleep(kTreeSampleIntervalMilliseconds);
    } else if (wait == WAIT_FAILED) {
      break;
    }
  }
  if (job_query_failed) {
    if (!terminate_job_tree(job.get(), process_handle.get(), tree_memory,
                            request.timeout_ms)) {
      return fail("native_worker_job_query_cleanup_failed");
    }
    return fail("native_worker_job_query_failed");
  }
  if (output_limit_exceeded) {
    if (!terminate_job_tree(job.get(), process_handle.get(), tree_memory,
                            request.timeout_ms)) {
      return fail("native_worker_output_limit_cleanup_failed");
    }
    result.failure_reason = "native_worker_output_limit_exceeded";
    result.output_bytes = request.max_output_bytes;
    return result;
  }
  if (!tree_complete && wait != WAIT_FAILED &&
      GetTickCount64() >= deadline) {
    result.timed_out = true;
    if (request.simulate_termination_api_failure) {
      job.reset();
      (void)WaitForSingleObject(process_handle.get(), request.timeout_ms);
      return fail("native_worker_timeout_cleanup_failed");
    }
    if (!terminate_job_tree(job.get(), process_handle.get(), tree_memory,
                            request.timeout_ms)) {
      return fail("native_worker_timeout_cleanup_failed");
    }
  } else if (!tree_complete) {
    if (!terminate_job_tree(job.get(), process_handle.get(), tree_memory,
                            request.timeout_ms)) {
      return fail("native_worker_wait_cleanup_failed");
    }
    return fail("native_worker_wait_failed");
  }
  if (!drain_output()) return fail("native_worker_output_read_failed");
  if (output_limit_exceeded) {
    result.failure_reason = "native_worker_output_limit_exceeded";
    result.output_bytes = request.max_output_bytes;
    return result;
  }
  result.output_bytes = captured_output.size();
  std::size_t written_total = 0U;
  while (written_total < captured_output.size()) {
    DWORD written = 0U;
    const auto remaining = captured_output.size() - written_total;
    const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(
        remaining, static_cast<std::size_t>(MAXDWORD)));
    if (!WriteFile(temporary_output.handle.get(),
                   captured_output.data() + written_total, requested, &written,
                   nullptr) || written == 0U) {
      return fail("native_worker_output_write_failed");
    }
    written_total += written;
  }
  if (!FlushFileBuffers(temporary_output.handle.get())) {
    return fail("native_worker_output_flush_failed");
  }
  temporary_output.handle.reset();
  result.captured_output = std::move(captured_output);
  DWORD exit_code = 0;
  if (!GetExitCodeProcess(process_handle.get(), &exit_code)) {
    return fail("native_worker_exit_query_failed");
  }
  result.exit_code = exit_code;
  if (!sample_retained_root(process_handle.get(), process.dwProcessId,
                            &tree_memory)) {
    return fail("native_worker_root_evidence_unavailable");
  }
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_memory{};
  JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION job_io{};
  if (!QueryInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                 &job_memory, sizeof(job_memory), nullptr) ||
      !QueryInformationJobObject(job.get(),
                                 JobObjectBasicAndIoAccountingInformation,
                                 &job_io, sizeof(job_io), nullptr)) {
    return fail("native_worker_evidence_unavailable");
  }
  if (job_io.BasicInfo.TotalProcesses == 0U ||
      tree_memory.observed_process_ids.size() !=
          job_io.BasicInfo.TotalProcesses ||
      tree_memory.observed_process_ids.count(process.dwProcessId) != 1U ||
      tree_memory.peak_active_processes == 0U ||
      tree_memory.peak_active_processes > request.max_active_processes ||
      tree_memory.root_peak_working_set_bytes == 0U ||
      tree_memory.root_peak_private_commit_bytes == 0U ||
      tree_memory.tree_peak_working_set_bytes <
          tree_memory.root_peak_working_set_bytes ||
      job_memory.PeakJobMemoryUsed == 0U) {
    return fail("native_worker_process_tree_evidence_incomplete");
  }
  result.job_total_processes = job_io.BasicInfo.TotalProcesses;
  result.job_peak_active_processes = tree_memory.peak_active_processes;
  result.job_total_terminated_processes =
      job_io.BasicInfo.TotalTerminatedProcesses;
  result.root_peak_working_set_bytes =
      tree_memory.root_peak_working_set_bytes;
  result.root_peak_private_commit_bytes =
      tree_memory.root_peak_private_commit_bytes;
  result.tree_peak_working_set_bytes =
      tree_memory.tree_peak_working_set_bytes;
  result.tree_peak_private_commit_bytes = job_memory.PeakJobMemoryUsed;
  result.tree_sample_interval_milliseconds =
      kTreeSampleIntervalMilliseconds;
  result.read_bytes = job_io.IoInfo.ReadTransferCount;
  result.write_bytes = job_io.IoInfo.WriteTransferCount;
  result.evidence_available = true;
  result.ok = !result.timed_out && result.exit_code == 0U;
  result.failure_reason = result.ok ? "" :
      (result.timed_out ? "native_worker_timeout" : "native_worker_child_failed");
  if (result.ok) {
    temporary_output.released = true;
  } else {
    // Failed and timed-out runs retain bounded captured bytes in memory, but
    // never publish a pathname whose cleanup is delegated to a caller that may
    // return early. The artifact RAII removes the file before this result is
    // returned.
    result.output_path.clear();
  }
  return result;
#endif
}

}  // namespace prisminfer

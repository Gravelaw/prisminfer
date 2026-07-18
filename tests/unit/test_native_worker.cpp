#include <iostream>
#include <fstream>
#include <future>
#include <string>

#include "prisminfer/native_worker.h"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
  if (argc == 2 && std::string(argv[1]) == "--child") {
    BOOL in_job = FALSE;
    if (!IsProcessInJob(GetCurrentProcess(), nullptr, &in_job)) return 2;
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY image_load{};
    if (!GetProcessMitigationPolicy(GetCurrentProcess(), ProcessImageLoadPolicy,
                                    &image_load, sizeof(image_load))) return 3;
    std::cout << "native-worker-child job=" << (in_job ? "1" : "0")
              << " system32=" << (image_load.PreferSystem32Images ? "1" : "0")
              << "\n";
    return in_job && image_load.PreferSystem32Images ? 0 : 4;
  }
  if (argc == 2 && std::string(argv[1]) == "--hang") {
    Sleep(1000U);
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--hang-short") {
    Sleep(150U);
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--hang-long") {
    Sleep(10'000U);
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--flood") {
    const std::string chunk(4096U, 'x');
    for (int index = 0; index < 1024; ++index) {
      std::cout << chunk;
    }
    return 0;
  }
  if (argc >= 2 && std::string(argv[1]) == "--verify-argv") {
    int wide_argc = 0;
    LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    if (wide_argv == nullptr) return 5;
    const std::wstring long_argument(600U, L'x');
    const bool matched = wide_argc == 5 &&
        std::wstring(wide_argv[2]) == L"&|<>%^!\"" &&
        std::wstring(wide_argv[3]) == L"snow-\u96ea path" &&
        std::wstring(wide_argv[4]) == long_argument;
    LocalFree(wide_argv);
    std::cout << "literal-argv=" << (matched ? "1" : "0") << "\n";
    return matched ? 0 : 6;
  }
  if (argc == 2 && std::string(argv[1]) == "--spawn-descendant") {
    wchar_t module_path[32768]{};
    const DWORD module_length =
        GetModuleFileNameW(nullptr, module_path, 32768U);
    if (module_length == 0U || module_length >= 32768U) return 7;
    std::wstring command =
        prisminfer::quote_windows_argument(module_path) + L" --hang";
    command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION descendant{};
    const BOOL created = CreateProcessW(
        module_path, command.data(), nullptr, nullptr, FALSE, 0U, nullptr,
        nullptr, &startup, &descendant);
    bool contained = !created;
    if (created) {
      contained = WaitForSingleObject(descendant.hProcess, 500U) == WAIT_OBJECT_0;
      if (!contained) TerminateProcess(descendant.hProcess, 1U);
      CloseHandle(descendant.hThread);
      CloseHandle(descendant.hProcess);
    }
    std::cout << "descendant-contained=" << (contained ? "1" : "0") << "\n";
    return contained ? 0 : 8;
  }
  if (argc == 2 && std::string(argv[1]) == "--spawn-descendant-clean") {
    wchar_t module_path[32768]{};
    const DWORD module_length =
        GetModuleFileNameW(nullptr, module_path, 32768U);
    if (module_length == 0U || module_length >= 32768U) return 13;
    std::wstring command =
        prisminfer::quote_windows_argument(module_path) + L" --hang-short";
    command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION descendant{};
    if (!CreateProcessW(module_path, command.data(), nullptr, nullptr, FALSE,
                        0U, nullptr, nullptr, &startup, &descendant)) {
      return 14;
    }
    const DWORD wait = WaitForSingleObject(descendant.hProcess, 2000U);
    CloseHandle(descendant.hThread);
    CloseHandle(descendant.hProcess);
    return wait == WAIT_OBJECT_0 ? 0 : 15;
  }
  if (argc == 2 && std::string(argv[1]) == "--spawn-descendant-tree") {
    wchar_t module_path[32768]{};
    const DWORD module_length =
        GetModuleFileNameW(nullptr, module_path, 32768U);
    if (module_length == 0U || module_length >= 32768U) return 9;
    std::wstring command =
        prisminfer::quote_windows_argument(module_path) + L" --hang-long";
    command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION descendant{};
    if (!CreateProcessW(module_path, command.data(), nullptr, nullptr, FALSE,
                        0U, nullptr, nullptr, &startup, &descendant)) {
      return 10;
    }
    std::cout << "descendant-pid=" << descendant.dwProcessId << "\n"
              << std::flush;
    CloseHandle(descendant.hThread);
    CloseHandle(descendant.hProcess);
    Sleep(10'000U);
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--spawn-descendant-and-exit") {
    wchar_t module_path[32768]{};
    const DWORD module_length =
        GetModuleFileNameW(nullptr, module_path, 32768U);
    if (module_length == 0U || module_length >= 32768U) return 11;
    std::wstring command =
        prisminfer::quote_windows_argument(module_path) + L" --hang-long";
    command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION descendant{};
    if (!CreateProcessW(module_path, command.data(), nullptr, nullptr, FALSE,
                        0U, nullptr, nullptr, &startup, &descendant)) {
      return 12;
    }
    std::cout << "orphan-pid=" << descendant.dwProcessId << "\n"
              << std::flush;
    CloseHandle(descendant.hThread);
    CloseHandle(descendant.hProcess);
    // Keep the root alive for multiple supervisor sample periods so the test
    // deterministically proves the later root-exited/descendant-live state.
    Sleep(100U);
    return 0;
  }
#endif
  using prisminfer::quote_windows_argument;
  if (expect(quote_windows_argument(L"plain") == L"plain",
             "plain argument remains unquoted")) return 1;
  if (expect(quote_windows_argument(L"") == L"\"\"",
             "empty argument is represented")) return 1;
  if (expect(quote_windows_argument(L"two words") == L"\"two words\"",
             "space argument is quoted")) return 1;
  if (expect(quote_windows_argument(L"a\"b") == L"\"a\\\"b\"",
             "embedded quote is escaped")) return 1;
  if (expect(quote_windows_argument(L"C:\\path with space\\") ==
                 L"\"C:\\path with space\\\\\"",
             "trailing slash before closing quote is doubled")) return 1;
#if defined(_WIN32)
  prisminfer::NativeWorkerRequest request;
  request.executable_path = std::filesystem::absolute(argv[0]);
  const auto test_hash = prisminfer::sha256_regular_file(request.executable_path);
  if (expect(!test_hash.empty(),
             "test executable has a stable SHA-256")) return 1;
  prisminfer::NativeWorkerTrustCatalog catalog({{
      request.executable_path, request.executable_path.parent_path(), test_hash,
      "test-approval"}});
  request.arguments = {L"--child"};
  request.timeout_ms = 2000U;
  auto missing_job_limit = request;
  missing_job_limit.max_job_memory_bytes = 0U;
  const auto rejected_job_limit =
      prisminfer::run_native_worker(catalog, missing_job_limit);
  if (expect(!rejected_job_limit.ok &&
                 rejected_job_limit.failure_reason ==
                     "native_worker_job_resource_limit_rejected",
             "missing Job memory limit is rejected before creation")) {
    return 1;
  }
  prisminfer::NativeWorkerTrustCatalog wrong_hash_catalog({{
      request.executable_path, request.executable_path.parent_path(),
      std::string(64U, '0'), "test-approval"}});
  const auto bad_identity = prisminfer::run_native_worker(wrong_hash_catalog, request);
  if (expect(!bad_identity.ok &&
                 bad_identity.failure_reason ==
                     "native_worker_executable_identity_rejected",
             "mismatched executable hash is rejected before launch")) return 1;
  auto unapproved = request;
  unapproved.executable_path = request.executable_path.parent_path() /
                              L"not-an-approved-executable.exe";
  const auto bad_root = prisminfer::run_native_worker(catalog, unapproved);
  if (expect(!bad_root.ok &&
                 bad_root.failure_reason == "native_worker_executable_not_approved",
             "plan cannot select an executable outside supervisor approval")) return 1;
  const auto child = prisminfer::run_native_worker(catalog, request);
  if (!child.ok) {
    std::cerr << "native worker failure: " << child.failure_reason
              << " exit=" << child.exit_code
              << " output=" << child.output_path.string() << "\n";
  }
  if (expect(child.ok, "suspended Job child exits cleanly")) return 1;
  if (expect(child.exit_code == 0U, "Job child exit code")) return 1;
  if (expect(child.evidence_available &&
                  child.executable_sha256 == test_hash &&
                  child.approval_identity == "test-approval" &&
                  child.output_limit_bytes == request.max_output_bytes &&
                  child.job_memory_limit_bytes ==
                      request.max_job_memory_bytes &&
                  child.job_cpu_time_limit_milliseconds == request.timeout_ms,
              "worker retains approval and bounded-output evidence")) return 1;
  if (expect(child.root_process_id != 0U && !child.job_identity.empty() &&
                 child.job_total_processes == 1U &&
                 child.job_peak_active_processes == 1U &&
                 child.root_peak_working_set_bytes > 0U &&
                 child.root_peak_private_commit_bytes > 0U &&
                 child.tree_peak_working_set_bytes >=
                     child.root_peak_working_set_bytes &&
                 child.tree_peak_private_commit_bytes >=
                     child.root_peak_private_commit_bytes &&
                 child.tree_sample_interval_milliseconds == 10U,
             "root and complete Job process-tree memory evidence is retained")) {
    return 1;
  }
  const auto approved_input_path =
      request.executable_path.parent_path() / "approved-worker-input.gguf";
  {
    std::ofstream approved_input(approved_input_path,
                                 std::ios::binary | std::ios::trunc);
    approved_input << "approved-model-bytes";
  }
  const auto approved_input_hash =
      prisminfer::sha256_regular_file(approved_input_path);
  auto approved_input_request = request;
  approved_input_request.approved_read_only_inputs.push_back(
      {approved_input_path, approved_input_path.parent_path(),
       approved_input_hash, "packet-a-test-input"});
  const auto approved_input_worker =
      prisminfer::run_native_worker(catalog, approved_input_request);
  if (expect(approved_input_worker.ok,
             "approved read-only input remains identity-bound through exit")) {
    return 1;
  }
  approved_input_request.approved_read_only_inputs.front().expected_sha256 =
      std::string(64U, '0');
  const auto rejected_input_worker =
      prisminfer::run_native_worker(catalog, approved_input_request);
  std::error_code approved_input_remove_error;
  std::filesystem::remove(approved_input_path, approved_input_remove_error);
  if (expect(!rejected_input_worker.ok &&
                 rejected_input_worker.failure_reason ==
                     "native_worker_input_identity_rejected",
             "mismatched approved input is rejected before worker resume")) {
    return 1;
  }
  const auto hardlink_path = request.executable_path.parent_path() /
                             L"prisminfer-native-worker-hardlink.exe";
  std::error_code hardlink_error;
  std::filesystem::remove(hardlink_path, hardlink_error);
  const BOOL hardlinked =
      CreateHardLinkW(hardlink_path.c_str(), request.executable_path.c_str(),
                      nullptr);
  if (expect(hardlinked != FALSE,
             "test executable hard link is created")) {
    return 1;
  }
  prisminfer::NativeWorkerTrustCatalog hardlink_catalog({{
      hardlink_path, request.executable_path.parent_path(), test_hash,
      "hardlink-approval"}});
  auto hardlink_request = request;
  hardlink_request.executable_path = hardlink_path;
  const auto hardlink_result =
      prisminfer::run_native_worker(hardlink_catalog, hardlink_request);
  std::filesystem::remove(hardlink_path, hardlink_error);
  if (expect(!hardlink_result.ok &&
                 hardlink_result.failure_reason ==
                     "native_worker_executable_identity_rejected",
             "multi-link executable identity is rejected")) {
    return 1;
  }
  const auto nested_directory = request.executable_path.parent_path() /
                                L"prisminfer-unheld-nested-root";
  const auto nested_executable = nested_directory / L"worker-copy.exe";
  std::error_code nested_error;
  std::filesystem::create_directories(nested_directory, nested_error);
  std::filesystem::copy_file(request.executable_path, nested_executable,
                             std::filesystem::copy_options::overwrite_existing,
                             nested_error);
  if (expect(!nested_error, "nested executable fixture is created")) return 1;
  prisminfer::NativeWorkerTrustCatalog nested_catalog({{
      nested_executable, request.executable_path.parent_path(), test_hash,
      "nested-approval"}});
  auto nested_request = request;
  nested_request.executable_path = nested_executable;
  const auto nested_result =
      prisminfer::run_native_worker(nested_catalog, nested_request);
  std::filesystem::remove_all(nested_directory, nested_error);
  if (expect(!nested_result.ok &&
                 nested_result.failure_reason ==
                     "native_worker_executable_identity_rejected",
             "unheld intermediate directory identity is rejected")) {
    return 1;
  }
  const auto& output_text = child.captured_output;
  bool path_tamper_written = false;
  {
    std::ofstream tampered_output(child.output_path,
                                  std::ios::binary | std::ios::trunc);
    tampered_output << "spoofed-path-bytes";
    tampered_output.flush();
    path_tamper_written = tampered_output.good();
  }
  std::error_code remove_error;
  std::filesystem::remove(child.output_path, remove_error);
  if (expect(path_tamper_written &&
                 output_text.find("native-worker-child job=1 system32=1") !=
                 std::string::npos,
             "captured bytes remain authoritative after path replacement")) {
    return 1;
  }

  request.arguments = {L"--hang"};
  request.timeout_ms = 50U;
  const auto timed_out = prisminfer::run_native_worker(catalog, request);
  std::filesystem::remove(timed_out.output_path, remove_error);
  if (expect(!timed_out.ok && timed_out.timed_out,
             "timeout terminates the contained child")) return 1;
  request.arguments = {L"--flood"};
  request.timeout_ms = 2000U;
  request.max_output_bytes = 16U * 1024U;
  const auto flooded = prisminfer::run_native_worker(catalog, request);
  if (expect(!flooded.ok &&
                 flooded.failure_reason == "native_worker_output_limit_exceeded",
             "output limit terminates and rejects a noisy child")) return 1;

  request.arguments = {L"--verify-argv", L"&|<>%^!\"", L"snow-\u96ea path",
                       std::wstring(600U, L'x')};
  request.timeout_ms = 2000U;
  request.max_output_bytes = 64U * 1024U;
  const auto literal = prisminfer::run_native_worker(catalog, request);
  if (expect(literal.ok, "Unicode, metacharacter, and long argv round-trip")) {
    return 1;
  }
  const auto& literal_text = literal.captured_output;
  std::filesystem::remove(literal.output_path, remove_error);
  if (expect(literal_text.find("literal-argv=1") != std::string::npos,
             "child observes literal argv without shell expansion")) return 1;

  auto long_root = std::filesystem::temp_directory_path() /
                   L"prisminfer-native-worker-long-root";
  while (long_root.wstring().size() < 300U) {
    long_root /= L"segment-0123456789abcdef";
  }
  std::wstring extended_root_text = long_root.wstring();
  if (extended_root_text.rfind(L"\\\\?\\", 0U) != 0U) {
    extended_root_text.insert(0U, L"\\\\?\\");
  }
  const std::filesystem::path extended_root(extended_root_text);
  const auto extended_executable = extended_root / L"worker-copy.exe";
  std::error_code long_path_error;
  std::filesystem::create_directories(extended_root, long_path_error);
  if (expect(!long_path_error, "extended-length trusted root is created")) {
    return 1;
  }
  std::filesystem::copy_file(request.executable_path, extended_executable,
                             std::filesystem::copy_options::overwrite_existing,
                             long_path_error);
  if (expect(!long_path_error, "extended-length executable is copied")) {
    return 1;
  }
  const auto long_hash = prisminfer::sha256_regular_file(extended_executable);
  prisminfer::NativeWorkerTrustCatalog long_catalog({{
      extended_executable, extended_root, long_hash, "long-path-approval"}});
  auto long_request = request;
  long_request.executable_path = extended_executable;
  long_request.arguments = {L"--child"};
  const auto long_child =
      prisminfer::run_native_worker(long_catalog, long_request);
  if (!long_child.ok) {
    std::cerr << "long path worker failure: " << long_child.failure_reason
              << "\n";
  }
  std::filesystem::remove(long_child.output_path, remove_error);
  std::filesystem::remove_all(extended_root, long_path_error);
  if (expect(long_child.ok, "extended-length executable path launches")) {
    return 1;
  }

  request.arguments = {L"--spawn-descendant"};
  const auto descendant = prisminfer::run_native_worker(catalog, request);
  if (expect(descendant.ok, "nested process cannot escape Job policy")) return 1;
  const auto& descendant_text = descendant.captured_output;
  std::filesystem::remove(descendant.output_path, remove_error);
  if (expect(descendant_text.find("descendant-contained=1") !=
                 std::string::npos,
             "child proves descendant containment")) return 1;

  request.arguments = {L"--spawn-descendant-clean"};
  request.max_active_processes = 2U;
  const auto complete_tree =
      prisminfer::run_native_worker(catalog, request);
  std::filesystem::remove(complete_tree.output_path, remove_error);
  if (expect(complete_tree.ok && complete_tree.job_total_processes == 2U &&
                 complete_tree.job_peak_active_processes == 2U &&
                 complete_tree.tree_peak_working_set_bytes >=
                     complete_tree.root_peak_working_set_bytes,
             "completed descendant is retained in whole-Job evidence")) {
    return 1;
  }

  request.arguments = {L"--spawn-descendant-tree"};
  request.timeout_ms = 1000U;
  request.max_active_processes = 2U;
  const auto tree_timeout = prisminfer::run_native_worker(catalog, request);
  if (expect(!tree_timeout.ok && tree_timeout.timed_out,
             "timeout terminates a live two-process Job tree")) return 1;
  const auto& tree_text = tree_timeout.captured_output;
  std::filesystem::remove(tree_timeout.output_path, remove_error);
  const auto pid_offset = tree_text.find("descendant-pid=");
  if (expect(pid_offset != std::string::npos,
             "contained child reports live descendant PID")) return 1;
  const auto pid = static_cast<DWORD>(std::stoul(
      tree_text.substr(pid_offset + std::string("descendant-pid=").size())));
  HANDLE descendant_process = OpenProcess(SYNCHRONIZE, FALSE, pid);
  const bool descendant_stopped = descendant_process == nullptr ||
      WaitForSingleObject(descendant_process, 0U) == WAIT_OBJECT_0;
  if (descendant_process != nullptr) CloseHandle(descendant_process);
  if (expect(descendant_stopped,
             "Job termination leaves no live descendant")) return 1;

  request.arguments = {L"--spawn-descendant-and-exit"};
  request.timeout_ms = 200U;
  const auto early_root_exit =
      prisminfer::run_native_worker(catalog, request);
  if (expect(!early_root_exit.ok && early_root_exit.timed_out,
             "root exit does not publish while a descendant remains live")) {
    return 1;
  }
  const auto& early_exit_text = early_root_exit.captured_output;
  std::filesystem::remove(early_root_exit.output_path, remove_error);
  const auto orphan_offset = early_exit_text.find("orphan-pid=");
  if (expect(orphan_offset != std::string::npos,
             "early-exit root reports descendant PID")) return 1;
  const auto orphan_pid = static_cast<DWORD>(std::stoul(
      early_exit_text.substr(orphan_offset + std::string("orphan-pid=").size())));
  HANDLE orphan_process = OpenProcess(SYNCHRONIZE, FALSE, orphan_pid);
  const bool orphan_stopped = orphan_process == nullptr ||
      WaitForSingleObject(orphan_process, 0U) == WAIT_OBJECT_0;
  if (orphan_process != nullptr) CloseHandle(orphan_process);
  if (expect(orphan_stopped,
             "early-exit root leaves no live descendant after timeout")) {
    return 1;
  }
  request.max_active_processes = 1U;
  request.timeout_ms = 2000U;

  request.arguments = {L"--hang"};
  request.timeout_ms = 50U;
  request.simulate_termination_api_failure = true;
  const auto cleanup_fault = prisminfer::run_native_worker(catalog, request);
  if (expect(!cleanup_fault.ok && cleanup_fault.failure_reason ==
                 "native_worker_timeout_cleanup_failed",
             "cleanup API fault fails closed after kill-on-close")) return 1;
  request.simulate_termination_api_failure = false;
  request.timeout_ms = 2000U;

  auto alternate_stream = request;
  alternate_stream.executable_path =
      std::filesystem::path(request.executable_path.wstring() + L":payload");
  const auto ads = prisminfer::run_native_worker(catalog, alternate_stream);
  if (expect(!ads.ok && ads.failure_reason ==
                 "native_worker_executable_path_syntax_rejected",
             "alternate data stream executable path is rejected")) return 1;
  auto device_path = request;
  device_path.executable_path = L"\\\\.\\C:\\untrusted.exe";
  const auto device = prisminfer::run_native_worker(catalog, device_path);
  if (expect(!device.ok && device.failure_reason ==
                 "native_worker_executable_path_syntax_rejected",
             "device executable path is rejected")) return 1;
  auto globalroot_path = request;
  globalroot_path.executable_path =
      L"\\\\?\\GLOBALROOT\\Device\\HarddiskVolumeShadowCopy1\\worker.exe";
  const auto globalroot =
      prisminfer::run_native_worker(catalog, globalroot_path);
  if (expect(!globalroot.ok && globalroot.failure_reason ==
                 "native_worker_executable_path_syntax_rejected",
             "GLOBALROOT executable path is rejected")) return 1;

  const auto link_path = std::filesystem::temp_directory_path() /
                         L"prisminfer-native-worker-reparse.exe";
  std::filesystem::remove(link_path, remove_error);
  const DWORD symlink_flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
  const BOOL linked = CreateSymbolicLinkW(
      link_path.c_str(), request.executable_path.c_str(), symlink_flags);
  if (expect(linked != FALSE, "test executable reparse point is created")) {
    return 1;
  }
  auto reparse_request = request;
  reparse_request.executable_path = link_path;
  const auto reparse =
      prisminfer::run_native_worker(catalog, reparse_request);
  std::filesystem::remove(link_path, remove_error);
  if (expect(!reparse.ok && reparse.failure_reason ==
                 "native_worker_executable_reparse_rejected",
             "executable reparse point is rejected before canonicalization")) {
    return 1;
  }

  const auto ancestor_root = std::filesystem::temp_directory_path() /
                             L"prisminfer-native-worker-ancestor-root";
  const auto ancestor_outside = std::filesystem::temp_directory_path() /
                                L"prisminfer-native-worker-ancestor-outside";
  const auto ancestor_link = ancestor_root / L"linked-directory";
  const auto outside_executable = ancestor_outside / L"worker.exe";
  std::filesystem::remove_all(ancestor_root, remove_error);
  std::filesystem::remove_all(ancestor_outside, remove_error);
  std::filesystem::create_directories(ancestor_root, remove_error);
  std::filesystem::create_directories(ancestor_outside, remove_error);
  std::filesystem::copy_file(request.executable_path, outside_executable,
                             std::filesystem::copy_options::overwrite_existing,
                             remove_error);
  const BOOL ancestor_linked = CreateSymbolicLinkW(
      ancestor_link.c_str(), ancestor_outside.c_str(),
      SYMBOLIC_LINK_FLAG_DIRECTORY |
          SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
  if (expect(ancestor_linked != FALSE,
             "ancestor directory reparse point is created")) return 1;
  const auto linked_executable = ancestor_link / L"worker.exe";
  const auto linked_hash = prisminfer::sha256_regular_file(linked_executable);
  prisminfer::NativeWorkerTrustCatalog ancestor_catalog({{
      linked_executable, ancestor_root, linked_hash,
      "ancestor-reparse-approval"}});
  auto ancestor_request = request;
  ancestor_request.executable_path = linked_executable;
  ancestor_request.arguments = {L"--child"};
  const auto ancestor_reparse =
      prisminfer::run_native_worker(ancestor_catalog, ancestor_request);
  std::filesystem::remove_all(ancestor_root, remove_error);
  std::filesystem::remove_all(ancestor_outside, remove_error);
  if (expect(!ancestor_reparse.ok && ancestor_reparse.failure_reason ==
                 "native_worker_executable_identity_rejected",
             "opened-handle final path rejects ancestor reparse escape")) {
    return 1;
  }

  const auto swap_root = std::filesystem::temp_directory_path() /
                         L"prisminfer-native-worker-swap";
  const auto swap_executable = swap_root / L"worker.exe";
  const auto swap_replacement = swap_root / L"replacement.exe";
  std::filesystem::remove_all(swap_root, remove_error);
  std::filesystem::create_directories(swap_root, remove_error);
  std::filesystem::copy_file(request.executable_path, swap_executable,
                             std::filesystem::copy_options::overwrite_existing,
                             remove_error);
  std::filesystem::copy_file(request.executable_path, swap_replacement,
                             std::filesystem::copy_options::overwrite_existing,
                             remove_error);
  const auto swap_hash = prisminfer::sha256_regular_file(swap_executable);
  prisminfer::NativeWorkerTrustCatalog swap_catalog({{
      swap_executable, swap_root, swap_hash, "swap-test-approval"}});
  auto swap_request = request;
  swap_request.executable_path = swap_executable;
  swap_request.arguments = {L"--hang"};
  swap_request.timeout_ms = 500U;
  auto swap_run = std::async(std::launch::async, [&]() {
    return prisminfer::run_native_worker(swap_catalog, swap_request);
  });
  Sleep(100U);
  const BOOL replaced = MoveFileExW(
      swap_replacement.c_str(), swap_executable.c_str(),
      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
  const auto swap_result = swap_run.get();
  std::filesystem::remove(swap_result.output_path, remove_error);
  std::filesystem::remove_all(swap_root, remove_error);
  if (expect(replaced == FALSE,
             "opened executable identity blocks replacement through launch")) {
    return 1;
  }
  if (expect(!swap_result.ok && swap_result.timed_out,
             "TOCTOU fixture remains governed until timeout")) return 1;

#endif
  return 0;
}

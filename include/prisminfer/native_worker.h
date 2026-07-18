#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace prisminfer {

struct NativeWorkerExecutableApproval {
  std::filesystem::path executable_path;
  std::filesystem::path trusted_executable_root;
  std::string expected_executable_sha256;
  std::string approval_identity;
};

// Supervisor-owned, immutable executable approvals. Worker requests never
// carry roots or hashes, so a plan can select only an already-approved image.
class NativeWorkerTrustCatalog {
 public:
  explicit NativeWorkerTrustCatalog(
      std::vector<NativeWorkerExecutableApproval> approvals);

  const NativeWorkerExecutableApproval* find(
      const std::filesystem::path& executable_path) const;

 private:
  const std::vector<NativeWorkerExecutableApproval> approvals_;
};

struct NativeWorkerRequest {
  std::filesystem::path executable_path;
  std::vector<std::wstring> arguments;
  std::uint32_t timeout_ms{0};
  std::uint64_t max_output_bytes{1U * 1024U * 1024U};
  std::uint32_t max_active_processes{1U};
  // Aggregate Job commit limit. Callers must replace this conservative
  // CPU-baseline default with the exact admitted worker-tree bound.
  std::uint64_t max_job_memory_bytes{8ULL * 1024ULL * 1024ULL * 1024ULL};
  // Deterministic CPU-only fault injection. It can only force fail-closed
  // cleanup behavior and never broadens launch authority.
  bool simulate_termination_api_failure{false};
};

struct NativeWorkerResult {
  bool ok{false};
  bool evidence_available{false};
  std::string failure_reason;
  std::filesystem::path output_path;
  // Bounded supervisor-captured bytes used for policy parsing. Consumers must
  // not reopen output_path for any decision because pathname identity can
  // change after the retained publication handle closes.
  std::string captured_output;
  std::uint32_t exit_code{0};
  bool timed_out{false};
  std::uint32_t root_process_id{0};
  std::string job_identity;
  std::uint32_t job_total_processes{0};
  std::uint32_t job_peak_active_processes{0};
  std::uint32_t job_total_terminated_processes{0};
  std::uint64_t root_peak_working_set_bytes{0};
  std::uint64_t root_peak_private_commit_bytes{0};
  std::uint64_t tree_peak_working_set_bytes{0};
  std::uint64_t tree_peak_private_commit_bytes{0};
  std::uint32_t tree_sample_interval_milliseconds{0};
  std::uint64_t read_bytes{0};
  std::uint64_t write_bytes{0};
  std::uint64_t output_bytes{0};
  std::uint64_t output_limit_bytes{0};
  std::uint64_t job_memory_limit_bytes{0};
  std::uint32_t job_cpu_time_limit_milliseconds{0};
  std::string executable_sha256;
  std::string approval_identity;
};

// Quotes one argv element using the CommandLineToArgvW-compatible rules used
// by CreateProcessW children built with the Microsoft C runtime.
std::wstring quote_windows_argument(const std::wstring& argument);

// Returns the lowercase SHA-256 of a regular, non-reparse file, or an empty
// string when the identity cannot be established.
std::string sha256_regular_file(const std::filesystem::path& path);

// Launches only an executable approved by the supervisor-owned catalog. It
// keeps an opened, share-restricted executable handle through creation,
// creates the child suspended, assigns it to a non-breakaway kill-on-close Job,
// then resumes it. The child receives a minimal explicit environment and a
// child-bound System32-preferred image-load policy, and inherits only
// pre-created NUL input and an opaque output handle. This CPU-only helper never
// opens a model or initializes CUDA.
NativeWorkerResult run_native_worker(const NativeWorkerTrustCatalog& catalog,
                                     const NativeWorkerRequest& request);

}  // namespace prisminfer

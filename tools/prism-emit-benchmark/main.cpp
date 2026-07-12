#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "prisminfer/errors.h"
#include "prisminfer/kernel_benchmark_manifest.h"
#include "prisminfer/sha256.h"

int main(int argc, char** argv) {
  constexpr std::uint64_t kMaximumEvidenceBytes = 64ULL * 1024ULL * 1024ULL;
  constexpr std::uint64_t kMaximumManifestBytes = 64ULL * 1024ULL;
  if (argc != 7) {
    std::cerr << "usage: prism-emit-benchmark INPUT_MANIFEST OUTPUT_MANIFEST "
                 "--evidence-root ROOT --raw-trials RAW_FILE|--failure-record FAILURE_FILE\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  auto input = prisminfer::read_kernel_benchmark_manifest(argv[1]);
  if (!input.ok) {
    std::cerr << "input_manifest_rejected:" << input.error << "\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  const bool completed = input.manifest.run_outcome == "completed";
  const std::string required_flag =
      completed ? "--raw-trials" : "--failure-record";
  const std::string evidence_hash = completed
      ? input.manifest.raw_trial_sha256
      : input.manifest.failure_record_sha256;
  if (std::string(argv[3]) != "--evidence-root" ||
      std::string(argv[5]) != required_flag || evidence_hash.empty()) {
    std::cerr << "evidence_argument_rejected\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  std::string error;
  std::string actual_evidence_hash;
  if (!prisminfer::sha256_trusted_regular_file_bounded(
          argv[4], argv[6], kMaximumEvidenceBytes, &actual_evidence_hash, &error)) {
    std::cerr << "evidence_hash_failed:" << error << "\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  if (actual_evidence_hash != evidence_hash) {
    std::cerr << "evidence_hash_mismatch\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  if (!prisminfer::write_kernel_benchmark_manifest(argv[2], input.manifest,
                                                    &error)) {
    std::cerr << "output_manifest_rejected:" << error << "\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  std::string digest;
  if (!prisminfer::sha256_regular_file_bounded(argv[2], kMaximumManifestBytes,
                                                &digest, &error)) {
    std::filesystem::remove(argv[2]);
    std::cerr << "output_manifest_hash_failed:" << error << "\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  const std::filesystem::path sidecar = std::string(argv[2]) + ".sha256";
  {
    std::ofstream output(sidecar, std::ios::out | std::ios::trunc);
    output << digest << "  " << std::filesystem::path(argv[2]).filename().string()
           << "\n";
    if (!output) {
      std::filesystem::remove(sidecar);
      std::filesystem::remove(argv[2]);
      std::cerr << "output_manifest_hash_write_failed\n";
      return static_cast<int>(prisminfer::ExitCode::FailedClosed);
    }
  }
  std::cout << "{\"status\":\"" << input.manifest.run_outcome
            << "\",\"claim_status\":\""
            << input.manifest.claim_status << "\",\"manifest_sha256\":\""
            << digest << "\"}\n";
  return input.manifest.run_outcome == "completed"
             ? static_cast<int>(prisminfer::ExitCode::Ok)
             : static_cast<int>(prisminfer::ExitCode::FailedClosed);
}

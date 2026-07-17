#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

#include "prisminfer/atomic_file.h"
#include "prisminfer/errors.h"
#include "prisminfer/kernel_benchmark_manifest.h"
#include "prisminfer/sha256.h"

int main(int argc, char** argv) {
  constexpr std::uint64_t kMaximumEvidenceBytes = 64ULL * 1024ULL * 1024ULL;
  constexpr std::uint64_t kMaximumManifestBytes = 64ULL * 1024ULL;
  if (argc != 9) {
    std::cerr << "usage: prism-emit-benchmark INPUT_MANIFEST OUTPUT_MANIFEST "
                 "--output-root ROOT --evidence-root ROOT "
                 "--raw-trials RAW_FILE|--failure-record FAILURE_FILE\n";
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
  if (std::string(argv[3]) != "--output-root" ||
      std::string(argv[5]) != "--evidence-root" ||
      std::string(argv[7]) != required_flag || evidence_hash.empty()) {
    std::cerr << "evidence_argument_rejected\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  std::string error;
  if (!prisminfer::is_direct_child_of_trusted_directory(argv[4], argv[2],
                                                         &error)) {
    std::cerr << "output_root_rejected:" << error << "\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  std::string actual_evidence_hash;
  if (!prisminfer::sha256_trusted_regular_file_bounded(
          argv[6], argv[8], kMaximumEvidenceBytes, &actual_evidence_hash, &error)) {
    std::cerr << "evidence_hash_failed:" << error << "\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  if (actual_evidence_hash != evidence_hash) {
    std::cerr << "evidence_hash_mismatch\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  const std::string canonical_manifest =
      prisminfer::canonical_kernel_benchmark_manifest_json(input.manifest);
  std::string digest;
  if (canonical_manifest.size() > kMaximumManifestBytes ||
      !prisminfer::sha256_text(canonical_manifest, &digest)) {
    std::cerr << "output_manifest_hash_failed\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  const std::filesystem::path sidecar = std::string(argv[2]) + ".sha256";
  const std::string sidecar_contents =
      digest + "  " + std::filesystem::path(argv[2]).filename().string() + "\n";
  if (!prisminfer::write_new_or_same_text_file_atomically(
          sidecar, sidecar_contents, kMaximumManifestBytes, {}, &error)) {
    std::cerr << "output_manifest_hash_write_failed:" << error << "\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  if (!prisminfer::write_kernel_benchmark_manifest(argv[2], input.manifest,
                                                    &error)) {
    std::cerr << "output_manifest_rejected:" << error << "\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  std::cout << "{\"status\":\"" << input.manifest.run_outcome
            << "\",\"claim_status\":\""
            << input.manifest.claim_status << "\",\"manifest_sha256\":\""
            << digest << "\"}\n";
  return input.manifest.run_outcome == "completed"
             ? static_cast<int>(prisminfer::ExitCode::Ok)
             : static_cast<int>(prisminfer::ExitCode::FailedClosed);
}

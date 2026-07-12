#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "prisminfer/errors.h"
#include "prisminfer/kernel_benchmark_manifest.h"
#include "prisminfer/sha256.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: prism-emit-benchmark INPUT_MANIFEST OUTPUT_MANIFEST\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  const auto input = prisminfer::read_kernel_benchmark_manifest(argv[1]);
  if (!input.ok) {
    std::cerr << "input_manifest_rejected:" << input.error << "\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  std::string error;
  if (!prisminfer::write_kernel_benchmark_manifest(argv[2], input.manifest,
                                                    &error)) {
    std::cerr << "output_manifest_rejected:" << error << "\n";
    return static_cast<int>(prisminfer::ExitCode::FailedClosed);
  }
  std::string digest;
  if (!prisminfer::sha256_file(argv[2], &digest, &error)) {
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

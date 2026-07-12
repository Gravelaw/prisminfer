#include <iostream>
#include <string>

#include "prisminfer/errors.h"
#include "prisminfer/kernel_benchmark_manifest.h"

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
  std::cout << "{\"status\":\"completed\",\"claim_status\":\""
            << input.manifest.claim_status << "\"}\n";
  return static_cast<int>(prisminfer::ExitCode::Ok);
}

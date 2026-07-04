#include <iostream>

#include "prisminfer/transfer_metrics.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

}  // namespace

int main() {
  prisminfer::TransferSample sample;
  sample.h2d_bytes = 1024;
  const auto ok = prisminfer::validate_transfer_sample("gpu", sample, true,
                                                       false);
  if (expect(ok.ok, "gpu transfer sample accepted")) return 1;

  const auto missing =
      prisminfer::validate_transfer_sample("gpu", {}, true, false);
  if (expect(!missing.ok, "missing transfer bytes rejected")) return 1;
  if (expect(missing.failure_reason == "transfer_bytes_required",
             "missing transfer reason")) return 1;

  sample.nvme_read_bytes = 4096;
  const auto warm_nvme = prisminfer::validate_transfer_sample(
      "nvme-simulated", sample, true, false);
  if (expect(!warm_nvme.ok, "warm-only nvme rejected")) return 1;
  if (expect(warm_nvme.failure_reason == "cold_cache_required_for_nvme",
             "nvme cold cache reason")) return 1;
  return 0;
}


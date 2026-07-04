#include "prisminfer/transfer_metrics.h"

namespace prisminfer {

TransferValidationResult validate_transfer_sample(
    const std::string& offload_policy,
    const TransferSample& sample,
    bool transfer_metrics_enabled,
    bool cold_cache_run) {
  if (offload_policy == "none") {
    return {true, ""};
  }
  if (!transfer_metrics_enabled) {
    return {false, "transfer_metrics_required"};
  }
  if (sample.h2d_bytes == 0 && sample.d2h_bytes == 0 &&
      sample.nvme_read_bytes == 0 && sample.nvme_write_bytes == 0) {
    return {false, "transfer_bytes_required"};
  }
  if ((offload_policy == "nvme-simulated" ||
       offload_policy == "nvme-experimental") &&
      sample.nvme_read_bytes == 0 && sample.nvme_write_bytes == 0) {
    return {false, "nvme_bytes_required"};
  }
  if ((offload_policy == "nvme-simulated" ||
       offload_policy == "nvme-experimental") &&
      !cold_cache_run) {
    return {false, "cold_cache_required_for_nvme"};
  }
  return {true, ""};
}

}  // namespace prisminfer


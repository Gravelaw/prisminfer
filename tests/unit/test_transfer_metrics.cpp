#include <iostream>
#include <limits>

#include "prisminfer/transfer_metrics.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

prisminfer::TransferEvent event(std::string id, std::string direction,
                               std::uint64_t submitted,
                               std::uint64_t completed) {
  return {std::move(id), "plan-1", std::move(direction), "host-allocation",
          "gpu-allocation", "pinned", "stream-1", "copy-engine-1",
          "tensor-1", submitted, completed, 1, 2, 3, 1, 0, false, false,
          completed < submitted};
}

}  // namespace

int main() {
  prisminfer::TransferSample sample;
  sample.declared_h2d_bytes = 1024;
  sample.observed_pinned_host_peak_bytes = 999;
  sample.observed_staging_peak_bytes = 999;
  const auto missing = prisminfer::validate_transfer_sample(
      "gpu", sample, true, false);
  if (expect(!missing.ok &&
                 missing.failure_reason == "observed_transfer_events_required",
             "declared bytes cannot populate observed transfer evidence")) {
    return 1;
  }

  sample.events.push_back(event("transfer-1", "h2d", 1024, 1024));
  if (expect(prisminfer::reconcile_transfer_sample(&sample).ok,
             "valid transfer events reconcile")) return 1;
  const auto ok = prisminfer::validate_transfer_sample(
      "gpu", sample, true, false);
  if (expect(ok.ok && sample.observed_h2d_completed_bytes == 1024,
             "actual completed bytes are event-derived")) return 1;
  if (expect(sample.observed_pinned_host_peak_bytes == 0U &&
                 sample.observed_staging_peak_bytes == 0U,
             "uninstrumented observed peaks are cleared during reconciliation")) {
    return 1;
  }

  auto duplicate = sample;
  duplicate.events.push_back(event("transfer-1", "h2d", 1, 1));
  const auto duplicate_result =
      prisminfer::reconcile_transfer_sample(&duplicate);
  if (expect(!duplicate_result.ok && duplicate_result.failure_reason ==
                 "transfer_event_id_duplicate" &&
                 duplicate.observed_h2d_completed_bytes == 0U,
             "duplicate event identity cannot inflate observed totals")) {
    return 1;
  }

  sample.dropped_records = 1;
  const auto dropped = prisminfer::validate_transfer_sample(
      "gpu", sample, true, false);
  if (expect(!dropped.ok && dropped.failure_reason ==
                 "transfer_instrumentation_dropped_records",
             "dropped instrumentation rejects measured transfer evidence")) {
    return 1;
  }

  prisminfer::TransferSample partial;
  partial.events.push_back(event("transfer-2", "d2h", 2048, 1024));
  if (expect(prisminfer::reconcile_transfer_sample(&partial).ok &&
                 partial.observed_d2h_submitted_bytes == 2048 &&
                 partial.observed_d2h_completed_bytes == 1024,
             "partial transfer keeps submitted and completed bytes separate")) {
    return 1;
  }

  prisminfer::TransferSample invalid;
  auto invalid_event = event("transfer-3", "h2d", 1, 1);
  invalid_event.finish_monotonic_nanoseconds = 1;
  invalid.events.push_back(invalid_event);
  if (expect(!prisminfer::reconcile_transfer_sample(&invalid).ok,
             "out-of-order event timestamps fail closed")) return 1;

  prisminfer::TransferSample overflow;
  overflow.events.push_back(event("transfer-4", "h2d",
                                  std::numeric_limits<std::uint64_t>::max(),
                                  std::numeric_limits<std::uint64_t>::max()));
  overflow.events.push_back(event("transfer-5", "h2d", 1, 1));
  const auto overflow_result =
      prisminfer::reconcile_transfer_sample(&overflow);
  if (expect(!overflow_result.ok && overflow_result.failure_reason ==
                 "transfer_submitted_bytes_overflow" &&
                 overflow.observed_h2d_submitted_bytes == 0,
             "overflow fails without publishing a partial observed total")) {
    return 1;
  }

  prisminfer::TransferSample nvme;
  nvme.declared_nvme_read_bytes = 4096;
  nvme.events.push_back(event("transfer-6", "h2d", 1, 1));
  if (expect(prisminfer::reconcile_transfer_sample(&nvme).ok,
             "nvme fixture transfer reconciles")) return 1;
  const auto warm_nvme = prisminfer::validate_transfer_sample(
      "nvme-experimental", nvme, true, false);
  if (expect(!warm_nvme.ok &&
                 warm_nvme.failure_reason == "cold_cache_required_for_nvme",
             "warm-only nvme evidence is rejected")) return 1;
  return 0;
}

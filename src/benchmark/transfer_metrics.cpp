#include "prisminfer/transfer_metrics.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace prisminfer {
namespace {

bool bounded_id(const std::string& value) {
  return !value.empty() && value.size() <= 128U &&
         std::all_of(value.begin(), value.end(), [](unsigned char character) {
           return (character >= 'a' && character <= 'z') ||
                  (character >= 'A' && character <= 'Z') ||
                  (character >= '0' && character <= '9') ||
                  character == '-' || character == '_' || character == '.' ||
                  character == ':' || character == '/';
         });
}

bool checked_add(std::uint64_t left, std::uint64_t right,
                 std::uint64_t* output) {
  if (output == nullptr ||
      left > std::numeric_limits<std::uint64_t>::max() - right) {
    return false;
  }
  *output = left + right;
  return true;
}

bool valid_event(const TransferEvent& event) {
  const bool direction = event.direction == "h2d" ||
                         event.direction == "d2h";
  const bool host_kind = event.host_memory_kind == "pageable" ||
                         event.host_memory_kind == "pinned";
  const auto duration = event.finish_monotonic_nanoseconds -
                        event.start_monotonic_nanoseconds;
  return bounded_id(event.transfer_event_id) &&
         bounded_id(event.plan_entry_id) && direction &&
         bounded_id(event.source_allocation_id) &&
         bounded_id(event.destination_allocation_id) && host_kind &&
         bounded_id(event.stream_id) && bounded_id(event.copy_engine) &&
         bounded_id(event.logical_object_id) && event.submitted_bytes > 0U &&
         event.completed_bytes <= event.submitted_bytes &&
         event.submit_monotonic_nanoseconds > 0U &&
         event.submit_monotonic_nanoseconds <=
             event.start_monotonic_nanoseconds &&
         event.start_monotonic_nanoseconds <=
             event.finish_monotonic_nanoseconds &&
         event.exposed_wait_nanoseconds <= duration &&
         event.overlap_nanoseconds <= duration &&
         event.partial == (event.completed_bytes < event.submitted_bytes);
}

}  // namespace

TransferValidationResult reconcile_transfer_sample(TransferSample* sample) {
  if (sample == nullptr) return {false, "transfer_sample_required"};
  sample->observed_h2d_submitted_bytes = 0U;
  sample->observed_h2d_completed_bytes = 0U;
  sample->observed_d2h_submitted_bytes = 0U;
  sample->observed_d2h_completed_bytes = 0U;
  sample->observed_pinned_host_peak_bytes = 0U;
  sample->observed_staging_peak_bytes = 0U;
  std::uint64_t h2d_submitted = 0U;
  std::uint64_t h2d_completed = 0U;
  std::uint64_t d2h_submitted = 0U;
  std::uint64_t d2h_completed = 0U;
  std::unordered_set<std::string> event_ids;
  if (sample->instrumentation_mode != "ordinary" &&
      sample->instrumentation_mode != "profiler") {
    return {false, "transfer_instrumentation_mode_invalid"};
  }
  for (const auto& event : sample->events) {
    if (!valid_event(event)) return {false, "transfer_event_invalid"};
    if (!event_ids.insert(event.transfer_event_id).second) {
      return {false, "transfer_event_id_duplicate"};
    }
    auto* submitted =
        event.direction == "h2d" ? &h2d_submitted : &d2h_submitted;
    auto* completed =
        event.direction == "h2d" ? &h2d_completed : &d2h_completed;
    std::uint64_t next = 0U;
    if (!checked_add(*submitted, event.submitted_bytes, &next)) {
      return {false, "transfer_submitted_bytes_overflow"};
    }
    *submitted = next;
    if (!checked_add(*completed, event.completed_bytes, &next)) {
      return {false, "transfer_completed_bytes_overflow"};
    }
    *completed = next;
  }
  sample->observed_h2d_submitted_bytes = h2d_submitted;
  sample->observed_h2d_completed_bytes = h2d_completed;
  sample->observed_d2h_submitted_bytes = d2h_submitted;
  sample->observed_d2h_completed_bytes = d2h_completed;
  return {true, ""};
}

TransferValidationResult validate_transfer_sample(
    const std::string& offload_policy,
    const TransferSample& sample,
    bool transfer_metrics_enabled,
    bool cold_cache_run) {
  if (offload_policy == "none") return {true, ""};
  if (!transfer_metrics_enabled) {
    return {false, "transfer_metrics_required"};
  }
  if (sample.dropped_records != 0U) {
    return {false, "transfer_instrumentation_dropped_records"};
  }
  if (sample.events.empty()) {
    return {false, "observed_transfer_events_required"};
  }
  if (sample.observed_h2d_completed_bytes == 0U &&
      sample.observed_d2h_completed_bytes == 0U) {
    return {false, "observed_transfer_bytes_required"};
  }
  if ((offload_policy == "nvme-simulated" ||
       offload_policy == "nvme-experimental") &&
      !cold_cache_run) {
    return {false, "cold_cache_required_for_nvme"};
  }
  if ((offload_policy == "nvme-simulated" ||
       offload_policy == "nvme-experimental") &&
      (sample.declared_nvme_read_bytes == 0U &&
       sample.declared_nvme_write_bytes == 0U)) {
    return {false, "declared_nvme_plan_required"};
  }
  return {true, ""};
}

}  // namespace prisminfer

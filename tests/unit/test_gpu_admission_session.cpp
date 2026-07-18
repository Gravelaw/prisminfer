#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <type_traits>

#include "prisminfer/gpu_admission_session.h"
#include "prisminfer/telemetry.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

static_assert(!std::is_default_constructible_v<
              prisminfer::NativeWorkerSupervisorAccess>);

constexpr std::uint64_t kGiB = 1ULL << 30;

#if defined(_WIN32)
std::set<std::filesystem::path> temporary_output_artifacts() {
  std::set<std::filesystem::path> artifacts;
  std::error_code error;
  const auto directory = std::filesystem::temp_directory_path(error);
  if (error) return artifacts;
  for (std::filesystem::directory_iterator current(directory, error), end;
       !error && current != end; current.increment(error)) {
    const auto filename = current->path().filename().wstring();
    if (filename.rfind(L"prisminfer-", 0U) == 0U &&
        current->path().extension() == L".log") {
      artifacts.insert(current->path());
    }
  }
  return artifacts;
}
#endif

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

prisminfer::AdmissionCellIdentity cell() {
  prisminfer::AdmissionCellIdentity identity;
  identity.run_sequence = 1U;
  identity.run_contract_hash.fill(1U);
  identity.threshold_registry_hash.fill(2U);
  identity.hardware_identity_hash.fill(3U);
  identity.runtime_identity_hash.fill(4U);
  identity.artifact_identity_hash.fill(5U);
  identity.service_profile_hash.fill(6U);
  return identity;
}

prisminfer::PredictedBytes prediction(
    std::uint64_t bytes, prisminfer::PredictionProvenance provenance) {
  return {bytes, provenance};
}

prisminfer::PreContextAdmissionRequest pre_request(std::uint64_t now) {
  prisminfer::PreContextAdmissionRequest request;
  request.requested_tier_bytes = 8U * kGiB;
  request.context_tokens = 8'192U;
  request.run_deadline_milliseconds = 60'000U;
  request.evaluation_monotonic_milliseconds = now;
  request.cell = cell();
  using prisminfer::PredictionProvenance;
  request.predicted_gpu.context_runtime =
      prediction(512ULL << 20, PredictionProvenance::PinnedRuntimeEstimate);
  request.predicted_gpu.backend =
      prediction(256ULL << 20, PredictionProvenance::PinnedRuntimeEstimate);
  request.predicted_gpu.model =
      prediction(5U * kGiB, PredictionProvenance::ApprovedArtifactCensus);
  request.predicted_gpu.state =
      prediction(256ULL << 20, PredictionProvenance::ApprovedArtifactCensus);
  request.predicted_gpu.workspace =
      prediction(256ULL << 20, PredictionProvenance::PinnedRuntimeEstimate);
  request.predicted_gpu.fragmentation =
      prediction(256ULL << 20, PredictionProvenance::PinnedRuntimeEstimate);
  request.gpu.available = true;
  request.gpu.adapter_identity_available = true;
  request.gpu.adapter_luid_high = 7;
  request.gpu.adapter_luid_low = 11U;
  request.gpu.captured_monotonic_milliseconds = now - 10U;
  request.gpu.physical_or_reportable_local_bytes = 16U * kGiB;
  request.gpu.dxgi_local_budget_bytes = 15U * kGiB;
  request.gpu.dxgi_local_current_usage_bytes = kGiB;
  request.thermal.available = true;
  request.thermal.captured_monotonic_milliseconds = now - 10U;
  request.thermal.current_celsius = 55U;
  request.thermal.reported_target_celsius = 90U;
  request.thermal.reported_slowdown_celsius = 92U;
  request.storage.available = true;
  request.storage.free_bytes = 100U * kGiB;
  request.storage.reserve_bytes = 10U * kGiB;
  request.storage.required_incremental_bytes = 20U * kGiB;
  request.host.available = true;
  request.host.system_commit_source = "get_performance_info";
  request.host.captured_monotonic_milliseconds = now - 10U;
  request.host.system_memory_total_bytes = 32U * kGiB;
  request.host.system_memory_available_bytes = 16U * kGiB;
  request.host.system_commit_total_bytes = 16U * kGiB;
  request.host.system_commit_limit_bytes = 64U * kGiB;
  request.host.system_commit_available_bytes = 48U * kGiB;
  request.host_policy = prisminfer::default_host_reserve_policy(
      prisminfer::HostAdmissionLane::DevelopmentNonPromotable);
  request.host_request.planned_incremental_resident_bytes = 4U * kGiB;
  request.host_request.planned_incremental_commit_bytes = 4U * kGiB;
  return request;
}

prisminfer::PostContextAdmissionRequest post_request(std::uint64_t now,
                                                     std::uint32_t pid) {
  const auto pre = pre_request(now);
  prisminfer::PostContextAdmissionRequest request;
  request.policy_ceiling_bytes = pre.policy_ceiling_bytes;
  request.requested_tier_bytes = pre.requested_tier_bytes;
  request.evaluation_monotonic_milliseconds = now;
  request.timing = pre.timing;
  request.cell = cell();
  request.gpu = pre.gpu;
  request.gpu.dxgi_local_budget_bytes = 14U * kGiB;
  request.gpu.dxgi_local_current_usage_bytes = 2U * kGiB;
  request.thermal = pre.thermal;
  request.owned_gpu.available = true;
  request.owned_gpu.captured_monotonic_milliseconds = now - 10U;
  request.owned_gpu.reconciled = true;
  request.owned_gpu.process_device_corroboration_available = true;
  request.owned_gpu.process_device_source = "wddm-process";
  request.owned_gpu.process_id = pid;
  request.owned_gpu.process_device_captured_monotonic_milliseconds = now - 10U;
  request.owned_gpu.adapter_identity_available = true;
  request.owned_gpu.adapter_luid_high = 7;
  request.owned_gpu.adapter_luid_low = 11U;
  request.owned_gpu.hard_cap_bytes = 8U * kGiB;
  request.owned_gpu.owned_current_bytes = 512ULL << 20;
  request.owned_gpu.process_device_current_bytes = 512ULL << 20;
  request.owned_gpu.owned_peak_bytes = 512ULL << 20;
  request.owned_gpu.cuda_context_runtime_current_bytes = 512ULL << 20;
  request.owned_gpu.cuda_context_runtime_at_owned_peak_bytes = 512ULL << 20;
  request.owned_gpu.cuda_mem_info_free_bytes = 12U * kGiB;
  request.owned_gpu.cuda_mem_info_total_bytes = 16U * kGiB;
  request.host = pre.host;
  request.host_policy = pre.host_policy;
  request.host_request = pre.host_request;
  return request;
}

prisminfer::SupervisorWatchdogSample watchdog_sample(std::uint64_t now,
                                                     std::uint32_t pid) {
  const auto post = post_request(now, pid);
  prisminfer::SupervisorWatchdogSample sample;
  sample.run_deadline_monotonic_milliseconds = now + 30'000U;
  sample.gpu = post.gpu;
  sample.owned_gpu = post.owned_gpu;
  sample.thermal = post.thermal;
  sample.host = post.host;
  sample.host_policy = post.host_policy;
  sample.host_request = post.host_request;
  return sample;
}

class AcceptingProtocolSupervisor final
    : public prisminfer::NativeWorkerProtocolSupervisor {
 public:
  bool bind_owned_worker(
      const prisminfer::NativeWorkerContainmentIdentity& identity) override {
    bound_pid = identity.root_process_id;
    return bound_pid != 0U && !identity.job_identity.empty();
  }
  prisminfer::NativeWorkerAdmissionGrant context_ready(
      std::uint64_t) override {
    return {true, 17U, 1U * kGiB, 1'000U, ""};
  }
  bool token_consumed(std::uint64_t token_id, std::uint64_t) override {
    ++consumption_count;
    return token_id == 17U && consumption_count == 1U;
  }
  bool heartbeat(std::uint64_t, std::uint64_t) override { return true; }
  void cooperative_cancel_acknowledged(std::uint64_t) override {
    cancel_acknowledged = true;
  }
  std::uint32_t bound_pid{0U};
  std::uint32_t consumption_count{0U};
  bool cancel_acknowledged{false};
};

}  // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
  if (argc == 2 && std::string(argv[1]).rfind("--protocol-", 0U) == 0U) {
    const std::string mode = argv[1];
    char nonce[64]{};
    if (GetEnvironmentVariableA("PRISMINFER_PROTOCOL_NONCE", nonce,
                                static_cast<DWORD>(sizeof(nonce))) != 32U) {
      return 2;
    }
    char handle_text[32]{};
    if (GetEnvironmentVariableA("PRISMINFER_PROTOCOL_OUT_HANDLE", handle_text,
                                static_cast<DWORD>(sizeof(handle_text))) == 0U) {
      return 6;
    }
    const auto raw_handle = std::strtoull(handle_text, nullptr, 10);
    const HANDLE protocol_output =
        reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(raw_handle));
    const auto write_protocol = [&](const std::string& message) {
      DWORD written = 0U;
      return WriteFile(protocol_output, message.data(),
                       static_cast<DWORD>(message.size()), &written, nullptr) &&
             written == message.size();
    };
    if (mode == "--protocol-no-context") {
      std::string version;
      std::string type;
      std::string returned_nonce;
      if (!(std::cin >> version >> type >> returned_nonce) ||
          type != "CANCEL" || returned_nonce != nonce) {
        return 4;
      }
      return write_protocol("PRISMINFER/1 CANCEL_ACK " +
                            std::string(nonce) + "\n") ? 0 : 7;
    }
    if (!write_protocol("PRISMINFER/1 CONTEXT_READY " + std::string(nonce) +
                        "\n")) {
      return 7;
    }
    std::string version;
    std::string type;
    std::string returned_nonce;
    std::uint64_t token_id = 0U;
    std::uint64_t cap = 0U;
    if (!(std::cin >> version >> type >> returned_nonce >> token_id >> cap) ||
        version != "PRISMINFER/1" || type != "ADMIT" ||
        returned_nonce != nonce || token_id == 0U || cap == 0U) {
      return 3;
    }
    if (!write_protocol("PRISMINFER/1 TOKEN_CONSUMED " + std::string(nonce) +
                        " " + std::to_string(token_id) + "\n")) {
      return 7;
    }
    if (mode == "--protocol-duplicate-token") {
      (void)write_protocol("PRISMINFER/1 TOKEN_CONSUMED " +
                           std::string(nonce) + " " +
                           std::to_string(token_id) + "\n");
      Sleep(1'000U);
      return 0;
    }
    if (!write_protocol("PRISMINFER/1 HEARTBEAT " + std::string(nonce) +
                        " 1\n")) {
      return 7;
    }
    if (mode == "--protocol-stale-heartbeat") {
      std::string cancel_version;
      std::string cancel_type;
      std::string cancel_nonce;
      if (!(std::cin >> cancel_version >> cancel_type >> cancel_nonce) ||
          cancel_type != "CANCEL" || cancel_nonce != nonce) {
        return 5;
      }
      return write_protocol("PRISMINFER/1 CANCEL_ACK " +
                            std::string(nonce) + "\n") ? 0 : 7;
    }
    Sleep(20U);
    return write_protocol("PRISMINFER/1 HEARTBEAT " + std::string(nonce) +
                          " 2\n") ? 0 : 7;
  }

  if (argc == 4 &&
      std::string(argv[1]) == "--uncooperative-evidence-supervisor") {
    char* high_end = nullptr;
    char* low_end = nullptr;
    const long parsed_high = std::strtol(argv[2], &high_end, 10);
    const unsigned long parsed_low = std::strtoul(argv[3], &low_end, 10);
    if (high_end == argv[2] || *high_end != '\0' || low_end == argv[3] ||
        *low_end != '\0' || parsed_high <= 0 || parsed_low == 0U) {
      return 89;
    }
    const auto luid_high = static_cast<std::int32_t>(parsed_high);
    const auto luid_low = static_cast<std::uint32_t>(parsed_low);
    const auto executable = std::filesystem::absolute(argv[0]);
    const auto hash = prisminfer::sha256_regular_file(executable);
    prisminfer::NativeWorkerTrustCatalog inner_catalog(
        {{executable, executable.parent_path(), hash,
          "uncooperative-evidence-worker"}});
    prisminfer::NativeWorkerRequest inner_request;
    inner_request.executable_path = executable;
    inner_request.arguments = {L"--protocol-child"};
    inner_request.timeout_ms = 5'000U;
    inner_request.max_job_memory_bytes = 1U * kGiB;
    auto inner = prisminfer::acquire_gpu_admission_session(
        cell(), luid_high, luid_low);
    auto inner_pre =
        pre_request(prisminfer::monotonic_time_milliseconds());
    inner_pre.gpu.adapter_luid_high = luid_high;
    inner_pre.gpu.adapter_luid_low = luid_low;
    if (!inner.session ||
        !inner.session->admit_pre_context(inner_pre).admitted) {
      return 87;
    }
    (void)inner.session->run_contained_worker(
        inner_catalog, inner_request, 100U,
        [luid_high, luid_low](std::stop_token, std::uint32_t pid,
                              std::uint64_t ready) {
          Sleep(2'000U);
          auto post = post_request(ready, pid);
          post.gpu.adapter_luid_high = luid_high;
          post.gpu.adapter_luid_low = luid_low;
          post.owned_gpu.adapter_luid_high = luid_high;
          post.owned_gpu.adapter_luid_low = luid_low;
          return post;
        },
        [](std::stop_token, std::uint32_t pid, std::uint64_t,
           std::uint64_t heartbeat) {
          return watchdog_sample(heartbeat, pid);
        });
    return 88;
  }

  const auto now = prisminfer::monotonic_time_milliseconds();
  auto acquired = prisminfer::acquire_gpu_admission_session(cell(), 7, 11U);
  if (expect(acquired.session.has_value(), "session acquires exclusive lease") ||
      expect(acquired.session->admit_pre_context(pre_request(now)).admitted,
             "Stage A admits before worker creation")) {
    return 1;
  }
  const auto executable = std::filesystem::absolute(argv[0]);
  const auto hash = prisminfer::sha256_regular_file(executable);
  prisminfer::NativeWorkerTrustCatalog catalog(
      {{executable, executable.parent_path(), hash, "session-protocol-test"}});
  prisminfer::NativeWorkerRequest request;
  request.executable_path = executable;
  request.arguments = {L"--protocol-child"};
  request.timeout_ms = 5'000U;
  request.max_job_memory_bytes = 1U * kGiB;
  const auto worker = acquired.session->run_contained_worker(
      catalog, request, 100U,
      [](std::stop_token, std::uint32_t pid, std::uint64_t ready) {
        return post_request(ready, pid);
      },
      [](std::stop_token, std::uint32_t pid, std::uint64_t,
         std::uint64_t heartbeat) {
        return watchdog_sample(heartbeat, pid);
      });
  if (expect(!worker.ok && worker.worker_exit_observed &&
                 worker.job_tree_empty &&
                 worker.failure_reason.rfind(
                     "native_worker_protocol_admission_rejected:", 0U) == 0U,
             "missing live process-device evidence aborts the contained Job") ||
      expect(acquired.session->state() ==
                 prisminfer::GpuAdmissionSessionState::FailedClosed,
             "production session cannot substitute simulated GPU evidence")) {
    std::cerr << "worker failure=" << worker.failure_reason << "\n";
    return 1;
  }
  {
    auto live = prisminfer::acquire_gpu_admission_session(cell(), 9, 13U);
    auto live_pre =
        pre_request(prisminfer::monotonic_time_milliseconds());
    live_pre.gpu.adapter_luid_high = 9;
    live_pre.gpu.adapter_luid_low = 13U;
    if (expect(live.session.has_value() &&
                   live.session->admit_pre_context(live_pre).admitted,
               "live-evidence test admits Stage A")) {
      return 1;
    }
    const auto live_sample_count =
        std::make_shared<std::atomic<std::uint32_t>>(0U);
    const auto live_worker = live.session->run_contained_worker(
        catalog, request, 100U,
        [](std::stop_token, std::uint32_t pid, std::uint64_t ready) {
          auto post = post_request(ready, pid);
          post.gpu.adapter_luid_high = 9;
          post.gpu.adapter_luid_low = 13U;
          post.owned_gpu.adapter_luid_high = 9;
          post.owned_gpu.adapter_luid_low = 13U;
          return post;
        },
        [](std::stop_token, std::uint32_t pid, std::uint64_t,
           std::uint64_t heartbeat) {
          auto sample = watchdog_sample(heartbeat, pid);
          sample.gpu.adapter_luid_high = 9;
          sample.gpu.adapter_luid_low = 13U;
          sample.owned_gpu.adapter_luid_high = 9;
          sample.owned_gpu.adapter_luid_low = 13U;
          return sample;
        },
        [live_sample_count](std::stop_token stop, std::uint32_t pid,
                            std::int32_t luid_high,
                            std::uint32_t luid_low) {
          prisminfer::ProcessDeviceMemorySample sample;
          if (stop.stop_requested()) return sample;
          sample.available = true;
          sample.source = "wddm-process";
          sample.process_id = pid;
          sample.captured_monotonic_milliseconds =
              prisminfer::monotonic_time_milliseconds();
          sample.adapter_luid_high = luid_high;
          sample.adapter_luid_low = luid_low;
          sample.current_bytes = 512ULL << 20;
          live_sample_count->fetch_add(1U, std::memory_order_relaxed);
          return sample;
        });
    if (expect(live_worker.ok && live_worker.worker_exit_observed &&
                   live_worker.job_tree_empty &&
                   live_sample_count->load(std::memory_order_relaxed) >= 2U,
               "fresh independent evidence admits Stage B and heartbeats")) {
      std::cerr << "live worker failure=" << live_worker.failure_reason
                << " samples="
                << live_sample_count->load(std::memory_order_relaxed) << "\n";
      return 1;
    }
    std::error_code remove_error;
    if (expect(!live_worker.output_path.empty() &&
                   std::filesystem::remove(live_worker.output_path,
                                           remove_error) &&
                   !remove_error,
               "successful test output is removed by its owner")) {
      return 1;
    }
  }
  {
    auto bounded = prisminfer::acquire_gpu_admission_session(cell(), 8, 12U);
    auto alternate_pre =
        pre_request(prisminfer::monotonic_time_milliseconds());
    alternate_pre.gpu.adapter_luid_high = 8;
    alternate_pre.gpu.adapter_luid_low = 12U;
    if (expect(bounded.session.has_value() &&
                   bounded.session->admit_pre_context(alternate_pre).admitted,
               "alternate adapter Stage A admits callback-timeout test")) {
      return 1;
    }
    const auto producer_active = std::make_shared<std::atomic<bool>>(false);
    auto producer_lifetime = std::make_shared<int>(42);
    const std::weak_ptr<int> producer_lifetime_observer = producer_lifetime;
    const auto started = prisminfer::monotonic_time_milliseconds();
    const auto timed_out = bounded.session->run_contained_worker(
        catalog, request, 100U,
        [producer_active, producer_lifetime](std::stop_token stop,
                                             std::uint32_t pid,
                                             std::uint64_t ready) {
          producer_active->store(true, std::memory_order_release);
          while (!stop.stop_requested()) Sleep(5U);
          producer_active->store(false, std::memory_order_release);
          auto post = post_request(ready, pid);
          post.gpu.adapter_luid_high = 8;
          post.gpu.adapter_luid_low = 12U;
          post.owned_gpu.adapter_luid_high = 8;
          post.owned_gpu.adapter_luid_low = 12U;
          (void)producer_lifetime;
          return post;
        },
        [](std::stop_token, std::uint32_t pid, std::uint64_t,
           std::uint64_t heartbeat) {
          return watchdog_sample(heartbeat, pid);
        });
    const auto elapsed =
        prisminfer::monotonic_time_milliseconds() - started;
    producer_lifetime.reset();
    const bool bounded_abort =
        !timed_out.ok && timed_out.worker_exit_observed &&
        timed_out.job_tree_empty &&
        !producer_active->load(std::memory_order_acquire) &&
        producer_lifetime_observer.expired() &&
        timed_out.failure_reason ==
            "native_worker_protocol_admission_rejected:"
            "post_context_evidence_timeout" &&
        elapsed < 1'500U;
    if (!bounded_abort) {
      std::cerr << "bounded abort details: ok=" << timed_out.ok
                << " worker_exit_observed="
                << timed_out.worker_exit_observed
                << " job_tree_empty=" << timed_out.job_tree_empty
                << " producer_active="
                << producer_active->load(std::memory_order_acquire)
                << " producer_lifetime_expired="
                << producer_lifetime_observer.expired()
                << " elapsed_ms=" << elapsed
                << " failure_reason=" << timed_out.failure_reason << "\n";
    }
    if (expect(bounded_abort,
               "blocked evidence producer cannot block bounded Job abort")) {
      return 1;
    }
  }
  {
    const auto temporary_outputs_before = temporary_output_artifacts();
    bool all_fail_stops_safely_contained = true;
    for (std::uint32_t attempt = 0U; attempt < 3U; ++attempt) {
      const auto luid_high = static_cast<std::int32_t>(10U + attempt);
      const auto luid_low = 14U + attempt;
      auto fail_stop_request = request;
      fail_stop_request.arguments = {
          L"--uncooperative-evidence-supervisor",
          std::to_wstring(luid_high), std::to_wstring(luid_low)};
      fail_stop_request.timeout_ms = 1'500U;
      fail_stop_request.max_active_processes = 4U;
      const auto started = prisminfer::monotonic_time_milliseconds();
      const auto fail_stopped =
          prisminfer::run_native_worker(catalog, fail_stop_request);
      const auto elapsed =
          prisminfer::monotonic_time_milliseconds() - started;
      const auto receipt = prisminfer::record_evidence_provider_fail_stop(
          fail_stopped, luid_high, luid_low, elapsed, 1'500U);
      const auto retry =
          prisminfer::acquire_exclusive_gpu_lease(luid_high, luid_low);
      const bool accepted_receipt_has_exact_evidence =
          !receipt.accepted ||
          (!fail_stopped.ok && !fail_stopped.timed_out &&
           fail_stopped.exit_code ==
               prisminfer::kEvidenceProviderFailStopExitCode &&
           fail_stopped.worker_exit_observed && fail_stopped.job_tree_empty &&
           fail_stopped.job_accounting_reconciled &&
           fail_stopped.artifact_handles_closed &&
           fail_stopped.temporary_files_reconciled &&
           fail_stopped.output_path.empty());
      const bool safely_contained =
          receipt.non_promotable && receipt.quarantined &&
          receipt.retry_prohibited && fail_stopped.output_path.empty() &&
          accepted_receipt_has_exact_evidence &&
          retry.status ==
              prisminfer::ExclusiveGpuLeaseStatus::AlreadyHeldInProcess;
      all_fail_stops_safely_contained =
          all_fail_stops_safely_contained && safely_contained;
      if (!safely_contained) {
        std::cerr << "fail-stop details: attempt=" << attempt
                  << " exit=" << fail_stopped.exit_code
                  << " elapsed_ms=" << elapsed
                  << " result=" << fail_stopped.failure_reason
                  << " receipt=" << receipt.reason
                  << " retry_status=" << static_cast<int>(retry.status)
                  << "\n";
      }
    }
    prisminfer::NativeWorkerResult incomplete_outer_evidence;
    const auto incomplete_receipt =
        prisminfer::record_evidence_provider_fail_stop(
            incomplete_outer_evidence, 20, 24U, 100U, 1'500U);
    const auto incomplete_retry =
        prisminfer::acquire_exclusive_gpu_lease(20, 24U);
    const bool incomplete_fail_stop_quarantined =
        !incomplete_receipt.accepted &&
        incomplete_receipt.non_promotable &&
        incomplete_receipt.quarantined &&
        incomplete_receipt.retry_prohibited &&
        incomplete_retry.status ==
            prisminfer::ExclusiveGpuLeaseStatus::AlreadyHeldInProcess;
    prisminfer::NativeWorkerResult exact_outer_evidence;
    exact_outer_evidence.exit_code =
        prisminfer::kEvidenceProviderFailStopExitCode;
    exact_outer_evidence.worker_exit_observed = true;
    exact_outer_evidence.job_tree_empty = true;
    exact_outer_evidence.job_accounting_reconciled = true;
    exact_outer_evidence.artifact_handles_closed = true;
    exact_outer_evidence.temporary_files_reconciled = true;
    const auto exact_receipt =
        prisminfer::record_evidence_provider_fail_stop(
            exact_outer_evidence, 21, 25U, 100U, 1'500U);
    const auto exact_retry =
        prisminfer::acquire_exclusive_gpu_lease(21, 25U);
    const bool exact_fail_stop_accepted =
        exact_receipt.accepted && exact_receipt.non_promotable &&
        exact_receipt.quarantined && exact_receipt.retry_prohibited &&
        exact_retry.status ==
            prisminfer::ExclusiveGpuLeaseStatus::AlreadyHeldInProcess;
    const auto temporary_outputs_after = temporary_output_artifacts();
    if (expect(all_fail_stops_safely_contained &&
                   incomplete_fail_stop_quarantined &&
                   exact_fail_stop_accepted &&
                   temporary_outputs_after == temporary_outputs_before,
               "uncooperative producer fail-stops under parent receipt and "
               "durable quarantine")) {
      std::cerr << "temporary outputs before="
                << temporary_outputs_before.size()
                << " after=" << temporary_outputs_after.size()
                << " incomplete_receipt=" << incomplete_receipt.reason
                << " incomplete_retry_status="
                << static_cast<int>(incomplete_retry.status)
                << " exact_receipt=" << exact_receipt.reason
                << " exact_retry_status="
                << static_cast<int>(exact_retry.status)
                << "\n";
      return 1;
    }
  }
#else
  (void)argc;
  (void)argv;
  auto acquired = prisminfer::acquire_gpu_admission_session(cell(), 7, 11U);
  if (expect(acquired.session.has_value(),
             "portable policy session can acquire its process lease") ||
      expect(acquired.session->admit_pre_context(pre_request(10'100U)).admitted,
             "portable Stage A policy remains testable")) {
    return 1;
  }
  prisminfer::NativeWorkerTrustCatalog catalog({});
  prisminfer::NativeWorkerRequest request;
  request.executable_path = "/unavailable/prisminfer-worker";
  request.timeout_ms = 1'000U;
  const auto worker = acquired.session->run_contained_worker(
      catalog, request, 100U,
      [](std::stop_token, std::uint32_t pid, std::uint64_t ready) {
        return post_request(ready, pid);
      },
      [](std::stop_token, std::uint32_t pid, std::uint64_t,
         std::uint64_t heartbeat) {
        return watchdog_sample(heartbeat, pid);
      });
  if (expect(!worker.ok &&
                 worker.failure_reason == "native_worker_windows_required" &&
                 acquired.session->state() ==
                     prisminfer::GpuAdmissionSessionState::Quarantined,
             "non-Windows production containment fails closed")) {
    return 1;
  }
#endif
  return 0;
}

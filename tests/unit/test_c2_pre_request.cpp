// Exercise the production request builder with deterministic sampling and clock
// dependencies. The renamed CLI entry point is never called by this CPU test.
#define main prism_c2_supervisor_cli_entry_for_test
#include "../../tools/prism-c2-supervisor/main.cpp"
#undef main

namespace {

bool check(bool value, const char* message) {
  if (!value) std::cerr << "FAIL: " << message << '\n';
  return value;
}

prisminfer::AdmissionCellIdentity test_cell() {
  prisminfer::AdmissionCellIdentity cell;
  cell.run_sequence = 1;
  cell.run_contract_hash.fill(1);
  cell.threshold_registry_hash.fill(2);
  cell.hardware_identity_hash.fill(3);
  cell.runtime_identity_hash.fill(4);
  cell.artifact_identity_hash.fill(5);
  cell.service_profile_hash.fill(6);
  return cell;
}

}  // namespace

int main() {
  Options options;
  options.gpu_uuid = "GPU-00000000-0000-0000-0000-000000000000";
  options.payload_bytes = 1ULL << 20;
  options.output_root = ".";
  prisminfer::WddmMemorySample wddm;
  wddm.available = true;
  wddm.adapter_luid_high = 7;
  wddm.adapter_luid_low = 11;
  wddm.captured_monotonic_milliseconds = 10'000;
  wddm.local_budget_bytes = 16 * kGiB;
  wddm.local_current_usage_bytes = 1 * kGiB;

  std::uint64_t thermal_capture = 10'200;
  std::uint64_t host_capture = 10'300;
  std::uint64_t evaluation = 10'500;
  bool observations_complete = false;
  PreRequestSampling sampling;
  sampling.thermal = [&](const std::string&) {
    prisminfer::GpuThermalSample sample;
    sample.available = true;
    sample.captured_monotonic_milliseconds = thermal_capture;
    sample.current_celsius = 55;
    sample.reported_target_celsius = 75;
    sample.reported_slowdown_celsius = 80;
    return sample;
  };
  sampling.host = [&] {
    prisminfer::HostTelemetrySample sample;
    sample.available = true;
    sample.system_commit_source = "get_performance_info";
    sample.captured_monotonic_milliseconds = host_capture;
    sample.system_memory_total_bytes = 32 * kGiB;
    sample.system_memory_available_bytes = 16 * kGiB;
    sample.system_commit_total_bytes = 16 * kGiB;
    sample.system_commit_limit_bytes = 64 * kGiB;
    sample.system_commit_available_bytes = 48 * kGiB;
    return sample;
  };
  sampling.storage = [&](const std::filesystem::path&, std::error_code& error) {
    error.clear();
    observations_complete = true;
    std::filesystem::space_info info{};
    info.available = 100 * kGiB;
    return info;
  };
  sampling.clock = [&] {
    if (!observations_complete) return std::uint64_t{0};
    return evaluation;
  };

  auto build = [&] {
    auto request = make_pre_request(options, wddm, test_cell(), sampling);
    request.exclusive_gpu_lease_held = true;
    return request;
  };
  auto request = build();
  if (!check(request.evaluation_monotonic_milliseconds == 10'500,
             "clock follows all observations") ||
      !check(request.gpu.captured_monotonic_milliseconds == 10'000 &&
                 request.thermal.captured_monotonic_milliseconds == 10'200 &&
                 request.host.captured_monotonic_milliseconds == 10'300,
             "true capture times are preserved") ||
      !check(prisminfer::evaluate_pre_context_admission(request).admitted,
             "delayed sampling at exact 500 ms GPU boundary admits")) {
    return 1;
  }

  evaluation = 10'501;
  request = build();
  if (!check(prisminfer::evaluate_pre_context_admission(request).reason ==
                 "pre_context_gpu_telemetry_invalid_or_stale",
             "one millisecond past the boundary rejects")) return 1;
  evaluation = 10'500;

  const auto expect_reason = [&](const char* reason) {
    const auto actual = prisminfer::evaluate_pre_context_admission(build()).reason;
    if (actual != reason) {
      std::cerr << "Expected " << reason << ", got " << actual << '\n';
      return false;
    }
    return true;
  };
  wddm.captured_monotonic_milliseconds = 10'501;
  if (!expect_reason("pre_context_gpu_telemetry_invalid_or_stale")) return 1;
  wddm.captured_monotonic_milliseconds = 10'000;
  thermal_capture = 10'501;
  if (!expect_reason("pre_context_gpu_thermal_telemetry_invalid_or_stale"))
    return 1;
  thermal_capture = 0;
  if (!expect_reason("pre_context_gpu_thermal_telemetry_invalid_or_stale"))
    return 1;
  thermal_capture = 10'200;
  host_capture = 10'501;
  if (!expect_reason("pre_context_host_admission_rejected")) return 1;
  host_capture = 0;
  if (!expect_reason("pre_context_host_admission_rejected")) return 1;
  host_capture = 10'300;
  wddm.local_current_usage_bytes = wddm.local_budget_bytes + 1;
  if (!expect_reason("pre_context_gpu_telemetry_invalid_or_stale")) return 1;
  wddm.local_current_usage_bytes = 1 * kGiB;
  evaluation = 0;
  if (!expect_reason("pre_context_run_deadline_overflowed")) return 1;
  return 0;
}

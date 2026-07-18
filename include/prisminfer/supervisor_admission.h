#pragma once

#include <cstdint>
#include <optional>
#include <atomic>
#include <memory>
#include <string>

#include "prisminfer/admission_cell_identity.h"
#include "prisminfer/host_admission.h"
#include "prisminfer/windows_evidence.h"

namespace prisminfer {

struct SupervisorTimingPolicy {
  std::uint64_t supervisor_sample_period_milliseconds{100};
  std::uint64_t worker_heartbeat_period_milliseconds{250};
  std::uint64_t maximum_guard_age_milliseconds{500};
  std::uint64_t cooperative_cancel_ack_milliseconds{500};
  std::uint64_t worker_exit_milliseconds{2'000};
  std::uint64_t cleanup_milliseconds{5'000};
  std::uint64_t declared_dispatch_bound_milliseconds{500};
  std::uint32_t automatic_same_context_retry_count{0};
};

enum class PredictionProvenance {
  Unavailable,
  ApprovedArtifactCensus,
  PinnedRuntimeEstimate,
  ExactZeroNotApplicable,
};

struct PredictedBytes {
  std::optional<std::uint64_t> bytes;
  PredictionProvenance provenance{PredictionProvenance::Unavailable};
};

struct PredictedGpuFootprint {
  PredictedBytes context_runtime;
  PredictedBytes backend;
  PredictedBytes model;
  PredictedBytes state;
  PredictedBytes workspace;
  PredictedBytes fragmentation;
};

struct PreContextGpuSample {
  bool available{false};
  bool adapter_identity_available{false};
  std::int32_t adapter_luid_high{0};
  std::uint32_t adapter_luid_low{0};
  std::uint64_t captured_monotonic_milliseconds{0};
  std::uint64_t physical_or_reportable_local_bytes{0};
  std::uint64_t dxgi_local_budget_bytes{0};
  std::uint64_t dxgi_local_current_usage_bytes{0};
};

struct GpuThermalSample {
  bool available{false};
  std::uint64_t captured_monotonic_milliseconds{0};
  std::int32_t current_celsius{0};
  std::optional<std::int32_t> reported_target_celsius;
  std::optional<std::int32_t> reported_slowdown_celsius;
  bool thermal_throttling{false};
  bool power_brake_slowdown{false};
};

struct StorageHeadroomSample {
  bool available{false};
  std::uint64_t free_bytes{0};
  std::uint64_t reserve_bytes{0};
  std::uint64_t required_incremental_bytes{0};
};

struct PreContextAdmissionRequest;
struct PreContextAdmissionDecision;

class PreContextAdmissionReceipt {
 public:
  PreContextAdmissionReceipt(const PreContextAdmissionReceipt&) = default;
  PreContextAdmissionReceipt(PreContextAdmissionReceipt&&) noexcept = default;
  PreContextAdmissionReceipt& operator=(const PreContextAdmissionReceipt&) =
      default;
  PreContextAdmissionReceipt& operator=(PreContextAdmissionReceipt&&) noexcept =
      default;

  [[nodiscard]] std::uint64_t policy_ceiling_bytes() const;
  [[nodiscard]] std::uint64_t requested_tier_bytes() const;
  [[nodiscard]] const SupervisorTimingPolicy& timing() const;
  [[nodiscard]] std::int32_t adapter_luid_high() const;
  [[nodiscard]] std::uint32_t adapter_luid_low() const;
  [[nodiscard]] std::uint64_t predicted_gpu_peak_bytes() const;
  [[nodiscard]] std::uint64_t pre_context_gpu_reserve_bytes() const;
  [[nodiscard]] std::uint64_t pre_context_effective_cap_bytes() const;
  [[nodiscard]] std::uint64_t run_started_monotonic_milliseconds() const;
  [[nodiscard]] std::uint64_t run_deadline_monotonic_milliseconds() const;
  [[nodiscard]] const HostReservePolicy& host_policy() const;
  [[nodiscard]] const HostAdmissionRequest& host_request() const;
  [[nodiscard]] std::int32_t gpu_warning_celsius() const;
  [[nodiscard]] std::int32_t gpu_stop_celsius() const;
  [[nodiscard]] const AdmissionCellIdentity& cell() const;

 private:
  friend PreContextAdmissionDecision evaluate_pre_context_admission(
      const PreContextAdmissionRequest& request);

  PreContextAdmissionReceipt(std::uint64_t policy_ceiling_bytes,
                             std::uint64_t requested_tier_bytes,
                             SupervisorTimingPolicy timing,
                             std::int32_t adapter_luid_high,
                             std::uint32_t adapter_luid_low,
                             std::uint64_t predicted_gpu_peak_bytes,
                             std::uint64_t pre_context_gpu_reserve_bytes,
                             std::uint64_t pre_context_effective_cap_bytes,
                             std::uint64_t run_started_monotonic_milliseconds,
                             std::uint64_t run_deadline_monotonic_milliseconds,
                             HostReservePolicy host_policy,
                             HostAdmissionRequest host_request,
                             std::int32_t gpu_warning_celsius,
                             std::int32_t gpu_stop_celsius,
                             AdmissionCellIdentity cell);

  std::uint64_t policy_ceiling_bytes_{0};
  std::uint64_t requested_tier_bytes_{0};
  SupervisorTimingPolicy timing_;
  std::int32_t adapter_luid_high_{0};
  std::uint32_t adapter_luid_low_{0};
  std::uint64_t predicted_gpu_peak_bytes_{0};
  std::uint64_t pre_context_gpu_reserve_bytes_{0};
  std::uint64_t pre_context_effective_cap_bytes_{0};
  std::uint64_t run_started_monotonic_milliseconds_{0};
  std::uint64_t run_deadline_monotonic_milliseconds_{0};
  HostReservePolicy host_policy_;
  HostAdmissionRequest host_request_;
  std::int32_t gpu_warning_celsius_{0};
  std::int32_t gpu_stop_celsius_{0};
  AdmissionCellIdentity cell_;
};

struct PreContextAdmissionRequest {
  std::uint64_t policy_ceiling_bytes{17'179'869'184ULL};
  std::uint64_t requested_tier_bytes{0};
  std::uint64_t context_tokens{0};
  std::uint64_t run_deadline_milliseconds{0};
  std::uint64_t evaluation_monotonic_milliseconds{0};
  bool exclusive_gpu_lease_held{false};
  bool competing_gpu_work_detected{false};
  SupervisorTimingPolicy timing;
  PredictedGpuFootprint predicted_gpu;
  PreContextGpuSample gpu;
  GpuThermalSample thermal;
  StorageHeadroomSample storage;
  HostTelemetrySample host;
  HostReservePolicy host_policy;
  HostAdmissionRequest host_request;
  AdmissionCellIdentity cell;
};

struct PreContextAdmissionDecision {
  bool admitted{false};
  std::string reason;
  std::uint64_t policy_ceiling_bytes{0};
  std::uint64_t requested_tier_bytes{0};
  SupervisorTimingPolicy timing;
  std::int32_t adapter_luid_high{0};
  std::uint32_t adapter_luid_low{0};
  std::uint64_t predicted_gpu_peak_bytes{0};
  std::uint64_t pre_context_live_capacity_bytes{0};
  std::uint64_t dxgi_local_headroom_bytes{0};
  std::uint64_t pre_context_gpu_reserve_bytes{0};
  std::uint64_t pre_context_effective_cap_bytes{0};
  std::int32_t gpu_warning_celsius{0};
  std::int32_t gpu_stop_celsius{0};
  std::uint64_t storage_payload_bytes{0};
  HostAdmissionDecision host_decision;
  std::optional<PreContextAdmissionReceipt> receipt;
};

[[nodiscard]] PreContextAdmissionDecision evaluate_pre_context_admission(
    const PreContextAdmissionRequest& request);

struct PostContextAdmissionRequest {
  std::optional<PreContextAdmissionReceipt> pre_context_receipt;
  std::uint64_t policy_ceiling_bytes{17'179'869'184ULL};
  std::uint64_t requested_tier_bytes{0};
  std::uint64_t evaluation_monotonic_milliseconds{0};
  SupervisorTimingPolicy timing;
  PreContextGpuSample gpu;
  GpuThermalSample thermal;
  OwnedGpuMemoryEvidence owned_gpu;
  HostTelemetrySample host;
  HostReservePolicy host_policy;
  HostAdmissionRequest host_request;
  AdmissionCellIdentity cell;
};

struct PostContextAdmissionDecision;

class PostContextAdmissionReceipt {
 public:
  PostContextAdmissionReceipt(const PostContextAdmissionReceipt&) = default;
  PostContextAdmissionReceipt(PostContextAdmissionReceipt&&) noexcept = default;
  PostContextAdmissionReceipt& operator=(const PostContextAdmissionReceipt&) =
      default;
  PostContextAdmissionReceipt& operator=(
      PostContextAdmissionReceipt&&) noexcept = default;

  [[nodiscard]] std::uint64_t receipt_id() const;
  [[nodiscard]] std::uint64_t policy_ceiling_bytes() const;
  [[nodiscard]] std::uint64_t requested_tier_bytes() const;
  [[nodiscard]] const SupervisorTimingPolicy& timing() const;
  [[nodiscard]] std::int32_t adapter_luid_high() const;
  [[nodiscard]] std::uint32_t adapter_luid_low() const;
  [[nodiscard]] std::uint64_t predicted_gpu_peak_bytes() const;
  [[nodiscard]] std::uint64_t gpu_reserve_bytes() const;
  [[nodiscard]] std::uint64_t effective_cap_bytes() const;
  [[nodiscard]] std::uint64_t evaluation_monotonic_milliseconds() const;
  [[nodiscard]] std::uint64_t run_started_monotonic_milliseconds() const;
  [[nodiscard]] std::uint64_t run_deadline_monotonic_milliseconds() const;
  [[nodiscard]] const HostReservePolicy& host_policy() const;
  [[nodiscard]] const HostAdmissionRequest& host_request() const;
  [[nodiscard]] std::int32_t gpu_warning_celsius() const;
  [[nodiscard]] std::int32_t gpu_stop_celsius() const;
  [[nodiscard]] const AdmissionCellIdentity& cell() const;

 private:
  friend class AdmissionTokenIssuer;
  friend PostContextAdmissionDecision evaluate_post_context_admission(
      const PostContextAdmissionRequest& request);

  PostContextAdmissionReceipt(
      std::uint64_t receipt_id, std::uint64_t policy_ceiling_bytes,
      std::uint64_t requested_tier_bytes, SupervisorTimingPolicy timing,
      std::int32_t adapter_luid_high, std::uint32_t adapter_luid_low,
      std::uint64_t predicted_gpu_peak_bytes, std::uint64_t gpu_reserve_bytes,
      std::uint64_t effective_cap_bytes,
      std::uint64_t evaluation_monotonic_milliseconds,
      std::uint64_t run_started_monotonic_milliseconds,
      std::uint64_t run_deadline_monotonic_milliseconds,
      HostReservePolicy host_policy, HostAdmissionRequest host_request,
      std::int32_t gpu_warning_celsius, std::int32_t gpu_stop_celsius,
      AdmissionCellIdentity cell);
  [[nodiscard]] bool consume_for_token() const;

  std::uint64_t receipt_id_{0};
  std::uint64_t policy_ceiling_bytes_{0};
  std::uint64_t requested_tier_bytes_{0};
  SupervisorTimingPolicy timing_;
  std::int32_t adapter_luid_high_{0};
  std::uint32_t adapter_luid_low_{0};
  std::uint64_t predicted_gpu_peak_bytes_{0};
  std::uint64_t gpu_reserve_bytes_{0};
  std::uint64_t effective_cap_bytes_{0};
  std::uint64_t evaluation_monotonic_milliseconds_{0};
  std::uint64_t run_started_monotonic_milliseconds_{0};
  std::uint64_t run_deadline_monotonic_milliseconds_{0};
  HostReservePolicy host_policy_;
  HostAdmissionRequest host_request_;
  std::int32_t gpu_warning_celsius_{0};
  std::int32_t gpu_stop_celsius_{0};
  AdmissionCellIdentity cell_;
  std::shared_ptr<std::atomic<bool>> token_consumed_;
};

struct PostContextAdmissionDecision {
  bool admitted{false};
  std::string reason;
  std::uint64_t reconciled_owned_current_bytes{0};
  std::uint64_t cuda_owned_capacity_bytes{0};
  std::uint64_t dxgi_owned_capacity_bytes{0};
  std::uint64_t post_context_live_capacity_bytes{0};
  std::uint64_t gpu_reserve_bytes{0};
  std::uint64_t post_context_effective_cap_bytes{0};
  HostAdmissionDecision host_decision;
  std::optional<PostContextAdmissionReceipt> receipt;
};

[[nodiscard]] PostContextAdmissionDecision evaluate_post_context_admission(
    const PostContextAdmissionRequest& request);

}  // namespace prisminfer

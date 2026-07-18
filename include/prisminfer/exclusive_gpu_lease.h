#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace prisminfer {

enum class ExclusiveGpuLeaseStatus {
  Acquired,
  InvalidAdapterIdentity,
  AlreadyHeldInProcess,
  HeldByAnotherProcess,
  SystemError,
};

struct ExclusiveGpuLeaseAcquireResult;

class ExclusiveGpuLease {
 public:
  ExclusiveGpuLease(const ExclusiveGpuLease&) = delete;
  ExclusiveGpuLease& operator=(const ExclusiveGpuLease&) = delete;
  ExclusiveGpuLease(ExclusiveGpuLease&& other) noexcept;
  ExclusiveGpuLease& operator=(ExclusiveGpuLease&& other) noexcept;
  ~ExclusiveGpuLease();

  [[nodiscard]] bool active() const;
  [[nodiscard]] std::int32_t adapter_luid_high() const;
  [[nodiscard]] std::uint32_t adapter_luid_low() const;
  [[nodiscard]] const std::string& lease_id() const;
  void quarantine_for_process_lifetime() noexcept;

 private:
  friend ExclusiveGpuLeaseAcquireResult acquire_exclusive_gpu_lease(
      std::int32_t adapter_luid_high, std::uint32_t adapter_luid_low);
  ExclusiveGpuLease(std::int32_t adapter_luid_high,
                    std::uint32_t adapter_luid_low, std::string lease_id,
                    void* native_handle);
  void release();

  std::int32_t adapter_luid_high_{0};
  std::uint32_t adapter_luid_low_{0};
  std::string lease_id_;
  void* native_handle_{nullptr};
};

struct ExclusiveGpuLeaseAcquireResult {
  ExclusiveGpuLeaseStatus status{ExclusiveGpuLeaseStatus::SystemError};
  std::optional<ExclusiveGpuLease> lease;
  std::uint32_t system_error{0};
};

[[nodiscard]] ExclusiveGpuLeaseAcquireResult acquire_exclusive_gpu_lease(
    std::int32_t adapter_luid_high, std::uint32_t adapter_luid_low);

}  // namespace prisminfer

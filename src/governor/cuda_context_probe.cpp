#include "prisminfer/cuda_context_probe.h"

#if defined(PRISMINFER_ENABLE_CUDA_PROBE)
#include <cuda_runtime_api.h>
#include <nvml.h>
#endif

namespace prisminfer {

#if defined(PRISMINFER_ENABLE_CUDA_PROBE)
namespace {

bool nvml_device_used_bytes(std::uint64_t* used_bytes) {
  nvmlReturn_t nvml_status = nvmlInit_v2();
  if (nvml_status != NVML_SUCCESS) {
    return false;
  }

  nvmlDevice_t device{};
  nvml_status = nvmlDeviceGetHandleByIndex_v2(0, &device);
  if (nvml_status != NVML_SUCCESS) {
    nvmlShutdown();
    return false;
  }

  nvmlMemory_t memory{};
  nvml_status = nvmlDeviceGetMemoryInfo(device, &memory);
  nvmlShutdown();
  if (nvml_status != NVML_SUCCESS) {
    return false;
  }

  *used_bytes = static_cast<std::uint64_t>(memory.used);
  return true;
}

}  // namespace
#endif

CudaProbeResult probe_cuda_context() {
#if defined(PRISMINFER_ENABLE_CUDA_PROBE)
  CudaProbeResult result;

  int device_count = 0;
  cudaError_t status = cudaGetDeviceCount(&device_count);
  if (status != cudaSuccess || device_count <= 0) {
    result.failure_reason =
        std::string("cuda_device_unavailable: ") + cudaGetErrorString(status);
    return result;
  }

  status = cudaSetDevice(0);
  if (status != cudaSuccess) {
    result.failure_reason =
        std::string("cuda_set_device_failed: ") + cudaGetErrorString(status);
    return result;
  }

  cudaDeviceProp properties{};
  status = cudaGetDeviceProperties(&properties, 0);
  if (status == cudaSuccess) {
    result.gpu_name = properties.name;
  }

  cudaDriverGetVersion(&result.driver_version);
  cudaRuntimeGetVersion(&result.runtime_version);

  std::uint64_t nvml_used_before = 0;
  const bool has_nvml_before = nvml_device_used_bytes(&nvml_used_before);

  status = cudaFree(nullptr);
  if (status != cudaSuccess) {
    result.failure_reason =
        std::string("cuda_context_create_failed: ") +
        cudaGetErrorString(status);
    return result;
  }

  std::size_t free_after = 0;
  std::size_t total_after = 0;
  status = cudaMemGetInfo(&free_after, &total_after);
  if (status != cudaSuccess) {
    result.failure_reason =
        std::string("cuda_mem_info_after_failed: ") +
        cudaGetErrorString(status);
    return result;
  }

  std::uint64_t nvml_used_after = 0;
  const bool has_nvml_after = nvml_device_used_bytes(&nvml_used_after);
  const auto used_after = static_cast<std::uint64_t>(total_after - free_after);
  const auto used_before =
      has_nvml_before ? nvml_used_before : used_after;

  result.available = true;
  result.context_created = true;
  result.device_used_before_bytes = used_before;
  result.device_used_after_bytes = has_nvml_after ? nvml_used_after : used_after;
  result.device_total_bytes = static_cast<std::uint64_t>(total_after);
  result.context_bytes =
      result.device_used_after_bytes > result.device_used_before_bytes
          ? result.device_used_after_bytes - result.device_used_before_bytes
          : 0;
  return result;
#else
  return CudaProbeResult{
      false,
      false,
      0,
      0,
      0,
      0,
      0,
      0,
      "",
      "cuda_probe_not_compiled"};
#endif
}

}  // namespace prisminfer

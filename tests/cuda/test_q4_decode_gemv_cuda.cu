#include <cuda_runtime.h>

#include <iostream>
#include <vector>

#include "prisminfer/kernels/q4_decode_gemv.h"

namespace prisminfer::kernels {

extern "C" void prisminfer_q4_decode_gemv_cuda_launch(const Q4Block* weights,
                                                       const float* input,
                                                       float* output,
                                                       unsigned int rows,
                                                       unsigned int cols,
                                                       cudaStream_t stream);

}  // namespace prisminfer::kernels

namespace {

int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

int expect_cuda(cudaError_t result, const char* message) {
  if (result == cudaSuccess) return 0;
  std::cerr << "FAIL: " << message << ": " << cudaGetErrorString(result)
            << "\n";
  return 1;
}

int cleanup(prisminfer::kernels::Q4Block* device_weights,
            float* device_input,
            float* device_output) {
  int failures = 0;
  if (device_weights != nullptr && cudaFree(device_weights) != cudaSuccess) {
    failures = 1;
  }
  if (device_input != nullptr && cudaFree(device_input) != cudaSuccess) {
    failures = 1;
  }
  if (device_output != nullptr && cudaFree(device_output) != cudaSuccess) {
    failures = 1;
  }
  return failures;
}

}  // namespace

int main() {
  constexpr unsigned int kRows = 2;
  constexpr unsigned int kCols = 4;
  const std::vector<prisminfer::kernels::Q4Block> weights = {
      {1.0F, 0x98U}, {0.5F, 0xA7U}, {1.0F, 0x87U}, {0.25F, 0xB6U}};
  const std::vector<float> input = {1.0F, 2.0F, -1.0F, 0.5F};
  const auto reference = prisminfer::kernels::q4_decode_gemv_reference(
      weights, input, kRows, kCols);
  std::vector<float> output(reference.size(), 0.0F);

  prisminfer::kernels::Q4Block* device_weights = nullptr;
  float* device_input = nullptr;
  float* device_output = nullptr;
  if (expect_cuda(cudaMalloc(reinterpret_cast<void**>(&device_weights),
                             weights.size() * sizeof(weights.front())),
                  "cudaMalloc weights") ||
      expect_cuda(cudaMalloc(reinterpret_cast<void**>(&device_input),
                             input.size() * sizeof(input.front())),
                  "cudaMalloc input") ||
      expect_cuda(cudaMalloc(reinterpret_cast<void**>(&device_output),
                             output.size() * sizeof(output.front())),
                  "cudaMalloc output")) {
    cleanup(device_weights, device_input, device_output);
    return 1;
  }

  if (expect_cuda(cudaMemcpy(device_weights, weights.data(),
                             weights.size() * sizeof(weights.front()),
                             cudaMemcpyHostToDevice),
                  "cudaMemcpy weights") ||
      expect_cuda(cudaMemcpy(device_input, input.data(),
                             input.size() * sizeof(input.front()),
                             cudaMemcpyHostToDevice),
                  "cudaMemcpy input")) {
    cleanup(device_weights, device_input, device_output);
    return 1;
  }

  prisminfer::kernels::prisminfer_q4_decode_gemv_cuda_launch(
      device_weights, device_input, device_output, kRows, kCols, nullptr);
  if (expect_cuda(cudaGetLastError(), "q4 decode gemv launch") ||
      expect_cuda(cudaDeviceSynchronize(), "q4 decode gemv sync") ||
      expect_cuda(cudaMemcpy(output.data(), device_output,
                             output.size() * sizeof(output.front()),
                             cudaMemcpyDeviceToHost),
                  "cudaMemcpy output")) {
    cleanup(device_weights, device_input, device_output);
    return 1;
  }

  const auto comparison = prisminfer::kernels::compare_q4_decode_gemv_to_reference(
      output, reference, 0.001);
  if (expect(comparison.ok, "CUDA q4 output matches CPU reference")) {
    std::cerr << "FAIL: reason=" << comparison.reason
              << " max_abs_error=" << comparison.max_abs_error << "\n";
    cleanup(device_weights, device_input, device_output);
    return 1;
  }
  return cleanup(device_weights, device_input, device_output);
}

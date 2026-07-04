#include "prisminfer/kernels/q4_decode_gemv.h"

#include <cuda_runtime.h>

namespace prisminfer::kernels {

namespace {

__device__ float decode_nibble_device(unsigned char nibble, float scale) {
  return static_cast<float>(static_cast<int>(nibble & 0x0F) - 8) * scale;
}

__global__ void q4_decode_gemv_kernel(const Q4Block* weights,
                                      const float* input,
                                      float* output,
                                      unsigned int rows,
                                      unsigned int cols) {
  const unsigned int row = blockIdx.x * blockDim.x + threadIdx.x;
  if (row >= rows) {
    return;
  }
  const unsigned long long blocks_per_row =
      (static_cast<unsigned long long>(cols) + 1ULL) / 2ULL;
  float sum = 0.0F;
  for (unsigned int col = 0; col < cols; ++col) {
    const unsigned long long block_index =
        static_cast<unsigned long long>(row) * blocks_per_row + (col / 2U);
    const Q4Block block = weights[block_index];
    const unsigned char nibble =
        (col % 2U == 0U) ? (block.packed_values & 0x0FU)
                         : ((block.packed_values >> 4U) & 0x0FU);
    sum += decode_nibble_device(nibble, block.scale) * input[col];
  }
  output[row] = sum;
}

}  // namespace

extern "C" void prisminfer_q4_decode_gemv_cuda_launch(
    const Q4Block* weights,
    const float* input,
    float* output,
    unsigned int rows,
    unsigned int cols,
    cudaStream_t stream) {
  constexpr unsigned int kThreadsPerBlock = 128;
  const unsigned int blocks = (rows + kThreadsPerBlock - 1U) / kThreadsPerBlock;
  q4_decode_gemv_kernel<<<blocks, kThreadsPerBlock, 0, stream>>>(
      weights, input, output, rows, cols);
}

}  // namespace prisminfer::kernels

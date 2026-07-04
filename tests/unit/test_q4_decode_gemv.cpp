#include <cmath>
#include <iostream>
#include <vector>

#include "prisminfer/kernels/q4_decode_gemv.h"

namespace {
int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}
}  // namespace

int main() {
  const std::vector<prisminfer::kernels::Q4Block> weights = {
      {1.0F, 0x98U}, {0.5F, 0xA7U}, {1.0F, 0x87U}, {0.25F, 0xB6U}};
  const std::vector<float> input = {1.0F, 2.0F, -1.0F, 0.5F};
  const auto output =
      prisminfer::kernels::q4_decode_gemv_reference(weights, input, 2, 4);
  if (expect(output.size() == 2, "two output rows")) return 1;
  if (expect(std::fabs(output[0] - 3.0F) < 0.001F,
             "first q4 gemv row")) {
    return 1;
  }
  if (expect(std::fabs(output[1] - -0.125F) < 0.001F,
             "second q4 gemv row")) {
    return 1;
  }
  const auto comparison =
      prisminfer::kernels::compare_q4_decode_gemv_to_reference(output, output,
                                                               0.0);
  if (expect(comparison.ok, "self comparison passes")) return 1;
  return 0;
}

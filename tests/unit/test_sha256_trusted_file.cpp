#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "prisminfer/sha256.h"

namespace {
int expect(bool condition, const char* message) {
  if (condition) return 0;
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}
}  // namespace

int main() {
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto temporary = std::filesystem::temp_directory_path() /
                         ("prisminfer-sha256-trust-" + std::to_string(nonce));
  const auto root = temporary / "root";
  const auto outside = temporary / "outside";
  std::error_code error_code;
  std::filesystem::create_directories(root, error_code);
  std::filesystem::create_directories(outside, error_code);
  if (expect(!error_code, "temporary directories created")) return 1;
  {
    std::ofstream output(root / "inside.txt", std::ios::binary);
    output << "inside";
  }
  {
    std::ofstream output(outside / "outside.txt", std::ios::binary);
    output << "outside";
  }

  std::string digest;
  std::string error;
  if (expect(prisminfer::sha256_trusted_regular_file_bounded(
                 root, root / "inside.txt", 64U, &digest, &error),
             error.c_str())) {
    std::filesystem::remove_all(temporary, error_code);
    return 1;
  }
  if (expect(digest ==
                 "106b086224a4d945eae25f7be3805a931a873270326dd868b0e41f71ee9fff72",
             "trusted file digest matches")) {
    std::filesystem::remove_all(temporary, error_code);
    return 1;
  }

  error.clear();
  if (expect(!prisminfer::sha256_trusted_regular_file_bounded(
                 root, outside / "outside.txt", 64U, &digest, &error),
             "outside file rejected")) {
    std::filesystem::remove_all(temporary, error_code);
    return 1;
  }

  error_code.clear();
  std::filesystem::create_directory_symlink(outside, root / "escape", error_code);
#ifndef _WIN32
  if (expect(!error_code, "POSIX symlink fixture created")) {
    std::filesystem::remove_all(temporary, error_code);
    return 1;
  }
#endif
  if (!error_code) {
    error.clear();
    if (expect(!prisminfer::sha256_trusted_regular_file_bounded(
                   root, root / "escape" / "outside.txt", 64U, &digest, &error),
               "symlink traversal rejected")) {
      std::filesystem::remove_all(temporary, error_code);
      return 1;
    }
  }

  std::filesystem::remove_all(temporary, error_code);
  return 0;
}

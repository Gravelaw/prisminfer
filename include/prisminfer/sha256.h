#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace prisminfer {

bool sha256_file(const std::filesystem::path& path, std::string* digest,
                 std::string* error);

bool sha256_text(const std::string& text, std::string* digest);

bool sha256_regular_file_bounded(const std::filesystem::path& path,
                                 std::uint64_t maximum_bytes,
                                 std::string* digest,
                                 std::string* error);

bool sha256_trusted_regular_file_bounded(
    const std::filesystem::path& trusted_root,
    const std::filesystem::path& path,
    std::uint64_t maximum_bytes,
    std::string* digest,
    std::string* error);

}  // namespace prisminfer

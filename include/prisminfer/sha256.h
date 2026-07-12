#pragma once

#include <filesystem>
#include <string>

namespace prisminfer {

bool sha256_file(const std::filesystem::path& path, std::string* digest,
                 std::string* error);

}  // namespace prisminfer

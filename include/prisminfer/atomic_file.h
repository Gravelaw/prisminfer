#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace prisminfer {

using AtomicFileValidator = std::function<bool(
    const std::filesystem::path&, std::string*)>;

// Accepts only a direct child of an existing, non-symlink directory. This is
// the path-level guard used before the atomic publisher operates below an
// explicitly configured output root.
bool is_direct_child_of_trusted_directory(
    const std::filesystem::path& trusted_directory,
    const std::filesystem::path& candidate, std::string* error);

// Publishes a complete file without replacing an existing pathname. An
// existing single-link regular file is accepted only when its bytes already
// match exactly. Temporary storage is created inside an exclusive directory
// below the target parent, validated before publication, and hard-linked into
// place so publication cannot follow or truncate an attacker-precreated path.
bool write_new_or_same_text_file_atomically(
    const std::filesystem::path& path, std::string_view contents,
    std::uint64_t maximum_bytes, const AtomicFileValidator& validator,
    std::string* error);

}  // namespace prisminfer

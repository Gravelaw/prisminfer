#include "prisminfer/atomic_file.h"

#include <array>
#include <chrono>
#include <exception>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <system_error>

namespace prisminfer {
namespace {

constexpr std::size_t kMaximumCreateAttempts = 32U;

bool read_exact_bounded(const std::filesystem::path& path,
                        std::uint64_t maximum_bytes, std::string* contents,
                        std::string* error) {
  std::error_code status_error;
  const auto status = std::filesystem::symlink_status(path, status_error);
  if (status_error || !std::filesystem::is_regular_file(status) ||
      std::filesystem::is_symlink(status)) {
    if (error) *error = "atomic_file_existing_not_regular";
    return false;
  }
  if (std::filesystem::hard_link_count(path, status_error) != 1U ||
      status_error) {
    if (error) *error = "atomic_file_existing_link_count_rejected";
    return false;
  }
  const auto size = std::filesystem::file_size(path, status_error);
  if (status_error || size > maximum_bytes) {
    if (error) *error = "atomic_file_existing_size_rejected";
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    if (error) *error = "atomic_file_existing_open_failed";
    return false;
  }
  contents->assign(std::istreambuf_iterator<char>(input),
                   std::istreambuf_iterator<char>());
  if (input.bad()) {
    if (error) *error = "atomic_file_existing_read_failed";
    return false;
  }
  return true;
}

std::string create_nonce() {
  std::array<std::uint32_t, 4> words{};
  std::random_device random;
  for (auto& word : words) word = random();
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const auto word : words) output << std::setw(8) << word;
  output << '-' << std::chrono::steady_clock::now().time_since_epoch().count();
  return output.str();
}

void cleanup_temporary(const std::filesystem::path& directory) {
  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);
}

}  // namespace

bool is_direct_child_of_trusted_directory(
    const std::filesystem::path& trusted_directory,
    const std::filesystem::path& candidate, std::string* error) {
  if (candidate.filename().empty()) {
    if (error) *error = "trusted_output_filename_missing";
    return false;
  }
  std::error_code status_error;
  const auto root_status =
      std::filesystem::symlink_status(trusted_directory, status_error);
  if (status_error || !std::filesystem::is_directory(root_status) ||
      std::filesystem::is_symlink(root_status)) {
    if (error) *error = "trusted_output_root_rejected";
    return false;
  }
  const auto canonical_root =
      std::filesystem::weakly_canonical(trusted_directory, status_error);
  if (status_error) {
    if (error) *error = "trusted_output_root_canonicalization_failed";
    return false;
  }
  const auto candidate_parent = candidate.has_parent_path()
                                    ? candidate.parent_path()
                                    : std::filesystem::path(".");
  const auto canonical_parent =
      std::filesystem::weakly_canonical(candidate_parent, status_error);
  if (status_error || canonical_parent != canonical_root) {
    if (error) *error = "trusted_output_path_rejected";
    return false;
  }
  return true;
}

bool write_new_or_same_text_file_atomically(
    const std::filesystem::path& path, std::string_view contents,
    std::uint64_t maximum_bytes, const AtomicFileValidator& validator,
    std::string* error) {
  if (contents.size() > maximum_bytes || path.filename().empty()) {
    if (error) *error = "atomic_file_input_rejected";
    return false;
  }
  std::error_code status_error;
  if (std::filesystem::exists(path, status_error)) {
    std::string existing;
    if (!read_exact_bounded(path, maximum_bytes, &existing, error)) return false;
    if (existing != contents) {
      if (error) *error = "atomic_file_existing_content_mismatch";
      return false;
    }
    return !validator || validator(path, error);
  }
  if (status_error) {
    if (error) *error = "atomic_file_target_status_failed";
    return false;
  }

  const auto parent = path.has_parent_path() ? path.parent_path()
                                              : std::filesystem::path(".");
  const auto parent_status = std::filesystem::symlink_status(parent, status_error);
  if (status_error || !std::filesystem::is_directory(parent_status) ||
      std::filesystem::is_symlink(parent_status)) {
    if (error) *error = "atomic_file_parent_rejected";
    return false;
  }

  std::filesystem::path temporary_directory;
  for (std::size_t attempt = 0U; attempt < kMaximumCreateAttempts; ++attempt) {
    try {
      temporary_directory =
          parent / (".prisminfer-" + create_nonce() + ".tmp");
    } catch (const std::exception&) {
      if (error) *error = "atomic_file_nonce_generation_failed";
      return false;
    }
    status_error.clear();
    if (std::filesystem::create_directory(temporary_directory, status_error)) {
      break;
    }
    temporary_directory.clear();
    if (status_error && status_error != std::errc::file_exists) {
      if (error) *error = "atomic_file_temporary_directory_failed";
      return false;
    }
  }
  if (temporary_directory.empty()) {
    if (error) *error = "atomic_file_temporary_collision_limit";
    return false;
  }

  const auto temporary_file = temporary_directory / "payload";
  {
    std::ofstream output(temporary_file,
                         std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output) {
      cleanup_temporary(temporary_directory);
      if (error) *error = "atomic_file_temporary_open_failed";
      return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.flush();
    if (!output) {
      cleanup_temporary(temporary_directory);
      if (error) *error = "atomic_file_temporary_write_failed";
      return false;
    }
  }
  if (validator && !validator(temporary_file, error)) {
    cleanup_temporary(temporary_directory);
    return false;
  }

  status_error.clear();
  std::filesystem::create_hard_link(temporary_file, path, status_error);
  if (status_error) {
    cleanup_temporary(temporary_directory);
    if (error) *error = "atomic_file_publish_failed";
    return false;
  }
  cleanup_temporary(temporary_directory);
  return true;
}

}  // namespace prisminfer

#include "prisminfer/flat_json.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace prisminfer {

namespace {

void skip_ws(const std::string& value, std::size_t* position) {
  while (*position < value.size() &&
         std::isspace(static_cast<unsigned char>(value[*position])) != 0) {
    ++(*position);
  }
}

bool parse_json_string(const std::string& value,
                       std::size_t* position,
                       std::string* output) {
  skip_ws(value, position);
  if (*position >= value.size() || value[*position] != '"') {
    return false;
  }
  ++(*position);
  std::string parsed;
  while (*position < value.size()) {
    const char ch = value[*position];
    ++(*position);
    if (ch == '"') {
      *output = parsed;
      return true;
    }
    if (static_cast<unsigned char>(ch) < 0x20U) {
      return false;
    }
    if (ch == '\\') {
      if (*position >= value.size()) {
        return false;
      }
      const char escaped = value[*position];
      ++(*position);
      switch (escaped) {
        case '"':
        case '\\':
        case '/':
          parsed.push_back(escaped);
          break;
        case 'n':
          parsed.push_back('\n');
          break;
        case 'r':
          parsed.push_back('\r');
          break;
        case 't':
          parsed.push_back('\t');
          break;
        default:
          return false;
      }
      continue;
    }
    parsed.push_back(ch);
  }
  return false;
}

bool parse_json_literal(const std::string& json,
                        std::size_t* position,
                        FlatJsonValue* output) {
  skip_ws(json, position);
  if (*position >= json.size()) {
    return false;
  }
  if (json[*position] == '"') {
    output->kind = FlatJsonValue::Kind::String;
    return parse_json_string(json, position, &output->text);
  }
  if (json.compare(*position, 4, "true") == 0) {
    *position += 4;
    output->kind = FlatJsonValue::Kind::Boolean;
    output->text = "true";
    return true;
  }
  if (json.compare(*position, 5, "false") == 0) {
    *position += 5;
    output->kind = FlatJsonValue::Kind::Boolean;
    output->text = "false";
    return true;
  }
  if (json[*position] == '{' || json[*position] == '[') {
    return false;
  }
  const std::size_t start = *position;
  while (*position < json.size()) {
    const char ch = json[*position];
    if (ch == ',' || ch == '}' ||
        std::isspace(static_cast<unsigned char>(ch)) != 0) {
      break;
    }
    ++(*position);
  }
  if (start == *position) {
    return false;
  }
  output->kind = FlatJsonValue::Kind::Number;
  output->text = json.substr(start, *position - start);
  return true;
}

FlatJsonResult fail(const std::string& error) {
  FlatJsonResult result;
  result.error = error;
  return result;
}

#ifdef _WIN32

class NativeManifestHandle {
 public:
  explicit NativeManifestHandle(HANDLE handle) : handle_(handle) {}
  ~NativeManifestHandle() {
    if (handle_ != INVALID_HANDLE_VALUE) {
      (void)CloseHandle(handle_);
    }
  }
  NativeManifestHandle(const NativeManifestHandle&) = delete;
  NativeManifestHandle& operator=(const NativeManifestHandle&) = delete;
  HANDLE get() const { return handle_; }

 private:
  HANDLE handle_{INVALID_HANDLE_VALUE};
};

#else

class NativeManifestHandle {
 public:
  explicit NativeManifestHandle(int handle) : handle_(handle) {}
  ~NativeManifestHandle() {
    if (handle_ >= 0) {
      (void)close(handle_);
    }
  }
  NativeManifestHandle(const NativeManifestHandle&) = delete;
  NativeManifestHandle& operator=(const NativeManifestHandle&) = delete;
  int get() const { return handle_; }

 private:
  int handle_{-1};
};

#endif

FlatJsonResult parse_flat_json_object(const std::string& json) {
  FlatJsonResult result;
  std::size_t position = 0;
  skip_ws(json, &position);
  if (position >= json.size() || json[position] != '{') {
    return fail("manifest_json_malformed");
  }
  ++position;
  skip_ws(json, &position);
  if (position < json.size() && json[position] == '}') {
    ++position;
    skip_ws(json, &position);
    result.ok = position == json.size();
    result.error = result.ok ? "" : "manifest_json_malformed";
    return result;
  }

  while (position < json.size()) {
    std::string key;
    FlatJsonValue value;
    if (!parse_json_string(json, &position, &key)) {
      return fail("manifest_json_malformed");
    }
    skip_ws(json, &position);
    if (position >= json.size() || json[position] != ':') {
      return fail("manifest_json_malformed");
    }
    ++position;
    if (!parse_json_literal(json, &position, &value)) {
      return fail("manifest_json_malformed");
    }
    if (result.fields.find(key) != result.fields.end()) {
      return fail("duplicate_field:" + key);
    }
    result.fields.emplace(key, value);
    skip_ws(json, &position);
    if (position >= json.size()) {
      return fail("manifest_json_malformed");
    }
    if (json[position] == '}') {
      ++position;
      skip_ws(json, &position);
      result.ok = position == json.size();
      result.error = result.ok ? "" : "manifest_json_malformed";
      return result;
    }
    if (json[position] != ',') {
      return fail("manifest_json_malformed");
    }
    ++position;
  }
  return fail("manifest_json_malformed");
}

}  // namespace

FlatJsonResult read_flat_json_file(const std::filesystem::path& path,
                                   std::uint64_t max_bytes) {
#ifdef _WIN32
  const NativeManifestHandle handle(CreateFileW(
      path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT |
          FILE_FLAG_BACKUP_SEMANTICS,
      nullptr));
  if (handle.get() == INVALID_HANDLE_VALUE) {
    return fail("manifest_open_failed");
  }
  BY_HANDLE_FILE_INFORMATION before{};
  if (!GetFileInformationByHandle(handle.get(), &before)) {
    return fail("manifest_metadata_unavailable");
  }
  if (GetFileType(handle.get()) != FILE_TYPE_DISK ||
      (before.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U ||
      (before.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
    return fail("manifest_not_regular_file");
  }
  const std::uint64_t size =
      (static_cast<std::uint64_t>(before.nFileSizeHigh) << 32U) |
      static_cast<std::uint64_t>(before.nFileSizeLow);
#else
  const NativeManifestHandle handle(
      open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  if (handle.get() < 0) {
    return fail("manifest_open_failed");
  }
  struct stat before {};
  if (fstat(handle.get(), &before) != 0) {
    return fail("manifest_metadata_unavailable");
  }
  if (!S_ISREG(before.st_mode) || before.st_size < 0) {
    return fail("manifest_not_regular_file");
  }
  const std::uint64_t size = static_cast<std::uint64_t>(before.st_size);
#endif
  if (size > max_bytes ||
      size > (std::numeric_limits<std::size_t>::max)()) {
    return fail("manifest_size_exceeds_limit");
  }

  std::string content(static_cast<std::size_t>(size), '\0');
  std::size_t offset = 0;
  while (offset < content.size()) {
#ifdef _WIN32
    const auto remaining = content.size() - offset;
    const auto request = static_cast<DWORD>(
        (std::min)(remaining, static_cast<std::size_t>(MAXDWORD)));
    DWORD received = 0;
    if (!ReadFile(handle.get(), content.data() + offset, request, &received,
                  nullptr) || received == 0U) {
      return fail("manifest_read_failed");
    }
    offset += static_cast<std::size_t>(received);
#else
    const auto received =
        read(handle.get(), content.data() + offset, content.size() - offset);
    if (received <= 0) {
      return fail("manifest_read_failed");
    }
    offset += static_cast<std::size_t>(received);
#endif
  }

#ifdef _WIN32
  BY_HANDLE_FILE_INFORMATION after{};
  if (!GetFileInformationByHandle(handle.get(), &after) ||
      after.nFileIndexHigh != before.nFileIndexHigh ||
      after.nFileIndexLow != before.nFileIndexLow ||
      after.nFileSizeHigh != before.nFileSizeHigh ||
      after.nFileSizeLow != before.nFileSizeLow ||
      CompareFileTime(&after.ftLastWriteTime, &before.ftLastWriteTime) != 0) {
    return fail("manifest_changed_during_read");
  }
#else
  struct stat after {};
  if (fstat(handle.get(), &after) != 0 || after.st_dev != before.st_dev ||
      after.st_ino != before.st_ino || after.st_size != before.st_size ||
#if defined(__APPLE__)
      after.st_mtimespec.tv_sec != before.st_mtimespec.tv_sec ||
      after.st_mtimespec.tv_nsec != before.st_mtimespec.tv_nsec) {
#else
      after.st_mtim.tv_sec != before.st_mtim.tv_sec ||
      after.st_mtim.tv_nsec != before.st_mtim.tv_nsec) {
#endif
    return fail("manifest_changed_during_read");
  }
#endif
  return parse_flat_json_object(content);
}

}  // namespace prisminfer

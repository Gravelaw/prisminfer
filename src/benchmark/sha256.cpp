#include "prisminfer/sha256.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace prisminfer {
namespace {
constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
std::uint32_t rotate_right(std::uint32_t value, std::uint32_t bits) {
  return (value >> bits) | (value << (32U - bits));
}
void process_block(const std::uint8_t* block, std::array<std::uint32_t, 8>* state) {
  std::array<std::uint32_t, 64> words{};
  for (std::size_t i = 0; i < 16; ++i) words[i] = (static_cast<std::uint32_t>(block[4*i]) << 24U) | (static_cast<std::uint32_t>(block[4*i+1]) << 16U) | (static_cast<std::uint32_t>(block[4*i+2]) << 8U) | block[4*i+3];
  for (std::size_t i = 16; i < words.size(); ++i) {
    const auto s0 = rotate_right(words[i-15], 7) ^ rotate_right(words[i-15], 18) ^ (words[i-15] >> 3U);
    const auto s1 = rotate_right(words[i-2], 17) ^ rotate_right(words[i-2], 19) ^ (words[i-2] >> 10U);
    words[i] = words[i-16] + s0 + words[i-7] + s1;
  }
  auto a=(*state)[0], b=(*state)[1], c=(*state)[2], d=(*state)[3], e=(*state)[4], f=(*state)[5], g=(*state)[6], h=(*state)[7];
  for (std::size_t i=0;i<64;++i) {
    const auto s1=rotate_right(e,6)^rotate_right(e,11)^rotate_right(e,25);
    const auto choose=(e&f)^((~e)&g);
    const auto t1=h+s1+choose+kRoundConstants[i]+words[i];
    const auto s0=rotate_right(a,2)^rotate_right(a,13)^rotate_right(a,22);
    const auto majority=(a&b)^(a&c)^(b&c);
    const auto t2=s0+majority;
    h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
  }
  (*state)[0]+=a;(*state)[1]+=b;(*state)[2]+=c;(*state)[3]+=d;(*state)[4]+=e;(*state)[5]+=f;(*state)[6]+=g;(*state)[7]+=h;
}

bool sha256_bytes(const std::vector<std::uint8_t>& data, std::string* digest) {
  std::array<std::uint32_t,8> state{0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
  std::array<std::uint8_t,64> block{};
  std::size_t position=0;
  while (position + 64U <= data.size()) { process_block(data.data()+position, &state); position += 64U; }
  const std::size_t remaining=data.size()-position;
  block.fill(0); for (std::size_t i=0;i<remaining;++i) block[i]=data[position+i];
  block[remaining]=0x80U;
  if (remaining >= 56U) { process_block(block.data(), &state); block.fill(0); }
  const std::uint64_t bit_count=static_cast<std::uint64_t>(data.size())*8U;
  for (std::size_t i=0;i<8;++i) block[63U-i]=static_cast<std::uint8_t>(bit_count>>(8U*i));
  process_block(block.data(), &state);
  std::ostringstream out; out<<std::hex<<std::setfill('0');
  for (const auto value:state) out<<std::setw(8)<<value;
  *digest=out.str(); return true;
}

#ifdef _WIN32
class ScopedHandle {
 public:
  explicit ScopedHandle(HANDLE value) : value_(value) {}
  ~ScopedHandle() { if (value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
  ScopedHandle(const ScopedHandle&) = delete;
  ScopedHandle& operator=(const ScopedHandle&) = delete;
  HANDLE get() const { return value_; }

 private:
  HANDLE value_{INVALID_HANDLE_VALUE};
};

bool final_path_from_handle(HANDLE handle, std::wstring* output) {
  const DWORD required = GetFinalPathNameByHandleW(
      handle, nullptr, 0U, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (required == 0U) return false;
  std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1U);
  const DWORD written = GetFinalPathNameByHandleW(
      handle, buffer.data(), static_cast<DWORD>(buffer.size()),
      FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (written == 0U || written >= buffer.size()) return false;
  output->assign(buffer.data(), written);
  return true;
}

bool full_input_path(const std::filesystem::path& path, std::wstring* output) {
  const DWORD required = GetFullPathNameW(path.c_str(), 0U, nullptr, nullptr);
  if (required == 0U) return false;
  std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1U);
  const DWORD written = GetFullPathNameW(path.c_str(),
                                         static_cast<DWORD>(buffer.size()),
                                         buffer.data(), nullptr);
  if (written == 0U || written >= buffer.size()) return false;
  output->assign(buffer.data(), written);
  return true;
}

std::wstring with_extended_prefix(const std::wstring& path) {
  if (path.rfind(L"\\\\?\\", 0U) == 0U) return path;
  if (path.rfind(L"\\\\", 0U) == 0U) return L"\\\\?\\UNC\\" + path.substr(2U);
  return L"\\\\?\\" + path;
}

bool equal_path_ci(const std::wstring& left, const std::wstring& right) {
  return left.size() == right.size() &&
         CompareStringOrdinal(left.data(), static_cast<int>(left.size()),
                              right.data(), static_cast<int>(right.size()),
                              TRUE) == CSTR_EQUAL;
}

bool final_path_is_descendant(const std::wstring& root,
                              const std::wstring& candidate) {
  if (candidate.size() <= root.size() ||
      CompareStringOrdinal(root.data(), static_cast<int>(root.size()),
                           candidate.data(), static_cast<int>(root.size()),
                           TRUE) != CSTR_EQUAL) {
    return false;
  }
  const wchar_t separator = candidate[root.size()];
  return separator == L'\\' || separator == L'/';
}

bool sha256_windows_trusted_handle(const std::filesystem::path& trusted_root,
                                   const std::filesystem::path& path,
                                   std::uint64_t maximum_bytes,
                                   std::string* digest,
                                   std::string* error) {
  ScopedHandle root(CreateFileW(trusted_root.c_str(), FILE_READ_ATTRIBUTES,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                nullptr));
  if (root.get() == INVALID_HANDLE_VALUE) {
    if (error) *error = "sha256_trusted_root_rejected";
    return false;
  }
  BY_HANDLE_FILE_INFORMATION root_info{};
  std::wstring root_final;
  std::wstring root_input;
  if (!GetFileInformationByHandle(root.get(), &root_info) ||
      (root_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U ||
      (root_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U ||
      !final_path_from_handle(root.get(), &root_final) ||
      !full_input_path(trusted_root, &root_input) ||
      !equal_path_ci(root_final, with_extended_prefix(root_input))) {
    if (error) *error = "sha256_trusted_root_rejected";
    return false;
  }

  ScopedHandle evidence(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING,
                                    FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (evidence.get() == INVALID_HANDLE_VALUE) {
    if (error) *error = "sha256_input_open_failed";
    return false;
  }
  BY_HANDLE_FILE_INFORMATION evidence_info{};
  std::wstring evidence_final;
  if (!GetFileInformationByHandle(evidence.get(), &evidence_info) ||
      (evidence_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U ||
      (evidence_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U ||
      !final_path_from_handle(evidence.get(), &evidence_final) ||
      !final_path_is_descendant(root_final, evidence_final)) {
    if (error) *error = "sha256_input_reparse_point";
    return false;
  }

  std::vector<std::uint8_t> data;
  std::array<std::uint8_t, 8192> buffer{};
  for (;;) {
    DWORD read = 0U;
    if (!ReadFile(evidence.get(), buffer.data(), static_cast<DWORD>(buffer.size()),
                  &read, nullptr)) {
      if (error) *error = "sha256_input_read_failed";
      return false;
    }
    if (read == 0U) break;
    const auto count = static_cast<std::uint64_t>(read);
    if (count > maximum_bytes ||
        static_cast<std::uint64_t>(data.size()) > maximum_bytes - count) {
      if (error) *error = "sha256_input_size_exceeds_limit";
      return false;
    }
    data.insert(data.end(), buffer.begin(), buffer.begin() + read);
  }
  return sha256_bytes(data, digest);
}
#else
class ScopedFd {
 public:
  explicit ScopedFd(int value = -1) : value_(value) {}
  ~ScopedFd() {
    if (value_ >= 0) close(value_);
  }
  ScopedFd(const ScopedFd&) = delete;
  ScopedFd& operator=(const ScopedFd&) = delete;
  ScopedFd(ScopedFd&& other) noexcept : value_(other.value_) {
    other.value_ = -1;
  }
  ScopedFd& operator=(ScopedFd&& other) noexcept {
    if (this != &other) {
      if (value_ >= 0) close(value_);
      value_ = other.value_;
      other.value_ = -1;
    }
    return *this;
  }
  int get() const { return value_; }

 private:
  int value_{-1};
};

bool sha256_posix_trusted_handle(const std::filesystem::path& trusted_root,
                                 const std::filesystem::path& path,
                                 std::uint64_t maximum_bytes,
                                 std::string* digest,
                                 std::string* error) {
  std::error_code path_error;
  const auto root =
      std::filesystem::absolute(trusted_root, path_error).lexically_normal();
  const auto candidate =
      std::filesystem::absolute(path, path_error).lexically_normal();
  if (path_error) {
    if (error) *error = "sha256_input_outside_trusted_root";
    return false;
  }
  const auto relative = candidate.lexically_relative(root);
  if (relative.empty() || relative == ".") {
    if (error) *error = "sha256_input_outside_trusted_root";
    return false;
  }
  for (const auto& component : relative) {
    if (component.empty() || component == "." || component == "..") {
      if (error) *error = "sha256_input_outside_trusted_root";
      return false;
    }
  }

  ScopedFd current(open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW |
                                          O_CLOEXEC));
  if (current.get() < 0) {
    if (error) *error = "sha256_trusted_root_rejected";
    return false;
  }
  struct stat metadata {};
  if (fstat(current.get(), &metadata) != 0 || !S_ISDIR(metadata.st_mode)) {
    if (error) *error = "sha256_trusted_root_rejected";
    return false;
  }

  auto component = relative.begin();
  while (component != relative.end()) {
    const auto next = std::next(component);
    const bool final = next == relative.end();
    const int flags = O_RDONLY | O_NOFOLLOW | O_CLOEXEC |
                      (final ? 0 : O_DIRECTORY);
    ScopedFd opened(openat(current.get(), component->c_str(), flags));
    if (opened.get() < 0 || fstat(opened.get(), &metadata) != 0 ||
        (final ? !S_ISREG(metadata.st_mode) : !S_ISDIR(metadata.st_mode))) {
      if (error) *error = "sha256_input_reparse_point";
      return false;
    }
    current = std::move(opened);
    component = next;
  }

  std::vector<std::uint8_t> data;
  std::array<std::uint8_t, 8192> buffer{};
  for (;;) {
    const ssize_t count = read(current.get(), buffer.data(), buffer.size());
    if (count < 0 && errno == EINTR) continue;
    if (count < 0) {
      if (error) *error = "sha256_input_read_failed";
      return false;
    }
    if (count == 0) break;
    const auto read_count = static_cast<std::uint64_t>(count);
    if (read_count > maximum_bytes ||
        static_cast<std::uint64_t>(data.size()) > maximum_bytes - read_count) {
      if (error) *error = "sha256_input_size_exceeds_limit";
      return false;
    }
    data.insert(data.end(), buffer.begin(), buffer.begin() + count);
  }
  return sha256_bytes(data, digest);
}
#endif
}  // namespace

bool sha256_file(const std::filesystem::path& path, std::string* digest, std::string* error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) { if (error) *error="sha256_input_open_failed"; return false; }
  const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)),
                                       std::istreambuf_iterator<char>());
  if (input.bad()) { if (error) *error="sha256_input_read_failed"; return false; }
  return sha256_bytes(data, digest);
}

bool sha256_regular_file_bounded(const std::filesystem::path& path,
                                 std::uint64_t maximum_bytes,
                                 std::string* digest,
                                 std::string* error) {
  std::error_code status_error;
  if (!std::filesystem::is_regular_file(path, status_error) || status_error) {
    if (error) *error = "sha256_input_not_regular_file";
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) { if (error) *error = "sha256_input_open_failed"; return false; }
  std::vector<std::uint8_t> data;
  std::array<char, 8192> buffer{};
  while (input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto count = input.gcount();
    if (count <= 0) break;
    const auto read_count = static_cast<std::uint64_t>(count);
    if (read_count > maximum_bytes ||
        static_cast<std::uint64_t>(data.size()) > maximum_bytes - read_count) {
      if (error) *error = "sha256_input_size_exceeds_limit";
      return false;
    }
    data.insert(data.end(), buffer.begin(), buffer.begin() + count);
  }
  if (input.bad()) { if (error) *error = "sha256_input_read_failed"; return false; }
  return sha256_bytes(data, digest);
}

bool sha256_trusted_regular_file_bounded(
    const std::filesystem::path& trusted_root,
    const std::filesystem::path& path,
    std::uint64_t maximum_bytes,
    std::string* digest,
    std::string* error) {
#ifdef _WIN32
  return sha256_windows_trusted_handle(trusted_root, path, maximum_bytes,
                                       digest, error);
#else
  return sha256_posix_trusted_handle(trusted_root, path, maximum_bytes,
                                     digest, error);
#endif
}
}  // namespace prisminfer

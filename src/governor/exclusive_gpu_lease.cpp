#include "prisminfer/exclusive_gpu_lease.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#ifdef __linux__
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <unistd.h>
#endif

namespace prisminfer {
namespace {

std::mutex g_registry_mutex;
std::set<std::string> g_active_lease_ids;

#ifdef _WIN32
struct WindowsLeaseState {
  std::mutex mutex;
  std::condition_variable condition;
  std::thread owner;
  bool ready{false};
  bool stop{false};
  ExclusiveGpuLeaseStatus status{ExclusiveGpuLeaseStatus::SystemError};
  std::uint32_t system_error{0};
};
#endif

std::string make_lease_id(std::int32_t high, std::uint32_t low) {
  char buffer[64]{};
  std::snprintf(buffer, sizeof(buffer), "prisminfer-gpu-%08x-%08x",
                static_cast<std::uint32_t>(high), low);
  return buffer;
}

#ifndef _WIN32
void* encode_fd(int fd) {
  return reinterpret_cast<void*>(static_cast<std::intptr_t>(fd + 1));
}

int decode_fd(void* handle) {
  return static_cast<int>(reinterpret_cast<std::intptr_t>(handle)) - 1;
}
#endif

}  // namespace

ExclusiveGpuLease::ExclusiveGpuLease(std::int32_t adapter_luid_high,
                                     std::uint32_t adapter_luid_low,
                                     std::string lease_id,
                                     void* native_handle)
    : adapter_luid_high_(adapter_luid_high),
      adapter_luid_low_(adapter_luid_low),
      lease_id_(std::move(lease_id)),
      native_handle_(native_handle) {}

ExclusiveGpuLease::ExclusiveGpuLease(ExclusiveGpuLease&& other) noexcept {
  *this = std::move(other);
}

ExclusiveGpuLease& ExclusiveGpuLease::operator=(
    ExclusiveGpuLease&& other) noexcept {
  if (this == &other) return *this;
  release();
  adapter_luid_high_ = other.adapter_luid_high_;
  adapter_luid_low_ = other.adapter_luid_low_;
  lease_id_ = std::move(other.lease_id_);
  native_handle_ = other.native_handle_;
  other.adapter_luid_high_ = 0;
  other.adapter_luid_low_ = 0;
  other.native_handle_ = nullptr;
  return *this;
}

ExclusiveGpuLease::~ExclusiveGpuLease() { release(); }

bool ExclusiveGpuLease::active() const { return native_handle_ != nullptr; }

std::int32_t ExclusiveGpuLease::adapter_luid_high() const {
  return adapter_luid_high_;
}

std::uint32_t ExclusiveGpuLease::adapter_luid_low() const {
  return adapter_luid_low_;
}

const std::string& ExclusiveGpuLease::lease_id() const { return lease_id_; }

void ExclusiveGpuLease::quarantine_for_process_lifetime() noexcept {
  // Intentionally retain the OS authority and in-process registry entry. The
  // OS releases it at supervisor exit; this adapter cannot be reused after an
  // unreconciled terminal path in the current supervisor process.
  native_handle_ = nullptr;
}

void ExclusiveGpuLease::release() {
  if (!native_handle_) return;
#ifdef _WIN32
  auto* state = static_cast<WindowsLeaseState*>(native_handle_);
  {
    std::lock_guard lock(state->mutex);
    state->stop = true;
  }
  state->condition.notify_one();
  state->owner.join();
  delete state;
#else
  const int fd = decode_fd(native_handle_);
  close(fd);
#endif
  {
    std::lock_guard lock(g_registry_mutex);
    g_active_lease_ids.erase(lease_id_);
  }
  native_handle_ = nullptr;
}

ExclusiveGpuLeaseAcquireResult acquire_exclusive_gpu_lease(
    std::int32_t adapter_luid_high, std::uint32_t adapter_luid_low) {
  ExclusiveGpuLeaseAcquireResult result;
  if (adapter_luid_high == 0 && adapter_luid_low == 0) {
    result.status = ExclusiveGpuLeaseStatus::InvalidAdapterIdentity;
    return result;
  }
  std::string lease_id;
  try {
    lease_id = make_lease_id(adapter_luid_high, adapter_luid_low);
  } catch (const std::bad_alloc&) {
#ifdef _WIN32
    result.system_error = ERROR_NOT_ENOUGH_MEMORY;
#else
    result.system_error = static_cast<std::uint32_t>(ENOMEM);
#endif
    return result;
  }
  std::lock_guard registry_lock(g_registry_mutex);
  if (g_active_lease_ids.contains(lease_id)) {
    result.status = ExclusiveGpuLeaseStatus::AlreadyHeldInProcess;
    return result;
  }
  try {
    g_active_lease_ids.insert(lease_id);
  } catch (const std::bad_alloc&) {
#ifdef _WIN32
    result.system_error = ERROR_NOT_ENOUGH_MEMORY;
#else
    result.system_error = static_cast<std::uint32_t>(ENOMEM);
#endif
    return result;
  }

#ifdef _WIN32
  std::string object_name;
  std::unique_ptr<WindowsLeaseState> state;
  try {
    object_name = "Global\\PrismInfer." + lease_id;
    state = std::make_unique<WindowsLeaseState>();
    auto* state_pointer = state.get();
    state->owner = std::thread([state_pointer, object_name] {
    const auto handle = CreateMutexA(nullptr, FALSE, object_name.c_str());
    if (!handle) {
      std::lock_guard lock(state_pointer->mutex);
      state_pointer->system_error = GetLastError();
      state_pointer->ready = true;
      state_pointer->condition.notify_one();
      return;
    }
    const auto wait = WaitForSingleObject(handle, 0);
    {
      std::lock_guard lock(state_pointer->mutex);
      if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED) {
        state_pointer->status = ExclusiveGpuLeaseStatus::Acquired;
      } else if (wait == WAIT_TIMEOUT) {
        state_pointer->status = ExclusiveGpuLeaseStatus::HeldByAnotherProcess;
      } else {
        state_pointer->system_error = GetLastError();
      }
      state_pointer->ready = true;
    }
    state_pointer->condition.notify_one();
    if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED) {
      std::unique_lock lock(state_pointer->mutex);
      state_pointer->condition.wait(
          lock, [state_pointer] { return state_pointer->stop; });
      lock.unlock();
      ReleaseMutex(handle);
    }
    CloseHandle(handle);
    });
  } catch (const std::bad_alloc&) {
    g_active_lease_ids.erase(lease_id);
    result.system_error = ERROR_NOT_ENOUGH_MEMORY;
    return result;
  } catch (const std::system_error& error) {
    g_active_lease_ids.erase(lease_id);
    result.system_error = static_cast<std::uint32_t>(error.code().value());
    return result;
  }
  {
    std::unique_lock lock(state->mutex);
    state->condition.wait(lock, [pointer = state.get()] {
      return pointer->ready;
    });
    result.status = state->status;
    result.system_error = state->system_error;
  }
  if (result.status != ExclusiveGpuLeaseStatus::Acquired) {
    state->owner.join();
    g_active_lease_ids.erase(lease_id);
    return result;
  }
  void* native_handle = state.release();
#else
#ifdef __linux__
  const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    result.system_error = static_cast<std::uint32_t>(errno);
    g_active_lease_ids.erase(lease_id);
    return result;
  }
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  if (lease_id.size() + 1 > sizeof(address.sun_path)) {
    result.system_error = static_cast<std::uint32_t>(ENAMETOOLONG);
    g_active_lease_ids.erase(lease_id);
    close(fd);
    return result;
  }
  address.sun_path[0] = '\0';
  std::copy(lease_id.begin(), lease_id.end(), address.sun_path + 1);
  const auto address_length = static_cast<socklen_t>(
      offsetof(sockaddr_un, sun_path) + 1 + lease_id.size());
  if (bind(fd, reinterpret_cast<const sockaddr*>(&address), address_length) !=
      0) {
    result.system_error = static_cast<std::uint32_t>(errno);
    result.status = errno == EADDRINUSE
                        ? ExclusiveGpuLeaseStatus::HeldByAnotherProcess
                        : ExclusiveGpuLeaseStatus::SystemError;
    g_active_lease_ids.erase(lease_id);
    close(fd);
    return result;
  }
#else
  result.system_error = static_cast<std::uint32_t>(ENOTSUP);
  g_active_lease_ids.erase(lease_id);
  return result;
#endif
  void* native_handle = encode_fd(fd);
#endif

  result.lease = ExclusiveGpuLease(adapter_luid_high, adapter_luid_low,
                                   std::move(lease_id), native_handle);
  result.status = ExclusiveGpuLeaseStatus::Acquired;
  return result;
}

}  // namespace prisminfer

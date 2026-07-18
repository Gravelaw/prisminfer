#include <cuda_runtime_api.h>
#include <windows.h>

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace {

constexpr std::uint64_t kMaximumPayloadBytes = 64ULL * 1024ULL * 1024ULL;

template <typename Integer>
bool parse_integer(const char* text, Integer* value) {
  if (text == nullptr || *text == '\0') return false;
  const auto* end = text + std::strlen(text);
  const auto parsed = std::from_chars(text, end, *value);
  return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool write_protocol(HANDLE handle, const std::string& message) {
  if (message.size() > MAXDWORD) return false;
  DWORD written = 0U;
  return WriteFile(handle, message.data(), static_cast<DWORD>(message.size()),
                   &written, nullptr) &&
         written == message.size();
}

bool read_nonce(std::string* nonce) {
  char value[64]{};
  const auto length = GetEnvironmentVariableA(
      "PRISMINFER_PROTOCOL_NONCE", value, static_cast<DWORD>(sizeof(value)));
  if (length != 32U) return false;
  *nonce = value;
  return true;
}

bool read_protocol_handle(HANDLE* handle) {
  char text[32]{};
  if (GetEnvironmentVariableA("PRISMINFER_PROTOCOL_OUT_HANDLE", text,
                              static_cast<DWORD>(sizeof(text))) == 0U) {
    return false;
  }
  std::uint64_t raw = 0U;
  if (!parse_integer(text, &raw) || raw == 0U) return false;
  *handle = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(raw));
  return true;
}

bool acknowledge_cancel(HANDLE output, const std::string& nonce,
                        const std::string& line) {
  std::istringstream parsed(line);
  std::string version;
  std::string type;
  std::string returned_nonce;
  std::string reason;
  std::string extra;
  if (!(parsed >> version >> type >> returned_nonce >> reason) ||
      parsed >> extra || version != "PRISMINFER/1" || type != "CANCEL" ||
      returned_nonce != nonce || reason.empty()) {
    return false;
  }
  return write_protocol(output,
                        "PRISMINFER/1 CANCEL_ACK " + nonce + "\n");
}

bool cleanup_payload(void* payload) {
  if (payload != nullptr && cudaFree(payload) != cudaSuccess) return false;
  return cudaDeviceSynchronize() == cudaSuccess;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 11 || std::string(argv[1]) != "--case" ||
      std::string(argv[3]) != "--device-index" ||
      std::string(argv[5]) != "--luid-high" ||
      std::string(argv[7]) != "--luid-low" ||
      std::string(argv[9]) != "--payload-bytes") {
    return 2;
  }
  const std::string case_name = argv[2];
  if (case_name != "success" &&
      case_name != "post-context-telemetry-loss" &&
      case_name != "heartbeat-loss" && case_name != "watchdog-cancel") {
    return 3;
  }
  int device_index = -1;
  std::int32_t expected_luid_high = 0;
  std::uint32_t expected_luid_low = 0U;
  std::uint64_t payload_bytes = 0U;
  if (!parse_integer(argv[4], &device_index) || device_index < 0 ||
      !parse_integer(argv[6], &expected_luid_high) ||
      !parse_integer(argv[8], &expected_luid_low) ||
      !parse_integer(argv[10], &payload_bytes)) {
    return 4;
  }
  if (payload_bytes == 0U || payload_bytes > kMaximumPayloadBytes) {
    return 5;
  }

  std::string nonce;
  HANDLE protocol_output = nullptr;
  if (!read_nonce(&nonce) || !read_protocol_handle(&protocol_output)) return 6;

  if (cudaSetDevice(device_index) != cudaSuccess) return 10;
  char cuda_luid[8]{};
  unsigned int node_mask = 0U;
  if (cudaDeviceGetLuid(cuda_luid, &node_mask, device_index) != cudaSuccess) {
    return 11;
  }
  std::uint32_t actual_luid_low = 0U;
  std::int32_t actual_luid_high = 0;
  std::memcpy(&actual_luid_low, cuda_luid, sizeof(actual_luid_low));
  std::memcpy(&actual_luid_high, cuda_luid + sizeof(actual_luid_low),
              sizeof(actual_luid_high));
  if (actual_luid_high != expected_luid_high ||
      actual_luid_low != expected_luid_low) {
    return 12;
  }
  if (cudaFree(nullptr) != cudaSuccess) return 13;
  std::size_t context_free = 0U;
  std::size_t context_total = 0U;
  if (cudaMemGetInfo(&context_free, &context_total) != cudaSuccess ||
      context_total == 0U || context_free > context_total) {
    return 14;
  }
  const std::string context =
      "PRISMINFER/1 CONTEXT_READY " + nonce + " " +
      std::to_string(actual_luid_high) + " " +
      std::to_string(actual_luid_low) + " " +
      std::to_string(context_free) + " " + std::to_string(context_total) +
      "\n";
  if (!write_protocol(protocol_output, context)) return 15;

  std::string command;
  if (!std::getline(std::cin, command)) return 16;
  if (command.find("PRISMINFER/1 CANCEL ") == 0U) {
    return acknowledge_cancel(protocol_output, nonce, command) ? 0 : 17;
  }
  std::istringstream admitted(command);
  std::string version;
  std::string type;
  std::string returned_nonce;
  std::string extra;
  std::uint64_t token_id = 0U;
  std::uint64_t effective_cap = 0U;
  if (!(admitted >> version >> type >> returned_nonce >> token_id >>
        effective_cap) ||
      admitted >> extra || version != "PRISMINFER/1" || type != "ADMIT" ||
      returned_nonce != nonce || token_id == 0U ||
      effective_cap < payload_bytes) {
    return 18;
  }
  if (!write_protocol(protocol_output,
                      "PRISMINFER/1 TOKEN_CONSUMED " + nonce + " " +
                          std::to_string(token_id) + "\n")) {
    return 19;
  }

  void* payload = nullptr;
  if (cudaMalloc(&payload, static_cast<std::size_t>(payload_bytes)) !=
      cudaSuccess) {
    return 20;
  }
  if (cudaMemset(payload, 0xA5, static_cast<std::size_t>(payload_bytes)) !=
          cudaSuccess ||
      cudaGetLastError() != cudaSuccess ||
      cudaDeviceSynchronize() != cudaSuccess) {
    (void)cleanup_payload(payload);
    return 21;
  }
  if (case_name == "heartbeat-loss") {
    if (!std::getline(std::cin, command)) {
      (void)cleanup_payload(payload);
      return 22;
    }
    const bool acknowledged = acknowledge_cancel(protocol_output, nonce, command);
    return cleanup_payload(payload) && acknowledged ? 0 : 23;
  }

  std::size_t active_free = 0U;
  std::size_t active_total = 0U;
  if (cudaMemGetInfo(&active_free, &active_total) != cudaSuccess ||
      active_total == 0U || active_free > active_total) {
    (void)cleanup_payload(payload);
    return 24;
  }
  const auto heartbeat = [&](std::uint64_t sequence) {
    return write_protocol(
        protocol_output,
        "PRISMINFER/1 HEARTBEAT " + nonce + " " +
            std::to_string(sequence) + " " + std::to_string(payload_bytes) +
            " " + std::to_string(active_free) + " " +
            std::to_string(active_total) + "\n");
  };
  if (!heartbeat(1U)) {
    (void)cleanup_payload(payload);
    return 25;
  }
  if (case_name == "watchdog-cancel") {
    if (!std::getline(std::cin, command)) {
      (void)cleanup_payload(payload);
      return 26;
    }
    const bool acknowledged = acknowledge_cancel(protocol_output, nonce, command);
    return cleanup_payload(payload) && acknowledged ? 0 : 27;
  }
  Sleep(100U);
  if (!heartbeat(2U)) {
    (void)cleanup_payload(payload);
    return 28;
  }
  return cleanup_payload(payload) ? 0 : 29;
}

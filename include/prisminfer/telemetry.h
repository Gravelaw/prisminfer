#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include "prisminfer/config.h"
#include "prisminfer/memory_cap.h"

namespace prisminfer {

std::uint64_t monotonic_time_ns();
std::string json_escape(const std::string& value);
std::string utc_timestamp();

class JsonlTelemetry {
 public:
  explicit JsonlTelemetry(const std::filesystem::path& path);

  bool ok() const;
  const std::string& error() const;

  void emit(const std::string& event,
            const RuntimeConfig& config,
            const std::map<std::string, std::string>& fields = {});

  void emit_memory_sample(const RuntimeConfig& config,
                          const MemorySample& sample,
                          bool telemetry_agreement);

 private:
  std::ofstream out_;
  std::string error_;
};

}  // namespace prisminfer

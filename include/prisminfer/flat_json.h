#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace prisminfer {

struct FlatJsonValue {
  enum class Kind { String, Number, Boolean };
  std::string text;
  Kind kind{Kind::String};
};

struct FlatJsonResult {
  bool ok{false};
  std::map<std::string, FlatJsonValue> fields;
  std::string error;
};

FlatJsonResult read_flat_json_file(const std::filesystem::path& path,
                                   std::uint64_t max_bytes);

}  // namespace prisminfer

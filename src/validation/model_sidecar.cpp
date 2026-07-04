#include "prisminfer/model_sidecar.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace prisminfer {

namespace {

std::filesystem::path normalize_path(const std::filesystem::path& path) {
  std::error_code error;
  const auto absolute = std::filesystem::absolute(path, error);
  if (error) {
    return path.lexically_normal();
  }
  return absolute.lexically_normal();
}

bool is_hex_sha256(const std::string& value) {
  return value.size() == 64 &&
         std::all_of(value.begin(), value.end(), [](unsigned char ch) {
           return std::isxdigit(ch) != 0;
         });
}

bool read_file(const std::filesystem::path& path,
               std::uint64_t max_bytes,
               std::string* content,
               std::string* failure_reason) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error) {
    *failure_reason = "sidecar_size_unavailable";
    return false;
  }
  if (size > max_bytes) {
    *failure_reason = "sidecar_size_exceeds_limit";
    return false;
  }

  std::ifstream in(path, std::ios::in | std::ios::binary);
  if (!in) {
    *failure_reason = "sidecar_open_failed";
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  *content = buffer.str();
  return true;
}

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

bool parse_flat_string_object(const std::string& json,
                              std::map<std::string, std::string>* fields) {
  std::size_t position = 0;
  skip_ws(json, &position);
  if (position >= json.size() || json[position] != '{') {
    return false;
  }
  ++position;

  skip_ws(json, &position);
  if (position < json.size() && json[position] == '}') {
    ++position;
    skip_ws(json, &position);
    return position == json.size();
  }

  while (position < json.size()) {
    std::string key;
    std::string value;
    if (!parse_json_string(json, &position, &key)) {
      return false;
    }
    skip_ws(json, &position);
    if (position >= json.size() || json[position] != ':') {
      return false;
    }
    ++position;
    if (!parse_json_string(json, &position, &value)) {
      return false;
    }
    (*fields)[key] = value;

    skip_ws(json, &position);
    if (position >= json.size()) {
      return false;
    }
    if (json[position] == '}') {
      ++position;
      skip_ws(json, &position);
      return position == json.size();
    }
    if (json[position] != ',') {
      return false;
    }
    ++position;
  }
  return false;
}

}  // namespace

ModelSidecarValidationResult validate_model_sidecar(
    const ModelSidecarValidationRequest& request) {
  ModelSidecarValidationResult result;
  result.skipped = request.model_path.empty() && request.sidecar_path.empty();
  if (result.skipped) {
    return result;
  }

  if (!request.model_path.empty()) {
    result.normalized_model_path = normalize_path(request.model_path);
  }
  if (!request.sidecar_path.empty()) {
    result.normalized_sidecar_path = normalize_path(request.sidecar_path);
  }

  std::error_code error;
  if (!request.model_path.empty()) {
    if (!std::filesystem::is_regular_file(result.normalized_model_path,
                                          error)) {
      result.valid = false;
      result.failure_reason = "model_file_missing";
      return result;
    }
    result.model_size_bytes =
        std::filesystem::file_size(result.normalized_model_path, error);
    if (error) {
      result.valid = false;
      result.failure_reason = "model_size_unavailable";
      return result;
    }
    if (result.model_size_bytes > request.max_model_bytes) {
      result.valid = false;
      result.failure_reason = "model_size_exceeds_limit";
      return result;
    }
  }

  if (request.sidecar_path.empty()) {
    result.valid = false;
    result.failure_reason = "sidecar_path_missing";
    return result;
  }
  if (!std::filesystem::is_regular_file(result.normalized_sidecar_path,
                                        error)) {
    result.valid = false;
    result.failure_reason = "sidecar_file_missing";
    return result;
  }
  result.sidecar_size_bytes =
      std::filesystem::file_size(result.normalized_sidecar_path, error);
  if (error) {
    result.valid = false;
    result.failure_reason = "sidecar_size_unavailable";
    return result;
  }

  std::string content;
  if (!read_file(result.normalized_sidecar_path, request.max_sidecar_bytes,
                 &content, &result.failure_reason)) {
    result.valid = false;
    return result;
  }

  std::map<std::string, std::string> fields;
  if (!parse_flat_string_object(content, &fields)) {
    result.valid = false;
    result.failure_reason = "sidecar_json_malformed";
    return result;
  }

  const std::set<std::string> allowed_keys{
      "schema_version", "model_sha256", "notes"};
  for (const auto& [key, value] : fields) {
    (void)value;
    if (allowed_keys.find(key) == allowed_keys.end()) {
      result.valid = false;
      result.failure_reason = "sidecar_unknown_field";
      return result;
    }
  }

  const auto schema = fields.find("schema_version");
  if (schema == fields.end() || schema->second != "0.1") {
    result.valid = false;
    result.failure_reason = "sidecar_schema_version_invalid";
    return result;
  }

  const auto hash = fields.find("model_sha256");
  if (hash == fields.end() || !is_hex_sha256(hash->second)) {
    result.valid = false;
    result.failure_reason = "sidecar_model_sha256_invalid";
    return result;
  }
  result.model_sha256 = hash->second;
  return result;
}

}  // namespace prisminfer

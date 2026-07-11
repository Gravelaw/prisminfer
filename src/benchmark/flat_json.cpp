#include "prisminfer/flat_json.h"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>

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
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (error || !std::filesystem::is_regular_file(status) ||
      std::filesystem::is_symlink(status)) {
    return fail("manifest_not_regular_file");
  }

  std::ifstream in(path, std::ios::in | std::ios::binary);
  if (!in) {
    return fail("manifest_open_failed");
  }

  std::string content;
  content.reserve(static_cast<std::size_t>(max_bytes));
  char chunk[4096];
  while (in) {
    in.read(chunk, static_cast<std::streamsize>(sizeof(chunk)));
    const auto count = in.gcount();
    if (count <= 0) {
      break;
    }
    const auto bytes = static_cast<std::uint64_t>(count);
    if (bytes > max_bytes || content.size() > max_bytes - bytes) {
      return fail("manifest_size_exceeds_limit");
    }
    content.append(chunk, static_cast<std::size_t>(count));
  }
  if (!in.eof()) {
    return fail("manifest_read_failed");
  }
  return parse_flat_json_object(content);
}

}  // namespace prisminfer

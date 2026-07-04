#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "prisminfer/model_sidecar.h"

namespace {

int expect(bool condition, const char* message) {
  if (condition) {
    return 0;
  }
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

void write_text(const std::filesystem::path& path, const std::string& value) {
  std::ofstream out(path, std::ios::out | std::ios::trunc);
  out << value;
}

}  // namespace

int main() {
  const auto root =
      std::filesystem::temp_directory_path() / "prisminfer_sidecar_test";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);

  const auto model = root / "tiny.gguf";
  const auto sidecar = root / "tiny.gguf.prism.json";
  const std::string hash(64, 'a');
  write_text(model, "model-bytes");
  write_text(sidecar,
             "{\"schema_version\":\"0.1\",\"model_sha256\":\"" + hash +
                 "\",\"notes\":\"fixture\"}");

  {
    prisminfer::ModelSidecarValidationRequest request;
    const auto result = prisminfer::validate_model_sidecar(request);
    if (expect(result.skipped, "empty request skips validation")) return 1;
    if (expect(result.valid, "empty request is valid")) return 1;
  }
  {
    prisminfer::ModelSidecarValidationRequest request;
    request.model_path = model;
    request.sidecar_path = sidecar;
    const auto result = prisminfer::validate_model_sidecar(request);
    if (expect(!result.skipped, "explicit request is not skipped")) return 1;
    if (expect(result.valid, result.failure_reason.c_str())) return 1;
    if (expect(result.model_sha256 == hash, "model hash is logged")) return 1;
    if (expect(result.normalized_model_path.is_absolute(),
               "model path is normalized")) return 1;
    if (expect(result.model_size_bytes == 11, "model size is recorded")) {
      return 1;
    }
  }
  {
    const auto malformed = root / "malformed.prism.json";
    write_text(malformed, "{\"schema_version\":\"0.1\"");
    prisminfer::ModelSidecarValidationRequest request;
    request.sidecar_path = malformed;
    const auto result = prisminfer::validate_model_sidecar(request);
    if (expect(!result.valid, "malformed sidecar is rejected")) return 1;
    if (expect(result.failure_reason == "sidecar_json_malformed",
               "malformed reason is specific")) return 1;
  }
  {
    const auto unknown = root / "unknown.prism.json";
    write_text(unknown,
               "{\"schema_version\":\"0.1\",\"model_sha256\":\"" + hash +
                   "\",\"extra\":\"nope\"}");
    prisminfer::ModelSidecarValidationRequest request;
    request.sidecar_path = unknown;
    const auto result = prisminfer::validate_model_sidecar(request);
    if (expect(!result.valid, "unknown sidecar fields are rejected")) return 1;
    if (expect(result.failure_reason == "sidecar_unknown_field",
               "unknown field reason is specific")) return 1;
  }
  {
    prisminfer::ModelSidecarValidationRequest request;
    request.model_path = model;
    request.sidecar_path = sidecar;
    request.max_model_bytes = 1;
    const auto result = prisminfer::validate_model_sidecar(request);
    if (expect(!result.valid, "oversized model is rejected")) return 1;
    if (expect(result.failure_reason == "model_size_exceeds_limit",
               "oversized model reason is specific")) return 1;
  }

  std::filesystem::remove_all(root, error);
  return 0;
}

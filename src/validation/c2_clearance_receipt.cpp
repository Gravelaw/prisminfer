#include "prisminfer/c2_clearance_receipt.h"

#include <array>
#include <charconv>
#include <map>
#include <set>
#include <sstream>

#include "prisminfer/atomic_file.h"
#include "prisminfer/flat_json.h"

namespace prisminfer {
namespace {

bool lower_hex(const std::string& value, std::size_t length) {
  if (value.size() != length) return false;
  for (const char ch : value) {
    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
  }
  return true;
}

bool bounded_text(const std::string& value, std::size_t maximum,
                  bool allow_empty = false) {
  if ((!allow_empty && value.empty()) || value.size() > maximum) return false;
  for (const unsigned char ch : value) {
    if (ch < 0x20U) return false;
  }
  return true;
}

bool authorization_identifier(const std::string& value) {
  if (value.empty() || value.size() > 128U) return false;
  for (const char ch : value) {
    if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
          (ch >= '0' && ch <= '9') || ch == '_' || ch == '.' || ch == ':' ||
          ch == '-')) {
      return false;
    }
  }
  return true;
}

std::string escaped(const std::string& value) {
  std::string output;
  output.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
      case '\\': output += "\\\\"; break;
      case '"': output += "\\\""; break;
      case '\n': output += "\\n"; break;
      case '\r': output += "\\r"; break;
      case '\t': output += "\\t"; break;
      default: output.push_back(ch); break;
    }
  }
  return output;
}

template <typename Integer>
bool parse_integer(const FlatJsonValue& value, Integer* output) {
  if (value.kind != FlatJsonValue::Kind::Number || value.text.empty()) {
    return false;
  }
  const auto* begin = value.text.data();
  const auto* end = begin + value.text.size();
  const auto parsed = std::from_chars(begin, end, *output);
  return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool require_string(const FlatJsonResult& json, const char* key,
                    std::string* value) {
  const auto found = json.fields.find(key);
  if (found == json.fields.end() ||
      found->second.kind != FlatJsonValue::Kind::String) return false;
  *value = found->second.text;
  return true;
}

bool require_bool(const FlatJsonResult& json, const char* key, bool expected) {
  const auto found = json.fields.find(key);
  return found != json.fields.end() &&
         found->second.kind == FlatJsonValue::Kind::Boolean &&
         found->second.text == (expected ? "true" : "false");
}

}  // namespace

std::string serialize_c2_clearance_receipt(const C2ClearanceReceipt& receipt) {
  std::ostringstream out;
  out << "{\n"
      << "  \"schema_version\":\"prisminfer-c2-clearance-receipt-v1\",\n"
      << "  \"receipt_class\":\"c2-clearance-candidate\",\n"
      << "  \"repository\":\"" << escaped(receipt.repository) << "\",\n"
      << "  \"reviewed_sha\":\"" << receipt.reviewed_sha << "\",\n"
      << "  \"source_tree_sha\":\"" << receipt.source_tree_sha << "\",\n"
      << "  \"worker_sha256\":\"" << receipt.worker_sha256 << "\",\n"
      << "  \"worker_approval_identity\":\""
      << escaped(receipt.worker_approval_identity) << "\",\n"
      << "  \"workflow_run_id\":\"" << escaped(receipt.workflow_run_id)
      << "\",\n"
      << "  \"authorization_id\":\"" << escaped(receipt.authorization_id)
      << "\",\n"
      << "  \"case_name\":\"" << escaped(receipt.case_name) << "\",\n"
      << "  \"status\":\"" << escaped(receipt.status) << "\",\n"
      << "  \"failure_reason\":\"" << escaped(receipt.failure_reason)
      << "\",\n"
      << "  \"cleanup_status\":\"" << escaped(receipt.cleanup_status)
      << "\",\n"
      << "  \"review_status\":\"pending-independent-review\",\n"
      << "  \"claim_scope\":\"synthetic-c2-candidate-non-promotable\",\n"
      << "  \"promotable\":false,\n"
      << "  \"c2_credit\":false,\n"
      << "  \"lease_id\":\"" << escaped(receipt.lease_id) << "\",\n"
      << "  \"job_identity\":\"" << escaped(receipt.job_identity) << "\",\n"
      << "  \"last_good_sample_sha256\":\""
      << receipt.last_good_sample_sha256 << "\",\n"
      << "  \"evidence_bundle_sha256\":\""
      << receipt.evidence_bundle_sha256 << "\",\n"
      << "  \"adapter_luid_high\":" << receipt.adapter_luid_high << ",\n"
      << "  \"adapter_luid_low\":" << receipt.adapter_luid_low << ",\n"
      << "  \"adapter_index\":" << receipt.adapter_index << ",\n"
      << "  \"worker_timeout_milliseconds\":"
      << receipt.worker_timeout_milliseconds << ",\n"
      << "  \"post_admission_payload_bytes\":"
      << receipt.post_admission_payload_bytes << ",\n"
      << "  \"maximum_payload_bytes\":" << receipt.maximum_payload_bytes
      << ",\n"
      << "  \"context_cuda_free_bytes\":"
      << receipt.context_cuda_free_bytes << ",\n"
      << "  \"context_cuda_total_bytes\":"
      << receipt.context_cuda_total_bytes << ",\n"
      << "  \"last_heartbeat_cuda_free_bytes\":"
      << receipt.last_heartbeat_cuda_free_bytes
      << ",\n"
      << "  \"last_heartbeat_cuda_total_bytes\":"
      << receipt.last_heartbeat_cuda_total_bytes
      << ",\n"
      << "  \"pre_wddm_local_usage_bytes\":"
      << receipt.pre_wddm_local_usage_bytes << ",\n"
      << "  \"final_wddm_local_usage_bytes\":"
      << receipt.final_wddm_local_usage_bytes << ",\n"
      << "  \"pre_host_memory_available_bytes\":"
      << receipt.pre_host_memory_available_bytes << ",\n"
      << "  \"final_host_memory_available_bytes\":"
      << receipt.final_host_memory_available_bytes << ",\n"
      << "  \"pre_host_commit_available_bytes\":"
      << receipt.pre_host_commit_available_bytes << ",\n"
      << "  \"final_host_commit_available_bytes\":"
      << receipt.final_host_commit_available_bytes << ",\n"
      << "  \"pre_gpu_temperature_celsius\":"
      << receipt.pre_gpu_temperature_celsius << ",\n"
      << "  \"final_gpu_temperature_celsius\":"
      << receipt.final_gpu_temperature_celsius
      << ",\n"
      << "  \"root_process_id\":" << receipt.root_process_id << ",\n"
      << "  \"heartbeat_count\":" << receipt.heartbeat_count << ",\n"
      << "  \"context_ready_observed\":"
      << (receipt.context_ready_observed ? "true" : "false") << ",\n"
      << "  \"token_consumed_observed\":"
      << (receipt.token_consumed_observed ? "true" : "false") << ",\n"
      << "  \"worker_exit_observed\":"
      << (receipt.worker_exit_observed ? "true" : "false") << ",\n"
      << "  \"job_tree_empty\":"
      << (receipt.job_tree_empty ? "true" : "false") << ",\n"
      << "  \"job_accounting_reconciled\":"
      << (receipt.job_accounting_reconciled ? "true" : "false") << ",\n"
      << "  \"device_resources_reconciled\":"
      << (receipt.device_resources_reconciled ? "true" : "false") << ",\n"
      << "  \"artifact_handles_closed\":"
      << (receipt.artifact_handles_closed ? "true" : "false") << ",\n"
      << "  \"temporary_files_reconciled\":"
      << (receipt.temporary_files_reconciled ? "true" : "false") << "\n"
      << "}\n";
  return out.str();
}

bool validate_c2_clearance_receipt_file(const std::filesystem::path& path,
                                        std::string* error) {
  const auto json = read_flat_json_file(path, kC2ReceiptMaximumBytes);
  if (!json.ok) {
    if (error) *error = json.error;
    return false;
  }
  static const std::set<std::string> required = {
      "schema_version", "receipt_class", "repository", "reviewed_sha",
      "source_tree_sha", "worker_sha256", "worker_approval_identity",
      "workflow_run_id", "authorization_id", "case_name", "status", "failure_reason",
      "cleanup_status", "review_status", "claim_scope", "promotable",
      "c2_credit", "lease_id", "job_identity", "last_good_sample_sha256",
      "evidence_bundle_sha256", "adapter_luid_high", "adapter_luid_low",
      "adapter_index", "worker_timeout_milliseconds",
      "post_admission_payload_bytes", "maximum_payload_bytes",
      "context_cuda_free_bytes", "context_cuda_total_bytes",
      "last_heartbeat_cuda_free_bytes",
      "last_heartbeat_cuda_total_bytes", "pre_wddm_local_usage_bytes",
      "final_wddm_local_usage_bytes", "pre_host_memory_available_bytes",
      "final_host_memory_available_bytes", "pre_host_commit_available_bytes",
      "final_host_commit_available_bytes", "pre_gpu_temperature_celsius",
      "final_gpu_temperature_celsius", "root_process_id",
      "heartbeat_count", "context_ready_observed", "token_consumed_observed",
      "worker_exit_observed", "job_tree_empty",
      "job_accounting_reconciled", "device_resources_reconciled",
      "artifact_handles_closed", "temporary_files_reconciled"};
  if (json.fields.size() != required.size()) {
    if (error) *error = "c2_receipt_field_set_invalid";
    return false;
  }
  for (const auto& [key, value] : json.fields) {
    (void)value;
    if (!required.contains(key)) {
      if (error) *error = "c2_receipt_unknown_field:" + key;
      return false;
    }
  }
  std::string schema;
  std::string receipt_class;
  std::string repository;
  std::string reviewed_sha;
  std::string tree_sha;
  std::string worker_sha;
  std::string approval;
  std::string run_id;
  std::string authorization_id;
  std::string case_name;
  std::string status;
  std::string failure_reason;
  std::string cleanup_status;
  std::string review_status;
  std::string claim_scope;
  std::string lease_id;
  std::string job_identity;
  std::string last_good;
  std::string bundle;
  const bool strings_ok =
      require_string(json, "schema_version", &schema) &&
      require_string(json, "receipt_class", &receipt_class) &&
      require_string(json, "repository", &repository) &&
      require_string(json, "reviewed_sha", &reviewed_sha) &&
      require_string(json, "source_tree_sha", &tree_sha) &&
      require_string(json, "worker_sha256", &worker_sha) &&
      require_string(json, "worker_approval_identity", &approval) &&
      require_string(json, "workflow_run_id", &run_id) &&
      require_string(json, "authorization_id", &authorization_id) &&
      require_string(json, "case_name", &case_name) &&
      require_string(json, "status", &status) &&
      require_string(json, "failure_reason", &failure_reason) &&
      require_string(json, "cleanup_status", &cleanup_status) &&
      require_string(json, "review_status", &review_status) &&
      require_string(json, "claim_scope", &claim_scope) &&
      require_string(json, "lease_id", &lease_id) &&
      require_string(json, "job_identity", &job_identity) &&
      require_string(json, "last_good_sample_sha256", &last_good) &&
      require_string(json, "evidence_bundle_sha256", &bundle);
  if (!strings_ok || schema != "prisminfer-c2-clearance-receipt-v1" ||
      receipt_class != "c2-clearance-candidate" ||
      review_status != "pending-independent-review" ||
      claim_scope != "synthetic-c2-candidate-non-promotable" ||
      !bounded_text(repository, 128U) || !lower_hex(reviewed_sha, 40U) ||
      !lower_hex(tree_sha, 40U) || !lower_hex(worker_sha, 64U) ||
      !bounded_text(approval, 128U) || !bounded_text(run_id, 128U) ||
      !authorization_identifier(authorization_id) ||
      !bounded_text(case_name, 64U) ||
      (status != "candidate-complete" && status != "rejected" &&
       status != "aborted") ||
      !bounded_text(failure_reason, 256U, true) ||
      (cleanup_status != "cleaned" && cleanup_status != "quarantined") ||
      !bounded_text(lease_id, 128U) || !bounded_text(job_identity, 256U) ||
      !lower_hex(last_good, 64U) || !lower_hex(bundle, 64U) ||
      !require_bool(json, "promotable", false) ||
      !require_bool(json, "c2_credit", false)) {
    if (error) *error = "c2_receipt_identity_or_classification_invalid";
    return false;
  }
  std::uint64_t payload = 0U;
  std::uint64_t maximum_payload = 0U;
  std::uint32_t timeout = 0U;
  const auto payload_field = json.fields.find("post_admission_payload_bytes");
  const auto maximum_field = json.fields.find("maximum_payload_bytes");
  const auto timeout_field = json.fields.find("worker_timeout_milliseconds");
  if (payload_field == json.fields.end() || maximum_field == json.fields.end() ||
      timeout_field == json.fields.end() ||
      !parse_integer(payload_field->second, &payload) ||
      !parse_integer(maximum_field->second, &maximum_payload) ||
      !parse_integer(timeout_field->second, &timeout) ||
      maximum_payload != kC2MaximumPayloadBytes ||
      payload > maximum_payload || timeout == 0U || timeout > 60'000U) {
    if (error) *error = "c2_receipt_bounds_invalid";
    return false;
  }
  return true;
}

bool write_c2_clearance_receipt(
    const std::filesystem::path& trusted_output_root,
    const std::filesystem::path& path, const C2ClearanceReceipt& receipt,
    std::string* error) {
  if (!is_direct_child_of_trusted_directory(trusted_output_root, path, error)) {
    return false;
  }
  const auto serialized = serialize_c2_clearance_receipt(receipt);
  return write_new_or_same_text_file_atomically(
      path, serialized, kC2ReceiptMaximumBytes,
      [](const std::filesystem::path& candidate, std::string* validation_error) {
        return validate_c2_clearance_receipt_file(candidate, validation_error);
      },
      error);
}

}  // namespace prisminfer

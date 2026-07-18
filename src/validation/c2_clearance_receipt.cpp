#include "prisminfer/c2_clearance_receipt.h"

#include <array>
#include <charconv>
#include <map>
#include <set>
#include <sstream>

#include "prisminfer/atomic_file.h"
#include "prisminfer/flat_json.h"
#include "prisminfer/sha256.h"

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

bool canonical_gpu_uuid(const std::string& value) {
  if (value.size() != 40U || value.rfind("GPU-", 0U) != 0U) return false;
  for (std::size_t index = 4U; index < value.size(); ++index) {
    const bool separator = index == 12U || index == 17U || index == 22U ||
                           index == 27U;
    if (separator ? value[index] != '-'
                  : !((value[index] >= '0' && value[index] <= '9') ||
                      (value[index] >= 'a' && value[index] <= 'f'))) {
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

bool require_boolean(const FlatJsonResult& json, const char* key) {
  const auto found = json.fields.find(key);
  return found != json.fields.end() &&
         found->second.kind == FlatJsonValue::Kind::Boolean;
}

template <typename Integer>
bool require_integer(const FlatJsonResult& json, const char* key,
                     Integer* value) {
  const auto found = json.fields.find(key);
  return found != json.fields.end() && parse_integer(found->second, value);
}

bool require_bool_value(const FlatJsonResult& json, const char* key,
                        bool* value) {
  const auto found = json.fields.find(key);
  if (found == json.fields.end() ||
      found->second.kind != FlatJsonValue::Kind::Boolean) {
    return false;
  }
  *value = found->second.text == "true";
  return true;
}

}  // namespace

bool compute_c2_clearance_evidence_hashes(C2ClearanceReceipt* receipt,
                                          std::string* error) {
  if (receipt == nullptr) {
    if (error) *error = "c2_evidence_receipt_required";
    return false;
  }
  std::ostringstream last;
  last << "c2-last-good-v1"
       << "|case=" << receipt->case_name
       << "|stage=" << receipt->last_good_stage
       << "|pid=" << receipt->root_process_id
       << "|luid_high=" << receipt->adapter_luid_high
       << "|luid_low=" << receipt->adapter_luid_low
       << "|sample_ms=" << receipt->last_sample_monotonic_milliseconds
       << "|wddm_budget=" << receipt->last_sample_wddm_local_budget_bytes
       << "|wddm_usage=" << receipt->last_sample_wddm_local_usage_bytes
       << "|process_source=" << receipt->process_wddm_source
       << "|process_ms="
       << receipt->process_wddm_sample_monotonic_milliseconds
       << "|process_current=" << receipt->process_wddm_current_bytes
       << "|process_peak=" << receipt->process_wddm_peak_bytes
       << "|host_ms=" << receipt->last_host_sample_monotonic_milliseconds
       << "|host_memory_available=" << receipt->last_host_memory_available_bytes
       << "|host_commit_available=" << receipt->last_host_commit_available_bytes
       << "|thermal_ms=" << receipt->last_thermal_sample_monotonic_milliseconds
       << "|temperature=" << receipt->last_gpu_temperature_celsius
       << "|slowdown=" << receipt->last_gpu_slowdown_celsius
       << "|thermal_throttling=" << receipt->last_gpu_thermal_throttling
       << "|power_brake=" << receipt->last_gpu_power_brake_slowdown
       << "|wddm_available=" << receipt->last_wddm_available
       << "|process_available=" << receipt->last_process_wddm_available
       << "|host_available=" << receipt->last_host_available
       << "|thermal_available=" << receipt->last_thermal_available
       << "|heartbeat=" << receipt->heartbeat_count;
  if (!sha256_text(last.str(), &receipt->last_good_sample_sha256)) {
    if (error) *error = "c2_last_good_hash_failed";
    return false;
  }
  if (!sha256_text(receipt->terminal_trigger_canonical,
                   &receipt->terminal_trigger_sha256)) {
    if (error) *error = "c2_terminal_trigger_hash_failed";
    return false;
  }
  std::ostringstream terminal;
  terminal << "c2-terminal-v1"
           << "|last_good=" << receipt->last_good_sample_sha256
           << "|status=" << receipt->status
           << "|failure=" << receipt->failure_reason
           << "|cleanup=" << receipt->cleanup_status
           << "|pre_cleanup=" << receipt->pre_cleanup_evidence_sha256
           << "|terminal_trigger=" << receipt->terminal_trigger_sha256
           << "|worker_exit=" << receipt->worker_exit_observed
           << "|job_tree_empty=" << receipt->job_tree_empty
           << "|job_accounting=" << receipt->job_accounting_reconciled
           << "|device_reconciled=" << receipt->device_resources_reconciled
           << "|handles_closed=" << receipt->artifact_handles_closed
           << "|temp_reconciled=" << receipt->temporary_files_reconciled
           << "|pre_wddm=" << receipt->pre_wddm_local_usage_bytes
           << "|final_wddm=" << receipt->final_wddm_local_usage_bytes
           << "|cleanup_delta="
           << receipt->cleanup_wddm_positive_delta_bytes
           << "|cleanup_tolerance=" << receipt->cleanup_wddm_tolerance_bytes;
  if (!sha256_text(terminal.str(), &receipt->evidence_bundle_sha256)) {
    if (error) *error = "c2_terminal_hash_failed";
    return false;
  }
  return true;
}

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
      << "  \"gpu_uuid\":\"" << receipt.gpu_uuid << "\",\n"
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
      << "  \"process_wddm_source\":\""
      << escaped(receipt.process_wddm_source) << "\",\n"
      << "  \"last_good_stage\":\"" << receipt.last_good_stage << "\",\n"
      << "  \"terminal_trigger_canonical\":\""
      << escaped(receipt.terminal_trigger_canonical) << "\",\n"
      << "  \"terminal_trigger_sha256\":\""
      << receipt.terminal_trigger_sha256 << "\",\n"
      << "  \"last_good_sample_sha256\":\""
      << receipt.last_good_sample_sha256 << "\",\n"
      << "  \"pre_cleanup_evidence_sha256\":\""
      << receipt.pre_cleanup_evidence_sha256 << "\",\n"
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
      << "  \"cleanup_wddm_positive_delta_bytes\":"
      << receipt.cleanup_wddm_positive_delta_bytes << ",\n"
      << "  \"cleanup_wddm_tolerance_bytes\":"
      << receipt.cleanup_wddm_tolerance_bytes << ",\n"
      << "  \"last_sample_monotonic_milliseconds\":"
      << receipt.last_sample_monotonic_milliseconds << ",\n"
      << "  \"last_sample_wddm_local_budget_bytes\":"
      << receipt.last_sample_wddm_local_budget_bytes << ",\n"
      << "  \"last_sample_wddm_local_usage_bytes\":"
      << receipt.last_sample_wddm_local_usage_bytes << ",\n"
      << "  \"process_wddm_sample_monotonic_milliseconds\":"
      << receipt.process_wddm_sample_monotonic_milliseconds << ",\n"
      << "  \"process_wddm_current_bytes\":"
      << receipt.process_wddm_current_bytes << ",\n"
      << "  \"process_wddm_peak_bytes\":"
      << receipt.process_wddm_peak_bytes << ",\n"
      << "  \"last_host_sample_monotonic_milliseconds\":"
      << receipt.last_host_sample_monotonic_milliseconds << ",\n"
      << "  \"last_host_memory_available_bytes\":"
      << receipt.last_host_memory_available_bytes << ",\n"
      << "  \"last_host_commit_available_bytes\":"
      << receipt.last_host_commit_available_bytes << ",\n"
      << "  \"last_thermal_sample_monotonic_milliseconds\":"
      << receipt.last_thermal_sample_monotonic_milliseconds << ",\n"
      << "  \"last_gpu_temperature_celsius\":"
      << receipt.last_gpu_temperature_celsius << ",\n"
      << "  \"last_gpu_slowdown_celsius\":"
      << receipt.last_gpu_slowdown_celsius << ",\n"
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
      << (receipt.temporary_files_reconciled ? "true" : "false") << ",\n"
      << "  \"last_gpu_thermal_throttling\":"
      << (receipt.last_gpu_thermal_throttling ? "true" : "false") << ",\n"
      << "  \"last_gpu_power_brake_slowdown\":"
      << (receipt.last_gpu_power_brake_slowdown ? "true" : "false") << ",\n"
      << "  \"last_wddm_available\":"
      << (receipt.last_wddm_available ? "true" : "false") << ",\n"
      << "  \"last_process_wddm_available\":"
      << (receipt.last_process_wddm_available ? "true" : "false") << ",\n"
      << "  \"last_host_available\":"
      << (receipt.last_host_available ? "true" : "false") << ",\n"
      << "  \"last_thermal_available\":"
      << (receipt.last_thermal_available ? "true" : "false") << "\n"
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
      "workflow_run_id", "authorization_id", "gpu_uuid", "case_name", "status", "failure_reason",
      "cleanup_status", "review_status", "claim_scope", "promotable",
      "c2_credit", "lease_id", "job_identity", "process_wddm_source",
      "last_good_stage",
      "terminal_trigger_canonical", "terminal_trigger_sha256",
      "last_good_sample_sha256", "pre_cleanup_evidence_sha256",
      "evidence_bundle_sha256", "adapter_luid_high", "adapter_luid_low",
      "adapter_index", "worker_timeout_milliseconds",
      "post_admission_payload_bytes", "maximum_payload_bytes",
      "context_cuda_free_bytes", "context_cuda_total_bytes",
      "last_heartbeat_cuda_free_bytes",
      "last_heartbeat_cuda_total_bytes", "pre_wddm_local_usage_bytes",
      "final_wddm_local_usage_bytes", "cleanup_wddm_positive_delta_bytes",
      "cleanup_wddm_tolerance_bytes", "pre_host_memory_available_bytes",
      "last_sample_monotonic_milliseconds",
      "last_sample_wddm_local_budget_bytes",
      "last_sample_wddm_local_usage_bytes",
      "process_wddm_sample_monotonic_milliseconds",
      "process_wddm_current_bytes", "process_wddm_peak_bytes",
      "last_host_sample_monotonic_milliseconds",
      "last_host_memory_available_bytes", "last_host_commit_available_bytes",
      "last_thermal_sample_monotonic_milliseconds",
      "last_gpu_temperature_celsius", "last_gpu_slowdown_celsius",
      "final_host_memory_available_bytes", "pre_host_commit_available_bytes",
      "final_host_commit_available_bytes", "pre_gpu_temperature_celsius",
      "final_gpu_temperature_celsius", "root_process_id",
      "heartbeat_count", "context_ready_observed", "token_consumed_observed",
      "worker_exit_observed", "job_tree_empty",
      "job_accounting_reconciled", "device_resources_reconciled",
      "artifact_handles_closed", "temporary_files_reconciled",
      "last_gpu_thermal_throttling", "last_gpu_power_brake_slowdown",
      "last_wddm_available", "last_process_wddm_available",
      "last_host_available", "last_thermal_available"};
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
  std::string gpu_uuid;
  std::string case_name;
  std::string status;
  std::string failure_reason;
  std::string cleanup_status;
  std::string review_status;
  std::string claim_scope;
  std::string lease_id;
  std::string job_identity;
  std::string process_source;
  std::string last_good_stage;
  std::string terminal_trigger;
  std::string terminal_trigger_hash;
  std::string last_good;
  std::string pre_cleanup;
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
      require_string(json, "gpu_uuid", &gpu_uuid) &&
      require_string(json, "case_name", &case_name) &&
      require_string(json, "status", &status) &&
      require_string(json, "failure_reason", &failure_reason) &&
      require_string(json, "cleanup_status", &cleanup_status) &&
      require_string(json, "review_status", &review_status) &&
      require_string(json, "claim_scope", &claim_scope) &&
      require_string(json, "lease_id", &lease_id) &&
      require_string(json, "job_identity", &job_identity) &&
      require_string(json, "process_wddm_source", &process_source) &&
      require_string(json, "last_good_stage", &last_good_stage) &&
      require_string(json, "terminal_trigger_canonical", &terminal_trigger) &&
      require_string(json, "terminal_trigger_sha256", &terminal_trigger_hash) &&
      require_string(json, "last_good_sample_sha256", &last_good) &&
      require_string(json, "pre_cleanup_evidence_sha256", &pre_cleanup) &&
      require_string(json, "evidence_bundle_sha256", &bundle);
  if (!strings_ok || schema != "prisminfer-c2-clearance-receipt-v1" ||
      receipt_class != "c2-clearance-candidate" ||
      review_status != "pending-independent-review" ||
      claim_scope != "synthetic-c2-candidate-non-promotable" ||
      !bounded_text(repository, 128U) || !lower_hex(reviewed_sha, 40U) ||
      !lower_hex(tree_sha, 40U) || !lower_hex(worker_sha, 64U) ||
      !bounded_text(approval, 128U) || !bounded_text(run_id, 128U) ||
      !authorization_identifier(authorization_id) ||
      !canonical_gpu_uuid(gpu_uuid) ||
      !bounded_text(case_name, 64U) ||
      (status != "candidate-complete" && status != "rejected" &&
       status != "aborted") ||
      !bounded_text(failure_reason, 256U, true) ||
      (cleanup_status != "cleaned" && cleanup_status != "quarantined") ||
      !bounded_text(lease_id, 128U) || !bounded_text(job_identity, 256U) ||
      !bounded_text(process_source, 64U) || !lower_hex(last_good, 64U) ||
      (last_good_stage != "pre-context" &&
       last_good_stage != "post-context" && last_good_stage != "watchdog") ||
      !bounded_text(terminal_trigger, 2048U) ||
      !lower_hex(terminal_trigger_hash, 64U) ||
      !lower_hex(pre_cleanup, 64U) || !lower_hex(bundle, 64U) ||
      !require_bool(json, "promotable", false) ||
      !require_bool(json, "c2_credit", false) ||
      !require_boolean(json, "last_gpu_thermal_throttling") ||
      !require_boolean(json, "last_gpu_power_brake_slowdown")) {
    if (error) *error = "c2_receipt_identity_or_classification_invalid";
    return false;
  }
  std::uint64_t payload = 0U;
  std::uint64_t maximum_payload = 0U;
  std::uint64_t cleanup_delta = 0U;
  std::uint64_t cleanup_tolerance = 0U;
  std::uint32_t timeout = 0U;
  const auto payload_field = json.fields.find("post_admission_payload_bytes");
  const auto maximum_field = json.fields.find("maximum_payload_bytes");
  const auto timeout_field = json.fields.find("worker_timeout_milliseconds");
  const auto cleanup_delta_field =
      json.fields.find("cleanup_wddm_positive_delta_bytes");
  const auto cleanup_tolerance_field =
      json.fields.find("cleanup_wddm_tolerance_bytes");
  if (payload_field == json.fields.end() || maximum_field == json.fields.end() ||
      timeout_field == json.fields.end() ||
      cleanup_delta_field == json.fields.end() ||
      cleanup_tolerance_field == json.fields.end() ||
      !parse_integer(payload_field->second, &payload) ||
      !parse_integer(maximum_field->second, &maximum_payload) ||
      !parse_integer(timeout_field->second, &timeout) ||
      !parse_integer(cleanup_delta_field->second, &cleanup_delta) ||
      !parse_integer(cleanup_tolerance_field->second, &cleanup_tolerance) ||
      maximum_payload != kC2MaximumPayloadBytes ||
      payload > maximum_payload || cleanup_tolerance != 16ULL * 1024ULL * 1024ULL ||
      cleanup_delta > cleanup_tolerance || timeout == 0U || timeout > 60'000U) {
    if (error) *error = "c2_receipt_bounds_invalid";
    return false;
  }
  C2ClearanceReceipt hashed;
  hashed.case_name = case_name;
  hashed.status = status;
  hashed.failure_reason = failure_reason;
  hashed.cleanup_status = cleanup_status;
  hashed.process_wddm_source = process_source;
  hashed.last_good_stage = last_good_stage;
  hashed.terminal_trigger_canonical = terminal_trigger;
  hashed.terminal_trigger_sha256 = terminal_trigger_hash;
  hashed.pre_cleanup_evidence_sha256 = pre_cleanup;
  const bool hash_fields_ok =
      require_integer(json, "root_process_id", &hashed.root_process_id) &&
      require_integer(json, "adapter_luid_high", &hashed.adapter_luid_high) &&
      require_integer(json, "adapter_luid_low", &hashed.adapter_luid_low) &&
      require_integer(json, "last_sample_monotonic_milliseconds",
                      &hashed.last_sample_monotonic_milliseconds) &&
      require_integer(json, "last_sample_wddm_local_budget_bytes",
                      &hashed.last_sample_wddm_local_budget_bytes) &&
      require_integer(json, "last_sample_wddm_local_usage_bytes",
                      &hashed.last_sample_wddm_local_usage_bytes) &&
      require_integer(json, "process_wddm_sample_monotonic_milliseconds",
                      &hashed.process_wddm_sample_monotonic_milliseconds) &&
      require_integer(json, "process_wddm_current_bytes",
                      &hashed.process_wddm_current_bytes) &&
      require_integer(json, "process_wddm_peak_bytes",
                      &hashed.process_wddm_peak_bytes) &&
      require_integer(json, "last_host_sample_monotonic_milliseconds",
                      &hashed.last_host_sample_monotonic_milliseconds) &&
      require_integer(json, "last_host_memory_available_bytes",
                      &hashed.last_host_memory_available_bytes) &&
      require_integer(json, "last_host_commit_available_bytes",
                      &hashed.last_host_commit_available_bytes) &&
      require_integer(json, "last_thermal_sample_monotonic_milliseconds",
                      &hashed.last_thermal_sample_monotonic_milliseconds) &&
      require_integer(json, "last_gpu_temperature_celsius",
                      &hashed.last_gpu_temperature_celsius) &&
      require_integer(json, "last_gpu_slowdown_celsius",
                      &hashed.last_gpu_slowdown_celsius) &&
      require_integer(json, "heartbeat_count", &hashed.heartbeat_count) &&
      require_integer(json, "pre_wddm_local_usage_bytes",
                      &hashed.pre_wddm_local_usage_bytes) &&
      require_integer(json, "final_wddm_local_usage_bytes",
                      &hashed.final_wddm_local_usage_bytes) &&
      require_integer(json, "cleanup_wddm_positive_delta_bytes",
                      &hashed.cleanup_wddm_positive_delta_bytes) &&
      require_integer(json, "cleanup_wddm_tolerance_bytes",
                      &hashed.cleanup_wddm_tolerance_bytes) &&
      require_bool_value(json, "worker_exit_observed",
                         &hashed.worker_exit_observed) &&
      require_bool_value(json, "job_tree_empty", &hashed.job_tree_empty) &&
      require_bool_value(json, "job_accounting_reconciled",
                         &hashed.job_accounting_reconciled) &&
      require_bool_value(json, "device_resources_reconciled",
                         &hashed.device_resources_reconciled) &&
      require_bool_value(json, "artifact_handles_closed",
                         &hashed.artifact_handles_closed) &&
      require_bool_value(json, "temporary_files_reconciled",
                         &hashed.temporary_files_reconciled) &&
      require_bool_value(json, "last_gpu_thermal_throttling",
                         &hashed.last_gpu_thermal_throttling) &&
      require_bool_value(json, "last_gpu_power_brake_slowdown",
                         &hashed.last_gpu_power_brake_slowdown) &&
      require_bool_value(json, "last_wddm_available",
                         &hashed.last_wddm_available) &&
      require_bool_value(json, "last_process_wddm_available",
                         &hashed.last_process_wddm_available) &&
      require_bool_value(json, "last_host_available",
                         &hashed.last_host_available) &&
      require_bool_value(json, "last_thermal_available",
                         &hashed.last_thermal_available);
  std::string hash_error;
  std::string recomputed_trigger;
  if (!sha256_text(terminal_trigger, &recomputed_trigger) ||
      recomputed_trigger != terminal_trigger_hash || !hash_fields_ok ||
      !compute_c2_clearance_evidence_hashes(&hashed, &hash_error) ||
      hashed.last_good_sample_sha256 != last_good ||
      hashed.evidence_bundle_sha256 != bundle) {
    if (error) *error = "c2_receipt_evidence_hash_mismatch";
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

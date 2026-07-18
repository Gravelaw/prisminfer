import copy
import importlib.util
import json
import pathlib
import unittest


REPO = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "c2_schema", REPO / "scripts" / "validate-json-schema.py")
SCHEMA_MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SCHEMA_MODULE)


def valid_receipt():
    return {
        "schema_version": "prisminfer-c2-clearance-receipt-v1",
        "receipt_class": "c2-clearance-candidate",
        "repository": "Gravelaw/prisminfer",
        "reviewed_sha": "1" * 40,
        "source_tree_sha": "2" * 40,
        "worker_sha256": "3" * 64,
        "worker_approval_identity": "c2-synthetic-worker-v1",
        "workflow_run_id": "unit-test",
        "authorization_id": "C2-AUTH-unit-test",
        "gpu_uuid": "GPU-11111111-2222-3333-4444-555555555555",
        "case_name": "success",
        "status": "candidate-complete",
        "failure_reason": "",
        "cleanup_status": "cleaned",
        "review_status": "pending-independent-review",
        "claim_scope": "synthetic-c2-candidate-non-promotable",
        "promotable": False,
        "c2_credit": False,
        "lease_id": "prisminfer-gpu-00000001-00000002",
        "job_identity": "job:1:2:3",
        "process_wddm_source": "wddm-process",
        "last_good_sample_sha256": "4" * 64,
        "pre_cleanup_evidence_sha256": "6" * 64,
        "evidence_bundle_sha256": "5" * 64,
        "adapter_luid_high": 1,
        "adapter_luid_low": 2,
        "adapter_index": 0,
        "worker_timeout_milliseconds": 10000,
        "post_admission_payload_bytes": 67108864,
        "maximum_payload_bytes": 67108864,
        "context_cuda_free_bytes": 8 << 30,
        "context_cuda_total_bytes": 16 << 30,
        "last_heartbeat_cuda_free_bytes": (8 << 30) - 67108864,
        "last_heartbeat_cuda_total_bytes": 16 << 30,
        "pre_wddm_local_usage_bytes": 256 << 20,
        "final_wddm_local_usage_bytes": 256 << 20,
        "cleanup_wddm_positive_delta_bytes": 0,
        "cleanup_wddm_tolerance_bytes": 16 << 20,
        "last_sample_monotonic_milliseconds": 100,
        "last_sample_wddm_local_budget_bytes": 16 << 30,
        "last_sample_wddm_local_usage_bytes": 2 << 30,
        "process_wddm_sample_monotonic_milliseconds": 101,
        "process_wddm_current_bytes": 576 << 20,
        "process_wddm_peak_bytes": 576 << 20,
        "last_host_sample_monotonic_milliseconds": 102,
        "last_host_memory_available_bytes": 15 << 30,
        "last_host_commit_available_bytes": 23 << 30,
        "last_thermal_sample_monotonic_milliseconds": 103,
        "last_gpu_temperature_celsius": 46,
        "last_gpu_slowdown_celsius": 90,
        "pre_host_memory_available_bytes": 16 << 30,
        "final_host_memory_available_bytes": 15 << 30,
        "pre_host_commit_available_bytes": 24 << 30,
        "final_host_commit_available_bytes": 23 << 30,
        "pre_gpu_temperature_celsius": 45,
        "final_gpu_temperature_celsius": 46,
        "root_process_id": 7,
        "heartbeat_count": 2,
        "context_ready_observed": True,
        "token_consumed_observed": True,
        "worker_exit_observed": True,
        "job_tree_empty": True,
        "job_accounting_reconciled": True,
        "device_resources_reconciled": True,
        "artifact_handles_closed": True,
        "temporary_files_reconciled": True,
        "last_gpu_thermal_throttling": False,
        "last_gpu_power_brake_slowdown": False,
        "last_wddm_available": True,
        "last_process_wddm_available": True,
        "last_host_available": True,
        "last_thermal_available": True,
    }


class C2ClearanceSchemaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.schema = json.loads(
            (REPO / "schemas" / "c2_clearance_receipt.schema.json").read_text(
                encoding="utf-8"
            )
        )

    def assert_valid(self, value):
        SCHEMA_MODULE.check(self.schema, value, self.schema)

    def assert_invalid(self, value):
        with self.assertRaises(Exception):
            self.assert_valid(value)

    def test_valid_candidate(self):
        self.assert_valid(valid_receipt())

    def test_payload_limit_is_binding(self):
        value = valid_receipt()
        value["post_admission_payload_bytes"] += 1
        self.assert_invalid(value)

    def test_promotion_and_credit_cannot_be_asserted(self):
        for field in ("promotable", "c2_credit"):
            value = valid_receipt()
            value[field] = True
            self.assert_invalid(value)

    def test_unknown_and_missing_fields_reject(self):
        value = valid_receipt()
        value["unexpected"] = 1
        self.assert_invalid(value)
        value = valid_receipt()
        del value["worker_sha256"]
        self.assert_invalid(value)

    def test_case_and_sha_are_strict(self):
        value = valid_receipt()
        value["case_name"] = "benchmark"
        self.assert_invalid(value)
        value = valid_receipt()
        value["reviewed_sha"] = "A" * 40
        self.assert_invalid(value)


if __name__ == "__main__":
    unittest.main()

import datetime as dt
import hashlib
import importlib.util
import json
import tempfile
import unittest
import os
from pathlib import Path


SCRIPT = Path(__file__).parents[2] / "scripts" / "evidence-runner.py"
REPO = Path(__file__).parents[2]
SPEC = importlib.util.spec_from_file_location("evidence_runner", SCRIPT)
runner = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(runner)

SCHEMA_SPEC = importlib.util.spec_from_file_location(
    "packet_a_schema", REPO / "scripts" / "validate-json-schema.py")
schema_validator = importlib.util.module_from_spec(SCHEMA_SPEC)
assert SCHEMA_SPEC.loader is not None
SCHEMA_SPEC.loader.exec_module(schema_validator)


class EvidenceRunnerTests(unittest.TestCase):
    def setUp(self):
        self.now = dt.datetime.now(dt.timezone.utc).replace(microsecond=0)
        self.run_id = "run_0123456789abcdef"
        self.trial = {
            "record_type": "trial", "run_id": self.run_id,
            "validation_cell_id": "cpu-cell", "evidence_policy_version": runner.POLICY,
            "sample_plan_version": runner.SAMPLE_PLAN, "sequence": 1,
            "trial_index": 1, "status": "ok",
        }
        self.terminal = {
            "record_type": "terminal", "run_id": self.run_id,
            "validation_cell_id": "cpu-cell", "evidence_policy_version": runner.POLICY,
            "sample_plan_version": runner.SAMPLE_PLAN, "sequence": 2,
            "outcome": "completed", "reason": "",
        }
        self.evidence = self.encode(self.trial, self.terminal)
        self.manifest = self.make_manifest(self.evidence)

    @staticmethod
    def encode(*records):
        return b"".join((json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n").encode() for record in records)

    def make_manifest(self, evidence, **changes):
        value = {
            "manifest_version": "1.0", "run_id": self.run_id,
            "run_fingerprint_sha256": "a" * 64, "validation_cell_id": "cpu-cell",
            "evidence_policy_version": runner.POLICY, "sample_plan_version": runner.SAMPLE_PLAN,
            "started_at_utc": (self.now - dt.timedelta(seconds=30)).isoformat().replace("+00:00", "Z"),
            "completed_at_utc": self.now.isoformat().replace("+00:00", "Z"),
            "terminal_sequence": 2, "declared_trial_count": 1,
            "evidence_sha256": hashlib.sha256(evidence).hexdigest(),
            "run_outcome": "completed", "claim_ceiling": "research-only", "failure_reason": "",
        }
        value.update(changes)
        if "run_fingerprint_sha256" not in changes:
            value["run_fingerprint_sha256"] = runner.expected_run_fingerprint(value)
        return value

    def validate(self, manifest=None, evidence=None):
        manifest = manifest or self.manifest
        evidence = evidence or self.evidence
        runner.validate_manifest(manifest, self.now)
        runner.validate_records(evidence, manifest)

    def assert_rejected(self, callback, reason):
        with self.assertRaisesRegex(runner.Rejected, reason):
            callback()

    def assert_schema(self, name, value):
        schema = json.loads((REPO / "schemas" / name).read_text(encoding="utf-8"))
        schema_validator.check(schema, value, schema)

    def test_checked_policy_plan_and_emitted_bundle_match_schemas(self):
        policy = json.loads((REPO / "configs" / "evidence-policy-v1.json").read_text(encoding="utf-8"))
        plan = json.loads((REPO / "configs" / "sample-plan-v1.json").read_text(encoding="utf-8"))
        self.assert_schema("evidence_policy.schema.json", policy)
        self.assert_schema("sample_plan.schema.json", plan)
        raw_records = [{"trial_index": 1, "status": "ok"}, {"trial_index": 2, "status": "ok"}]
        for record in raw_records:
            self.assert_schema("evidence_input_record.schema.json", record)
        with tempfile.TemporaryDirectory() as directory:
            final = runner.emit_bundle(
                validation_cell_id="cpu-cell", declared_trial_count=2,
                raw_input=self.encode(*raw_records), failure_input=None,
                output_root=Path(directory), started=self.now - dt.timedelta(seconds=1))
            self.assert_schema("evidence_runner_manifest.schema.json", json.loads((final / "manifest.json").read_text()))
            self.assert_schema("committed_evidence_bundle.schema.json", json.loads((final / "COMMIT.json").read_text()))
            for line in (final / "evidence.jsonl").read_text().splitlines():
                self.assert_schema("evidence_record.schema.json", json.loads(line))

    def test_valid_completed_bundle_publishes_commit_last(self):
        self.validate()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest_bytes = (json.dumps(self.manifest, sort_keys=True) + "\n").encode()
            final = runner.publish(root, manifest_bytes, self.evidence, self.manifest)
            self.assertEqual({p.name for p in final.iterdir()}, {
                "manifest.json", "evidence.jsonl", "manifest.sha256",
                "evidence.sha256", "COMMIT.json",
            })
            commit = json.loads((final / "COMMIT.json").read_text())
            self.assertEqual(commit["run_id"], self.run_id)
            verified = runner.validate_committed_bundle(final, self.now)
            self.assertEqual(verified["run_id"], self.run_id)

    def test_runner_emits_its_own_manifest_identity_and_records(self):
        raw = self.encode({"trial_index": 1, "status": "ok"}, {"trial_index": 2, "status": "ok"})
        with tempfile.TemporaryDirectory() as directory:
            final = runner.emit_bundle(validation_cell_id="cpu-cell", declared_trial_count=2,
                                       raw_input=raw, failure_input=None, output_root=Path(directory),
                                       started=self.now - dt.timedelta(seconds=1))
            manifest = runner.validate_committed_bundle(final, self.now, require_fresh=True)
            self.assertTrue(manifest["run_id"].startswith("run_"))
            self.assertEqual(manifest["declared_trial_count"], 2)
            self.assertEqual(manifest["run_fingerprint_sha256"], runner.expected_run_fingerprint(manifest))

    def test_runner_emits_fail_closed_terminal_only_outcomes(self):
        for outcome in sorted(runner.FAILURE_OUTCOMES):
            with self.subTest(outcome=outcome), tempfile.TemporaryDirectory() as directory:
                failure = self.encode({"outcome": outcome, "reason": "fixture_reason"})
                final = runner.emit_bundle(validation_cell_id="cpu-cell", declared_trial_count=0,
                                           raw_input=None, failure_input=failure, output_root=Path(directory),
                                           started=self.now - dt.timedelta(seconds=1))
                manifest = runner.validate_committed_bundle(final, self.now)
                self.assertEqual(manifest["run_outcome"], outcome)
                self.assertEqual(manifest["claim_ceiling"], "rejected")

    def test_committed_bundle_rejects_missing_and_hash_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = (json.dumps(self.manifest, sort_keys=True) + "\n").encode()
            final = runner.publish(root, data, self.evidence, self.manifest)
            (final / "evidence.sha256").write_text("0" * 64 + "  evidence.jsonl\n")
            self.assert_rejected(
                lambda: runner.validate_committed_bundle(final, self.now),
                "evidence_digest_rejected",
            )
            (final / "evidence.sha256").unlink()
            self.assert_rejected(
                lambda: runner.validate_committed_bundle(final, self.now),
                "file_set_rejected",
            )

    def test_duplicate_json_field_rejected(self):
        self.assert_rejected(lambda: runner.strict_json(b'{"a":1,"a":2}', 100, "record"), "duplicate_field")
        self.assert_rejected(lambda: runner.strict_json(b'{"a":NaN}', 100, "record"), "non_finite_json")

    def test_partial_and_missing_newline_rejected(self):
        self.assert_rejected(lambda: runner.validate_records(self.encode(self.trial), self.manifest), "terminal_or_count")
        self.assert_rejected(lambda: runner.validate_records(self.evidence.rstrip(b"\n"), self.manifest), "final_newline")

    def test_count_sequence_and_identity_mismatch_rejected(self):
        bad = dict(self.manifest, declared_trial_count=2)
        self.assert_rejected(lambda: runner.validate_manifest(bad, self.now), "count_or_sequence")
        wrong = dict(self.trial, run_id="run_ffffffffffffffff")
        self.assert_rejected(lambda: runner.validate_records(self.encode(wrong, self.terminal), self.manifest), "identity_mismatch")
        boolean = dict(self.trial, sequence=True, trial_index=True)
        self.assert_rejected(lambda: runner.validate_records(self.encode(boolean, self.terminal), self.manifest), "sequence_type")

    def test_stale_future_and_policy_mismatch_rejected(self):
        stale = dict(self.manifest, completed_at_utc=(self.now - dt.timedelta(minutes=16)).isoformat().replace("+00:00", "Z"))
        future = dict(self.manifest, completed_at_utc=(self.now + dt.timedelta(minutes=6)).isoformat().replace("+00:00", "Z"))
        policy = dict(self.manifest, evidence_policy_version="evidence-policy-v0")
        self.assert_rejected(lambda: runner.validate_manifest(stale, self.now), "freshness")
        self.assert_rejected(lambda: runner.validate_manifest(future, self.now), "freshness")
        self.assert_rejected(lambda: runner.validate_manifest(policy, self.now), "version_identity")

    def test_hash_mismatch_is_detectable(self):
        self.assertNotEqual(hashlib.sha256(self.evidence + b"x").hexdigest(), self.manifest["evidence_sha256"])

    def test_fingerprint_is_derived_and_replay_is_rejected(self):
        bad = dict(self.manifest, run_fingerprint_sha256="0" * 64)
        self.assert_rejected(lambda: runner.validate_manifest(bad, self.now), "fingerprint_mismatch")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = (json.dumps(self.manifest) + "\n").encode()
            runner.publish(root, data, self.evidence, self.manifest)
            replay = dict(self.manifest, run_id="run_fedcba9876543210")
            self.assert_rejected(lambda: runner.publish(root, data, self.evidence, replay), "replay_rejected")

    def test_emit_rejects_byte_identical_input_replay(self):
        raw = self.encode({"trial_index": 1, "status": "ok"})
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = runner.emit_bundle(validation_cell_id="cpu-cell", declared_trial_count=1,
                                       raw_input=raw, failure_input=None, output_root=root,
                                       started=self.now - dt.timedelta(seconds=1))
            self.assertEqual(first.name, runner.validate_committed_bundle(first, self.now)["run_fingerprint_sha256"])
            self.assert_rejected(
                lambda: runner.emit_bundle(validation_cell_id="cpu-cell", declared_trial_count=1,
                                           raw_input=raw, failure_input=None, output_root=root,
                                           started=self.now - dt.timedelta(seconds=1)),
                "replay_rejected")

    def test_aborted_is_terminal_only_and_rejected_ceiling(self):
        terminal = dict(self.terminal, sequence=1, outcome="aborted", reason="watchdog_timeout")
        evidence = self.encode(terminal)
        manifest = self.make_manifest(
            evidence, terminal_sequence=1, declared_trial_count=0,
            run_outcome="aborted", claim_ceiling="rejected", failure_reason="watchdog_timeout",
        )
        self.validate(manifest, evidence)
        self.assert_rejected(lambda: runner.validate_records(self.encode(self.trial, terminal), manifest), "trial_record")

    def test_existing_bundle_target_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / self.manifest["run_fingerprint_sha256"]).mkdir()
            data = (json.dumps(self.manifest) + "\n").encode()
            self.assert_rejected(lambda: runner.publish(root, data, self.evidence, self.manifest), "replay_rejected")

    def test_retained_bundle_integrity_is_distinct_from_promotion_freshness(self):
        old = self.make_manifest(
            self.evidence,
            started_at_utc=(self.now - dt.timedelta(hours=2, seconds=30)).isoformat().replace("+00:00", "Z"),
            completed_at_utc=(self.now - dt.timedelta(hours=2)).isoformat().replace("+00:00", "Z"),
        )
        with tempfile.TemporaryDirectory() as directory:
            data = (json.dumps(old) + "\n").encode()
            final = runner.publish(Path(directory), data, self.evidence, old)
            self.assertEqual(runner.validate_committed_bundle(final, self.now)["run_id"], self.run_id)
            self.assert_rejected(
                lambda: runner.validate_committed_bundle(final, self.now, require_fresh=True),
                "freshness_rejected",
            )

    @unittest.skipUnless(__import__("os").name == "nt", "Windows ADS rejection")
    def test_windows_ads_input_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory) / "input.json"
            base.write_text("{}")
            self.assert_rejected(lambda: runner.read_opened(Path(str(base) + ":stream"), 100, "record"), "windows_path_rejected")

    @unittest.skipUnless(os.name == "nt", "Windows handle invariants")
    def test_windows_held_root_blocks_replacement_and_reparse(self):
        import sys
        sys.path.insert(0, str(SCRIPT.parent))
        import win32_handle_fs as winfs
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "root"; root.mkdir()
            handle = winfs.open_directory_tree(str(root), writable_final=True)
            try:
                with self.assertRaises(OSError):
                    os.rename(root, Path(directory) / "swapped")
            finally:
                winfs.close(handle)
            target = Path(directory) / "target"; target.mkdir()
            junction = Path(directory) / "junction"
            try:
                os.symlink(target, junction, target_is_directory=True)
            except OSError:
                self.skipTest("directory symlink privilege unavailable")
            with self.assertRaises(OSError):
                winfs.open_directory_tree(str(junction), writable_final=False)


if __name__ == "__main__":
    unittest.main()

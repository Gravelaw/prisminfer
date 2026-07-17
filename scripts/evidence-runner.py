#!/usr/bin/env python3
"""Strict, CPU-only Packet A evidence validator and atomic bundle publisher."""

from __future__ import annotations

import argparse
import ctypes
import datetime as dt
import hashlib
import json
import os
import secrets
import stat
import sys
from pathlib import Path

MAX_MANIFEST = 64 * 1024
MAX_EVIDENCE = 64 * 1024 * 1024
MAX_RECORDS = 1_000_001
POLICY = "evidence-policy-v1"
SAMPLE_PLAN = "sample-plan-v1"
MANIFEST_KEYS = {
    "manifest_version", "run_id", "run_fingerprint_sha256",
    "validation_cell_id", "evidence_policy_version", "sample_plan_version",
    "started_at_utc", "completed_at_utc", "terminal_sequence",
    "declared_trial_count", "evidence_sha256", "run_outcome",
    "claim_ceiling", "failure_reason",
}
IDENTITY_KEYS = {
    "record_type", "run_id", "validation_cell_id", "evidence_policy_version",
    "sample_plan_version", "sequence",
}
TRIAL_KEYS = IDENTITY_KEYS | {"trial_index", "status"}
TERMINAL_KEYS = IDENTITY_KEYS | {"outcome", "reason"}
OUTCOMES = {"completed", "skipped", "unsupported", "rejected", "aborted"}
FAILURE_OUTCOMES = OUTCOMES - {"completed"}
RAW_TRIAL_INPUT_KEYS = {"trial_index", "status"}
FAILURE_INPUT_KEYS = {"outcome", "reason"}
BUNDLE_FILES = {
    "manifest.json", "evidence.jsonl", "manifest.sha256",
    "evidence.sha256", "COMMIT.json",
}
COMMIT_KEYS = {
    "bundle_version", "run_id", "run_fingerprint_sha256",
    "manifest_sha256", "evidence_sha256",
}


class Rejected(ValueError):
    pass


def expected_run_fingerprint(manifest: dict[str, object]) -> str:
    """Hash only policy-controlled run inputs, never clocks or emitted evidence."""
    inputs = {
        "fingerprint_version": "run-fingerprint-v1",
        "validation_cell_id": manifest["validation_cell_id"],
        "evidence_policy_version": manifest["evidence_policy_version"],
        "sample_plan_version": manifest["sample_plan_version"],
        "declared_trial_count": manifest["declared_trial_count"],
        "claim_ceiling": manifest["claim_ceiling"],
        "run_outcome": manifest["run_outcome"],
        "failure_reason": manifest["failure_reason"],
    }
    encoded = json.dumps(inputs, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(encoded).hexdigest()


def no_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise Rejected(f"duplicate_field:{key}")
        result[key] = value
    return result


def strict_json(data: bytes, maximum: int, label: str) -> dict[str, object]:
    if not data or len(data) > maximum or data.startswith(b"\xef\xbb\xbf"):
        raise Rejected(f"{label}_size_or_encoding_rejected")
    try:
        value = json.loads(data.decode("utf-8"), object_pairs_hook=no_duplicates,
                           parse_constant=lambda value: (_ for _ in ()).throw(Rejected(f"non_finite_json:{value}")))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise Rejected(f"{label}_json_malformed") from exc
    if not isinstance(value, dict):
        raise Rejected(f"{label}_not_object")
    return value


def open_posix_directory_tree(path: Path) -> int:
    absolute = Path(os.path.abspath(path))
    descriptor = os.open("/", os.O_RDONLY | os.O_DIRECTORY)
    try:
        for component in absolute.parts[1:]:
            child = os.open(component, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=descriptor)
            os.close(descriptor); descriptor = child
        return descriptor
    except Exception:
        os.close(descriptor); raise


def read_opened(path: Path, maximum: int, label: str) -> bytes:
    if os.name == "nt":
        if any(part in (".", "..") for part in path.parts) or ":" in path.name:
            raise Rejected(f"{label}_windows_path_rejected")
        sys.path.insert(0, str(Path(__file__).parent))
        import win32_handle_fs as winfs
        directory = None
        try:
            directory = winfs.open_directory_tree(str(path.parent), writable_final=False)
            return winfs.open_relative(directory, path.name, maximum)
        except OSError as exc:
            raise Rejected(f"{label}_handle_open_rejected") from exc
        finally:
            if directory is not None:
                winfs.close(directory)
    directory = open_posix_directory_tree(path.parent)
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
    fd = os.open(path.name, flags, dir_fd=directory)
    try:
        before = os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_size > maximum:
            raise Rejected(f"{label}_not_bounded_regular_file")
        chunks: list[bytes] = []
        remaining = before.st_size
        while remaining:
            chunk = os.read(fd, min(1024 * 1024, remaining))
            if not chunk:
                raise Rejected(f"{label}_short_read")
            chunks.append(chunk)
            remaining -= len(chunk)
        after = os.fstat(fd)
        if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) != (
            after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns
        ):
            raise Rejected(f"{label}_changed_during_read")
        return b"".join(chunks)
    finally:
        os.close(fd)
        os.close(directory)


def validate_committed_bundle(bundle: Path, now: dt.datetime, require_fresh: bool = False) -> dict[str, object]:
    limits = {"manifest.json": MAX_MANIFEST, "evidence.jsonl": MAX_EVIDENCE,
              "manifest.sha256": 256, "evidence.sha256": 256, "COMMIT.json": 4096}
    labels = {name: "bundle_" + name.split(".")[0] for name in limits}
    directory = None
    try:
        if os.name == "nt":
            sys.path.insert(0, str(Path(__file__).parent))
            import win32_handle_fs as winfs
            directory = winfs.open_directory_tree(str(bundle), writable_final=False)
            if winfs.list_directory(directory) != BUNDLE_FILES:
                raise Rejected("bundle_file_set_rejected")
            payloads = {name: winfs.open_relative(directory, name, limits[name]) for name in limits}
            if winfs.list_directory(directory) != BUNDLE_FILES:
                raise Rejected("bundle_changed_during_validation")
        else:
            directory = open_posix_directory_tree(bundle)
            if set(os.listdir(directory)) != BUNDLE_FILES:
                raise Rejected("bundle_file_set_rejected")
            payloads = {}
            for name, maximum in limits.items():
                fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW, dir_fd=directory)
                try:
                    info = os.fstat(fd)
                    if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or info.st_size > maximum:
                        raise Rejected(f"{labels[name]}_not_bounded_regular_file")
                    payloads[name] = b""
                    while len(payloads[name]) < info.st_size:
                        chunk = os.read(fd, min(1024 * 1024, info.st_size - len(payloads[name])))
                        if not chunk:
                            raise Rejected(f"{labels[name]}_short_read")
                        payloads[name] += chunk
                finally:
                    os.close(fd)
            if set(os.listdir(directory)) != BUNDLE_FILES:
                raise Rejected("bundle_changed_during_validation")
    except OSError as exc:
        raise Rejected("bundle_handle_validation_rejected") from exc
    finally:
        if directory is not None:
            winfs.close(directory) if os.name == "nt" else os.close(directory)
    manifest = strict_json(payloads["manifest.json"], MAX_MANIFEST, "bundle_manifest")
    commit = strict_json(payloads["COMMIT.json"], 4096, "bundle_commit")
    if set(commit) != COMMIT_KEYS or commit.get("bundle_version") != "committed-evidence-v1":
        raise Rejected("bundle_commit_fields_rejected")
    manifest_hash = hashlib.sha256(payloads["manifest.json"]).hexdigest()
    evidence_hash = hashlib.sha256(payloads["evidence.jsonl"]).hexdigest()
    if payloads["manifest.sha256"] != f"{manifest_hash}  manifest.json\n".encode():
        raise Rejected("bundle_manifest_digest_rejected")
    if payloads["evidence.sha256"] != f"{evidence_hash}  evidence.jsonl\n".encode():
        raise Rejected("bundle_evidence_digest_rejected")
    identities = (
        commit["run_id"] == manifest["run_id"],
        bundle.name == manifest["run_fingerprint_sha256"],
        commit["run_fingerprint_sha256"] == manifest["run_fingerprint_sha256"],
        commit["manifest_sha256"] == manifest_hash,
        commit["evidence_sha256"] == evidence_hash == manifest["evidence_sha256"],
    )
    if not all(identities):
        raise Rejected("bundle_identity_or_hash_mismatch")
    validate_manifest(manifest, now, enforce_freshness=require_fresh)
    validate_records(payloads["evidence.jsonl"], manifest)
    return manifest


def parse_time(value: object, field: str) -> dt.datetime:
    if not isinstance(value, str) or not value.endswith("Z"):
        raise Rejected(f"invalid_field:{field}")
    try:
        parsed = dt.datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as exc:
        raise Rejected(f"invalid_field:{field}") from exc
    return parsed


def require_string(value: object, field: str, maximum: int = 512) -> str:
    if not isinstance(value, str) or len(value) > maximum:
        raise Rejected(f"invalid_field:{field}")
    return value


def validate_manifest(manifest: dict[str, object], now: dt.datetime, enforce_freshness: bool = True) -> None:
    if set(manifest) != MANIFEST_KEYS:
        extra = sorted(set(manifest) - MANIFEST_KEYS)
        missing = sorted(MANIFEST_KEYS - set(manifest))
        raise Rejected(f"manifest_fields_rejected:extra={extra}:missing={missing}")
    if manifest["manifest_version"] != "1.0" or manifest["evidence_policy_version"] != POLICY or manifest["sample_plan_version"] != SAMPLE_PLAN:
        raise Rejected("manifest_version_identity_mismatch")
    run_id = require_string(manifest["run_id"], "run_id", 64)
    if len(run_id) < 16 or not run_id[0].isalnum() or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-" for c in run_id):
        raise Rejected("invalid_field:run_id")
    fingerprint = require_string(manifest["run_fingerprint_sha256"], "run_fingerprint_sha256", 64)
    digest = require_string(manifest["evidence_sha256"], "evidence_sha256", 64)
    if any(len(value) != 64 or any(c not in "0123456789abcdef" for c in value) for value in (fingerprint, digest)):
        raise Rejected("invalid_sha256_identity")
    require_string(manifest["validation_cell_id"], "validation_cell_id", 128)
    started = parse_time(manifest["started_at_utc"], "started_at_utc")
    completed = parse_time(manifest["completed_at_utc"], "completed_at_utc")
    if completed < started or (enforce_freshness and (completed < now - dt.timedelta(minutes=15) or completed > now + dt.timedelta(minutes=5))):
        raise Rejected("manifest_freshness_rejected")
    count = manifest["declared_trial_count"]
    terminal = manifest["terminal_sequence"]
    if type(count) is not int or type(terminal) is not int or count < 0 or count > 1_000_000 or terminal != count + 1:
        raise Rejected("manifest_count_or_sequence_rejected")
    outcome = manifest["run_outcome"]
    reason = require_string(manifest["failure_reason"], "failure_reason")
    if outcome not in OUTCOMES:
        raise Rejected("invalid_field:run_outcome")
    if outcome == "completed":
        if count == 0 or reason or manifest["claim_ceiling"] != "research-only":
            raise Rejected("completed_manifest_rejected")
    elif count != 0 or not reason or manifest["claim_ceiling"] != "rejected":
        raise Rejected("failure_manifest_rejected")
    if fingerprint != expected_run_fingerprint(manifest):
        raise Rejected("run_fingerprint_mismatch")


def validate_records(data: bytes, manifest: dict[str, object]) -> None:
    if not data.endswith(b"\n"):
        raise Rejected("evidence_missing_final_newline")
    lines = data.splitlines()
    if not lines or len(lines) > MAX_RECORDS:
        raise Rejected("evidence_record_count_rejected")
    trials = 0
    terminal_seen = False
    for index, line in enumerate(lines, 1):
        record = strict_json(line, 64 * 1024, "record")
        expected_keys = TRIAL_KEYS if record.get("record_type") == "trial" else TERMINAL_KEYS
        if set(record) != expected_keys:
            raise Rejected(f"record_fields_rejected:{index}")
        for key in ("run_id", "validation_cell_id", "evidence_policy_version", "sample_plan_version"):
            if record[key] != manifest[key]:
                raise Rejected(f"record_identity_mismatch:{index}:{key}")
        if record["sequence"] != index:
            raise Rejected(f"record_sequence_mismatch:{index}")
        if type(record["sequence"]) is not int:
            raise Rejected(f"record_sequence_type_rejected:{index}")
        if record["record_type"] == "trial":
            if type(record["trial_index"]) is not int or terminal_seen or manifest["run_outcome"] != "completed" or record["trial_index"] != index or record["status"] != "ok":
                raise Rejected(f"trial_record_rejected:{index}")
            trials += 1
        else:
            if terminal_seen or index != len(lines) or record["outcome"] != manifest["run_outcome"] or record["reason"] != manifest["failure_reason"]:
                raise Rejected(f"terminal_record_rejected:{index}")
            terminal_seen = True
    if not terminal_seen or trials != manifest["declared_trial_count"] or len(lines) != manifest["terminal_sequence"]:
        raise Rejected("evidence_terminal_or_count_mismatch")


def parse_jsonl_input(data: bytes, label: str) -> list[dict[str, object]]:
    if not data.endswith(b"\n"):
        raise Rejected(f"{label}_missing_final_newline")
    lines = data.splitlines()
    if not lines or len(lines) > MAX_RECORDS:
        raise Rejected(f"{label}_record_count_rejected")
    return [strict_json(line, 64 * 1024, label) for line in lines]


def emit_bundle(*, validation_cell_id: str, declared_trial_count: int,
                raw_input: bytes | None, failure_input: bytes | None,
                output_root: Path, started: dt.datetime | None = None) -> Path:
    if (raw_input is None) == (failure_input is None):
        raise Rejected("exactly_one_evidence_input_required")
    started = started or dt.datetime.now(dt.timezone.utc)
    run_id = "run_" + secrets.token_hex(16)
    base = {"run_id": run_id, "validation_cell_id": validation_cell_id,
            "evidence_policy_version": POLICY, "sample_plan_version": SAMPLE_PLAN}
    records: list[dict[str, object]] = []
    if raw_input is not None:
        inputs = parse_jsonl_input(raw_input, "raw_trial")
        if type(declared_trial_count) is not int or declared_trial_count <= 0 or len(inputs) != declared_trial_count:
            raise Rejected("raw_trial_declared_count_mismatch")
        for index, value in enumerate(inputs, 1):
            if set(value) != RAW_TRIAL_INPUT_KEYS or type(value["trial_index"]) is not int or value["trial_index"] != index or value["status"] != "ok":
                raise Rejected(f"raw_trial_rejected:{index}")
            records.append({"record_type": "trial", **base, "sequence": index,
                            "trial_index": index, "status": "ok"})
        outcome, reason, ceiling = "completed", "", "research-only"
    else:
        inputs = parse_jsonl_input(failure_input or b"", "failure")
        if declared_trial_count != 0 or len(inputs) != 1 or set(inputs[0]) != FAILURE_INPUT_KEYS:
            raise Rejected("failure_input_rejected")
        outcome = inputs[0]["outcome"]; reason = require_string(inputs[0]["reason"], "reason")
        if outcome not in FAILURE_OUTCOMES or not reason:
            raise Rejected("failure_input_rejected")
        ceiling = "rejected"
    terminal_sequence = len(records) + 1
    records.append({"record_type": "terminal", **base, "sequence": terminal_sequence,
                    "outcome": outcome, "reason": reason})
    evidence_data = b"".join((json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n").encode() for record in records)
    completed = dt.datetime.now(dt.timezone.utc)
    manifest: dict[str, object] = {
        "manifest_version": "1.0", **base,
        "run_fingerprint_sha256": "0" * 64,
        "started_at_utc": started.isoformat().replace("+00:00", "Z"),
        "completed_at_utc": completed.isoformat().replace("+00:00", "Z"),
        "terminal_sequence": terminal_sequence, "declared_trial_count": declared_trial_count,
        "evidence_sha256": hashlib.sha256(evidence_data).hexdigest(), "run_outcome": outcome,
        "claim_ceiling": ceiling, "failure_reason": reason,
    }
    manifest["run_fingerprint_sha256"] = expected_run_fingerprint(manifest)
    validate_manifest(manifest, completed)
    validate_records(evidence_data, manifest)
    manifest_data = (json.dumps(manifest, sort_keys=True, separators=(",", ":")) + "\n").encode()
    return publish(output_root, manifest_data, evidence_data, manifest)


def fsync_directory(path: Path) -> None:
    if os.name != "nt":
        fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)


def rename_noreplace(source: str, destination: str, directory_fd: int) -> None:
    libc = ctypes.CDLL(None, use_errno=True)
    renameat2 = getattr(libc, "renameat2", None)
    if renameat2 is None:
        raise Rejected("exclusive_directory_rename_unavailable")
    renameat2.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
    renameat2.restype = ctypes.c_int
    if renameat2(directory_fd, os.fsencode(source), directory_fd, os.fsencode(destination), 1) != 0:
        error = ctypes.get_errno()
        if error == 17:
            raise Rejected("bundle_target_exists")
        raise OSError(error, "renameat2")


def publish(output_root: Path, manifest_data: bytes, evidence_data: bytes, manifest: dict[str, object]) -> Path:
    root_stat = os.lstat(output_root)
    if not stat.S_ISDIR(root_stat.st_mode) or stat.S_ISLNK(root_stat.st_mode):
        raise Rejected("output_root_rejected")
    run_id = str(manifest["run_id"])
    final_name = str(manifest["run_fingerprint_sha256"])
    final = output_root / final_name
    temporary_name = f".prisminfer-{secrets.token_hex(16)}.tmp"
    root_descriptor = None
    temporary_descriptor = None
    winfs = None
    try:
        files = {
            "manifest.json": manifest_data,
            "evidence.jsonl": evidence_data,
            "manifest.sha256": (hashlib.sha256(manifest_data).hexdigest() + "  manifest.json\n").encode(),
            "evidence.sha256": (hashlib.sha256(evidence_data).hexdigest() + "  evidence.jsonl\n").encode(),
        }
        commit = {
            "bundle_version": "committed-evidence-v1", "run_id": run_id,
            "run_fingerprint_sha256": manifest["run_fingerprint_sha256"],
            "manifest_sha256": hashlib.sha256(manifest_data).hexdigest(),
            "evidence_sha256": hashlib.sha256(evidence_data).hexdigest(),
        }
        files["COMMIT.json"] = (json.dumps(commit, sort_keys=True, separators=(",", ":")) + "\n").encode()
        if os.name == "nt":
            sys.path.insert(0, str(Path(__file__).parent))
            import win32_handle_fs as winfs
            root_descriptor = winfs.open_directory_tree(str(output_root), writable_final=True)
            temporary_descriptor = winfs.create_relative(root_descriptor, temporary_name, directory=True)
            for name, content in files.items():
                handle = winfs.create_relative(temporary_descriptor, name, directory=False)
                try:
                    winfs.write_all(handle, content)
                finally:
                    winfs.close(handle)
            winfs.flush(temporary_descriptor)
            try:
                winfs.rename_relative(temporary_descriptor, root_descriptor, final_name)
            except FileExistsError as exc:
                raise Rejected("run_fingerprint_replay_rejected") from exc
            winfs.flush(root_descriptor)
        else:
            root_descriptor = open_posix_directory_tree(output_root)
            os.mkdir(temporary_name, 0o700, dir_fd=root_descriptor)
            temporary_descriptor = os.open(temporary_name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=root_descriptor)
            for name, content in files.items():
                fd = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=temporary_descriptor)
                try:
                    view = memoryview(content)
                    while view:
                        view = view[os.write(fd, view):]
                    os.fsync(fd)
                finally:
                    os.close(fd)
            os.fsync(temporary_descriptor)
            try:
                rename_noreplace(temporary_name, final_name, root_descriptor)
            except Rejected as exc:
                if str(exc) == "bundle_target_exists":
                    raise Rejected("run_fingerprint_replay_rejected") from exc
                raise
            os.fsync(root_descriptor)
        return final
    finally:
        if temporary_descriptor is not None:
            winfs.close(temporary_descriptor) if os.name == "nt" else os.close(temporary_descriptor)
        if root_descriptor is not None:
            winfs.close(root_descriptor) if os.name == "nt" else os.close(root_descriptor)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--validation-cell-id", required=True)
    parser.add_argument("--declared-trial-count", required=True, type=int)
    parser.add_argument("--evidence-root", required=True, type=Path)
    parser.add_argument("--raw-trials", type=Path)
    parser.add_argument("--failure-record", type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    args = parser.parse_args()
    if (args.raw_trials is None) == (args.failure_record is None):
        raise Rejected("exactly_one_evidence_input_required")
    evidence_path = args.raw_trials or args.failure_record
    assert evidence_path is not None
    evidence_root = Path(os.path.abspath(args.evidence_root))
    evidence_leaf = Path(os.path.abspath(evidence_path))
    if evidence_leaf.parent != evidence_root or ":" in evidence_leaf.name or evidence_leaf.name in (".", ".."):
        raise Rejected("evidence_root_mismatch")
    evidence_data = read_opened(evidence_leaf, MAX_EVIDENCE, "evidence")
    destination = emit_bundle(validation_cell_id=args.validation_cell_id,
                              declared_trial_count=args.declared_trial_count,
                              raw_input=evidence_data if args.raw_trials else None,
                              failure_input=evidence_data if args.failure_record else None,
                              output_root=Path(os.path.abspath(args.output_root)))
    manifest = validate_committed_bundle(destination, dt.datetime.now(dt.timezone.utc), require_fresh=True)
    print(json.dumps({"status": manifest["run_outcome"], "bundle": str(destination)}, sort_keys=True))
    return 0 if manifest["run_outcome"] == "completed" else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Rejected as exc:
        print(f"evidence_rejected:{exc}", file=sys.stderr)
        raise SystemExit(2)

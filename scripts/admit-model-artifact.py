#!/usr/bin/env python3
"""Produce strict external provenance records for the Packet A Llama GGUF."""

import argparse
import hashlib
import json
import os
import stat
import sys
from pathlib import Path


SOURCE_FILES = [
    "LICENSE", "README.md", "USE_POLICY.md", "config.json", "generation_config.json",
    "model-00001-of-00004.safetensors", "model-00002-of-00004.safetensors",
    "model-00003-of-00004.safetensors", "model-00004-of-00004.safetensors",
    "model.safetensors.index.json", "special_tokens_map.json", "tokenizer.json",
    "tokenizer_config.json",
]
ARTIFACT_SHA256 = "7b064f5842bf9532c91456deda288a1b672397a54fa729aa665952863033557c"
PRODUCER_REVISION = "bf5b95e96dac0462e2a09145ec66cae9a3f12067"
QUANTIZER_REVISION = "b5e95468b1676e1e5c9d80d1eeeb26f542a38f42"
EXPECTED_INVENTORY_SHA256 = "e49ef5d6b443965f8a1f89a4d9ccc6b01809e3b4117ce1120243772cb957df15"
EXPECTED_RETAINED_SHA256 = "ec785e66d759a9c7eb1e7c85d2e31e8293bb2fc7a6b1fee3fe4674420c0c2cd9"
EXPECTED_PRODUCER_README_SHA256 = "6eaacf655181af2b04b6583e5a338591de89cb80a4b9f6babc77e35452fd8f5e"
EXPECTED_SOURCE_SHA256 = {
    "LICENSE": "64e1b2889b7892e6bbe7a7ed5bfe6ff793c61f9d584345f8f41cf9f5cb30a369",
    "README.md": "ed0e2e86f7a40c38b793b0a04dfd993b2c1dfb34906804f96b999f0bbd8d70e6",
    "USE_POLICY.md": "a568f2ebc73cec3fd74ba2afd992d4e945a8c7a9d851f9b66163aac834b7b859",
    "config.json": "29e4c210b0d6ac178b16b2a255a568bdb23b581e50ca1ef6a6d071dd85704e6e",
    "generation_config.json": "189fb0c0d7fd8a527db217c0a60a0e013f0394cd8800f9697a666a9e75e5f7fd",
    "model-00001-of-00004.safetensors": "2b1879f356aed350030bb40eb45ad362c89d9891096f79a3ab323d3ba5607668",
    "model-00002-of-00004.safetensors": "09d433f650646834a83c580877bd60c6d1f88f7755305c12576b5c7058f9af15",
    "model-00003-of-00004.safetensors": "fc1cdddd6bfa91128d6e94ee73d0ce62bfcdb7af29e978ddcab30c66ae9ea7fa",
    "model-00004-of-00004.safetensors": "92ecfe1a2414458b4821ac8c13cf8cb70aed66b5eea8dc5ad9eeb4ff309d6d7b",
    "model.safetensors.index.json": "146776fce3f6db1103aa6f249e65ee5544c5923ce6f971b092eee79aa6e5d37b",
    "special_tokens_map.json": "6f38c73729248f6c127296386e3cdde96e254636cc58b4169d3fd32328d9a8ec",
    "tokenizer.json": "79e3e522635f3171300913bb421464a87de6222182a0570b9b2ccba2a964b2b4",
    "tokenizer_config.json": "177c7b61e616fecb84c17ce0591acb92c6c4d60e9ac5ababfb940ff23bbcd424",
}


def open_posix_directory_tree(path: Path) -> int:
    absolute = Path(os.path.abspath(path)); descriptor = os.open("/", os.O_RDONLY | os.O_DIRECTORY)
    try:
        for component in absolute.parts[1:]:
            child = os.open(component, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=descriptor)
            os.close(descriptor); descriptor = child
        return descriptor
    except Exception:
        os.close(descriptor); raise


def opened_sha(path: Path, capture_limit: int = 0) -> tuple[str, int, bytes | None]:
    absolute = Path(os.path.abspath(path))
    if absolute.name in ("", ".", "..") or any(c in absolute.name for c in "/\\:\0"):
        raise ValueError("unsafe_source_leaf")
    if os.name == "nt":
        import msvcrt
        sys.path.insert(0, str(Path(__file__).parent)); import win32_handle_fs as winfs
        directory = winfs.open_directory_tree(str(absolute.parent), writable_final=False)
        try:
            fd = msvcrt.open_osfhandle(winfs.open_relative_handle(directory, absolute.name), os.O_RDONLY | os.O_BINARY)
        except Exception:
            winfs.close(directory); raise
    else:
        directory = open_posix_directory_tree(absolute.parent)
        try:
            fd = os.open(absolute.name, os.O_RDONLY | getattr(os, "O_BINARY", 0) | os.O_NOFOLLOW | os.O_NONBLOCK,
                         dir_fd=directory)
        except Exception:
            os.close(directory); raise
    try:
        info = os.fstat(fd)
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
            raise ValueError(f"not_regular:{path.name}")
        if capture_limit and info.st_size > capture_limit: raise ValueError(f"capture_too_large:{path.name}")
        digest = hashlib.sha256(); remaining = info.st_size; captured = bytearray() if capture_limit else None
        while remaining:
            block = os.read(fd, min(8 * 1024 * 1024, remaining))
            if not block: raise ValueError(f"short_read:{path.name}")
            digest.update(block); remaining -= len(block)
            if captured is not None: captured.extend(block)
        after = os.fstat(fd)
        if (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns) != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns):
            raise ValueError(f"changed_during_read:{path.name}")
        return digest.hexdigest(), info.st_size, bytes(captured) if captured is not None else None
    finally:
        os.close(fd)
        winfs.close(directory) if os.name == "nt" else os.close(directory)


def write_exclusive(path: Path, value: dict) -> str:
    data = (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_BINARY", 0), 0o600)
    try:
        view = memoryview(data)
        while view:
            written = os.write(fd, view)
            if written <= 0: raise OSError("provenance_short_write")
            view = view[written:]
        os.fsync(fd)
    finally:
        os.close(fd)
    written = path.read_bytes()
    if written != data: raise OSError("provenance_writeback_mismatch")
    return hashlib.sha256(written).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--artifact", required=True, type=Path)
    parser.add_argument("--inventory-directory", required=True, type=Path)
    parser.add_argument("--producer-readme", required=True, type=Path)
    parser.add_argument("--output-directory", required=True, type=Path)
    args = parser.parse_args()
    args.output_directory.mkdir(parents=True, mode=0o700, exist_ok=False)

    files = []
    for name in SOURCE_FILES:
        digest, size, captured = opened_sha(args.source / name, 1024 * 1024 if name == "tokenizer_config.json" else 0)
        if digest != EXPECTED_SOURCE_SHA256[name]: raise ValueError(f"source_identity_mismatch:{name}")
        files.append({"path": name, "bytes": size, "sha256": digest})
        if name == "tokenizer_config.json": tokenizer_config_bytes = captured
    source_manifest = {
        "manifest_version": "packet-a-source-v1", "repository": "meta-llama/Llama-3.1-8B-Instruct",
        "revision": "0e9e39f249a16976918f6564b8830bc894c89659", "files": files,
        "file_count": len(files), "total_bytes": sum(item["bytes"] for item in files),
        "chat_template_sha256": hashlib.sha256(json.loads(
            tokenizer_config_bytes.decode("utf-8"))["chat_template"].encode()).hexdigest(),
    }
    source_hash = write_exclusive(args.output_directory / "source-manifest.json", source_manifest)

    artifact_hash, artifact_bytes, _ = opened_sha(args.artifact)
    if artifact_hash != ARTIFACT_SHA256 or artifact_bytes != 4920739232:
        raise ValueError("artifact_identity_mismatch")
    inventory_hash, _, inventory_bytes = opened_sha(args.inventory_directory / "inventory.json", 8 * 1024 * 1024)
    retained_hash, _, _ = opened_sha(args.inventory_directory / "retained-slices.json")
    if inventory_hash != EXPECTED_INVENTORY_SHA256 or retained_hash != EXPECTED_RETAINED_SHA256:
        raise ValueError("inventory_identity_mismatch")
    inventory = json.loads(inventory_bytes.decode("utf-8"))
    if inventory["artifact_sha256"] != artifact_hash or inventory["retained_slice_manifest_sha256"] != retained_hash:
        raise ValueError("inventory_lineage_mismatch")
    readme_hash, _, _ = opened_sha(args.producer_readme)
    if readme_hash != EXPECTED_PRODUCER_README_SHA256: raise ValueError("producer_readme_identity_mismatch")
    receipt = {
        "receipt_version": "packet-a-imported-quant-v1",
        "producer_repository": "bartowski/Meta-Llama-3.1-8B-Instruct-GGUF",
        "producer_revision": PRODUCER_REVISION, "producer_readme_sha256": readme_hash,
        "published_lfs_sha256": ARTIFACT_SHA256, "local_opened_sha256": artifact_hash,
        "source_repository": source_manifest["repository"], "source_revision": source_manifest["revision"],
        "source_lineage_evidence": "Hub base_model tag plus matching Llama architecture/tokenizer metadata; producer parent F16 was not retained",
        "quantizer_release": "llama.cpp b3472", "quantizer_revision": QUANTIZER_REVISION,
        "quantization_recipe": "Q4_K_M", "quantization_version": 2,
        "imatrix_status": "used", "imatrix_entries": 224, "imatrix_chunks": 125,
        "producer_host_identity": "not published by producer",
        "admission_exception": "repository-owner one-time 2026-07-17 related-community-artifact transfer exception",
        "claim_boundary": "CPU/offline artifact admission only; no inference, CUDA, calibration, benchmark, or performance claim",
    }
    receipt_hash = write_exclusive(args.output_directory / "quantization-receipt.json", receipt)
    record = {
        "record_version": "packet-a-artifact-v2", "artifact_kind": "gguf",
        "artifact_filename": "Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf", "artifact_sha256": artifact_hash,
        "artifact_bytes": artifact_bytes, "source_repository": source_manifest["repository"],
        "source_revision": source_manifest["revision"], "source_manifest_sha256": source_hash,
        "producer_repository": receipt["producer_repository"], "producer_revision": PRODUCER_REVISION,
        "producer_readme_sha256": readme_hash, "quantizer_revision": QUANTIZER_REVISION,
        "quantization_recipe": "Q4_K_M", "quantization_receipt_sha256": receipt_hash,
        "inventory_sha256": inventory_hash, "retained_slice_manifest_sha256": retained_hash,
        "license_status": "accepted", "license_sha256": files[0]["sha256"],
        "use_policy_sha256": next(item["sha256"] for item in files if item["path"] == "USE_POLICY.md"),
        "lineage_status": "admitted-related-community-quant-under-one-time-exception",
        "claim_boundary": "CPU/offline artifact admission only; no inference, CUDA, calibration, benchmark, or performance claim",
    }
    record_hash = write_exclusive(args.output_directory / "artifact-record.json", record)
    print(json.dumps({"artifact_sha256": artifact_hash, "source_manifest_sha256": source_hash,
                      "quantization_receipt_sha256": receipt_hash, "artifact_record_sha256": record_hash,
                      "inventory_sha256": inventory_hash, "retained_slice_manifest_sha256": retained_hash}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

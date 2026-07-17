#!/usr/bin/env python3
"""Handle-bound byte, GGUF, inventory, and provenance DAG verifier for #80."""

import argparse
import hashlib
import json
import os
import stat
import subprocess
import sys
from pathlib import Path


def safe_leaf(name: str) -> None:
    if not name or name in (".", "..") or Path(name).name != name or any(c in name for c in "/\\:\0"):
        raise ValueError("unsafe_relative_leaf")


def read_relative(root: Path, name: str, maximum: int) -> bytes:
    safe_leaf(name)
    if os.name == "nt":
        sys.path.insert(0, str(Path(__file__).parent)); import win32_handle_fs as winfs
        handle = winfs.open_directory_tree(str(root), writable_final=False)
        try: return winfs.open_relative(handle, name, maximum)
        finally: winfs.close(handle)
    directory = os.open(root, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW, dir_fd=directory)
        try:
            info = os.fstat(fd)
            if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or info.st_size > maximum: raise ValueError("file rejected")
            data = b""
            while len(data) < info.st_size:
                chunk = os.read(fd, min(1024 * 1024, info.st_size - len(data)))
                if not chunk: raise ValueError("relative_file_short_read")
                data += chunk
            after = os.fstat(fd)
            if (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns) != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns):
                raise ValueError("relative_file_changed_during_read")
            return data
        finally: os.close(fd)
    finally: os.close(directory)


def hash_artifact(root: Path, name: str):
    safe_leaf(name)
    if os.name == "nt":
        sys.path.insert(0, str(Path(__file__).parent)); import win32_handle_fs as winfs
        handle = winfs.open_directory_tree(str(root), writable_final=False)
        try: return winfs.hash_relative(handle, name)
        finally: winfs.close(handle)
    directory = os.open(root, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW, dir_fd=directory)
        try:
            info = os.fstat(fd)
            if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1: raise ValueError("artifact_not_regular_single_link")
            digest = hashlib.sha256(); prefix = b""; remaining = info.st_size
            while remaining:
                chunk = os.read(fd, min(8 * 1024 * 1024, remaining))
                if not chunk: raise ValueError("artifact_short_read")
                digest.update(chunk); remaining -= len(chunk)
                if len(prefix) < 4: prefix += chunk[:4-len(prefix)]
            after = os.fstat(fd)
            if (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns) != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns):
                raise ValueError("artifact_changed_during_hash")
            return digest.hexdigest(), info.st_size, prefix
        finally: os.close(fd)
    finally: os.close(directory)


def parse(data):
    def pairs(values):
        result = {}
        for key, value in values:
            if key in result: raise ValueError(f"duplicate_json_field:{key}")
            result[key] = value
        return result
    return json.loads(data.decode("utf-8"), object_pairs_hook=pairs,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(f"nonfinite_json:{value}")))
def sha(data): return hashlib.sha256(data).hexdigest()


def validate_schema(name, value):
    import importlib.util
    repo = Path(__file__).parents[1]
    spec = importlib.util.spec_from_file_location("packet_a_schema", repo / "scripts" / "validate-json-schema.py")
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    schema = module.load(repo / "schemas" / name)
    module.check(schema, value, schema)


def main():
    p = argparse.ArgumentParser(); p.add_argument("--catalog", type=Path, required=True)
    p.add_argument("--artifact-root", type=Path); p.add_argument("--provenance-root", type=Path, required=True)
    p.add_argument("--inventory-root", type=Path)
    p.add_argument("--decoder-executable", type=Path)
    p.add_argument("--eligibility", type=Path, required=True)
    p.add_argument("--small-only", action="store_true")
    args = p.parse_args()
    catalog = parse(args.catalog.read_bytes())
    eligibility_data = args.eligibility.read_bytes()
    eligibility = parse(eligibility_data)
    cell = next(value for value in catalog["cells"] if value["cell_id"] == "llama-3.1-8b-instruct-foundation")
    record_data = read_relative(args.provenance_root, "artifact-record.json", 65536)
    receipt_data = read_relative(args.provenance_root, "quantization-receipt.json", 65536)
    source_data = read_relative(args.provenance_root, "source-manifest.json", 65536)
    record, receipt, source = map(parse, (record_data, receipt_data, source_data))
    validate_schema("model_artifact_record.schema.json", record)
    validate_schema("quantization_receipt.schema.json", receipt)
    validate_schema("source_manifest.schema.json", source)
    validate_schema("ggml_tensor_eligibility.schema.json", eligibility)
    small_hashes = {"artifact_record_sha256": sha(record_data), "quantization_receipt_sha256": sha(receipt_data),
                    "source_manifest_sha256": sha(source_data), "eligibility_sha256": sha(eligibility_data)}
    if any(cell[name] != value for name, value in small_hashes.items()): raise ValueError("checked provenance content address mismatch")
    if record["artifact_sha256"] != cell["artifact_sha256"] or receipt["published_lfs_sha256"] != cell["artifact_sha256"] or receipt["local_opened_sha256"] != cell["artifact_sha256"] or record["quantization_receipt_sha256"] != small_hashes["quantization_receipt_sha256"] or record["source_manifest_sha256"] != small_hashes["source_manifest_sha256"] or record["inventory_sha256"] != cell["inventory_sha256"] or record["retained_slice_manifest_sha256"] != cell["retained_slice_manifest_sha256"] or record["producer_repository"] != receipt["producer_repository"] or record["producer_revision"] != receipt["producer_revision"] or record["producer_readme_sha256"] != receipt["producer_readme_sha256"] or record["quantizer_revision"] != receipt["quantizer_revision"] or record["source_repository"] != source["repository"] or record["source_revision"] != source["revision"]: raise ValueError("checked provenance parent edge mismatch")
    source_files = {item["path"]: item for item in source["files"]}
    expected_source_names = {"LICENSE", "README.md", "USE_POLICY.md", "config.json", "generation_config.json", "model-00001-of-00004.safetensors", "model-00002-of-00004.safetensors", "model-00003-of-00004.safetensors", "model-00004-of-00004.safetensors", "model.safetensors.index.json", "special_tokens_map.json", "tokenizer.json", "tokenizer_config.json"}
    if set(source_files) != expected_source_names or len(source_files) != source["file_count"] or sum(item["bytes"] for item in source_files.values()) != source["total_bytes"] or record["license_sha256"] != source_files["LICENSE"]["sha256"] or record["use_policy_sha256"] != source_files["USE_POLICY.md"]["sha256"]: raise ValueError("checked source manifest projection mismatch")
    projections = {"source_file_count": source["file_count"], "source_total_bytes": source["total_bytes"], "source_config_sha256": source_files["config.json"]["sha256"], "tokenizer_sha256": source_files["tokenizer.json"]["sha256"], "tokenizer_config_sha256": source_files["tokenizer_config.json"]["sha256"], "chat_template_sha256": source["chat_template_sha256"], "license_sha256": record["license_sha256"], "use_policy_sha256": record["use_policy_sha256"]}
    if any(cell[name] != value for name, value in projections.items()): raise ValueError("checked catalog projection mismatch")
    if args.small_only:
        print("Verified checked-in model provenance DAG PASS")
        return
    if args.artifact_root is None or args.inventory_root is None or args.decoder_executable is None:
        raise ValueError("external artifact verification arguments required")
    artifact_hash, artifact_bytes, magic = hash_artifact(args.artifact_root, cell["artifact_sha256"] + ".gguf")
    if artifact_hash != cell["artifact_sha256"] or artifact_bytes != 4920739232 or magic != b"GGUF": raise ValueError("artifact bytes/GGUF mismatch")
    inventory_data = read_relative(args.inventory_root, "inventory.json", 8 * 1024 * 1024)
    retained_data = read_relative(args.inventory_root, "retained-slices.json", 1024 * 1024)
    expected = {"artifact_record_sha256": sha(record_data), "quantization_receipt_sha256": sha(receipt_data),
                "source_manifest_sha256": sha(source_data), "inventory_sha256": sha(inventory_data),
                "retained_slice_manifest_sha256": sha(retained_data)}
    for field, value in expected.items():
        if cell[field] != value: raise ValueError(f"catalog DAG mismatch:{field}")
    record, receipt, inventory, retained = map(parse, (record_data, receipt_data, inventory_data, retained_data))
    source = parse(source_data)
    validate_schema("model_artifact_record.schema.json", record)
    validate_schema("quantization_receipt.schema.json", receipt)
    validate_schema("source_manifest.schema.json", source)
    validate_schema("ggml_tensor_eligibility.schema.json", eligibility)
    repo = Path(__file__).parents[1]
    decoder_sources = [repo / "tests/unit/test_real_gguf_slices.cpp",
                       repo / "include/prisminfer/kernels/ggml_q4_k_reference.h",
                       repo / "src/kernels/ggml_q4_k_reference.cpp"]
    decoder_binding = sha(":".join(sha(path.read_bytes()) for path in decoder_sources).encode())
    if eligibility["decoder_build_binding_sha256"] != decoder_binding:
        raise ValueError("decoder source binding mismatch")
    identity = subprocess.run([str(args.decoder_executable), "--identity"], check=True,
                              capture_output=True, text=True, timeout=10).stdout
    if identity != f"packet-a-real-slice-decoder-v1:{decoder_binding}\n":
        raise ValueError("decoder executable identity mismatch")
    if record["artifact_sha256"] != artifact_hash or record["quantization_receipt_sha256"] != expected["quantization_receipt_sha256"] or record["inventory_sha256"] != expected["inventory_sha256"] or record["retained_slice_manifest_sha256"] != expected["retained_slice_manifest_sha256"]: raise ValueError("artifact record DAG mismatch")
    if receipt["published_lfs_sha256"] != artifact_hash or receipt["local_opened_sha256"] != artifact_hash: raise ValueError("quantization receipt mismatch")
    if record["source_manifest_sha256"] != expected["source_manifest_sha256"] or record["producer_repository"] != receipt["producer_repository"] or record["producer_revision"] != receipt["producer_revision"] or record["producer_readme_sha256"] != receipt["producer_readme_sha256"] or record["quantizer_revision"] != receipt["quantizer_revision"] or record["source_repository"] != source["repository"] or record["source_revision"] != source["revision"]: raise ValueError("provenance parent edge mismatch")
    source_files = {item["path"]: item for item in source["files"]}
    expected_source_names = {"LICENSE", "README.md", "USE_POLICY.md", "config.json", "generation_config.json", "model-00001-of-00004.safetensors", "model-00002-of-00004.safetensors", "model-00003-of-00004.safetensors", "model-00004-of-00004.safetensors", "model.safetensors.index.json", "special_tokens_map.json", "tokenizer.json", "tokenizer_config.json"}
    if set(source_files) != expected_source_names or len(source_files) != source["file_count"] or sum(item["bytes"] for item in source_files.values()) != source["total_bytes"] or record["license_sha256"] != source_files["LICENSE"]["sha256"] or record["use_policy_sha256"] != source_files["USE_POLICY.md"]["sha256"]: raise ValueError("source manifest projection mismatch")
    source_projections = {"source_file_count": source["file_count"], "source_total_bytes": source["total_bytes"], "source_config_sha256": source_files["config.json"]["sha256"], "tokenizer_sha256": source_files["tokenizer.json"]["sha256"], "tokenizer_config_sha256": source_files["tokenizer_config.json"]["sha256"], "chat_template_sha256": source["chat_template_sha256"]}
    if any(cell[name] != value for name, value in source_projections.items()): raise ValueError("catalog source projection mismatch")
    catalog_links = ("artifact_sha256", "artifact_record_sha256", "quantization_receipt_sha256", "source_manifest_sha256", "inventory_sha256", "retained_slice_manifest_sha256", "license_sha256", "use_policy_sha256")
    record_links = {"artifact_sha256": artifact_hash, "artifact_record_sha256": sha(record_data), "quantization_receipt_sha256": sha(receipt_data), "source_manifest_sha256": sha(source_data), "inventory_sha256": sha(inventory_data), "retained_slice_manifest_sha256": sha(retained_data), "license_sha256": record["license_sha256"], "use_policy_sha256": record["use_policy_sha256"]}
    if any(cell[name] != record_links[name] for name in catalog_links): raise ValueError("catalog full lineage mismatch")
    if inventory["artifact_sha256"] != artifact_hash or inventory["tensor_type_counts"] != {"F32": 66, "Q4_K": 193, "Q6_K": 33}: raise ValueError("inventory mismatch")
    if eligibility["artifact_sha256"] != artifact_hash or eligibility["inventory_sha256"] != expected["inventory_sha256"] or eligibility["retained_slice_manifest_sha256"] != expected["retained_slice_manifest_sha256"]: raise ValueError("eligibility DAG mismatch")
    if cell["eligibility_sha256"] != sha(eligibility_data): raise ValueError("eligibility content address mismatch")
    types = {item["ggml_type"]: item for item in eligibility["observed_types"]}
    if set(types) != {"F32", "Q4_K", "Q6_K"} or any(types[name]["tensor_count"] != count for name, count in inventory["tensor_type_counts"].items()) or types["Q4_K"]["custom_path_eligible"] is not True or types["F32"]["custom_path_eligible"] is not False or types["Q6_K"]["custom_path_eligible"] is not False: raise ValueError("eligibility type/route mismatch")
    if retained["artifact_sha256"] != artifact_hash or len(retained["slices"]) != 6 or inventory["retained_slices"] != retained["slices"]: raise ValueError("retained slice mismatch")
    tensors = {item["name"]: item for item in inventory["tensors"]}; observed = set()
    for item in retained["slices"]:
        identity = (item["ggml_type"], item["position"])
        if identity in observed or identity[0] not in {"F32", "Q4_K", "Q6_K"} or identity[1] not in {"first", "last"}: raise ValueError("slice identity mismatch")
        observed.add(identity); tensor = tensors[item["tensor_name"]]
        expected_offset = tensor["offset"] if item["position"] == "first" else tensor["offset"] + tensor["length"] - item["raw_bytes"]
        if tensor["ggml_type"] != item["ggml_type"] or item["source_offset"] != expected_offset: raise ValueError("slice tensor/offset mismatch")
        raw = read_relative(args.inventory_root, item["raw_file"], 4096)
        reference = read_relative(args.inventory_root, item["reference_file"], 4096)
        if len(raw) != item["raw_bytes"] or len(reference) != item["block_elements"] * 4 or sha(raw) != item["raw_sha256"] or sha(reference) != item["reference_sha256"]: raise ValueError("slice byte/hash mismatch")
        if item["ggml_type"] == "F32":
            if raw != reference: raise ValueError("F32 slice differential mismatch")
        else:
            subprocess.run([str(args.decoder_executable), item["ggml_type"], raw.hex(), reference.hex()], check=True)
            corrupted = bytearray(reference); corrupted[:4] = b"\x00\x00\xc0\x7f"
            rejected = subprocess.run([str(args.decoder_executable), item["ggml_type"], raw.hex(), corrupted.hex()],
                                      capture_output=True, timeout=10)
            if rejected.returncode == 0:
                raise ValueError("decoder accepted corrupted differential reference")
    if observed != {(kind, position) for kind in ("F32", "Q4_K", "Q6_K") for position in ("first", "last")}: raise ValueError("slice coverage mismatch")
    print("Verified external model-cell bytes and provenance DAG PASS")


if __name__ == "__main__": main()

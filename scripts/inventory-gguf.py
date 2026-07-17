#!/usr/bin/env python3
"""Inventory one GGUF and retain deterministic per-type differential slices."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import stat
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

MAX_TENSORS = 100_000
MAX_RANK = 4
CHUNK = 8 * 1024 * 1024
UINT64_MAX = (1 << 64) - 1
MAX_METADATA_BYTES = 64 * 1024 * 1024
MAX_TENSOR_INFO_BYTES = 64 * 1024 * 1024
MAX_STRING_BYTES = 16 * 1024 * 1024
MAX_ARRAY_ELEMENTS = 300_000
PINNED_REVISION = "13f2b28b098623391b1aacfd27995e1c8b7de9a9"
PINNED_ORACLE_HASHES = {
    "gguf_reader.py": "91cb9e81f1a67f5856f012cf3f526ad9ffdb6b887d2aa66e2593ee6f63764aad",
    "quants.py": "2c927a1b3d9f0920dcf4007fb686e1b0999333e9f65ce43dcc689900c0beae8b",
}
SCALAR_SIZES = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}


def parse_preamble(handle, file_size: int) -> tuple[int, int, int]:
    if file_size < 24:
        raise ValueError("gguf_header_truncated")
    handle.seek(0)
    header = handle.read(24)
    if header[:4] != b"GGUF":
        raise ValueError("gguf_magic_rejected")
    version, tensor_count, metadata_count = struct.unpack("<IQQ", header[4:])
    if version not in (2, 3):
        raise ValueError("gguf_version_rejected")
    if tensor_count == 0 or tensor_count > MAX_TENSORS:
        raise ValueError("gguf_tensor_count_rejected")
    if metadata_count > MAX_TENSORS:
        raise ValueError("gguf_metadata_count_rejected")
    return version, tensor_count, metadata_count


def checked_tensor_length(shape: list[int], block_elements: int, block_bytes: int) -> int:
    elements = 1
    for dimension in shape:
        if dimension > UINT64_MAX // elements:
            raise ValueError("tensor_element_count_overflow")
        elements *= dimension
    if elements % block_elements:
        raise ValueError("tensor_partial_quant_block")
    blocks = elements // block_elements
    if blocks > UINT64_MAX // block_bytes:
        raise ValueError("tensor_byte_count_overflow")
    return blocks * block_bytes


def validate_spans(spans: list[tuple[int, int, str]], file_size: int, alignment: int) -> None:
    for start, end, name in spans:
        if start < 0 or end <= start or start > UINT64_MAX or end > UINT64_MAX or end > file_size:
            raise ValueError(f"tensor_bounds_rejected:{name}")
        if start % alignment:
            raise ValueError(f"tensor_alignment_rejected:{name}")
    ordered = sorted(spans)
    for previous, current in zip(ordered, ordered[1:]):
        if previous[1] > current[0]:
            raise ValueError(f"tensor_overlap:{previous[2]}:{current[2]}")


def read_exact(handle, offset: int, length: int, file_size: int) -> tuple[bytes, int]:
    if length < 0 or offset < 0 or length > file_size - offset:
        raise ValueError("gguf_container_truncated")
    handle.seek(offset); data = handle.read(length)
    if len(data) != length: raise ValueError("gguf_container_short_read")
    return data, offset + length


def read_u32(handle, offset, file_size):
    data, offset = read_exact(handle, offset, 4, file_size); return struct.unpack("<I", data)[0], offset


def read_u64(handle, offset, file_size):
    data, offset = read_exact(handle, offset, 8, file_size); return struct.unpack("<Q", data)[0], offset


def read_string(handle, offset, file_size, maximum=MAX_STRING_BYTES):
    length, offset = read_u64(handle, offset, file_size)
    if length > maximum: raise ValueError("gguf_string_length_rejected")
    data, offset = read_exact(handle, offset, length, file_size)
    try: return data.decode("utf-8"), offset
    except UnicodeDecodeError as exc: raise ValueError("gguf_string_utf8_rejected") from exc


def skip_value(handle, offset, file_size, value_type, limit, depth=0):
    if depth > 1: raise ValueError("gguf_nested_array_rejected")
    if value_type in SCALAR_SIZES:
        result = read_exact(handle, offset, SCALAR_SIZES[value_type], file_size)[1]
        if result > limit: raise ValueError("gguf_metadata_bytes_rejected")
        return result
    if value_type == 8:
        result = read_string(handle, offset, file_size)[1]
        if result > limit: raise ValueError("gguf_metadata_bytes_rejected")
        return result
    if value_type != 9: raise ValueError("gguf_metadata_type_rejected")
    element_type, offset = read_u32(handle, offset, file_size)
    count, offset = read_u64(handle, offset, file_size)
    if count > MAX_ARRAY_ELEMENTS: raise ValueError(f"gguf_array_count_rejected:{count}")
    if element_type in SCALAR_SIZES:
        total = count * SCALAR_SIZES[element_type]
        result = read_exact(handle, offset, total, file_size)[1]
        if result > limit: raise ValueError("gguf_metadata_bytes_rejected")
        return result
    if element_type != 8: raise ValueError("gguf_array_type_rejected")
    if count > max(0, limit - offset) // 8: raise ValueError("gguf_metadata_bytes_rejected")
    for _ in range(count):
        _, offset = read_string(handle, offset, file_size)
        if offset > limit: raise ValueError("gguf_metadata_bytes_rejected")
    return offset


def scan_container(handle, file_size: int):
    version, tensor_count, metadata_count = parse_preamble(handle, file_size)
    offset = 24; alignment = 32; keys = set(); metadata_limit = 24 + MAX_METADATA_BYTES
    for _ in range(metadata_count):
        key, offset = read_string(handle, offset, file_size, 1024)
        if key in keys: raise ValueError("gguf_metadata_key_duplicate")
        keys.add(key); value_type, offset = read_u32(handle, offset, file_size)
        value_start = offset
        if key == "general.alignment":
            if value_type != 4: raise ValueError("gguf_alignment_type_rejected")
            alignment, offset = read_u32(handle, offset, file_size)
        else:
            offset = skip_value(handle, offset, file_size, value_type, metadata_limit)
        if offset > metadata_limit: raise ValueError("gguf_metadata_bytes_rejected")
    if alignment < 1 or alignment > 65536 or alignment & (alignment - 1):
        raise ValueError("gguf_alignment_rejected")
    headers = []; names = set(); tensor_start = offset
    if tensor_count > MAX_TENSOR_INFO_BYTES // 32: raise ValueError("gguf_tensor_info_bytes_rejected")
    for _ in range(tensor_count):
        name, offset = read_string(handle, offset, file_size, 4096)
        if name in names: raise ValueError("tensor_name_duplicate")
        names.add(name); rank, offset = read_u32(handle, offset, file_size)
        if rank < 1 or rank > MAX_RANK: raise ValueError("tensor_rank_rejected")
        shape = []
        for _ in range(rank): dimension, offset = read_u64(handle, offset, file_size); shape.append(dimension)
        if any(value == 0 for value in shape): raise ValueError("tensor_shape_rejected")
        type_id, offset = read_u32(handle, offset, file_size)
        if type_id not in range(0, 40): raise ValueError("tensor_type_id_rejected")
        relative_offset, offset = read_u64(handle, offset, file_size)
        if offset - tensor_start > MAX_TENSOR_INFO_BYTES: raise ValueError("gguf_tensor_info_bytes_rejected")
        headers.append({"name": name, "shape": shape, "type_id": type_id, "relative_offset": relative_offset})
    data_offset = (offset + alignment - 1) & ~(alignment - 1)
    if data_offset > file_size: raise ValueError("gguf_tensor_info_truncated")
    return version, tensor_count, metadata_count, alignment, data_offset, headers


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


def open_artifact_descriptor(artifact: Path):
    artifact = Path(os.path.abspath(artifact))
    if artifact.name in ("", ".", "..") or ":" in artifact.name:
        raise ValueError("artifact_leaf_rejected")
    directory_descriptor = None
    try:
        if os.name == "nt":
            import msvcrt
            sys.path.insert(0, str(Path(__file__).parent)); import win32_handle_fs as winfs
            directory_descriptor = winfs.open_directory_tree(str(artifact.parent), writable_final=False)
            descriptor = msvcrt.open_osfhandle(
                winfs.open_relative_handle(directory_descriptor, artifact.name), os.O_RDONLY | os.O_BINARY)
        else:
            directory_descriptor = open_posix_directory_tree(artifact.parent)
            descriptor = os.open(artifact.name, os.O_RDONLY | getattr(os, "O_BINARY", 0) | os.O_NOFOLLOW | os.O_NONBLOCK,
                                 dir_fd=directory_descriptor)
        info = os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
            os.close(descriptor); raise ValueError("artifact_not_regular_single_link")
        return descriptor, directory_descriptor, artifact
    except Exception:
        if directory_descriptor is not None:
            winfs.close(directory_descriptor) if os.name == "nt" else os.close(directory_descriptor)
        raise


def materialize_verified_oracle(path: Path):
    root = path.parent
    revision = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], check=True,
                              capture_output=True, text=True).stdout.strip()
    if revision != PINNED_REVISION: raise ValueError("gguf_oracle_revision_rejected")
    path_prefix = path.relative_to(root).as_posix()
    prefix = path_prefix + "/gguf/"
    names = subprocess.run(
        ["git", "-C", str(root), "ls-tree", "-r", "--name-only", PINNED_REVISION, "--", prefix],
        check=True, capture_output=True, text=True).stdout.splitlines()
    names = [name for name in names if name.startswith(prefix) and name.endswith(".py")]
    if not names: raise ValueError("gguf_oracle_tree_rejected")
    temporary = tempfile.TemporaryDirectory(prefix="prisminfer-gguf-oracle-")
    oracle = Path(temporary.name)
    for repository_name in names:
        relative = repository_name[len(path_prefix) + 1:]
        target = oracle / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        content = subprocess.run(["git", "-C", str(root), "show", f"{PINNED_REVISION}:{repository_name}"],
                                 check=True, capture_output=True).stdout
        target.write_bytes(content)
    observed = {}
    for name, expected in PINNED_ORACLE_HASHES.items():
        digest = hashlib.sha256((oracle / "gguf" / name).read_bytes()).hexdigest()
        if digest != expected: raise ValueError(f"gguf_oracle_hash_rejected:{name}")
        observed[name] = digest
    return temporary, oracle, observed


def sha256_range(handle, offset: int, length: int) -> str:
    digest = hashlib.sha256()
    handle.seek(offset)
    remaining = length
    while remaining:
        block = handle.read(min(CHUNK, remaining))
        if not block:
            raise ValueError("tensor_short_read")
        digest.update(block)
        remaining -= len(block)
    return digest.hexdigest()


def write_exclusive(path: Path, data: bytes) -> None:
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_BINARY", 0), 0o600)
    try:
        view = memoryview(data)
        while view:
            view = view[os.write(descriptor, view):]
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--output-directory", required=True, type=Path)
    parser.add_argument("--gguf-python-path", required=True, type=Path)
    arguments = parser.parse_args()

    oracle_source = Path(os.path.abspath(arguments.gguf_python_path))
    oracle_temporary, oracle_path, oracle_hashes = materialize_verified_oracle(oracle_source)
    sys.path.insert(0, str(oracle_path))
    import numpy as np
    import gguf
    from gguf.quants import dequantize

    artifact = Path(os.path.abspath(arguments.artifact))
    output = arguments.output_directory.resolve()
    output.mkdir(mode=0o700, parents=True, exist_ok=False)

    descriptor, directory_descriptor, artifact = open_artifact_descriptor(artifact)
    try:
        before = os.fstat(descriptor)
        reader_handle = os.fdopen(os.dup(descriptor), "rb", buffering=0)
        version, declared_tensors, declared_metadata, scanned_alignment, scanned_data_offset, scanned_headers = scan_container(reader_handle, before.st_size)
        reader = gguf.GGUFReader(reader_handle, "r")
        if len(reader.tensors) != declared_tensors:
            raise ValueError("gguf_tensor_count_rejected")
        if int(reader.alignment) != scanned_alignment:
            raise ValueError("gguf_alignment_crosscheck_rejected")

        tensors = []
        spans = []
        by_type = {}
        artifact_digest = hashlib.sha256()
        with os.fdopen(os.dup(descriptor), "rb", buffering=0) as handle:
            handle.seek(0)
            while block := handle.read(CHUNK):
                artifact_digest.update(block)
            seen_names = set()
            for tensor, scanned in zip(reader.tensors, scanned_headers):
                shape = [int(value) for value in tensor.shape.tolist()]
                offset = int(tensor.data_offset)
                length = int(tensor.n_bytes)
                if tensor.name in seen_names:
                    raise ValueError(f"tensor_name_duplicate:{tensor.name}")
                seen_names.add(tensor.name)
                if tensor.name != scanned["name"] or shape != scanned["shape"] or int(tensor.tensor_type) != scanned["type_id"] or offset != scanned_data_offset + scanned["relative_offset"]:
                    raise ValueError(f"tensor_header_crosscheck_rejected:{tensor.name}")
                if not shape or len(shape) > MAX_RANK or any(value <= 0 for value in shape):
                    raise ValueError(f"tensor_shape_rejected:{tensor.name}")
                block_elements, block_bytes = gguf.GGML_QUANT_SIZES[tensor.tensor_type]
                if checked_tensor_length(shape, block_elements, block_bytes) != length:
                    raise ValueError(f"tensor_length_rejected:{tensor.name}")
                if length > UINT64_MAX - offset:
                    raise ValueError(f"tensor_bounds_rejected:{tensor.name}")
                spans.append((offset, offset + length, tensor.name))
                type_name = tensor.tensor_type.name
                record = {
                    "name": tensor.name, "shape": shape, "ggml_type": type_name,
                    "type_id": int(tensor.tensor_type), "offset": offset,
                    "length": length, "sha256": sha256_range(handle, offset, length),
                }
                tensors.append(record)
                by_type.setdefault(type_name, []).append((tensor, record))

        validate_spans(spans, before.st_size, int(reader.alignment))

        slices = []
        with os.fdopen(os.dup(descriptor), "rb", buffering=0) as handle:
            for type_name in sorted(by_type):
                tensor, record = sorted(by_type[type_name], key=lambda item: item[1]["name"])[0]
                block_elements, block_bytes = gguf.GGML_QUANT_SIZES[tensor.tensor_type]
                if record["length"] < block_bytes:
                    raise ValueError(f"tensor_block_too_short:{record['name']}")
                locations = [("first", record["offset"])]
                if record["length"] > block_bytes:
                    locations.append(("last", record["offset"] + record["length"] - block_bytes))
                for position, offset in locations:
                    handle.seek(offset)
                    raw = handle.read(block_bytes)
                    if len(raw) != block_bytes:
                        raise ValueError("slice_short_read")
                    raw_name = f"{type_name.lower()}-{position}.bin"
                    output_name = f"{type_name.lower()}-{position}-f32.bin"
                    values = dequantize(np.frombuffer(raw, dtype=np.uint8).reshape(1, -1), tensor.tensor_type)
                    decoded = np.asarray(values, dtype="<f4").tobytes(order="C")
                    write_exclusive(output / raw_name, raw)
                    write_exclusive(output / output_name, decoded)
                    slices.append({
                        "ggml_type": type_name, "tensor_name": record["name"],
                        "position": position, "source_offset": offset,
                        "block_elements": block_elements, "raw_bytes": block_bytes,
                        "raw_file": raw_name, "raw_sha256": hashlib.sha256(raw).hexdigest(),
                        "reference_file": output_name,
                        "reference_sha256": hashlib.sha256(decoded).hexdigest(),
                    })

        after = os.fstat(descriptor)
        if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) != (
            after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns
        ):
            raise ValueError("artifact_changed_during_inventory")
        type_counts = {name: len(values) for name, values in sorted(by_type.items())}
        retained_manifest = {
            "manifest_version": "packet-a-retained-gguf-slices-v1",
            "artifact_sha256": artifact_digest.hexdigest(),
            "selection_rule": "lexicographically-first tensor per observed type; first and final complete encoded block",
            "upstream_semantics_revision": "llama.cpp-13f2b28b098623391b1aacfd27995e1c8b7de9a9",
            "oracle_source_sha256": oracle_hashes,
            "slices": slices,
        }
        retained_encoded = (json.dumps(retained_manifest, indent=2, sort_keys=True) + "\n").encode()
        write_exclusive(output / "retained-slices.json", retained_encoded)
        inventory = {
            "inventory_version": "packet-a-gguf-inventory-v1",
            "gguf_version": version, "declared_metadata_count": declared_metadata,
            "alignment": int(reader.alignment),
            "artifact_filename": artifact.name, "artifact_bytes": before.st_size,
            "artifact_sha256": artifact_digest.hexdigest(),
            "tensor_count": len(tensors), "tensor_type_counts": type_counts,
            "tensors": sorted(tensors, key=lambda item: item["name"]),
            "retained_slices": slices,
            "retained_slice_manifest_sha256": hashlib.sha256(retained_encoded).hexdigest(),
        }
        encoded = (json.dumps(inventory, indent=2, sort_keys=True) + "\n").encode()
        write_exclusive(output / "inventory.json", encoded)
        print(json.dumps({"artifact_sha256": inventory["artifact_sha256"], "tensor_count": len(tensors), "types": type_counts}, sort_keys=True))
        return 0
    finally:
        if "reader_handle" in locals(): reader_handle.close()
        os.close(descriptor)
        if directory_descriptor is not None:
            winfs.close(directory_descriptor) if os.name == "nt" else os.close(directory_descriptor)
        oracle_temporary.cleanup()


if __name__ == "__main__":
    raise SystemExit(main())

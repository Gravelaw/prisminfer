import importlib.util
import io
import os
import random
import struct
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[2] / "scripts" / "inventory-gguf.py"
SPEC = importlib.util.spec_from_file_location("inventory_gguf", SCRIPT)
inventory = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(inventory)


class InventoryGgufTests(unittest.TestCase):
    @staticmethod
    def header(version=3, tensors=1, metadata=0):
        return b"GGUF" + struct.pack("<IQQ", version, tensors, metadata)

    @staticmethod
    def string(value):
        data = value.encode()
        return struct.pack("<Q", len(data)) + data

    def container(self, tensors=None, metadata=b"", metadata_count=0):
        tensors = tensors or [("weight", 1, [1], 0, 0)]
        body = self.header(tensors=len(tensors), metadata=metadata_count) + metadata
        for name, rank, shape, type_id, offset in tensors:
            body += self.string(name) + struct.pack("<I", rank)
            body += b"".join(struct.pack("<Q", value) for value in shape)
            body += struct.pack("<IQ", type_id, offset)
        body += b"\0" * ((-len(body)) % 32)
        return body + b"\0" * 4096

    def test_preamble_accepts_v2_and_v3(self):
        self.assertEqual(inventory.parse_preamble(io.BytesIO(self.header(2)), 24), (2, 1, 0))
        self.assertEqual(inventory.parse_preamble(io.BytesIO(self.header(3)), 24), (3, 1, 0))

    def test_preamble_rejects_magic_version_truncation_and_counts(self):
        cases = [
            (b"bad!" + self.header()[4:], 24, "magic"),
            (self.header(4), 24, "version"),
            (self.header(tensors=0), 24, "tensor_count"),
            (self.header(metadata=inventory.MAX_TENSORS + 1), 24, "metadata_count"),
            (self.header()[:12], 12, "truncated"),
        ]
        for data, size, reason in cases:
            with self.subTest(reason=reason), self.assertRaisesRegex(ValueError, reason):
                inventory.parse_preamble(io.BytesIO(data), size)

    def test_tensor_length_rejects_overflow_and_partial_blocks(self):
        self.assertEqual(inventory.checked_tensor_length([256, 2], 256, 144), 288)
        with self.assertRaisesRegex(ValueError, "partial_quant_block"):
            inventory.checked_tensor_length([255], 256, 144)
        with self.assertRaisesRegex(ValueError, "element_count_overflow"):
            inventory.checked_tensor_length([inventory.UINT64_MAX, 2], 1, 4)
        with self.assertRaisesRegex(ValueError, "byte_count_overflow"):
            inventory.checked_tensor_length([inventory.UINT64_MAX], 1, 4)

    def test_bounded_container_scan_accepts_minimal_and_alignment(self):
        data = self.container()
        result = inventory.scan_container(io.BytesIO(data), len(data))
        self.assertEqual(result[:4], (3, 1, 0, 32))
        alignment = self.string("general.alignment") + struct.pack("<II", 4, 64)
        data = self.container(metadata=alignment, metadata_count=1)
        self.assertEqual(inventory.scan_container(io.BytesIO(data), len(data))[3], 64)

    def test_bounded_container_scan_rejects_truncation_lengths_and_arrays(self):
        fixtures = [
            (self.header(metadata=1) + struct.pack("<Q", inventory.MAX_STRING_BYTES + 1), "string_length"),
            (self.header(metadata=1) + self.string("x") + struct.pack("<IIQ", 9, 0, inventory.MAX_ARRAY_ELEMENTS + 1), "array_count"),
            (self.container()[:31], "truncated"),
        ]
        for data, reason in fixtures:
            with self.subTest(reason=reason), self.assertRaisesRegex(ValueError, reason):
                inventory.scan_container(io.BytesIO(data), len(data))

    def test_bounded_container_scan_rejects_duplicate_rank_shape_type_and_alignment(self):
        duplicate = self.container(tensors=[("same", 1, [1], 0, 0), ("same", 1, [1], 0, 32)])
        bad_alignment = self.string("general.alignment") + struct.pack("<II", 4, 3)
        fixtures = [
            (duplicate, "name_duplicate"),
            (self.container(tensors=[("x", 5, [1, 1, 1, 1, 1], 0, 0)]), "rank"),
            (self.container(tensors=[("x", 1, [0], 0, 0)]), "shape"),
            (self.container(tensors=[("x", 1, [1], 99, 0)]), "type_id"),
            (self.container(metadata=bad_alignment, metadata_count=1), "alignment_rejected"),
        ]
        for data, reason in fixtures:
            with self.subTest(reason=reason), self.assertRaisesRegex(ValueError, reason):
                inventory.scan_container(io.BytesIO(data), len(data))

    def test_span_validation_rejects_bounds_alignment_and_overlap(self):
        inventory.validate_spans([(32, 64, "a"), (64, 96, "b")], 96, 32)
        for spans, reason in [
            ([(33, 64, "a")], "alignment"),
            ([(32, 97, "a")], "bounds"),
            ([(32, 80, "a"), (64, 96, "b")], "overlap"),
        ]:
            with self.subTest(reason=reason), self.assertRaisesRegex(ValueError, reason):
                inventory.validate_spans(spans, 96, 32)

    def test_deterministic_header_mutation_fuzz_is_bounded(self):
        baseline = self.container()
        rng = random.Random(0x74)
        for _ in range(256):
            mutated = bytearray(baseline)
            for _ in range(rng.randint(1, 4)):
                index = rng.randrange(min(len(mutated), 128))
                mutated[index] ^= 1 << rng.randrange(8)
            try:
                inventory.scan_container(io.BytesIO(mutated), len(mutated))
            except (ValueError, UnicodeDecodeError, struct.error):
                pass

    def test_section_budgets_reject_before_unbounded_iteration(self):
        original_metadata = inventory.MAX_METADATA_BYTES
        original_tensor = inventory.MAX_TENSOR_INFO_BYTES
        try:
            inventory.MAX_METADATA_BYTES = 16
            array = self.string("x") + struct.pack("<IIQ", 9, 8, 2) + self.string("a") + self.string("b")
            with self.assertRaisesRegex(ValueError, "metadata_bytes"):
                inventory.scan_container(io.BytesIO(self.header(metadata=1) + array), 24 + len(array))
            inventory.MAX_TENSOR_INFO_BYTES = 31
            with self.assertRaisesRegex(ValueError, "tensor_info_bytes"):
                inventory.scan_container(io.BytesIO(self.container()), len(self.container()))
        finally:
            inventory.MAX_METADATA_BYTES = original_metadata
            inventory.MAX_TENSOR_INFO_BYTES = original_tensor

    @unittest.skipUnless(os.name == "nt", "Windows held artifact leaf")
    def test_windows_artifact_leaf_reparse_is_rejected(self):
        import sys
        sys.path.insert(0, str(SCRIPT.parent))
        import win32_handle_fs as winfs
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); target = root / "target.gguf"; target.write_bytes(b"GGUF")
            link = root / "link.gguf"
            try:
                os.symlink(target, link)
            except OSError:
                self.skipTest("file symlink privilege unavailable")
            handle = winfs.open_directory_tree(str(root), writable_final=False)
            try:
                with self.assertRaises(OSError):
                    winfs.open_relative_handle(handle, link.name)
            finally:
                winfs.close(handle)

    @unittest.skipIf(os.name == "nt", "POSIX artifact leaf invariants")
    def test_posix_artifact_hardlink_and_fifo_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); target = root / "target.gguf"; target.write_bytes(b"GGUF")
            linked = root / "linked.gguf"; os.link(target, linked)
            with self.assertRaisesRegex(ValueError, "regular_single_link"):
                inventory.open_artifact_descriptor(linked)
            fifo = root / "artifact.fifo"; os.mkfifo(fifo)
            with self.assertRaisesRegex(ValueError, "regular_single_link"):
                inventory.open_artifact_descriptor(fifo)


if __name__ == "__main__":
    unittest.main()

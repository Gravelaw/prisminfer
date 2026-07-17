import importlib.util
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock


SCRIPT = Path(__file__).parents[2] / "scripts" / "admit-model-artifact.py"
SPEC = importlib.util.spec_from_file_location("admit_model_artifact", SCRIPT)
admission = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(admission)
VERIFY_SPEC = importlib.util.spec_from_file_location(
    "verify_model_artifact", Path(__file__).parents[2] / "scripts" / "verify-model-artifact.py")
verifier = importlib.util.module_from_spec(VERIFY_SPEC)
assert VERIFY_SPEC.loader is not None
VERIFY_SPEC.loader.exec_module(verifier)


class AdmissionWriteTests(unittest.TestCase):
    def test_artifact_verifier_rejects_non_leaf_paths(self):
        for value in ("../escape.bin", "sub/file.bin", "sub\\file.bin", "x:stream"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                verifier.safe_leaf(value)

    def test_write_exclusive_handles_short_writes_and_verifies_bytes(self):
        real_write = admission.os.write

        def short_write(fd, data):
            return real_write(fd, bytes(data[:3]))

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "record.json"
            with mock.patch.object(admission.os, "write", side_effect=short_write):
                digest = admission.write_exclusive(path, {"record_version": "test", "value": 7})
            self.assertEqual(digest, admission.hashlib.sha256(path.read_bytes()).hexdigest())
            self.assertTrue(path.read_bytes().endswith(b"\n"))

    @unittest.skipIf(os.name == "nt", "POSIX provenance path invariants")
    def test_posix_admission_rejects_hardlink_and_intermediate_symlink(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); target = root / "target.bin"; target.write_bytes(b"payload")
            linked = root / "linked.bin"; os.link(target, linked)
            with self.assertRaises(ValueError):
                admission.opened_sha(linked)
            real = root / "real"; real.mkdir(); (real / "leaf.bin").write_bytes(b"payload")
            alias = root / "alias"; os.symlink(real, alias, target_is_directory=True)
            with self.assertRaises(OSError):
                admission.opened_sha(alias / "leaf.bin")

    @unittest.skipIf(os.name == "nt", "POSIX short-read fault injection")
    def test_posix_relative_read_fails_on_premature_eof(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); (root / "leaf.bin").write_bytes(b"payload")
            with mock.patch.object(verifier.os, "read", return_value=b""):
                with self.assertRaisesRegex(ValueError, "short_read"):
                    verifier.read_relative(root, "leaf.bin", 100)


if __name__ == "__main__":
    unittest.main()

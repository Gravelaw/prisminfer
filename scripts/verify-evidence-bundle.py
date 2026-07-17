#!/usr/bin/env python3
"""Fail-closed consumer for Packet A committed evidence bundles."""

import argparse
import datetime as dt
import importlib.util
import json
import sys
from pathlib import Path


RUNNER = Path(__file__).with_name("evidence-runner.py")
SPEC = importlib.util.spec_from_file_location("evidence_runner", RUNNER)
runner = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(runner)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("bundle", type=Path)
    args = parser.parse_args()
    manifest = runner.validate_committed_bundle(
        args.bundle.resolve(strict=True), dt.datetime.now(dt.timezone.utc)
    )
    print(json.dumps({"status": "committed", "run_id": manifest["run_id"]}, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except runner.Rejected as exc:
        print(f"evidence_bundle_rejected:{exc}", file=sys.stderr)
        raise SystemExit(2)

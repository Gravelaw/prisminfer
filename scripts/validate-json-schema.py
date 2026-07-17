#!/usr/bin/env python3
"""Dependency-free validator for the strict JSON Schema subset used in Packet A."""

import argparse
import json
import re
from pathlib import Path


class Invalid(ValueError): pass


def load(path: Path):
    def pairs(values):
        result = {}
        for key, value in values:
            if key in result: raise Invalid(f"duplicate property: {key}")
            result[key] = value
        return result
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=pairs,
                      parse_constant=lambda value: (_ for _ in ()).throw(Invalid(f"non-finite: {value}")))


def resolve(root, reference):
    if not reference.startswith("#/"): raise Invalid(f"unsupported reference: {reference}")
    value = root
    for part in reference[2:].split("/"):
        value = value[part.replace("~1", "/").replace("~0", "~")]
    return value


def check(schema, value, root, path="$", conditional=False):
    if "$ref" in schema:
        return check(resolve(root, schema["$ref"]), value, root, path, conditional)
    if "const" in schema and value != schema["const"]: raise Invalid(f"{path}: const")
    if "enum" in schema and value not in schema["enum"]: raise Invalid(f"{path}: enum")
    kind = schema.get("type")
    matches = {
        "object": isinstance(value, dict), "array": isinstance(value, list),
        "string": isinstance(value, str), "integer": type(value) is int,
        "number": type(value) in (int, float), "boolean": type(value) is bool,
        "null": value is None,
    }
    if kind and not matches[kind]: raise Invalid(f"{path}: type {kind}")
    if isinstance(value, dict):
        required = schema.get("required", [])
        missing = [name for name in required if name not in value]
        if missing: raise Invalid(f"{path}: missing {missing}")
        properties = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            unknown = [name for name in value if name not in properties]
            if unknown: raise Invalid(f"{path}: unknown {unknown}")
        for name, child in properties.items():
            if name in value: check(child, value[name], root, f"{path}.{name}")
    if isinstance(value, list):
        if len(value) < schema.get("minItems", 0): raise Invalid(f"{path}: minItems")
        if "maxItems" in schema and len(value) > schema["maxItems"]: raise Invalid(f"{path}: maxItems")
        if "items" in schema:
            for index, item in enumerate(value): check(schema["items"], item, root, f"{path}[{index}]")
    if isinstance(value, str):
        if len(value) < schema.get("minLength", 0): raise Invalid(f"{path}: minLength")
        if "maxLength" in schema and len(value) > schema["maxLength"]: raise Invalid(f"{path}: maxLength")
        if "pattern" in schema and re.search(schema["pattern"], value) is None: raise Invalid(f"{path}: pattern")
    if type(value) in (int, float):
        if "minimum" in schema and value < schema["minimum"]: raise Invalid(f"{path}: minimum")
        if "maximum" in schema and value > schema["maximum"]: raise Invalid(f"{path}: maximum")
    for child in schema.get("allOf", []): check(child, value, root, path)
    if "oneOf" in schema:
        accepted = 0
        for child in schema["oneOf"]:
            try: check(child, value, root, path); accepted += 1
            except Invalid: pass
        if accepted != 1: raise Invalid(f"{path}: oneOf matched {accepted}")
    if "if" in schema:
        try: check(schema["if"], value, root, path, True); condition = True
        except Invalid: condition = False
        branch = schema.get("then" if condition else "else")
        if branch is not None: check(branch, value, root, path)


def main():
    parser = argparse.ArgumentParser(); parser.add_argument("schema", type=Path); parser.add_argument("document", type=Path)
    args = parser.parse_args(); schema = load(args.schema); document = load(args.document)
    check(schema, document, schema); print("JSON Schema PASS")


if __name__ == "__main__":
    main()

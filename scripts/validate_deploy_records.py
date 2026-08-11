#!/usr/bin/env python3
"""Validate the release-record examples against the shipped JSON Schemas."""
from __future__ import annotations

import json
from pathlib import Path
import sys

try:
    import jsonschema
except ImportError as exc:  # pragma: no cover - developer environment dependency
    raise SystemExit("jsonschema is required: python -m pip install 'jsonschema>=4,<5'") from exc

ROOT = Path(__file__).resolve().parents[1]
PAIRS = (
    ("deploy/deployment-manifest.schema.json", "deploy/deployment-manifest.example.json"),
    ("deploy/qualification-record.schema.json", "deploy/qualification-record.example.json"),
)

for schema_name, example_name in PAIRS:
    schema = json.loads((ROOT / schema_name).read_text(encoding="utf-8"))
    example = json.loads((ROOT / example_name).read_text(encoding="utf-8"))
    validator = jsonschema.Draft202012Validator(schema, format_checker=jsonschema.FormatChecker())
    errors = sorted(validator.iter_errors(example), key=lambda error: list(error.absolute_path))
    if errors:
        for error in errors:
            path = ".".join(str(part) for part in error.absolute_path) or "<root>"
            print(f"{example_name}:{path}: {error.message}", file=sys.stderr)
        raise SystemExit(1)

print("deployment record schemas: PASS")

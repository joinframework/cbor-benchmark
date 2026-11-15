#!/usr/bin/env python3

import json
import struct
import sys
from pathlib import Path

import cbor2


DATASETS = ["canada", "citm_catalog", "twitter"]


def head(major: int, length: int) -> bytes:
    if length < 24:
        return bytes([major << 5 | length])
    for info, fmt in ((24, ">B"), (25, ">H"), (26, ">I"), (27, ">Q")):
        if length < 1 << (8 * struct.calcsize(fmt)):
            return bytes([major << 5 | info]) + struct.pack(fmt, length)
    raise ValueError(length)


def encode(value) -> bytes:
    if isinstance(value, dict):
        return head(5, len(value)) + b"".join(encode(k) + encode(v) for k, v in value.items())
    if isinstance(value, list):
        return head(4, len(value)) + b"".join(encode(v) for v in value)
    if isinstance(value, float):
        return cbor2.dumps(value, canonical=True)
    return cbor2.dumps(value)


def main() -> None:
    project_dir = Path(__file__).resolve().parent.parent
    json_dir    = Path(sys.argv[1]) if len(sys.argv) > 1 else project_dir.parent / "json-benchmark" / "data"
    output_dir  = Path(sys.argv[2]) if len(sys.argv) > 2 else project_dir / "data"

    for name in DATASETS:
        source = json_dir / f"{name}.json"
        target = output_dir / f"{name}.cbor"
        with open(source, encoding="utf-8") as f:
            document = json.load(f)
        encoded = encode(document)
        if cbor2.loads(encoded) != document:
            sys.exit(f"{source}: round trip mismatch")
        target.write_bytes(encoded)
        print(f"  {source.name} ({source.stat().st_size} bytes) -> {target.name} ({len(encoded)} bytes)")


if __name__ == "__main__":
    main()

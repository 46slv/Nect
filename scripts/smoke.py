#!/usr/bin/env python3
"""Exercise the real Nect CLI/API in a new temporary directory."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

def run(exe: Path, args: list[str], data: str | None = None) -> str:
    completed = subprocess.run(
        [str(exe), *args], input=data, text=True, encoding="utf-8",
        capture_output=True, timeout=20, check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"{args}: exit {completed.returncode}: {completed.stderr}")
    return completed.stdout

def atomic_new_file(target: Path, text: str) -> None:
    if target.exists():
        raise FileExistsError(target)
    scratch = target.with_suffix(target.suffix + ".tmp")
    try:
        with scratch.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(scratch, target)
    finally:
        if scratch.exists():
            scratch.unlink()

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    args = parser.parse_args()
    exe = args.exe.resolve(strict=True)

    output = Path(tempfile.mkdtemp(prefix="nect-smoke-"))
    original = run(exe, ["--demo"])
    source = output / "source.nect.json"
    atomic_new_file(source, original)
    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()

    a = {"object": "path-A", "point": "point-A1", "field": "x"}
    b = {"object": "path-B", "point": "point-B1", "field": "x"}
    commands = [
        {"op": "apply", "expected_revision": 0, "commands": [
            {"type": "link", "target": b, "binding": {
                "source": a, "scale": 2, "offset": 5, "mode": "copy_local_value"}}]},
        {"op": "apply", "expected_revision": 1, "commands": [
            {"type": "set", "ref": a, "value": 180}]},
        {"op": "get", "ref": b},
        {"op": "inspect"},
        {"op": "undo", "expected_revision": 2},
        {"op": "get", "ref": b},
        {"op": "redo", "expected_revision": 3},
        {"op": "get", "ref": b},
    ]

    responses = [json.loads(line) for line in run(
        exe, ["--serve", str(source)],
        "\n".join(json.dumps(command) for command in commands) + "\n",
    ).splitlines()]

    if len(responses) != len(commands) or not all(x.get("ok") for x in responses):
        raise AssertionError(responses)

    actual = [responses[i]["result"]["evaluated"] for i in (2, 5, 7)]
    if actual != [365, 205, 365]:
        raise AssertionError(f"Expected [365,205,365], got {actual}")

    edited = json.dumps(responses[3]["result"], ensure_ascii=False, indent=2) + "\n"
    target = output / "edited.nect.json"
    atomic_new_file(target, edited)

    validated = json.loads(run(exe, ["--validate"], target.read_text(encoding="utf-8")))
    if not validated.get("ok"):
        raise AssertionError(validated)

    svg = run(exe, ["--svg"], target.read_text(encoding="utf-8"))
    root = ET.fromstring(svg)
    paths = root.findall(".//{http://www.w3.org/2000/svg}path")
    if len(paths) != 2 or not paths[0].attrib["d"].startswith("M 180 150 C "):
        raise AssertionError("Edited first curve missing")
    if not paths[1].attrib["d"].startswith("M 365 300 C "):
        raise AssertionError("Binding was not reevaluated")

    atomic_new_file(output / "edited.svg", svg)

    if hashlib.sha256(source.read_bytes()).hexdigest() != source_hash:
        raise AssertionError("Source file changed")

    report = {
        "status": "PASS",
        "scope": "M0 headless real-process workflow",
        "source_unchanged": True,
        "binding_values_edit_undo_redo": actual,
        "fresh_process_native_validate": True,
        "fresh_process_svg_anchor_checks": True,
        "mcp_tested": False,
        "gui_tested": False,
    }
    atomic_new_file(output / "report.json", json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(report, ensure_ascii=False, indent=2))

if __name__ == "__main__":
    main()

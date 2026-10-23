#!/usr/bin/env python3
"""Seed and qualify the R12 Q1 Canvas performance fixture through the desktop API."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from session_client import call as desktop_call  # noqa: E402

SEED = 18301
PATH_COUNT = 80
POINTS_PER_PATH = 4


def canonical_bytes(value: object) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True,
                      separators=(",", ":"), allow_nan=False).encode("utf-8")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def point(point_id: str, x: float, y: float) -> dict[str, object]:
    return {
        "id": point_id,
        "x": {"literal": x},
        "y": {"literal": y},
        "in_angle": {"literal": 180},
        "in_length": {"literal": 18},
        "out_angle": {"literal": 0},
        "out_length": {"literal": 18},
    }


def command_packet(composition_id: str) -> dict[str, object]:
    rng = random.Random(SEED)
    commands: list[dict[str, object]] = []
    for index in range(PATH_COUNT):
        offset_x = 0 if index == 0 else rng.randint(-6, 6)
        offset_y = 0 if index == 0 else rng.randint(-6, 6)
        x = float(70 + (index % 10) * 84 + offset_x)
        y = float(70 + (index // 10) * 66 + offset_y)
        suffix = str(index)
        points = [
            point(f"bench-point-{suffix}-{point_index}",
                  x + point_index * 25,
                  y + (-18 if point_index == 1 else 18 if point_index == 2 else 0))
            for point_index in range(POINTS_PER_PATH)
        ]
        commands.append({
            "type": "create_path",
            "composition": composition_id,
            "parent": "",
            "id": f"bench-path-{suffix}",
            "name": f"Benchmark path {suffix}",
            "contours": [{"id": f"bench-contour-{suffix}", "closed": False, "points": points}],
        })
    return {
        "schema": "nect-perf-q1-command-stream-1",
        "seed": SEED,
        "config": {
            "path_count": PATH_COUNT,
            "points_per_path": POINTS_PER_PATH,
            "layout": "10 columns x 8 rows; 84 du horizontal and 66 du vertical spacing",
            "path": "four cubic anchors with 18 du polar handles; first path fixed for benchmark input coordinates",
        },
        "request": {"op": "apply", "expected_revision": 0, "commands": commands},
    }


def fail_if_not_ok(response: dict[str, object], label: str) -> dict[str, object]:
    if response.get("ok") is not True:
        raise RuntimeError(f"{label} failed: {json.dumps(response, ensure_ascii=False, sort_keys=True)}")
    return response


def identity_call(endpoint: str, identity: dict[str, str], operation: str,
                  **fields: object) -> dict[str, object]:
    response = desktop_call(endpoint, {
        "op": operation,
        "session_id": identity["session_id"],
        "document_id": identity["document_id"],
        **fields,
    })
    return fail_if_not_ok(response, operation)


def core_call(endpoint: str, identity: dict[str, str], request: dict[str, object]) -> dict[str, object]:
    return identity_call(endpoint, identity, "core", request=request)


def launch_desktop(executable: Path, run_dir: Path) -> tuple[subprocess.Popen[bytes], str, dict[str, object]]:
    run_dir.mkdir(parents=True, exist_ok=True)
    endpoint = "nect-perf-q1-" + uuid.uuid4().hex
    ready = run_dir / "ready.json"
    args = [str(executable), "--automation-endpoint", endpoint,
            "--recovery-dir", str(run_dir / "recovery"), "--ready-file", str(ready)]
    env = dict(os.environ)
    env["QT_QPA_PLATFORM"] = "offscreen"
    process = subprocess.Popen(args, cwd=ROOT, env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    deadline = time.monotonic() + 15
    try:
        while not ready.exists():
            if process.poll() is not None:
                error = process.stderr.read().decode("utf-8", errors="replace") if process.stderr else ""
                raise RuntimeError(f"Desktop exited before API readiness ({process.returncode}): {error}")
            if time.monotonic() >= deadline:
                raise TimeoutError("Desktop did not publish its ready file within 15 seconds")
            time.sleep(0.02)
        ready_info = json.loads(ready.read_text(encoding="utf-8"))
        hello = desktop_call(endpoint, {"op": "hello"})
        fail_if_not_ok(hello, "hello")
        if ready_info.get("session_id") != hello.get("session_id") or \
                ready_info.get("document_id") != hello.get("document_id"):
            raise RuntimeError("Ready-file identity does not match the live API identity")
        return process, endpoint, hello
    except Exception:
        stop_desktop(process)
        raise


def stop_desktop(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def api_identity(response: dict[str, object]) -> dict[str, str]:
    return {"session_id": str(response["session_id"]), "document_id": str(response["document_id"])}


def save_native(endpoint: str, identity: dict[str, str], revision: int, path: Path) -> dict[str, object]:
    response = identity_call(endpoint, identity, "save", expected_revision=revision, path=str(path.resolve()))
    if response.get("revision") != revision:
        raise RuntimeError(f"Save changed Session revision: {response}")
    if not path.is_file():
        raise RuntimeError(f"Public save did not create the requested native file: {path}")
    return response


def inspect_document(endpoint: str, identity: dict[str, str]) -> dict[str, object]:
    response = core_call(endpoint, identity, {"op": "inspect"})
    result = response.get("result")
    if not isinstance(result, dict):
        raise RuntimeError("Public inspect did not return a document object")
    return result


def apply_packet(endpoint: str, identity: dict[str, str], packet: dict[str, object]) -> tuple[int, dict[str, object]]:
    request = packet["request"]
    if not isinstance(request, dict):
        raise RuntimeError("Invalid canonical command packet")
    response = core_call(endpoint, identity, request)
    revision = int(response["revision"])
    if revision != 1:
        raise RuntimeError(f"Fixture seed must commit exactly one revision, got {revision}")
    return revision, response


def seed_process(executable: Path, run_dir: Path, base: Path,
                 fixture: Path, packet: dict[str, object]) -> dict[str, object]:
    run_dir.mkdir(parents=True, exist_ok=True)
    process, endpoint, hello = launch_desktop(executable, run_dir)
    try:
        identity = api_identity(hello)
        opened = identity_call(endpoint, identity, "open", expected_revision=int(hello["revision"]),
                               path=str(base.resolve()))
        identity = api_identity(opened)
        if int(opened["revision"]) != 0:
            raise RuntimeError("Opening the deterministic baseline must start at revision zero")
        base_document = inspect_document(endpoint, identity)

        compositions = base_document.get("compositions")
        if not isinstance(compositions, list) or len(compositions) != 1:
            raise RuntimeError("The deterministic baseline must contain one Composition")
        composition_id = str(compositions[0]["id"])
        if composition_id != packet.get("composition_id", composition_id):
            raise RuntimeError("The command packet Composition identity changed")

        revision, _ = apply_packet(endpoint, identity, packet)
        save_native(endpoint, identity, revision, fixture)
        readback = inspect_document(endpoint, identity)
        native_bytes = fixture.read_bytes()
        native = json.loads(native_bytes.decode("utf-8"))
        if native != readback:
            raise RuntimeError("Native save and public inspect readback disagree")
        status = desktop_call(endpoint, {"op": "hello"})
        fail_if_not_ok(status, "post-save hello")
        if status.get("document_id") != identity["document_id"] or status.get("revision") != revision:
            raise RuntimeError("Post-save public identity/revision readback changed")
        objects = native.get("objects")
        if not isinstance(objects, list):
            raise RuntimeError("Native document has no object list")
        point_count = sum(
            len(contour.get("points", []))
            for obj in objects
            for contour in obj.get("contours", [])
        )
        if len(objects) != PATH_COUNT or point_count != PATH_COUNT * POINTS_PER_PATH:
            raise RuntimeError(f"Unexpected fixture counts: objects={len(objects)} points={point_count}")
        return {
            "process_id": process.pid,
            "session_id": identity["session_id"],
            "document_id": identity["document_id"],
            "composition_id": composition_id,
            "artboard_id": compositions[0]["artboards"][0]["id"],
            "starting_revision": 0,
            "final_revision": revision,
            "object_count": len(objects),
            "point_count": point_count,
            "native_sha256": sha256(native_bytes),
            "normalized_native_sha256": sha256(canonical_bytes(native)),
            "native_version": native.get("version"),
            "readback_equal": True,
        }
    finally:
        stop_desktop(process)


def fixture_manifest(executable: Path, output_dir: Path) -> dict[str, object]:
    base = output_dir / "baseline.nect"
    fixture_a = output_dir / "fixture-process-1.nect"
    fixture_b = output_dir / "fixture-process-2.nect"
    command_file = output_dir / "command-stream.json"
    manifest_file = output_dir / "fixture-manifest.json"

    first_run_dir = output_dir / "seed-process-1"
    first_process, first_endpoint, first_hello = launch_desktop(executable, first_run_dir)
    try:
        first_identity = api_identity(first_hello)
        save_native(first_endpoint, first_identity, int(first_hello["revision"]), base)
        base_document = inspect_document(first_endpoint, first_identity)
    finally:
        stop_desktop(first_process)

    compositions = base_document.get("compositions")
    if not isinstance(compositions, list) or len(compositions) != 1:
        raise RuntimeError("The deterministic baseline must contain one Composition")
    composition_id = str(compositions[0]["id"])
    packet = command_packet(composition_id)
    packet["composition_id"] = composition_id
    # The hashed request is the exact semantic `core.apply` request. Process-local
    # Session IDs are transport identity and are read back separately per process.
    canonical_request = packet["request"]
    command_file.write_bytes(canonical_bytes(canonical_request) + b"\n")
    command_hash = sha256(canonical_bytes(canonical_request))

    first = seed_process(executable, output_dir / "seed-process-1-replay",
                         base, fixture_a, packet)
    second = seed_process(executable, output_dir / "seed-process-2",
                          base, fixture_b, packet)
    base_bytes = base.read_bytes()
    a_bytes = fixture_a.read_bytes()
    b_bytes = fixture_b.read_bytes()
    byte_equal = a_bytes == b_bytes
    semantic_equal = canonical_bytes(json.loads(a_bytes)) == canonical_bytes(json.loads(b_bytes))
    if not byte_equal and not semantic_equal:
        raise RuntimeError("Independent API replay produced different native semantic states")
    if first["native_sha256"] != second["native_sha256"] and not semantic_equal:
        raise RuntimeError("Independent process native identities differ semantically")

    shutil.copyfile(fixture_a, output_dir / "fixture.nect")
    native = json.loads(a_bytes.decode("utf-8"))
    manifest: dict[str, object] = {
        "schema": "nect-perf-q1-fixture-manifest-1",
        "source": "public desktop JSON-lines Session API via scripts/session_client.py",
        "seed": SEED,
        "config": packet["config"],
        "command_stream_file": command_file.name,
        "command_stream_sha256": command_hash,
        "command_stream_hash_scope": "Canonical core.apply request; process-local session/document identity envelope is recorded per run and excluded.",
        "baseline_file": base.name,
        "baseline_sha256": sha256(base_bytes),
        "fixture_file": "fixture.nect",
        "fixture_sha256": sha256((output_dir / "fixture.nect").read_bytes()),
        "normalized_native_sha256": sha256(canonical_bytes(native)),
        "normalized_native_hash_scope": "Canonical sorted-key JSON of the full native document; no fields are excluded.",
        "volatile_fields_excluded": [],
        "byte_identical_second_process_replay": byte_equal,
        "semantic_second_process_replay": semantic_equal,
        "nonvolatile_fields": ["document id", "composition id", "artboard id", "object ids", "point ids", "native version", "authored geometry"],
        "processes": [first, second],
        "starting_revision": 0,
        "fixture_revision": 1,
        "object_count": first["object_count"],
        "point_count": first["point_count"],
        "document_id": native["id"],
        "composition_id": composition_id,
        "artboard_id": compositions[0]["artboards"][0]["id"],
        "native_version": native["version"],
    }
    if first["document_id"] != second["document_id"] or first["composition_id"] != second["composition_id"]:
        raise RuntimeError("Stable document/composition identity changed during independent replay")
    manifest_file.write_bytes(canonical_bytes(manifest) + b"\n")
    return manifest


def run_benchmark(benchmark: Path, output_dir: Path) -> dict[str, object]:
    fixture = output_dir / "fixture.nect"
    manifest = output_dir / "fixture-manifest.json"
    result = output_dir / "aba.json"
    command = [str(benchmark), str(result), "--q1-aba", str(fixture), str(manifest)]
    completed = subprocess.run(command, cwd=ROOT, check=False, capture_output=True, text=True)
    (output_dir / "benchmark.stdout.log").write_text(completed.stdout, encoding="utf-8")
    (output_dir / "benchmark.stderr.log").write_text(completed.stderr, encoding="utf-8")
    if not result.is_file():
        raise RuntimeError(f"Visible benchmark produced no result artifact; exit={completed.returncode}; {completed.stderr}")
    report = json.loads(result.read_text(encoding="utf-8"))
    report["process_exit_code"] = completed.returncode
    report["stdout"] = completed.stdout.strip()
    report["stderr"] = completed.stderr.strip()
    if completed.returncode != 0:
        report["qualification_status"] = "BLOCKED"
        report["blocker"] = completed.stderr.strip() or f"Benchmark exited {completed.returncode}"
        result.write_bytes(canonical_bytes(report) + b"\n")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--desktop", type=Path, required=True, help="Release nect_desktop.exe")
    parser.add_argument("--benchmark", type=Path, help="Release canvas_benchmark.exe; runs the visible A/B/A pass")
    parser.add_argument("--output-dir", type=Path, help="Owned result directory; required with --benchmark")
    args = parser.parse_args()
    desktop = args.desktop.resolve()
    benchmark = args.benchmark.resolve() if args.benchmark else None
    if not desktop.is_file():
        parser.error(f"Desktop executable does not exist: {desktop}")
    if benchmark and not benchmark.is_file():
        parser.error(f"Benchmark executable does not exist: {benchmark}")
    if benchmark and not args.output_dir:
        parser.error("--output-dir is required for a visible benchmark run")

    temporary = None
    if args.output_dir:
        output_dir = args.output_dir.resolve()
        if benchmark and not output_dir.is_relative_to((ROOT / "build").resolve()):
            parser.error("Visible Q1 outputs must stay under this repository's ignored build directory")
        output_dir.mkdir(parents=True, exist_ok=True)
    else:
        temporary = tempfile.TemporaryDirectory(prefix="nect-perf-q1-")
        output_dir = Path(temporary.name)

    try:
        manifest = fixture_manifest(desktop, output_dir)
        print(json.dumps({
            "fixture": str(output_dir / "fixture.nect"),
            "manifest": str(output_dir / "fixture-manifest.json"),
            "command_stream_sha256": manifest["command_stream_sha256"],
            "fixture_sha256": manifest["fixture_sha256"],
            "byte_identical_second_process_replay": manifest["byte_identical_second_process_replay"],
            "object_count": manifest["object_count"],
            "point_count": manifest["point_count"],
        }, ensure_ascii=False, sort_keys=True))
        if benchmark:
            report = run_benchmark(benchmark, output_dir)
            status = report.get("qualification_status", "BLOCKED")
            print(f"Q1 visible A/B/A status: {status}; result: {output_dir / 'aba.json'}")
            return 0 if status == "PASS" else 1
        return 0
    finally:
        if temporary:
            temporary.cleanup()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"perf_q1.py: {error}", file=sys.stderr)
        raise SystemExit(1)

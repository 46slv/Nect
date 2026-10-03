"""Independent cold CLI/JSON-lines contract. No desktop sockets or pixel oracle.

Usage: python3 tests/arithmetic_process_contract.py /absolute/path/to/nect
       python3 tests/arithmetic_process_contract.py /absolute/path/to/nect --discovery-only
"""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import tempfile

CSS_IDS = ["normal", "multiply", "screen", "overlay", "darken", "lighten",
           "color-dodge", "color-burn", "hard-light", "soft-light", "difference",
           "exclusion", "hue", "saturation", "color", "luminosity"]
MODES = ["linear-burn", "linear-dodge", "linear-light", "vivid-light", "pin-light",
         "hard-mix", "subtract", "divide", "darker-color", "lighter-color"]
CLASSIC = ["classic-color-burn", "classic-color-dodge", "classic-difference"]
EARLIER = [f"0.{minor}" for minor in range(1, 80)]
checks = 0
processes = 0


def check(condition, why):
    global checks
    if not condition:
        raise AssertionError(why)
    checks += 1


def rectangle(id_):
    return dict(id=id_, name=id_, kind="path", visible=True,
                compositing=dict(version=1, opacity=dict(literal=1), blend="normal",
                                 isolated=False, mask=None),
                transform=[dict(literal=v) for v in [1, 0, 0, 1, 0, 0]],
                anchor=[dict(literal=0), dict(literal=0)], transform_parent=None,
                contours=[dict(id=id_ + "-contour", closed=True, points=[
                    dict(id=id_ + "-p" + str(i), x=dict(literal=x), y=dict(literal=y),
                         in_angle=dict(literal=0), in_length=dict(literal=0),
                         out_angle=dict(literal=0), out_length=dict(literal=0))
                    for i, (x, y) in enumerate([(2, 2), (30, 2), (30, 22), (2, 22)])])],
                stack=[dict(kind="operation", operation=dict(
                    id=id_ + "-fill", type="nect.paint.fill", version=1, enabled=True,
                    parameters={c: dict(literal=v) for c, v in zip("rgba", [0.8, 0.2, 0.4, 1])},
                    composite="below", fill_rule="nonzero"))], legacy_stroke="")


def run(exe, mode, native):
    global processes
    processes += 1
    return subprocess.run([exe, mode], input=json.dumps(native), text=True,
                          capture_output=True, timeout=20)


def serve(exe, path, commands):
    global processes
    processes += 1
    before = path.read_bytes()
    before_stat = path.stat()
    completed = subprocess.run([exe, "--serve", str(path)],
                               input="\n".join(map(json.dumps, commands)) + "\n",
                               text=True, capture_output=True, timeout=30)
    check(completed.returncode == 0, completed.stderr)
    replies = [json.loads(line) for line in completed.stdout.splitlines()]
    check(len(replies) == len(commands), "One complete JSON reply per cold request")
    after_stat = path.stat()
    check(path.read_bytes() == before
          and (after_stat.st_dev, after_stat.st_ino, after_stat.st_mtime_ns)
          == (before_stat.st_dev, before_stat.st_ino, before_stat.st_mtime_ns),
          "Serve leaves source file bytes and identity unchanged")
    return replies


def edit(object_, blend):
    return dict(type="set_compositing", object=object_, blend=blend, isolated=False)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("exe")
    parser.add_argument("--discovery-only", action="store_true")
    args = parser.parse_args()
    exe = str(Path(args.exe).resolve())
    global processes
    processes += 1
    demo = subprocess.run([exe, "--demo"], text=True, capture_output=True, timeout=20)
    check(demo.returncode == 0, demo.stderr)
    sample = json.loads(demo.stdout)
    comp = sample["compositions"][0]["id"]
    board = sample["compositions"][0]["artboards"][0]["id"]
    sample["compositions"][0]["artboards"][0].update(x=0, y=0, width=64, height=64)
    sample["objects"] = sorted([rectangle("backdrop")] + [rectangle(m) for m in MODES],
                               key=lambda obj: obj["id"])
    sample["compositions"][0]["roots"] = ["backdrop"] + MODES
    with tempfile.TemporaryDirectory(prefix="nect-arithmetic-process-") as tmp:
        path = Path(tmp) / "source.nect"
        path.write_text(json.dumps(sample), encoding="utf-8")
        discovery = serve(exe, path, [dict(op="compositing_types")])[0]
        check(discovery["ok"], "Cold discovery succeeds")
        check(discovery["result"]["blends"] == CSS_IDS + MODES,
              "Cold registry is the exact independently frozen26 IDs")
        descriptors = discovery["result"]["blend_descriptors"]
        check([d["id"] for d in descriptors] == CSS_IDS + MODES,
              "Cold descriptors match exact registry order")
        for descriptor in descriptors[16:]:
            check(descriptor["introduced_native_version"] == "0.80"
                  and descriptor["svg"]["representation"] == "unsupported"
                  and descriptor["ae_oracle_status"] == "unverified",
                  "Arithmetic discovery is native0.80 / SVG unsupported / AE unverified")
        if args.discovery_only:
            print(json.dumps(dict(status="PASS", checks=checks, cold_processes=processes,
                                  scope="arithmetic discovery only"), sort_keys=True))
            return
        check(sample["version"] == "0.80", "Current isolated cold writer0.80")
        commands = [dict(op="apply", expected_revision=0, commands=[edit(m, m) for m in MODES]),
                    dict(op="inspect"), dict(op="undo", expected_revision=1), dict(op="inspect"),
                    dict(op="redo", expected_revision=2), dict(op="inspect"),
                    dict(op="apply", expected_revision=3,
                         commands=[edit(MODES[0], "normal"), edit(MODES[-1], "future-mode")]),
                    dict(op="inspect"),
                    dict(op="apply", expected_revision=3,
                         commands=[dict(edit(MODES[0], MODES[0]), profile="linear-srgb16")]),
                    dict(op="inspect"),
                    dict(op="apply", expected_revision=99, commands=[edit(MODES[0], "normal")]),
                    dict(op="inspect"), dict(op="export_plan", composition=comp, artboard=board)]
        for forbidden in CLASSIC + ["Linear-Dodge", "linear_dodge", "linear-dodge ", "plus-lighter"]:
            commands.extend([dict(op="apply", expected_revision=3,
                                  commands=[edit(MODES[0], "normal"), edit(MODES[-1], forbidden)]),
                             dict(op="inspect")])
        replies = serve(exe, path, commands)
        check(replies[0]["ok"] and replies[0]["revision"] == 1, "All ten cold edits use one revision")
        authored = replies[1]["result"]
        expected = copy.deepcopy(sample)
        for obj in expected["objects"]:
            if obj["id"] in MODES:
                obj["compositing"]["blend"] = obj["id"]
        check(authored == expected, "Cold inspect retains exact native authored IDs and all other fields")
        check(replies[2]["ok"] and replies[3]["result"] == sample, "Cold Undo exact source")
        check(replies[4]["ok"] and replies[5]["result"] == authored, "Cold Redo exact source")
        for index, code in [(6, "UNSUPPORTED_BLEND"), (8, "UNSUPPORTED_BLEND_PROFILE"),
                            (10, "REVISION_CONFLICT")]:
            check(not replies[index]["ok"] and replies[index]["error"]["code"] == code
                  and replies[index]["revision"] == 3 and replies[index + 1]["result"] == authored,
                  "Cold refusal preserves source and revision: " + code)
        plan = replies[12]["result"]
        check(replies[12]["ok"] and plan["svg_export_supported"] is False,
              "Cold export plan refuses arithmetic SVG")
        entries = plan["unsupported_blends"]
        check(sorted((e["object"], e["blend"]) for e in entries) == sorted((m, m) for m in MODES)
              and all(isinstance(e["reason"], str) and e["reason"] for e in entries),
              "Cold plan lists all ten exact Object/blend/reason entries")
        for index in range(13, len(replies), 2):
            check(not replies[index]["ok"] and replies[index]["error"]["code"] == "UNSUPPORTED_BLEND"
                  and replies[index]["revision"] == 3 and replies[index + 1]["result"] == authored,
                  "Cold failed-later Classic/alias command is atomic")
        normalized = run(exe, "--normalize", authored)
        check(normalized.returncode == 0 and json.loads(normalized.stdout) == authored,
              "Separate cold normalize preserves exact native0.80 authoring")
        for mode in MODES:
            single = copy.deepcopy(sample)
            target = next(o for o in single["objects"] if o["id"] == mode)
            target["compositing"]["blend"] = mode
            svg = run(exe, "--svg", single)
            check(svg.returncode == 2 and "UNSUPPORTED_SVG_BLEND" in svg.stderr and svg.stdout == "",
                  "CLI SVG refuses before writing any output bytes: " + mode)
            for version in EARLIER:
                lie = copy.deepcopy(single)
                lie["version"] = version
                refusal = run(exe, "--normalize", lie)
                check(refusal.returncode == 2 and "NATIVE_VERSION_MISMATCH" in refusal.stderr
                      and refusal.stdout == "", "Cold older-version exact ID refusal: " + mode + "/" + version)
        # Real cold Session creates hidden Definition sources, not manually
        # fabricated proxy objects. Their native gates apply even without use.
        hidden_path = Path(tmp) / "hidden.nect"
        hidden_path.write_text(json.dumps(authored), encoding="utf-8")
        hidden = serve(exe, hidden_path, [
            dict(op="apply", expected_revision=0, commands=[
                dict(type="create_definition", id="def-" + m, name="D " + m, root=m)
                for m in MODES] + [dict(type="set_visibility", object=m, visible=False) for m in MODES]),
            dict(op="inspect")])
        check(hidden[0]["ok"], "Cold Session authors hidden definitions")
        hidden_native = hidden[1]["result"]
        for mode in MODES:
            lie = copy.deepcopy(hidden_native)
            for obj in lie["objects"]:
                if obj["id"] in MODES and obj["id"] != mode:
                    obj["compositing"]["blend"] = "normal"
            lie["version"] = "0.79"
            refusal = run(exe, "--normalize", lie)
            check(refusal.returncode == 2 and "NATIVE_VERSION_MISMATCH" in refusal.stderr
                  and refusal.stdout == "", "Cold hidden Definition0.79 gate: " + mode)
        for mode in CSS_IDS:
            supported = copy.deepcopy(sample)
            supported["objects"][0]["compositing"]["blend"] = mode
            svg = run(exe, "--svg", supported)
            check(svg.returncode == 0 and svg.stdout.startswith("<svg ") and svg.stdout.endswith("</svg>\n"),
                  "All inherited CSS modes still export: " + mode)
        check(path.read_text(encoding="utf-8") == json.dumps(sample), "Cold work never rewrites the native source file")
    print(json.dumps(dict(status="PASS", checks=checks, cold_processes=processes, native="0.80",
                          scope="cold CLI/JSON-lines native/Session/SVG refusal", live_desktop_ipc=False),
                     sort_keys=True))


if __name__ == "__main__":
    main()

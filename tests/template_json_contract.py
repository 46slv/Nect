"""Black-box JSON-lines Template lifecycle checks against the rebuilt CLI."""
import atexit
import json
from pathlib import Path
import subprocess
import sys
import tempfile

EXE = str(Path(sys.argv[1]).resolve())
checks = 0


def check(condition, message):
    global checks
    if not condition:
        raise AssertionError(message)
    checks += 1


def command(exe, *args, input_text=None):
    return subprocess.run([exe, *args], input=input_text, text=True,
        capture_output=True, encoding="utf-8", timeout=20)


SERVERS = {}


def stop_servers():
    for process in SERVERS.values():
        if process.poll() is None:
            process.stdin.close()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=5)


atexit.register(stop_servers)


def serve(path, requests):
    key = str(path.resolve())
    process = SERVERS.get(key)
    if process is None:
        process = subprocess.Popen([EXE, "--serve", str(path)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", bufsize=1)
        SERVERS[key] = process
    replies = []
    for request in requests:
        process.stdin.write(json.dumps(request, separators=(",", ":")) + "\n")
        process.stdin.flush()
        line = process.stdout.readline()
        if not line:
            raise AssertionError("JSON-lines server exited before replying: " + process.stderr.read())
        replies.append(json.loads(line))
    check(process.poll() is None and len(replies) == len(requests),
        "One persistent JSON-lines process returns one response per request")
    return replies


def close_server(path):
    process = SERVERS.pop(str(path.resolve()))
    process.stdin.close()
    result = process.wait(timeout=10)
    stderr = process.stderr.read()
    check(result == 0, "JSON-lines server closes cleanly: " + stderr)


def expected_error(reply, code, revision):
    check(not reply["ok"] and reply["error"]["code"] == code and reply["revision"] == revision,
        f"Expected {code} at unchanged revision {revision}, received {reply}")


sample_result = command(EXE, "--demo")
check(sample_result.returncode == 0, "Rebuilt CLI produces its demo native document")
sample = json.loads(sample_result.stdout)
composition = sample["compositions"][0]
source_artboard = composition["artboards"][0]
source_id = source_artboard["id"]
composition_id = composition["id"]
target_id = "template-target"
source_artboard["name"] = "Same display name"
composition["artboards"].append(dict(id=target_id, name="Same display name", x=900, y=50,
    width=640, height=480))
if "templates" not in composition:
    composition["templates"] = []

with tempfile.TemporaryDirectory(prefix="nect-template-json-") as directory:
    root = Path(directory)
    original = root / "source.nect"
    original.write_text(json.dumps(sample, separators=(",", ":")), encoding="utf-8")
    original_bytes = original.read_bytes()

    create = dict(type="create_artboard_template", composition=composition_id, id="template-one",
        name="Shared Template", source_artboard=source_id, definition=None)
    rename = dict(type="rename_artboard_template", composition=composition_id, template="template-one",
        name="Renamed Shared Template")
    assign = dict(type="assign_artboard_template", composition=composition_id, artboard=target_id,
        template="template-one", content_instance=None)
    margin = dict(left=30, top=20, right=30, bottom=20)
    grid = dict(id="template-grid-" + target_id, bounds=dict(x=20, y=20, width=760, height=440),
        columns=2, rows=2, column_gutter=20, row_gutter=20)
    first = dict(op="apply", expected_revision=0, commands=[create, rename, assign,
        dict(type="set_artboard_template_override", composition=composition_id, artboard=target_id,
            field="frame.width", value=800),
        dict(type="set_artboard_template_override", composition=composition_id, artboard=target_id,
            field="layout.margin", value=margin),
        dict(type="set_artboard_template_override", composition=composition_id, artboard=target_id,
            field="layout.grid", value=grid)])
    inspect = dict(op="inspect")
    artboards = dict(op="artboards", composition=composition_id)
    width = dict(op="get", ref=dict(object=target_id, point="", field="artboard.width"))
    margin_left = dict(op="get", ref=dict(object=target_id, point="", field="margin.left"))
    grid_columns = dict(op="get", ref=dict(object=grid["id"], point="", field="grid.columns"))
    history = dict(op="history")
    replies = serve(original, [first, inspect, artboards, width, margin_left, grid_columns, history])
    check(replies[0]["ok"] and replies[0]["result"]["changed"] and replies[0]["revision"] == 1,
        "Create, rename, assign, frame, Margin and Grid commands commit in one Session revision: " + repr(replies[0]))
    native = replies[1]["result"]
    comp = next(value for value in native["compositions"] if value["id"] == composition_id)
    template = next(value for value in comp["templates"] if value["id"] == "template-one")
    target = next(value for value in comp["artboards"] if value["id"] == target_id)
    check(template == dict(id="template-one", name="Renamed Shared Template", source_artboard=source_id,
        definition=None), "Template creation and rename preserve stable source and Template IDs")
    check(source_artboard["name"] == target["name"] == "Same display name" and
        target["template_assignment"]["template_id"] == "template-one" and
        target["template_assignment"]["content_instance"] is None,
        "Same-named Artboards do not retarget the explicitly assigned Template by display name")
    evaluated_target = next(value["evaluated"] for value in replies[2]["result"]
        if value["authored"]["id"] == target_id)
    check(evaluated_target["width"] == 800 and evaluated_target["height"] == 480 and
        evaluated_target["layout"]["margin"] == margin and evaluated_target["layout"]["grid"] == grid,
        "Artboard listing returns the authored local family overrides and evaluated frame")
    check(replies[3]["result"]["authored"]["source_kind"] == "template_override" and
        replies[3]["result"]["authored"]["template_override"] == 800 and replies[3]["result"]["evaluated"] == 800 and
        replies[4]["result"]["authored"]["literal"] == 30 and replies[4]["result"]["evaluated"] == 30 and
        replies[5]["result"]["authored"]["literal"] == 2 and
        replies[5]["result"]["evaluated"] == 2,
        "Typed JSON getters return the authored/evaluated frame and target-local family values")
    history_before_failures = replies[6]["result"]

    invalids = [
        dict(type="rename_artboard_template", composition="missing-composition", template="template-one", name="bad"),
        dict(type="rename_artboard_template", composition=composition_id, template="missing-template", name="bad"),
        dict(type="assign_artboard_template", composition=composition_id, artboard="missing-target",
            template="template-one", content_instance=None),
        dict(type="assign_artboard_template", composition="missing-composition", artboard=target_id,
            template="template-one", content_instance=None),
        dict(type="set_artboard_template_override", composition=composition_id, artboard=target_id,
            field="frame.width", value=dict(wrong="type")),
        dict(type="set_artboard_template_override", composition=composition_id, artboard=target_id,
            field="object.opacity", value=0.5),
    ]
    before_invalid = native
    requests = []
    for mutation in invalids:
        requests.append(dict(op="apply", expected_revision=1, commands=[mutation]))
    requests.extend([dict(op="inspect"), dict(op="history")])
    invalid_replies = serve(original, requests)
    expected_error(invalid_replies[0], "MISSING_COMPOSITION", 1)
    expected_error(invalid_replies[1], "MISSING_ARTBOARD_TEMPLATE", 1)
    expected_error(invalid_replies[2], "MISSING_ARTBOARD", 1)
    expected_error(invalid_replies[3], "MISSING_COMPOSITION", 1)
    expected_error(invalid_replies[4], "INVALID_REQUEST", 1)
    expected_error(invalid_replies[5], "UNSUPPORTED_TEMPLATE_OVERRIDE", 1)
    check(invalid_replies[6]["result"] == before_invalid and
        invalid_replies[7]["result"] == history_before_failures,
        "Wrong IDs, Composition IDs, value type and override domain preserve native state and History")

    stale = dict(op="apply", expected_revision=0, commands=[dict(
        type="set_artboard_template_override", composition=composition_id, artboard=target_id,
        field="frame.width", value=790)])
    valid_then_in_use = dict(op="apply", expected_revision=1, commands=[dict(
        type="set_artboard_template_override", composition=composition_id, artboard=target_id,
        field="frame.width", value=790), dict(type="delete_artboard_template",
        composition=composition_id, template="template-one")])
    requests = [stale, valid_then_in_use, dict(op="inspect"), dict(op="history")]
    failure_replies = serve(original, requests)
    expected_error(failure_replies[0], "REVISION_CONFLICT", 1)
    expected_error(failure_replies[1], "ARTBOARD_TEMPLATE_IN_USE", 1)
    check(failure_replies[2]["result"] == before_invalid and
        failure_replies[3]["result"] == history_before_failures,
        "Stale command and later failed batch preserve exact native state, revision and History")

    reset = dict(op="apply", expected_revision=1, commands=[
        dict(type="reset_artboard_template_override", composition=composition_id, artboard=target_id, field="frame.width"),
        dict(type="reset_artboard_template_override", composition=composition_id, artboard=target_id, field="layout.margin"),
        dict(type="reset_artboard_template_override", composition=composition_id, artboard=target_id, field="layout.grid")])
    reset_replies = serve(original, [reset, dict(op="inspect"), artboards,
        dict(op="get", ref=dict(object=target_id, point="", field="artboard.width"))])
    reset_native = reset_replies[1]["result"]
    reset_comp = next(value for value in reset_native["compositions"] if value["id"] == composition_id)
    reset_target = next(value for value in reset_comp["artboards"] if value["id"] == target_id)
    reset_evaluated = next(value["evaluated"] for value in reset_replies[2]["result"]
        if value["authored"]["id"] == target_id)
    check(reset_replies[0]["ok"] and reset_replies[0]["revision"] == 2 and
        reset_target["template_assignment"]["template_id"] == "template-one" and "layout" not in reset_target and
        reset_evaluated["width"] == source_artboard["width"] and
        reset_replies[3]["result"]["authored"]["source_kind"] == "template" and
        reset_replies[3]["result"]["evaluated"] == source_artboard["width"],
        "Reset removes only local frame/family overrides and resumes live Template inheritance")

    detach_delete = dict(op="apply", expected_revision=2, commands=[
        dict(type="detach_artboard_template", composition=composition_id, artboard=target_id,
            id_prefix="detached-template"),
        dict(type="delete_artboard_template", composition=composition_id, template="template-one")])
    final_replies = serve(original, [detach_delete, dict(op="inspect"), artboards])
    final_native = final_replies[1]["result"]
    final_comp = next(value for value in final_native["compositions"] if value["id"] == composition_id)
    final_target = next(value for value in final_comp["artboards"] if value["id"] == target_id)
    final_evaluated = next(value["evaluated"] for value in final_replies[2]["result"]
        if value["authored"]["id"] == target_id)
    check(final_replies[0]["ok"] and final_replies[0]["revision"] == 3 and
        final_comp["templates"] == [] and "template_assignment" not in final_target and
        final_target["width"] == source_artboard["width"] and final_evaluated["width"] == source_artboard["width"],
        "Detach materializes the inherited Artboard and permits subsequent Template deletion")

    final_path = root / "final.nect"
    final_path.write_text(json.dumps(final_native, separators=(",", ":")), encoding="utf-8")
    final_bytes = final_path.read_bytes()
    cold = serve(final_path, [dict(op="inspect"), artboards,
        dict(op="get", ref=dict(object=target_id, point="", field="artboard.width"))])
    cold_target = next(value for value in cold[1]["result"] if value["authored"]["id"] == target_id)
    close_server(final_path)
    close_server(original)
    check(cold[0]["result"] == final_native and cold_target["authored"] == final_target and
        cold_target["evaluated"] == final_evaluated and
        cold[2]["result"]["evaluated"] == source_artboard["width"] and final_path.read_bytes() == final_bytes,
        "A fresh JSON-lines process cold-opens the detached native state with identical typed state and stable bytes")
    check(original.read_bytes() == original_bytes,
        "Template Session commands never rewrite the original input fixture bytes")

print(f"PASS {checks} Template JSON-lines lifecycle and native reopen checks")

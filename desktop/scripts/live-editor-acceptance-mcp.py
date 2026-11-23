"""Local verification adapter for the supplied, hash-bound Nect acceptance build."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
from queue import Empty, Queue
from threading import Thread
import sys
import uuid

REV = "2025-06-18"
PREVIEW_ONLY = os.environ.get("NECT_MCP_PREVIEW_READ_ONLY") == "1"
BASE = Path(__file__).resolve().parent
PIPE_IDENTIFIER = os.environ.get("NECT_MCP_LOCAL_PIPE_PROOF")
if os.name != "nt":
    raise RuntimeError("A Windows acceptance host is required")
EXE = Path(os.environ["NECT_ACCEPTANCE_EXE"]).resolve(strict=True)
HASH = os.environ["NECT_ACCEPTANCE_SHA256"].lower()
if EXE.suffix.lower() != ".exe" or len(HASH) != 64:
    raise RuntimeError("A Windows acceptance build and its expected SHA-256 are required")
if PIPE_IDENTIFIER is not None:
    if not PREVIEW_ONLY:
        raise RuntimeError("The local pipe proof requires read-only preview mode")
    from live_inspect_pipe_proof_client import inspect_endpoint, inspect_once
    PIPE_ENDPOINT = inspect_endpoint(PIPE_IDENTIFIER)

def schema(props):
    return {"type": "object", "properties": props, "required": list(props), "additionalProperties": False}

ident = {"type": "string", "minLength": 1, "maxLength": 256}
revision = {"type": "integer", "minimum": 0}
target = schema(dict(documentGeneration=revision, compositionId=ident, sessionId=ident,
                     sessionRevision=revision, projectRevision=revision))
TOOLS = [
    {"name": "nect_test_inspect", "description": "Read target identity of the isolated Nect fixture",
     "inputSchema": schema({}), "annotations": {"readOnlyHint": True, "openWorldHint": False}},
    {"name": "nect_test_set_blend_distance", "description": "Set Blend Distance in the isolated fixture",
     "inputSchema": schema(dict(target=target, layerId=ident, nodeId=ident, value={"type": "number"})),
     "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False}},
]
OPERATIONS = dict(nect_test_inspect="editor.inspect", nect_test_set_blend_distance="blend.setDistance")
for suffix, operation, description, destructive in (
    ("export_svg", "artwork.exportSvg", "Write a new SVG in the isolated fixture output", False),
    ("export_png", "artwork.exportPng", "Write a new PNG in the isolated fixture output", False),
    ("save_copy", "document.saveCopy", "Save a project copy in the isolated fixture output", False),
    ("reopen_copy", "document.reopenCopy", "Reopen the exact current snapshot from the isolated copy; replaces the test editor session", True),
):
    name = "nect_test_" + suffix
    TOOLS.append({"name":name,"description":description,"inputSchema":schema({"target":target}),
                  "annotations":{"readOnlyHint":False,"destructiveHint":destructive,"openWorldHint":False}})
    OPERATIONS[name] = operation
for direction in ("undo","redo"):
    name="nect_test_"+direction
    TOOLS.append({"name":name,"description":direction.title()+" one document edit in the isolated fixture",
                  "inputSchema":schema({"target":target}),
                  "annotations":{"readOnlyHint":False,"destructiveHint":True,"openWorldHint":False}})
    OPERATIONS[name]="history."+direction
request_id = {"type":"string","pattern":"^[A-Za-z0-9_-]{1,96}$"}
for suffix, operation, description, props, readonly in (
    ("save_state", "document.saveState", "Read save epoch, pending and dirty state for the test document", {}, True),
    ("save_receipt", "document.saveReceipt", "Look up the result of an already-issued Save As request", {"requestId":request_id}, True),
    ("save_as", "document.saveAs", "Bind the test document to the fixed new path selected by the acceptance app", dict(target=target,saveEpoch=revision,requestId=request_id), False),
):
    name="nect_test_"+suffix
    TOOLS.append({"name":name,"description":description,"inputSchema":schema(props),
                  "annotations":{"readOnlyHint":readonly,"destructiveHint":not readonly,
                                 "idempotentHint":not readonly,"openWorldHint":False}})
    OPERATIONS[name]=operation
if os.environ.get("NECT_ACCEPTANCE_ARTWORK") == "1":
    name = "nect_test_create_artwork"
    coord = {"type":"number", "minimum":-1000000, "maximum":1000000}
    TOOLS.append({
        "name":name, "description":"Create one shape or default text object in the isolated fixture",
        "inputSchema":schema(dict(target=target,
            artworkType={"enum":["Circle","Ellipse","Rectangle","Polygon","Star","Text"]},
            at={"type":"array","items":coord,"minItems":2,"maxItems":2})),
        "annotations":{"readOnlyHint":False,"destructiveHint":False,"idempotentHint":False,
                       "openWorldHint":False},
    })
    OPERATIONS[name] = "artwork.create"
if os.environ.get("NECT_ACCEPTANCE_ARTWORK") == os.environ.get("NECT_ACCEPTANCE_TEXT") == "1":
    name = "nect_test_set_text_content"
    TOOLS.append({
        "name": name, "description": "Set text in the addressed object in the isolated fixture",
        "inputSchema": schema(dict(target=target, layerId=ident, nodeId=ident,
                                   text={"type":"string","maxLength":1024})),
        "annotations":{"readOnlyHint":False,"destructiveHint":False,"openWorldHint":False},
    })
    OPERATIONS[name] = "text.setContent"
if os.environ.get("NECT_ACCEPTANCE_TEXT_READ") == "1":
    name = "nect_test_read_text_content"
    TOOLS.append({
        "name": name, "description": "Read exact authored text from one addressed fixture object",
        "inputSchema": schema(dict(target=target, layerId=ident, nodeId=ident)),
        "annotations":{"readOnlyHint":True,"destructiveHint":False,"openWorldHint":False},
    })
    OPERATIONS[name] = "text.readContent"
if os.environ.get("NECT_ACCEPTANCE_BLEND_STEPS") == "1":
    name = "nect_test_set_blend_steps"
    TOOLS.append({
        "name": name,
        "description": "Set an integer Steps count in the addressed isolated Blend fixture",
        "inputSchema": schema(dict(target=target, layerId=ident, nodeId=ident,
                                   value={"type": "integer", "minimum": 0, "maximum": 254})),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "blend.setSteps"
    read_name = "nect_test_read_blend_settings"
    TOOLS.append({
        "name": read_name,
        "description": "Read only the active Blend spacing control in the addressed isolated fixture",
        "inputSchema": schema(dict(target=target, layerId=ident, nodeId=ident)),
        "annotations": {"readOnlyHint": True, "openWorldHint": False},
    })
    OPERATIONS[read_name] = "blend.settings"
    mode_name = "nect_test_set_blend_mode"
    TOOLS.append({
        "name": mode_name,
        "description": "Set a declared Blend mode without changing its input wires in the isolated fixture",
        "inputSchema": schema(dict(target=target, layerId=ident, nodeId=ident,
                                   mode={"enum": ["steps", "three-source-steps", "smooth-color",
                                                  "distance", "spine-distance", "spine-steps",
                                                  "spine-tangent-steps"]})),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[mode_name] = "blend.setMode"
    input_name = "nect_test_set_blend_input"
    TOOLS.append({
        "name": input_name,
        "description": "Connect or disconnect one active input in the addressed isolated Blend",
        "inputSchema": schema(dict(target=target, layerId=ident, nodeId=ident,
                                   input={"enum": ["a", "b", "c", "spine"]},
                                   sourceId={"anyOf": [ident, {"type": "null"}]},
                                   sourcePort={"anyOf": [ident, {"type": "null"}]})),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[input_name] = "blend.setInput"
if os.environ.get("NECT_ACCEPTANCE_OBJECTS") == "1":
    name = "nect_test_list_artwork"
    TOOLS.append({
        "name": name, "description": "List layer addresses and state, or addresses and types of authored nodes, in the isolated fixture",
        "inputSchema": schema(dict(target=target, offset=revision,
                                   layerIndex={"anyOf":[revision,{"type":"null"}]})),
        "annotations":{"readOnlyHint":True,"openWorldHint":False},
    })
    OPERATIONS[name] = "artwork.list"
if os.environ.get("NECT_ACCEPTANCE_GRAPH_WIRES") == "1":
    name = "nect_test_read_node_input_wires"
    TOOLS.append({
        "name": name,
        "description": "Read authored input wires of the addressed node in the isolated fixture",
        "inputSchema": schema(dict(target=target, layerId=ident, nodeId=ident, offset=revision)),
        "annotations": {"readOnlyHint": True, "openWorldHint": False},
    })
    OPERATIONS[name] = "graph.inputWires"
if os.environ.get("NECT_ACCEPTANCE_LAYER_STATE") == "1":
    name = "nect_test_read_layer_state"
    TOOLS.append({
        "name": name,
        "description": "Read stored visible, locked, guide and solo flags for one specific fixture layer",
        "inputSchema": schema(dict(target=target, layerId=ident)),
        "annotations": {"readOnlyHint": True, "openWorldHint": False},
    })
    OPERATIONS[name] = "layer.state"
if os.environ.get("NECT_ACCEPTANCE_LAYER_VISIBILITY") == "1":
    name = "nect_test_set_layer_visibility"
    TOOLS.append({
        "name": name,
        "description": "Explicitly set the visibility of one layer in the isolated fixture",
        "inputSchema": schema(dict(target=target, layerId=ident, visible={"type": "boolean"})),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "layer.setVisibility"
if os.environ.get("NECT_ACCEPTANCE_LAYER_SOLO") == "1":
    name = "nect_test_set_layer_solo"
    TOOLS.append({
        "name": name,
        "description": "Explicitly set the Solo state of one layer in the isolated fixture",
        "inputSchema": schema(dict(target=target, layerId=ident, solo={"type": "boolean"})),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "layer.setSolo"
if os.environ.get("NECT_ACCEPTANCE_LAYER_GUIDE") == "1":
    name = "nect_test_set_layer_guide"
    TOOLS.append({
        "name": name,
        "description": "Convert one layer to or from a Guide in the isolated fixture",
        "inputSchema": schema(dict(target=target, layerId=ident, guide={"type": "boolean"})),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "layer.setGuide"
if os.environ.get("NECT_ACCEPTANCE_LAYER_LOCK") == "1":
    name = "nect_test_set_layer_lock"
    TOOLS.append({
        "name": name,
        "description": "Explicitly lock or unlock one layer in the isolated fixture",
        "inputSchema": schema(dict(target=target, layerId=ident, locked={"type": "boolean"})),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "layer.setLock"
if os.environ.get("NECT_ACCEPTANCE_LAYER_RENAME") == "1":
    name = "nect_test_rename_layer"
    TOOLS.append({
        "name": name,
        "description": "Rename one unlocked layer in the isolated fixture",
        "inputSchema": schema(dict(target=target, layerId=ident, name=ident)),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "layer.rename"
if os.environ.get("NECT_ACCEPTANCE_LAYER_REORDER") == "1":
    name = "nect_test_reorder_layer"
    TOOLS.append({
        "name": name,
        "description": "Reorder one layer relative to a stable ID in the isolated fixture; null moves it to the front",
        "inputSchema": schema(dict(target=target, layerId=ident, beforeLayerId={"anyOf": [ident, {"type": "null"}]})),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "layer.reorder"
if os.environ.get("NECT_ACCEPTANCE_SOLID_FILL_READ") == "1":
    name = "nect_test_read_solid_fill_color"
    TOOLS.append({
        "name": name,
        "description": "Read the authored literal RGB and alpha of one exact Fill node, not its evaluated color",
        "inputSchema": schema(dict(target=target, layerId=ident, nodeId=ident)),
        "annotations": {"readOnlyHint": True, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "fill.readSolidColor"
if os.environ.get("NECT_ACCEPTANCE_SOLID_FILL") == "1":
    name = "nect_test_set_solid_fill_color"
    TOOLS.append({
        "name": name,
        "description": "Change the literal RGB color of one exact Fill node without changing its alpha",
        "inputSchema": schema(dict(target=target, layerId=ident, nodeId=ident,
            hex={"type":"string","pattern":"^#[0-9a-fA-F]{6}$","minLength":7,"maxLength":7})),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "fill.setSolidColor"
if os.environ.get("NECT_ACCEPTANCE_ARTWORK_GAPS") == "1":
    name = "nect_test_distribute_artwork_gaps"
    TOOLS.append({
        "name": name,
        "description": "Distribute artwork edge gaps automatically or to an explicit distance in the isolated fixture",
        "inputSchema": schema(dict(target=target,
            layerIds={"type":"array","items":ident,"minItems":2,"maxItems":100,"uniqueItems":True},
            action={"enum":["gap-x","gap-y"]},
            spacing={"type":["number","null"],"minimum":-100000,"maximum":100000})),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "artwork.distributeGaps"
if os.environ.get("NECT_ACCEPTANCE_ARTWORK_DISTRIBUTE") == "1":
    name = "nect_test_distribute_artwork"
    TOOLS.append({
        "name": name,
        "description": "Distribute selected artwork centers or edges, holding the endpoints in the isolated fixture",
        "inputSchema": schema(dict(target=target,
            layerIds={"type":"array","items":ident,"minItems":3,"maxItems":100,"uniqueItems":True},
            action={"enum":["left","center-x","right","top","center-y","bottom"]})),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "artwork.distribute"
if os.environ.get("NECT_ACCEPTANCE_ARTWORK_ALIGN") == "1":
    name = "nect_test_align_artwork"
    TOOLS.append({
        "name": name,
        "description": "Align explicit artwork layers to their selection bounds or the artboard in the isolated fixture",
        "inputSchema": schema(dict(
            target=target,
            layerIds={"type":"array","items":ident,"minItems":1,"maxItems":100,"uniqueItems":True},
            action={"enum":["left","center-x","right","top","center-y","bottom"]},
            reference={"enum":["selection","artboard"]},
        )),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "artwork.align"
if os.environ.get("NECT_ACCEPTANCE_ARTWORK_TRANSLATE") == "1":
    name = "nect_test_translate_artwork"
    TOOLS.append({
        "name": name,
        "description": "Move explicit artwork layers by a document-coordinate delta in the isolated fixture",
        "inputSchema": schema(dict(
            target=target,
            layerIds={"type":"array","items":ident,"minItems":1,"maxItems":100,"uniqueItems":True},
            delta={"type":"array","items":{"type":"number"},"minItems":2,"maxItems":2},
        )),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "idempotentHint": False,
                        "openWorldHint": False},
    })
    OPERATIONS[name] = "artwork.translate"
if os.environ.get("NECT_ACCEPTANCE_LAYER_DELETE") == "1":
    name = "nect_test_delete_layer"
    TOOLS.append({
        "name": name,
        "description": "Delete one exact layer from the isolated fixture after explicit confirmation",
        "inputSchema": schema(dict(target=target, layerId=ident, confirmed={"const":True})),
        "annotations": {"readOnlyHint": False, "destructiveHint": True, "idempotentHint": False,
                        "openWorldHint": False},
    })
    OPERATIONS[name] = "layer.delete"
if os.environ.get("NECT_ACCEPTANCE_LAYER_CREATE_BLANK") == "1":
    name = "nect_test_create_blank_layer"
    TOOLS.append({
        "name": name,
        "description": "Add one empty layer in the isolated fixture",
        "inputSchema": schema(dict(target=target, name={"type": ["string", "null"], "maxLength": 120})),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "idempotentHint": False,
                        "openWorldHint": False},
    })
    OPERATIONS[name] = "layer.createBlank"
if os.environ.get("NECT_ACCEPTANCE_LAYER_DUPLICATE") == "1":
    name = "nect_test_duplicate_layer"
    TOOLS.append({
        "name": name,
        "description": "Duplicate one unlocked authored layer within the isolated fixture",
        "inputSchema": schema(dict(target=target, layerId=ident)),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "idempotentHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "layer.duplicate"
if os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDERS") == "1":
    name = "nect_test_list_layer_folders"
    TOOLS.append({
        "name": name,
        "description": "Read the isolated fixture folder topology without names or edits",
        "inputSchema": schema(dict(target=target, offset={"type": "integer", "minimum": 0})),
        "annotations": {"readOnlyHint": True, "openWorldHint": False},
    })
    OPERATIONS[name] = "layerFolders.list"
if os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDER_CREATE") == "1":
    name = "nect_test_create_layer_folder"
    picked = {"type": "array", "items": ident, "maxItems": 32, "uniqueItems": True}
    TOOLS.append({
        "name": name,
        "description": "Create a new Folder from exact siblings in the isolated fixture",
        "inputSchema": schema(dict(target=target, name={"type":"string","minLength":1,"maxLength":100},
                                   layerIds=picked, folderIds=picked)),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "idempotentHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "layerFolders.create"
if os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDER_RENAME") == "1":
    name = "nect_test_rename_layer_folder"
    label = {"type":"string","minLength":1,"maxLength":100}
    TOOLS.append({
        "name": name,
        "description": "Rename one exact Folder in the isolated fixture if the expected name matches",
        "inputSchema": schema(dict(target=target, folderId=ident, expectedName=label, name=label)),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "layerFolders.rename"
if os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDER_MEMBERSHIP") == "1":
    name = "nect_test_change_layer_folder_membership"
    selected = {"type":"array","items":ident,"minItems":1,"maxItems":32,"uniqueItems":True}
    TOOLS.append({
        "name": name,
        "description": "Add or remove an exact Layer selection without changing paint order in the isolated fixture",
        "inputSchema": schema(dict(target=target, folderId=ident, mode={"enum":["add","remove"]}, layerIds=selected)),
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    })
    OPERATIONS[name] = "layerFolders.changeMembership"
if os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDER_DISSOLVE") == "1":
    name = "nect_test_dissolve_layer_folder"
    TOOLS.append({
        "name": name,
        "description": "Ungroup one Folder in the isolated fixture, preserving all artwork",
        "inputSchema": schema(dict(target=target, folderId=ident)),
        "annotations": {"readOnlyHint": False, "destructiveHint": True, "openWorldHint": False},
    })
    OPERATIONS[name] = "layerFolders.dissolve"
if os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDER_PLAN") == "1":
    selection = {"type":"array","items":ident,"minItems":1,"maxItems":32,"uniqueItems":True}
    request = {"oneOf": [
        schema(dict(folderId=ident, mode={"enum":["move","remove"]}, layerIds=selection)),
        schema(dict(folderId=ident, mode={"const":"order"}, direction={"enum":["forward","backward"]})),
    ]}
    TOOLS.extend([
        {"name":"nect_test_plan_layer_folder","description":"Preview a folder edit on this isolated document",
         "inputSchema":schema(dict(target=target, request=request)),
         "annotations":{"readOnlyHint":True,"openWorldHint":False}},
        {"name":"nect_test_apply_layer_folder_plan","description":"Apply the exact reviewed plan once in this isolated document",
         "inputSchema":schema(dict(target=target, planId={"type":"string","format":"uuid"}, confirmed={"const":True})),
         "annotations":{"readOnlyHint":False,"destructiveHint":True,"idempotentHint":False,"openWorldHint":False}},
    ])
    OPERATIONS["nect_test_plan_layer_folder"] = "layerFolders.plan"
    OPERATIONS["nect_test_apply_layer_folder_plan"] = "layerFolders.applyPlan"
if PREVIEW_ONLY:
    TOOLS = [{
        "name": "nect_test_inspect",
        "description": "Read target identity of a locally enabled, disposable Nect preview",
        "inputSchema": schema({}),
        "annotations": {"readOnlyHint": True, "openWorldHint": False},
    }]
    OPERATIONS = {"nect_test_inspect": "editor.inspect"}
SUCCEEDED = {"ok", "changed", "unchanged", "accepted-by-writer", "reopened"}

def verify_exe():
    if not EXE.is_file() or hashlib.sha256(EXE.read_bytes()).hexdigest() != HASH:
        raise RuntimeError("acceptance build identity mismatch")


def make_child():
    verify_exe()
    root = EXE.parent / ("nect-svg-acceptance-" + uuid.uuid4().hex)
    return subprocess.Popen([str(EXE)], cwd=str(EXE.parent), stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=sys.stderr, env={
                                **os.environ, "NECT_SVG_ACCEPTANCE_ROOT": str(root),
                                "NECT_LIVE_EDITOR_ACCEPTANCE_STDIO": "0" if PREVIEW_ONLY else "1",
                                "NECT_LIVE_INSPECT_PREVIEW_STDIO": "1" if PREVIEW_ONLY else "0",
                                "TEMP": str(EXE.parent), "TMP": str(EXE.parent), "TMPDIR": str(EXE.parent),
                            }, creationflags=subprocess.CREATE_NO_WINDOW)

def abandon_native(child):
    if child.poll() is None:
        child.terminate()
        child.wait(timeout=2)
    return {"status": "outcome-unknown"}


def app_call(child, native_replies, name, args):
    try:
        if child.poll() is not None:
            return {"status": "app-unavailable"}
        if name == "nect_test_rename_layer":
            label = args.get("name")
            if not isinstance(label, str) or not 1 <= len(label) <= ident["maxLength"]:
                return {"status": "invalid-arguments"}
        packet = json.dumps({"version": 1, "operation": OPERATIONS[name], "arguments": args},
                            allow_nan=False, separators=(",", ":")).encode("utf-8")
        if len(packet) > 4096:
            return {"status": "invalid-arguments"}
        child.stdin.write(packet + b"\n")
        child.stdin.flush()
        try:
            line = native_replies.get(timeout=9)
        except Empty:
            # No correlation is possible after this deadline. Never admit another
            # request on this stream; its next line may be the old reply.
            return abandon_native(child)
        if line is None:
            return abandon_native(child)
        if not line.endswith(b"\n") or len(line) > 4097:
            return abandon_native(child)
        result = json.loads(line)
        if not isinstance(result, dict) or not isinstance(result.get("status"), str):
            return abandon_native(child)
        return result
    except (OSError, ValueError):
        return abandon_native(child)

def serve(child):
    native_replies = Queue()
    def read_native():
        try:
            while line := child.stdout.readline(4098):
                native_replies.put(line)
                if len(line) > 4097 or not line.endswith(b"\n"):
                    return
        finally:
            native_replies.put(None)
    if child is not None:
        Thread(target=read_native, daemon=True).start()
    handshake = "start"
    preview_used = False
    def response(id_, **payload):
        sys.stdout.write(json.dumps({"jsonrpc": "2.0", "id": id_, **payload}) + "\n")
        sys.stdout.flush()
    def error(id_, code, message):
        response(id_, error={"code": code, "message": message})
    while data := sys.stdin.buffer.readline(8193):
        if len(data) > 8192 or not data.endswith(b"\n"):
            break
        try:
            m = json.loads(data)
        except ValueError:
            error(None, -32700, "Parse error")
            continue
        if not isinstance(m, dict) or m.get("jsonrpc") != "2.0" or not isinstance(m.get("method"), str):
            error(None, -32600, "Invalid request")
            continue
        method, params, id_ = m["method"], m.get("params", {}), m.get("id")
        if "id" not in m:
            if method == "notifications/initialized" and handshake == "initialized":
                handshake = "ready"
            continue
        if type(id_) not in (int, str) or not isinstance(params, dict):
            error(None, -32600, "Invalid request")
        elif method == "initialize" and handshake == "start":
            if (not isinstance(params.get("protocolVersion"), str)
                    or not isinstance(params.get("clientInfo"), dict)
                    or not isinstance(params.get("capabilities"), dict)):
                error(id_, -32602, "Invalid initialize params")
                continue
            handshake = "initialized"
            response(id_, result={"protocolVersion": REV, "serverInfo": {
                "name": "nect-live-preview-proof" if PREVIEW_ONLY else "nect-live-acceptance", "version": "0.1.0"},
                "capabilities": {"tools": {"listChanged": False}}})
        elif method == "ping":
            response(id_, result={})
        elif handshake != "ready":
            error(id_, -32002, "Initialization required")
        elif method == "tools/list":
            response(id_, result={"tools": TOOLS})
        elif method == "tools/call":
            name, args = params.get("name"), params.get("arguments", {})
            if not isinstance(name, str) or name not in OPERATIONS or not isinstance(args, dict):
                error(id_, -32602, "Unknown tool or invalid arguments")
                continue
            if PREVIEW_ONLY:
                if args:
                    error(id_, -32602, "Preview inspection takes no arguments")
                    continue
                if preview_used:
                    result = {"status": "proof-consumed"}
                else:
                    preview_used = True
                    if PIPE_IDENTIFIER is not None:
                        result = inspect_once(PIPE_ENDPOINT, EXE)
                    else:
                        result = app_call(child, native_replies, name, args)
            else:
                result = app_call(child, native_replies, name, args)
            response(id_, result={"content": [{"type": "text", "text": json.dumps(result)}],
                                  "structuredContent": result,
                                  "isError": result["status"] not in SUCCEEDED})
        else:
            error(id_, -32601, "Method not found")

if __name__ == "__main__":
    if PIPE_IDENTIFIER is not None:
        verify_exe()
        serve(None)
    else:
        child = make_child()
        try:
            serve(child)
        finally:
            child.stdin.close()
            if child.poll() is None:
                child.terminate()
            child.wait(timeout=5)

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
BASE = Path(__file__).resolve().parent
EXE = Path(os.environ["NECT_ACCEPTANCE_EXE"]).resolve(strict=True)
HASH = os.environ["NECT_ACCEPTANCE_SHA256"].lower()
if os.name != "nt" or EXE.suffix.lower() != ".exe" or len(HASH) != 64:
    raise RuntimeError("A Windows acceptance build and its expected SHA-256 are required")

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
SUCCEEDED = {"ok", "changed", "unchanged", "accepted-by-writer", "reopened"}

def make_child():
    if not EXE.is_file() or hashlib.sha256(EXE.read_bytes()).hexdigest() != HASH:
        raise RuntimeError("acceptance build identity mismatch")
    root = EXE.parent / ("nect-svg-acceptance-" + uuid.uuid4().hex)
    return subprocess.Popen([str(EXE)], cwd=str(EXE.parent), stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=sys.stderr, env={
                                **os.environ, "NECT_SVG_ACCEPTANCE_ROOT": str(root),
                                "NECT_LIVE_EDITOR_ACCEPTANCE_STDIO": "1",
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
    Thread(target=read_native, daemon=True).start()
    handshake = "start"
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
                "name": "nect-live-acceptance", "version": "0.1.0"},
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
            result = app_call(child, native_replies, name, args)
            response(id_, result={"content": [{"type": "text", "text": json.dumps(result)}],
                                  "structuredContent": result,
                                  "isError": result["status"] not in SUCCEEDED})
        else:
            error(id_, -32601, "Method not found")

if __name__ == "__main__":
    child = make_child()
    try:
        serve(child)
    finally:
        child.stdin.close()
        if child.poll() is None:
            child.terminate()
        child.wait(timeout=5)

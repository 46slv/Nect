"""End-to-end MCP JSON-RPC check of the isolated native editor."""
from pathlib import Path
from queue import Queue
import json
import hashlib
import os
import subprocess
import sys
from threading import Thread

base = Path(__file__).resolve().parent
result_file = Path(os.environ["NECT_ACCEPTANCE_RECEIPT"]).resolve()
assert not result_file.exists()
exe = Path(os.environ["NECT_ACCEPTANCE_EXE"]).resolve(strict=True)
root = exe.parent
assert result_file.parent == root
before_dirs=set(root.glob("nect-svg-acceptance-*"))
fixture = json.loads((base.parent / "examples/blend-distance.json").read_text(encoding="utf-8"))
layer = fixture["layers"][0]
node = next(n for n in layer["nodes"] if n["type"] == "Blend")
with subprocess.Popen([sys.executable, "-B", str(base / "live-editor-acceptance-mcp.py")],
                      cwd=base, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                      env={**os.environ, "TEMP":str(root), "TMP":str(root), "TMPDIR":str(root)},
                      creationflags=subprocess.CREATE_NO_WINDOW) as bridge:
    replies = Queue()
    def pull():
        for line in bridge.stdout:
            replies.put(line)
        replies.put(None)
    Thread(target=pull, daemon=True).start()
    def send(message):
        bridge.stdin.write(json.dumps(dict(jsonrpc="2.0", **message)).encode() + b"\n")
        bridge.stdin.flush()
    def call(id_, method, **params):
        send(dict(id=id_, method=method, params=params))
        line = replies.get(timeout=12)
        assert line, "MCP subprocess stopped without a reply"
        reply = json.loads(line)
        assert reply["id"] == id_ and reply["jsonrpc"] == "2.0", reply
        return reply["result"]
    try:
        hello = call(1, "initialize", protocolVersion="2025-06-18", capabilities={}, clientInfo={"name":"nect-smoke","version":"1"})
        assert hello["protocolVersion"] == "2025-06-18"
        send(dict(method="notifications/initialized"))
        listed = call(2, "tools/list")
        assert [t["name"] for t in listed["tools"]] == ["nect_test_inspect", "nect_test_set_blend_distance", "nect_test_export_svg", "nect_test_export_png", "nect_test_save_copy", "nect_test_reopen_copy", "nect_test_undo", "nect_test_redo","nect_test_save_state","nect_test_save_receipt","nect_test_save_as"]
        assert [t["annotations"]["readOnlyHint"] for t in listed["tools"]] == [True,False,False,False,False,False,False,False,True,True,False]
        start = call(3, "tools/call", name="nect_test_inspect", arguments={})
        assert not start["isError"], start
        target = start["structuredContent"]["target"]
        intent = dict(target=target, layerId=layer["id"], nodeId=node["id"], value=60)
        edited = call(4, "tools/call", name="nect_test_set_blend_distance", arguments=intent)
        assert not edited["isError"] and edited["structuredContent"]["status"] == "changed", edited
        current = edited["structuredContent"]["target"]
        assert current["projectRevision"] == target["projectRevision"] + 1
        stale = call(5, "tools/call", name="nect_test_set_blend_distance", arguments={**intent, "value":70})
        assert stale["isError"] and stale["structuredContent"]["status"] == "stale-target", stale
        after = call(6, "tools/call", name="nect_test_inspect", arguments={})
        assert after["structuredContent"]["target"] == current
        old_output = call(7, "tools/call", name="nect_test_export_svg", arguments={"target":target})
        assert old_output["isError"] and old_output["structuredContent"]["status"] == "stale-target", old_output
        next_id=7
        def invoke(name, args, expected_status, errored=False):
            global next_id
            next_id += 1
            r=call(next_id,"tools/call",name="nect_test_"+name,arguments=args)
            assert r["isError"] == errored and r["structuredContent"]["status"] == expected_status,r
            return r["structuredContent"]
        undone=invoke("undo",{"target":current},"changed")["target"]
        assert undone["projectRevision"]==current["projectRevision"]+1
        invoke("undo",{"target":current},"stale-target",True)
        assert invoke("undo",{"target":undone},"unchanged")["target"]==undone
        current=invoke("redo",{"target":undone},"changed")["target"]
        assert current["projectRevision"]==undone["projectRevision"]+1
        state=invoke("save_state",{},"ok")
        assert state==dict(status="ok",saveEpoch=0,dirty=True,pending=False)
        k="save_as_fault_01"
        cmd=dict(target=current,saveEpoch=0,requestId=k)
        invoke("save_as",{**cmd,"path":"forbidden.json"},"invalid-arguments",True)
        next_id+=1
        send(dict(id=next_id,method="tools/call",params={"name":"nect_test_save_as","arguments":cmd}))
        dropped = replies.get(timeout=12)
        assert dropped is not None
        # The normal client handler never consumes or interprets this dropped result.
        del dropped
        receipt=invoke("save_receipt",{"requestId":k},"accepted-by-writer")
        assert receipt==dict(status="accepted-by-writer",saveEpoch=1)
        assert invoke("save_state",{},"ok")==dict(status="ok",saveEpoch=1,dirty=False,pending=False)
        assert invoke("save_as",cmd,"accepted-by-writer")==receipt
        invoke("save_as",{**cmd,"saveEpoch":1},"request-conflict",True)
        invoke("save_copy",{"target":target},"stale-target",True)
        invoke("save_copy",{"target":current,"path":"user-path.json"},"invalid-arguments",True)
        invoke("save_copy",{"target":current},"accepted-by-writer")
        assert invoke("inspect",{},"ok")["target"] == current
        invoke("reopen_copy",{"target":current},"reopened")
        reopened=invoke("inspect",{},"ok")["target"]
        assert reopened["documentGeneration"]>current["documentGeneration"] and reopened["sessionId"]!=current["sessionId"]
        invoke("reopen_copy",{"target":current},"stale-target",True)
        invoke("export_svg",{"target":reopened},"accepted-by-writer")
        invoke("export_svg",{"target":reopened},"outcome-unknown",True)
        invoke("export_png",{"target":reopened},"accepted-by-writer")
        invoke("export_png",{"target":reopened},"outcome-unknown",True)
        assert invoke("inspect",{},"ok")["target"] == reopened
        bridge.stdin.close()
        assert bridge.wait(timeout=10) == 0
        [run]=set(root.glob("nect-svg-acceptance-*"))-before_dirs
        expected = {
            "copySHA256": "5cd60a9b6de8662cd5fcad4d22b170b578e3d5cfcc6f76ba079b7761c94bc85d",
            "svgSHA256": "2fc89ac6200e9ad681cc8ec7fec3ba382d6937d4943fab4386261782034f1095",
            "pngSHA256": "c58c92625c8400699021ccfbce42be1d43c24b84cbdef603921dca2b4162c84d",
        }
        digests={}
        for role,filename in (("copy","live-api-project.json"),("svg","live-api-export.svg"),("png","live-api-export.png")):
            digests[role]=hashlib.sha256((run/"documents"/filename).read_bytes()).hexdigest()
            assert digests[role]==expected[role+"SHA256"].lower()
        bound=run/"documents/live-api-save-as.json"
        assert hashlib.sha256(bound.read_bytes()).hexdigest()==digests["copy"]
        assert len(list((run/"documents").glob("live-api-save-as*")))==1
        evidence={"saveAs":"native binding on fixed test path","replyRecovery":"dropped, checked receipt, replay reused outcome",
                  "scope":"independent MCP stdio client, isolated hidden Windows Nect; not Hermes or normal product",
                  "protocol":hello["protocolVersion"],"tools":len(listed["tools"]),"run":run.name,
                  "edit":"changed","history":"undo/redo verified","saveCopy":"accepted-by-writer","reopen":"reopened",
                  "exportSvg":"accepted-by-writer","exportPng":"accepted-by-writer","caseGuards":"pass",
                  "digest":digests}
        with result_file.open("x", encoding="utf-8") as f:
            json.dump(evidence, f)
        print(json.dumps(evidence))
    finally:
        if not bridge.stdin.closed:
            bridge.stdin.close()
        if bridge.poll() is None:
            try:
                bridge.wait(timeout=7)
            except subprocess.TimeoutExpired:
                bridge.terminate()

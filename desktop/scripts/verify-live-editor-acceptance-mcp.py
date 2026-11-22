"""End-to-end MCP JSON-RPC check of the isolated native editor."""
from pathlib import Path
from queue import Queue
import json
import hashlib
import os
import re
import subprocess
import sys
from threading import Thread

base = Path(__file__).resolve().parent
result_file = Path(os.environ["NECT_ACCEPTANCE_RECEIPT"]).resolve()
assert not result_file.exists()
exe = Path(os.environ["NECT_ACCEPTANCE_EXE"]).resolve(strict=True)
root = exe.parent
assert result_file.parent == root
list_objects = os.environ.get("NECT_ACCEPTANCE_OBJECTS") == "1"
graph_wires = os.environ.get("NECT_ACCEPTANCE_GRAPH_WIRES") == "1"
layer_visibility = os.environ.get("NECT_ACCEPTANCE_LAYER_VISIBILITY") == "1"
artwork = os.environ.get("NECT_ACCEPTANCE_ARTWORK") == "1"
text_edit = artwork and os.environ.get("NECT_ACCEPTANCE_TEXT") == "1"
mixed_text_probe = os.environ.get("NECT_ACCEPTANCE_TEXT_JP_DEFAULT_PROBE") == "1"
embedded_jp = os.environ.get("NECT_SVG_ACCEPTANCE_JP_FIXTURE") == "1"
assert not (embedded_jp and mixed_text_probe)
assert not (embedded_jp or mixed_text_probe) or text_edit
replacement = "\u63db\u30fbA1\uff0812\uff09" if mixed_text_probe else ("\u65e5\u672c\u8a9e A1\uff0812\uff09" if embedded_jp else "Nect A1 (12)")
def fixture_runs():
    return {
        path for path in root.glob("nect-svg-acceptance-*")
        if re.fullmatch(r"nect-svg-acceptance-[a-f0-9]{32}", path.name) and path.is_dir()
    }

before_dirs=fixture_runs()
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
        extra = ["nect_test_create_artwork"] if artwork else []
        if text_edit:
            extra.append("nect_test_set_text_content")
        if list_objects:
            extra.append("nect_test_list_artwork")
        if graph_wires:
            extra.append("nect_test_read_node_input_wires")
        if layer_visibility:
            extra.append("nect_test_set_layer_visibility")
        assert [t["name"] for t in listed["tools"]] == ["nect_test_inspect", "nect_test_set_blend_distance", "nect_test_export_svg", "nect_test_export_png", "nect_test_save_copy", "nect_test_reopen_copy", "nect_test_undo", "nect_test_redo","nect_test_save_state","nect_test_save_receipt","nect_test_save_as"] + extra
        assert [t["annotations"]["readOnlyHint"] for t in listed["tools"]] == [True,False,False,False,False,False,False,False,True,True,False] + [False] * (len(extra) - int(list_objects) - int(graph_wires) - int(layer_visibility)) + [True] * (int(list_objects) + int(graph_wires)) + [False] * int(layer_visibility)
        start = call(3, "tools/call", name="nect_test_inspect", arguments={})
        assert not start["isError"], start
        target = start["structuredContent"]["target"]
        source_layer, source_node = layer["id"], node["id"]
        if list_objects:
            def list_page(id_, layer_index, revision):
                return call(id_, "tools/call", name="nect_test_list_artwork",
                            arguments=dict(target=revision, layerIndex=layer_index, offset=0))
            layers = list_page(1001, None, target)
            assert not layers["isError"], layers
            layer_page = layers["structuredContent"]
            assert set(layer_page) == {"status","target","layerId","items","nextOffset"}
            assert layer_page["target"] == target and layer_page["layerId"] is None
            assert all(set(item) == {"layerId","layerIndex","locked"} for item in layer_page["items"])
            [address] = [item for item in layer_page["items"] if item["layerId"] == layer["id"]]
            nodes = list_page(1002, address["layerIndex"], target)
            assert not nodes["isError"], nodes
            page = nodes["structuredContent"]
            assert page["target"] == target and page["layerId"] == address["layerId"]
            assert all(set(item) == {"nodeId","type"} for item in page["items"])
            [blend] = [item for item in page["items"] if item["type"] == "Blend"]
            assert blend["nodeId"] == node["id"]
            source_layer, source_node = page["layerId"], blend["nodeId"]
        if layer_visibility:
            def set_visible(id_, revision, value, **extra_args):
                args = dict(target=revision, layerId=source_layer, visible=value)
                return call(id_, "tools/call", name="nect_test_set_layer_visibility",
                            arguments=dict(args, **extra_args))

            def expect_visibility(reply, status, failed=False):
                assert reply["isError"] == failed and reply["structuredContent"]["status"] == status, reply
                return reply["structuredContent"]

            expect_visibility(set_visible(1201, target, False, key="solo"), "invalid-arguments", True)
            expect_visibility(set_visible(1202, target, "false"), "invalid-arguments", True)
            assert expect_visibility(set_visible(1203, target, True), "unchanged")["target"] == target
            hidden = expect_visibility(set_visible(1204, target, False), "changed")["target"]
            assert hidden["projectRevision"] == target["projectRevision"] + 1
            expect_visibility(set_visible(1205, target, True), "stale-target", True)
            assert expect_visibility(set_visible(1206, hidden, False), "unchanged")["target"] == hidden
            restore = call(1207, "tools/call", name="nect_test_undo", arguments=dict(target=hidden))
            target = expect_visibility(restore, "changed")["target"]
            assert expect_visibility(set_visible(1208, target, True), "unchanged")["target"] == target
        if graph_wires:
            expected_wires = [
                dict(input=key, sourceId=param["source"], sourcePort=param.get("sourcePort"))
                for key, param in node["params"].items() if param.get("source") is not None
            ]
            assert expected_wires, "Fixture needs at least one authored input wire"

            def read_wires(id_, revision, offset=0, **extra_args):
                args = dict(target=revision, layerId=source_layer, nodeId=source_node, offset=offset)
                return call(id_, "tools/call", name="nect_test_read_node_input_wires",
                            arguments=dict(args, **extra_args))

            wires = read_wires(1101, target)
            assert not wires["isError"] and wires["structuredContent"] == dict(
                status="ok", target=target, layerId=source_layer, nodeId=source_node,
                items=expected_wires, nextOffset=None
            ), wires
            empty = read_wires(1102, target, offset=len(expected_wires))
            assert not empty["isError"] and empty["structuredContent"]["items"] == [], empty
            wrong = read_wires(1103, target, path="not-an-api-field")
            assert wrong["isError"] and wrong["structuredContent"] == {"status": "invalid-arguments"}, wrong
            readback = call(1104, "tools/call", name="nect_test_inspect", arguments={})
            assert readback["structuredContent"]["target"] == target, readback
        intent = dict(target=target, layerId=source_layer, nodeId=source_node, value=60)
        edited = call(4, "tools/call", name="nect_test_set_blend_distance", arguments=intent)
        assert not edited["isError"] and edited["structuredContent"]["status"] == "changed", edited
        current = edited["structuredContent"]["target"]
        assert current["projectRevision"] == target["projectRevision"] + 1
        if list_objects:
            rejected = list_page(1003, 0, target)
            assert rejected["isError"] and rejected["structuredContent"] == {"status":"stale-target"}, rejected
        if graph_wires:
            rejected = read_wires(1105, target)
            assert rejected["isError"] and rejected["structuredContent"] == {"status": "stale-target"}, rejected
            latest = read_wires(1106, current)
            assert not latest["isError"] and latest["structuredContent"]["items"] == expected_wires, latest
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
        made = []
        created = []
        if artwork:
            create_cases = [("Rectangle",[430,180])]
            if not embedded_jp:
                create_cases.append(("Text",[610,230]))
            for kind, point in create_cases:
                create_args = dict(target=current, artworkType=kind, at=point)
                invoke("create_artwork",{**create_args,"at":[0]},"invalid-arguments",True)
                obj=invoke("create_artwork",create_args,"changed")
                assert obj["target"]["projectRevision"]==current["projectRevision"]+1
                invoke("create_artwork",create_args,"stale-target",True)
                made.append((kind,point,obj["layerId"],obj["nodeId"]))
                current=obj["target"]
            created = [x[0] for x in made]
            if embedded_jp:
                assets=json.loads((base.parent/"tests/fixtures/project-integrity-native.json").read_bytes())
                [t_layer]=[l for l in assets["compositions"][0]["layers"] if l["id"]=="integrity-text-layer"]
                [t_node]=[n for n in t_layer["nodes"] if n["id"]=="integrity-text"]
                made.append(("Text",t_node["params"]["position"]["value"],t_layer["id"],t_node["id"]))
            if text_edit:
                _,_,target_layer,target_node=made[-1]
                request=dict(target=current,layerId=target_layer,nodeId=target_node,text=replacement)
                invoke("set_text_content",{**request,"nodeId":"missing"},"unsupported-target",True)
                updated=invoke("set_text_content",request,"changed")["target"]
                assert updated["projectRevision"]==current["projectRevision"]+1
                invoke("set_text_content",request,"stale-target",True)
                unchanged=invoke("set_text_content",{**request,"target":updated},"unchanged")["target"]
                assert unchanged==updated
                current=updated
            before_undo=current
            current=invoke("undo",{"target":current},"changed")["target"]
            invoke("redo",{"target":before_undo},"stale-target",True)
            current=invoke("redo",{"target":current},"changed")["target"]
            assert current["projectRevision"]==before_undo["projectRevision"]+2
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
        if mixed_text_probe:
            for kind in ("svg", "png"):
                invoke("export_"+kind,{"target":reopened},"unsupported-glyph",True)
        else:
            invoke("export_svg",{"target":reopened},"accepted-by-writer")
            invoke("export_svg",{"target":reopened},"outcome-unknown",True)
            invoke("export_png",{"target":reopened},"accepted-by-writer")
            invoke("export_png",{"target":reopened},"outcome-unknown",True)
        assert invoke("inspect",{},"ok")["target"] == reopened
        bridge.stdin.close()
        assert bridge.wait(timeout=10) == 0
        [run]=fixture_runs()-before_dirs
        expected = {
            "copySHA256": "5cd60a9b6de8662cd5fcad4d22b170b578e3d5cfcc6f76ba079b7761c94bc85d",
            "svgSHA256": "2fc89ac6200e9ad681cc8ec7fec3ba382d6937d4943fab4386261782034f1095",
            "pngSHA256": "c58c92625c8400699021ccfbce42be1d43c24b84cbdef603921dca2b4162c84d",
        }
        outputs = run/"documents"
        if mixed_text_probe:
            assert not (outputs/"live-api-export.svg").exists()
            assert not (outputs/"live-api-export.png").exists()
        digests={}
        for role,filename in (("copy","live-api-project.json"),("svg","live-api-export.svg"),("png","live-api-export.png")):
            if mixed_text_probe and role != "copy":
                continue
            digests[role]=hashlib.sha256((run/"documents"/filename).read_bytes()).hexdigest()
            if artwork:
                assert digests[role]!=expected[role+"SHA256"].lower(), role
            else:
                assert digests[role]==expected[role+"SHA256"].lower()
        if artwork:
            artifacts=run/"documents"
            snapshot=json.loads((artifacts/"live-api-project.json").read_bytes())
            [composition]=[x for x in snapshot["compositions"] if x["id"]==reopened["compositionId"]]
            for kind,point,layer_id,node_id in made:
                [shape_layer]=[x for x in composition["layers"] if x["id"]==layer_id]
                [shape]=[n for n in shape_layer["nodes"] if n["id"]==node_id]
                assert shape["type"]==kind, shape["type"]
                if kind=="Rectangle":
                    assert shape["params"]["center"]["value"]==point, shape["params"]["center"]
                else:
                    assert shape["params"]["position"]["value"]==point, shape["params"]["position"]
                    if text_edit:
                        expected_id="font-integrity-fixture" if embedded_jp else "default"
                        assert shape["text"]==replacement and shape["font"]==expected_id
                        if embedded_jp:
                            assert snapshot["fontAssets"][expected_id]==assets["fontAssets"][expected_id]
            if not mixed_text_probe:
                svg=(artifacts/"live-api-export.svg").read_bytes()
                png=(artifacts/"live-api-export.png").read_bytes()
                from xml.etree import ElementTree
                assert ElementTree.fromstring(svg).tag.endswith("}svg")
                assert png[:8]==b"\x89PNG\r\n\x1a\n" and png[12:16]==b"IHDR"
                assert (int.from_bytes(png[16:20],"big"),int.from_bytes(png[20:24],"big"))==(960,540)
        bound=run/"documents/live-api-save-as.json"
        assert hashlib.sha256(bound.read_bytes()).hexdigest()==digests["copy"]
        assert len(list((run/"documents").glob("live-api-save-as*")))==1
        evidence={"saveAs":"native binding on fixed test path","replyRecovery":"dropped, checked receipt, replay reused outcome",
                  "scope":"independent MCP stdio client, isolated hidden Windows Nect; not Hermes or normal product",
                  "protocol":hello["protocolVersion"],"tools":len(listed["tools"]),"run":run.name,
                  "edit":"changed","history":"undo/redo verified","saveCopy":"accepted-by-writer","reopen":"reopened",
                  "exportSvg":"unsupported-glyph" if mixed_text_probe else "accepted-by-writer",
                  "exportPng":"unsupported-glyph" if mixed_text_probe else "accepted-by-writer","caseGuards":"pass",
                  "artwork":created, "authoredObjectDiscovery":list_objects, "authoredNodeWireDiscovery":graph_wires, "layerVisibility":layer_visibility, "embeddedJapaneseFixture":embedded_jp, "textEdit":text_edit,"japaneseDefaultFontNegativeProbe":mixed_text_probe, "digest":digests}
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

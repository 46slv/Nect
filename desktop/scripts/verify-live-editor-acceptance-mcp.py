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
layer_state = os.environ.get("NECT_ACCEPTANCE_LAYER_STATE") == "1"
layer_guide = os.environ.get("NECT_ACCEPTANCE_LAYER_GUIDE") == "1"
layer_solo = os.environ.get("NECT_ACCEPTANCE_LAYER_SOLO") == "1"
layer_lock = os.environ.get("NECT_ACCEPTANCE_LAYER_LOCK") == "1"
layer_rename = os.environ.get("NECT_ACCEPTANCE_LAYER_RENAME") == "1"
layer_reorder = os.environ.get("NECT_ACCEPTANCE_LAYER_REORDER") == "1"
layer_delete = os.environ.get("NECT_ACCEPTANCE_LAYER_DELETE") == "1"
layer_delete_restore = os.environ.get("NECT_ACCEPTANCE_LAYER_DELETE_RESTORE") == "1"
layer_delete_member = os.environ.get("NECT_ACCEPTANCE_LAYER_DELETE_MEMBER") == "1"
layer_create_blank = os.environ.get("NECT_ACCEPTANCE_LAYER_CREATE_BLANK") == "1"
layer_duplicate = os.environ.get("NECT_ACCEPTANCE_LAYER_DUPLICATE") == "1"
layer_folders = os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDERS") == "1"
layer_folder_plan = os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDER_PLAN") == "1"
folder_plan_move_order = os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDER_MOVE_ORDER_CASE") == "1"
assert not folder_plan_move_order or layer_folder_plan
folder_create_tool = os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDER_CREATE") == "1"
create_layer_folder = folder_create_tool and not (layer_folder_plan or layer_delete_member)
rename_layer_folder = os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDER_RENAME") == "1"
change_folder_membership = os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDER_MEMBERSHIP") == "1"
dissolve_layer_folder = os.environ.get("NECT_ACCEPTANCE_LAYER_FOLDER_DISSOLVE") == "1"
renamed_roundtrip = os.environ.get("NECT_ACCEPTANCE_RENAMED_ROUNDTRIP") == "1"
locked_roundtrip = os.environ.get("NECT_ACCEPTANCE_LOCKED_ROUNDTRIP") == "1"
hidden_roundtrip = os.environ.get("NECT_ACCEPTANCE_HIDDEN_ROUNDTRIP") == "1"
artwork = os.environ.get("NECT_ACCEPTANCE_ARTWORK") == "1"
text_edit = artwork and os.environ.get("NECT_ACCEPTANCE_TEXT") == "1"
solid_fill = os.environ.get("NECT_ACCEPTANCE_SOLID_FILL") == "1"
solid_fill_read = os.environ.get("NECT_ACCEPTANCE_SOLID_FILL_READ") == "1"
assert not solid_fill_read or solid_fill
artwork_gaps = os.environ.get("NECT_ACCEPTANCE_ARTWORK_GAPS") == "1"
artwork_gaps_auto = os.environ.get("NECT_ACCEPTANCE_ARTWORK_GAPS_AUTO") == "1"
assert not artwork_gaps_auto or artwork_gaps
artwork_distribute = os.environ.get("NECT_ACCEPTANCE_ARTWORK_DISTRIBUTE") == "1"
artwork_align = os.environ.get("NECT_ACCEPTANCE_ARTWORK_ALIGN") == "1"
artwork_translate = os.environ.get("NECT_ACCEPTANCE_ARTWORK_TRANSLATE") == "1"
mixed_text_probe = os.environ.get("NECT_ACCEPTANCE_TEXT_JP_DEFAULT_PROBE") == "1"
embedded_jp = os.environ.get("NECT_SVG_ACCEPTANCE_JP_FIXTURE") == "1"
assert not layer_folder_plan or (artwork and folder_create_tool and not (
    layer_folders or layer_reorder or layer_duplicate or embedded_jp or mixed_text_probe
))
assert not dissolve_layer_folder or (create_layer_folder and not change_folder_membership)
assert not change_folder_membership or (layer_folders and not (artwork or hidden_roundtrip or locked_roundtrip or
    renamed_roundtrip or rename_layer_folder or create_layer_folder))
assert not rename_layer_folder or (layer_folders and not (artwork or hidden_roundtrip or locked_roundtrip or renamed_roundtrip))
assert not create_layer_folder or (layer_folders and not (artwork or hidden_roundtrip or locked_roundtrip or renamed_roundtrip))
assert not layer_folders or not (embedded_jp or mixed_text_probe or layer_reorder or layer_duplicate)
assert not layer_state or (layer_guide or layer_solo)
assert not layer_guide or (artwork and layer_lock and not (
    layer_solo or layer_folder_plan or layer_folders or layer_reorder or layer_duplicate or
    embedded_jp or mixed_text_probe or hidden_roundtrip or locked_roundtrip or renamed_roundtrip
))
assert not layer_solo or (artwork and layer_visibility and not (
    layer_folder_plan or layer_folders or layer_reorder or layer_duplicate or embedded_jp or mixed_text_probe
))
artwork_authoring_roundtrip = artwork_translate and solid_fill_read
if artwork_authoring_roundtrip:
    assert not any(root.glob("nect-svg-acceptance-*")), "Use a fresh fixture root, not prior recovery state"
assert sum((artwork_align, artwork_translate,
    artwork_distribute, artwork_gaps, solid_fill and not artwork_authoring_roundtrip)) <= 1
assert not (artwork_align or artwork_translate or artwork_distribute or artwork_gaps or solid_fill) or (artwork and layer_lock and list_objects and not (
    layer_delete or layer_solo or layer_guide or layer_folders or layer_folder_plan or
    hidden_roundtrip or locked_roundtrip or renamed_roundtrip or text_edit or embedded_jp or mixed_text_probe
))
assert not layer_lock or list_objects
assert not layer_delete_member or (
    layer_delete and artwork and list_objects and folder_create_tool and not (layer_delete_restore or
    layer_folder_plan or embedded_jp or mixed_text_probe or text_edit)
)
assert not layer_delete_restore or layer_delete
assert not layer_delete or (
    list_objects and not (layer_create_blank or layer_duplicate or layer_reorder or
    (artwork and not layer_delete_member) or layer_folders or
    layer_state or layer_solo or layer_guide or layer_lock or renamed_roundtrip or locked_roundtrip or hidden_roundtrip)
)
assert not layer_create_blank or (
    list_objects and not (layer_duplicate or layer_reorder or artwork or layer_folders or
    renamed_roundtrip or locked_roundtrip or hidden_roundtrip or layer_folder_plan)
)
assert not layer_duplicate or (
    list_objects and not (layer_reorder or layer_rename or layer_lock or layer_visibility or
    artwork or embedded_jp or mixed_text_probe or renamed_roundtrip or locked_roundtrip or hidden_roundtrip)
)
assert not layer_reorder or (
    artwork and list_objects and not (text_edit or embedded_jp or mixed_text_probe or
    layer_visibility or layer_lock or layer_rename or
    renamed_roundtrip or locked_roundtrip or hidden_roundtrip)
)
assert not renamed_roundtrip or (layer_rename and not locked_roundtrip and not hidden_roundtrip and not artwork and not embedded_jp and not mixed_text_probe)
assert not locked_roundtrip or (layer_lock and not hidden_roundtrip and not artwork and not embedded_jp and not mixed_text_probe)
assert not (embedded_jp and mixed_text_probe)
assert not hidden_roundtrip or (layer_visibility and not artwork and not embedded_jp and not mixed_text_probe)
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
        if layer_state:
            extra.append("nect_test_read_layer_state")
        if layer_visibility:
            extra.append("nect_test_set_layer_visibility")
        if layer_solo:
            extra.append("nect_test_set_layer_solo")
        if layer_guide:
            extra.append("nect_test_set_layer_guide")
        if layer_lock:
            extra.append("nect_test_set_layer_lock")
        if layer_rename:
            extra.append("nect_test_rename_layer")
        if solid_fill_read:
            extra.append("nect_test_read_solid_fill_color")
        if solid_fill:
            extra.append("nect_test_set_solid_fill_color")
        if artwork_gaps:
            extra.append("nect_test_distribute_artwork_gaps")
        if artwork_distribute:
            extra.append("nect_test_distribute_artwork")
        if artwork_align:
            extra.append("nect_test_align_artwork")
        if artwork_translate:
            extra.append("nect_test_translate_artwork")
        if layer_reorder:
            extra.append("nect_test_reorder_layer")
        if layer_delete:
            extra.append("nect_test_delete_layer")
        if layer_create_blank:
            extra.append("nect_test_create_blank_layer")
        if layer_duplicate:
            extra.append("nect_test_duplicate_layer")
        if layer_folders:
            extra.append("nect_test_list_layer_folders")
        if folder_create_tool:
            extra.append("nect_test_create_layer_folder")
        if rename_layer_folder:
            extra.append("nect_test_rename_layer_folder")
        if change_folder_membership:
            extra.append("nect_test_change_layer_folder_membership")
        if dissolve_layer_folder:
            extra.append("nect_test_dissolve_layer_folder")
        if layer_folder_plan:
            extra.extend(["nect_test_plan_layer_folder", "nect_test_apply_layer_folder_plan"])
        assert [t["name"] for t in listed["tools"]] == ["nect_test_inspect", "nect_test_set_blend_distance", "nect_test_export_svg", "nect_test_export_png", "nect_test_save_copy", "nect_test_reopen_copy", "nect_test_undo", "nect_test_redo","nect_test_save_state","nect_test_save_receipt","nect_test_save_as"] + extra
        read_only_names = {"nect_test_inspect", "nect_test_save_state", "nect_test_save_receipt",
                           "nect_test_list_artwork", "nect_test_read_node_input_wires",
                           "nect_test_list_layer_folders", "nect_test_plan_layer_folder", "nect_test_read_layer_state", "nect_test_read_solid_fill_color"}
        assert all(t["annotations"]["readOnlyHint"] == (t["name"] in read_only_names) for t in listed["tools"])
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
            assert all(set(item) == {"layerId","layerIndex","locked","visible"} for item in layer_page["items"])
            [address] = [item for item in layer_page["items"] if item["layerId"] == layer["id"]]
            assert address["visible"] is True
            nodes = list_page(1002, address["layerIndex"], target)
            assert not nodes["isError"], nodes
            page = nodes["structuredContent"]
            assert page["target"] == target and page["layerId"] == address["layerId"]
            assert all(set(item) == {"nodeId","type"} for item in page["items"])
            [blend] = [item for item in page["items"] if item["type"] == "Blend"]
            assert blend["nodeId"] == node["id"]
            source_layer, source_node = page["layerId"], blend["nodeId"]
        if layer_rename:
            def rename(id_, revision, name):
                result = call(id_, "tools/call", name="nect_test_rename_layer",
                              arguments=dict(target=revision, layerId=source_layer, name=name))
                return result["isError"], result["structuredContent"]

            original_name = layer["name"]
            label = "Blend \u540d\u79f0"
            assert rename(1401, target, "  ") == (True, {"status": "invalid-arguments"})
            assert rename(1402, target, "x" * 257) == (True, {"status": "invalid-arguments"})
            failed, change = rename(1403, target, "  " + label + "  ")
            assert not failed and change["status"] == "changed" and change["target"]["projectRevision"] == target["projectRevision"] + 1
            assert rename(1404, target, original_name) == (True, {"status": "stale-target"})
            assert rename(1405, change["target"], label) == (False, {"status": "unchanged", "target": change["target"]})
            undo = call(1406, "tools/call", name="nect_test_undo", arguments=dict(target=change["target"]))
            assert not undo["isError"] and undo["structuredContent"]["status"] == "changed"
            target = undo["structuredContent"]["target"]
            assert rename(1407, target, original_name) == (False, {"status": "unchanged", "target": target})
        if layer_lock:
            def set_locked(id_, revision, value, **extra_args):
                args = dict(target=revision, layerId=source_layer, locked=value)
                return call(id_, "tools/call", name="nect_test_set_layer_lock",
                            arguments=dict(args, **extra_args))

            def expect_lock(reply, status, failed=False):
                assert reply["isError"] == failed and reply["structuredContent"]["status"] == status, reply
                return reply["structuredContent"]

            assert address["locked"] is False
            expect_lock(set_locked(1301, target, True, key="guide"), "invalid-arguments", True)
            expect_lock(set_locked(1302, target, 1), "invalid-arguments", True)
            assert expect_lock(set_locked(1303, target, False), "unchanged")["target"] == target
            locked = expect_lock(set_locked(1304, target, True), "changed")["target"]
            assert locked["projectRevision"] == target["projectRevision"] + 1
            blocked_edit = call(1311, "tools/call", name="nect_test_set_blend_distance",
                                arguments=dict(target=locked,layerId=source_layer,nodeId=source_node,value=60))
            expect_lock(blocked_edit, "rejected", True)
            readback = call(1312, "tools/call", name="nect_test_inspect", arguments={})
            assert not readback["isError"] and readback["structuredContent"]["target"] == locked, readback
            expect_lock(set_locked(1305, target, False), "stale-target", True)
            assert expect_lock(set_locked(1306, locked, True), "unchanged")["target"] == locked
            page = list_page(1307, None, locked)
            assert not page["isError"], page
            [current_address] = [x for x in page["structuredContent"]["items"] if x["layerId"] == source_layer]
            assert current_address == {**address, "locked": True}
            undo = call(1308, "tools/call", name="nect_test_undo", arguments=dict(target=locked))
            target = expect_lock(undo, "changed")["target"]
            assert expect_lock(set_locked(1309, target, False), "unchanged")["target"] == target
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
            if list_objects:
                layers_after = list_page(1209, None, hidden)
                [hidden_layer] = [x for x in layers_after["structuredContent"]["items"] if x["layerId"]==source_layer]
                assert not layers_after["isError"] and hidden_layer["visible"] is False, layers_after
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
        if layer_folders:
            def folders(id_, revision, offset=0):
                return call(id_, "tools/call", name="nect_test_list_layer_folders",
                            arguments=dict(target=revision, offset=offset))
            folder_items = [
                dict(kind="folder", folderId="fixture-folder-parent", parentFolderId=None),
                dict(kind="folder", folderId="fixture-folder-child", parentFolderId="fixture-folder-parent"),
                dict(kind="membership", folderId="fixture-folder-child", layerId=source_layer),
            ]
            if change_folder_membership:
                folder_items = [dict(kind="folder", folderId="fixture-folder-flat", parentFolderId=None)]
            for id_, offset in [(1170, 0), (1171, len(folder_items)-1), (1172, len(folder_items))]:
                page = folders(id_, target, offset)
                assert not page["isError"] and page["structuredContent"] == dict(
                    status="ok", target=target, items=folder_items[offset:], nextOffset=None), page
            bad = folders(1173, target, len(folder_items)+1)
            assert bad["isError"] and bad["structuredContent"] == {"status":"invalid-arguments"}, bad
            assert call(1174, "tools/call", name="nect_test_inspect",
                        arguments={})["structuredContent"]["target"] == target
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
        if layer_folders:
            rejected = folders(1175, target)
            assert rejected["isError"] and rejected["structuredContent"] == {"status":"stale-target"}, rejected
            latest = folders(1176, current)
            assert not latest["isError"] and latest["structuredContent"]["items"] == folder_items, latest
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
            if artwork_gaps:
                right_type = "Ellipse" if artwork_gaps_auto else "Rectangle"
                create_cases = [("Rectangle",[260,180]), ("Circle",[505,210]), (right_type,[770,180])]
            elif artwork_distribute:
                create_cases.extend([("Rectangle",[505,230]), ("Rectangle",[770,180])])
            elif not embedded_jp and not layer_reorder:
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
        if solid_fill:
            _, _, fill_layer, shape_id = made[0]
            def fill_identity(revision):
                layer_page = list_page(1870, None, revision)
                [entry] = [item for item in layer_page["structuredContent"]["items"]
                           if item["layerId"] == fill_layer]
                node_page = list_page(1871, entry["layerIndex"], revision)
                [fill] = [item for item in node_page["structuredContent"]["items"] if item["type"] == "Fill"]
                return fill["nodeId"]
            fill_node = fill_identity(current)
            fill_hex = "#1177aa"
            paint = dict(target=current, layerId=fill_layer, nodeId=fill_node, hex=fill_hex)
            def read_fill(revision, expected, is_error=False, node=fill_node):
                return invoke("read_solid_fill_color",
                              dict(target=revision, layerId=fill_layer, nodeId=node),
                              expected, is_error)
            if solid_fill_read:
                read_fill(current, "unsupported-target", True, shape_id)
                initial_fill = read_fill(current, "ok")
                assert initial_fill["mode"] == "literal" and initial_fill["editable"]
                assert initial_fill["rgb"] != fill_hex and initial_fill["alpha"] == 1
                assert invoke("inspect", {}, "ok")["target"] == current
            invoke("set_solid_fill_color", {**paint, "nodeId":shape_id}, "unsupported-target", True)
            invoke("set_solid_fill_color", {**paint, "hex":"#fff"}, "invalid-arguments", True)
            current = invoke("set_layer_lock", dict(target=current, layerId=fill_layer, locked=True), "changed")["target"]
            invoke("set_solid_fill_color", {**paint, "target":current}, "rejected", True)
            if solid_fill_read:
                locked_fill = read_fill(current, "ok")
                assert locked_fill["locked"] and not locked_fill["editable"]
                assert locked_fill["rgb"] == initial_fill["rgb"]
            current = invoke("set_layer_lock", dict(target=current, layerId=fill_layer, locked=False), "changed")["target"]
            paint["target"] = current
            changed = invoke("set_solid_fill_color", paint, "changed")["target"]
            assert changed["projectRevision"] == current["projectRevision"] + 1
            invoke("set_solid_fill_color", paint, "stale-target", True)
            if solid_fill_read:
                read_fill(current, "stale-target", True)
                applied = read_fill(changed, "ok")
                assert applied["rgb"] == fill_hex and applied["alpha"] == 1
                assert invoke("inspect", {}, "ok")["target"] == changed
            undone = invoke("undo", {"target":changed}, "changed")["target"]
            current = invoke("redo", {"target":undone}, "changed")["target"]
            assert invoke("set_solid_fill_color", {**paint, "target":current}, "unchanged")["target"] == current
        if artwork_gaps:
            gap_ids = [item[2] for item in made]
            current = invoke("set_layer_lock", dict(target=current, layerId=gap_ids[1], locked=True),
                             "changed")["target"]
            arrange = dict(target=current, layerIds=gap_ids, action="gap-x", spacing=None)
            invoke("distribute_artwork_gaps", arrange, "rejected", True)
            assert invoke("inspect", {}, "ok")["target"] == current
            current = invoke("set_layer_lock", dict(target=current, layerId=gap_ids[1], locked=False),
                             "changed")["target"]
            arrange["target"] = current
            invoke("distribute_artwork_gaps", {**arrange, "layerIds":gap_ids[:2]}, "invalid-arguments", True)
            auto = invoke("distribute_artwork_gaps", arrange, "changed")["target"]
            assert auto["projectRevision"] == current["projectRevision"] + 1
            invoke("distribute_artwork_gaps", arrange, "stale-target", True)
            assert invoke("distribute_artwork_gaps", {**arrange, "target":auto},
                          "unchanged")["target"] == auto
            fixed = dict(target=auto, layerIds=gap_ids[:2], action="gap-x", spacing=25)
            fixed_result = invoke("distribute_artwork_gaps", fixed, "changed")["target"]
            undone = invoke("undo", {"target":fixed_result}, "changed")["target"]
            assert invoke("distribute_artwork_gaps", {**arrange, "target":undone},
                          "unchanged")["target"] == undone
            current = invoke("redo", {"target":undone}, "changed")["target"]
            assert invoke("distribute_artwork_gaps", {**fixed, "target":current},
                          "unchanged")["target"] == current
            if artwork_gaps_auto:
                current = invoke("undo", {"target":current}, "changed")["target"]
            kind, point, layer, node = made[1]
            saved_middle_x = 500 if artwork_gaps_auto else 495
            made[1] = (kind, [saved_middle_x, point[1]], layer, node)
        if artwork_distribute:
            ordered = [item[2] for item in made]
            middle_id = ordered[1]
            current = invoke("set_layer_lock", dict(target=current, layerId=middle_id, locked=True),
                             "changed")["target"]
            arrange = dict(target=current, layerIds=ordered, action="center-x")
            invoke("distribute_artwork", arrange, "rejected", True)
            assert invoke("inspect", {}, "ok")["target"] == current
            current = invoke("set_layer_lock", dict(target=current, layerId=middle_id, locked=False),
                             "changed")["target"]
            arrange["target"] = current
            invoke("distribute_artwork", {**arrange, "layerIds":ordered[:2]}, "invalid-arguments", True)
            changed = invoke("distribute_artwork", arrange, "changed")["target"]
            assert changed["projectRevision"] == current["projectRevision"] + 1
            invoke("distribute_artwork", arrange, "stale-target", True)
            assert invoke("distribute_artwork", {**arrange, "target":changed},
                          "unchanged")["target"] == changed
            undone = invoke("undo", {"target":changed}, "changed")["target"]
            current = invoke("redo", {"target":undone}, "changed")["target"]
            kind, point, layer, node = made[1]
            made[1] = (kind, [600, point[1]], layer, node)
        if artwork_align:
            [aligned_id, other_id] = [item[2] for item in made]
            align = dict(target=current, layerIds=[aligned_id], action="center-x", reference="artboard")
            invoke("align_artwork", {**align, "reference":"selection"}, "invalid-arguments", True)
            current = invoke("set_layer_lock", dict(target=current, layerId=other_id, locked=True),
                             "changed")["target"]
            align["target"] = current
            invoke("align_artwork", {**align, "reference":"selection", "layerIds":[aligned_id, other_id]},
                   "rejected", True)
            assert invoke("inspect", {}, "ok")["target"] == current
            aligned = invoke("align_artwork", align, "changed")["target"]
            assert aligned["projectRevision"] == current["projectRevision"] + 1
            invoke("align_artwork", align, "stale-target", True)
            assert invoke("align_artwork", {**align, "target":aligned}, "unchanged")["target"] == aligned
            undone = invoke("undo", {"target":aligned}, "changed")["target"]
            current = invoke("redo", {"target":undone}, "changed")["target"]
            kind, point, layer, node = made[0]
            made[0] = (kind, [480, point[1]], layer, node)  # 960 px fixture artboard.
        if artwork_translate:
            moved_ids = [item[2] for item in made]
            delta = [12.5, -8]
            current = invoke("set_layer_lock", dict(target=current, layerId=moved_ids[1], locked=True),
                             "changed")["target"]
            translate = dict(target=current, layerIds=moved_ids, delta=delta)
            invoke("translate_artwork", translate, "rejected", True)
            assert invoke("inspect", {}, "ok")["target"] == current
            current = invoke("set_layer_lock", dict(target=current, layerId=moved_ids[1], locked=False),
                             "changed")["target"]
            translate["target"] = current
            invoke("translate_artwork", {**translate, "layerIds":[moved_ids[0], "absent"]},
                   "unsupported-target", True)
            assert invoke("translate_artwork", {**translate, "delta":[0, 0]},
                          "unchanged")["target"] == current
            moved = invoke("translate_artwork", translate, "changed")["target"]
            assert moved["projectRevision"] == current["projectRevision"] + 1
            invoke("translate_artwork", translate, "stale-target", True)
            undone = invoke("undo", {"target":moved}, "changed")["target"]
            current = invoke("redo", {"target":undone}, "changed")["target"]
            made = [(kind, [p+d for p,d in zip(point, delta)], layer, node)
                    for kind,point,layer,node in made]
            if artwork_authoring_roundtrip:
                read_fill(translate["target"], "stale-target", True)
                after_motion = read_fill(current, "ok")
                assert (after_motion["rgb"], after_motion["alpha"]) == (fill_hex, initial_fill["alpha"])
                assert after_motion["target"] == current
        if layer_state:
            def read_layer_state(revision, lid, *, visible=True, locked=False, guide=False, solo=False):
                reply = invoke("read_layer_state", dict(target=revision, layerId=lid), "ok")
                assert reply == dict(status="ok", target=revision, layerId=lid,
                                     visible=visible, locked=locked, guide=guide, solo=solo), reply
            read_layer_state(current, source_layer)
            invoke("read_layer_state", dict(target=current, layerId="absent"), "unsupported-target", True)
            invoke("read_layer_state", dict(target=current, layerId=source_layer, includeNames=True),
                   "invalid-arguments", True)
            assert invoke("inspect", {}, "ok")["target"] == current
        if layer_guide:
            [guide_id, retained_id] = [item[2] for item in made]
            if layer_state:
                read_layer_state(current, guide_id)
            before = current
            convert = dict(target=current, layerId=guide_id, guide=True)
            invoke("set_layer_guide", {**convert, "guide": 1}, "invalid-arguments", True)
            invoke("set_layer_guide", {**convert, "layerId":"absent"}, "unsupported-target", True)
            assert invoke("set_layer_guide", {**convert, "guide":False}, "unchanged")["target"] == current
            current = invoke("set_layer_guide", convert, "changed")["target"]
            assert current["projectRevision"] == before["projectRevision"] + 1
            invoke("set_layer_guide", convert, "stale-target", True)
            if layer_state:
                invoke("read_layer_state", dict(target=before, layerId=guide_id), "stale-target", True)
                read_layer_state(current, guide_id, guide=True)
            assert invoke("set_layer_guide", {**convert, "target":current}, "unchanged")["target"] == current
            current = invoke("set_layer_guide", {**convert, "target":current, "guide":False}, "changed")["target"]
            restored = invoke("undo", {"target":current}, "changed")["target"]
            assert invoke("set_layer_guide", {**convert, "target":restored}, "unchanged")["target"] == restored
            redone = invoke("redo", {"target":restored}, "changed")["target"]
            assert invoke("set_layer_guide", {**convert, "target":redone, "guide":False},
                          "unchanged")["target"] == redone
            current = invoke("undo", {"target":redone}, "changed")["target"]
            current = invoke("set_layer_lock", dict(target=current, layerId=guide_id, locked=True), "changed")["target"]
            invoke("set_layer_guide", {**convert, "target":current, "guide":False}, "rejected", True)
            assert invoke("inspect", {}, "ok")["target"] == current
        if layer_solo:
            [rect_id, text_id] = [item[2] for item in made]
            if layer_state:
                read_layer_state(current, rect_id)
            original = current
            first = dict(target=current, layerId=rect_id, solo=True)
            invoke("set_layer_solo", {**first, "solo": 1}, "invalid-arguments", True)
            invoke("set_layer_solo", {**first, "toggle":True}, "invalid-arguments", True)
            invoke("set_layer_solo", {**first, "layerId":"absent"}, "unsupported-target", True)
            assert invoke("set_layer_solo", {**first, "solo":False}, "unchanged")["target"] == current
            current = invoke("set_layer_solo", first, "changed")["target"]
            assert current["projectRevision"] == original["projectRevision"] + 1
            invoke("set_layer_solo", first, "stale-target", True)
            if layer_state:
                invoke("read_layer_state", dict(target=original, layerId=rect_id), "stale-target", True)
                read_layer_state(current, rect_id, solo=True)
            current = invoke("set_layer_solo", dict(target=current, layerId=text_id, solo=True), "changed")["target"]
            undone = invoke("undo", {"target":current}, "changed")["target"]
            assert invoke("set_layer_solo", dict(target=undone, layerId=text_id, solo=False),
                          "unchanged")["target"] == undone
            current = invoke("redo", {"target":undone}, "changed")["target"]
            assert invoke("set_layer_solo", dict(target=current, layerId=text_id, solo=True),
                          "unchanged")["target"] == current
            current = invoke("set_layer_visibility", dict(target=current, layerId=text_id, visible=False),
                             "changed")["target"]
        if layer_folder_plan:
            original_plan_order = [source_layer, made[0][2], made[1][2]]
            planned_order = [source_layer, made[1][2], made[0][2]]
            plan_folder_name = "Synthetic Reviewed Folder"
            grouped = invoke("create_layer_folder", dict(
                target=current, name=plan_folder_name, layerIds=original_plan_order[:1] if folder_plan_move_order else original_plan_order, folderIds=[]
            ), "changed")
            current = grouped["target"]
            folder = grouped["folderId"]
            mode = "move" if folder_plan_move_order else "remove"
            chosen = made[1][2] if folder_plan_move_order else made[0][2]
            plan_args = dict(target=current, request=dict(mode=mode, folderId=folder, layerIds=[chosen]))
            superseded = invoke("plan_layer_folder", plan_args, "ok")
            plan = invoke("plan_layer_folder", plan_args, "ok")
            assert plan["before"] == original_plan_order and plan["after"] == planned_order, plan
            assert plan["membership"] == [dict(folderId=folder, layerIds=planned_order[:2])], plan
            assert plan["target"] == current and invoke("inspect", {}, "ok")["target"] == current
            apply = dict(target=current, planId=plan["planId"], confirmed=True)
            invoke("apply_layer_folder_plan", {**apply, "planId": superseded["planId"]}, "unknown-plan", True)
            invoke("apply_layer_folder_plan", {**apply, "confirmed": False}, "invalid-arguments", True)
            assert invoke("inspect", {}, "ok")["target"] == current
            applied = invoke("apply_layer_folder_plan", apply, "changed")["target"]
            assert applied["projectRevision"] == current["projectRevision"] + 1
            invoke("apply_layer_folder_plan", apply, "unknown-plan", True)
            assert invoke("inspect", {}, "ok")["target"] == applied
            undone = invoke("undo", {"target": applied}, "changed")["target"]
            restored_plan = invoke("plan_layer_folder", {**plan_args, "target": undone}, "ok")
            assert restored_plan["before"] == original_plan_order and restored_plan["after"] == planned_order
            current = invoke("redo", {"target": undone}, "changed")["target"]
            invoke("apply_layer_folder_plan", dict(target=undone, planId=restored_plan["planId"], confirmed=True),
                   "stale-target", True)
            expected_plan_members = planned_order[:2]
            if folder_plan_move_order:
                order_args = dict(target=current, request=dict(mode="order", folderId=folder, direction="forward"))
                order_plan = invoke("plan_layer_folder", order_args, "ok")
                final_order = [planned_order[-1], *planned_order[:2]]
                assert order_plan["before"] == planned_order and order_plan["after"] == final_order, order_plan
                assert order_plan["membership"] == [] and order_plan["orderChanged"] is True
                assert invoke("inspect", {}, "ok")["target"] == current
                order_apply = dict(target=current, planId=order_plan["planId"], confirmed=True)
                ordered = invoke("apply_layer_folder_plan", order_apply, "changed")["target"]
                assert ordered["projectRevision"] == current["projectRevision"] + 1
                invoke("apply_layer_folder_plan", order_apply, "unknown-plan", True)
                undone = invoke("undo", {"target": ordered}, "changed")["target"]
                assert invoke("plan_layer_folder", {**order_args, "target": undone}, "ok")["before"] == planned_order
                current = invoke("redo", {"target": undone}, "changed")["target"]
                at_edge = invoke("plan_layer_folder", {**order_args, "target": current}, "ok")
                assert at_edge["before"] == at_edge["after"] == final_order and not at_edge["orderChanged"], at_edge
                assert invoke("apply_layer_folder_plan", dict(target=current, planId=at_edge["planId"], confirmed=True),
                              "unchanged")["target"] == current
                planned_order = final_order
        if layer_reorder:
            created_layer = made[0][2]

            def layer_order(id_, revision):
                reply = list_page(id_, None, revision)
                assert not reply["isError"], reply
                data = reply["structuredContent"]
                assert data["target"] == revision and data["nextOffset"] is None, data
                assert [item["layerIndex"] for item in data["items"]] == list(range(len(data["items"])))
                return [item["layerId"] for item in data["items"]]

            ordered = layer_order(1501, current)
            assert ordered == [source_layer, created_layer], ordered
            next_order = [created_layer, source_layer]
            move = dict(target=current, layerId=source_layer, beforeLayerId=None)
            invoke("reorder_layer", {**move, "beforeLayerId": 1}, "invalid-arguments", True)
            invoke("reorder_layer", {**move, "beforeLayerId": "missing"}, "unsupported-target", True)
            assert invoke("reorder_layer", {**move, "beforeLayerId": created_layer},
                          "unchanged")["target"] == current
            moved = invoke("reorder_layer", move, "changed")["target"]
            assert moved["projectRevision"] == current["projectRevision"] + 1
            assert layer_order(1502, moved) == next_order
            invoke("reorder_layer", move, "stale-target", True)
            assert invoke("reorder_layer", {**move, "target": moved}, "unchanged")["target"] == moved
            undone = invoke("undo", {"target": moved}, "changed")["target"]
            assert layer_order(1503, undone) == ordered
            current = invoke("redo", {"target": undone}, "changed")["target"]
            assert layer_order(1504, current) == next_order
        if layer_delete:
            def list_exact(id_, revision, layer_index=None):
                reply = list_page(id_, layer_index, revision)
                assert not reply["isError"], reply
                data = reply["structuredContent"]
                assert data["target"] == revision and data["nextOffset"] is None, data
                return data["items"]
        if layer_delete_member:
            [rect_id, text_id] = [item[2] for item in made]
            grouped_order = [source_layer, rect_id, text_id]
            survivor_order = [source_layer, text_id]
            group_name = "Synthetic Deletion Folder"
            grouped = invoke("create_layer_folder", dict(
                target=current, name=group_name, layerIds=grouped_order, folderIds=[]
            ), "changed")
            folder_id, current = grouped["folderId"], grouped["target"]
            assert [x["layerId"] for x in list_exact(1820, current)] == grouped_order
            original_graphs = [list_exact(1821+i, current, i) for i in range(3)]
            request = dict(target=current, layerId=rect_id, confirmed=True)
            next_id += 1
            send(dict(id=next_id, method="tools/call",
                      params={"name":"nect_test_delete_layer", "arguments":request}))
            assert replies.get(timeout=12) is not None  # Discard acknowledgement.
            observed = invoke("inspect", {}, "ok")["target"]
            assert observed["projectRevision"] == current["projectRevision"] + 1
            assert [x["layerId"] for x in list_exact(1824, observed)] == survivor_order
            invoke("delete_layer", request, "stale-target", True)
            undone = invoke("undo", {"target":observed}, "changed")["target"]
            assert [x["layerId"] for x in list_exact(1825, undone)] == grouped_order
            assert [list_exact(1826+i, undone, i) for i in range(3)] == original_graphs
            current = invoke("redo", {"target":undone}, "changed")["target"]
            assert [x["layerId"] for x in list_exact(1829, current)] == survivor_order
        elif layer_delete:
            assert [x["layerId"] for x in list_exact(1800, current)] == [source_layer]
            original_nodes = list_exact(1801, current, 0)
            request = dict(target=current, layerId=source_layer, confirmed=True)
            invoke("delete_layer", {**request, "confirmed":False}, "invalid-arguments", True)
            invoke("delete_layer", {**request, "layerId":"absent"}, "unsupported-target", True)
            assert invoke("inspect", {}, "ok")["target"] == current
            next_id += 1
            send(dict(id=next_id, method="tools/call",
                      params={"name":"nect_test_delete_layer","arguments":request}))
            assert replies.get(timeout=12) is not None  # Discard acknowledgement.
            observed = invoke("inspect", {}, "ok")["target"]
            assert observed["projectRevision"] == current["projectRevision"] + 1
            [blank] = list_exact(1802, observed)
            blank_id = blank["layerId"]
            assert blank_id != source_layer
            [output] = list_exact(1803, observed, 0)
            assert output["type"] == "Output"
            invoke("delete_layer", request, "stale-target", True)
            undone = invoke("undo", {"target":observed}, "changed")["target"]
            assert [x["layerId"] for x in list_exact(1804, undone)] == [source_layer]
            assert list_exact(1805, undone, 0) == original_nodes
            if layer_delete_restore:
                current = undone
            else:
                current = invoke("redo", {"target":undone}, "changed")["target"]
                assert [x["layerId"] for x in list_exact(1806, current)] == [blank_id]
                assert list_exact(1807, current, 0) == [output]
        if layer_create_blank:
            def blank_listing(id_, revision, layer_index=None):
                reply = list_page(id_, layer_index, revision)
                assert not reply["isError"], reply
                data = reply["structuredContent"]
                assert data["target"] == revision and data["nextOffset"] is None, data
                return data["items"]
            blank_name = "Synthetic Empty Layer"
            request = dict(target=current, name=blank_name)
            invoke("create_blank_layer", {**request, "name":" "}, "invalid-arguments", True)
            invoke("create_blank_layer", {**request, "path":"not-an-argument"}, "invalid-arguments", True)
            assert [x["layerId"] for x in blank_listing(1701, current)] == [source_layer]
            next_id += 1
            send(dict(id=next_id, method="tools/call", params={"name":"nect_test_create_blank_layer","arguments":request}))
            assert replies.get(timeout=12) is not None  # Discard this reply and reconcile by reading.
            observed = invoke("inspect", {}, "ok")["target"]
            assert observed["projectRevision"] == current["projectRevision"] + 1
            [prior, blank] = blank_listing(1702, observed)
            assert prior["layerId"] == source_layer and blank["layerId"] != source_layer
            empty_id = blank["layerId"]
            [out] = blank_listing(1703, observed, 1)
            assert out["type"] == "Output"
            out_id = out["nodeId"]
            invoke("create_blank_layer", request, "stale-target", True)
            undone = invoke("undo", {"target":observed}, "changed")["target"]
            assert [x["layerId"] for x in blank_listing(1704, undone)] == [source_layer]
            current = invoke("redo", {"target":undone}, "changed")["target"]
            assert [x["layerId"] for x in blank_listing(1705, current)] == [source_layer, empty_id]
        if layer_duplicate:
            def layer_ids(id_, revision):
                reply = list_page(id_, None, revision)
                assert not reply["isError"], reply
                data = reply["structuredContent"]
                assert data["target"] == revision and data["nextOffset"] is None, data
                return [item["layerId"] for item in data["items"]]

            assert layer_ids(1601, current) == [source_layer]
            duplicate = dict(target=current, layerId=source_layer)
            invoke("duplicate_layer", {**duplicate, "layerId": "missing"}, "unsupported-target", True)
            invoke("duplicate_layer", {**duplicate, "token": "ignored"}, "invalid-arguments", True)
            next_id += 1
            send(dict(id=next_id, method="tools/call", params={"name":"nect_test_duplicate_layer","arguments":duplicate}))
            assert replies.get(timeout=12) is not None  # Intentionally do not parse this reply.
            observed = invoke("inspect", {}, "ok")["target"]
            assert observed["projectRevision"] == current["projectRevision"] + 1
            after_ids = layer_ids(1602, observed)
            assert len(after_ids) == 2 and after_ids[0] == source_layer
            copied_layer = after_ids[1]
            assert copied_layer != source_layer
            invoke("duplicate_layer", duplicate, "stale-target", True)
            assert layer_ids(1603, observed) == after_ids
            undone = invoke("undo", {"target": observed}, "changed")["target"]
            assert layer_ids(1604, undone) == [source_layer]
            current = invoke("redo", {"target": undone}, "changed")["target"]
            assert layer_ids(1605, current) == after_ids
        if hidden_roundtrip:
            current=invoke("set_layer_visibility",dict(target=current,layerId=source_layer,visible=False),"changed")["target"]
        if locked_roundtrip:
            current=invoke("set_layer_lock",dict(target=current,layerId=source_layer,locked=True),"changed")["target"]
        if renamed_roundtrip:
            current=invoke("rename_layer",dict(target=current,layerId=source_layer,name=label),"changed")["target"]
        if create_layer_folder:
            create_folder = dict(target=current, name="Synthetic New Folder", layerIds=[source_layer], folderIds=[])
            invoke("create_layer_folder", {**create_folder, "folderId":"invented"}, "invalid-arguments", True)
            invoke("create_layer_folder", {**create_folder, "layerIds":["absent"]}, "rejected", True)
            assert invoke("inspect",{},"ok")["target"] == current
            next_id += 1
            send(dict(id=next_id, method="tools/call",
                      params={"name":"nect_test_create_layer_folder","arguments":create_folder}))
            assert replies.get(timeout=12) is not None  # Deliberately discard the creation result.
            current = invoke("inspect",{},"ok")["target"]
            assert current["projectRevision"] == create_folder["target"]["projectRevision"] + 1
            new_items = invoke("list_layer_folders",{"target":current,"offset":0},"ok")["items"]
            [created_folder] = [x["folderId"] for x in new_items if x["kind"] == "folder" and
                                x["parentFolderId"] == "fixture-folder-child"]
            assert new_items == folder_items[:2] + [
                dict(kind="folder", folderId=created_folder, parentFolderId="fixture-folder-child"),
                dict(kind="membership", folderId=created_folder, layerId=source_layer),
            ]
            invoke("create_layer_folder",create_folder,"stale-target",True)
            undone = invoke("undo",{"target":current},"changed")["target"]
            assert invoke("list_layer_folders",{"target":undone,"offset":0},"ok")["items"] == folder_items
            current = invoke("redo",{"target":undone},"changed")["target"]
            assert invoke("list_layer_folders",{"target":current,"offset":0},"ok")["items"] == new_items
            folder_items = new_items
        if rename_layer_folder:
            renaming = dict(target=current, folderId="fixture-folder-child", expectedName="Synthetic Child",
                            name="Renamed Synthetic Child")
            invoke("rename_layer_folder",{**renaming,"expectedName":"Incorrect"},"name-conflict",True)
            assert invoke("rename_layer_folder",{**renaming,"name":"Synthetic Child"},
                          "unchanged")["target"] == current
            renamed = invoke("rename_layer_folder",renaming,"changed")["target"]
            assert renamed["projectRevision"] == current["projectRevision"] + 1
            invoke("rename_layer_folder",renaming,"stale-target",True)
            assert invoke("list_layer_folders",{"target":renamed,"offset":0},"ok")["items"] == folder_items
            undone = invoke("undo",{"target":renamed},"changed")["target"]
            assert invoke("rename_layer_folder",{**renaming,"target":undone,"name":"Synthetic Child"},
                          "unchanged")["target"] == undone
            current = invoke("redo",{"target":undone},"changed")["target"]
            assert invoke("rename_layer_folder",{**renaming,"target":current,"expectedName":renaming["name"]},
                          "unchanged")["target"] == current
        if change_folder_membership:
            membership = dict(target=current, folderId="fixture-folder-flat", mode="add", layerIds=[source_layer])
            invoke("change_layer_folder_membership",{**membership,"mode":"remove"},"rejected",True)
            invoke("change_layer_folder_membership",{**membership,"expectedPaintOrder":[]},"invalid-arguments",True)
            assert invoke("inspect",{},"ok")["target"] == current
            added = invoke("change_layer_folder_membership",membership,"changed")["target"]
            assert added["projectRevision"] == current["projectRevision"] + 1
            added_items = folder_items + [dict(kind="membership",folderId=membership["folderId"],layerId=source_layer)]
            assert invoke("list_layer_folders",{"target":added,"offset":0},"ok")["items"] == added_items
            invoke("change_layer_folder_membership",membership,"stale-target",True)
            removed = invoke("change_layer_folder_membership",{**membership,"target":added,"mode":"remove"},
                             "changed")["target"]
            assert invoke("list_layer_folders",{"target":removed,"offset":0},"ok")["items"] == folder_items
            undone = invoke("undo",{"target":removed},"changed")["target"]
            assert invoke("list_layer_folders",{"target":undone,"offset":0},"ok")["items"] == added_items
            redone = invoke("redo",{"target":undone},"changed")["target"]
            assert invoke("list_layer_folders",{"target":redone,"offset":0},"ok")["items"] == folder_items
            current = invoke("undo",{"target":redone},"changed")["target"]
            folder_items = added_items
        if dissolve_layer_folder:
            dissolve = dict(target=current,folderId="fixture-folder-child")
            invoke("dissolve_layer_folder",{**dissolve,"deleteChildren":True},"invalid-arguments",True)
            invoke("dissolve_layer_folder",{**dissolve,"folderId":"absent"},"unsupported-target",True)
            assert invoke("inspect",{},"ok")["target"] == current
            dissolved = invoke("dissolve_layer_folder",dissolve,"changed")["target"]
            assert dissolved["projectRevision"] == current["projectRevision"] + 1
            promoted = [folder_items[0],
                        dict(kind="folder",folderId=created_folder,parentFolderId="fixture-folder-parent"),
                        folder_items[-1]]
            assert invoke("list_layer_folders",{"target":dissolved,"offset":0},"ok")["items"] == promoted
            invoke("dissolve_layer_folder",dissolve,"stale-target",True)
            undone = invoke("undo",{"target":dissolved},"changed")["target"]
            assert invoke("list_layer_folders",{"target":undone,"offset":0},"ok")["items"] == folder_items
            current = invoke("redo",{"target":undone},"changed")["target"]
            assert invoke("list_layer_folders",{"target":current,"offset":0},"ok")["items"] == promoted
            folder_items = promoted
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
        if layer_visibility:
            # Identical layer IDs in the reopened copy must not revive edit authority.
            invoke("set_layer_visibility",dict(target=current,layerId=source_layer,visible=hidden_roundtrip),
                   "stale-target",True)
        if graph_wires:
            invoke("read_node_input_wires",dict(target=current,layerId=source_layer,nodeId=source_node,offset=0),
                   "stale-target",True)
        if layer_lock:
            invoke("set_layer_lock",dict(target=current,layerId=source_layer,locked=False),
                   "stale-target",True)
            assert invoke("set_layer_lock",dict(target=reopened,layerId=source_layer,locked=locked_roundtrip),
                          "unchanged")["target"]==reopened
            listed_again = list_page(1310, None, reopened)
            assert not listed_again["isError"], listed_again
            [reopened_layer] = [x for x in listed_again["structuredContent"]["items"] if x["layerId"]==source_layer]
            assert reopened_layer == {**address, "locked": locked_roundtrip, "visible": not hidden_roundtrip}
        if renamed_roundtrip:
            invoke("rename_layer",dict(target=current,layerId=source_layer,name=original_name),"stale-target",True)
            assert invoke("rename_layer",dict(target=reopened,layerId=source_layer,name=label),"unchanged")["target"]==reopened
        if layer_reorder:
            invoke("reorder_layer", {**move, "target": current}, "stale-target", True)
            assert invoke("reorder_layer", {**move, "target": reopened}, "unchanged")["target"] == reopened
            assert layer_order(1505, reopened) == next_order
        if solid_fill:
            assert fill_identity(reopened) == fill_node
            if solid_fill_read:
                read_fill(current, "stale-target", True)
                saved_fill = read_fill(reopened, "ok")
                assert saved_fill["rgb"] == fill_hex and saved_fill["alpha"] == initial_fill["alpha"]
                assert saved_fill["target"] == reopened and saved_fill["editable"]
            invoke("set_solid_fill_color", {**paint, "target":current}, "stale-target", True)
            assert invoke("set_solid_fill_color", {**paint, "target":reopened},
                          "unchanged")["target"] == reopened
        if artwork_gaps_auto:
            invoke("distribute_artwork_gaps", {**arrange, "target":current}, "stale-target", True)
            assert invoke("distribute_artwork_gaps", {**arrange, "target":reopened},
                          "unchanged")["target"] == reopened
        if layer_delete_member:
            invoke("delete_layer", {**request, "target":current}, "stale-target", True)
            assert [x["layerId"] for x in list_exact(1830, reopened)] == survivor_order
            assert [list_exact(1831+i, reopened, i) for i in range(2)] == original_graphs[::2]
        elif layer_delete:
            invoke("delete_layer", {**request, "target":current}, "stale-target", True)
            saved_layer = source_layer if layer_delete_restore else blank_id
            assert [x["layerId"] for x in list_exact(1808, reopened)] == [saved_layer]
            assert list_exact(1809, reopened, 0) == (original_nodes if layer_delete_restore else [output])
        if layer_create_blank:
            invoke("create_blank_layer", {**request, "target":current}, "stale-target", True)
            assert [x["layerId"] for x in blank_listing(1706, reopened)] == [source_layer, empty_id]
            assert blank_listing(1707, reopened, 1) == [dict(nodeId=out_id, type="Output")]
        if layer_duplicate:
            invoke("duplicate_layer", {**duplicate, "target": current}, "stale-target", True)
            assert layer_ids(1606, reopened) == after_ids
        if layer_visibility or graph_wires or layer_lock or renamed_roundtrip or layer_reorder or layer_duplicate:
            assert invoke("inspect",{},"ok")["target"]==reopened
        if layer_state:
            invoke("read_layer_state", dict(target=current, layerId=source_layer), "stale-target", True)
            if layer_guide:
                read_layer_state(reopened, guide_id, locked=True, guide=True)
                read_layer_state(reopened, retained_id)
            if layer_solo:
                read_layer_state(reopened, rect_id, solo=True)
                read_layer_state(reopened, text_id, solo=True, visible=False)
            read_layer_state(reopened, source_layer)
            assert invoke("inspect", {}, "ok")["target"] == reopened
        if layer_guide:
            invoke("set_layer_guide", dict(target=current, layerId=guide_id, guide=False), "stale-target", True)
            invoke("set_layer_guide", dict(target=reopened, layerId=guide_id, guide=False), "rejected", True)
            assert invoke("set_layer_guide", dict(target=reopened, layerId=retained_id, guide=False),
                          "unchanged")["target"] == reopened
        if layer_solo:
            invoke("set_layer_solo", dict(target=current, layerId=rect_id, solo=False), "stale-target", True)
            for lid, flag in [(rect_id, True), (text_id, True), (source_layer, False)]:
                assert invoke("set_layer_solo", dict(target=reopened, layerId=lid, solo=flag),
                              "unchanged")["target"] == reopened
        if layer_folders:
            readback = folders(1177, reopened)
            assert not readback["isError"] and readback["structuredContent"] == dict(
                status="ok", target=reopened, items=folder_items, nextOffset=None), readback
        if hidden_roundtrip:
            unchanged=invoke("set_layer_visibility",dict(target=reopened,layerId=source_layer,visible=False),"unchanged")
            assert unchanged["target"]==reopened
            if list_objects:
                layers_reopened = list_page(1210, None, reopened)
                [hidden_layer] = [x for x in layers_reopened["structuredContent"]["items"] if x["layerId"]==source_layer]
                assert not layers_reopened["isError"] and hidden_layer["visible"] is False, layers_reopened
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
            data=(outputs/filename).read_bytes()
            digests[role]=hashlib.sha256(data).hexdigest()
            if renamed_roundtrip and role in ("copy", "svg"):
                new_name, old_name = label.encode("utf-8"), original_name.encode("utf-8")
                if role == "svg":
                    new_attr, old_attr = (b'data-layer="'+name+b'"' for name in (new_name, old_name))
                    assert new_attr in data
                    normalized = data.replace(new_attr, old_attr)
                else:
                    new_string, old_string = (b'"'+name+b'"' for name in (new_name, old_name))
                    assert data.count(new_string) == 1
                    normalized = data.replace(new_string, old_string)
                assert hashlib.sha256(normalized).hexdigest()==expected[role+"SHA256"].lower(), role
            elif layer_folders and role == "copy":
                snapshot = json.loads(data)
                [comp] = [c for c in snapshot["compositions"] if c["id"] == reopened["compositionId"]]
                expected_folders = [
                    dict(id="fixture-folder-parent", name="Synthetic Parent", members=[]),
                    dict(id="fixture-folder-child", name="Synthetic Child", parentId="fixture-folder-parent",
                         members=[dict(layerId=source_layer)]),
                ]
                if create_layer_folder:
                    expected_folders[1]["members"] = []
                    expected_folders.append(dict(id=created_folder, name="Synthetic New Folder",
                                                parentId="fixture-folder-child", members=[dict(layerId=source_layer)]))
                if rename_layer_folder:
                    expected_folders[1]["name"] = "Renamed Synthetic Child"
                if change_folder_membership:
                    expected_folders = [dict(id="fixture-folder-flat", name="Synthetic Flat",
                                             members=[dict(layerId=source_layer)])]
                if dissolve_layer_folder:
                    expected_folders[2]["parentId"] = "fixture-folder-parent"
                    del expected_folders[1]
                assert comp["editor"]["layerFolders"] == expected_folders
                assert [layer["id"] for layer in comp["layers"]] == [source_layer]
            elif layer_delete and not (layer_delete_restore or layer_delete_member):
                assert digests[role] != expected[role+"SHA256"].lower(), role
                if role == "copy":
                    snapshot = json.loads(data)
                    [comp] = [c for c in snapshot["compositions"] if c["id"] == reopened["compositionId"]]
                    [persisted] = comp["layers"]
                    assert persisted["id"] == blank_id and persisted["output"] == output["nodeId"]
                    assert [(n["id"], n["type"]) for n in persisted["nodes"]] == [(output["nodeId"], "Output")]
                elif role == "svg":
                    from xml.etree import ElementTree
                    canvas = ElementTree.fromstring(data)
                    assert not any("data-layer" in element.attrib for element in canvas.iter())
            elif layer_create_blank and role == "copy":
                snapshot = json.loads(data)
                [comp] = [c for c in snapshot["compositions"] if c["id"] == reopened["compositionId"]]
                assert [layer["id"] for layer in comp["layers"]] == [source_layer, empty_id]
                empty_layer = comp["layers"][1]
                assert empty_layer["name"] == blank_name and empty_layer["output"] == out_id
                assert [(n["id"], n["type"]) for n in empty_layer["nodes"]] == [(out_id, "Output")]
            elif layer_duplicate:
                if role != "png":
                    assert digests[role] != expected[role+"SHA256"].lower(), role
            elif artwork or hidden_roundtrip or (locked_roundtrip and role == "copy"):
                assert digests[role]!=expected[role+"SHA256"].lower(), role
            else:
                assert digests[role]==expected[role+"SHA256"].lower()
        if artwork:
            artifacts=run/"documents"
            snapshot=json.loads((artifacts/"live-api-project.json").read_bytes())
            [composition]=[x for x in snapshot["compositions"] if x["id"]==reopened["compositionId"]]
            for kind,point,layer_id,node_id in made:
                if layer_delete_member and layer_id == rect_id:
                    continue
                [shape_layer]=[x for x in composition["layers"] if x["id"]==layer_id]
                [shape]=[n for n in shape_layer["nodes"] if n["id"]==node_id]
                assert shape["type"]==kind, shape["type"]
                if kind in ("Rectangle", "Circle", "Ellipse"):
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
                tree = ElementTree.fromstring(svg)
                assert tree.tag.endswith("}svg")
                if solid_fill:
                    [painted] = [layer for layer in composition["layers"] if layer["id"] == fill_layer]
                    [fill] = [node for node in painted["nodes"] if node["id"] == fill_node]
                    assert fill["params"]["color"]["value"] == dict(hex=fill_hex, alpha=1)
                    [rectangle_paint] = [element for element in tree.iter()
                                         if element.get("fill", "").lower() == fill_hex]
                    assert rectangle_paint.get("data-layer") == painted["name"]
                    if solid_fill_read:
                        assert float(rectangle_paint.get("fill-opacity")) == initial_fill["alpha"]
                if layer_guide:
                    assert [l["id"] for l in composition["layers"]] == [source_layer, guide_id, retained_id]
                    source, guide_layer, text_layer = composition["layers"]
                    assert guide_layer["guide"] is True and guide_layer["locked"] is True and guide_layer["visible"]
                    assert not source.get("guide") and not text_layer.get("guide")
                    labels = {n.attrib["data-layer"] for n in tree.iter() if "data-layer" in n.attrib}
                    assert labels == {source["name"], text_layer["name"]}, labels
                if layer_solo:
                    assert [l["id"] for l in composition["layers"]] == [source_layer, rect_id, text_id]
                    source, rect, text_layer = composition["layers"]
                    assert not source.get("solo") and source["visible"]
                    assert rect["solo"] is True and rect["visible"]
                    assert text_layer["solo"] is True and not text_layer["visible"]
                    labels = {n.attrib["data-layer"] for n in tree.iter() if "data-layer" in n.attrib}
                    assert labels == {rect["name"]}, labels
                if layer_delete_member:
                    assert [l["id"] for l in composition["layers"]] == survivor_order
                    assert composition["editor"]["layerFolders"] == [
                        dict(id=folder_id, name=group_name, members=[dict(layerId=l) for l in survivor_order])
                    ]
                    expected_labels = [l["name"] for l in composition["layers"]]
                    assert len(set(expected_labels)) == 2
                    labels = list(dict.fromkeys(n.attrib["data-layer"] for n in tree.iter() if "data-layer" in n.attrib))
                    assert labels == expected_labels, labels
                if layer_folder_plan:
                    assert [l["id"] for l in composition["layers"]] == planned_order
                    assert composition["editor"]["layerFolders"] == [
                        dict(id=folder, name=plan_folder_name, members=[dict(layerId=l) for l in expected_plan_members])
                    ]
                    expected_labels = [l["name"] for l in composition["layers"]]
                    assert len(set(expected_labels)) == 3
                    labels = list(dict.fromkeys(n.attrib["data-layer"] for n in tree.iter() if "data-layer" in n.attrib))
                    assert labels == expected_labels, labels
                if layer_reorder:
                    assert [l["id"] for l in composition["layers"]] == next_order
                    expected_labels = [l["name"] for l in composition["layers"]]
                    assert len(set(expected_labels)) == 2
                    labels = list(dict.fromkeys(n.attrib["data-layer"] for n in tree.iter() if "data-layer" in n.attrib))
                    assert labels == expected_labels, labels
                assert png[:8]==b"\x89PNG\r\n\x1a\n" and png[12:16]==b"IHDR"
                assert (int.from_bytes(png[16:20],"big"),int.from_bytes(png[20:24],"big"))==(960,540)
        if hidden_roundtrip:
            from xml.etree import ElementTree
            snapshot=json.loads((outputs/"live-api-project.json").read_bytes())
            [composition]=[x for x in snapshot["compositions"] if x["id"]==reopened["compositionId"]]
            [persisted]=[x for x in composition["layers"] if x["id"]==source_layer]
            assert persisted["visible"] is False
            assert any(n["id"]==source_node for n in persisted["nodes"])
            svg=ElementTree.fromstring((outputs/"live-api-export.svg").read_bytes())
            assert svg.tag.endswith("}svg") and not svg.findall(".//{*}path")
            png=(outputs/"live-api-export.png").read_bytes()
            assert png[:8]==b"\x89PNG\r\n\x1a\n" and png[12:16]==b"IHDR"
            assert (int.from_bytes(png[16:20],"big"),int.from_bytes(png[20:24],"big"))==(960,540)
        if locked_roundtrip:
            snapshot=json.loads((outputs/"live-api-project.json").read_bytes())
            [composition]=[x for x in snapshot["compositions"] if x["id"]==reopened["compositionId"]]
            [persisted]=[x for x in composition["layers"] if x["id"]==source_layer]
            assert persisted["locked"] is True and persisted["visible"] is True
            assert any(n["id"]==source_node for n in persisted["nodes"])
        if renamed_roundtrip:
            snapshot=json.loads((outputs/"live-api-project.json").read_bytes())
            [composition]=[x for x in snapshot["compositions"] if x["id"]==reopened["compositionId"]]
            [persisted]=[x for x in composition["layers"] if x["id"]==source_layer]
            assert persisted["name"]==label
            assert any(n["id"]==source_node for n in persisted["nodes"])
        if layer_duplicate:
            from xml.etree import ElementTree
            snapshot = json.loads((outputs/"live-api-project.json").read_bytes())
            [composition] = [x for x in snapshot["compositions"] if x["id"] == reopened["compositionId"]]
            original, copied = composition["layers"]
            assert [original["id"], copied["id"]] == [source_layer, copied_layer]
            assert copied["name"] == original["name"] + " copy"
            assert any(n["id"] == source_node for n in original["nodes"])
            assert [n["type"] for n in copied["nodes"]] == [n["type"] for n in original["nodes"]]
            assert {n["id"] for n in copied["nodes"]}.isdisjoint(n["id"] for n in original["nodes"])
            original_by_id = {n["id"]: n for n in original["nodes"]}
            copied_by_id = {n["id"]: n for n in copied["nodes"]}
            remap = dict(zip(original_by_id, copied_by_id, strict=True))
            assert copied["output"] == remap[original["output"]]
            verified_wires = 0
            for original_id, copied_id in remap.items():
                old_params = original_by_id[original_id]["params"]
                new_params = copied_by_id[copied_id]["params"]
                for key, old_port in old_params.items():
                    if isinstance(old_port, dict) and old_port.get("source"):
                        assert old_port["source"] in remap, old_port
                        new_port = new_params[key]
                        assert new_port["source"] == remap[old_port["source"]]
                        assert new_port.get("sourcePort") == old_port.get("sourcePort")
                        verified_wires += 1
            assert verified_wires == 5
            tree = ElementTree.fromstring((outputs/"live-api-export.svg").read_bytes())
            labels = {n.attrib["data-layer"] for n in tree.iter() if "data-layer" in n.attrib}
            assert labels == {original["name"], copied["name"]}
            png = (outputs/"live-api-export.png").read_bytes()
            assert png[:8] == b"\x89PNG\r\n\x1a\n" and png[12:16] == b"IHDR"
            assert (int.from_bytes(png[16:20], "big"), int.from_bytes(png[20:24], "big")) == (960, 540)
        bound=run/"documents/live-api-save-as.json"
        assert hashlib.sha256(bound.read_bytes()).hexdigest()==digests["copy"]
        assert len(list((run/"documents").glob("live-api-save-as*")))==1
        evidence={"saveAs":"native binding on fixed test path","replyRecovery":"dropped, checked receipt, replay reused outcome",
                  "scope":"independent MCP stdio client, isolated hidden Windows Nect; not Hermes or normal product",
                  "protocol":hello["protocolVersion"],"tools":len(listed["tools"]),"run":run.name,
                  "edit":"changed","history":"undo/redo verified","saveCopy":"accepted-by-writer","reopen":"reopened",
                  "exportSvg":"unsupported-glyph" if mixed_text_probe else "accepted-by-writer",
                  "exportPng":"unsupported-glyph" if mixed_text_probe else "accepted-by-writer","caseGuards":"pass",
                  "artwork":created, "authoredObjectDiscovery":list_objects, "authoredNodeWireDiscovery":graph_wires, "layerVisibility":layer_visibility, "layerLock":layer_lock, "layerRename":layer_rename, "layerReorder":layer_reorder, "layerDuplicate":layer_duplicate, "layerFolderTopology":layer_folders, "renamedLayerRoundtrip":renamed_roundtrip, "lockedLayerRoundtrip":locked_roundtrip, "hiddenLayerRoundtrip":hidden_roundtrip, "embeddedJapaneseFixture":embedded_jp, "textEdit":text_edit,"japaneseDefaultFontNegativeProbe":mixed_text_probe, "digest":digests}
        if layer_duplicate:
            evidence["duplicateReplyRecovery"] = "discarded reply; one copy discovered; exact old target rejected"
            evidence["copiedInternalWires"] = verified_wires
        if create_layer_folder:
            evidence["layerFolderCreate"] = "one nested folder found after discarded reply; exact replay stale; Undo/Redo and reopen verified"
        if rename_layer_folder:
            evidence["layerFolderRename"] = "same ID and topology; old name and revision guarded; Undo/Redo and reopen verified"
        if change_folder_membership:
            evidence["layerFolderMembership"] = "add and remove one root layer; stale request rejected; Undo/Redo and reopen verified without paint-order change"
        if dissolve_layer_folder:
            evidence["layerFolderDissolve"] = "one intermediate folder dissolved; nested child and authored layer preserved; exact replay stale; Undo/Redo and reopen verified"
        if layer_folder_plan:
            key = "layerFolderReviewedMoveAndOrder" if folder_plan_move_order else "layerFolderReviewedRemoval"
            evidence[key] = dict(
                before=original_plan_order, afterPrimaryEdit=plan['after'], after=planned_order, folderId=folder,
                previewNonMutating=True, supersededAndReplayRejected=True,
                confirmationRequired=True, staleAfterRedoRejected=True,
                undoRedoAndReopenVerified=True, svgLayerOrderVerified=True,
            )
            if folder_plan_move_order:
                evidence[key]["edgeNoopVerified"] = True
        if layer_solo:
            evidence["layerSolo"] = "two persisted Solo flags; hidden Solo and non-Solo excluded from SVG; one-revision changes, stale rejection, Undo/Redo, and PNG export"
        if layer_guide:
            evidence["layerGuide"] = "guide conversion and restoration reuse the same layer; lock and stale rejection; Undo/Redo and persisted Guide excluded from SVG; PNG export"
        if layer_state:
            evidence["layerState"] = "exact stored flags before edit, after edit and reopen; stale and unsupported IDs rejected; revision unchanged by reads"
        if layer_create_blank:
            evidence["blankLayer"] = "one empty Output created; dropped reply reconciled by listing; stale replay refused; Undo/Redo, reopened IDs and byte-identical baseline SVG/PNG"
        if solid_fill:
            evidence["solidFill"] = dict(layerId=fill_layer, nodeId=fill_node, hex=fill_hex,
                lockedAndStaleRejected=True, undoRedoReopenAndSVGVerified=True)
        if solid_fill_read:
            evidence["solidFillReadback"] = dict(readOnly=True, staleRejected=True,
                lockedLiteralReadable=True, reopenedColor=saved_fill["rgb"], reopenedAlpha=saved_fill["alpha"])
        if artwork_authoring_roundtrip:
            evidence["artworkAuthoringRoundtrip"] = dict(
                sequence=["create", "recolor", "translate", "save", "reopen", "exportSvg", "exportPng"],
                rectangleCenter=made[0][1], textPosition=made[1][1],
                rectangleFill=fill_hex, sameLayerAndFillIdsAfterReopen=True,
                outputScope="SVG named layer and fill, PNG signature and canvas dimensions")
        if artwork_gaps:
            evidence["artworkGaps"] = dict(action="gap-x", endpointCenters=[260, 770],
                fixedSpacing=25, savedMiddleCenter=saved_middle_x, restoredAutomaticOnUndo=True,
                lockedAndStaleRejected=True, savedMode="auto" if artwork_gaps_auto else "fixed",
                differentEndpointWidthsAndReopenedNoOp=artwork_gaps_auto)
        if artwork_distribute:
            evidence["artworkDistribution"] = dict(action="center-x", endpoints=[430, 770],
                savedMiddleX=600, originalMiddleX=505, lockedAndStaleRejected=True,
                undoRedoAndReopenVerified=True)
        if artwork_align:
            evidence["artworkAlignment"] = dict(reference="artboard", action="center-x",
                alignedLayer=aligned_id, untouchedLayer=other_id, savedCenterX=480,
                lockedMixedSelectionRejected=True, noOpAndStaleVerified=True)
        if artwork_translate:
            evidence["artworkTranslation"] = dict(delta=delta, movedLayers=moved_ids,
                rejectsLockedAndStale=True, verifiedViaUndoRedoSavedCoordinates=True)
        if layer_delete_member:
            evidence["folderMemberDeletion"] = dict(
                folderId=folder_id, removedLayerId=rect_id, survivorIds=survivor_order,
                discardedReplyReconciledByID=True, oldRequestRefused=True,
                undoRestoredAllGraphs=True, redoAndReopenRetainedMembershipAndSvgOrder=True,
            )
        elif layer_delete:
            evidence["layerDeletion"] = dict(
                discardedReplyReconciledByID=True, oldLayerId=source_layer, blankLayerId=blank_id,
                oldRequestRefused=True, undoRestoredOriginalGraph=True,
                savedUndoWithBaselineOutputs=layer_delete_restore,
                redoAndReopenRetainedReplacement=not layer_delete_restore,
            )
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

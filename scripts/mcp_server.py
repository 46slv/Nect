"""MCP 2025-06-18 stdio adapter forwarding to one live Nect desktop Session.

Run: python scripts/mcp_server.py --endpoint <desktop pipe/socket>
The local JSON-lines Session API itself is not MCP. This process implements
initialize, initialized, ping, tools/list and tools/call; it never loads a document.
"""
import argparse
import json
import sys
from session_client import call, LIMIT

IDENTITY = {'session_id': {'type': 'string'}, 'document_id': {'type': 'string'}}
TOOLS = [
    {'name': 'nect_session', 'description': 'Read live desktop identity, revision and persistence receipts: pending/writing/native saved/recovery revisions and explicit errors.',
     'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
     'annotations': {'readOnlyHint': True, 'openWorldHint': False}},
    {'name': 'nect_command',
     'description': ('Inspect/evaluate/export_svg or edit the desktop-owned Session. request.op: '
                     'inspect, properties, get, resolve_name, evaluate, expression_language, compositing_types, compositing_plan, export_svg, artboards, operator_types, gradient_types, render_plan, conversion_plan, apply, undo, redo, history, restore_history. '
                     'history lists stable state IDs and bounded retained memory estimates for this live Session only. '
                     'restore_history takes state_id and expected_revision, atomically returns to a retained state in one revision, '
                     'and keeps future states available until a new edit replaces the redo branch. '
                     'apply requires expected_revision and commands. Commands include create_path, '
                     'add_point, remove_point, close_contour, set, link, unlink, rename, '
                     'reorder_points, reorder_objects, group_contiguous, delete_objects, create_primitive, '
                     'enable_point_edit, clear_point_edit, convert_to_path, add_operation, remove_operation, reorder_operations, '
                     'enable_operation, operation_options and set_gradient. operator_types and gradient_types return exact templates. '
                     'nect.shape.offset expands/contracts closed simple regions in object-local units; amount and miter_limit are normal properties. '
                     'operation_options accepts line_join miter/round/bevel for Offset; its composite must remain below. '
                     'Offset also deforms earlier paint geometry, preserving paint bases; open or intersecting contours reject explicitly. '
                     'primitive_types returns Circle/Rectangle/Polygon/Star source templates for create_primitive. '
                     'Polygon/Star generator.points is a linkable integer (Polygon 3..256, Star 2..256); rotation uses degrees. '
                     'Radius/outer_radius/inner_radius and center_x/center_y use local du. '
                     'Generated point IDs preserve reduced angular phase and outer/inner role across count changes. '
                     'Count changes reject if a corrected or referenced point disappears; they never retarget by list index. '
                     'clear_point_edit {object} explicitly removes all point overrides and their bindings in one undoable edit. '
                     'Anchor refs transform.anchor_x/y are local du; changing them preserves artwork. center_anchor {object} uses evaluated geometric bounds excluding stroke width. '
                     'set_position {object,x,y} sets the anchor position in effective-parent coordinates; transform_around_anchor {object,rotation,scale_x,scale_y} applies degree rotation and local-axis scale factors once about it. '
                     'These edit the canonical affine Scalars, reject changed driven fields, and do not create independent TRS properties. '
                     'set_transform_parent {object,parent:id|null,preserve_world:bool} replaces structural transform inheritance within the same Composition; null returns to the structural parent. '
                     'Keep-world parenting rejects singular new parents or driven fields that must change. Structure still owns ordering, grouping and selection. '
                     'transforms returns local/world matrices, effective parent, local anchor, parent-space position and world anchor for every object. '
                     'Batch commands: edit_properties {targets:[Ref],value:number,relative:bool}; link_properties {targets:[Ref],source:Ref,relative:bool}; unlink_properties {targets:[Ref]}. '
                     'Targets are 1..1000 unique compatible scalar refs. Absolute edits assign each target, relative edits add to each initial value once; driven edits reject until explicitly unlinked. '
                     'Relative links preserve each initial difference and unlink freezes each initial evaluated value; all target values come from one snapshot. '
                     'set_visibility {object,visible} controls ordinary artwork visibility independently of mask-source use. '
                     'set_compositing {object,blend,isolated} sets a supported blend/isolation; composite.opacity is a normal [0,1] Scalar. '
                     'compositing_types lists exact blends and geometry-mask semantics; compositing_plan {composition} reports ordered resolved scopes. '
                     'set_mask {object,mask:null|{id,source,version:1,enabled,fill_rule:"nonzero"|"evenodd"}} uses final Path/Text geometry in Composition space, ignoring source paint/opacity/visibility. '
                     'mask_objects {composition,parent,members:[ordered contiguous sibling IDs],id,mask_id,name,top:bool} wraps members in a masked Group and hides the topmost (last) or bottommost (first) source. '
                     'put_inside {composition,parent,group,members:[ordered IDs]} moves contiguous siblings immediately preceding group into its children before existing content, preserving world transforms; target effects intentionally apply. '
                     'Normal Groups pass through; opacity/blend/mask/nonneutral scopes isolate then apply to the aggregate. Full AE blend and alpha/luma mask parity are unsupported. '
                     'set_expression {targets:[Ref],expression:{source:string,version:1},replace_binding:bool} assigns a bounded pure expression to compatible scalars. '
                     'Use expression_language for limits/functions; ref("object-id","point-id-or-empty","field") uses stable IDs. '
                     'Typing numbers cannot replace a formula. Unlink freezes its result; link commands explicitly replace it. Existing binding replacement needs replace_binding:true. '
                     'Formula errors/cycles/units/ranges reject the whole command; expressions persist in native 0.13 and SVG contains evaluated values only. '
                     'align_objects {objects:[id],axis:x/y,alignment:min/center/max,artboard:null/id} aligns geometric bounds (excluding stroke) to initial selection envelope or an Artboard in the same Composition. Rejects structural ancestor/descendant overlap and driven/unpreservable changes atomically. '
                     'translate_objects {objects:[id],dx:number,dy:number} translates selected world matrices once, including selected ancestors/followers, in one Composition. '
                     'duplicate_objects {objects:[id],prefix:unused ID prefix of 1..48 characters} makes independent in-place copies, once per selected Group closure, in one Composition. '
                     'Internal bindings/expressions/masks/Transform Parents follow copied IDs; outgoing references, Named Colors and image assets stay shared. Existing inbound references and Collection membership stay unchanged. '
                     'Copies follow each selected sibling run in paint order. apply returns created_ids for new objects; inspect reads their hierarchy and fresh nested IDs. Use translate_objects explicitly to move copies. '
                     'set_gradient replaces the authored gradient on one paint; preserve its IDs and existing bindings when editing stops. '
                     'Gradient numeric refs are op.OP_ID.gradient.GRADIENT_ID.start_x/start_y/end_x/end_y or stop.STOP_ID.offset/r/g/b/a. '
                     'Artboard commands: add_artboard {composition,artboard,index}, update_artboard {composition,artboard}, '
                     'delete_artboard/detach_artboard_parent {composition,artboard:id}, reorder_artboards {composition,order:[ids]}. '
                     'Artboard fields are id,name,x,y,width,height and optional parent_size:{artboard:id,width:bool,height:bool}. '
                     'Size inheritance is same-composition; artboards readback returns authored/evaluated frames in export order. '
                     'Frame movement changes crops only; reorder changes order only; neither moves artwork. '
                     'Text: text_defaults returns a source template; text_fonts lists installed families. '
                     'create_text {composition,parent,id,name,source}; update_text {object,source} preserves source ID and existing Scalars. '
                     'Text source includes content,family,locale,weight,italic,layout auto/frame,direction horizontal/vertical,alignment start/center/end. '
                     'Numeric text.* refs: origin_x,origin_y,font_size,frame_width,frame_height,tracking,line_spacing (0=font default). '
                     'text_layout {object} reports bounds, overflow, actual fonts and warnings. export_plan {composition,artboard} discloses outlined SVG text; native text stays editable. '
                     'Typed colors: color_properties or properties lists aggregate color refs and four ordinary numeric channels; get supports both types. '
                     'used_colors inventories enabled paint inputs grouped by exact RGBA; equal values do not imply links. '
                     'create_named_color {color:{id,name,space:"srgb",profile:"srgb",alpha:"straight",rgba:[four Scalars]}}; '
                     'rename_named_color {color:id,name}; delete_named_color {color:id} rejects while referenced. '
                     'set_color {ref,value:{space:"srgb",profile:"srgb",alpha:"straight",rgba:[four numbers]}} rejects driven channels. '
                     'link_color {target,source} links all four channels; unlink_color {ref} freezes the resolved value. '
                     'Aggregate refs use point:"", field:"color" for a named color owner ID, op.OP.color for paint, or op.OP.gradient.GRAD.stop.STOP.color for a gradient stop. '
                     'Raster Images: assets lists metadata and placements (no byte payload); image.width/height are ordinary du Scalars. '
                     'create_image {composition,parent,id,name,source:{asset,width:Scalar,height:Scalar}} reuses an accepted asset. '
                     'delete_raster_asset {asset} rejects while placed. Use nect_image for local file import, check, reload, relink or embed. '
                     'Use properties to discover stable refs and units. All mutations are atomic and undoable.'),
     'inputSchema': {'type': 'object', 'properties': dict(IDENTITY, request={'type': 'object'}),
                     'required': ['session_id', 'document_id', 'request'], 'additionalProperties': False},
     'annotations': {'readOnlyHint': False, 'destructiveHint': True, 'openWorldHint': False}},
    {'name': 'nect_file',
     'description': ('Create/open/save a native document or protect recovery in the live desktop. '
                     'new/open/open_recovery rotate session identity; read returned identity before subsequent edits. '
                     'open protects outgoing work. open_recovery opens an unnamed copy without overwriting its source. '
                     'Committed edits live-save about once per second in the background. recover explicitly flushes committed protection. '
                     'save preserves previous file backups; external native changes reject with FILE_CHANGED, use another Save As path or reopen. '
                     'Read nect_session.persistence for verified per-destination revisions; queued data is not saved.'),
     'inputSchema': {'type': 'object', 'properties': dict(IDENTITY,
         op={'type': 'string', 'enum': ['new', 'open', 'open_recovery', 'save', 'recover']},
         expected_revision={'type': 'integer', 'minimum': 0}, path={'type': 'string'}),
         'required': ['session_id', 'document_id', 'op', 'expected_revision'], 'additionalProperties': False},
     'annotations': {'readOnlyHint': False, 'destructiveHint': True, 'openWorldHint': False}},
    {'name': 'nect_export_png',
     'description': 'Export committed artwork from one Artboard to an absolute local .png path. Shared Canvas compositing, no UI overlays. Atomic replacement; no native/history changes. Explicit scale in pixels per document unit (0 < scale <= 16), transparent or white background, 8-bit sRGB, maximum 8192 per axis / 16 MP. Active gestures and native/linked-source destinations reject.',
     'inputSchema': {'type':'object','properties':dict(IDENTITY,
         op={'type':'string','enum':['export_png']},expected_revision={'type':'integer','minimum':0},
         path={'type':'string'},composition={'type':'string'},artboard={'type':'string'},
         scale={'type':'number'},background={'type':'string','enum':['transparent','white']}),
         'required':['session_id','document_id','op','expected_revision','path','composition','artboard','scale','background'],'additionalProperties':False},
     'annotations':{'readOnlyHint':False,'destructiveHint':True,'openWorldHint':False}},
    {'name': 'nect_image',
     'description': ('Import PNG/JPEG as a Linked or Embedded asset with one Image placement, or manage an existing asset. '
                     'import_image requires path (absolute local Windows drive path), explicit mode linked/embedded, composition,parent (empty for root), '
                     'asset (new stable ID), id (new Image ID), name, x,y; placement starts at one du per oriented source pixel. '
                     'asset operation requires asset ID and action status/check/reload/relink/embed; relink also requires path. '
                     'status reads the last observation without filesystem access; check compares linked source bytes without accepting pixels or changing revision. '
                     'reload and relink atomically replace accepted bytes for ALL placements, keeping display dimensions and transforms. '
                     'embed keeps accepted cached bytes and clears the link; no file read. All authored changes are one Undo. '
                     'Native open never fetches links; current/changed/missing/unreadable are explicit check observations. '
                     'PNG/JPEG 8-bit RGB/gray/palette only, JPEG EXIF orientation and bounded ICC to sRGB. '
                     'Limits: 8 MiB and 16MP per source,8192 per axis;24 MiB and32MP per document;128 assets. '
                     'Unsupported CMYK,high-bit-depth,animation,PNG eXIf and unsupported color metadata reject without changes.'),
     'inputSchema': {'type':'object','properties':dict(IDENTITY,
         op={'type':'string','enum':['import_image','asset']},expected_revision={'type':'integer','minimum':0},
         path={'type':'string'},mode={'type':'string','enum':['linked','embedded']},composition={'type':'string'},parent={'type':'string'},
         asset={'type':'string'},id={'type':'string'},name={'type':'string'},x={'type':'number'},y={'type':'number'},
         action={'type':'string','enum':['status','check','reload','relink','embed']}),
         'required':['session_id','document_id','op','expected_revision','asset'],'additionalProperties':False},
     'annotations':{'readOnlyHint':False,'destructiveHint':False,'openWorldHint':False}},
]


class ProtocolError(Exception):
    def __init__(self, code, message):
        self.code, self.message = code, message


def unique(pairs):
    out = {}
    for key, value in pairs:
        if key in out:
            raise ValueError('Duplicate JSON key')
        out[key] = value
    return out


def run(endpoint):
    initialized = False
    ready = False
    for line in iter(lambda: sys.stdin.buffer.readline(LIMIT + 1), b''):
        request_id = None
        notification = False
        try:
            if len(line) > LIMIT:
                raise ProtocolError(-32700, 'Message exceeds 64 MiB')
            try:
                message = json.loads(line, object_pairs_hook=unique,
                    parse_constant=lambda _: (_ for _ in ()).throw(ValueError('Non-finite JSON number')))
            except (ValueError, UnicodeError):
                raise ProtocolError(-32700, 'Invalid JSON')
            if not isinstance(message, dict) or message.get('jsonrpc') != '2.0' or not isinstance(message.get('method'), str):
                raise ProtocolError(-32600, 'JSON-RPC 2.0 request object required')
            notification = 'id' not in message
            request_id = message.get('id')
            if not notification and (isinstance(request_id, bool) or not isinstance(request_id, (str, int))):
                raise ProtocolError(-32600, 'Request ID must be a string or integer')
            method = message['method']
            params = message.get('params', {})
            if not isinstance(params, dict):
                raise ProtocolError(-32602, 'Named parameters required')
            if method == 'notifications/initialized' and initialized:
                ready = True
                continue
            if notification:
                continue
            if method == 'ping':
                result = {}
            elif method == 'initialize':
                if initialized:
                    raise ProtocolError(-32600, 'Already initialized')
                if not isinstance(params.get('protocolVersion'), str) or not isinstance(params.get('capabilities'), dict) or not isinstance(params.get('clientInfo'), dict):
                    raise ProtocolError(-32602, 'Initialization requires protocolVersion, capabilities and clientInfo')
                initialized = True
                result = {'protocolVersion': '2025-06-18', 'capabilities': {'tools': {'listChanged': False}},
                          'serverInfo': {'name': 'nect-desktop', 'version': '0.1.0'},
                          'instructions': 'Read nect_session first. Keep identity and expected revision explicit. Never blindly retry a failed transport mutation.'}
            elif not ready:
                raise ProtocolError(-32002, 'Initialize and send notifications/initialized first')
            elif method == 'tools/list':
                if params:
                    raise ProtocolError(-32602, 'No pagination cursor is supported')
                result = {'tools': TOOLS}
            elif method == 'tools/call':
                name = params.get('name')
                arguments = params.get('arguments', {})
                tool = next((t for t in TOOLS if t['name'] == name), None)
                if tool is None:
                    raise ProtocolError(-32602, 'Unknown tool')
                if not isinstance(arguments, dict):
                    raise ProtocolError(-32602, 'Tool arguments must be an object')
                schema = tool['inputSchema']
                if set(arguments) - set(schema['properties']) or set(schema.get('required', [])) - set(arguments):
                    raise ProtocolError(-32602, 'Missing or unknown tool arguments')
                for key, value in arguments.items():
                    rule = schema['properties'][key]
                    expected = {'string': str, 'object': dict, 'integer': int, 'number': (int, float)}[rule['type']]
                    if not isinstance(value, expected) or isinstance(value, bool) or ('enum' in rule and value not in rule['enum']) or ('minimum' in rule and value < rule['minimum']):
                        raise ProtocolError(-32602, 'Invalid argument: ' + key)
                envelope = {'op': 'hello'} if name == 'nect_session' else dict(arguments)
                if name == 'nect_command':
                    envelope['op'] = 'core'
                try:
                    reply = call(endpoint, envelope)
                except (OSError, ValueError, TimeoutError) as error:
                    reply = {'ok': False, 'error': {'code': 'TRANSPORT_ERROR', 'message': str(error)}}
                result = {'content': [{'type': 'text', 'text': json.dumps(reply, ensure_ascii=False)}],
                          'structuredContent': reply, 'isError': not reply.get('ok', False)}
            else:
                raise ProtocolError(-32601, 'Method not found')
            response = {'jsonrpc': '2.0', 'id': request_id, 'result': result}
        except ProtocolError as error:
            if notification:
                continue
            response = {'jsonrpc': '2.0', 'id': request_id, 'error': {'code': error.code, 'message': error.message}}
        sys.stdout.buffer.write(json.dumps(response, ensure_ascii=False, allow_nan=False).encode('utf-8') + b'\n')
        sys.stdout.buffer.flush()
        if len(line) > LIMIT:
            return


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint', required=True)
    run(parser.parse_args().endpoint)

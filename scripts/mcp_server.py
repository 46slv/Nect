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
    {'name': 'nect_session', 'description': 'Read the live desktop document/session identity and revision.',
     'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
     'annotations': {'readOnlyHint': True, 'openWorldHint': False}},
    {'name': 'nect_command',
     'description': ('Inspect/evaluate/export_svg or edit the desktop-owned Session. request.op: '
                     'inspect, properties, get, resolve_name, evaluate, export_svg, artboards, operator_types, gradient_types, render_plan, conversion_plan, apply, undo, redo, history, restore_history. '
                     'history lists stable state IDs and bounded retained memory estimates for this live Session only. '
                     'restore_history takes state_id and expected_revision, atomically returns to a retained state in one revision, '
                     'and keeps future states available until a new edit replaces the redo branch. '
                     'apply requires expected_revision and commands. Commands include create_path, '
                     'add_point, remove_point, close_contour, set, link, unlink, rename, '
                     'reorder_points, reorder_objects, group_contiguous, delete_objects, create_primitive, '
                     'enable_point_edit, convert_to_path, add_operation, remove_operation, reorder_operations, '
                     'enable_operation, operation_options and set_gradient. operator_types and gradient_types return exact templates. '
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
                     'Use properties to discover stable refs and units. All mutations are atomic and undoable.'),
     'inputSchema': {'type': 'object', 'properties': dict(IDENTITY, request={'type': 'object'}),
                     'required': ['session_id', 'document_id', 'request'], 'additionalProperties': False},
     'annotations': {'readOnlyHint': False, 'destructiveHint': True, 'openWorldHint': False}},
    {'name': 'nect_file',
     'description': ('Create/open/save a native document or protect recovery in the live desktop. '
                     'new/open rotate session identity; read returned identity before subsequent edits. '
                     'open protects outgoing work in recovery. save preserves previous file backups.'),
     'inputSchema': {'type': 'object', 'properties': dict(IDENTITY,
         op={'type': 'string', 'enum': ['new', 'open', 'save', 'recover']},
         expected_revision={'type': 'integer', 'minimum': 0}, path={'type': 'string'}),
         'required': ['session_id', 'document_id', 'op', 'expected_revision'], 'additionalProperties': False},
     'annotations': {'readOnlyHint': False, 'destructiveHint': True, 'openWorldHint': False}},
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
                raise ProtocolError(-32700, 'Message exceeds 8 MiB')
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
                    expected = {'string': str, 'object': dict, 'integer': int}[rule['type']]
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

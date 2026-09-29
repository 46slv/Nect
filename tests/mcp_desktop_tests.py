"""Seeded formal MCP client -> sidecar -> live Qt Session, including restart.

Uses a temporary document and offscreen desktop; this is semantic/persistence
evidence, not a viewport performance or visual quality claim.
"""
import json
import hashlib
import struct
import zlib
import base64
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import time
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from session_client import call as desktop_api_call
EXE = str(Path(sys.argv[1]).resolve())
desktop = None
mcp = None


def start(endpoint, temp, native=None):
    ready = temp / ('ready-' + uuid.uuid4().hex + '.json')
    args = [EXE, '--automation-endpoint', endpoint, '--recovery-dir', str(temp / 'recovery'), '--ready-file', str(ready)]
    if native:
        args.append(str(native))
    process = subprocess.Popen(args, env=dict(os.environ, QT_QPA_PLATFORM='offscreen'))
    deadline = time.monotonic() + 10
    while not ready.exists():
        if process.poll() is not None or time.monotonic() >= deadline:
            process.kill()
            raise AssertionError('Desktop did not start')
        time.sleep(.02)
    return process, json.loads(ready.read_text(encoding='utf-8'))


seq = 0
def rpc(method, params=None):
    global seq
    seq += 1
    message = dict(jsonrpc='2.0', id=seq, method=method)
    if params is not None:
        message['params'] = params
    mcp.stdin.write(json.dumps(message) + '\n')
    mcp.stdin.flush()
    reply = json.loads(mcp.stdout.readline())
    assert reply['id'] == seq and reply['jsonrpc'] == '2.0', reply
    return reply


def tool(name, arguments=None):
    reply = rpc('tools/call', dict(name=name, arguments=arguments or {}))
    assert 'result' in reply, reply
    result = reply['result']
    assert json.loads(result['content'][0]['text']) == result['structuredContent']
    assert result['isError'] == (not result['structuredContent']['ok'])
    return result['structuredContent']


identity = {}
def core(op, **kwargs):
    return tool('nect_command', dict(identity, request=dict(op=op, **kwargs)))


def apply(commands, revision):
    result = core('apply', expected_revision=revision, commands=commands)
    assert result['ok'], result
    assert result['revision'] == revision + 1
    assert result['result']['changed_ids']
    return result['revision']


def point(id_, x, y):
    return dict(id=id_, x={'literal': x}, y={'literal': y}, in_angle={'literal': 180},
                in_length={'literal': 20}, out_angle={'literal': 0}, out_length={'literal': 20})


def make_png(rgb, width=2, height=2):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    rows = (b'\0' + bytes(rgb) * width) * height
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')


def make_rgba_png(rows):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    height = len(rows)
    width = len(rows[0])
    raw = b''.join(b'\0' + b''.join(bytes(pixel) for pixel in row) for row in rows)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')


try:
    with tempfile.TemporaryDirectory(prefix='nect-mcp-') as directory:
        temp = Path(directory)
        endpoint = 'nect-test-' + uuid.uuid4().hex
        desktop, _ = start(endpoint, temp)
        mcp = subprocess.Popen([sys.executable, str(ROOT / 'scripts/mcp_server.py'), '--endpoint', endpoint],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, encoding='utf-8')
        assert rpc('tools/list')['error']['code'] == -32002
        init = rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={}, clientInfo=dict(name='nect-scenario', version='1')))
        assert init['result']['protocolVersion'] == '2025-06-18'
        mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', method='notifications/initialized')) + '\n'); mcp.stdin.flush()
        listed_tools = rpc('tools/list')['result']['tools']
        assert {t['name'] for t in listed_tools} == {'nect_session', 'nect_command', 'nect_file', 'nect_image', 'nect_export_png', 'nect_analyze_regions', 'nect_import_svg'}
        analyze_schema = next(t for t in listed_tools if t['name'] == 'nect_analyze_regions')['inputSchema']
        assert analyze_schema['properties']['include_color_groups'] == {'type': 'boolean'}
        assert analyze_schema['properties']['include_color_components'] == {'type': 'boolean'}
        assert analyze_schema['properties']['intersect_color_component_index'] == {'type': 'integer', 'minimum': 0}
        assert 'include_color_groups' not in analyze_schema['required']
        assert 'include_color_components' not in analyze_schema['required']
        assert 'intersect_color_component_index' not in analyze_schema['required']
        live = tool('nect_session')
        identity = {key: live[key] for key in ('session_id', 'document_id')}
        comp = core('inspect')['result']['compositions'][0]
        region_request = dict(identity, op='analyze_regions', expected_revision=live['revision'],
            composition=comp['id'], artboard=comp['artboards'][0]['id'], scale=1, threshold=128)
        direct_regions = desktop_api_call(endpoint, region_request)
        mcp_regions = tool('nect_analyze_regions', region_request)
        assert direct_regions == mcp_regions and direct_regions['ok']
        assert direct_regions['revision'] == live['revision']
        assert direct_regions['result']['source_revision'] == live['revision']
        assert direct_regions['result']['regions'] == []
        assert direct_regions['result']['outer_contours'] == []
        assert direct_regions['result']['contour_rule'] == 'foreground-right-clockwise-outer'
        assert direct_regions['result']['contour_coordinate_space'] == 'artboard-output-pixel-corners'
        assert direct_regions['result']['contour_closed'] == 'implicit-last-to-first'
        assert direct_regions['result']['morphology'] == dict(operation='dilate', kernel='cross-4-radius-1',
            border='outside-background-clipped', coordinate_space='artboard-output-pixels', area=0, runs=[])
        assert direct_regions['result']['erosion'] == dict(operation='erode', kernel='cross-4-radius-1',
            border='outside-background', coordinate_space='artboard-output-pixels', area=0, runs=[])
        initial_document = core('inspect')['result']
        color_fixture_path = temp / 'mcp-color-groups.png'
        color_fixture_path.write_bytes(make_png((12, 34, 56), width=3, height=2))
        color_fixture = tool('nect_image', dict(identity, op='import_image', expected_revision=live['revision'],
            path=str(color_fixture_path), mode='embedded', composition=comp['id'], parent='',
            asset='mcp-color-groups-asset', id='mcp-color-groups-image', name='Color group fixture', x=0, y=0))
        assert color_fixture['ok'], color_fixture
        color_groups_request = dict(identity, op='analyze_regions', expected_revision=color_fixture['revision'],
            composition=comp['id'], artboard=comp['artboards'][0]['id'], scale=1, threshold=128,
            include_color_groups=True)
        direct_color_groups = desktop_api_call(endpoint, color_groups_request)
        mcp_color_groups = tool('nect_analyze_regions', color_groups_request)
        assert direct_color_groups == mcp_color_groups and direct_color_groups['ok']
        assert direct_color_groups['result']['color_groups'] == [dict(rgb=[12, 34, 56], area=6,
            bounds=dict(x=0, y=0, width=3, height=2),
            runs=[dict(y=0, x=0, width=3), dict(y=1, x=0, width=3)])]
        color_document_before_analysis = core('inspect')['result']
        color_history_before_analysis = core('history')['result']
        component_request = dict(color_groups_request, include_color_components=True)
        direct_color_components = desktop_api_call(endpoint, component_request)
        mcp_color_components = tool('nect_analyze_regions', component_request)
        assert direct_color_components == mcp_color_components and direct_color_components['ok']
        assert direct_color_components['result']['color_components'] == [dict(component_index=0, rgb=[12, 34, 56],
            area=6, bounds=dict(x=0, y=0, width=3, height=2),
            runs=[dict(y=0, x=0, width=3), dict(y=1, x=0, width=3)])]
        assert 'color_component_mask_intersection' not in direct_color_components['result']
        components_false_request = dict(color_groups_request, include_color_components=False)
        assert desktop_api_call(endpoint, components_false_request) == direct_color_groups == \
            tool('nect_analyze_regions', components_false_request)
        invalid_component_requests = [
            dict(identity, op='analyze_regions', expected_revision=color_fixture['revision'],
                composition=comp['id'], artboard=comp['artboards'][0]['id'], scale=1, threshold=128,
                include_color_components=True),
            dict(color_groups_request, include_color_groups=False, include_color_components=True),
        ]
        for invalid_component_request in invalid_component_requests:
            direct_invalid_components = desktop_api_call(endpoint, invalid_component_request)
            mcp_invalid_components = tool('nect_analyze_regions', invalid_component_request)
            assert direct_invalid_components == mcp_invalid_components
            assert not direct_invalid_components['ok'] and direct_invalid_components['error']['code'] == 'INVALID_REQUEST'
        invalid_component_type = dict(color_groups_request, include_color_components='true')
        direct_invalid_type = desktop_api_call(endpoint, invalid_component_type)
        mcp_invalid_type = rpc('tools/call', dict(name='nect_analyze_regions', arguments=invalid_component_type))
        assert not direct_invalid_type['ok'] and direct_invalid_type['error']['code'] == 'INVALID_REQUEST'
        assert mcp_invalid_type['error']['code'] == -32602 and \
            mcp_invalid_type['error']['message'] == 'Invalid argument: include_color_components'
        assert core('inspect')['result'] == color_document_before_analysis
        assert core('history')['result'] == color_history_before_analysis
        assert tool('nect_session')['revision'] == color_fixture['revision']

        mask_rows = [[(255, 0, 0, 255) for _ in range(5)] for _ in range(5)]
        mask_rows[2][2] = (0, 0, 255, 255)
        mask_fixture_path = temp / 'mcp-component-mask-intersection.png'
        mask_fixture_path.write_bytes(make_rgba_png(mask_rows))
        mask_fixture = tool('nect_image', dict(identity, op='import_image', expected_revision=color_fixture['revision'],
            path=str(mask_fixture_path), mode='embedded', composition=comp['id'], parent='',
            asset='mcp-component-mask-asset', id='mcp-component-mask-image',
            name='Red with blue center', x=0, y=0))
        assert mask_fixture['ok'], mask_fixture
        mask_document_before_analysis = core('inspect')['result']
        mask_history_before_analysis = core('history')['result']
        mask_request = dict(identity, op='analyze_regions', expected_revision=mask_fixture['revision'],
            composition=comp['id'], artboard=comp['artboards'][0]['id'], scale=1, threshold=128,
            include_color_groups=True, include_color_components=True, intersect_color_component_index=1)
        direct_mask_intersection = desktop_api_call(endpoint, mask_request)
        mcp_mask_intersection = tool('nect_analyze_regions', mask_request)
        assert direct_mask_intersection == mcp_mask_intersection and direct_mask_intersection['ok']
        mask_result = direct_mask_intersection['result']
        ring_runs = [dict(y=0, x=0, width=5), dict(y=1, x=0, width=1), dict(y=1, x=4, width=1),
                     dict(y=2, x=0, width=1), dict(y=2, x=4, width=1), dict(y=3, x=0, width=1),
                     dict(y=3, x=4, width=1), dict(y=4, x=0, width=5)]
        assert mask_result['color_components'] == [
            dict(component_index=0, rgb=[0, 0, 255], area=1, bounds=dict(x=2, y=2, width=1, height=1),
                 runs=[dict(y=2, x=2, width=1)]),
            dict(component_index=1, rgb=[255, 0, 0], area=24, bounds=dict(x=0, y=0, width=5, height=5),
                 runs=[dict(y=0, x=0, width=5), dict(y=1, x=0, width=5), dict(y=2, x=0, width=2),
                       dict(y=2, x=3, width=2), dict(y=3, x=0, width=5), dict(y=4, x=0, width=5)])]
        assert mask_result['color_component_mask_intersection'] == dict(
            operation='intersection', component_index=1, rgb=[255, 0, 0], other_operand='mask_boolean',
            coordinate_space='artboard-output-pixels', width=mask_result['width'], height=mask_result['height'],
            source_revision=mask_fixture['revision'], area=16, runs=ring_runs)
        blue_mask_request = dict(mask_request, intersect_color_component_index=0)
        direct_blue_intersection = desktop_api_call(endpoint, blue_mask_request)
        assert direct_blue_intersection == tool('nect_analyze_regions', blue_mask_request)
        assert direct_blue_intersection['result']['color_component_mask_intersection'] == dict(
            operation='intersection', component_index=0, rgb=[0, 0, 255], other_operand='mask_boolean',
            coordinate_space='artboard-output-pixels', width=mask_result['width'], height=mask_result['height'],
            source_revision=mask_fixture['revision'], area=0, runs=[])

        invalid_intersection_requests = [
            dict(identity, op='analyze_regions', expected_revision=mask_fixture['revision'],
                composition=comp['id'], artboard=comp['artboards'][0]['id'], scale=1, threshold=128,
                intersect_color_component_index=0),
            dict(mask_request, include_color_components=False),
            dict(mask_request, include_color_groups=False),
            dict(mask_request, intersect_color_component_index=2),
        ]
        for invalid_intersection_request in invalid_intersection_requests:
            direct_invalid_intersection = desktop_api_call(endpoint, invalid_intersection_request)
            mcp_invalid_intersection = tool('nect_analyze_regions', invalid_intersection_request)
            assert direct_invalid_intersection == mcp_invalid_intersection
            assert not direct_invalid_intersection['ok'] and \
                direct_invalid_intersection['error']['code'] == 'INVALID_REQUEST' and \
                'result' not in direct_invalid_intersection
        for invalid_index in (True, 1.5, -1):
            invalid_index_request = dict(mask_request, intersect_color_component_index=invalid_index)
            direct_invalid_index = desktop_api_call(endpoint, invalid_index_request)
            mcp_invalid_index = rpc('tools/call', dict(name='nect_analyze_regions', arguments=invalid_index_request))
            assert not direct_invalid_index['ok'] and direct_invalid_index['error']['code'] == 'INVALID_REQUEST'
            assert mcp_invalid_index['error']['code'] == -32602 and \
                mcp_invalid_index['error']['message'] == 'Invalid argument: intersect_color_component_index'
        assert core('inspect')['result'] == mask_document_before_analysis
        assert core('history')['result'] == mask_history_before_analysis
        assert tool('nect_session')['revision'] == mask_fixture['revision']
        mask_undo = core('undo', expected_revision=mask_fixture['revision'])
        assert mask_undo['ok'] and core('inspect')['result'] == color_document_before_analysis

        checker_rows = [[(255, 0, 0, 255) if (x + y) % 2 == 0 else (0, 0, 255, 255)
                         for x in range(101)] for y in range(100)]
        checker_path = temp / 'mcp-color-component-limit.png'
        checker_path.write_bytes(make_rgba_png(checker_rows))
        checker_fixture = tool('nect_image', dict(identity, op='import_image', expected_revision=mask_undo['revision'],
            path=str(checker_path), mode='embedded', composition=comp['id'], parent='',
            asset='mcp-color-component-limit-asset', id='mcp-color-component-limit-image',
            name='Color component cap fixture', x=0, y=0))
        assert checker_fixture['ok'], checker_fixture
        checker_document_before_analysis = core('inspect')['result']
        checker_history_before_analysis = core('history')['result']
        checker_component_request = dict(identity, op='analyze_regions', expected_revision=checker_fixture['revision'],
            composition=comp['id'], artboard=comp['artboards'][0]['id'], scale=1, threshold=1,
            include_color_groups=True, include_color_components=True, intersect_color_component_index=10000)
        direct_component_limit = desktop_api_call(endpoint, checker_component_request)
        mcp_component_limit = tool('nect_analyze_regions', checker_component_request)
        assert direct_component_limit == mcp_component_limit
        assert not direct_component_limit['ok'] and direct_component_limit['error']['code'] == 'ANALYSIS_LIMIT'
        assert '10,000 components' in direct_component_limit['error']['message']
        assert 'result' not in direct_component_limit
        assert core('inspect')['result'] == checker_document_before_analysis
        assert core('history')['result'] == checker_history_before_analysis
        assert tool('nect_session')['revision'] == checker_fixture['revision']
        checker_undo = core('undo', expected_revision=checker_fixture['revision'])
        assert checker_undo['ok'] and core('inspect')['result'] == color_document_before_analysis
        color_undo = core('undo', expected_revision=checker_undo['revision'])
        assert color_undo['ok'] and core('inspect')['result'] == initial_document
        rng = random.Random(7821)
        commands = []
        for i in range(24):
            commands.append(dict(type='create_path', composition=comp['id'], parent='', id=f'path-{i}', name=f'Motif {i}',
                contours=[dict(id=f'contour-{i}', closed=False, points=[point(f'p-{i}-0', rng.randrange(40, 850), rng.randrange(40, 570)),
                                                                     point(f'p-{i}-1', rng.randrange(40, 850), rng.randrange(40, 570))])]))
        rev = apply(commands, color_undo['revision'])
        guide_source_ref=dict(object='mcp-guide-source',point='',field='guide.position')
        guide_target_ref=dict(object='mcp-guide-target',point='',field='guide.position')
        rev=apply([dict(type='add_guide',composition=comp['id'],guide=dict(id=guide_source_ref['object'],name='MCP Guide source',axis='x',position=100)),
            dict(type='add_guide',composition=comp['id'],guide=dict(id=guide_target_ref['object'],name='MCP Guide target',axis='x',position=240))],rev)
        rev=apply([dict(type='link_guide_position',target=guide_target_ref,source=guide_source_ref,replace_driver=False)],rev)
        guide_value=core('get',ref=guide_target_ref)['result']
        assert guide_value['authored']==dict(literal=240,driver=guide_source_ref,source_kind='link',expression=None) and guide_value['evaluated']==100
        guide_property=next(value for value in core('properties')['result'] if value['ref']==guide_target_ref)
        assert guide_property['type']=='number' and guide_property['space']=='composition' and guide_property['link'] is True
        assert core('resolve_name',name='MCP Guide target',point='',field='guide.position')['result']==guide_target_ref
        rev=apply([dict(type='update_guide',composition=comp['id'],
            guide=dict(id=guide_source_ref['object'],name='MCP renamed source',axis='x',position=120))],rev)
        assert core('get',ref=guide_target_ref)['result']['evaluated']==120
        guide_expression=dict(source='ref("mcp-guide-source","","guide.position") + 20',version=1)
        rev=apply([dict(type='set_guide_position_expression',target=guide_target_ref,
            expression=guide_expression,replace_driver=True)],rev)
        guide_expression_value=core('get',ref=guide_target_ref)['result']
        assert guide_expression_value['authored']==dict(literal=240,driver=None,source_kind='expression',expression=guide_expression)
        assert guide_expression_value['evaluated']==140 and guide_expression_value['expression'] is True
        guide_expression_metadata=next(value for value in core('properties')['result'] if value['ref']==guide_target_ref)
        assert guide_expression_metadata==guide_expression_value
        guide_before_failure=core('inspect')['result']
        guide_failure=core('apply',expected_revision=rev,commands=[dict(type='set',ref=guide_target_ref,value=9)])
        assert not guide_failure['ok'] and guide_failure['error']['code']=='TYPE_MISMATCH' and guide_failure['revision']==rev
        assert core('inspect')['result']==guide_before_failure
        source = dict(object='path-0', point='p-0-0', field='x')
        target = dict(object='path-1', point='p-1-0', field='x')
        rev = apply([dict(type='link', target=target, binding=dict(source=source, scale=1, offset=12, mode='copy_local_value'))], rev)
        for i in range(36):
            value = 120 + i
            rev = apply([dict(type='set', ref=source, value=value)], rev)
            assert core('get', ref=target)['result']['evaluated'] == value + 12
        before = core('inspect')['result']
        failed = core('apply', expected_revision=rev, commands=[dict(type='set', ref=source, value=999),
            dict(type='set', ref=target, value=2)])
        assert not failed['ok'] and failed['error']['code'] == 'DRIVEN_PROPERTY'
        assert core('inspect')['result'] == before and failed['revision'] == rev
        stale = core('apply', expected_revision=rev-1, commands=[dict(type='set', ref=source, value=2)])
        assert stale['error']['code'] == 'REVISION_CONFLICT'
        rev = apply([dict(type='rename', object='path-0', name='Renamed source'),
            dict(type='reorder_points', object='path-0', contour='contour-0', order=['p-0-1', 'p-0-0']),
            dict(type='reorder_objects', composition=comp['id'], parent='', order=[f'path-{i}' for i in reversed(range(24))])], rev)
        assert core('get', ref=target)['result']['evaluated'] == 167
        assert core('resolve_name', name='Renamed source', point='p-0-0', field='x')['result'] == source
        assert core('undo', expected_revision=rev)['ok']; rev += 1
        assert core('redo', expected_revision=rev)['ok']; rev += 1
        definitions = core('operator_types')['result']
        duplicate_before = core('inspect')['result']
        duplicated = core('apply', expected_revision=rev, commands=[dict(type='duplicate_objects', objects=['path-0','path-1'], prefix='mcp-copy')])
        assert duplicated['ok'], duplicated
        rev = duplicated['revision']
        assert duplicated['result']['created_ids'] == ['mcp-copy-1','mcp-copy-2']
        copied_objects = {o['id']: o for o in core('inspect')['result']['objects']}
        copied_target = next(p for p in copied_objects['mcp-copy-2']['contours'][0]['points'] if p['x'].get('binding'))
        copied_source = copied_target['x']['binding']['source']
        assert copied_source['object'] == 'mcp-copy-1'
        assert copied_source['point'] != 'p-0-0'
        assert core('undo', expected_revision=rev)['ok']; rev += 1
        assert core('inspect')['result'] == duplicate_before
        fill = next(x['template'] for x in definitions if x['type']=='nect.paint.fill')
        repeat = next(x['template'] for x in definitions if x['type']=='nect.shape.repeater')
        fill['id']='motif-fill';fill['parameters']['r']['literal']=.8
        repeat['id']='motif-repeat'
        rev = apply([dict(type='add_operation',object='path-0',index=0,operation=fill),
            dict(type='add_operation',object='path-0',index=2,operation=repeat)],rev)
        copies=3
        for _ in range(16):
            copies=rng.choice([n for n in range(2,8) if n!=copies])
            response=core('apply',expected_revision=rev,commands=[dict(type='set',
                ref=dict(object='path-0',point='',field='op.motif-repeat.copies'),value=copies)])
            assert response['ok'] and 'path-0' in response['result']['changed_ids']
            rev=response['revision']
            plan=core('render_plan',object='path-0')['result']
            assert plan['path_instances']==copies and len(plan['paint_layers'])==2*copies
        expected_svg_paths=23+2*copies
        gradients=core('gradient_types')['result']
        gradient=next(x['template'] for x in gradients if x['type']=='linear')
        gradient['id']='motif-gradient'
        rev=apply([dict(type='set_gradient',object='path-0',operation='motif-fill',gradient=gradient)],rev)
        gradient_enabled_ref=dict(object='path-0',point='',field='op.motif-fill.gradient.motif-gradient.enabled')
        operation_enabled_ref=dict(object='path-0',point='',field='op.motif-fill.enabled')
        gradient_enabled=core('get',ref=gradient_enabled_ref)['result']
        assert gradient_enabled['type']=='bool' and gradient_enabled['unit']=='boolean' and \
            gradient_enabled['space']=='local' and gradient_enabled['origin']=='authored' and \
            gradient_enabled['authored']==dict(literal=True,driver=None) and gradient_enabled['evaluated'] is True and \
            gradient_enabled['link'] is True and gradient_enabled['expression'] is False, gradient_enabled
        assert core('resolve_name',name=next(obj['name'] for obj in core('inspect')['result']['objects']
            if obj['id']=='path-0'),point='',field=gradient_enabled_ref['field'])['result']==gradient_enabled_ref
        assert next(item for item in core('properties')['result'] if item['ref']==gradient_enabled_ref)==gradient_enabled
        assert core('get',ref=operation_enabled_ref)['result']['evaluated'] is True
        assert gradient_enabled_ref['field']!=operation_enabled_ref['field'] and gradient_enabled==desktop_api_call(
            endpoint,dict(identity,op='core',request=dict(op='get',ref=gradient_enabled_ref)))['result']
        bad_gradient_point=core('get',ref=dict(gradient_enabled_ref,point='unexpected-point'))
        bad_gradient_id=core('get',ref=dict(gradient_enabled_ref,field='op.motif-fill.gradient.missing-gradient.enabled'))
        bad_gradient_operation=core('get',ref=dict(gradient_enabled_ref,field='op.missing-operation.gradient.motif-gradient.enabled'))
        assert bad_gradient_point['error']['code']=='INVALID_GRADIENT_REF' and \
            bad_gradient_id['error']['code']=='MISSING_GRADIENT' and \
            bad_gradient_operation['error']['code']=='MISSING_OPERATION'
        scalar_before=core('get',ref=dict(object='path-0',point='',field='op.motif-fill.gradient.motif-gradient.start_x'))['result']['evaluated']
        atomic=core('apply',expected_revision=rev,commands=[
            dict(type='set',ref=dict(object='path-0',point='',field='op.motif-fill.gradient.motif-gradient.start_x'),value=scalar_before+1),
            dict(type='set',ref=gradient_enabled_ref,value=0)])
        assert not atomic['ok'] and atomic['error']['code']=='MISSING_REFERENCE' and atomic['revision']==rev
        assert core('get',ref=dict(object='path-0',point='',field='op.motif-fill.gradient.motif-gradient.start_x'))['result']['evaluated']==scalar_before
        bypassed_gradient=dict(gradient,enabled=False)
        rev=apply([dict(type='set_gradient',object='path-0',operation='motif-fill',gradient=bypassed_gradient)],rev)
        disabled=core('get',ref=gradient_enabled_ref)['result']
        assert disabled['authored']==dict(literal=False,driver=None) and disabled['evaluated'] is False and \
            core('get',ref=operation_enabled_ref)['result']['evaluated'] is True and \
            any(item['ref']==gradient_enabled_ref for item in core('properties')['result'])
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('get',ref=gradient_enabled_ref)['result']==gradient_enabled
        generic_set=core('apply',expected_revision=rev,commands=[dict(type='set',ref=gradient_enabled_ref,value=0)])
        assert not generic_set['ok'] and generic_set['error']['code']=='MISSING_REFERENCE' and generic_set['revision']==rev
        target_gradient=dict(gradient,id='mcp-target-gradient',enabled=False)
        target_gradient['stops']=[dict(stop,id='mcp-target-'+stop['id']) for stop in gradient['stops']]
        target_gradient_ref=dict(object='path-1',point='',field='op.path-1-stroke.gradient.mcp-target-gradient.enabled')
        target_stop_ref=dict(object='path-1',point='',field='op.path-1-stroke.gradient.mcp-target-gradient.stop.mcp-target-'+gradient['stops'][0]['id']+'.color')
        rev=apply([dict(type='set_gradient',object='path-1',operation='path-1-stroke',gradient=target_gradient)],rev)
        linked=core('apply',expected_revision=rev,commands=[dict(type='link_gradient_enabled',target=target_gradient_ref,
            source=gradient_enabled_ref,replace_driver=False)])
        assert linked['ok'] and linked['revision']==rev+1;rev=linked['revision']
        linked_value=core('get',ref=target_gradient_ref)['result']
        direct_value=desktop_api_call(endpoint,dict(identity,op='core',request=dict(op='get',ref=target_gradient_ref)))['result']
        assert linked_value['type']=='bool' and linked_value['authored']==dict(literal=False,driver=dict(link=gradient_enabled_ref)) and \
            linked_value['evaluated'] is True and linked_value==direct_value and \
            next(item for item in core('properties')['result'] if item['ref']==target_gradient_ref)==linked_value
        plan=core('render_plan',object='path-1')['result']
        assert next(layer for layer in plan['paint_layers'] if layer['operation']=='path-1-stroke').get('gradient') is not None
        active_color_inventory=core('used_colors')['result']
        active_color_refs=[ref for color in active_color_inventory['colors'] for ref in color['uses']]
        assert target_stop_ref in active_color_refs, (target_stop_ref,active_color_refs)
        assert '<linearGradient' in core('export_svg',composition=comp['id'],artboard=comp['artboards'][0]['id'])['result']
        stale=core('apply',expected_revision=rev-1,commands=[dict(type='link_gradient_enabled',target=target_gradient_ref,
            source=gradient_enabled_ref,replace_driver=True)])
        assert not stale['ok'] and stale['error']['code']=='REVISION_CONFLICT' and stale['revision']==rev
        direct_toggle=core('apply',expected_revision=rev,commands=[dict(type='set_gradient',object='path-1',operation='path-1-stroke',
            gradient=dict(target_gradient,enabled=True))])
        assert not direct_toggle['ok'] and direct_toggle['error']['code']=='DRIVEN_PROPERTY' and direct_toggle['revision']==rev
        target_replacement=dict(target_gradient,end_x=dict(literal=350))
        rev=apply([dict(type='set_gradient',object='path-1',operation='path-1-stroke',gradient=target_replacement)],rev)
        preserved=core('get',ref=target_gradient_ref)['result']
        assert preserved['authored']==dict(literal=False,driver=dict(link=gradient_enabled_ref)) and preserved['evaluated'] is True
        source_off=dict(gradient,enabled=False)
        direct_toggle=desktop_api_call(endpoint,dict(identity,op='core',request=dict(op='apply',expected_revision=rev,
            commands=[dict(type='set_gradient',object='path-0',operation='motif-fill',gradient=source_off)])))
        assert direct_toggle['ok'];rev+=1
        assert core('get',ref=target_gradient_ref)['result']['evaluated'] is False
        assert not any(target_stop_ref in color['uses'] for color in core('used_colors')['result']['colors'])
        rev=apply([dict(type='unlink_gradient_enabled',target=target_gradient_ref)],rev)
        frozen=core('get',ref=target_gradient_ref)['result']
        source_on=dict(gradient,enabled=True)
        api_source=desktop_api_call(endpoint,dict(identity,op='core',request=dict(op='apply',expected_revision=rev,
            commands=[dict(type='set_gradient',object='path-0',operation='motif-fill',gradient=source_on)])))
        assert api_source['ok'];rev+=1
        frozen_after=core('get',ref=target_gradient_ref)['result']
        assert frozen['authored']==dict(literal=False,driver=None) and frozen['evaluated'] is False and frozen_after==frozen
        mcp_native=core('inspect')['result']
        assert mcp_native['version']=='0.54' and core('get',ref=target_gradient_ref)['result']==frozen, (mcp_native.get('version'),core('get',ref=target_gradient_ref)['result'],frozen)
        stop_ref=dict(object='path-0',point='',field='op.motif-fill.gradient.motif-gradient.stop.start-stop.r')
        rev=apply([dict(type='set',ref=stop_ref,value=.75),dict(type='link',
            target=dict(object='path-1',point='',field='stroke.r'),
            binding=dict(source=stop_ref,scale=1,offset=0,mode='copy_local_value'))],rev)
        assert core('get',ref=dict(object='path-1',point='',field='stroke.r'))['result']['evaluated']==.75
        plan=core('render_plan',object='path-0')['result']
        assert sum('gradient' in x for x in plan['paint_layers'])==copies
        first=comp['artboards'][0]
        child=dict(id='alternate-frame',name='Alternate crop',x=100,y=50,width=160,height=240,
            parent_size=dict(artboard=first['id'],width=True,height=False))
        rev=apply([dict(type='add_artboard',composition=comp['id'],artboard=child,index=1)],rev)
        first=dict(first,width=700)
        changed=core('apply',expected_revision=rev,commands=[dict(type='update_artboard',composition=comp['id'],artboard=first)])
        assert changed['ok'] and first['id'] in changed['result']['changed_ids'] and child['id'] in changed['result']['changed_ids']
        rev=changed['revision']
        rev=apply([dict(type='reorder_artboards',composition=comp['id'],order=[child['id'],first['id']])],rev)
        boards=core('artboards',composition=comp['id'])['result']
        assert boards[0]['authored']['id']==child['id'] and boards[0]['authored']['width']==160
        assert boards[0]['evaluated']['width']==700 and boards[0]['evaluated']['height']==240
        width_ref=dict(object=child['id'],point='',field='artboard.width')
        width=core('get',ref=width_ref)['result']
        assert width['type']=='number' and width['unit']=='du' and width['authored']==dict(
            literal=160,driver=dict(object=first['id'],point='',field='artboard.width'),
            source_kind='parent_size',expression=None)
        assert width['evaluated']==700 and width['link'] is True and width['expression'] is True
        assert core('resolve_name',name='Alternate crop',point='',field='artboard.width')['result']==width_ref
        assert any(entry['ref']==width_ref and entry['evaluated']==700 for entry in core('properties')['result'])
        formula=f'ref("{first["id"]}","","artboard.height") * 2'
        rev=apply([dict(type='set_artboard_size_expression',target=width_ref,
            expression=dict(source=formula,version=1),replace_driver=True)],rev)
        expressed=core('get',ref=width_ref)['result']
        assert expressed['authored']==dict(literal=160,driver=None,source_kind='expression',
            expression=dict(source=formula,version=1)) and expressed['evaluated']==first['height']*2
        undone=core('undo',expected_revision=rev)
        assert undone['ok'];rev=undone['revision']
        assert core('get',ref=width_ref)['result']==width
        invalid=core('get',ref=dict(width_ref,point='not-empty'))
        assert not invalid['ok'] and invalid['error']['code']=='INVALID_ARTBOARD_REF'
        crop_svg=core('export_svg',composition=comp['id'],artboard=child['id'])['result']
        assert ET.fromstring(crop_svg).attrib['viewBox']=='100 50 700 240'
        first_size=first
        source_board=dict(id='mcp-margin-source',name='Margin source',x=0,y=0,width=40,height=100,
            parent_size=dict(artboard='mcp-margin-upstream',width=True,height=False))
        upstream_board=dict(id='mcp-margin-upstream',name='Margin upstream',x=0,y=0,width=40,height=100)
        gutter_source_board=dict(id='mcp-grid-column-gutter-source',name='Grid gutter source',x=0,y=0,width=10,height=20)
        target_margin_ref=dict(object=first['id'],point='',field='margin.left')
        target_margin_top_ref=dict(object=first['id'],point='',field='margin.top')
        target_margin_right_ref=dict(object=first['id'],point='',field='margin.right')
        target_margin_bottom_ref=dict(object=first['id'],point='',field='margin.bottom')
        target_grid_x_ref=dict(object='mcp-margin-grid',point='',field='grid.bounds.x')
        target_grid_y_ref=dict(object='mcp-margin-grid',point='',field='grid.bounds.y')
        target_grid_width_ref=dict(object='mcp-margin-grid',point='',field='grid.bounds.width')
        target_grid_height_ref=dict(object='mcp-margin-grid',point='',field='grid.bounds.height')
        target_grid_column_gutter_ref=dict(object='mcp-margin-grid',point='',field='grid.column_gutter')
        target_grid_row_gutter_ref=dict(object='mcp-margin-grid',point='',field='grid.row_gutter')
        margin_source_ref=dict(object='mcp-margin-source',point='',field='artboard.width')
        margin_top_source_ref=dict(object='mcp-margin-upstream',point='',field='artboard.height')
        grid_column_gutter_source_ref=dict(object='mcp-grid-column-gutter-source',point='',field='artboard.width')
        grid_row_gutter_source_ref=dict(object='mcp-grid-column-gutter-source',point='',field='artboard.height')
        margin_layout=dict(margin=dict(left=40,top=20,right=40,bottom=20),grid=dict(id='mcp-margin-grid',
            bounds=dict(x=40,y=20,width=first_size['width']-80,height=first_size['height']-80),
            columns=2,rows=1,column_gutter=20,row_gutter=0))
        rev=apply([dict(type='add_artboard',composition=comp['id'],artboard=source_board,index=2),
            dict(type='add_artboard',composition=comp['id'],artboard=upstream_board,index=3),
            dict(type='add_artboard',composition=comp['id'],artboard=gutter_source_board,index=4),
            dict(type='set_artboard_layout',composition=comp['id'],artboard_id=first['id'],layout=margin_layout)],rev)
        rev=apply([dict(type='link_margin_left',target=target_margin_ref,source=margin_source_ref,
            replace_driver=False)],rev)
        rev=apply([dict(type='link_margin_top',target=target_margin_top_ref,source=margin_top_source_ref,
            replace_driver=False)],rev)
        rev=apply([dict(type='link_margin_right',target=target_margin_right_ref,source=margin_source_ref,
            replace_driver=False)],rev)
        rev=apply([dict(type='link_margin_bottom',target=target_margin_bottom_ref,source=margin_source_ref,
            replace_driver=False)],rev)
        rev=apply([dict(type='link_grid_bounds_x',target=target_grid_x_ref,source=margin_source_ref,
            replace_driver=False)],rev)
        rev=apply([dict(type='link_grid_bounds_y',target=target_grid_y_ref,source=margin_source_ref,
            replace_driver=False)],rev)
        rev=apply([dict(type='link_grid_bounds_width',target=target_grid_width_ref,source=margin_source_ref,
            replace_driver=False)],rev)
        rev=apply([dict(type='link_grid_bounds_height',target=target_grid_height_ref,source=margin_top_source_ref,
            replace_driver=False)],rev)
        rev=apply([dict(type='link_grid_column_gutter',target=target_grid_column_gutter_ref,
            source=grid_column_gutter_source_ref,replace_driver=False)],rev)
        rev=apply([dict(type='link_grid_row_gutter',target=target_grid_row_gutter_ref,
            source=grid_row_gutter_source_ref,replace_driver=False)],rev)
        margin_linked=core('get',ref=target_margin_ref)['result']
        margin_top_linked=core('get',ref=target_margin_top_ref)['result']
        margin_right_linked=core('get',ref=target_margin_right_ref)['result']
        margin_bottom_linked=core('get',ref=target_margin_bottom_ref)['result']
        grid_x_linked=core('get',ref=target_grid_x_ref)['result']
        grid_y_linked=core('get',ref=target_grid_y_ref)['result']
        grid_width_linked=core('get',ref=target_grid_width_ref)['result']
        grid_height_linked=core('get',ref=target_grid_height_ref)['result']
        grid_column_gutter_linked=core('get',ref=target_grid_column_gutter_ref)['result']
        grid_row_gutter_linked=core('get',ref=target_grid_row_gutter_ref)['result']
        assert margin_linked['authored']==dict(literal=40,driver=margin_source_ref,source_kind='link') and \
            margin_linked['evaluated']==40 and margin_linked['link'] is True
        assert margin_top_linked['authored']==dict(literal=20,driver=margin_top_source_ref,source_kind='link') and \
            margin_top_linked['evaluated']==100 and margin_top_linked['link'] is True and margin_top_linked['expression'] is True
        assert margin_right_linked['authored']==dict(literal=40,driver=margin_source_ref,source_kind='link') and \
            margin_right_linked['evaluated']==40 and margin_right_linked['link'] is True and \
            margin_right_linked['expression'] is True
        assert margin_bottom_linked['authored']==dict(literal=20,driver=margin_source_ref,source_kind='link') and \
            margin_bottom_linked['evaluated']==40 and margin_bottom_linked['link'] is True and \
            margin_bottom_linked['expression'] is True
        assert grid_x_linked['authored']==dict(literal=40,driver=margin_source_ref,source_kind='link') and \
            grid_x_linked['evaluated']==40 and grid_x_linked['link'] is True
        assert grid_y_linked['authored']==dict(literal=20,driver=margin_source_ref,source_kind='link') and \
            grid_y_linked['evaluated']==40 and grid_y_linked['link'] is True
        assert grid_width_linked['authored']==dict(literal=first_size['width']-80,driver=margin_source_ref,
            source_kind='link') and grid_width_linked['evaluated']==40 and grid_width_linked['link'] is True and \
            grid_width_linked['expression'] is True
        assert grid_height_linked['authored']==dict(literal=first_size['height']-80,driver=margin_top_source_ref,
            source_kind='link') and grid_height_linked['evaluated']==100 and grid_height_linked['link'] is True and \
            grid_height_linked['expression'] is True
        assert grid_column_gutter_linked['authored']==dict(literal=20,driver=grid_column_gutter_source_ref,
            source_kind='link') and grid_column_gutter_linked['evaluated']==10 and \
            grid_column_gutter_linked['link'] is True and grid_column_gutter_linked['expression'] is True
        assert grid_row_gutter_linked['authored']==dict(literal=0,driver=grid_row_gutter_source_ref,
            source_kind='link') and grid_row_gutter_linked['evaluated']==20 and \
            grid_row_gutter_linked['link'] is True and grid_row_gutter_linked['expression'] is True
        upstream_changed=dict(upstream_board,width=60,height=80)
        rev=apply([dict(type='update_artboard',composition=comp['id'],artboard=upstream_changed)],rev)
        gutter_source_changed=dict(gutter_source_board,width=12,height=30)
        rev=apply([dict(type='update_artboard',composition=comp['id'],artboard=gutter_source_changed)],rev)
        margin_updated=core('get',ref=target_margin_ref)['result']
        margin_top_updated=core('get',ref=target_margin_top_ref)['result']
        margin_right_updated=core('get',ref=target_margin_right_ref)['result']
        margin_bottom_updated=core('get',ref=target_margin_bottom_ref)['result']
        grid_x_updated=core('get',ref=target_grid_x_ref)['result']
        grid_y_updated=core('get',ref=target_grid_y_ref)['result']
        grid_width_updated=core('get',ref=target_grid_width_ref)['result']
        grid_height_updated=core('get',ref=target_grid_height_ref)['result']
        grid_column_gutter_updated=core('get',ref=target_grid_column_gutter_ref)['result']
        grid_row_gutter_updated=core('get',ref=target_grid_row_gutter_ref)['result']
        margin_property=next(item for item in core('properties')['result'] if item['ref']==target_margin_ref)
        margin_top_property=next(item for item in core('properties')['result'] if item['ref']==target_margin_top_ref)
        margin_right_property=next(item for item in core('properties')['result'] if item['ref']==target_margin_right_ref)
        margin_bottom_property=next(item for item in core('properties')['result'] if item['ref']==target_margin_bottom_ref)
        grid_x_property=next(item for item in core('properties')['result'] if item['ref']==target_grid_x_ref)
        grid_y_property=next(item for item in core('properties')['result'] if item['ref']==target_grid_y_ref)
        grid_width_property=next(item for item in core('properties')['result'] if item['ref']==target_grid_width_ref)
        grid_height_property=next(item for item in core('properties')['result'] if item['ref']==target_grid_height_ref)
        grid_column_gutter_property=next(item for item in core('properties')['result'] if item['ref']==target_grid_column_gutter_ref)
        grid_row_gutter_property=next(item for item in core('properties')['result'] if item['ref']==target_grid_row_gutter_ref)
        resolved_first=next(item for item in core('artboards',composition=comp['id'])['result']
            if item['authored']['id']==first['id'])
        assert margin_updated['authored']==margin_linked['authored'] and margin_updated['evaluated']==60 and \
            margin_property==margin_updated and resolved_first['evaluated']['layout']['margin']['left']==60
        assert margin_top_updated['authored']==margin_top_linked['authored'] and margin_top_updated['evaluated']==80 and \
            margin_top_updated['expression'] is True and margin_top_property==margin_top_updated and \
            resolved_first['evaluated']['layout']['margin']['top']==80
        assert margin_right_updated['authored']==margin_right_linked['authored'] and margin_right_updated['evaluated']==60 and \
            margin_right_property==margin_right_updated and resolved_first['evaluated']['layout']['margin']['right']==60
        assert margin_bottom_updated['authored']==margin_bottom_linked['authored'] and margin_bottom_updated['evaluated']==60 and \
            margin_bottom_updated['link'] is True and margin_bottom_updated['expression'] is True and \
            margin_bottom_property==margin_bottom_updated and resolved_first['evaluated']['layout']['margin']['bottom']==60
        assert grid_x_updated['authored']==grid_x_linked['authored'] and grid_x_updated['evaluated']==60 and \
            grid_x_property==grid_x_updated and resolved_first['evaluated']['layout']['grid']['bounds']['x']==60
        assert grid_y_updated['authored']==grid_y_linked['authored'] and grid_y_updated['evaluated']==60 and \
            grid_y_property==grid_y_updated and resolved_first['evaluated']['layout']['grid']['bounds']['y']==60
        assert grid_width_updated['authored']==grid_width_linked['authored'] and grid_width_updated['evaluated']==60 and \
            grid_width_property==grid_width_updated and \
            resolved_first['evaluated']['layout']['grid']['bounds']['width']==60
        assert grid_height_updated['authored']==grid_height_linked['authored'] and grid_height_updated['evaluated']==80 and \
            grid_height_property==grid_height_updated and \
            resolved_first['evaluated']['layout']['grid']['bounds']['height']==80
        assert grid_column_gutter_updated['authored']==grid_column_gutter_linked['authored'] and \
            grid_column_gutter_updated['evaluated']==12 and grid_column_gutter_property==grid_column_gutter_updated and \
            resolved_first['evaluated']['layout']['grid']['column_gutter']==12
        assert grid_row_gutter_updated['authored']==grid_row_gutter_linked['authored'] and \
            grid_row_gutter_updated['evaluated']==30 and grid_row_gutter_property==grid_row_gutter_updated and \
            resolved_first['evaluated']['layout']['grid']['row_gutter']==30
        grid_width_expression='ref("mcp-margin-upstream","","artboard.width") + 10'
        rev=apply([dict(type='set_grid_bounds_width_expression',target=target_grid_width_ref,
            expression=dict(source=grid_width_expression,version=1),replace_driver=True)],rev)
        grid_width_expressed=core('get',ref=target_grid_width_ref)['result']
        assert grid_width_expressed['authored']==dict(literal=first_size['width']-80,driver=None,
            source_kind='expression',expression=dict(source=grid_width_expression,version=1)) and \
            grid_width_expressed['evaluated']==70 and grid_width_expressed['expression'] is True
        assert next(item for item in core('properties')['result'] if item['ref']==target_grid_width_ref)==grid_width_expressed
        grid_y_expression='ref("mcp-margin-upstream","","artboard.width")'
        rev=apply([dict(type='set_grid_bounds_y_expression',target=target_grid_y_ref,
            expression=dict(source=grid_y_expression,version=1),replace_driver=True)],rev)
        grid_y_expressed=core('get',ref=target_grid_y_ref)['result']
        assert grid_y_expressed['authored']==dict(literal=20,driver=None,source_kind='expression',
            expression=dict(source=grid_y_expression,version=1)) and grid_y_expressed['evaluated']==60 and \
            grid_y_expressed['expression'] is True
        assert next(item for item in core('properties')['result'] if item['ref']==target_grid_y_ref)==grid_y_expressed
        grid_height_expression='ref("mcp-margin-upstream","","artboard.height") + 10'
        rev=apply([dict(type='set_grid_bounds_height_expression',target=target_grid_height_ref,
            expression=dict(source=grid_height_expression,version=1),replace_driver=True)],rev)
        grid_height_expressed=core('get',ref=target_grid_height_ref)['result']
        assert grid_height_expressed['authored']==dict(literal=first_size['height']-80,driver=None,
            source_kind='expression',expression=dict(source=grid_height_expression,version=1)) and \
            grid_height_expressed['evaluated']==90 and grid_height_expressed['expression'] is True
        assert next(item for item in core('properties')['result'] if item['ref']==target_grid_height_ref)==grid_height_expressed
        grid_y_native=core('inspect')['result']
        grid_y_layout=next(board for board in grid_y_native['compositions'][0]['artboards']
            if board['id']==first['id'])['layout']['grid']
        margin_top_native=next(board for board in grid_y_native['compositions'][0]['artboards']
            if board['id']==first['id'])['layout']['margin']
        assert grid_y_native['version']=='0.54' and grid_y_layout['bounds_y_expression']==dict(
            source=grid_y_expression,version=1) and 'bounds_y_driver' not in grid_y_layout and \
            grid_y_layout['bounds_width_expression']==dict(source=grid_width_expression,version=1) and \
            'bounds_width_driver' not in grid_y_layout and \
            grid_y_layout['bounds_height_expression']==dict(source=grid_height_expression,version=1) and \
            'bounds_height_driver' not in grid_y_layout and \
            grid_y_layout['column_gutter_driver']==dict(link=grid_column_gutter_source_ref) and \
            grid_y_layout['row_gutter_driver']==dict(link=grid_row_gutter_source_ref) and \
            margin_top_native['top_driver']==dict(link=margin_top_source_ref) and \
            margin_top_native['right_driver']==dict(link=margin_source_ref) and \
            margin_top_native['bottom_driver']==dict(link=margin_source_ref)
        rev=apply([dict(type='unlink_grid_bounds_y',target=target_grid_y_ref)],rev)
        grid_y_frozen=core('get',ref=target_grid_y_ref)['result']
        assert grid_y_frozen['authored']==dict(literal=60,driver=None,source_kind='literal') and grid_y_frozen['evaluated']==60
        rev=apply([dict(type='unlink_grid_bounds_width',target=target_grid_width_ref)],rev)
        grid_width_frozen=core('get',ref=target_grid_width_ref)['result']
        assert grid_width_frozen['authored']==dict(literal=70,driver=None,source_kind='literal') and \
            grid_width_frozen['evaluated']==70 and grid_width_frozen['link'] is True and \
            grid_width_frozen['expression'] is True
        rev=apply([dict(type='unlink_grid_bounds_height',target=target_grid_height_ref)],rev)
        grid_height_frozen=core('get',ref=target_grid_height_ref)['result']
        assert grid_height_frozen['authored']==dict(literal=90,driver=None,source_kind='literal') and \
            grid_height_frozen['evaluated']==90 and grid_height_frozen['link'] is True and \
            grid_height_frozen['expression'] is True
        rev=apply([dict(type='unlink_grid_column_gutter',target=target_grid_column_gutter_ref)],rev)
        grid_column_gutter_frozen=core('get',ref=target_grid_column_gutter_ref)['result']
        assert grid_column_gutter_frozen['authored']==dict(literal=12,driver=None,source_kind='literal') and \
            grid_column_gutter_frozen['evaluated']==12 and grid_column_gutter_frozen['link'] is True and \
            grid_column_gutter_frozen['expression'] is True
        rev=apply([dict(type='unlink_grid_row_gutter',target=target_grid_row_gutter_ref)],rev)
        grid_row_gutter_frozen=core('get',ref=target_grid_row_gutter_ref)['result']
        assert grid_row_gutter_frozen['authored']==dict(literal=30,driver=None,source_kind='literal') and \
            grid_row_gutter_frozen['evaluated']==30 and grid_row_gutter_frozen['link'] is True and \
            grid_row_gutter_frozen['expression'] is True
        grid_row_gutter_expression='ref("mcp-grid-column-gutter-source","","artboard.height") + 10'
        rev=apply([dict(type='set_grid_row_gutter_expression',target=target_grid_row_gutter_ref,
            expression=dict(source=grid_row_gutter_expression,version=1),replace_driver=False)],rev)
        grid_row_gutter_expressed=core('get',ref=target_grid_row_gutter_ref)['result']
        grid_row_gutter_expression_property=next(item for item in core('properties')['result']
            if item['ref']==target_grid_row_gutter_ref)
        assert grid_row_gutter_expressed['authored']==dict(literal=30,driver=None,source_kind='expression',
            expression=dict(source=grid_row_gutter_expression,version=1)) and \
            grid_row_gutter_expressed['evaluated']==40 and grid_row_gutter_expressed['expression'] is True and \
            grid_row_gutter_expression_property==grid_row_gutter_expressed
        row_gutter_expression_native=core('inspect')['result']
        row_gutter_expression_layout=next(board for board in row_gutter_expression_native['compositions'][0]['artboards']
            if board['id']==first['id'])['layout']['grid']
        assert row_gutter_expression_native['version']=='0.54' and \
            row_gutter_expression_layout['row_gutter_expression']==dict(source=grid_row_gutter_expression,version=1) and \
            'row_gutter_driver' not in row_gutter_expression_layout
        blocked_row_gutter_replacement=core('apply',expected_revision=rev,commands=[
            dict(type='link_grid_row_gutter',target=target_grid_row_gutter_ref,
                source=grid_row_gutter_source_ref,replace_driver=False)])
        assert not blocked_row_gutter_replacement['ok'] and \
            blocked_row_gutter_replacement['error']['code']=='DRIVEN_GRID_ROW_GUTTER' and \
            blocked_row_gutter_replacement['revision']==rev
        rev=apply([dict(type='link_grid_row_gutter',target=target_grid_row_gutter_ref,
            source=grid_row_gutter_source_ref,replace_driver=True)],rev)
        grid_row_gutter_replaced=core('get',ref=target_grid_row_gutter_ref)['result']
        assert grid_row_gutter_replaced['authored']==dict(literal=30,driver=grid_row_gutter_source_ref,
            source_kind='link') and grid_row_gutter_replaced['evaluated']==30
        grid_column_gutter_expression='ref("mcp-grid-column-gutter-source","","artboard.width") + 10'
        rev=apply([dict(type='set_grid_column_gutter_expression',target=target_grid_column_gutter_ref,
            expression=dict(source=grid_column_gutter_expression,version=1),replace_driver=False)],rev)
        grid_column_gutter_expressed=core('get',ref=target_grid_column_gutter_ref)['result']
        grid_column_gutter_expression_property=next(item for item in core('properties')['result']
            if item['ref']==target_grid_column_gutter_ref)
        assert grid_column_gutter_expressed['authored']==dict(literal=12,driver=None,source_kind='expression',
            expression=dict(source=grid_column_gutter_expression,version=1)) and \
            grid_column_gutter_expressed['evaluated']==22 and grid_column_gutter_expressed['expression'] is True and \
            grid_column_gutter_expression_property==grid_column_gutter_expressed
        column_gutter_expression_native=core('inspect')['result']
        column_gutter_expression_layout=next(board for board in column_gutter_expression_native['compositions'][0]['artboards']
            if board['id']==first['id'])['layout']['grid']
        assert column_gutter_expression_native['version']=='0.54' and \
            column_gutter_expression_layout['column_gutter_expression']==dict(source=grid_column_gutter_expression,version=1) and \
            'column_gutter_driver' not in column_gutter_expression_layout
        blocked_column_gutter_replacement=core('apply',expected_revision=rev,commands=[
            dict(type='link_grid_column_gutter',target=target_grid_column_gutter_ref,
                source=grid_column_gutter_source_ref,replace_driver=False)])
        assert not blocked_column_gutter_replacement['ok'] and \
            blocked_column_gutter_replacement['error']['code']=='DRIVEN_GRID_COLUMN_GUTTER' and \
            blocked_column_gutter_replacement['revision']==rev
        rev=apply([dict(type='link_grid_column_gutter',target=target_grid_column_gutter_ref,
            source=grid_column_gutter_source_ref,replace_driver=True)],rev)
        grid_column_gutter_replaced=core('get',ref=target_grid_column_gutter_ref)['result']
        assert grid_column_gutter_replaced['authored']==dict(literal=12,driver=grid_column_gutter_source_ref,
            source_kind='link') and grid_column_gutter_replaced['evaluated']==12
        grid_expression='ref("mcp-margin-upstream","","artboard.width") + 10'
        rev=apply([dict(type='set_grid_bounds_x_expression',target=target_grid_x_ref,
            expression=dict(source=grid_expression,version=1),replace_driver=True)],rev)
        grid_x_expressed=core('get',ref=target_grid_x_ref)['result']
        assert grid_x_expressed['authored']==dict(literal=40,driver=None,source_kind='expression',
            expression=dict(source=grid_expression,version=1)) and grid_x_expressed['evaluated']==70
        assert next(item for item in core('properties')['result'] if item['ref']==target_grid_x_ref)==grid_x_expressed
        expression_native=core('inspect')['result']
        expression_layout=next(board for board in expression_native['compositions'][0]['artboards']
            if board['id']==first['id'])['layout']['grid']
        assert expression_native['version']=='0.54' and expression_layout['bounds_x_expression']==dict(
            source=grid_expression,version=1) and 'bounds_x_driver' not in expression_layout
        margin_expression='ref("mcp-margin-upstream","","artboard.width") + 10'
        rev=apply([dict(type='set_margin_left_expression',target=target_margin_ref,
            expression=dict(source=margin_expression,version=1),replace_driver=True)],rev)
        margin_expressed=core('get',ref=target_margin_ref)['result']
        assert margin_expressed['authored']==dict(literal=40,driver=None,source_kind='expression',
            expression=dict(source=margin_expression,version=1)) and margin_expressed['evaluated']==70
        assert next(item for item in core('properties')['result'] if item['ref']==target_margin_ref)==margin_expressed
        margin_expression_native=core('inspect')['result']
        margin_expression_layout=next(board for board in margin_expression_native['compositions'][0]['artboards']
            if board['id']==first['id'])['layout']
        assert margin_expression_native['version']=='0.54' and margin_expression_layout['margin']['left_expression']==dict(
            source=margin_expression,version=1) and 'left_driver' not in margin_expression_layout['margin'] and \
            margin_expression_layout['margin']['top_driver']==dict(link=margin_top_source_ref)
        margin_top_expression='ref("mcp-margin-upstream","","artboard.height") + 10'
        rev=apply([dict(type='set_margin_top_expression',target=target_margin_top_ref,
            expression=dict(source=margin_top_expression,version=1),replace_driver=True)],rev)
        margin_top_expressed=core('get',ref=target_margin_top_ref)['result']
        assert margin_top_expressed['authored']==dict(literal=20,driver=None,source_kind='expression',
            expression=dict(source=margin_top_expression,version=1)) and margin_top_expressed['evaluated']==90 and \
            margin_top_expressed['expression'] is True
        assert next(item for item in core('properties')['result'] if item['ref']==target_margin_top_ref)==margin_top_expressed
        margin_top_expression_native=core('inspect')['result']
        margin_top_expression_layout=next(board for board in margin_top_expression_native['compositions'][0]['artboards']
            if board['id']==first['id'])['layout']['margin']
        assert margin_top_expression_native['version']=='0.54' and margin_top_expression_layout['top_expression']==dict(
            source=margin_top_expression,version=1) and 'top_driver' not in margin_top_expression_layout
        margin_right_expression='ref("mcp-margin-source","","artboard.width") + 10'
        rev=apply([dict(type='set_margin_right_expression',target=target_margin_right_ref,
            expression=dict(source=margin_right_expression,version=1),replace_driver=True)],rev)
        margin_right_expressed=core('get',ref=target_margin_right_ref)['result']
        assert margin_right_expressed['authored']==dict(literal=40,driver=None,source_kind='expression',
            expression=dict(source=margin_right_expression,version=1)) and margin_right_expressed['evaluated']==70 and \
            margin_right_expressed['expression'] is True
        assert next(item for item in core('properties')['result'] if item['ref']==target_margin_right_ref)==margin_right_expressed
        margin_right_expression_native=core('inspect')['result']
        margin_right_expression_layout=next(board for board in margin_right_expression_native['compositions'][0]['artboards']
            if board['id']==first['id'])['layout']['margin']
        assert margin_right_expression_native['version']=='0.54' and margin_right_expression_layout['right_expression']==dict(
            source=margin_right_expression,version=1) and 'right_driver' not in margin_right_expression_layout
        rev=apply([dict(type='unlink_margin_right',target=target_margin_right_ref)],rev)
        margin_right_frozen=core('get',ref=target_margin_right_ref)['result']
        assert margin_right_frozen['authored']==dict(literal=70,driver=None,source_kind='literal') and \
            margin_right_frozen['evaluated']==70 and margin_right_frozen['link'] is True and \
            margin_right_frozen['expression'] is True
        same_margin_top_expression=core('apply',expected_revision=rev,commands=[dict(type='set_margin_top_expression',
            target=target_margin_top_ref,expression=dict(source=margin_top_expression,version=1),replace_driver=False)])
        assert same_margin_top_expression['ok'] and same_margin_top_expression['revision']==rev and \
            not same_margin_top_expression['result']['changed_ids'] and core('get',ref=target_margin_top_ref)['result']==margin_top_expressed
        same_margin_expression=core('apply',expected_revision=rev,commands=[dict(type='set_margin_left_expression',
            target=target_margin_ref,expression=dict(source=margin_expression,version=1),replace_driver=False)])
        assert same_margin_expression['ok'] and same_margin_expression['revision']==rev and \
            not same_margin_expression['result']['changed_ids'] and core('get',ref=target_margin_ref)['result']==margin_expressed
        same_grid_expression=core('apply',expected_revision=rev,commands=[dict(type='set_grid_bounds_x_expression',
            target=target_grid_x_ref,expression=dict(source=grid_expression,version=1),replace_driver=False)])
        assert same_grid_expression['ok'] and same_grid_expression['revision']==rev and \
            not same_grid_expression['result']['changed_ids']
        assert core('get',ref=target_grid_x_ref)['result']==grid_x_expressed
        rev=apply([dict(type='link_grid_bounds_x',target=target_grid_x_ref,source=margin_source_ref,
            replace_driver=True)],rev)
        assert core('get',ref=target_grid_x_ref)['result']['evaluated']==60
        rev=apply([dict(type='set_grid_bounds_x_expression',target=target_grid_x_ref,
            expression=dict(source=grid_expression,version=1),replace_driver=True)],rev)
        assert core('get',ref=target_grid_x_ref)['result']==grid_x_expressed
        generic_margin_set=core('apply',expected_revision=rev,commands=[dict(type='set',ref=target_margin_ref,value=45)])
        assert not generic_margin_set['ok'] and generic_margin_set['error']['code']=='MISSING_REFERENCE' and \
            generic_margin_set['revision']==rev
        generic_margin_top_set=core('apply',expected_revision=rev,commands=[dict(type='set',ref=target_margin_top_ref,value=45)])
        assert not generic_margin_top_set['ok'] and generic_margin_top_set['error']['code']=='MISSING_REFERENCE' and \
            generic_margin_top_set['revision']==rev
        generic_grid_x_set=core('apply',expected_revision=rev,commands=[dict(type='set',ref=target_grid_x_ref,value=45)])
        assert not generic_grid_x_set['ok'] and generic_grid_x_set['error']['code']=='MISSING_REFERENCE' and \
            generic_grid_x_set['revision']==rev
        rev=apply([dict(type='unlink_grid_bounds_x',target=target_grid_x_ref)],rev)
        grid_x_frozen=core('get',ref=target_grid_x_ref)['result']
        assert grid_x_frozen['authored']==dict(literal=70,driver=None,source_kind='literal') and grid_x_frozen['evaluated']==70
        rev=apply([dict(type='unlink_margin_left',target=target_margin_ref)],rev)
        margin_frozen=core('get',ref=target_margin_ref)['result']
        assert margin_frozen['authored']==dict(literal=70,driver=None,source_kind='literal') and margin_frozen['evaluated']==70
        rev=apply([dict(type='unlink_margin_top',target=target_margin_top_ref)],rev)
        margin_top_frozen=core('get',ref=target_margin_top_ref)['result']
        assert margin_top_frozen['authored']==dict(literal=90,driver=None,source_kind='literal') and \
            margin_top_frozen['evaluated']==90 and margin_top_frozen['expression'] is True
        rev=apply([dict(type='unlink_margin_bottom',target=target_margin_bottom_ref)],rev)
        margin_bottom_frozen=core('get',ref=target_margin_bottom_ref)['result']
        assert margin_bottom_frozen['authored']==dict(literal=60,driver=None,source_kind='literal') and \
            margin_bottom_frozen['evaluated']==60 and margin_bottom_frozen['link'] is True and \
            margin_bottom_frozen['expression'] is True
        text_source=core('text_defaults')['result'];text_source.update(id='title-source',content='\u82b1\u306e\u5f62\nNect 2026',direction='vertical')
        rev=apply([dict(type='create_text',composition=comp['id'],parent='',id='title',name='Editable title',source=text_source)],rev)
        text_source['content']='\u82b1\u306e\u8a18\u61b6\nNect 2026'
        rev=apply([dict(type='update_text',object='title',source=text_source),dict(type='link',
            target=dict(object='title',point='',field='text.font_size'),binding=dict(source=source,scale=.2,offset=0,mode='copy_local_value'))],rev)
        assert core('get',ref=dict(object='title',point='',field='text.font_size'))['result']['evaluated']==31
        peer_source=core('text_defaults')['result'];peer_source.update(id='typed-peer-source',content='Peer text',family='Different Test Family',
            locale='fr-FR',layout='frame',direction='vertical',alignment='center')
        rev=apply([dict(type='create_text',composition=comp['id'],parent='',id='typed-peer',name='Second typed source',source=peer_source)],rev)
        text_fields=['text.content','text.family','text.locale','text.layout','text.direction','text.alignment']
        text_values={
            'title':[text_source['content'],text_source['family'],text_source['locale'],text_source['layout'],text_source['direction'],text_source['alignment']],
            'typed-peer':[peer_source['content'],peer_source['family'],peer_source['locale'],peer_source['layout'],peer_source['direction'],peer_source['alignment']],
        }
        properties_before=core('properties')['result']
        for object_id,expected_values in text_values.items():
            entries=[entry for entry in properties_before if entry['ref']['object']==object_id and entry['ref']['field'] in text_fields]
            assert len(entries)==6 and {entry['ref']['field'] for entry in entries}==set(text_fields)
            for field,value in zip(text_fields,expected_values):
                ref=dict(object=object_id,point='',field=field)
                entry=next(item for item in entries if item['ref']==ref)
                result=core('get',ref=ref)['result']
                kind='string' if field in text_fields[:3] else 'enum'
                authored=dict(literal=value,driver=None)
                assert result==entry and result['type']==kind and result['authored']==authored
                assert result['evaluated']==value and result['link']==(field in ('text.content','text.family','text.locale','text.direction','text.layout','text.alignment')) and result['expression'] is False
                if kind=='enum':
                    choices={'text.layout':['auto','frame'],'text.direction':['horizontal','vertical'],
                        'text.alignment':['start','center','end']}[field]
                    assert result['choices']==choices
        assert core('resolve_name',name='Editable title',point='',field='text.content')['result']==dict(object='title',point='',field='text.content')
        before_reads=tool('nect_session')
        properties_after=core('properties')['result']
        core('get',ref=dict(object='title',point='',field='text.content'))
        core('resolve_name',name='Editable title',point='',field='text.content')
        assert properties_after==properties_before and tool('nect_session')['revision']==before_reads['revision']==rev
        invalid=core('get',ref=dict(object='title',point='',field='text.unregistered'))
        assert not invalid['ok'] and invalid['error']['code']=='UNKNOWN_TEXT_PROPERTY'
        invalid=core('get',ref=dict(object='title',point='a-point',field='text.content'))
        assert not invalid['ok'] and invalid['error']['code']=='INVALID_TEXT_REF'
        invalid=core('get',ref=dict(object='path-0',point='',field='text.content'))
        assert not invalid['ok'] and invalid['error']['code']=='TYPE_MISMATCH'
        rejected=core('apply',expected_revision=rev,commands=[dict(type='link_properties',targets=[dict(object='title',point='',field='text.font_size')],
            source=dict(object='title',point='',field='text.content'),relative=False)])
        assert not rejected['ok'] and rejected['error']['code']=='MISSING_REFERENCE' and rejected['revision']==rev
        rejected=core('apply',expected_revision=rev,commands=[dict(type='set_expression',targets=[dict(object='title',point='',field='text.content')],
            expression=dict(source='1',version=1),replace_binding=False)])
        assert not rejected['ok'] and rejected['revision']==rev
        current_document=core('inspect')['result'];current_comp=next(item for item in current_document['compositions'] if item['id']==comp['id'])
        rev=apply([dict(type='rename',object='title',name='Renamed typed title'),dict(type='reorder_objects',composition=comp['id'],parent='',
            order=['title']+[object_id for object_id in current_comp['roots'] if object_id!='title'])],rev)
        assert core('resolve_name',name='Renamed typed title',point='',field='text.content')['result']==dict(object='title',point='',field='text.content')
        text_source['content']='Edited through UpdateText'
        rev=apply([dict(type='update_text',object='title',source=text_source)],rev)
        assert core('get',ref=dict(object='title',point='',field='text.content'))['result']['evaluated']=='Edited through UpdateText'
        title_content=dict(object='title',point='',field='text.content')
        peer_content=dict(object='typed-peer',point='',field='text.content')
        rev=apply([dict(type='link_text_content',target=title_content,source=peer_content,replace_driver=False)],rev)
        linked=core('get',ref=title_content)['result']
        assert linked['authored']==dict(literal='Edited through UpdateText',driver=dict(link=peer_content))
        assert linked['evaluated']=='Peer text' and linked['link'] is True and linked['expression'] is False
        peer_source['content']='Revised peer text'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        linked_doc=core('inspect')['result'];title_obj=next(item for item in linked_doc['objects'] if item['id']=='title')
        assert title_obj['text']['content']=='Edited through UpdateText' and title_obj['text']['content_driver']==dict(link=peer_content)
        assert core('get',ref=title_content)['result']['evaluated']=='Revised peer text'
        blocked=core('apply',expected_revision=rev,commands=[dict(type='update_text',object='title',source=dict(text_source,content='Blocked literal edit'))])
        assert not blocked['ok'] and blocked['error']['code']=='DRIVEN_PROPERTY' and blocked['revision']==rev
        rev=apply([dict(type='unlink_text_content',target=title_content)],rev)
        assert core('get',ref=title_content)['result']['authored']==dict(literal='Revised peer text',driver=None)
        peer_source['content']='Later peer text'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_content)['result']['evaluated']=='Revised peer text'
        title_family=dict(object='title',point='',field='text.family')
        peer_family=dict(object='typed-peer',point='',field='text.family')
        rev=apply([dict(type='link_text_family',target=title_family,source=peer_family,replace_driver=False)],rev)
        linked_family=core('get',ref=title_family)['result']
        assert linked_family['authored']==dict(literal=text_source['family'],driver=dict(link=peer_family))
        assert linked_family['evaluated']=='Different Test Family' and linked_family['link'] is True and linked_family['expression'] is False
        peer_source['family']='Revised Test Family'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_family)['result']['evaluated']=='Revised Test Family'
        linked_doc=core('inspect')['result'];title_obj=next(item for item in linked_doc['objects'] if item['id']=='title')
        assert title_obj['text']['family']==text_source['family'] and title_obj['text']['family_driver']==dict(link=peer_family)
        blocked_family_source=dict(title_obj['text'],family='Blocked family')
        blocked=core('apply',expected_revision=rev,commands=[dict(type='update_text',object='title',source=blocked_family_source)])
        assert not blocked['ok'] and blocked['error']['code']=='DRIVEN_PROPERTY' and blocked['revision']==rev
        rev=apply([dict(type='unlink_text_family',target=title_family)],rev)
        assert core('get',ref=title_family)['result']['authored']==dict(literal='Revised Test Family',driver=None)
        peer_source['family']='Later Test Family'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_family)['result']['evaluated']=='Revised Test Family'
        title_direction=dict(object='title',point='',field='text.direction')
        peer_direction=dict(object='typed-peer',point='',field='text.direction')
        rev=apply([dict(type='link_text_direction',target=title_direction,source=peer_direction,replace_driver=False)],rev)
        linked_direction=core('get',ref=title_direction)['result']
        assert linked_direction['authored']==dict(literal='vertical',driver=dict(link=peer_direction))
        assert linked_direction['evaluated']=='vertical' and linked_direction['choices']==['horizontal','vertical']
        title_obj=next(item for item in core('inspect')['result']['objects'] if item['id']=='title')
        blocked_direction_source=dict(title_obj['text'],direction='horizontal')
        blocked=core('apply',expected_revision=rev,commands=[dict(type='update_text',object='title',source=blocked_direction_source)])
        assert not blocked['ok'] and blocked['error']['code']=='DRIVEN_PROPERTY' and blocked['revision']==rev
        peer_source['direction']='horizontal'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_direction)['result']['evaluated']=='horizontal'
        peer_source['direction']='vertical'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_direction)['result']['evaluated']=='vertical'
        rev=apply([dict(type='unlink_text_direction',target=title_direction)],rev)
        assert core('get',ref=title_direction)['result']['authored']==dict(literal='vertical',driver=None)
        undo=core('undo',expected_revision=rev);assert undo['ok'];rev=undo['revision']
        assert core('get',ref=title_direction)['result']['authored']==dict(literal='vertical',driver=dict(link=peer_direction))
        peer_source['direction']='horizontal'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_direction)['result']['evaluated']=='horizontal'
        rev=apply([dict(type='unlink_text_direction',target=title_direction)],rev)
        assert core('get',ref=title_direction)['result']['authored']==dict(literal='horizontal',driver=None)
        title_layout=dict(object='title',point='',field='text.layout')
        peer_layout=dict(object='typed-peer',point='',field='text.layout')
        rev=apply([dict(type='link_text_layout',target=title_layout,source=peer_layout,replace_driver=False)],rev)
        linked_layout=core('get',ref=title_layout)['result']
        assert linked_layout['authored']==dict(literal='auto',driver=dict(link=peer_layout))
        assert linked_layout['evaluated']=='frame' and linked_layout['choices']==['auto','frame']
        title_obj=next(item for item in core('inspect')['result']['objects'] if item['id']=='title')
        blocked_layout_source=dict(title_obj['text'],layout='frame')
        blocked=core('apply',expected_revision=rev,commands=[dict(type='update_text',object='title',source=blocked_layout_source)])
        assert not blocked['ok'] and blocked['error']['code']=='DRIVEN_PROPERTY' and blocked['revision']==rev
        peer_source['layout']='auto'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_layout)['result']['evaluated']=='auto'
        peer_source['layout']='frame'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_layout)['result']['evaluated']=='frame'
        rev=apply([dict(type='unlink_text_layout',target=title_layout)],rev)
        assert core('get',ref=title_layout)['result']['authored']==dict(literal='frame',driver=None)
        undo=core('undo',expected_revision=rev);assert undo['ok'];rev=undo['revision']
        assert core('get',ref=title_layout)['result']['authored']==dict(literal='auto',driver=dict(link=peer_layout))
        peer_source['layout']='auto'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_layout)['result']['evaluated']=='auto'
        rev=apply([dict(type='unlink_text_layout',target=title_layout)],rev)
        peer_source['layout']='frame'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_layout)['result']['authored']==dict(literal='auto',driver=None)
        title_alignment=dict(object='title',point='',field='text.alignment')
        peer_alignment=dict(object='typed-peer',point='',field='text.alignment')
        rev=apply([dict(type='link_text_alignment',target=title_alignment,source=peer_alignment,replace_driver=False)],rev)
        linked_alignment=core('get',ref=title_alignment)['result']
        assert linked_alignment['authored']==dict(literal='start',driver=dict(link=peer_alignment))
        assert linked_alignment['evaluated']=='center' and linked_alignment['choices']==['start','center','end']
        title_obj=next(item for item in core('inspect')['result']['objects'] if item['id']=='title')
        blocked_alignment_source=dict(title_obj['text'],alignment='end')
        blocked=core('apply',expected_revision=rev,commands=[dict(type='update_text',object='title',source=blocked_alignment_source)])
        assert not blocked['ok'] and blocked['error']['code']=='DRIVEN_PROPERTY' and blocked['revision']==rev
        peer_source['alignment']='end'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_alignment)['result']['evaluated']=='end'
        rev=apply([dict(type='unlink_text_alignment',target=title_alignment)],rev)
        assert core('get',ref=title_alignment)['result']['authored']==dict(literal='end',driver=None)
        undo=core('undo',expected_revision=rev);assert undo['ok'];rev=undo['revision']
        assert core('get',ref=title_alignment)['result']['authored']==dict(literal='start',driver=dict(link=peer_alignment))
        peer_source['alignment']='center'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_alignment)['result']['evaluated']=='center'
        rev=apply([dict(type='unlink_text_alignment',target=title_alignment)],rev)
        peer_source['alignment']='end'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_alignment)['result']['authored']==dict(literal='center',driver=None)
        title_locale=dict(object='title',point='',field='text.locale')
        peer_locale=dict(object='typed-peer',point='',field='text.locale')
        locale_literal=core('get',ref=title_locale)['result']['authored']['literal']
        rev=apply([dict(type='link_text_locale',target=title_locale,source=peer_locale,replace_driver=False)],rev)
        linked_locale=core('get',ref=title_locale)['result']
        locale_entry=next(item for item in core('properties')['result'] if item['ref']==title_locale)
        assert linked_locale['authored']==dict(literal=locale_literal,driver=dict(link=peer_locale))
        assert linked_locale['evaluated']=='fr-FR' and linked_locale['link'] is True
        assert locale_entry==linked_locale
        title_obj=next(item for item in core('inspect')['result']['objects'] if item['id']=='title')
        blocked_locale_source=dict(title_obj['text'],locale='de-DE')
        blocked=core('apply',expected_revision=rev,commands=[dict(type='update_text',object='title',source=blocked_locale_source)])
        assert not blocked['ok'] and blocked['error']['code']=='DRIVEN_PROPERTY' and blocked['revision']==rev
        peer_source['locale']='ja-JP'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_locale)['result']['evaluated']=='ja-JP'
        rev=apply([dict(type='unlink_text_locale',target=title_locale)],rev)
        assert core('get',ref=title_locale)['result']['authored']==dict(literal='ja-JP',driver=None)
        undo=core('undo',expected_revision=rev);assert undo['ok'];rev=undo['revision']
        assert core('get',ref=title_locale)['result']['authored']==dict(literal=locale_literal,driver=dict(link=peer_locale))
        peer_source['locale']='ar-SA'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_locale)['result']['evaluated']=='ar-SA'
        rev=apply([dict(type='unlink_text_locale',target=title_locale)],rev)
        peer_source['locale']='fr-FR'
        rev=apply([dict(type='update_text',object='typed-peer',source=peer_source)],rev)
        assert core('get',ref=title_locale)['result']['authored']==dict(literal='ar-SA',driver=None)
        rev=apply([dict(type='delete_objects',objects=['typed-peer'])],rev)
        layout=core('text_layout',object='title')['result'];assert layout['glyph_count']>0 and layout['used_fonts']
        assert core('export_plan',composition=comp['id'],artboard=first['id'])['result']['text_policy']=='outlines'
        expected_svg_paths+=1
        brand=dict(id='brand-color',name='Brand accent',space='srgb',profile='srgb',alpha='straight',
            rgba=[{'literal':v} for v in (.2,.4,.6,.8)])
        brand_ref=dict(object='brand-color',point='',field='color')
        title_color=dict(object='title',point='',field='op.title-fill.color')
        gradient_color=dict(object='path-0',point='',field='op.motif-fill.gradient.motif-gradient.stop.start-stop.color')
        independent_color=dict(object='path-2',point='',field='op.path-2-stroke.color')
        rev=apply([dict(type='create_named_color',color=brand),dict(type='link_color',target=title_color,source=brand_ref),
            dict(type='link_color',target=gradient_color,source=brand_ref),dict(type='set_color',ref=independent_color,
            value=dict(space='srgb',profile='srgb',alpha='straight',rgba=[.2,.4,.6,.8]))],rev)
        changed=core('apply',expected_revision=rev,commands=[dict(type='rename_named_color',color='brand-color',name='Linked accent'),
            dict(type='set_color',ref=brand_ref,value=dict(space='srgb',profile='srgb',alpha='straight',rgba=[.7,.3,.2,.9]))])
        assert changed['ok'] and {'brand-color','title','path-0','path-1'}.issubset(changed['result']['changed_ids'])
        assert 'path-2' not in changed['result']['changed_ids'];rev=changed['revision']
        discovered_color=core('resolve_name',name='Linked accent',point='',field='color')['result']
        assert discovered_color==brand_ref
        assert core('get',ref=discovered_color)['result']['evaluated']['rgba']==[.7,.3,.2,.9]
        assert core('get',ref=title_color)['result']['evaluated']['rgba']==[.7,.3,.2,.9]
        assert core('get',ref=independent_color)['result']['evaluated']['rgba']==[.2,.4,.6,.8]
        assert core('get',ref=gradient_color)['result']['link']==brand_ref
        blocked=core('apply',expected_revision=rev,commands=[dict(type='delete_named_color',color='brand-color')])
        assert not blocked['ok'] and blocked['error']['code']=='MISSING_REFERENCE' and blocked['revision']==rev
        assert core('used_colors')['result']['equal_values_imply_link'] is False
        primitives={item['type']:item['template'] for item in core('primitive_types')['result']}
        polygon=primitives['nect.shape.polygon'];polygon['id']='linked-polygon-source'
        polygon['parameters']['points']={'literal':6}
        star=primitives['nect.shape.star'];star['id']='linked-star-source'
        polygon_count=dict(object='linked-polygon',point='',field='generator.points')
        star_count=dict(object='linked-star',point='',field='generator.points')
        star['parameters']['points']={'literal':5,'binding':dict(source=polygon_count,scale=1,offset=0,mode='copy_local_value')}
        rev=apply([dict(type='create_primitive',composition=comp['id'],parent='',id='linked-polygon',name='Linked polygon',source=polygon),
                   dict(type='create_primitive',composition=comp['id'],parent='',id='linked-star',name='Linked star',source=star)],rev)
        assert core('get',ref=star_count)['result']['evaluated']==6
        corrected_vertex=dict(object='linked-star',point='linked-star-source-outer-1-6',field='x')
        rev=apply([dict(type='set',ref=corrected_vertex,value=123)],rev)
        before_topology=core('inspect')['result']
        blocked=core('apply',expected_revision=rev,commands=[dict(type='set',ref=polygon_count,value=7)])
        assert not blocked['ok'] and blocked['error']['code']=='UNRESOLVED_POINT_EDIT' and blocked['revision']==rev
        assert core('inspect')['result']==before_topology
        rev=apply([dict(type='clear_point_edit',object='linked-star'),dict(type='set',ref=polygon_count,value=7)],rev)
        assert core('get',ref=star_count)['result']['evaluated']==7
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==before_topology and core('get',ref=corrected_vertex)['result']['evaluated']==123
        expected_svg_paths+=2
        # Structure and transform following remain separate across GUI/MCP/native.
        rev=apply([dict(type='group_contiguous',composition=comp['id'],parent='',members=['path-3','path-2'],id='transform-group',name='Structure group'),
            dict(type='set',ref=dict(object='transform-group',point='',field='transform.tx'),value=100),
            dict(type='set',ref=dict(object='path-3',point='',field='transform.tx'),value=20),
            dict(type='set',ref=dict(object='path-2',point='',field='transform.tx'),value=30),
            dict(type='set_transform_parent',object='path-2',parent='path-3',preserve_world=False)],rev)
        transforms=lambda:{x['object']:x for x in core('transforms')['result']}
        assert transforms()['path-2']['world'][4]==150
        followed=core('apply',expected_revision=rev,commands=[dict(type='set',ref=dict(object='path-3',point='',field='transform.tx'),value=40)])
        assert followed['ok'] and {'path-3','path-2'}.issubset(followed['result']['changed_ids']);rev=followed['revision']
        assert transforms()['path-2']['world'][4]==170
        rev=apply([dict(type='center_anchor',object='path-3'),dict(type='transform_around_anchor',object='path-3',rotation=90,scale_x=1,scale_y=1),
            dict(type='set_position',object='path-3',x=320,y=180)],rev)
        assert all(abs(v-e)<1e-8 for v,e in zip(transforms()['path-3']['position'],[320,180]))
        before_world=transforms()['path-2']['world']
        for parent in (None,'path-3'):
            rev=apply([dict(type='set_transform_parent',object='path-2',parent=parent,preserve_world=True)],rev)
            assert all(abs(v-e)<1e-8 for v,e in zip(transforms()['path-2']['world'],before_world))
        before_cycle=core('inspect')['result']
        rejected=core('apply',expected_revision=rev,commands=[dict(type='set_transform_parent',object='path-3',parent='path-2',preserve_world=False)])
        assert not rejected['ok'] and rejected['error']['code']=='TRANSFORM_CYCLE' and rejected['revision']==rev
        assert core('inspect')['result']==before_cycle
        # One snapshot serves every batch target, including relative links.
        batch_refs=[dict(object=f'path-{i}',point='',field='transform.tx') for i in (10,11,12)]
        rev=apply([dict(type='set',ref=ref,value=value) for ref,value in zip(batch_refs,(100,200,300))],rev)
        batch=core('apply',expected_revision=rev,commands=[dict(type='edit_properties',targets=batch_refs,value=10,relative=True)])
        assert batch['ok'] and {'path-10','path-11','path-12'}.issubset(batch['result']['changed_ids']);rev=batch['revision']
        assert [core('get',ref=ref)['result']['evaluated'] for ref in batch_refs]==[110,210,310]
        before_batch=core('inspect')['result']
        rev=apply([dict(type='edit_properties',targets=batch_refs,value=400,relative=False)],rev)
        assert [core('get',ref=ref)['result']['evaluated'] for ref in batch_refs]==[400,400,400]
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==before_batch
        batch_source=dict(object='path-9',point='',field='transform.tx')
        rev=apply([dict(type='link_properties',targets=batch_refs,source=batch_source,relative=True)],rev)
        rev=apply([dict(type='set',ref=batch_source,value=10)],rev)
        assert [core('get',ref=ref)['result']['evaluated'] for ref in batch_refs]==[120,220,320]
        before_rejected=core('inspect')['result']
        rejected=core('apply',expected_revision=rev,commands=[dict(type='edit_properties',targets=batch_refs,value=500,relative=False)])
        assert not rejected['ok'] and rejected['error']['code']=='DRIVEN_PROPERTY' and core('inspect')['result']==before_rejected
        rev=apply([dict(type='unlink_properties',targets=batch_refs)],rev)
        rev=apply([dict(type='set',ref=batch_source,value=20)],rev)
        assert [core('get',ref=ref)['result']['evaluated'] for ref in batch_refs]==[120,220,320]
        old_transforms=transforms();before_translation=core('inspect')['result']
        rev=apply([dict(type='translate_objects',objects=['path-3','path-2'],dx=25,dy=-15)],rev)
        new_transforms=transforms()
        for id_ in ('path-3','path-2'):
            expected_world=old_transforms[id_]['world'][:];expected_world[4]+=25;expected_world[5]-=15
            assert all(abs(v-e)<1e-8 for v,e in zip(new_transforms[id_]['world'],expected_world))
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==before_translation
        old_transforms=transforms()
        rev=apply([dict(type='transform_objects',objects=['path-3','path-2'],rotation=90,scale_x=1,scale_y=1,pivot=[0,0])],rev)
        rotated_transforms=transforms()
        for id_ in ('path-3','path-2'):
            a,b,c,d,tx,ty=old_transforms[id_]['world']
            assert all(abs(v-e)<1e-8 for v,e in zip(rotated_transforms[id_]['world'],[-b,a,-d,c,-ty,tx]))
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==before_translation
        stroke_before=core('inspect')['result']
        rev=apply([dict(type='stroke_style',object='path-2',operation='path-2-stroke',line_cap='round',line_join='bevel',miter_limit=6)],rev)
        styled=next(o for o in core('inspect')['result']['objects'] if o['id']=='path-2')
        stroke=next(op for op in styled['stack'] if op['id']=='path-2-stroke')
        assert stroke['version']==2 and stroke['line_cap']=='round' and stroke['line_join']=='bevel'
        assert core('get',ref=dict(object='path-2',point='',field='op.path-2-stroke.miter_limit'))['result']['evaluated']==6
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==stroke_before
        # Formulas use the live Session and survive its normal save/restart path.
        formula='ref("path-9","","transform.tx") * 2 + 5'
        rev=apply([dict(type='set_expression',targets=batch_refs,expression=dict(source=formula,version=1),replace_binding=False)],rev)
        assert all(core('get',ref=ref)['result']['evaluated']==45 for ref in batch_refs)
        formula_before=core('inspect')['result']
        rejected=core('apply',expected_revision=rev,commands=[dict(type='set_expression',targets=batch_refs,expression=dict(source='1 / 0',version=1),replace_binding=False)])
        assert not rejected['ok'] and core('inspect')['result']==formula_before
        changed=core('apply',expected_revision=rev,commands=[dict(type='set',ref=batch_source,value=30)])
        assert changed['ok'] and {'path-9','path-10','path-11','path-12'}.issubset(changed['result']['changed_ids']);rev=changed['revision']
        assert all(core('get',ref=ref)['result']['evaluated']==65 for ref in batch_refs)
        assert core('expression_language')['result']['source_bytes']==4096
        history_before=core('history')['result'];history_state=history_before['current_id']
        historical_document=core('inspect')['result']
        for step in range(80):
            rev=apply([dict(type='set',ref=source,value=180+step)],rev)
        long_history=core('history')['result'];later_state=long_history['current_id']
        assert len(long_history['states'])>80 and long_history['cross_restart'] is False
        assert long_history['retained_bytes']<=long_history['max_bytes']
        restored=core('restore_history',state_id=history_state,expected_revision=rev)
        assert restored['ok'] and restored['revision']==rev+1 and {'path-0','path-1'}.issubset(restored['result']['changed_ids']);rev+=1
        assert core('inspect')['result']==historical_document
        assert core('restore_history',state_id=later_state,expected_revision=rev)['ok'];rev+=1
        assert core('get',ref=target)['result']['evaluated']==271
        assert core('restore_history',state_id=history_state,expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==historical_document
        unchanged=core('restore_history',state_id=history_state,expected_revision=rev)
        assert unchanged['ok'] and unchanged['revision']==rev and unchanged['result']['changed'] is False
        native = temp / 'scenario.nect'
        saved = tool('nect_file', dict(identity, op='save', path=str(native), expected_revision=rev))
        assert saved['ok'], saved
        expected = core('inspect')['result']
        png = tool('nect_export_png',dict(identity,op='export_png',expected_revision=rev,
            path=str(temp/'output.png'),composition=comp['id'],artboard=comp['artboards'][0]['id'],scale=1,background='transparent'))
        assert png['ok'] and png['revision']==rev,png
        png_bytes=(temp/'output.png').read_bytes()
        assert png_bytes[:8]==b'\x89PNG\r\n\x1a\n'
        assert struct.unpack('>II',png_bytes[16:24])==(png['result']['width'],png['result']['height'])
        svg = core('export_svg', composition=comp['id'], artboard=comp['artboards'][0]['id'])['result']
        assert len(ET.fromstring(svg).findall('.//{http://www.w3.org/2000/svg}path')) == expected_svg_paths
        svg_root=ET.fromstring(svg);ns='{http://www.w3.org/2000/svg}'
        exported_follower=next(g for g in svg_root.iter(ns+'g') if g.attrib.get('id')=='path-2')
        matrix=[float(v) for v in exported_follower.attrib['transform'].removeprefix('matrix(').removesuffix(')').split()]
        assert all(abs(v-e)<1e-8 for v,e in zip(matrix,transforms()['path-2']['world']))
        structural_group=next(g for g in svg_root.iter(ns+'g') if g.attrib.get('id')=='transform-group')
        assert 'transform' not in structural_group.attrib, 'SVG must not double-apply structural transforms to external followers'
        gradient_defs=svg_root.findall('.//'+ns+'linearGradient')
        assert len(gradient_defs)==copies and len({x.attrib['id'] for x in gradient_defs})==copies
        assert all(x.attrib['gradientUnits']=='userSpaceOnUse' for x in gradient_defs)
        assert float(gradient_defs[0].findall(ns+'stop')[0].attrib['offset'])==0
        assert core('inspect')['result'] == expected
        assert json.loads(native.read_text(encoding='utf-8')) == expected
        # Geometry masks and common compositing share the formal MCP Session,
        # including a hidden editable source and retained native/recovery state.
        mask_source=dict(primitives['nect.shape.circle'])
        mask_source['id']='mcp-mask-source'
        # Templates were only modified for Polygon/Star above.
        rev=apply([dict(type='create_primitive',composition=comp['id'],parent='',id='mcp-mask',
                        name='Hidden mask',source=mask_source),
                   dict(type='mask_objects',composition=comp['id'],parent='',members=['linked-star','mcp-mask'],
                        id='mcp-masked-group',mask_id='mcp-geometry-clip',name='Masked star',top=True),
                   dict(type='set_compositing',object='mcp-masked-group',blend='screen',isolated=False),
                   dict(type='set',ref=dict(object='mcp-masked-group',point='',field='composite.opacity'),value=.65)],rev)
        masked=core('inspect')['result']
        assert next(o for o in masked['objects'] if o['id']=='mcp-mask')['visible'] is False
        point_edit_ref=dict(object='mcp-mask',point='',field='point_edit.enabled')
        absent_point_edit=core('get',ref=point_edit_ref)
        assert not absent_point_edit['ok'] and absent_point_edit['error']['code']=='NO_POINT_EDIT'
        assert not any(item['ref']==point_edit_ref for item in core('properties')['result'])
        rev=apply([dict(type='set',ref=dict(object='mcp-mask',point='mcp-mask-source-east',field='x'),value=123)],rev)
        point_edit=core('get',ref=point_edit_ref)
        assert point_edit['ok'] and point_edit['result']['type']=='bool'
        assert point_edit['result']['unit']=='boolean' and point_edit['result']['space']=='local'
        assert point_edit['result']['origin']=='authored'
        assert point_edit['result']['authored']==dict(literal=True,driver=None)
        assert point_edit['result']['evaluated'] is True
        assert point_edit['result']['link'] is False and point_edit['result']['expression'] is False
        assert core('resolve_name',name='Hidden mask',point='',field='point_edit.enabled')['result']==point_edit_ref
        assert any(item['ref']==point_edit_ref for item in core('properties')['result'])
        assert point_edit['result']==desktop_api_call(endpoint,dict(identity,op='core',
            request=dict(op='get',ref=point_edit_ref)))['result']
        bad_point_edit=core('apply',expected_revision=rev,commands=[dict(type='set',ref=point_edit_ref,value=0)])
        assert not bad_point_edit['ok'] and bad_point_edit['revision']==rev
        rev=apply([dict(type='enable_point_edit',object='mcp-mask',enabled=False)],rev)
        bypassed_point_edit=core('get',ref=point_edit_ref)
        assert bypassed_point_edit['result']['authored']==dict(literal=False,driver=None)
        assert bypassed_point_edit['result']['evaluated'] is False
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('get',ref=point_edit_ref)['result']['authored']==dict(literal=True,driver=None)
        visibility_ref=dict(object='mcp-mask',point='',field='object.visible')
        rev=apply([dict(type='link_object_visibility',target=visibility_ref,
                        source=dict(object='linked-star',point='',field='object.visible'),replace_driver=False)],rev)
        linked_visibility=core('get',ref=visibility_ref)['result']
        assert linked_visibility['authored']==dict(literal=False,driver=dict(link=dict(
            object='linked-star',point='',field='object.visible'))) and linked_visibility['evaluated'] is True
        refused=core('apply',expected_revision=rev,commands=[dict(type='set_visibility',object='mcp-mask',visible=True)])
        assert not refused['ok'] and refused['error']['code']=='DRIVEN_PROPERTY' and refused['revision']==rev
        rev=apply([dict(type='unlink_object_visibility',target=visibility_ref)],rev)
        frozen_visibility=core('get',ref=visibility_ref)['result']
        assert frozen_visibility['authored']==dict(literal=True,driver=None) and frozen_visibility['evaluated'] is True
        rev=apply([dict(type='set_visibility',object='mcp-mask',visible=False)],rev)
        visibility=core('get',ref=visibility_ref)
        assert visibility['ok'] and visibility['result']['type']=='bool'
        assert visibility['result']['authored']==dict(literal=False,driver=None)
        assert visibility['result']['evaluated'] is False and visibility['result']['link'] is True
        direct_visibility=desktop_api_call(endpoint,dict(identity,op='core',request=dict(op='get',ref=visibility_ref)))
        assert visibility['result']==direct_visibility['result'],(visibility,direct_visibility)
        assert any(item['ref']==visibility_ref for item in core('properties')['result'])
        mask_ref=dict(object='mcp-mask',point='',field='generator.radius')
        changed=core('apply',expected_revision=rev,commands=[dict(type='set',ref=mask_ref,value=75)])
        assert changed['ok'] and {'mcp-mask','mcp-masked-group'}.issubset(changed['result']['changed_ids']);rev=changed['revision']
        plan=core('compositing_plan',composition=comp['id'])['result']
        group=next(n for n in plan['roots'] if n['object']=='mcp-masked-group')
        assert group['isolated'] and group['opacity']==.65 and group['mask']['source']=='mcp-mask'
        assert group['blend']=='screen' and plan['backdrop']=='transparent'
        isolation_ref=dict(object='mcp-masked-group',point='',field='composite.isolated')
        isolation=core('get',ref=isolation_ref)
        assert isolation['ok'] and isolation['result']['type']=='bool'
        assert isolation['result']['authored']==dict(literal=False,driver=None)
        assert isolation['result']['evaluated'] is False and group['isolated'] is True
        assert isolation['result']==desktop_api_call(endpoint,dict(identity,op='core',
            request=dict(op='get',ref=isolation_ref)))['result']
        assert any(item['ref']==isolation_ref for item in core('properties')['result'])
        # Formal MCP exposes the same authored/evaluated contract and commands as the direct API.
        rev=apply([dict(type='set_compositing',object='mcp-mask',blend='normal',isolated=True)],rev)
        rev=apply([dict(type='link_composite_isolated',target=isolation_ref,
            source=dict(object='mcp-mask',point='',field='composite.isolated'),replace_driver=False)],rev)
        linked_isolation=core('get',ref=isolation_ref)
        assert linked_isolation['ok'] and linked_isolation['result']['authored']==dict(literal=False,driver=dict(link=dict(
            object='mcp-mask',point='',field='composite.isolated')))
        assert linked_isolation['result']['evaluated'] is True and linked_isolation['result']['link'] is True
        assert linked_isolation['result']==desktop_api_call(endpoint,dict(identity,op='core',
            request=dict(op='get',ref=isolation_ref)))['result']
        assert any(item['ref']==isolation_ref and item['authored']==linked_isolation['result']['authored']
            and item['evaluated'] is True for item in core('properties')['result'])
        rev=apply([dict(type='set_compositing',object='mcp-masked-group',blend='multiply',isolated=False)],rev)
        assert core('get',ref=isolation_ref)['result']['authored']['literal'] is False
        assert core('get',ref=isolation_ref)['result']['evaluated'] is True
        rev=apply([dict(type='set_compositing',object='mcp-masked-group',blend='screen',isolated=False)],rev)
        refused=core('apply',expected_revision=rev,commands=[dict(type='set_compositing',
            object='mcp-masked-group',blend='screen',isolated=True)])
        assert not refused['ok'] and refused['error']['code']=='DRIVEN_PROPERTY' and refused['revision']==rev
        rev=apply([dict(type='unlink_composite_isolated',target=isolation_ref)],rev)
        frozen_isolation=core('get',ref=isolation_ref)['result']
        assert frozen_isolation['authored']==dict(literal=True,driver=None) and frozen_isolation['evaluated'] is True
        rev=apply([dict(type='set_compositing',object='mcp-mask',blend='normal',isolated=False),
            dict(type='set_compositing',object='mcp-masked-group',blend='screen',isolated=False)],rev)
        restored_isolation=core('get',ref=isolation_ref)['result']
        assert restored_isolation['authored']==dict(literal=False,driver=None) and restored_isolation['evaluated'] is False
        mask_enabled_ref=dict(object='mcp-masked-group',point='',field='mask.enabled')
        mask_enabled=core('get',ref=mask_enabled_ref)
        assert mask_enabled['ok'] and mask_enabled['result']['type']=='bool'
        assert mask_enabled['result']['unit']=='boolean' and mask_enabled['result']['space']=='local'
        assert mask_enabled['result']['origin']=='authored'
        assert mask_enabled['result']['authored']==dict(literal=True,driver=None)
        assert mask_enabled['result']['evaluated'] is True
        assert mask_enabled['result']['link'] is False and mask_enabled['result']['expression'] is False
        assert mask_enabled['result']==desktop_api_call(endpoint,dict(identity,op='core',
            request=dict(op='get',ref=mask_enabled_ref)))['result']
        assert any(item['ref']==mask_enabled_ref for item in core('properties')['result'])
        before_mask_disable=core('inspect')['result']
        disabled_mask=dict(id='mcp-geometry-clip',source='mcp-mask',version=1,enabled=False,fill_rule='nonzero')
        rev=apply([dict(type='set_mask',object='mcp-masked-group',mask=disabled_mask)],rev)
        bypassed=core('get',ref=mask_enabled_ref)['result']
        bypass_plan=core('compositing_plan',composition=comp['id'])['result']
        bypass_group=next(n for n in bypass_plan['roots'] if n['object']=='mcp-masked-group')
        assert bypassed['authored']==dict(literal=False,driver=None) and bypassed['evaluated'] is False
        assert bypass_group['mask'] is None and any(o['id']=='mcp-mask' for o in core('inspect')['result']['objects'])
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==before_mask_disable
        assert core('get',ref=mask_enabled_ref)['result']['authored']==dict(literal=True,driver=None)
        target_mask_enabled_ref=dict(object='mcp-masked-group',point='',field='mask.mcp-geometry-clip.enabled')
        source_mask_enabled_ref=dict(object='mcp-mask',point='',field='mask.mcp-source-clip.enabled')
        rev=apply([dict(type='set_mask',object='mcp-masked-group',mask=dict(disabled_mask,enabled=False)),
                   dict(type='set_mask',object='mcp-mask',mask=dict(id='mcp-source-clip',source='linked-star',
                       version=1,enabled=True,fill_rule='nonzero'))],rev)
        rev=apply([dict(type='link_mask_enabled',target=target_mask_enabled_ref,
                        source=source_mask_enabled_ref,replace_driver=False)],rev)
        linked_mask_enabled=core('get',ref=target_mask_enabled_ref)['result']
        assert linked_mask_enabled['authored']==dict(literal=False,driver=dict(link=source_mask_enabled_ref))
        assert linked_mask_enabled['evaluated'] is True and linked_mask_enabled['link'] is True
        assert linked_mask_enabled==desktop_api_call(endpoint,dict(identity,op='core',
            request=dict(op='get',ref=target_mask_enabled_ref)))['result']
        assert core('get',ref=mask_enabled_ref)['result']['authored']==dict(literal=False,driver=None)
        assert next(item for item in core('properties')['result'] if item['ref']==target_mask_enabled_ref)==linked_mask_enabled
        assert next(o for o in core('inspect')['result']['objects'] if o['id']=='mcp-masked-group')['compositing']['mask']['enabled_driver']==dict(link=source_mask_enabled_ref)
        rev=apply([dict(type='set_mask',object='mcp-mask',mask=dict(id='mcp-source-clip',source='linked-star',
            version=1,enabled=False,fill_rule='nonzero'))],rev)
        bypassed_linked_mask=core('get',ref=target_mask_enabled_ref)['result']
        bypassed_linked_plan=core('compositing_plan',composition=comp['id'])['result']
        bypassed_linked_group=next(n for n in bypassed_linked_plan['roots'] if n['object']=='mcp-masked-group')
        assert bypassed_linked_mask['authored']==dict(literal=False,driver=dict(link=source_mask_enabled_ref))
        assert bypassed_linked_mask['evaluated'] is False and bypassed_linked_group['mask'] is None
        rev=apply([dict(type='set_mask',object='mcp-mask',mask=dict(id='mcp-source-clip',source='linked-star',
            version=1,enabled=True,fill_rule='nonzero'))],rev)
        rev=apply([dict(type='unlink_mask_enabled',target=target_mask_enabled_ref)],rev)
        frozen_mask_enabled=core('get',ref=target_mask_enabled_ref)['result']
        assert frozen_mask_enabled['authored']==dict(literal=True,driver=None) and frozen_mask_enabled['evaluated'] is True
        assert core('get',ref=mask_enabled_ref)['result']['authored']==dict(literal=True,driver=None)
        before_bad=core('inspect')['result']
        bad_mask_scalar=core('apply',expected_revision=rev,commands=[dict(type='set',ref=mask_enabled_ref,value=0)])
        assert not bad_mask_scalar['ok'] and bad_mask_scalar['revision']==rev
        assert core('inspect')['result']==before_bad
        before_bad=core('inspect')['result']
        bad=core('apply',expected_revision=rev,commands=[dict(type='set_visibility',object='mcp-mask',visible=True),
            dict(type='set_compositing',object='mcp-masked-group',blend='unsupported-add',isolated=False)])
        assert not bad['ok'] and bad['revision']==rev and core('inspect')['result']==before_bad
        mask_svg=core('export_svg',composition=comp['id'],artboard=comp['artboards'][0]['id'])['result']
        assert 'id="mcp-geometry-clip"' in mask_svg and 'mix-blend-mode:screen' in mask_svg
        assert 'id="mcp-mask"' not in mask_svg
        # Point Edit enabled links use the exact retained correction identity
        # through both the formal MCP command and the desktop API adapter.
        point_edit_target_ref=dict(object='mcp-mask',point='',field='point_edit.mcp-mask-source-point-edit.enabled')
        point_edit_source=dict(primitives['nect.shape.circle']);point_edit_source['id']='mcp-point-edit-link-generator'
        point_edit_source_ref=dict(object='mcp-point-edit-source',point='',
            field='point_edit.mcp-point-edit-link-generator-point-edit.enabled')
        point_edit_target_point=dict(object='mcp-mask',point='mcp-mask-source-east',field='x')
        rev=apply([dict(type='create_primitive',composition=comp['id'],parent='',id='mcp-point-edit-source',
                        name='Point Edit link source',source=point_edit_source),
                   dict(type='set',ref=dict(object='mcp-point-edit-source',point='mcp-point-edit-link-generator-east',field='x'),value=360),
                   dict(type='enable_point_edit',object='mcp-mask',enabled=False)],rev)
        assert core('resolve_name',name='Hidden mask',point='',field=point_edit_target_ref['field'])['result']==point_edit_target_ref
        rev=apply([dict(type='link_point_edit_enabled',target=point_edit_target_ref,
                        source=point_edit_source_ref,replace_driver=False)],rev)
        linked_point_edit=core('get',ref=point_edit_target_ref)['result']
        direct_point_edit=desktop_api_call(endpoint,dict(identity,op='core',request=dict(op='get',ref=point_edit_target_ref)))
        assert linked_point_edit['authored']==dict(literal=False,driver=dict(link=point_edit_source_ref))
        assert linked_point_edit['evaluated'] is True and linked_point_edit['link'] is True
        assert direct_point_edit['ok'] and direct_point_edit['result']==linked_point_edit
        assert next(value for value in core('properties')['result'] if value['ref']==point_edit_target_ref)==linked_point_edit
        refused_point_edit=core('apply',expected_revision=rev,commands=[
            dict(type='enable_point_edit',object='mcp-mask',enabled=True)])
        assert not refused_point_edit['ok'] and refused_point_edit['error']['code']=='DRIVEN_PROPERTY' and refused_point_edit['revision']==rev
        rev=apply([dict(type='enable_point_edit',object='mcp-point-edit-source',enabled=False)],rev)
        bypassed_point_edit=core('get',ref=point_edit_target_ref)['result']
        bypassed_point=core('get',ref=point_edit_target_point)['result']
        assert bypassed_point_edit['authored']==dict(literal=False,driver=dict(link=point_edit_source_ref))
        assert bypassed_point_edit['evaluated'] is False and bypassed_point['evaluated']!=123
        rev=apply([dict(type='enable_point_edit',object='mcp-point-edit-source',enabled=True)],rev)
        assert core('get',ref=point_edit_target_ref)['result']['evaluated'] is True
        assert core('get',ref=point_edit_target_point)['result']['evaluated']==123
        point_edit_unlink=desktop_api_call(endpoint,dict(identity,op='core',request=dict(op='apply',
            expected_revision=rev,commands=[dict(type='unlink_point_edit_enabled',target=point_edit_target_ref)])))
        assert point_edit_unlink['ok'];rev=point_edit_unlink['revision']
        frozen_point_edit=core('get',ref=point_edit_target_ref)['result']
        assert frozen_point_edit['authored']==dict(literal=True,driver=None) and frozen_point_edit['evaluated'] is True
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        undone_point_edit=core('get',ref=point_edit_target_ref)['result']
        assert undone_point_edit['authored']==dict(literal=False,driver=dict(link=point_edit_source_ref))
        assert undone_point_edit['evaluated'] is True
        assert core('redo',expected_revision=rev)['ok'];rev+=1
        assert core('get',ref=point_edit_target_ref)['result']['authored']==dict(literal=True,driver=None)
        assert core('get',ref=point_edit_target_ref)['result']['evaluated'] is True
        rev=apply([dict(type='enable_point_edit',object='mcp-point-edit-source',enabled=False)],rev)
        assert core('get',ref=point_edit_target_ref)['result']['evaluated'] is True
        assert core('get',ref=point_edit_target_point)['result']['evaluated']==123
        # Retained Offset also shapes a hidden mask source through formal MCP.
        offset_template=next(v['template'] for v in core('operator_types')['result'] if v['type']=='nect.shape.offset')
        offset_template['id']='mcp-offset';offset_template['line_join']='round'
        before_offset=core('inspect')['result']
        mask_object=next(o for o in before_offset['objects'] if o['id']=='mcp-mask')
        rev=apply([dict(type='add_operation',object='mcp-mask',operation=offset_template,index=len(mask_object['stack']))],rev)
        amount=dict(object='mcp-mask',point='',field='op.mcp-offset.amount')
        changed=core('apply',expected_revision=rev,commands=[dict(type='set_expression',targets=[amount],
            expression=dict(version=1,source='ref("mcp-mask","","generator.radius") / 5'),replace_binding=False)])
        assert changed['ok'] and {'mcp-mask','mcp-masked-group'}.issubset(changed['result']['changed_ids']);rev=changed['revision']
        assert core('get',ref=amount)['result']['evaluated']==15
        after_offset=core('inspect')['result']
        bad=core('apply',expected_revision=rev,commands=[dict(type='operation_options',object='mcp-mask',operation='mcp-offset',composite='below',fill_rule='nonzero',line_join='unknown')])
        assert not bad['ok'] and core('inspect')['result']==after_offset
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('get',ref=amount)['result']['evaluated']==10
        assert core('redo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==after_offset
        # Local image lifecycle uses the same Session through formal MCP.
        image_path=temp/'linked.png';original_image=make_png((220,80,30));image_path.write_bytes(original_image)
        def image(action,**kwargs):
            return tool('nect_image',dict(identity,op='asset',asset='mcp-image-asset',action=action,expected_revision=rev,**kwargs))
        imported=tool('nect_image',dict(identity,op='import_image',expected_revision=rev,path=str(image_path),mode='linked',
            composition=comp['id'],parent='',asset='mcp-image-asset',id='mcp-image',name='Linked artwork',x=40,y=60))
        assert imported['ok'],imported
        rev=imported['revision'];assert image('status')['result']['state']=='current'
        rev=apply([dict(type='set',ref=dict(object='mcp-image',point='',field='image.width'),value=160),
                   dict(type='set',ref=dict(object='mcp-image',point='',field='image.height'),value=120),
                   dict(type='create_image',composition=comp['id'],parent='',id='mcp-image-copy',name='Shared image',
                        source=dict(asset='mcp-image-asset',width=dict(literal=80),height=dict(literal=60))),
                   dict(type='set_mask',object='mcp-image',mask=dict(id='mcp-image-mask',source='mcp-mask',version=1,enabled=True,fill_rule='nonzero')),
                   dict(type='set_compositing',object='mcp-image',blend='multiply',isolated=False)],rev)
        accepted=core('inspect')['result'];assert base64.b64decode(accepted['raster_assets'][0]['bytes'])==original_image
        image_path.write_bytes(make_png((20,180,220)))
        assert image('check')['result']['state']=='changed' and core('inspect')['result']==accepted
        reload=image('reload');assert reload['ok'] and {'mcp-image','mcp-image-copy','mcp-image-asset'}.issubset(reload['result']['changed_ids']);rev=reload['revision']
        assert core('get',ref=dict(object='mcp-image',point='',field='image.width'))['result']['evaluated']==160
        assert core('undo',expected_revision=rev)['ok'];rev+=1;assert core('inspect')['result']==accepted
        assert core('redo',expected_revision=rev)['ok'];rev+=1
        linked_before_missing=core('inspect')['result'];image_path.unlink()
        assert image('check')['result']['state']=='missing'
        failed=image('reload');assert not failed['ok'] and failed['revision']==rev and core('inspect')['result']==linked_before_missing
        relink=temp/'replacement.png';relink.write_bytes(original_image)
        changed=image('relink',path=str(relink));assert changed['ok'];rev=changed['revision']
        embedded=image('embed');assert embedded['ok'];rev=embedded['revision'];relink.unlink()
        assert image('status')['result']['state']=='embedded'
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert image('check')['result']['state']=='missing'
        # Save a missing link intentionally: restart/recovery must keep accepted pixels.
        image_svg=core('export_svg',composition=comp['id'],artboard=comp['artboards'][0]['id'])
        assert image_svg['ok'] and image_svg['result'].count('data:image/png;base64,')==1
        assert 'bytes' not in core('assets')['result'][0]
        # Automatic protection must reach a verified receipt without an explicit
        # Save/recover call, through the real desktop event loop and worker.
        rev=apply([dict(type='set',ref=source,value=156)],rev)
        expected=core('inspect')['result']
        deadline=time.monotonic()+8
        while time.monotonic()<deadline:
            durable=tool('nect_session')['persistence']
            if durable['saved_revision']==rev and durable['recovery_revision']==rev:
                break
            time.sleep(.05)
        else:
            raise AssertionError(('Live save did not finish',durable))
        assert json.loads(native.read_text(encoding='utf-8'))==expected
        recovery = temp / 'recovery' / (identity['session_id'] + '.nect')
        # Actual abnormal termination: recover the exact committed snapshot in a new process.
        desktop.kill();desktop.wait(timeout=5)
        desktop, _ = start(endpoint, temp, native)
        assert core('apply', expected_revision=0, commands=[dict(type='set', ref=source, value=1)])['error']['code'] == 'SESSION_CONFLICT'
        live = tool('nect_session'); identity = {key: live[key] for key in ('session_id', 'document_id')}
        assert core('inspect')['result'] == expected
        assert tool('nect_image',dict(identity,op='asset',asset='mcp-image-asset',action='status',expected_revision=0))['result']['state']=='unchecked'
        assert core('get', ref=target)['result']['evaluated'] == 168
        assert len(core('history')['result']['states'])==1
        # Recovery is opened through the same formal MCP surface as an unnamed
        # document, so subsequent live saves cannot replace the recovery source.
        assert tool('nect_file',dict(identity,op='open_recovery',path=str(recovery),expected_revision=0))['ok']
        live=tool('nect_session');identity={key:live[key] for key in ('session_id','document_id')}
        assert live['file']=='' and live['persistence']['saved_revision'] is None
        assert core('inspect')['result']==expected
        align_commands=[]
        for index,x in enumerate((10,110,270)):
            align_commands.append(dict(type='create_path',composition=comp['id'],parent='',id=f'align-{index}',name=f'Align {index}',
                contours=[dict(id=f'align-contour-{index}',closed=False,points=[point(f'align-{index}-a',x,5),point(f'align-{index}-b',x+20,5)])]))
        alignment_rev=apply(align_commands,0)
        alignment_before=core('inspect')['result']
        alignment_rev=apply([dict(type='align_objects',objects=['align-0','align-1'],axis='x',alignment='min',artboard=None)],alignment_rev)
        assert core('get',ref=dict(object='align-1',point='',field='transform.tx'))['result']['evaluated']==-100
        assert core('undo',expected_revision=alignment_rev)['ok'] and core('inspect')['result']==alignment_before
        spacing_rev=apply([dict(type='distribute_objects',objects=['align-2','align-0','align-1'],axis='x')],alignment_rev+1)
        assert core('get',ref=dict(object='align-1',point='',field='transform.tx'))['result']['evaluated']==30
        assert core('undo',expected_revision=spacing_rev)['ok'] and core('inspect')['result']==alignment_before
        line_image_path=temp/'line-candidate.png';line_image_path.write_bytes(make_png((220,80,30),width=5,height=1))
        line_image=tool('nect_image',dict(identity,op='import_image',expected_revision=spacing_rev+1,path=str(line_image_path),
            mode='embedded',composition=comp['id'],parent='',asset='mcp-line-asset',id='mcp-line-image',
            name='One-pixel line fixture',x=100,y=100))
        assert line_image['ok'],line_image
        svg_input=temp/'original-vector.svg'
        svg_input.write_text('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 40 30"><g fill="#c04020"><path d="M2 2h30v20h-30zM10 10A5 5 0 0 1 20 10"/><circle cx="20" cy="15" r="4"/></g></svg>',encoding='utf-8')
        vector_before=core('inspect')['result']
        vector=tool('nect_import_svg',dict(identity,op='import_svg',expected_revision=line_image['revision'],path=str(svg_input),composition=comp['id'],prefix='mcp-vector',name='Vector',x=10,y=20))
        assert vector['ok'] and vector['result']['paths']==2 and vector['result']['root']=='mcp-vector'
        vector_document_before_analysis=core('inspect')['result']
        vector_analysis_request=dict(identity,op='analyze_regions',expected_revision=vector['revision'],
            composition=comp['id'],artboard=comp['artboards'][0]['id'],scale=1,threshold=128)
        direct_vector_analysis=desktop_api_call(endpoint,vector_analysis_request)
        mcp_vector_analysis=tool('nect_analyze_regions',vector_analysis_request)
        assert direct_vector_analysis==mcp_vector_analysis and direct_vector_analysis['ok']
        assert direct_vector_analysis['revision']==vector['revision']
        assert direct_vector_analysis['result']['source_revision']==vector['revision']
        vector_false_request=dict(vector_analysis_request,include_color_groups=False)
        direct_vector_false=desktop_api_call(endpoint,vector_false_request)
        mcp_vector_false=tool('nect_analyze_regions',vector_false_request)
        assert direct_vector_false==direct_vector_analysis==mcp_vector_false
        vector_morphology=direct_vector_analysis['result']['morphology']
        assert vector_morphology['operation']=='dilate'
        assert vector_morphology['kernel']=='cross-4-radius-1'
        assert vector_morphology['border']=='outside-background-clipped'
        assert vector_morphology['coordinate_space']=='artboard-output-pixels'
        assert vector_morphology['runs'] and vector_morphology['area']==sum(run['width'] for run in vector_morphology['runs'])
        vector_erosion=direct_vector_analysis['result']['erosion']
        assert vector_erosion['operation']=='erode'
        assert vector_erosion['kernel']=='cross-4-radius-1'
        assert vector_erosion['border']=='outside-background'
        assert vector_erosion['coordinate_space']=='artboard-output-pixels'
        assert vector_erosion['runs'] and vector_erosion['area']==sum(run['width'] for run in vector_erosion['runs'])
        vector_boolean=direct_vector_analysis['result']['mask_boolean']
        assert vector_boolean['operation']=='difference'
        assert vector_boolean['operands']==[
            dict(role='left',mask='morphology',operation='dilate'),
            dict(role='right',mask='erosion',operation='erode')]
        assert vector_boolean['coordinate_space']=='artboard-output-pixels'
        assert vector_boolean['width']==direct_vector_analysis['result']['width']
        assert vector_boolean['height']==direct_vector_analysis['result']['height']
        assert vector_boolean['source_revision']==vector['revision']
        assert vector_boolean['runs'] and vector_boolean['area']==sum(run['width'] for run in vector_boolean['runs'])
        previous=None
        for run in vector_boolean['runs']:
            assert run['width']>0 and 0<=run['y']<vector_boolean['height']
            assert 0<=run['x'] and run['x']+run['width']<=vector_boolean['width']
            if previous is not None:
                assert run['y']>previous['y'] or (run['y']==previous['y'] and previous['x']+previous['width']<run['x'])
            previous=run
        assert core('inspect')['result']==vector_document_before_analysis
        assert tool('nect_session')['revision']==vector['revision']
        assert direct_vector_analysis['result']['regions'], 'Filled SVG artwork yields at least one analyzed region'
        vector_contours=direct_vector_analysis['result']['outer_contours']
        assert vector_contours, 'Filled SVG artwork yields at least one outer contour'
        assert all(contour['closed'] is True and 0 <= contour['region_index'] < len(direct_vector_analysis['result']['regions'])
                   and len(contour['vertices']) >= 4
                   and all(len(point) == 2 and all(isinstance(coordinate, int) for coordinate in point)
                           for point in contour['vertices']) for contour in vector_contours)
        vector_edges=direct_vector_analysis['result']['edge_runs']
        assert vector_edges and direct_vector_analysis['result']['edge_pixel_count']>0, 'Filled SVG artwork yields analyzed edge pixels'
        assert direct_vector_analysis['result']['edge_rule']=='foreground-4-neighbor'
        assert sum(run['width'] for run in vector_edges)==direct_vector_analysis['result']['edge_pixel_count']
        vector_lines=direct_vector_analysis['result']['line_candidates']
        assert vector_lines and any(candidate['direction']=='horizontal' and candidate['length_pixels']==5
                                    for candidate in vector_lines), 'A five-pixel raster stroke yields a nonempty horizontal candidate'
        assert direct_vector_analysis['result']['line_rule']=='exact-one-pixel-wide-4-direction-min3'
        assert direct_vector_analysis['result']['line_coordinate_space']=='artboard-output-pixel-centers'
        assert any(o['id']=='mcp-vector' and o['kind']=='group' for o in core('inspect')['result']['objects'])
        vector_document=core('inspect')['result']
        ungroup_revision=apply([dict(type='ungroup',composition=comp['id'],parent='',group='mcp-vector')],vector['revision'])
        assert not any(o['id']=='mcp-vector' for o in core('inspect')['result']['objects'])
        assert core('undo',expected_revision=ungroup_revision)['ok'] and core('inspect')['result']==vector_document
        assert core('undo',expected_revision=ungroup_revision+1)['ok'] and core('inspect')['result']==vector_before
        # P03-SAVE-AS-02: formal MCP Save As preserves typed Text state and
        # moves the active native/recovery provenance to an owned destination.
        live=tool('nect_session');identity={key:live[key] for key in ('session_id','document_id')};rev=live['revision']
        save_source=core('text_defaults')['result']
        save_source.update(id='mcp-save-as-source-text',content='Save As source',layout='auto')
        save_target=core('text_defaults')['result']
        save_target.update(id='mcp-save-as-target-text',content='Target text in an owned frame',layout='frame')
        save_target['parameters']['frame_width']=dict(literal=96)
        save_target['parameters']['frame_height']=dict(literal=48)
        rev=apply([dict(type='create_text',composition=comp['id'],parent='',id='mcp-save-as-source',name='Save As source',source=save_source),
                   dict(type='create_text',composition=comp['id'],parent='',id='mcp-save-as-target',name='Save As target',source=save_target)],rev)
        save_source_ref=dict(object='mcp-save-as-source',point='',field='text.layout')
        save_target_ref=dict(object='mcp-save-as-target',point='',field='text.layout')
        rev=apply([dict(type='link_text_layout',target=save_target_ref,source=save_source_ref,replace_driver=False)],rev)
        linked_layout=core('get',ref=save_target_ref)['result']
        save_width_ref=dict(object='mcp-save-as-target',point='',field='text.frame_width')
        save_height_ref=dict(object='mcp-save-as-target',point='',field='text.frame_height')
        assert linked_layout['authored']==dict(literal='frame',driver=dict(link=save_source_ref))
        assert linked_layout['evaluated']=='auto'
        assert core('get',ref=save_width_ref)['result']['evaluated']==96
        assert core('get',ref=save_height_ref)['result']['evaluated']==48
        save_grid_expression='ref("mcp-margin-upstream","","artboard.width") + 10'
        rev=apply([dict(type='set_grid_bounds_x_expression',target=target_grid_x_ref,
            expression=dict(source=save_grid_expression,version=1),replace_driver=False)],rev)
        save_grid=core('get',ref=target_grid_x_ref)['result']
        assert save_grid['authored']==dict(literal=70,driver=None,source_kind='expression',
            expression=dict(source=save_grid_expression,version=1)) and save_grid['evaluated']==70
        save_document=core('inspect')['result']
        save_session_id=identity['session_id'];save_document_id=identity['document_id']
        original_native=temp/'mcp-save-as-original.nect'
        destination_native=temp/'mcp-save-as-destination.nect'
        original_saved=tool('nect_file',dict(identity,op='save',path=str(original_native),expected_revision=rev))
        assert original_saved['ok'] and original_saved['revision']==rev,original_saved
        original_live=tool('nect_session')
        assert Path(original_live['file']).resolve()==original_native.resolve()
        assert original_live['revision']==rev and original_live['persistence']['saved_revision']==rev
        assert tool('nect_file',dict(identity,op='recover',expected_revision=rev))['ok']
        original_live=tool('nect_session')
        assert original_live['persistence']['recovery_revision']==rev
        original_recovery=Path(original_live['persistence']['recovery_file'])
        original_receipt=original_recovery.with_suffix('.recovery.json')
        original_bytes=original_native.read_bytes()
        original_hash=hashlib.sha256(original_bytes).hexdigest()
        protected_bytes=original_recovery.read_bytes()
        protected_receipt=original_receipt.read_bytes()
        assert protected_bytes==original_bytes
        assert json.loads(protected_receipt)['source_file']==original_live['file']

        # Stale identities/revisions must be rejected before touching any path.
        rejected_identity_path=temp/'stale-identity-save.nect'
        stale_session=tool('nect_file',dict(identity,session_id='stale-save-session',op='save',
            path=str(rejected_identity_path),expected_revision=rev))
        assert not stale_session['ok'] and stale_session['error']['code']=='SESSION_CONFLICT',stale_session
        stale_document=tool('nect_file',dict(identity,document_id='stale-save-document',op='save',
            path=str(rejected_identity_path),expected_revision=rev))
        assert not stale_document['ok'] and stale_document['error']['code']=='SESSION_CONFLICT',stale_document
        stale_revision_path=temp/'stale-revision-save.nect'
        stale_revision=tool('nect_file',dict(identity,op='save',path=str(stale_revision_path),expected_revision=rev-1))
        assert not stale_revision['ok'] and stale_revision['error']['code']=='REVISION_CONFLICT',stale_revision
        rejected_destination=temp/'missing-save-as-parent'/'failed.nect'
        assert not rejected_destination.parent.exists()
        failed_save=tool('nect_file',dict(identity,op='save',path=str(rejected_destination),expected_revision=rev))
        assert not failed_save['ok'] and failed_save['error']['code']=='IO_ERROR',failed_save
        unchanged=tool('nect_session')
        assert unchanged['session_id']==save_session_id and unchanged['document_id']==save_document_id
        assert unchanged['revision']==rev and Path(unchanged['file']).resolve()==original_native.resolve()
        assert unchanged['persistence']['saved_revision']==rev and unchanged['persistence']['recovery_revision']==rev
        assert core('inspect')['result']==save_document
        assert not rejected_destination.exists() and not rejected_identity_path.exists() and not stale_revision_path.exists()
        assert original_native.read_bytes()==original_bytes and hashlib.sha256(original_native.read_bytes()).hexdigest()==original_hash
        assert original_recovery.read_bytes()==protected_bytes and original_receipt.read_bytes()==protected_receipt

        destination_saved=tool('nect_file',dict(identity,op='save',path=str(destination_native),expected_revision=rev))
        assert destination_saved['ok'] and destination_saved['revision']==rev,destination_saved
        destination_live=tool('nect_session')
        assert Path(destination_live['file']).resolve()==destination_native.resolve()
        assert destination_live['revision']==rev and destination_live['persistence']['saved_revision']==rev
        assert tool('nect_file',dict(identity,op='recover',expected_revision=rev))['ok']
        destination_live=tool('nect_session')
        assert destination_live['persistence']['recovery_revision']==rev
        destination_bytes=destination_native.read_bytes()
        destination_hash=hashlib.sha256(destination_bytes).hexdigest()
        assert destination_bytes==original_bytes and destination_hash==original_hash
        assert original_native.read_bytes()==original_bytes and hashlib.sha256(original_native.read_bytes()).hexdigest()==original_hash
        recovery_receipt=json.loads(original_receipt.read_text(encoding='utf-8'))
        assert recovery_receipt['source_file']==destination_live['file']
        assert recovery_receipt['revision']==rev and recovery_receipt['sha256']==hashlib.sha256(original_recovery.read_bytes()).hexdigest()
        native_save_as=json.loads(destination_bytes.decode('utf-8'))
        assert native_save_as['version']=='0.54'
        native_objects={obj['id']:obj for obj in native_save_as['objects']}
        saved_source=native_objects['mcp-save-as-source']['text']
        saved_target=native_objects['mcp-save-as-target']['text']
        saved_guides={guide['id']:guide for composition in native_save_as['compositions']
            for guide in composition['guides']}
        assert saved_guides[guide_source_ref['object']]['position']==120
        assert saved_guides[guide_target_ref['object']]['position']==240
        assert saved_guides[guide_target_ref['object']]['position_expression']==guide_expression
        assert 'position_driver' not in saved_guides[guide_target_ref['object']]
        assert saved_source['id']=='mcp-save-as-source-text' and saved_source['layout']=='auto'
        assert saved_target['id']=='mcp-save-as-target-text' and saved_target['layout']=='frame'
        assert saved_target['layout_driver']==dict(link=save_source_ref)
        assert saved_target['parameters']['frame_width']['literal']==96
        assert saved_target['parameters']['frame_height']['literal']==48
        saved_grid=next(board for board in native_save_as['compositions'][0]['artboards']
            if board['id']==first['id'])['layout']['grid']
        assert saved_grid['bounds']['x']==70 and saved_grid['bounds']['width']==70 and \
            'bounds_width_driver' not in saved_grid
        assert saved_grid['bounds_x_expression']==dict(source=save_grid_expression,version=1)
        assert 'bounds_x_driver' not in saved_grid

        # Cold-open the Save As destination in a new desktop Session and read it
        # through the same formal MCP client used for the live mutations.
        desktop.kill();desktop.wait(timeout=5)
        desktop,_=start(endpoint,temp,destination_native)
        cold_live=tool('nect_session')
        identity={key:cold_live[key] for key in ('session_id','document_id')}
        assert cold_live['session_id']!=save_session_id and cold_live['document_id']==save_document_id
        assert cold_live['revision']==0 and cold_live['persistence']['saved_revision']==0
        assert Path(cold_live['file']).resolve()==destination_native.resolve()
        cold_layout=core('get',ref=save_target_ref)['result']
        assert cold_layout['authored']==dict(literal='frame',driver=dict(link=save_source_ref))
        assert cold_layout['evaluated']=='auto'
        cold_grid=core('get',ref=target_grid_x_ref)['result']
        assert cold_grid==save_grid
        cold_guide=core('get',ref=guide_target_ref)['result']
        assert cold_guide['authored']==dict(literal=240,driver=None,source_kind='expression',expression=guide_expression)
        assert cold_guide['evaluated']==140
        assert core('get',ref=save_width_ref)['result']['evaluated']==96
        assert core('get',ref=save_height_ref)['result']['evaluated']==48
        cold_document=core('inspect')['result']
        cold_objects={obj['id']:obj for obj in cold_document['objects']}
        assert cold_objects['mcp-save-as-source']['text']['id']=='mcp-save-as-source-text'
        assert cold_objects['mcp-save-as-target']['text']['layout_driver']==dict(link=save_source_ref)
        assert destination_native.read_bytes()==destination_bytes
        assert original_native.read_bytes()==original_bytes and hashlib.sha256(original_native.read_bytes()).hexdigest()==original_hash
        fill_live=tool('nect_session');identity={key:fill_live[key] for key in ('session_id','document_id')}
        fill_source=dict(object='mcp-fill-source',point='',field='op.mcp-source-fill.fill_rule')
        fill_target=dict(object='mcp-fill-target',point='',field='op.mcp-target-fill.fill_rule')
        fill_operation=lambda id_,rule:dict(id=id_,type='nect.paint.fill',version=1,enabled=True,
            parameters={key:dict(literal=value) for key,value in dict(r=0,g=0,b=0,a=1).items()},composite='below',fill_rule=rule)
        square=lambda id_,x,y,width,height:dict(id=id_+'-contour',closed=True,points=[
            point(id_+'-p0',x,y),point(id_+'-p1',x+width,y),point(id_+'-p2',x+width,y+height),point(id_+'-p3',x,y+height)])
        fill_rev=apply([
            dict(type='create_path',composition=comp['id'],parent='',id='mcp-fill-source',name='MCP Fill source',
                contours=[square('mcp-fill-source',20,20,80,80)]),
            dict(type='create_path',composition=comp['id'],parent='',id='mcp-fill-target',name='MCP Fill target',
                contours=[square('mcp-fill-target',120,20,80,80)]),
            dict(type='add_operation',object='mcp-fill-source',index=1,operation=fill_operation('mcp-source-fill','evenodd')),
            dict(type='add_operation',object='mcp-fill-target',index=1,operation=fill_operation('mcp-target-fill','nonzero')),
            dict(type='set_visibility',object='mcp-fill-source',visible=False)],0)
        assert core('resolve_name',name='MCP Fill target',point='',field=fill_target['field'])['result']==fill_target
        fill_rev=apply([dict(type='link_fill_rule',target=fill_target,source=fill_source,replace_driver=False)],fill_rev)
        mcp_fill=core('get',ref=fill_target)['result']
        direct_fill=desktop_api_call(endpoint,dict(identity,op='core',request=dict(op='get',ref=fill_target)))
        assert direct_fill['ok'] and direct_fill['result']==mcp_fill, (direct_fill,mcp_fill)
        assert mcp_fill['authored']==dict(literal='nonzero',driver=dict(link=fill_source)) and \
            mcp_fill['evaluated']=='evenodd' and mcp_fill['choices']==['nonzero','evenodd'] and mcp_fill['link'] is True and mcp_fill['expression'] is False, mcp_fill
        fill_metadata=next(item for item in core('properties')['result'] if item['ref']==fill_target)
        assert fill_metadata==mcp_fill
        enabled_ref=dict(object='mcp-fill-target',point='',field='op.mcp-target-fill.enabled')
        enabled_mcp=core('get',ref=enabled_ref)['result']
        enabled_direct=desktop_api_call(endpoint,dict(identity,op='core',request=dict(op='get',ref=enabled_ref)))
        assert enabled_direct['ok'] and enabled_direct['result']==enabled_mcp and \
            enabled_mcp['type']=='bool' and enabled_mcp['authored']==dict(literal=True,driver=None) and \
            enabled_mcp['evaluated'] is True and enabled_mcp['link'] is True and enabled_mcp['expression'] is False, (enabled_direct,enabled_mcp)
        fill_rev=apply([dict(type='operation_options',object='mcp-fill-source',operation='mcp-source-fill',
            composite='below',fill_rule='nonzero')],fill_rev)
        assert core('get',ref=fill_target)['result']['evaluated']=='nonzero'
        fill_plan=core('render_plan',object='mcp-fill-target')['result']
        fill_svg=core('export_svg',composition=comp['id'],artboard=comp['artboards'][0]['id'])['result']
        assert next(layer for layer in fill_plan['paint_layers'] if layer['operation']=='mcp-target-fill')['fill_rule']=='nonzero' and 'fill-rule="nonzero"' in fill_svg
        failed_fill_batch=core('apply',expected_revision=fill_rev,commands=[
            dict(type='operation_options',object='mcp-fill-target',operation='mcp-target-fill',composite='above',fill_rule='nonzero'),
            dict(type='link_fill_rule',target=fill_target,source=fill_target,replace_driver=False)])
        assert not failed_fill_batch['ok'] and failed_fill_batch['error']['code']=='DEPENDENCY_CYCLE' and failed_fill_batch['revision']==fill_rev
        fill_rev=apply([dict(type='unlink_fill_rule',target=fill_target)],fill_rev)
        assert core('get',ref=fill_target)['result']['authored']==dict(literal='nonzero',driver=None)
        fill_undo=core('undo',expected_revision=fill_rev)
        assert fill_undo['ok'] and core('get',ref=fill_target)['result']['authored']==dict(literal='nonzero',driver=dict(link=fill_source))
        fill_rev=fill_undo['revision']
        enabled_source=dict(object='mcp-fill-source',point='',field='op.mcp-source-fill.enabled')
        fill_rev=apply([
            dict(type='enable_operation',object='mcp-fill-source',operation='mcp-source-fill',enabled=False),
            dict(type='link_operation_enabled',target=enabled_ref,source=enabled_source,replace_driver=False)],fill_rev)
        linked_enabled=core('get',ref=enabled_ref)['result']
        direct_enabled=desktop_api_call(endpoint,dict(identity,op='core',request=dict(op='get',ref=enabled_ref)))
        assert direct_enabled['ok'] and direct_enabled['result']==linked_enabled
        assert linked_enabled['authored']==dict(literal=True,driver=dict(link=enabled_source)) and \
            linked_enabled['evaluated'] is False and linked_enabled['link'] is True and linked_enabled['expression'] is False
        enabled_metadata=next(item for item in core('properties')['result'] if item['ref']==enabled_ref)
        assert enabled_metadata==linked_enabled
        disabled_plan=core('render_plan',object='mcp-fill-target')['result']
        assert not any(layer['operation']=='mcp-target-fill' for layer in disabled_plan['paint_layers'])
        fill_rev=apply([dict(type='enable_operation',object='mcp-fill-source',operation='mcp-source-fill',enabled=True)],fill_rev)
        assert core('get',ref=enabled_ref)['result']['evaluated'] is True
        enabled_plan=core('render_plan',object='mcp-fill-target')['result']
        assert any(layer['operation']=='mcp-target-fill' for layer in enabled_plan['paint_layers'])
        failed_enabled=core('apply',expected_revision=fill_rev,commands=[
            dict(type='enable_operation',object='mcp-fill-target',operation='mcp-target-fill',enabled=False)])
        assert not failed_enabled['ok'] and failed_enabled['error']['code']=='DRIVEN_PROPERTY' and failed_enabled['revision']==fill_rev
        fill_rev=apply([dict(type='unlink_operation_enabled',target=enabled_ref)],fill_rev)
        assert core('get',ref=enabled_ref)['result']['authored']==dict(literal=True,driver=None)
        fill_rev=apply([dict(type='enable_operation',object='mcp-fill-source',operation='mcp-source-fill',enabled=False)],fill_rev)
        assert core('get',ref=enabled_ref)['result']['evaluated'] is True
        receipt = dict(status='PASS', seed=7821, paths=24, semantic_mutations=rev,
            mcp_initialize_list_call=True, same_live_desktop_session=True, atomic_failure=True, independent_duplication=True, geometric_alignment_undo=True, equal_gap_spacing_undo=True, editable_svg_undo=True,
            stale_session_rejected=True, native_restart=True, abnormal_exit_recovery=True,
            independent_svg_parser_paths=expected_svg_paths, ordered_stack_readback=True,
            automatic_native_and_recovery_receipts=True, recovery_op_detaches_source=True, image_lifecycle_native_recovery=True,
            typed_text_save_as=True, stale_save_identity_and_revision_rejected=True, invalid_save_as_atomic=True,
            save_as_recovery_provenance=True, save_as_destination_cold_open=True, fill_rule_link=True,
            operation_enabled_link=True, grid_bounds_width_link=True,
            gui_save_as_acceptance=False, gui_performance_claim=False)
        print(json.dumps(receipt, indent=2))
finally:
    if mcp:
        mcp.stdin.close()
        try:
            mcp.wait(timeout=5)
        except subprocess.TimeoutExpired:
            mcp.kill();mcp.wait()
    if desktop and desktop.poll() is None:
        desktop.kill();desktop.wait(timeout=5)

"""Seeded formal MCP client -> sidecar -> live Qt Session, including restart.

Uses a temporary document and offscreen desktop; this is semantic/persistence
evidence, not a viewport performance or visual quality claim.
"""
import json
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
        assert {t['name'] for t in rpc('tools/list')['result']['tools']} == {'nect_session', 'nect_command', 'nect_file'}
        live = tool('nect_session')
        identity = {key: live[key] for key in ('session_id', 'document_id')}
        comp = core('inspect')['result']['compositions'][0]
        rng = random.Random(7821)
        commands = []
        for i in range(24):
            commands.append(dict(type='create_path', composition=comp['id'], parent='', id=f'path-{i}', name=f'Motif {i}',
                contours=[dict(id=f'contour-{i}', closed=False, points=[point(f'p-{i}-0', rng.randrange(40, 850), rng.randrange(40, 570)),
                                                                     point(f'p-{i}-1', rng.randrange(40, 850), rng.randrange(40, 570))])]))
        rev = apply(commands, 0)
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
        stop_ref=dict(object='path-0',point='',field='op.motif-fill.gradient.motif-gradient.stop.start-stop.r')
        rev=apply([dict(type='set',ref=stop_ref,value=.75),dict(type='link',
            target=dict(object='path-1',point='',field='stroke.r'),
            binding=dict(source=stop_ref,scale=1,offset=0,mode='copy_local_value'))],rev)
        assert core('get',ref=dict(object='path-1',point='',field='stroke.r'))['result']['evaluated']==.75
        plan=core('render_plan',object='path-0')['result']
        assert sum('gradient' in x for x in plan['paint_layers'])==copies
        native = temp / 'scenario.nect'
        saved = tool('nect_file', dict(identity, op='save', path=str(native), expected_revision=rev))
        assert saved['ok'], saved
        expected = core('inspect')['result']
        svg = core('export_svg', composition=comp['id'], artboard=comp['artboards'][0]['id'])['result']
        assert len(ET.fromstring(svg).findall('.//{http://www.w3.org/2000/svg}path')) == expected_svg_paths
        svg_root=ET.fromstring(svg);ns='{http://www.w3.org/2000/svg}'
        gradient_defs=svg_root.findall('.//'+ns+'linearGradient')
        assert len(gradient_defs)==copies and len({x.attrib['id'] for x in gradient_defs})==copies
        assert all(x.attrib['gradientUnits']=='userSpaceOnUse' for x in gradient_defs)
        assert float(gradient_defs[0].findall(ns+'stop')[0].attrib['offset'])==0
        assert core('inspect')['result'] == expected
        assert json.loads(native.read_text(encoding='utf-8')) == expected
        assert tool('nect_file', dict(identity, op='recover', expected_revision=rev))['ok']
        recovery = temp / 'recovery' / (identity['session_id'] + '.nect')
        # Actual abnormal termination: recover the exact committed snapshot in a new process.
        desktop.kill();desktop.wait(timeout=5)
        desktop, _ = start(endpoint, temp, recovery)
        assert core('apply', expected_revision=0, commands=[dict(type='set', ref=source, value=1)])['error']['code'] == 'SESSION_CONFLICT'
        live = tool('nect_session'); identity = {key: live[key] for key in ('session_id', 'document_id')}
        assert core('inspect')['result'] == expected
        assert core('get', ref=target)['result']['evaluated'] == 167
        receipt = dict(status='PASS', seed=7821, paths=24, semantic_mutations=rev,
            mcp_initialize_list_call=True, same_live_desktop_session=True, atomic_failure=True,
            stale_session_rejected=True, native_restart=True, abnormal_exit_recovery=True,
            independent_svg_parser_paths=expected_svg_paths, ordered_stack_readback=True, gui_performance_claim=False)
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

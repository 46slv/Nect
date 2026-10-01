"""Formal MCP and direct Desktop API parity for non-owning Collections."""
import atexit
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from session_client import call as desktop_api_call

EXE = str(Path(sys.argv[1]).resolve())
desktop = mcp = None
endpoint = ''
identity = {}
sequence = 0


def rpc(method, params=None):
    global sequence
    sequence += 1
    mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', id=sequence, method=method,
                                   params=params or {})) + '\n')
    mcp.stdin.flush()
    reply = json.loads(mcp.stdout.readline())
    assert reply.get('id') == sequence and reply.get('jsonrpc') == '2.0', reply
    return reply


def tool(name, arguments=None):
    result = rpc('tools/call', dict(name=name, arguments=arguments or {}))['result']
    structured = result['structuredContent']
    assert json.loads(result['content'][0]['text']) == structured
    assert result['isError'] == (not structured.get('ok', False)), result
    return structured


def core(op, **fields):
    return tool('nect_command', dict(identity, request=dict(op=op, **fields)))


def compare(op, **fields):
    request = dict(op=op, **fields)
    via_mcp = core(op, **fields)
    via_api = desktop_api_call(endpoint, dict(identity, op='core', request=request))
    assert via_mcp == via_api, (request, via_mcp, via_api)
    return via_mcp


def start(endpoint_name, recovery, ready_path, native_path=None):
    command = [EXE, '--automation-endpoint', endpoint_name,
               '--recovery-dir', str(recovery), '--ready-file', str(ready_path)]
    if native_path:
        command.append(str(native_path))
    proc = subprocess.Popen(command, env=dict(os.environ, QT_QPA_PLATFORM='offscreen'))
    deadline = time.monotonic() + 12
    while not ready_path.exists():
        if proc.poll() is not None or time.monotonic() >= deadline:
            raise AssertionError('Desktop did not start for MCP Collection contract')
        time.sleep(.02)
    return proc


def connect(endpoint_name):
    return subprocess.Popen([sys.executable, str(ROOT / 'scripts/mcp_server.py'),
                             '--endpoint', endpoint_name], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, encoding='utf-8')


def stop(proc):
    if proc:
        if proc.stdin:
            proc.stdin.close()
        if proc.poll() is None:
            proc.terminate()
            proc.wait(timeout=5)


atexit.register(lambda: (stop(mcp), stop(desktop)))


def initialize():
    assert rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={},
                                 clientInfo=dict(name='collection-contract', version='1')))['result']['protocolVersion'] == '2025-06-18'
    mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', method='notifications/initialized')) + '\n')
    mcp.stdin.flush()


def main():
    global desktop, mcp, endpoint, identity, sequence
    with tempfile.TemporaryDirectory(prefix='nect-mcp-collection-') as directory:
        temp = Path(directory)
        native_path = temp / 'collection-cold-reopen.nect'
        endpoint = 'nect-collection-' + uuid.uuid4().hex
        desktop = start(endpoint, temp / 'recovery', temp / 'ready.json')
        mcp = connect(endpoint)
        initialize()
        description = next(item for item in rpc('tools/list')['result']['tools']
                           if item['name'] == 'nect_command')['description']
        assert 'set_collection_members' in description
        live = tool('nect_session')
        identity = {key: live[key] for key in ('session_id', 'document_id')}
        revision = live['revision']
        inspected = compare('inspect')['result']
        composition = inspected['compositions'][0]['id']
        source = copy.deepcopy(next(entry['template'] for entry in core('primitive_types')['result']
                                    if entry['type'] == 'nect.shape.rectangle'))
        source['id'] = 'mcp-collection-source'
        follow_rectangle = copy.deepcopy(source)
        follow_rectangle['id'] = 'mcp-follow-rectangle-source'
        follow_text = core('text_defaults')['result']
        follow_text.update(id='mcp-follow-text-source', content='Editable follower text')
        follow_path = [
            dict(id='mcp-follow-path-point-a', x=dict(literal=0), y=dict(literal=0),
                 in_angle=dict(literal=0), in_length=dict(literal=0), out_angle=dict(literal=0), out_length=dict(literal=0)),
            dict(id='mcp-follow-path-point-b', x=dict(literal=240), y=dict(literal=20),
                 in_angle=dict(literal=180), in_length=dict(literal=0), out_angle=dict(literal=0), out_length=dict(literal=0))]
        setup = core('apply', expected_revision=revision, commands=[
            dict(type='create_primitive', composition=composition, parent='',
                 id='mcp-collection-A', name='A', source=source),
            dict(type='create_primitive', composition=composition, parent='',
                 id='mcp-follow-rectangle', name='Follow Rectangle', source=follow_rectangle),
            dict(type='create_text', composition=composition, parent='',
                 id='mcp-follow-text', name='Follow Text', source=follow_text),
            dict(type='create_path', composition=composition, parent='', id='mcp-follow-path',
                 name='Authored Path', contours=[dict(id='mcp-follow-contour', closed=False, points=follow_path)]),
            dict(type='group_contiguous', composition=composition, parent='',
                 members=['mcp-follow-rectangle', 'mcp-follow-text'], id='mcp-follow-group', name='Follower group'),
            dict(type='create_collection', id='mcp-collection-K', name='K', members=['mcp-collection-A'])])
        assert setup['ok'], setup
        revision = setup['revision']
        relation = dict(id='mcp-follow-relation', path='mcp-follow-path', contour='mcp-follow-contour',
            start_mode='distance', start=40, normal_offset=2, reversed=False, items={
                'mcp-follow-rectangle': dict(distance=0, normal_offset=0, follow_tangent=True),
                'mcp-follow-text': dict(distance=120, normal_offset=3, follow_tangent=False)})
        attached = core('apply', expected_revision=revision, commands=[dict(type='attach_group_path_follow',
            group='mcp-follow-group', relation=relation)])
        assert attached['ok'], attached
        revision = attached['revision']
        session_after_attach = tool('nect_session')
        assert session_after_attach['revision'] == revision
        follow_readback = compare('inspect')['result']
        follow_group = next(item for item in follow_readback['objects'] if item['id'] == 'mcp-follow-group')
        assert follow_group['path_follow'] == relation
        expected = dict(id='mcp-collection-K', name='K', members=['mcp-collection-A'])
        assert compare('collections')['result'] == [expected]
        assert compare('collection', id='mcp-collection-K')['result'] == expected
        before_svg = compare('export_svg', composition=composition,
                             artboard=inspected['compositions'][0]['artboards'][0]['id'])['result']
        stale = compare('apply', expected_revision=revision - 1,
                        commands=[dict(type='rename_collection', collection='mcp-collection-K', name='Stale')])
        assert not stale['ok'] and stale['error']['code'] == 'REVISION_CONFLICT', stale
        renamed = core('apply', expected_revision=revision, commands=[
            dict(type='rename_collection', collection='mcp-collection-K', name='Renamed K'),
            dict(type='set_collection_members', collection='mcp-collection-K', members=[])])
        assert renamed['ok'], renamed
        revision = renamed['revision']
        assert compare('collection', id='mcp-collection-K')['result'] == dict(
            id='mcp-collection-K', name='Renamed K', members=[])
        assert compare('export_svg', composition=composition,
                       artboard=inspected['compositions'][0]['artboards'][0]['id'])['result'] == before_svg
        saved = tool('nect_file', dict(identity, op='save', expected_revision=revision,
                                       path=str(native_path)))
        assert saved['ok'] and native_path.exists(), saved
        stop(mcp); mcp = None
        stop(desktop); desktop = None
        endpoint = 'nect-collection-reopen-' + uuid.uuid4().hex
        desktop = start(endpoint, temp / 'reopened-recovery', temp / 'reopened-ready.json', native_path)
        mcp = connect(endpoint)
        sequence = 0
        initialize()
        live = tool('nect_session')
        identity = {key: live[key] for key in ('session_id', 'document_id')}
        assert compare('collection', id='mcp-collection-K')['result'] == dict(
            id='mcp-collection-K', name='Renamed K', members=[])
        reopened_group = next(item for item in compare('inspect')['result']['objects']
                              if item['id'] == 'mcp-follow-group')
        assert reopened_group['path_follow'] == relation
        print('PASS formal MCP Collection mutation, API parity, unchanged render and cold reopen')


if __name__ == '__main__':
    main()

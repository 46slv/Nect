"""Formal MCP contract for the first same-document Definition/Instance vertical."""
import copy
import atexit
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
sequence = 0
identity = {}
endpoint = ''


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


def direct(request):
    return desktop_api_call(endpoint, dict(identity, op='core', request=request))


def compare(op, **fields):
    request = dict(op=op, **fields)
    via_mcp, via_api = core(op, **fields), direct(request)
    assert via_mcp == via_api, (request, via_mcp, via_api)
    return via_mcp


def start(desktop_exe, endpoint_name, recovery, ready_path, native_path=None):
    command = [desktop_exe, '--automation-endpoint', endpoint_name,
        '--recovery-dir', str(recovery), '--ready-file', str(ready_path)]
    if native_path:
        command.append(str(native_path))
    proc = subprocess.Popen(command, env=dict(os.environ, QT_QPA_PLATFORM='offscreen'))
    deadline = time.monotonic() + 12
    while not ready_path.exists():
        if proc.poll() is not None or time.monotonic() >= deadline:
            raise AssertionError('Desktop did not start for MCP Definition contract')
        time.sleep(.02)
    return proc


def connect(endpoint_name):
    return subprocess.Popen([sys.executable, str(ROOT / 'scripts/mcp_server.py'),
        '--endpoint', endpoint_name], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, text=True, encoding='utf-8')


def stop(proc):
    if proc:
        if proc.stdin:
            proc.stdin.close()
        if proc.poll() is None:
            proc.terminate()
            proc.wait(timeout=5)


atexit.register(lambda: (stop(mcp), stop(desktop)))


def main():
    global desktop, mcp, identity, endpoint, sequence
    with tempfile.TemporaryDirectory(prefix='nect-mcp-definition-') as directory:
        temp = Path(directory)
        endpoint = 'nect-definition-' + uuid.uuid4().hex
        native_path = temp / 'definition-cold-reopen.nect'
        desktop = start(EXE, endpoint, temp / 'recovery', temp / 'ready.json')
        mcp = connect(endpoint)
        assert rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={},
            clientInfo=dict(name='definition-contract', version='1')))['result']['protocolVersion'] == '2025-06-18'
        mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', method='notifications/initialized')) + '\n')
        mcp.stdin.flush()
        tools = rpc('tools/list')['result']['tools']
        description = next(item for item in tools if item['name'] == 'nect_command')['description']
        assert 'Definition/Instance v1' in description and 'detach_instance' in description

        live = tool('nect_session')
        identity = {key: live[key] for key in ('session_id', 'document_id')}
        revision = live['revision']
        composition = compare('inspect')['result']['compositions'][0]['id']
        primitive_templates = {entry['type']: entry['template'] for entry in core('primitive_types')['result']}
        path_source = copy.deepcopy(primitive_templates['nect.shape.rectangle'])
        path_source['id'] = 'mcp-r04-path-source'
        text_source = core('text_defaults')['result']
        text_source['id'] = 'mcp-r04-text-source'
        text_source['content'] = 'Definition source text'
        setup = core('apply', expected_revision=revision, commands=[
            dict(type='create_primitive', composition=composition, parent='', id='mcp-r04-path',
                name='A', source=path_source),
            dict(type='create_text', composition=composition, parent='', id='mcp-r04-text',
                name='B', source=text_source)])
        assert setup['ok'], setup
        revision = setup['revision']
        grouped = core('apply', expected_revision=revision, commands=[dict(type='group_contiguous',
            composition=composition, parent='', members=['mcp-r04-path', 'mcp-r04-text'],
            id='mcp-r04-root', name='D')])
        assert grouped['ok'], grouped
        revision = grouped['revision']
        placed = core('apply', expected_revision=revision, commands=[
            dict(type='create_definition', id='mcp-r04-definition', name='Definition D', root='mcp-r04-root'),
            dict(type='create_instance', composition=composition, parent='', id='mcp-r04-i1',
                definition='mcp-r04-definition', name='I1'),
            dict(type='create_instance', composition=composition, parent='', id='mcp-r04-i2',
                definition='mcp-r04-definition', name='I2'),
            dict(type='set', ref=dict(object='mcp-r04-i1', point='', field='transform.tx'), value=35),
            dict(type='set', ref=dict(object='mcp-r04-i2', point='', field='transform.tx'), value=125),
            dict(type='set_instance_override', instance='mcp-r04-i1',
                target=dict(object='mcp-r04-path', point='', field='composite.opacity'), value=.25),
            dict(type='set_instance_override', instance='mcp-r04-i2',
                target=dict(object='mcp-r04-text', point='', field='text.font_size'), value=36)])
        assert placed['ok'], placed
        revision = placed['revision']
        assert [item['id'] for item in compare('definitions')['result']] == ['mcp-r04-definition']
        definition = compare('definition', id='mcp-r04-definition')['result']
        assert definition == dict(id='mcp-r04-definition', name='Definition D', root='mcp-r04-root')
        inspected = compare('inspect')['result']
        instances = {item['id']: item for item in inspected['objects'] if item['id'] in ('mcp-r04-i1', 'mcp-r04-i2')}
        assert instances['mcp-r04-i1']['instance']['overrides'][0]['target']['field'] == 'composite.opacity'
        assert instances['mcp-r04-i2']['instance']['overrides'][0]['target']['field'] == 'text.font_size'
        exported = compare('export_svg', composition=composition, artboard=inspected['compositions'][0]['artboards'][0]['id'])['result']
        assert '<svg' in exported and 'mcp-r04-i1' in exported and 'mcp-r04-i2' in exported

        stale = dict(op='apply', expected_revision=revision - 1, commands=[dict(type='rename_definition',
            definition='mcp-r04-definition', name='Stale')])
        stale_result = core('apply', expected_revision=revision - 1, commands=stale['commands'])
        assert stale_result == direct(stale) and not stale_result['ok']
        renamed = core('apply', expected_revision=revision, commands=[dict(type='rename_definition',
            definition='mcp-r04-definition', name='Renamed D')])
        assert renamed['ok'], renamed
        revision = renamed['revision']
        assert compare('definition', id='mcp-r04-definition')['result']['name'] == 'Renamed D'

        saved = tool('nect_file', dict(identity, op='save', expected_revision=revision, path=str(native_path)))
        assert saved['ok'] and native_path.exists(), saved
        stop(mcp); mcp = None
        stop(desktop); desktop = None
        endpoint = 'nect-definition-reopen-' + uuid.uuid4().hex
        desktop = start(EXE, endpoint, temp / 'reopened-recovery', temp / 'reopened-ready.json', native_path)
        mcp = connect(endpoint); sequence = 0
        assert rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={},
            clientInfo=dict(name='definition-cold-reopen', version='1')))['result']['protocolVersion'] == '2025-06-18'
        mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', method='notifications/initialized')) + '\n')
        mcp.stdin.flush()
        live = tool('nect_session');identity = {key: live[key] for key in ('session_id', 'document_id')}
        reopened = compare('inspect')['result']
        assert any(item['id'] == 'mcp-r04-definition' for item in compare('definitions')['result'])
        reopened_instances = {item['id']: item for item in reopened['objects'] if item['id'] in ('mcp-r04-i1', 'mcp-r04-i2')}
        assert reopened_instances['mcp-r04-i1']['instance']['definition'] == 'mcp-r04-definition'
        assert len(reopened_instances['mcp-r04-i1']['instance']['overrides']) == 1
        assert len(reopened_instances['mcp-r04-i2']['instance']['overrides']) == 1
        print('PASS formal MCP Definition/Instance identity, mutation, export, and cold reopen contract')


if __name__ == '__main__':
    main()

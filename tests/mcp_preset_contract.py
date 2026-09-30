"""Focused live MCP contract for document-local PresetDefinition v1."""
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
desktop = None
mcp = None
sequence = 0


def ordinary_stack_operations(obj):
    operations = []
    for entry in obj['stack']:
        if entry.get('kind') == 'operation':
            operations.append(entry['operation'])
        elif entry.get('kind') == 'macro':
            continue
        elif 'kind' in entry:
            raise AssertionError('Unknown native processing entry kind')
        else:
            operations.append(entry)
    return operations


def rpc(method, params=None):
    global sequence
    sequence += 1
    message = dict(jsonrpc='2.0', id=sequence, method=method)
    if params is not None:
        message['params'] = params
    mcp.stdin.write(json.dumps(message) + '\n')
    mcp.stdin.flush()
    reply = json.loads(mcp.stdout.readline())
    assert reply.get('id') == sequence and reply.get('jsonrpc') == '2.0', reply
    return reply


identity = {}
endpoint = ''


def tool(name, arguments=None):
    reply = rpc('tools/call', dict(name=name, arguments=arguments or {}))
    assert 'result' in reply, reply
    result = reply['result']
    structured = result['structuredContent']
    assert json.loads(result['content'][0]['text']) == structured
    assert result['isError'] == (not structured.get('ok', False)), result
    return structured


def core(op, **fields):
    return tool('nect_command', dict(identity, request=dict(op=op, **fields)))


def direct_core(request):
    return desktop_api_call(endpoint, dict(identity, op='core', request=request))


def compare_read(op, **fields):
    via_mcp = core(op, **fields)
    direct = direct_core(dict(op=op, **fields))
    assert via_mcp == direct, (op, via_mcp, direct)
    return via_mcp


def main():
    global desktop, mcp, identity, endpoint, sequence
    with tempfile.TemporaryDirectory(prefix='nect-mcp-preset-') as directory:
        temp = Path(directory)
        endpoint = 'nect-preset-test-' + uuid.uuid4().hex
        ready = temp / 'ready.json'
        desktop = subprocess.Popen([EXE, '--automation-endpoint', endpoint,
            '--recovery-dir', str(temp / 'recovery'), '--ready-file', str(ready)],
            env=dict(os.environ, QT_QPA_PLATFORM='offscreen'))
        deadline = time.monotonic() + 10
        while not ready.exists():
            if desktop.poll() is not None or time.monotonic() >= deadline:
                raise AssertionError('Desktop did not start')
            time.sleep(.02)

        mcp = subprocess.Popen([sys.executable, str(ROOT / 'scripts/mcp_server.py'),
            '--endpoint', endpoint], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, encoding='utf-8')
        assert rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={},
            clientInfo=dict(name='preset-contract', version='1')))['result']['protocolVersion'] == '2025-06-18'
        mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', method='notifications/initialized')) + '\n')
        mcp.stdin.flush()
        tools = rpc('tools/list')['result']['tools']
        command_tool = next(item for item in tools if item['name'] == 'nect_command')
        description = command_tool['description']
        assert 'PresetDefinition v1' in description and 'captured_source_operations' in description
        assert 'PRESET_NONPORTABLE_SOURCE' in description

        live = tool('nect_session')
        identity = {key: live[key] for key in ('session_id', 'document_id')}
        composition = core('inspect')['result']['compositions'][0]['id']
        primitive_templates = {item['type']: item['template'] for item in core('primitive_types')['result']}
        source_template = copy.deepcopy(primitive_templates['nect.shape.rectangle'])
        target_template = copy.deepcopy(primitive_templates['nect.shape.rectangle'])
        source_template['id'] = 'mcp-preset-source-generator'
        target_template['id'] = 'mcp-preset-target-generator'
        revision = live['revision']
        created = core('apply', expected_revision=revision, commands=[
            dict(type='create_primitive', composition=composition, parent='', id='mcp-preset-source',
                name='Preset source', source=source_template),
            dict(type='create_primitive', composition=composition, parent='', id='mcp-preset-target',
                name='Preset target', source=target_template)])
        assert created['ok'], created
        revision = created['revision']

        operation_templates = {item['type']: item['template'] for item in core('operator_types')['result']}
        offset = copy.deepcopy(operation_templates['nect.shape.offset'])
        offset['id'] = 'mcp-preset-source-offset'
        offset['parameters']['amount']['literal'] = 18
        repeater = copy.deepcopy(operation_templates['nect.shape.repeater'])
        repeater['id'] = 'mcp-preset-source-repeater'
        repeater['parameters']['copies']['literal'] = 4
        repeater['parameters']['position_x']['literal'] = 36
        source_stack = ordinary_stack_operations(next(obj for obj in core('inspect')['result']['objects']
            if obj['id'] == 'mcp-preset-source'))
        changed = core('apply', expected_revision=revision, commands=[
            dict(type='add_operation', object='mcp-preset-source', index=len(source_stack), operation=offset),
            dict(type='add_operation', object='mcp-preset-source', index=len(source_stack) + 1, operation=repeater)])
        assert changed['ok'], changed
        revision = changed['revision']

        capture_request = dict(op='apply', expected_revision=revision, commands=[dict(
            type='create_preset_from_stack', id='mcp-preset-captured', object='mcp-preset-source',
            label='Captured Pair', category='Shape', tags=['offset', 'repeat'])])
        captured = core('apply', expected_revision=revision, commands=capture_request['commands'])
        assert captured['ok'] and captured['result']['captured_source_operations'] == [
            dict(id='mcp-preset-source-offset', type='nect.shape.offset'),
            dict(id='mcp-preset-source-repeater', type='nect.shape.repeater')], captured
        revision = captured['revision']
        assert compare_read('presets')['result'][0]['id'] == 'mcp-preset-captured'
        definition = compare_read('preset', id='mcp-preset-captured')['result']
        assert [entry['type'] for entry in definition['entries']] == [
            'nect.shape.offset', 'nect.shape.repeater']
        assert definition['entries'][0]['parameters']['amount'] == 18
        assert definition['entries'][1]['parameters']['position_x'] == 36

        # Exercise explicit literal creation through the same formal MCP tool as well.
        explicit = copy.deepcopy(definition)
        explicit['id'] = 'mcp-preset-explicit'
        explicit['label'] = 'Explicit Pair'
        created_explicit = core('apply', expected_revision=revision, commands=[
            dict(type='create_preset', definition=explicit)])
        assert created_explicit['ok'], created_explicit
        revision = created_explicit['revision']
        assert compare_read('preset', id='mcp-preset-explicit')['result'] == explicit

        apply_request = dict(op='apply', expected_revision=revision, commands=[dict(
            type='apply_preset', preset='mcp-preset-captured', object='mcp-preset-target',
            operation_id_prefix='mcp-use')])
        applied = core('apply', expected_revision=revision, commands=apply_request['commands'])
        assert applied['ok'] and applied['result']['applied_presets'][0]['operation_ids'] == [
            'mcp-use-op-1', 'mcp-use-op-2'], applied
        revision = applied['revision']
        target_stack = ordinary_stack_operations(next(obj for obj in compare_read('inspect')['result']['objects']
            if obj['id'] == 'mcp-preset-target'))
        applied_pair = [entry for entry in target_stack if entry['id'] in ('mcp-use-op-1', 'mcp-use-op-2')]
        assert [entry['type'] for entry in applied_pair] == ['nect.shape.offset', 'nect.shape.repeater']
        assert applied_pair[0]['parameters']['amount']['literal'] == 18
        assert applied_pair[1]['parameters']['position_x']['literal'] == 36
        apply_undo = core('undo', expected_revision=revision)
        assert apply_undo['ok'], apply_undo
        revision = apply_undo['revision']
        target_stack = ordinary_stack_operations(next(obj for obj in compare_read('inspect')['result']['objects']
            if obj['id'] == 'mcp-preset-target'))
        assert not any(entry['id'] in ('mcp-use-op-1', 'mcp-use-op-2') for entry in target_stack)
        apply_redo = core('redo', expected_revision=revision)
        assert apply_redo['ok'], apply_redo
        revision = apply_redo['revision']
        target_stack = ordinary_stack_operations(next(obj for obj in compare_read('inspect')['result']['objects']
            if obj['id'] == 'mcp-preset-target'))
        assert {'mcp-use-op-1', 'mcp-use-op-2'}.issubset({entry['id'] for entry in target_stack})

        definition = compare_read('preset', id='mcp-preset-captured')['result']
        definition['entries'][0]['parameters']['amount'] = 7
        updated = core('apply', expected_revision=revision, commands=[dict(type='update_preset', definition=definition)])
        assert updated['ok'], updated
        revision = updated['revision']
        assert compare_read('preset', id='mcp-preset-captured')['result']['entries'][0]['parameters']['amount'] == 7
        target_stack = ordinary_stack_operations(next(obj for obj in compare_read('inspect')['result']['objects']
            if obj['id'] == 'mcp-preset-target'))
        assert next(entry for entry in target_stack if entry['id'] == 'mcp-use-op-1')['parameters']['amount']['literal'] == 18

        renamed = core('apply', expected_revision=revision, commands=[dict(
            type='rename_preset', preset='mcp-preset-captured', label='Renamed Pair')])
        assert renamed['ok'], renamed
        revision = renamed['revision']
        assert compare_read('preset', id='mcp-preset-captured')['result']['label'] == 'Renamed Pair'

        stale_request = dict(op='apply', expected_revision=revision - 1, commands=[dict(
            type='apply_preset', preset='mcp-preset-captured', object='mcp-preset-target',
            operation_id_prefix='stale-use')])
        direct_stale = direct_core(stale_request)
        mcp_stale = core('apply', expected_revision=revision - 1, commands=stale_request['commands'])
        assert mcp_stale == direct_stale and not mcp_stale['ok'] and mcp_stale['revision'] == revision, mcp_stale
        assert mcp_stale['error']['code'] in ('REVISION_CONFLICT', 'STALE_REVISION')

        # An enabled link on a captured operation must preserve the precise source Ref in MCP errors.
        source_stack = ordinary_stack_operations(next(obj for obj in core('inspect')['result']['objects']
            if obj['id'] == 'mcp-preset-source'))
        source_stroke = next(op['id'] for op in source_stack if op['type'] == 'nect.paint.stroke')
        enabled_target = dict(object='mcp-preset-source', point='', field='op.mcp-preset-source-offset.enabled')
        enabled_source = dict(object='mcp-preset-source', point='', field='op.' + source_stroke + '.enabled')
        linked = core('apply', expected_revision=revision, commands=[dict(type='link_operation_enabled',
            target=enabled_target, source=enabled_source, replace_driver=False)])
        assert linked['ok'], linked
        revision = linked['revision']
        refusal_request = dict(op='apply', expected_revision=revision, commands=[dict(
            type='create_preset_from_stack', id='mcp-preset-refused', object='mcp-preset-source',
            label='Should Refuse', category='Shape', tags=[])])
        direct_refusal = direct_core(refusal_request)
        mcp_refusal = core('apply', expected_revision=revision, commands=refusal_request['commands'])
        expected_ref = dict(object='mcp-preset-source', point='', field='op.mcp-preset-source-offset.enabled')
        assert mcp_refusal == direct_refusal and not mcp_refusal['ok'] and \
            mcp_refusal['error']['code'] == 'PRESET_NONPORTABLE_SOURCE' and \
            mcp_refusal['error']['references'] == [expected_ref], mcp_refusal

        removed = core('apply', expected_revision=revision, commands=[dict(
            type='delete_preset', preset='mcp-preset-explicit')])
        assert removed['ok'], removed
        revision = removed['revision']
        assert compare_read('presets')['result'] and all(item['id'] != 'mcp-preset-explicit'
            for item in compare_read('presets')['result'] )
        undone = core('undo', expected_revision=revision)
        assert undone['ok'], undone
        revision = undone['revision']
        assert compare_read('preset', id='mcp-preset-explicit')['result']['label'] == 'Explicit Pair'

        native_path = temp / 'preset-cold-reopen.nect'
        saved = tool('nect_file', dict(identity, op='save', expected_revision=revision, path=str(native_path)))
        assert saved['ok'] and native_path.exists(), saved
        mcp.stdin.close()
        mcp.wait(timeout=5)
        mcp = None
        desktop.kill()
        desktop.wait(timeout=5)
        endpoint = 'nect-preset-reopen-' + uuid.uuid4().hex
        ready = temp / 'reopened-ready.json'
        desktop = subprocess.Popen([EXE, '--automation-endpoint', endpoint,
            '--recovery-dir', str(temp / 'reopened-recovery'), '--ready-file', str(ready), str(native_path)],
            env=dict(os.environ, QT_QPA_PLATFORM='offscreen'))
        deadline = time.monotonic() + 10
        while not ready.exists():
            if desktop.poll() is not None or time.monotonic() >= deadline:
                raise AssertionError('Desktop did not cold-open the saved preset document')
            time.sleep(.02)
        mcp = subprocess.Popen([sys.executable, str(ROOT / 'scripts/mcp_server.py'),
            '--endpoint', endpoint], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, encoding='utf-8')
        sequence = 0
        assert rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={},
            clientInfo=dict(name='preset-cold-reopen', version='1')))['result']['protocolVersion'] == '2025-06-18'
        mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', method='notifications/initialized')) + '\n')
        mcp.stdin.flush()
        live_after_reopen = tool('nect_session')
        identity = {key: live_after_reopen[key] for key in ('session_id', 'document_id')}
        cold_document = compare_read('inspect')['result']
        assert cold_document['version'] == '0.65'
        cold_presets = {preset['id']: preset for preset in cold_document['presets']}
        assert cold_presets['mcp-preset-captured']['entries'][0]['parameters']['amount'] == 7
        assert cold_presets['mcp-preset-explicit']['label'] == 'Explicit Pair'
        cold_target = ordinary_stack_operations(next(obj for obj in cold_document['objects']
            if obj['id'] == 'mcp-preset-target'))
        assert {'mcp-use-op-1', 'mcp-use-op-2'}.issubset({entry['id'] for entry in cold_target})

        print(json.dumps(dict(status='PASS', revision=revision, capture_ids=[
            'mcp-preset-source-offset', 'mcp-preset-source-repeater'],
            applied_ids=['mcp-use-op-1', 'mcp-use-op-2'], direct_api_readback=True,
            stale_revision_atomic=True, exact_nonportable_ref=True,
            create_update_rename_delete_undo=True, native_0_65_cold_reopen=True), indent=2))


try:
    main()
finally:
    if mcp:
        mcp.stdin.close()
        try:
            mcp.wait(timeout=5)
        except subprocess.TimeoutExpired:
            mcp.kill()
            mcp.wait()
    if desktop and desktop.poll() is None:
        desktop.kill()
        desktop.wait(timeout=5)

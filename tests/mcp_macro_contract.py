"""Formal MCP/JSON-lines parity contract for the Macro first vertical."""
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
desktop = desktop_parity = mcp = None
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


def stop(proc):
    if proc:
        if proc.stdin:
            proc.stdin.close()
        if proc.poll() is None:
            proc.terminate()
            proc.wait(timeout=5)


atexit.register(lambda: (stop(mcp), stop(desktop), stop(desktop_parity)))


def main():
    global desktop, desktop_parity, mcp, identity, endpoint, sequence
    with tempfile.TemporaryDirectory(prefix='nect-mcp-macro-') as directory:
        temp = Path(directory)
        endpoint = 'nect-macro-' + uuid.uuid4().hex
        ready = temp / 'ready.json'
        desktop = subprocess.Popen([EXE, '--automation-endpoint', endpoint,
            '--recovery-dir', str(temp / 'recovery'), '--ready-file', str(ready)],
            env=dict(os.environ, QT_QPA_PLATFORM='offscreen'))
        deadline = time.monotonic() + 12
        while not ready.exists():
            if desktop.poll() is not None or time.monotonic() >= deadline:
                raise AssertionError('Desktop did not start for MCP Macro contract')
            time.sleep(.02)

        mcp = subprocess.Popen([sys.executable, str(ROOT / 'scripts/mcp_server.py'),
            '--endpoint', endpoint], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, encoding='utf-8')
        assert rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={},
            clientInfo=dict(name='macro-contract', version='1')))['result']['protocolVersion'] == '2025-06-18'
        mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', method='notifications/initialized')) + '\n')
        mcp.stdin.flush()
        description = next(item for item in rpc('tools/list')['result']['tools']
            if item['name'] == 'nect_command')['description']
        for term in ('Macro v1', 'macro.offset.amount', 'macros', 'detach_macro_instance',
                'update_macro_instance', 'pinned revision', 'import_apply_macro',
                'accepted asset revision', 'stable PublicParamID'):
            assert term in description, term

        live = tool('nect_session')
        identity = {key: live[key] for key in ('session_id', 'document_id')}
        revision = live['revision']
        composition = compare('inspect')['result']['compositions'][0]['id']
        primitive_templates = {entry['type']: entry['template'] for entry in core('primitive_types')['result']}
        source = copy.deepcopy(primitive_templates['nect.shape.rectangle'])
        source['id'] = 'mcp-macro-rectangle-template'
        operation_templates = {entry['type']: entry['template'] for entry in core('operator_types')['result']}
        offset = copy.deepcopy(operation_templates['nect.shape.offset'])
        offset['id'] = 'mcp-macro-ordinary-offset'
        offset['parameters']['amount']['literal'] = 3
        setup = core('apply', expected_revision=revision, commands=[
            dict(type='create_primitive', composition=composition, parent='', id='mcp-macro-path',
                name='Macro target', source=source),
            dict(type='add_operation', object='mcp-macro-path', operation=offset, index=0)])
        assert setup['ok'], setup
        revision = setup['revision']

        node_offset = copy.deepcopy(operation_templates['nect.shape.offset'])
        node_offset['id'] = 'mcp-macro-node-offset'
        node_offset['parameters']['amount']['literal'] = 8
        node_repeater = copy.deepcopy(operation_templates['nect.shape.repeater'])
        node_repeater['id'] = 'mcp-macro-node-repeater'
        node_repeater['parameters']['copies']['literal'] = 2
        node_repeater['parameters']['position_x']['literal'] = 120
        graph = dict(revision=1,
            input=dict(id='mcp-macro-input', domain='local_paths_and_paint'),
            output=dict(id='mcp-macro-output', domain='local_paths_and_paint'),
            nodes=[dict(operation=node_offset, input_port='mcp-macro-offset-in', output_port='mcp-macro-offset-out'),
                dict(operation=node_repeater, input_port='mcp-macro-repeater-in', output_port='mcp-macro-repeater-out')],
            edges=[
                dict(from_=dict(node='', port='mcp-macro-input'), to=dict(node='mcp-macro-node-offset', port='mcp-macro-offset-in')),
                dict(from_=dict(node='mcp-macro-node-offset', port='mcp-macro-offset-out'), to=dict(node='mcp-macro-node-repeater', port='mcp-macro-repeater-in')),
                dict(from_=dict(node='mcp-macro-node-repeater', port='mcp-macro-repeater-out'), to=dict(node='', port='mcp-macro-output'))],
            output_mapping=dict(node='mcp-macro-node-repeater', port='mcp-macro-repeater-out'),
            public_parameters=[dict(id='macro.offset.amount', label='Amount', node='mcp-macro-node-offset',
                parameter='amount', value_type='number', unit='du', domain='local_paths_and_paint')])
        graph['edges'] = [{'from': edge['from_'], 'to': edge['to']} for edge in graph['edges']]
        definition = dict(id='mcp-offset-repeat', label='MCP Offset Repeat', latest_revision=1, revisions=[graph])
        created = core('apply', expected_revision=revision,
            commands=[dict(type='create_macro_definition', definition=definition)])
        assert created['ok'], created
        revision = created['revision']
        assert compare('macros')['result'] == [definition]
        assert compare('macro', id='mcp-offset-repeat')['result'] == definition

        instantiated = core('apply', expected_revision=revision, commands=[dict(type='instantiate_macro',
            object='mcp-macro-path', definition='mcp-offset-repeat', instance='mcp-macro-instance', revision=1, index=1)])
        assert instantiated['ok'], instantiated
        revision = instantiated['revision']
        amount_ref = dict(object='mcp-macro-path', point='mcp-macro-instance', field='macro.offset.amount')
        listed = compare('properties')['result']
        amount_property = next(item for item in listed if item['ref'] == amount_ref)
        assert amount_property['unit'] == 'du' and amount_property['authored'] == 8
        amount = compare('get', ref=amount_ref)['result']
        assert amount['ref'] == amount_ref and amount['evaluated'] == 8

        stale = core('apply', expected_revision=revision - 1, commands=[dict(type='set_macro_override',
            object='mcp-macro-path', instance='mcp-macro-instance',
            public_parameter='macro.offset.amount', value=31)])
        direct_stale = direct(dict(op='apply', expected_revision=revision - 1, commands=[dict(
            type='set_macro_override', object='mcp-macro-path', instance='mcp-macro-instance',
            public_parameter='macro.offset.amount', value=31)]))
        assert stale == direct_stale and not stale['ok'] and stale['revision'] == revision, stale

        changed = core('apply', expected_revision=revision, commands=[dict(type='set_macro_override',
            object='mcp-macro-path', instance='mcp-macro-instance',
            public_parameter='macro.offset.amount', value=31)])
        assert changed['ok'], changed
        revision = changed['revision']
        amount = compare('get', ref=amount_ref)['result']
        assert amount['ref'] == amount_ref and amount['authored'] == amount['evaluated'] == 31
        history = core('undo', expected_revision=revision)
        assert history['ok'], history
        revision = history['revision']
        assert compare('get', ref=amount_ref)['result']['evaluated'] == 8
        history = core('redo', expected_revision=revision)
        assert history['ok'], history
        revision = history['revision']
        assert compare('get', ref=amount_ref)['result']['evaluated'] == 31

        stack = next(item['stack'] for item in compare('inspect')['result']['objects']
            if item['id'] == 'mcp-macro-path')
        assert [entry['kind'] for entry in stack] == ['operation', 'macro', 'operation'], stack
        pinned = stack[1]
        assert pinned['definition'] == 'mcp-offset-repeat' and pinned['revision'] == 1
        stroke_id = stack[2]['operation']['id']
        reordered = core('apply', expected_revision=revision, commands=[dict(type='reorder_operations',
            object='mcp-macro-path', order=['mcp-macro-instance', 'mcp-macro-ordinary-offset', stroke_id])])
        assert reordered['ok'], reordered
        revision = reordered['revision']
        stack = next(item['stack'] for item in compare('inspect')['result']['objects']
            if item['id'] == 'mcp-macro-path')
        assert [entry.get('id', entry.get('operation', {}).get('id')) for entry in stack] == [
            'mcp-macro-instance', 'mcp-macro-ordinary-offset', stroke_id]
        disabled = core('apply', expected_revision=revision, commands=[dict(type='enable_operation',
            object='mcp-macro-path', operation='mcp-macro-instance', enabled=False)])
        assert disabled['ok'], disabled
        revision = disabled['revision']
        stack = next(item['stack'] for item in compare('inspect')['result']['objects']
            if item['id'] == 'mcp-macro-path')
        assert stack[0]['kind'] == 'macro' and stack[0]['enabled'] is False
        enabled = core('apply', expected_revision=revision, commands=[dict(type='enable_operation',
            object='mcp-macro-path', operation='mcp-macro-instance', enabled=True)])
        assert enabled['ok'], enabled
        revision = enabled['revision']

        updated_graph = copy.deepcopy(graph)
        updated_graph['revision'] = 2
        updated_graph['nodes'][1]['operation']['parameters']['copies']['literal'] = 3
        definition_v2 = core('apply', expected_revision=revision, commands=[dict(type='update_macro_definition',
            definition='mcp-offset-repeat', revision=updated_graph)])
        assert definition_v2['ok'], definition_v2
        revision = definition_v2['revision']
        assert compare('macro', id='mcp-offset-repeat')['result']['latest_revision'] == 2
        stack = next(item['stack'] for item in compare('inspect')['result']['objects']
            if item['id'] == 'mcp-macro-path')
        assert stack[0]['revision'] == 1
        migrated = core('apply', expected_revision=revision, commands=[dict(type='update_macro_instance',
            object='mcp-macro-path', instance='mcp-macro-instance', revision=2)])
        assert migrated['ok'], migrated
        revision = migrated['revision']
        stack = next(item['stack'] for item in compare('inspect')['result']['objects']
            if item['id'] == 'mcp-macro-path')
        assert stack[0]['revision'] == 2
        assert compare('get', ref=amount_ref)['result']['evaluated'] == 31

        detached = core('apply', expected_revision=revision, commands=[dict(type='detach_macro_instance',
            object='mcp-macro-path', instance='mcp-macro-instance', operation_id_prefix='mcp-detach')])
        assert detached['ok'], detached
        revision = detached['revision']
        stack = next(item['stack'] for item in compare('inspect')['result']['objects']
            if item['id'] == 'mcp-macro-path')
        assert [entry['kind'] for entry in stack] == ['operation', 'operation', 'operation', 'operation']
        operations = [entry['operation'] for entry in stack]
        assert [item['type'] for item in operations] == [
            'nect.shape.offset', 'nect.shape.repeater', 'nect.shape.offset', 'nect.paint.stroke']
        assert [item['id'] for item in operations[:2]] == ['mcp-detach-detached-1', 'mcp-detach-detached-2']
        assert operations[0]['parameters']['amount']['literal'] == 31

        retained_source = dict(id=definition['id'], label=definition['label'], latest_revision=2,
            revisions=[copy.deepcopy(graph), copy.deepcopy(updated_graph)])
        import_command = dict(type='import_apply_macro', definition=retained_source,
            definition_id='mcp-library-copy', object='mcp-macro-path', instance='mcp-library-instance',
            pinned_revision=1, index=0, asset_id='mcp-workspace-asset', accepted_revision=7)
        mcp_import = core('apply', expected_revision=revision, commands=[import_command])
        assert mcp_import['ok'] and mcp_import['result']['changed'], mcp_import
        revision = mcp_import['revision']
        receipt = mcp_import['result']['applied_library_macros'][0]
        assert receipt['accepted_revision'] == 7 and receipt['pinned_revision'] == 1
        assert receipt['source_definition_id'] == definition['id'] and receipt['definition_id'] == 'mcp-library-copy'
        assert receipt['retained_revisions'] == [1, 2] and receipt['overrides'] == {}
        assert receipt['definition_present_after_batch'] and receipt['instance_present_after_batch']

        endpoint_parity = 'nect-macro-parity-' + uuid.uuid4().hex
        ready_parity = temp / 'ready-parity.json'
        desktop_parity = subprocess.Popen([EXE, '--automation-endpoint', endpoint_parity,
            '--recovery-dir', str(temp / 'recovery-parity'), '--ready-file', str(ready_parity)],
            env=dict(os.environ, QT_QPA_PLATFORM='offscreen'))
        deadline = time.monotonic() + 12
        while not ready_parity.exists():
            if desktop_parity.poll() is not None or time.monotonic() >= deadline:
                raise AssertionError('Second Desktop did not start for successful JSON-lines/MCP parity')
            time.sleep(.02)
        hello = desktop_api_call(endpoint_parity, dict(op='hello'))
        assert hello['ok'], hello
        identity_parity = {key: hello[key] for key in ('session_id', 'document_id')}
        def direct_parity(request):
            return desktop_api_call(endpoint_parity, dict(identity_parity, op='core', request=request))
        inspect_parity = direct_parity(dict(op='inspect'))
        composition_parity = inspect_parity['result']['compositions'][0]['id']
        rectangle_parity = copy.deepcopy(primitive_templates['nect.shape.rectangle'])
        rectangle_parity['id'] = 'mcp-parity-rectangle-template'
        setup_parity = direct_parity(dict(op='apply', expected_revision=hello['revision'], commands=[
            dict(type='create_primitive', composition=composition_parity, parent='', id='mcp-macro-path',
                name='Macro target', source=rectangle_parity)]))
        assert setup_parity['ok'], setup_parity
        api_import = direct_parity(dict(op='apply', expected_revision=setup_parity['revision'], commands=[import_command]))
        assert api_import['ok'] and api_import['result'] == mcp_import['result'], (api_import, mcp_import)

        rejected_batch = [dict(import_command,
            definition_id='mcp-rollback-definition', instance='mcp-rollback-instance'),
            dict(type='delete_macro_definition', definition='mcp-intentionally-missing')]
        mcp_rollback = core('apply', expected_revision=revision, commands=rejected_batch)
        api_rollback = direct_parity(dict(op='apply', expected_revision=api_import['revision'], commands=rejected_batch))
        assert not mcp_rollback['ok'] and not api_rollback['ok']
        assert mcp_rollback['error'] == api_rollback['error']
        assert mcp_rollback['error']['code'] == 'MISSING_MACRO_DEFINITION'
        assert mcp_rollback['revision'] == revision and api_rollback['revision'] == api_import['revision']

        malformed_command = dict(import_command, definition_id='mcp-malformed-definition',
            instance='mcp-malformed-instance', overrides=[31])
        mcp_malformed = core('apply', expected_revision=revision, commands=[malformed_command])
        api_malformed = direct_parity(dict(op='apply', expected_revision=api_import['revision'], commands=[malformed_command]))
        assert not mcp_malformed['ok'] and not api_malformed['ok']
        assert mcp_malformed['error'] == api_malformed['error']
        assert mcp_malformed['error']['code'] == 'INVALID_MACRO_OVERRIDES'
        stop(mcp); mcp = None
        stop(desktop_parity); desktop_parity = None
        stop(desktop); desktop = None
        print('PASS formal MCP/JSON-lines Macro import receipts, all retained graph revisions, explicit pin, strict overrides, batch rollback and prior lifecycle parity')


if __name__ == '__main__':
    main()

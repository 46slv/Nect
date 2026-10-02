"""Bounded font-authoring parity through real MCP and the desktop-owned Session.

The native Text fixture deliberately bypasses CreateText's projection-dependent
anchor initialization. This proves portable authoring, not Windows shaping or UI.
Run with a nect_desktop executable, or --discovery-only without a desktop build.
"""
import argparse
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

CHECKS = 0


def check(condition, detail):
    global CHECKS
    if not condition:
        raise AssertionError(detail)
    CHECKS += 1


def stop(proc):
    if proc is not None:
        if proc.stdin:
            proc.stdin.close()
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=5)


class Client:
    def __init__(self, endpoint):
        self.endpoint, self.identity, self.sequence = endpoint, {}, 0
        self.proc = subprocess.Popen([sys.executable, str(ROOT / 'scripts/mcp_server.py'),
            '--endpoint', endpoint], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, encoding='utf-8')

    def rpc(self, method, params=None):
        self.sequence += 1
        self.proc.stdin.write(json.dumps(dict(jsonrpc='2.0', id=self.sequence,
            method=method, params=params or {}), allow_nan=False) + '\n')
        self.proc.stdin.flush()
        line = self.proc.stdout.readline()
        check(bool(line), 'MCP exited without a response')
        reply = json.loads(line)
        check(reply.get('id') == self.sequence and reply.get('jsonrpc') == '2.0', reply)
        check('error' not in reply, reply)
        return reply['result']

    def initialize(self):
        result = self.rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={},
            clientInfo=dict(name='font-mcp-parity', version='1')))
        check(result['protocolVersion'] == '2025-06-18', result)
        self.proc.stdin.write(json.dumps(dict(jsonrpc='2.0', method='notifications/initialized')) + '\n')
        self.proc.stdin.flush()

    def tool(self, name, arguments=None):
        result = self.rpc('tools/call', dict(name=name, arguments=arguments or {}))
        structured = result['structuredContent']
        check(json.loads(result['content'][0]['text']) == structured, result)
        check(result['isError'] == (not structured.get('ok', False)), result)
        return structured

    def refresh(self):
        live = self.tool('nect_session')
        check(live['ok'], live)
        self.identity = {key: live[key] for key in ('session_id', 'document_id')}
        return live

    def core(self, op, **fields):
        return self.tool('nect_command', dict(self.identity, request=dict(op=op, **fields)))

    def direct(self, request):
        return desktop_api_call(self.endpoint, dict(self.identity, op='core', request=request))

    def compare(self, op, **fields):
        request = dict(op=op, **fields)
        mcp, api = self.core(op, **fields), self.direct(request)
        check(mcp == api, (request, mcp, api))
        return mcp

    def snapshot(self):
        result = self.compare('inspect')
        check(result['ok'], result)
        return result['result']


def discovery(client):
    client.initialize()
    tools = client.rpc('tools/list')['tools']
    command = next(tool for tool in tools if tool['name'] == 'nect_command')
    description = command['description']
    for token in ('add_text_font_feature', 'update_text_font_feature', 'remove_text_font_feature',
                  'set_text_additional_axis', 'remove_text_additional_axis', 'whole_text',
                  '4294967295', 'finite doubles', 'Native 0.78', 'USE_TYPED_COMMAND',
                  'text.weight', 'text.italic', 'wght/ital', 'TEXT_AXIS_CONFLICT',
                  'font_request', 'font_runs', 'TEXT_PLATFORM_UNSUPPORTED'):
        check(token in description, 'Missing font discovery: ' + token)
    check('unchanged' in description and 'not proof of actual shaping' in description,
          'Discovery must separate typed authoring from actual shaping')
    resources = client.rpc('resources/list')['resources']
    check(any(item['uri'] == 'nect://capabilities' for item in resources), resources)
    payload = client.rpc('resources/read', dict(uri='nect://capabilities'))
    capabilities = json.loads(payload['contents'][0]['text'])
    for op in ('text_defaults', 'text_fonts', 'text_layout'):
        check(op in capabilities['semanticRequestOperations'], capabilities)
    check(any('TEXT_PLATFORM_UNSUPPORTED' in item['statement']
              for item in capabilities['unsupportedCapabilityGroups']), capabilities)


def start(exe, directory, label, native=None):
    ready = directory / (label + '-ready.json')
    endpoint = ('nect-font-' + uuid.uuid4().hex if os.name == 'nt'
                else str(directory / (label + '.sock')))
    log = open(directory / (label + '-desktop.log'), 'w', encoding='utf-8')
    command = [exe, '--automation-endpoint', endpoint, '--recovery-dir',
               str(directory / (label + '-recovery')), '--ready-file', str(ready)]
    if native:
        command.append(str(native))
    proc = subprocess.Popen(command, env=dict(os.environ, QT_QPA_PLATFORM='offscreen'),
                            stdout=log, stderr=subprocess.STDOUT)
    log.close()
    try:
        deadline = time.monotonic() + 15
        while not ready.exists():
            if proc.poll() is not None or time.monotonic() >= deadline:
                raise AssertionError('Desktop startup failed: ' +
                    (directory / (label + '-desktop.log')).read_text(encoding='utf-8'))
            time.sleep(.02)
        # Use the endpoint reported by the running host, including its Unix path.
        return proc, Client(json.loads(ready.read_text(encoding='utf-8'))['endpoint'])
    except BaseException:
        stop(proc)
        raise


def fixture(client):
    native = client.snapshot()
    source = client.compare('text_defaults')['result']
    source.update(id='font-source', content='Portable font authoring', family='Arial',
                  weight=600, italic=False,
                  weight_driver=dict(link=dict(object='font-peer', point='', field='text.weight')),
                  italic_driver=dict(link=dict(object='font-peer', point='', field='text.italic')))
    peer = copy.deepcopy(source)
    peer.update(id='font-peer-source', content='Weight and italic owner', weight=500, italic=True)
    del peer['weight_driver'], peer['italic_driver']

    def obj(id_, text=None):
        result = dict(id=id_, name=id_, visible=True, kind='text' if text else 'group',
            compositing=dict(version=1, opacity=dict(literal=1), blend='normal', isolated=False, mask=None),
            transform=[dict(literal=v) for v in (1, 0, 0, 1, 0, 0)],
            anchor=[dict(literal=0), dict(literal=0)], transform_parent=None,
            stack=[], legacy_stroke='')
        if text:
            result['text'] = text
        else:
            result['children'] = []
            del result['legacy_stroke']
        return result

    # Native encodes the Object map in lexical ID order.
    native['objects'] = [obj('font-group'), obj('font-peer', peer), obj('font-text', source)]
    native['compositions'][0]['roots'] = [item['id'] for item in native['objects']]
    return native


def text_source(native):
    return next(item['text'] for item in native['objects'] if item['id'] == 'font-text')


def command(type_, **fields):
    return dict(type=type_, object='font-text', **fields)


def live_contract(exe, directory):
    desktop = client = None
    try:
        desktop, client = start(exe, directory, 'initial')
        discovery(client)
        live = client.refresh()
        native = fixture(client)
        initial_path = directory / 'preauthored.nect'
        initial_path.write_text(json.dumps(native), encoding='utf-8')
        opened = client.tool('nect_file', dict(client.identity, op='open',
            path=str(initial_path), expected_revision=live['revision']))
        check(opened['ok'], opened)
        client.refresh()
        initial = client.snapshot()
        check(initial == native and initial['version'] == '0.78', initial)
        original = copy.deepcopy(text_source(initial))
        revision = 0
        mutations = 0

        def apply_parity(commands):
            nonlocal revision, mutations
            before = client.snapshot()
            result = client.core('apply', expected_revision=revision, commands=commands)
            check(result['ok'] and result['revision'] == revision + 1, result)
            authored = client.snapshot()
            undone = client.core('undo', expected_revision=result['revision'])
            check(undone['ok'] and undone['revision'] == result['revision'] + 1, undone)
            check(client.snapshot() == before, 'Undo changed pre-command authored state')
            replay = client.direct(dict(op='apply', expected_revision=undone['revision'], commands=commands))
            check(replay['ok'] and replay['revision'] == undone['revision'] + 1, replay)
            check(replay['result'] == result['result'], (replay, result))
            check(client.snapshot() == authored, 'Direct API replay differs from formal MCP mutation')
            revision = replay['revision']
            mutations += 1
            return text_source(authored)

        features = [dict(feature_tag='lig ', parameter=0, scope='whole_text'),
                    dict(feature_tag='KERN', parameter=1, scope='whole_text')]
        authored = apply_parity([command('add_text_font_feature', feature=item) for item in features])
        check(authored['font_features'] == features, authored)
        features[0]['parameter'] = 4294967295
        authored = apply_parity([command('update_text_font_feature', feature_tag='lig ', parameter=4294967295)])
        check(authored['font_features'] == features, 'uint32 maximum, exact order/case/padding lost')
        authored = apply_parity([command('set_text_additional_axis', axis_tag='wdth', value=12.25)])
        check(authored['additional_axis_values'] == {'wdth': 12.25}, authored)
        precise = {'wdth': 87.1234567890123, 'XTRA': -1.2345678901234567e-100, 'HUGE': 1e300}
        authored = apply_parity([command('set_text_additional_axis', axis_tag=tag, value=value)
                                 for tag, value in precise.items()])
        check(authored['additional_axis_values'] == precise, 'Finite doubles changed across Qt JSON bridge')
        preserved = {key: value for key, value in authored.items()
                     if key not in ('font_features', 'additional_axis_values')}
        check(preserved == original, 'Font commands changed other source fields or drivers')
        for field, expected in (('text.weight', 500), ('text.italic', True)):
            result = client.compare('get', ref=dict(object='font-text', point='', field=field))
            check(result['ok'] and result['result']['evaluated'] == expected, result)

        # UpdateText may edit ordinary literals only while both collections agree.
        unchanged = copy.deepcopy(authored)
        unchanged['content'] = 'Ordinary content edit with preserved font intent'
        authored = apply_parity([command('update_text', source=unchanged)])
        check(authored == unchanged, authored)

        invalid = [
            (command('add_text_font_feature', feature=features[0]), 'DUPLICATE_TEXT_FONT_FEATURE'),
            (command('add_text_font_feature', feature=dict(feature_tag='bad', parameter=0, scope='whole_text')), 'INVALID_TEXT_FONT_TAG'),
            (command('add_text_font_feature', feature=dict(feature_tag='a\nbc', parameter=0, scope='whole_text')), 'INVALID_TEXT_FONT_TAG'),
            (command('add_text_font_feature', feature=dict(feature_tag='liga', parameter=0, scope='range')), 'INVALID_TEXT_FONT_SCOPE'),
            (command('update_text_font_feature', feature_tag='gone', parameter=0), 'MISSING_TEXT_FONT_FEATURE'),
            (command('remove_text_font_feature', feature_tag='gone'), 'MISSING_TEXT_FONT_FEATURE'),
            (command('remove_text_additional_axis', axis_tag='gone'), 'MISSING_TEXT_AXIS'),
            (command('set_text_additional_axis', axis_tag='bad', value=1), 'INVALID_TEXT_FONT_TAG'),
            (dict(type='remove_text_font_feature', object='missing', feature_tag='KERN'), 'MISSING_OBJECT'),
            (dict(type='set_text_additional_axis', object='font-group', axis_tag='wdth', value=1), 'INVALID_TEXT'),
        ]
        for value in (-1, 4294967296, .5, True):
            invalid.append((command('update_text_font_feature', feature_tag='lig ', parameter=value), 'INVALID_TEXT_FONT_FEATURE'))
        for tag in ('wght', 'ital'):
            invalid += [(command('set_text_additional_axis', axis_tag=tag, value=500), 'TEXT_AXIS_CONFLICT'),
                        (command('remove_text_additional_axis', axis_tag=tag), 'TEXT_AXIS_CONFLICT')]
        for field in ('font_features', 'additional_axis_values'):
            changed = copy.deepcopy(authored)
            del changed[field]
            invalid.append((command('update_text', source=changed), 'USE_TYPED_COMMAND'))

        def reject(commands, code, expected=None):
            before, history = client.snapshot(), client.compare('history')
            result = client.compare('apply', expected_revision=revision if expected is None else expected,
                                    commands=commands)
            check(not result['ok'] and result['error']['code'] == code and result['revision'] == revision, result)
            check(client.snapshot() == before and client.compare('history') == history,
                  'Rejected command changed authored state or history')

        for bad, code in invalid:
            reject([bad], code)
        reject([command('set_text_additional_axis', axis_tag='wdth', value=99)], 'REVISION_CONFLICT', revision - 1)
        reject([command('set_text_additional_axis', axis_tag='wdth', value=99),
                command('remove_text_font_feature', feature_tag='gone')], 'MISSING_TEXT_FONT_FEATURE')

        layout = client.compare('text_layout', object='font-text')
        fonts = client.compare('text_fonts')
        if os.name != 'nt':
            for result in (layout, fonts):
                check(not result['ok'] and result['error']['code'] == 'TEXT_PLATFORM_UNSUPPORTED', result)
            projection = 'TEXT_PLATFORM_UNSUPPORTED'
        else:
            check(layout['ok'] and fonts['ok'], (layout, fonts))
            check('font_request' in layout['result'] and 'font_runs' in layout['result'], layout)
            projection = 'receipt transport only; no DirectWrite effect claim'

        retained = client.snapshot()
        authored = apply_parity([command('remove_text_font_feature', feature_tag=item['feature_tag']) for item in features] +
                                [command('remove_text_additional_axis', axis_tag=tag) for tag in precise])
        check('font_features' not in authored and 'additional_axis_values' not in authored, authored)
        removed = client.snapshot()
        undone = client.core('undo', expected_revision=revision)
        check(undone['ok'] and client.snapshot() == retained, 'Undo removal lost precise authored intent')
        redone = client.core('redo', expected_revision=undone['revision'])
        check(redone['ok'] and client.snapshot() == removed, 'Redo removal changed authored state')
        restored = client.core('undo', expected_revision=redone['revision'])
        check(restored['ok'] and client.snapshot() == retained, 'Restore before save failed')
        revision = restored['revision']

        destination = directory / 'font-native-reopen.nect'
        saved = client.tool('nect_file', dict(client.identity, op='save', path=str(destination), expected_revision=revision))
        check(saved['ok'] and saved['revision'] == revision, saved)
        saved_bytes = destination.read_bytes()
        check(json.loads(saved_bytes) == retained, 'Native save differs from inspect')
        check(b'font_request' not in saved_bytes and b'font_runs' not in saved_bytes, 'Derived receipts leaked into native')
        previous_identity = dict(client.identity)
        stop(client.proc); client = None
        stop(desktop); desktop = None
        desktop, client = start(exe, directory, 'reopen', destination)
        client.initialize()
        live = client.refresh()
        check(client.identity != previous_identity and live['revision'] == 0, live)
        check(client.snapshot() == retained and destination.read_bytes() == saved_bytes,
              'Separate desktop process native cold reopen lost exact authoring or rewrote bytes')
        stale = client.tool('nect_command', dict(previous_identity,
            request=dict(op='apply', expected_revision=0,
                         commands=[command('remove_text_font_feature', feature_tag='KERN')])))
        check(not stale['ok'] and stale['error']['code'] == 'SESSION_CONFLICT', stale)
        check(client.snapshot() == retained and client.refresh()['revision'] == 0, 'Stale identity changed reopened state')
        return dict(status='PASS', checks=CHECKS, mutation_api_mcp_replays=mutations,
                    rejected_requests=len(invalid) + 3, native_version='0.78',
                    actual_desktop_processes=2, mcp_initialize_tools_resources=True,
                    uint32_max_exact=True, finite_doubles_exact=True, undo_redo=True,
                    native_cold_reopen=True, projection=projection,
                    gui_acceptance=False, directwrite_effect_claim=False)
    finally:
        if client:
            stop(client.proc)
        stop(desktop)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('desktop', nargs='?')
    parser.add_argument('--discovery-only', action='store_true')
    args = parser.parse_args()
    if args.discovery_only:
        client = Client('unused-font-discovery-endpoint')
        try:
            discovery(client)
            print(json.dumps(dict(status='PASS', checks=CHECKS, discovery_only=True)))
        finally:
            stop(client.proc)
    else:
        if not args.desktop:
            parser.error('a desktop executable or --discovery-only is required')
        with tempfile.TemporaryDirectory(prefix='nect-font-mcp-') as directory:
            print(json.dumps(live_contract(str(Path(args.desktop).resolve()), Path(directory)), indent=2))


if __name__ == '__main__':
    main()

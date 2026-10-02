"""Formal MCP plus Host Save As/cold-reopen parity for Artboard Templates."""
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
NATIVE_VERSION = re.search(r'native_version\s*=\s*"([^"]+)"',
    (ROOT / 'include/nect/io.hpp').read_text(encoding='utf-8')).group(1)
sys.path.insert(0, str(ROOT / 'scripts'))
from session_client import call as desktop_api_call

DESKTOP_EXE = str(Path(sys.argv[1]).resolve())
CLI_EXE = str(Path(sys.argv[2]).resolve())
desktop = mcp = None
sequence = 0
identity = {}
endpoint = ''
checks = 0
SOURCE_GRID = dict(id='source-grid', bounds=dict(x=20, y=20, width=760, height=560),
    columns=2, rows=2, column_gutter=20, row_gutter=20)


def check(condition, message):
    global checks
    if not condition:
        raise AssertionError(message)
    checks += 1


def run(executable, *args, **kwargs):
    return subprocess.run([executable, *args], capture_output=True, text=True,
        encoding='utf-8', timeout=30, **kwargs)


def cli_fixture(path):
    demo = run(CLI_EXE, '--demo')
    check(demo.returncode == 0, 'Rebuilt CLI creates a current native starting fixture')
    sample = json.loads(demo.stdout)
    check(sample['version'] == NATIVE_VERSION, 'Fixture is at the canonical current native version')
    composition = sample['compositions'][0]
    source = composition['artboards'][0]
    source_id, composition_id = source['id'], composition['id']
    # Author the exact native fixture through one canonical Session revision,
    # then persist its inspect document as the input opened by the Desktop Host.
    path.write_text(json.dumps(sample, separators=(',', ':')), encoding='utf-8')
    process = subprocess.Popen([CLI_EXE, '--serve', str(path)], stdin=subprocess.PIPE,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8', bufsize=1)

    def request(payload):
        process.stdin.write(json.dumps(payload, separators=(',', ':')) + '\n')
        process.stdin.flush()
        line = process.stdout.readline()
        if not line:
            raise AssertionError('Fixture Session exited before responding')
        response = json.loads(line)
        check(response.get('ok') is True, 'Native fixture setup command succeeds: ' + repr(response))
        return response

    try:
        definitions = request({'op': 'primitive_types'})['result']
        primitive = copy.deepcopy(next(item['template'] for item in definitions
            if item['type'] == 'nect.shape.rectangle'))
        primitive['id'] = 'mcp-template-rectangle-source'
        grid_primitive = copy.deepcopy(primitive)
        grid_primitive['parameters'].update(center_x=dict(literal=5), center_y=dict(literal=5),
            width=dict(literal=10), height=dict(literal=10))
        operations = request({'op': 'operator_types'})['result']
        fill = copy.deepcopy(next(item['template'] for item in operations
            if item['type'] == 'nect.paint.fill'))
        fill['id'] = 'mcp-template-fill'
        for key, value in dict(r=.8, g=.3, b=.1, a=1).items():
            fill['parameters'][key]['literal'] = value
        source.update(name='S', x=0, y=0, width=800, height=600)
        board_a = dict(id='target-a', name='Same target', x=900, y=0, width=640, height=480)
        board_b = dict(id='target-b', name='Same target', x=1800, y=0, width=640, height=480)
        board_c = dict(id='target-c', name='Probe', x=2700, y=0, width=640, height=480)
        grid = copy.deepcopy(SOURCE_GRID)
        setup = request({'op': 'apply', 'expected_revision': 0, 'commands': [
            dict(type='update_artboard', composition=composition_id, artboard=source),
            dict(type='add_artboard', composition=composition_id, artboard=board_a, index=1),
            dict(type='add_artboard', composition=composition_id, artboard=board_b, index=2),
            dict(type='add_artboard', composition=composition_id, artboard=board_c, index=3),
            dict(type='create_folder', composition=composition_id, parent='', id='mcp-grid-probe-root',
                name='Independent Grid parity probes'),
            *[dict(type='create_primitive', composition=composition_id, parent='mcp-grid-probe-root',
                id=f'mcp-grid-probe-{letter}', name=f'Grid probe {letter.upper()}',
                source=dict(grid_primitive, id=f'mcp-grid-probe-source-{letter}')) for letter in 'abc'],
            *[dict(type='set', ref=dict(object=f'mcp-grid-probe-{letter}', point='', field='transform.tx'),
                value=x) for letter, x in zip('abc', (0, 30, 90))],
            dict(type='set_artboard_layout', composition=composition_id, artboard_id=source_id,
                layout=dict(margin=dict(left=10, top=10, right=10, bottom=10), grid=grid)),
            dict(type='create_folder', composition=composition_id, parent='', id='mcp-template-root',
                name='Shared definition source'),
            dict(type='create_primitive', composition=composition_id, parent='mcp-template-root', id='mcp-template-rectangle',
                name='Shared mark', source=primitive),
            dict(type='add_operation', object='mcp-template-rectangle', index=0, operation=fill),
            dict(type='create_definition', id='mcp-template-definition', name='Shared definition', root='mcp-template-root')
        ]})
        check(setup['revision'] == 1,
            'Fixture source, same-named targets, Grid, paint and Definition are one canonical Session revision')
        authored = request({'op': 'inspect'})['result']
        check(authored['version'] == NATIVE_VERSION and authored['compositions'][0]['id'] == composition_id and
            len(authored['compositions'][0]['artboards']) == 4 and
            authored['definitions'][0]['id'] == 'mcp-template-definition',
            'Native fixture readback contains the exact authored Composition, four Artboards and Definition')
        path.write_text(json.dumps(authored, separators=(',', ':')), encoding='utf-8')
        validation = run(CLI_EXE, '--validate', input=path.read_text(encoding='utf-8'))
        check(validation.returncode == 0 and json.loads(validation.stdout)['native_version'] == NATIVE_VERSION,
            'Persisted canonical Session readback validates at the current version before Desktop Host open')
    except BaseException:
        stop(process)
        raise
    finally:
        if process.stdin and not process.stdin.closed:
            process.stdin.close()
    result = process.wait(timeout=10)
    stderr = process.stderr.read()
    check(result == 0, 'Native fixture writer process exits cleanly: ' + stderr)
    return composition_id, source_id


def rpc(method, params=None):
    global sequence
    sequence += 1
    mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', id=sequence, method=method,
        params=params or {})) + '\n')
    mcp.stdin.flush()
    reply = json.loads(mcp.stdout.readline())
    check(reply.get('id') == sequence and reply.get('jsonrpc') == '2.0',
        'Formal MCP request has matching JSON-RPC identity')
    return reply


def tool(name, arguments=None):
    result = rpc('tools/call', dict(name=name, arguments=arguments or {}))['result']
    structured = result['structuredContent']
    check(json.loads(result['content'][0]['text']) == structured,
        'Formal MCP typed structuredContent equals its JSON text')
    check(result['isError'] == (not structured.get('ok', False)),
        'Formal MCP error flag matches canonical Session result')
    return structured


def core(op, **fields):
    return tool('nect_command', dict(identity, request=dict(op=op, **fields)))


def direct(request):
    return desktop_api_call(endpoint, dict(identity, op='core', request=request))


def compare(op, **fields):
    if op not in {'inspect', 'get', 'properties', 'export_svg', 'artboards', 'history'}:
        raise AssertionError('MCP/API comparison is restricted to read-only operations')
    request = dict(op=op, **fields)
    via_mcp, via_api = core(op, **fields), direct(request)
    check(via_mcp == via_api, 'Formal MCP and canonical Host API return the same typed result: ' + op)
    return via_mcp


def apply_mcp(expected_revision, commands):
    return core('apply', expected_revision=expected_revision, commands=commands)


def _matrix(value):
    match = re.fullmatch(r'matrix\(([^)]*)\)', value.strip())
    if not match:
        raise AssertionError('SVG geometry exposes a concrete affine matrix')
    result = [float(part) for part in match.group(1).split()]
    if len(result) != 6:
        raise AssertionError('SVG affine matrix has six numeric components')
    return result


def _compose(outer, inner):
    a, b, c, d, e, f = outer
    g, h, i, j, k, l = inner
    return [a*g+c*h, b*g+d*h, a*i+c*j, b*i+d*j, a*k+c*l+e, b*k+d*l+f]


def svg_source_paths(svg, title_text):
    root = ET.fromstring(svg)
    parents = {child: parent for parent in root.iter() for child in parent}
    results = []
    for title in root.iter():
        if title.tag.rsplit('}', 1)[-1] != 'title' or title.text != title_text:
            continue
        owner = parents.get(title)
        for path in owner.iter() if owner is not None else ():
            if path.tag.rsplit('}', 1)[-1] != 'path' or not path.get('d'):
                continue
            chain = []
            node = path
            while node is not None:
                transform = node.get('transform')
                if transform and transform.startswith('matrix('):
                    chain.append(_matrix(transform))
                node = parents.get(node)
            transform = [1, 0, 0, 1, 0, 0]
            for matrix in reversed(chain):
                transform = _compose(transform, matrix)
            coordinates = [float(part) for part in re.findall(
                r'[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?', path.get('d'))]
            if not coordinates or len(coordinates) % 2:
                continue
            points = []
            for index in range(0, len(coordinates), 2):
                x, y = coordinates[index:index+2]
                a, b, c, d, e, f = transform
                points.append((a*x+c*y+e, b*x+d*y+f))
            results.append(dict(bounds=(min(point[0] for point in points),
                min(point[1] for point in points), max(point[0] for point in points),
                max(point[1] for point in points)), fill=path.get('fill'), points=len(points)))
    return results


def start(desktop_exe, endpoint_name, recovery, ready_path, native_path):
    command = [desktop_exe, '--automation-endpoint', endpoint_name,
        '--recovery-dir', str(recovery), '--ready-file', str(ready_path), str(native_path)]
    proc = subprocess.Popen(command, env=dict(os.environ, QT_QPA_PLATFORM='offscreen'),
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        deadline = time.monotonic() + 15
        while not ready_path.exists():
            if proc.poll() is not None or time.monotonic() >= deadline:
                stdout, stderr = proc.communicate(timeout=2)
                raise AssertionError('Desktop Host did not open native fixture: ' +
                    stdout.decode('utf-8', 'replace') + stderr.decode('utf-8', 'replace'))
            time.sleep(.02)
    except BaseException:
        stop(proc)
        raise
    return proc


def connect(endpoint_name):
    return subprocess.Popen([sys.executable, str(ROOT / 'scripts/mcp_server.py'),
        '--endpoint', endpoint_name], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, text=True, encoding='utf-8', bufsize=1)


def stop(proc):
    if proc:
        if proc.stdin:
            try:
                proc.stdin.close()
            except OSError:
                pass
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.communicate(timeout=8)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.communicate(timeout=8)
        elif proc.stdout or proc.stderr:
            proc.communicate(timeout=8)


def main():
    global desktop, mcp, identity, endpoint, sequence
    with tempfile.TemporaryDirectory(prefix='nect-mcp-template-') as directory:
        try:
            temp = Path(directory)
            source_path = temp / 'input.nect'
            working_path = temp / 'working.nect'
            destination_path = temp / 'save-as.nect'
            composition_id, source_id = cli_fixture(source_path)
            original_bytes = source_path.read_bytes()
            original_sha = hashlib.sha256(original_bytes).hexdigest()
            endpoint = 'nect-template-' + uuid.uuid4().hex
            desktop = start(DESKTOP_EXE, endpoint, temp / 'recovery', temp / 'ready.json', source_path)
            mcp = connect(endpoint)
            check(rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={},
                clientInfo=dict(name='template-contract', version='1')))['result']['protocolVersion'] == '2025-06-18',
                'Formal MCP initializes the documented protocol')
            mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', method='notifications/initialized')) + '\n')
            mcp.stdin.flush()
            definitions = rpc('tools/list')['result']['tools']
            description = next(item for item in definitions if item['name'] == 'nect_command')['description']
            for operation in ('create_artboard_template', 'rename_artboard_template', 'delete_artboard_template',
                    'assign_artboard_template', 'set_artboard_template_override', 'reset_artboard_template_override',
                    'detach_artboard_template', 'duplicate_template_artboard'):
                check(operation in description, 'Formal MCP description advertises Template lifecycle operation ' + operation)
            for operation in ('add_artboard_guide', 'update_artboard_guide', 'delete_artboard_guide',
                    'set_artboard_guide_override', 'reset_artboard_guide_override', 'detach_artboard_guide'):
                check(operation in description, 'Formal MCP description advertises Artboard Guide command ' + operation)
            check('guide_artboard?:id' in description and 'omitted/null scope retains global Guide semantics' in description,
                'Formal MCP description exposes target-local Guide scope and legacy global behavior')
            check('inherited Template Grids resolve by target assignment Grid ID without local materialization' in description and
                'distribute_objects {objects:[id],axis:x/y,reference?:selection|artboard:id|grid:id|key_object:id,spacing?:du}' in description and
                'Selection reference keeps the outer objects fixed' in description,
                'Formal MCP description exposes inherited target-local Grid bounds for both Align and Distribute')
            check('artboard.guide.position' in description and f'Native writer {NATIVE_VERSION}' in description,
                'Formal MCP description states the typed scoped Guide Ref and current native boundary')
            for field in ('transform.tx', 'transform.ty', 'generator.width', 'generator.height'):
                check(field in description, 'Formal MCP description advertises supported descendant R04 field ' + field)
            check('Fill/Color' in description and 'no local Fill/Color override' in description,
                'Formal MCP description states source Fill/Color propagation boundary')

            live = tool('nect_session')
            identity = {key: live[key] for key in ('session_id', 'document_id')}
            revision = live['revision']
            check(live['document_id'] == 'document-demo', 'Desktop Host opened the exact prepared native document')
            initial = compare('inspect')['result']
            initial_comp = next(item for item in initial['compositions'] if item['id'] == composition_id)
            check(source_id == 'art-main' and [item['id'] for item in initial_comp['artboards']] ==
                ['art-main', 'target-a', 'target-b', 'target-c'] and
                initial_comp['templates'] == [] and initial['definitions'][0]['id'] == 'mcp-template-definition',
                'Host opened the exact prepared native source Artboard, same-named targets, Grid and Definition')

            working_saved = tool('nect_file', dict(identity, op='save', expected_revision=revision,
                path=str(working_path)))
            check(working_saved['ok'] and working_path.exists() and
                source_path.read_bytes() == original_bytes and
                hashlib.sha256(source_path.read_bytes()).hexdigest() == original_sha,
                'Before parity mutations Host Save As selects a distinct working native destination and preserves source bytes')
            working_native = working_path.read_bytes()
            check(json.loads(working_native.decode('utf-8')) == initial,
                'Working destination is the exact cold-readable starting native document')

            commands = [
                dict(type='add_artboard_guide', composition=composition_id, artboard=source_id,
                    guide=dict(id='source-guide-x', name='Source X', axis='x', position=40, enabled=True)),
                dict(type='add_artboard_guide', composition=composition_id, artboard=source_id,
                    guide=dict(id='source-guide-y', name='Source Y', axis='y', position=60, enabled=True)),
                dict(type='add_artboard_guide', composition=composition_id, artboard=source_id,
                    guide=dict(id='GX', name='Scoped X', axis='x', position=40, enabled=True)),
                dict(type='add_artboard_guide', composition=composition_id, artboard='target-a',
                    guide=dict(id='local-guide-a', name='Local A', axis='x', position=15, enabled=True)),
                dict(type='add_artboard_guide', composition=composition_id, artboard='target-a',
                    guide=dict(id='temporary-guide-a', name='Temporary A', axis='y', position=22, enabled=True)),
                dict(type='create_artboard_template', composition=composition_id,
                    id='mcp-template-main', name='Shared source', source_artboard=source_id,
                    definition='mcp-template-definition'),
                dict(type='create_artboard_template', composition=composition_id,
                    id='mcp-template-probe', name='Probe source', source_artboard=source_id, definition=None),
                dict(type='rename_artboard_template', composition=composition_id,
                    template='mcp-template-probe', name='Renamed probe'),
                dict(type='assign_artboard_template', composition=composition_id, artboard='target-a',
                    template='mcp-template-main', content_instance='content-a'),
                dict(type='set_artboard_guide_override', composition=composition_id, artboard='target-a',
                    guide_id='GX', field='position', value=90),
                dict(type='assign_artboard_template', composition=composition_id, artboard='target-b',
                    template='mcp-template-main', content_instance='content-b'),
                dict(type='assign_artboard_template', composition=composition_id, artboard='target-c',
                    template='mcp-template-probe', content_instance=None),
                dict(type='set_artboard_guide_override', composition=composition_id, artboard='target-a',
                    guide_id='source-guide-x', field='position', value=90),
                dict(type='reset_artboard_guide_override', composition=composition_id, artboard='target-a',
                    guide_id='source-guide-x', field='position'),
                dict(type='set_artboard_guide_override', composition=composition_id, artboard='target-a',
                    guide_id='source-guide-x', field='position', value=90),
                dict(type='set_artboard_guide_override', composition=composition_id, artboard='target-b',
                    guide_id='source-guide-y', field='enabled', value=False),
                dict(type='set_artboard_template_override', composition=composition_id, artboard='target-a',
                    field='frame.width', value=900),
                dict(type='set_artboard_template_override', composition=composition_id, artboard='target-a',
                    field='layout.margin', value=dict(left=12, top=12, right=12, bottom=12)),
                dict(type='set_artboard_template_override', composition=composition_id, artboard='target-a',
                    field='layout.grid', value=dict(id='template-grid-target-a',
                        bounds=dict(x=20, y=20, width=860, height=560), columns=2, rows=2,
                        column_gutter=20, row_gutter=20)),
                dict(type='set_artboard_template_override', composition=composition_id, artboard='target-c',
                    field='frame.width', value=850),
                dict(type='set_artboard_template_override', composition=composition_id, artboard='target-c',
                    field='layout.margin', value=None),
                dict(type='set_artboard_template_override', composition=composition_id, artboard='target-c',
                    field='layout.grid', value=dict(id='template-grid-target-c',
                        bounds=dict(x=20, y=20, width=760, height=560), columns=2, rows=2,
                        column_gutter=20, row_gutter=20)),
                dict(type='reset_artboard_template_override', composition=composition_id,
                    artboard='target-c', field='frame.width'),
                dict(type='reset_artboard_template_override', composition=composition_id,
                    artboard='target-c', field='layout.margin'),
                dict(type='reset_artboard_template_override', composition=composition_id,
                    artboard='target-c', field='layout.grid')]
            created = apply_mcp(revision, commands)
            check(created['ok'] and created['revision'] == revision + 1,
                'Template and Artboard Guide lifecycle commands share one formal MCP Session mutation')
            revision = created['revision']

            guide_baseline = compare('inspect')['result']
            scoped_a = apply_mcp(revision, [dict(type='align_objects', objects=['path-A'], axis='x',
                alignment='min', reference='guide:GX', guide_artboard='target-a')])
            check(scoped_a['ok'] and scoped_a['revision'] == revision + 1 and
                compare('get', ref=dict(object='path-A', point='', field='transform.tx'))['result']['evaluated'] == 890,
                'MCP Guide GX scoped to A reaches its fixed global bound at 990 from the independent Guide fixture')
            revision = scoped_a['revision']
            scoped_b = apply_mcp(revision, [dict(type='align_objects', objects=['path-A'], axis='x',
                alignment='min', reference='guide:GX', guide_artboard='target-b')])
            check(scoped_b['ok'] and scoped_b['revision'] == revision + 1 and
                compare('get', ref=dict(object='path-A', point='', field='transform.tx'))['result']['evaluated'] == 1740,
                'The same stable Guide GX scoped to B reaches its distinct fixed global bound at 1840')
            revision = scoped_b['revision']
            check(compare('inspect')['result'] == direct(dict(op='inspect'))['result'],
                'Scoped Guide alignment state is identical through formal MCP and the canonical Host API')
            undone_b = core('undo', expected_revision=revision)
            check(undone_b['ok'] and undone_b['revision'] == revision + 1 and
                compare('get', ref=dict(object='path-A', point='', field='transform.tx'))['result']['evaluated'] == 890,
                'One Host Undo removes only the second scoped Guide alignment')
            revision = undone_b['revision']
            undone_a = core('undo', expected_revision=revision)
            check(undone_a['ok'] and undone_a['revision'] == revision + 1 and
                compare('inspect')['result'] == guide_baseline,
                'A second Host Undo restores the exact pre-alignment typed state')
            revision = undone_a['revision']

            disabled_gx = apply_mcp(revision, [dict(type='set_artboard_guide_override',
                composition=composition_id, artboard='target-b', guide_id='GX', field='enabled', value=False)])
            check(disabled_gx['ok'] and disabled_gx['revision'] == revision + 1,
                'A target-local disabled Guide state is authored through formal MCP')
            revision = disabled_gx['revision']
            settled = tool('nect_file', dict(identity, op='save', expected_revision=revision, path=str(working_path)))
            check(settled['ok'], 'Flush committed working state before the negative Guide parity probe')
            before_disabled_refusal = compare('inspect')
            history_before_disabled_refusal = compare('history')
            native_before_disabled_refusal = working_path.read_bytes()
            hash_before_disabled_refusal = hashlib.sha256(native_before_disabled_refusal).hexdigest()
            disabled_command = [dict(type='align_objects', objects=['path-A'], axis='x',
                alignment='min', reference='guide:GX', guide_artboard='target-b')]
            disabled_refusal = apply_mcp(revision, disabled_command)
            disabled_api_refusal = direct(dict(op='apply', expected_revision=revision, commands=disabled_command))
            check(disabled_refusal == disabled_api_refusal and not disabled_refusal['ok'] and
                disabled_refusal['error']['code'] == 'DISABLED_ARTBOARD_GUIDE' and
                disabled_refusal['revision'] == revision and compare('inspect') == before_disabled_refusal and
                compare('history') == history_before_disabled_refusal and
                working_path.read_bytes() == native_before_disabled_refusal and
                hashlib.sha256(working_path.read_bytes()).hexdigest() == hash_before_disabled_refusal,
                'Disabled scoped Guide refusal preserves exact MCP/API state, revision, History and working native bytes')

            inspect = compare('inspect')['result']
            comp = next(item for item in inspect['compositions'] if item['id'] == composition_id)
            templates = {item['id']: item for item in comp['templates']}
            check(templates['mcp-template-main']['source_artboard'] == source_id and
                templates['mcp-template-main']['definition'] == 'mcp-template-definition' and
                templates['mcp-template-probe']['name'] == 'Renamed probe',
                'Typed readback retains exact source Artboard, Definition and renamed stable Template IDs')
            guide_a = compare('get', ref=dict(object='target-a', point='source-guide-x',
                field='artboard.guide.position'))['result']
            guide_b = compare('get', ref=dict(object='target-b', point='source-guide-y',
                field='artboard.guide.enabled'))['result']
            check(guide_a['evaluated'] == 90 and guide_a['source']['literal_position'] == 40 and
                guide_a['source_artboard'] == source_id and guide_a['position_overridden'] and
                guide_b['evaluated'] is False and guide_b['enabled_overridden'],
                'MCP/API typed scoped Guide reads retain target-local position and enabled override metadata')
            guide_refs = compare('properties')['result']
            check(any(item.get('ref') == dict(object='target-a', point='source-guide-x',
                field='artboard.guide.position') for item in guide_refs),
                'Property list exposes a target-scoped inherited Artboard Guide Ref')
            instances = {item['id']: item for item in inspect['objects']
                if item['id'] in ('content-a', 'content-b')}
            check(instances['content-a']['instance']['definition'] == 'mcp-template-definition' and
                instances['content-b']['instance']['definition'] == 'mcp-template-definition',
                'Both same-named targets own distinct ordinary Definition Instances')

            before_projection = compare('export_svg', composition=composition_id, artboard='target-a')['result']
            geometry_edit = apply_mcp(revision, [
                dict(type='set_instance_override', instance='content-a',
                    target=dict(object='mcp-template-rectangle', point='', field='generator.width'), value=42),
                dict(type='set_instance_override', instance='content-a',
                    target=dict(object='mcp-template-rectangle', point='', field='transform.tx'), value=33),
                dict(type='update_artboard_guide', composition=composition_id, artboard=source_id,
                    guide=dict(id='source-guide-x', name='Updated X', axis='x', position=50, enabled=True)),
                dict(type='update_artboard_guide', composition=composition_id, artboard=source_id,
                    guide=dict(id='source-guide-y', name='Updated Y', axis='y', position=80, enabled=True)),
                dict(type='delete_artboard_guide', composition=composition_id, artboard='target-a',
                    guide_id='temporary-guide-a')])
            check(geometry_edit['ok'] and geometry_edit['revision'] == revision + 1,
                'Geometry plus Guide update/delete commands apply through the formal MCP: ' + repr(geometry_edit))
            revision = geometry_edit['revision']
            updated_guide = compare('get', ref=dict(object='target-a', point='source-guide-x',
                field='artboard.guide.position'))['result']
            updated_inspect = compare('inspect')['result']
            updated_comp = next(item for item in updated_inspect['compositions'] if item['id'] == composition_id)
            updated_a = next(item for item in updated_comp['artboards'] if item['id'] == 'target-a')
            check(updated_guide['evaluated'] == 90 and updated_guide['source']['literal_position'] == 50 and
                updated_guide['source']['name'] == 'Updated X' and not any(
                    guide['id'] == 'temporary-guide-a' for guide in updated_a.get('local_guides', [])),
                'Source edits keep the A override by stable ID and a deleted target-local Guide stays absent')
            geometry_projection = compare('export_svg', composition=composition_id, artboard='target-a')['result']
            geometry_paths = svg_source_paths(geometry_projection, 'Shared mark')
            expected_bounds = (912.0, -70.0, 954.0, 70.0)
            check(geometry_projection != before_projection and any(
                all(abs(actual-expected) < 1e-7 for actual, expected in zip(path['bounds'], expected_bounds))
                and path['fill'] == 'rgb(80%,30%,10%)' for path in geometry_paths),
                'Actual Host SVG numeric geometry oracle mismatch: ' + repr(geometry_paths))

            reset_position = apply_mcp(revision, [
                dict(type='reset_artboard_guide_override', composition=composition_id, artboard='target-a',
                    guide_id='source-guide-x', field='position'),
                dict(type='set_artboard_guide_override', composition=composition_id, artboard='target-a',
                    guide_id='source-guide-x', field='position', value=90)])
            check(reset_position['ok'] and reset_position['revision'] == revision + 1 and
                compare('get', ref=dict(object='target-a', point='source-guide-x',
                    field='artboard.guide.position'))['result']['evaluated'] == 90,
                'MCP reset and explicit reapply of one Guide field remain atomic and target-scoped')
            revision = reset_position['revision']

            detached_guide = apply_mcp(revision,[dict(type='detach_artboard_guide', composition=composition_id,
                artboard='target-b', guide_id='source-guide-y', new_guide_id='detached-guide-b-y')])
            identity_map = detached_guide['result']['detached_artboard_guides'][0]
            check(detached_guide['ok'] and detached_guide['revision'] == revision + 1 and
                identity_map['target'] == dict(composition=composition_id, artboard='target-b') and
                identity_map['source_guide_id'] == 'source-guide-y' and
                identity_map['new_authored_guide']['id'] == 'detached-guide-b-y' and
                identity_map['new_authored_guide']['position'] == 80 and identity_map['source_suppressed'],
                'Item detach returns the exact scoped occurrence/new authored ID map and freezes effective values')
            revision = detached_guide['revision']

            fill_edit = apply_mcp(revision, [dict(type='set',
                ref=dict(object='mcp-template-rectangle', point='', field='op.mcp-template-fill.r'), value=.25)])
            check(fill_edit['ok'] and fill_edit['revision'] == revision + 1,
                'Source Fill edit applies through the formal MCP after geometry is independently observable')
            revision = fill_edit['revision']
            inspect = compare('inspect')['result']
            instance_a = next(item for item in inspect['objects'] if item['id'] == 'content-a')
            override_fields = {item['target']['field'] for item in instance_a['instance']['overrides']}
            check(override_fields == {'generator.width', 'transform.tx'},
                'Typed/native readback keeps only the two bounded local Scalars; source Fill is not overridden')
            after_projection = compare('export_svg', composition=composition_id, artboard='target-a')['result']
            recolored_paths = svg_source_paths(after_projection, 'Shared mark')
            check(after_projection != geometry_projection and any(
                all(abs(actual-expected) < 1e-7 for actual, expected in zip(path['bounds'], expected_bounds))
                and path['fill'] == 'rgb(25%,30%,10%)' for path in recolored_paths),
                'Source Fill propagates through actual SVG while the independently overridden numeric bounds stay fixed')

            artboards = compare('artboards', composition=composition_id)['result']
            frames = {item['authored']['id']: item for item in artboards}
            a = frames['target-a']; b = frames['target-b']; c = frames['target-c']
            check(a['authored']['template_assignment']['template_id'] == 'mcp-template-main' and
                a['authored']['template_assignment']['content_instance'] == 'content-a' and
                a['authored']['template_assignment']['width_override'] == 900 and
                a['evaluated']['width'] == 900 and a['evaluated']['height'] == 600,
                'Host Artboard readback distinguishes A authored frame width from inherited evaluated height')
            check(a['evaluated']['layout']['margin']['left'] == 12 and
                a['evaluated']['layout']['grid']['id'] == 'template-grid-target-a' and
                a['evaluated']['layout']['grid']['bounds']['width'] == 860,
                'Host Artboard readback returns A target-local Margin/Grid values and reserved Grid ID')
            check(b['authored']['template_assignment']['template_id'] == 'mcp-template-main' and
                b['authored']['template_assignment']['content_instance'] == 'content-b' and
                b['evaluated']['width'] == 800 and b['evaluated']['layout']['grid']['id'] == 'template-grid-target-b' and
                b['evaluated']['layout']['grid']['bounds']['width'] == 760,
                'Same-named B remains assigned by stable ID with inherited frame/layout and its target-local Grid ID')
            check(c['authored']['template_assignment']['template_id'] == 'mcp-template-probe' and
                c['authored']['template_assignment']['width_override'] is None and
                c['evaluated']['width'] == 800 and c['evaluated']['layout']['margin']['left'] == 10 and
                c['evaluated']['layout']['grid']['id'] == 'template-grid-target-c' and
                c['evaluated']['layout']['grid']['bounds']['width'] == 760,
                'Reset of all local C families returns to the valid inherited frame, Margin and Grid: ' + repr(c))

            grid_align_b = apply_mcp(revision, [dict(type='align_objects', objects=['path-B'], axis='x',
                alignment='min', reference='grid:template-grid-target-b')])
            check(grid_align_b['ok'] and grid_align_b['revision'] == revision + 1 and
                compare('get', ref=dict(object='path-B', point='', field='transform.tx'))['result']['evaluated'] == 1720,
                'MCP aligns to inherited target-B Grid bounds at fixed global x=1820')
            revision = grid_align_b['revision']
            grid_align_c = apply_mcp(revision, [dict(type='align_objects', objects=['path-B'], axis='x',
                alignment='min', reference='grid:template-grid-target-c')])
            check(grid_align_c['ok'] and grid_align_c['revision'] == revision + 1 and
                compare('get', ref=dict(object='path-B', point='', field='transform.tx'))['result']['evaluated'] == 2620,
                'The distinct inherited target-C Grid ID aligns to fixed global x=2720')
            revision = grid_align_c['revision']
            check(compare('inspect')['result'] == direct(dict(op='inspect'))['result'],
                'Inherited Grid alignment authored state is identical through formal MCP and the Host API')
            inherited_state = compare('inspect')['result']
            source_board = next(item for item in inherited_state['compositions'][0]['artboards'] if item['id'] == source_id)
            boards_by_id = {item['id']: item for item in inherited_state['compositions'][0]['artboards']}
            check(source_board['layout']['grid'] == SOURCE_GRID and
                'layout' not in boards_by_id['target-b'] and 'layout' not in boards_by_id['target-c'],
                'Aligning to two target-local inherited Grid IDs preserves source Grid bytes and materializes no target layout')
            undo_grid_c = core('undo', expected_revision=revision)
            check(undo_grid_c['ok'] and undo_grid_c['revision'] == revision + 1 and
                compare('get', ref=dict(object='path-B', point='', field='transform.tx'))['result']['evaluated'] == 1720,
                'One Undo removes only target-C Grid alignment')
            revision = undo_grid_c['revision']
            undo_grid_b = core('undo', expected_revision=revision)
            check(undo_grid_b['ok'] and undo_grid_b['revision'] == revision + 1 and
                compare('get', ref=dict(object='path-B', point='', field='transform.tx'))['result']['evaluated'] == 0,
                'Second Undo restores the original independent alignment probe')
            revision = undo_grid_b['revision']

            for grid_id, expected_minima in (
                    ('template-grid-target-b', (2002.5, 2195.0, 2387.5)),
                    ('template-grid-target-c', (2902.5, 3095.0, 3287.5))):
                distributed = apply_mcp(revision, [dict(type='distribute_objects',
                    objects=['mcp-grid-probe-a', 'mcp-grid-probe-b', 'mcp-grid-probe-c'],
                    axis='x', reference='grid:' + grid_id)])
                check(distributed['ok'] and distributed['revision'] == revision + 1,
                    'Formal MCP distributes three independent objects into inherited Grid ' + grid_id)
                revision = distributed['revision']
                actual_minima = tuple(compare('get', ref=dict(object=f'mcp-grid-probe-{letter}', point='',
                    field='transform.tx'))['result']['evaluated'] for letter in 'abc')
                check(all(abs(actual - expected) < 1e-8 for actual, expected in zip(actual_minima, expected_minima)),
                    'Inherited Grid ' + grid_id + ' distribution matches fixed equal-gap geometry: ' + repr(actual_minima))
                state = compare('inspect')['result']
                source_board = next(item for item in state['compositions'][0]['artboards'] if item['id'] == source_id)
                boards_by_id = {item['id']: item for item in state['compositions'][0]['artboards']}
                check(source_board['layout']['grid'] == SOURCE_GRID and
                    'layout' not in boards_by_id['target-b'] and 'layout' not in boards_by_id['target-c'],
                    'Distribution through ' + grid_id + ' preserves the source and does not materialize local Artboard Grids')

            suppressed_grid = apply_mcp(revision, [dict(type='set_artboard_template_override',
                composition=composition_id, artboard='target-c', field='layout.grid', value=None)])
            check(suppressed_grid['ok'] and suppressed_grid['revision'] == revision + 1,
                'A target-local null override suppresses inherited Grid C through the formal MCP')
            revision = suppressed_grid['revision']
            settled_grid = tool('nect_file', dict(identity, op='save', expected_revision=revision,
                path=str(working_path)))
            check(settled_grid['ok'], 'Flush committed Grid state before the null-suppression refusal probe')
            before_grid_refusal = compare('inspect')
            history_before_grid_refusal = compare('history')
            native_before_grid_refusal = working_path.read_bytes()
            hash_before_grid_refusal = hashlib.sha256(native_before_grid_refusal).hexdigest()
            missing_grid_command = [dict(type='align_objects', objects=['path-B'], axis='x',
                alignment='min', reference='grid:template-grid-target-c')]
            missing_grid = apply_mcp(revision, missing_grid_command)
            missing_grid_api = direct(dict(op='apply', expected_revision=revision, commands=missing_grid_command))
            check(missing_grid == missing_grid_api and not missing_grid['ok'] and
                missing_grid['error']['code'] == 'MISSING_GRID' and
                missing_grid['revision'] == revision and compare('inspect') == before_grid_refusal and
                compare('history') == history_before_grid_refusal and
                working_path.read_bytes() == native_before_grid_refusal and
                hashlib.sha256(working_path.read_bytes()).hexdigest() == hash_before_grid_refusal,
                'Null-suppressed inherited Grid refusal preserves exact typed state, revision, History and native bytes')
            reset_grid_c = apply_mcp(revision, [dict(type='reset_artboard_template_override',
                composition=composition_id, artboard='target-c', field='layout.grid')])
            check(reset_grid_c['ok'] and reset_grid_c['revision'] == revision + 1,
                'Reset restores target-C Grid inheritance after the negative MCP probe')
            revision = reset_grid_c['revision']

            settled_before_duplicate = tool('nect_file', dict(identity, op='save', expected_revision=revision,
                path=str(working_path)))
            check(settled_before_duplicate['ok'] and source_path.read_bytes() == original_bytes,
                'Settle the isolated working file before duplicating the assigned Template Artboard')
            before_duplicate = compare('inspect')['result']
            duplicate_command = dict(type='duplicate_template_artboard', composition=composition_id,
                artboard='target-a', id_prefix='mcp-copy', x=3600, y=200, index=4)
            duplicated = apply_mcp(revision, [duplicate_command])
            check(duplicated['ok'] and duplicated['revision'] == revision + 1,
                'Formal MCP duplicates one assigned Template Artboard in a single Session revision: ' + repr(duplicated))
            revision = duplicated['revision']
            after_duplicate = compare('inspect')['result']
            duplicate_comp = next(item for item in after_duplicate['compositions'] if item['id'] == composition_id)
            duplicate_board = next(item for item in duplicate_comp['artboards'] if item['id'] == 'mcp-copy-artboard')
            duplicate_template = duplicate_board['template_assignment']
            duplicate_guides = duplicate_board['local_guides']
            duplicate_content = next(item for item in after_duplicate['objects'] if item['id'] == 'mcp-copy-content-1')
            original_content = next(item for item in after_duplicate['objects'] if item['id'] == 'content-a')
            original_board = next(item for item in duplicate_comp['artboards'] if item['id'] == 'target-a')
            before_duplicate_comp = next(item for item in before_duplicate['compositions'] if item['id'] == composition_id)
            before_duplicate_board = next(item for item in before_duplicate_comp['artboards'] if item['id'] == 'target-a')
            before_duplicate_content = next(item for item in before_duplicate['objects'] if item['id'] == 'content-a')
            duplicate_grid = duplicate_board['layout']['grid']
            expected_duplicate_grid = copy.deepcopy(original_board['layout']['grid'])
            expected_duplicate_grid['id'] = 'mcp-copy-grid'
            duplicate_guide_positions = {item['guide_id']: item['position']
                for item in duplicate_template['guide_position_overrides']}
            check((duplicate_board['x'], duplicate_board['y']) == (3600, 200) and
                duplicate_template['template_id'] == 'mcp-template-main' and
                duplicate_template['content_instance'] == 'mcp-copy-content-1' and
                duplicate_template['grid_id'] == 'mcp-copy-grid' and duplicate_grid == expected_duplicate_grid and
                duplicate_board['layout']['margin'] == original_board['layout']['margin'] and
                duplicate_template['width_override'] == original_board['template_assignment']['width_override'] and
                [item['id'] for item in duplicate_comp['artboards']] ==
                    ['art-main', 'target-a', 'target-b', 'target-c', 'mcp-copy-artboard'] and
                original_board == before_duplicate_board and original_content == before_duplicate_content,
                'Duplicate preserves source/Template IDs while applying exact x/y placement and fresh Artboard/Content/Grid IDs')
            check([guide['id'] for guide in duplicate_guides] == ['mcp-copy-guide-1'] and
                duplicate_guides[0]['name'] == 'Local A' and duplicate_guides[0]['position'] == 15 and
                duplicate_guide_positions == {'source-guide-x': 90, 'GX': 90},
                'Duplicate allocates fresh local Guide identity and preserves stable inherited Guide overrides')
            duplicate_override_fields = {item['target']['field'] for item in duplicate_content['instance']['overrides']}
            check(duplicate_content['instance']['definition'] == 'mcp-template-definition' and
                duplicate_content['instance']['overrides'] == original_content['instance']['overrides'] and
                duplicate_override_fields == {'generator.width', 'transform.tx'} and
                next(item for item in duplicate_comp['templates'] if item['id'] == 'mcp-template-main')['source_artboard'] == source_id and
                next(item for item in after_duplicate['definitions'] if item['id'] == 'mcp-template-definition')['root'] ==
                    'mcp-template-root',
                'Duplicate preserves source Definition and both exact descendant R04 overrides without changing their source IDs')
            duplicate_frame = next(item for item in compare('artboards', composition=composition_id)['result']
                if item['authored']['id'] == 'mcp-copy-artboard')
            check(compare('get', ref=dict(object='mcp-copy-content-1', point='', field='transform.tx'))['result']['evaluated'] == 3600 and
                compare('get', ref=dict(object='mcp-copy-content-1', point='', field='transform.ty'))['result']['evaluated'] == 200 and
                duplicate_frame['evaluated']['height'] == 600,
                'Fresh duplicate content placement and inherited evaluated frame are exact through MCP/API readback')

            undo_duplicate = core('undo', expected_revision=revision)
            check(undo_duplicate['ok'] and undo_duplicate['revision'] == revision + 1 and
                compare('inspect')['result'] == before_duplicate,
                'One MCP Host Undo removes all generated duplicate identities and restores exact prior state')
            revision = undo_duplicate['revision']
            redo_duplicate = core('redo', expected_revision=revision)
            check(redo_duplicate['ok'] and redo_duplicate['revision'] == revision + 1 and
                compare('inspect')['result'] == after_duplicate,
                'One MCP Host Redo restores exact duplicate IDs, placement, source refs and overrides')
            revision = redo_duplicate['revision']

            settled_duplicate = tool('nect_file', dict(identity, op='save', expected_revision=revision,
                path=str(working_path)))
            check(settled_duplicate['ok'], 'Flush duplicate result before the atomic collision refusal probe')
            duplicate_history = compare('history')
            duplicate_bytes = working_path.read_bytes()
            duplicate_hash = hashlib.sha256(duplicate_bytes).hexdigest()
            collision_commands = [duplicate_command]
            collision = apply_mcp(revision, collision_commands)
            collision_api = direct(dict(op='apply', expected_revision=revision, commands=collision_commands))
            check(collision == collision_api and not collision['ok'] and collision['error']['code'] == 'DUPLICATE_ID' and
                collision['revision'] == revision and
                compare('inspect')['result'] == after_duplicate and compare('history') == duplicate_history and
                working_path.read_bytes() == duplicate_bytes and
                hashlib.sha256(working_path.read_bytes()).hexdigest() == duplicate_hash,
                'Duplicate ID collision is atomic across MCP/API state, revision, History and working native bytes')

            settled_before_refusal = tool('nect_file', dict(identity, op='save', expected_revision=revision,
                path=str(working_path)))
            check(settled_before_refusal['ok'], 'Flush the current working native file before atomic failure checks')
            before_refusal = compare('inspect')
            history_before = compare('history')
            current_native = working_path.read_bytes()
            current_hash = hashlib.sha256(current_native).hexdigest()
            rejected = apply_mcp(revision, [dict(
                type='set_artboard_template_override', composition=composition_id, artboard='target-a',
                field='object.opacity', value=.5)])
            check(not rejected['ok'] and rejected['error']['code'] == 'UNSUPPORTED_TEMPLATE_OVERRIDE' and
                rejected['revision'] == revision and compare('inspect') == before_refusal and
                compare('history') == history_before and working_path.read_bytes() == current_native and
                hashlib.sha256(working_path.read_bytes()).hexdigest() == current_hash and
                source_path.read_bytes() == original_bytes,
                'Unsupported MCP domain refusal preserves typed native state, revision, History and both source/working bytes')
            stale = direct(dict(op='apply', expected_revision=revision - 1, commands=[dict(
                type='set_artboard_template_override', composition=composition_id, artboard='target-a',
                field='frame.width', value=950)]))
            check(not stale['ok'] and stale['error']['code'] == 'REVISION_CONFLICT' and stale['revision'] == revision and
                compare('inspect') == before_refusal and compare('history') == history_before and
                working_path.read_bytes() == current_native and source_path.read_bytes() == original_bytes,
                'Stale MCP revision refusal is identical to canonical API and preserves source/working native bytes and History')
            in_use = apply_mcp(revision, [dict(
                type='delete_artboard_template', composition=composition_id, template='mcp-template-main')])
            check(not in_use['ok'] and in_use['error']['code'] == 'ARTBOARD_TEMPLATE_IN_USE' and
                in_use['revision'] == revision and compare('inspect') == before_refusal and
                compare('history') == history_before and working_path.read_bytes() == current_native and
                source_path.read_bytes() == original_bytes,
                'Assigned Template source cannot be deleted and failure is atomic across API, History and native bytes')

            detached = apply_mcp(revision, [
                dict(type='detach_artboard_template', composition=composition_id, artboard='target-c',
                    id_prefix='mcp-template-detached'),
                dict(type='delete_artboard_template', composition=composition_id, template='mcp-template-probe')])
            check(detached['ok'] and detached['revision'] == revision + 1,
                'Probe Template detaches and deletes through a successful formal MCP lifecycle batch')
            revision = detached['revision']
            after_detach = compare('inspect')['result']
            detached_comp = next(item for item in after_detach['compositions'] if item['id'] == composition_id)
            check([item['id'] for item in detached_comp['templates']] == ['mcp-template-main'] and
                'template_assignment' not in next(item for item in detached_comp['artboards']
                    if item['id'] == 'target-c'),
                'Delete removes the exact detached probe Template while assigned A/B ownership remains')

            saved_projection = compare('export_svg', composition=composition_id, artboard='target-a')['result']
            saved_projection_paths = svg_source_paths(saved_projection, 'Shared mark')
            check(any(all(abs(actual-expected) < 1e-7 for actual, expected in zip(path['bounds'], expected_bounds)) and
                path['fill'] == 'rgb(25%,30%,10%)' for path in saved_projection_paths),
                'Final saved-state projection retains the fixed target-A geometry and source Fill oracle: ' + repr(saved_projection_paths))

            settled_for_save_as = tool('nect_file', dict(identity, op='save', expected_revision=revision,
                path=str(working_path)))
            check(settled_for_save_as['ok'], 'Flush exact current document to its isolated working destination before final Save As')
            working_before_save_as = working_path.read_bytes()
            working_before_save_as_sha = hashlib.sha256(working_before_save_as).hexdigest()
            destination_saved = tool('nect_file', dict(identity, op='save', expected_revision=revision,
                path=str(destination_path)))
            check(destination_saved['ok'] and destination_path.exists(),
                'Formal MCP nect_file performs Host Save As to a separate owned scratch native destination')
            saved_bytes = destination_path.read_bytes()
            saved_sha = hashlib.sha256(saved_bytes).hexdigest()
            check(source_path.read_bytes() == original_bytes and
                hashlib.sha256(source_path.read_bytes()).hexdigest() == original_sha and
                working_path.read_bytes() == working_before_save_as and
                hashlib.sha256(working_path.read_bytes()).hexdigest() == working_before_save_as_sha,
                'Final Host Save As preserves both the original input and settled working native bytes exactly')
            saved_native = json.loads(saved_bytes.decode('utf-8'))
            schema = json.loads((ROOT / f'schemas/native-v{NATIVE_VERSION}.schema.json').read_text(encoding='utf-8'))
            check(schema['$id'] == f'urn:nect:native:{NATIVE_VERSION}' and
                schema['title'] == f'Nect native v{NATIVE_VERSION}',
                'Current native schema parses and identifies the canonical writer version')
            try:
                import jsonschema
            except ImportError:
                print('SCHEMA_VALIDATOR=NOT_RUN (jsonschema unavailable; no dependency installed)')
            else:
                jsonschema.Draft202012Validator.check_schema(schema)
                errors = list(jsonschema.Draft202012Validator(schema).iter_errors(saved_native))
                check(not errors, 'Available Draft 2020-12 validator accepts the actual Host Save As native document: ' +
                    repr([error.message for error in errors[:3]]))
            saved_comp = next(item for item in saved_native['compositions'] if item['id'] == composition_id)
            saved_boards = {item['id']: item for item in saved_comp['artboards']}
            saved_objects = {item['id']: item for item in saved_native['objects']}
            check(saved_native['version'] == NATIVE_VERSION and
                saved_boards['target-a']['template_assignment']['content_instance'] == 'content-a' and
                saved_boards['target-b']['template_assignment']['content_instance'] == 'content-b' and
                saved_boards['target-c'].get('template_assignment') is None and
                {item['id'] for item in saved_comp['templates']} == {'mcp-template-main'} and
                {entry['target']['field'] for entry in saved_objects['content-a']['instance']['overrides']} == override_fields and
                saved_objects['mcp-template-rectangle']['stack'][0]['operation']['parameters']['r']['literal'] == .25 and
                {guide['id'] for guide in saved_boards['art-main']['local_guides']} == {'source-guide-x','source-guide-y','GX'} and
                [guide['id'] for guide in saved_boards['target-a']['local_guides']] == ['local-guide-a'] and
                {(entry['guide_id'], entry['position']) for entry in
                    saved_boards['target-a']['template_assignment']['guide_position_overrides']} ==
                    {('source-guide-x',90),('GX',90)} and
                {(entry['guide_id'], entry['enabled']) for entry in
                    saved_boards['target-b']['template_assignment']['guide_enabled_overrides']} ==
                    {('GX',False)} and
                [guide['id'] for guide in saved_boards['target-b']['local_guides']] == ['detached-guide-b-y'] and
                saved_boards['target-b']['local_guides'][0]['position'] == 80 and
                saved_boards['target-b']['local_guides'][0]['enabled'] is False and
                saved_boards['target-b']['template_assignment']['detached_guides'] == ['source-guide-y'] and
                {guide['position'] for guide in saved_boards['target-c']['local_guides']} == {40,50,80} and
                saved_boards['mcp-copy-artboard']['x'] == 3600 and saved_boards['mcp-copy-artboard']['y'] == 200 and
                saved_boards['mcp-copy-artboard']['template_assignment']['content_instance'] == 'mcp-copy-content-1' and
                saved_boards['mcp-copy-artboard']['template_assignment']['grid_id'] == 'mcp-copy-grid' and
                saved_boards['mcp-copy-artboard']['local_guides'][0]['id'] == 'mcp-copy-guide-1' and
                saved_objects['mcp-copy-content-1']['instance']['definition'] == 'mcp-template-definition',
                'Save As bytes retain exact native Guide authoring, overrides, item detach and full detach beside Template/R04 state')
            check(saved_native == after_detach,
                'Save As native bytes exactly equal the canonical typed authored Document readback')

            stop(mcp);mcp=None
            stop(desktop);desktop=None
            endpoint = 'nect-template-cold-' + uuid.uuid4().hex
            desktop = start(DESKTOP_EXE, endpoint, temp / 'cold-recovery', temp / 'cold-ready.json', destination_path)
            mcp = connect(endpoint);sequence=0
            check(rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},
                clientInfo=dict(name='template-cold-open',version='1')))['result']['protocolVersion']=='2025-06-18',
                'Fresh Desktop Host process initializes the formal MCP endpoint')
            mcp.stdin.write(json.dumps(dict(jsonrpc='2.0',method='notifications/initialized'))+'\n');mcp.stdin.flush()
            cold_live=tool('nect_session');identity={key:cold_live[key] for key in ('session_id','document_id')}
            check(identity['document_id']=='document-demo' and identity['session_id']!=live['session_id'],
                'Cold Host has a fresh Session identity for the Save As document')
            cold_inspect=compare('inspect')['result']
            check(cold_inspect == saved_native,
                'Fresh Host inspect exactly matches every authored native field written by Save As')
            cold_a=next(item for item in cold_inspect['objects'] if item['id']=='content-a')
            cold_comp=next(item for item in cold_inspect['compositions'] if item['id']==composition_id)
            check(cold_a['instance']['definition']=='mcp-template-definition' and
                {entry['target']['field'] for entry in cold_a['instance']['overrides']}==override_fields and
                {item['id'] for item in cold_comp['templates']}=={'mcp-template-main'},
                'Fresh Host reopen retains assigned Definition content, local descendant overrides and exact Template IDs')
            cold_copy=next(item for item in cold_comp['artboards'] if item['id']=='mcp-copy-artboard')
            cold_copy_instance=next(item for item in cold_inspect['objects'] if item['id']=='mcp-copy-content-1')
            check((cold_copy['x'],cold_copy['y'])==(3600,200) and
                cold_copy['template_assignment']['template_id']=='mcp-template-main' and
                cold_copy['template_assignment']['grid_id']=='mcp-copy-grid' and
                cold_copy['template_assignment']['guide_position_overrides']==
                    next(item for item in cold_comp['artboards'] if item['id']=='target-a')['template_assignment']['guide_position_overrides'] and
                cold_copy_instance['instance']['definition']=='mcp-template-definition' and
                cold_copy_instance['instance']['overrides']==cold_a['instance']['overrides'] and
                cold_copy['local_guides'][0]['id']=='mcp-copy-guide-1',
                'Fresh Host cold reopen retains duplicate placement, fresh IDs, source refs, local Guides and exact R04 overrides')
            cold_frames=compare('artboards',composition=composition_id)['result']
            cold_by_id={item['authored']['id']:item for item in cold_frames}
            check(cold_by_id['target-a']['evaluated']==frames['target-a']['evaluated'] and
                cold_by_id['target-b']['evaluated']==frames['target-b']['evaluated'] and
                'template_assignment' not in cold_by_id['target-c']['authored'],
                'Fresh Host reopen preserves authored/evaluated layout for A/B and detached C state')
            cold_projection=compare('export_svg',composition=composition_id,artboard='target-a')['result']
            cold_paths=svg_source_paths(cold_projection,'Shared mark')
            check(cold_projection == saved_projection,
                'Fresh Host returns the exact same projected SVG after native cold reopen')
            check(any(
                all(abs(actual-expected)<1e-7 for actual,expected in zip(path['bounds'],expected_bounds)) and
                path['fill']=='rgb(25%,30%,10%)' for path in cold_paths),
                'Fresh Host projects fixed numeric geometry and propagated source Fill from assigned content: ' + repr(cold_paths))
            check(destination_path.read_bytes()==saved_bytes and
                hashlib.sha256(destination_path.read_bytes()).hexdigest()==saved_sha and
                source_path.read_bytes()==original_bytes and working_path.read_bytes()==working_before_save_as,
                'Cold open leaves saved destination, original source and working destination bytes unchanged')
            print(f'PASS formal MCP Template/API/Save As/cold Host parity ({checks} assertions)')
        finally:
            stop(mcp);mcp=None
            stop(desktop);desktop=None


if __name__ == '__main__':
    main()

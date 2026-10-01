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
sys.path.insert(0, str(ROOT / 'scripts'))
from session_client import call as desktop_api_call

DESKTOP_EXE = str(Path(sys.argv[1]).resolve())
CLI_EXE = str(Path(sys.argv[2]).resolve())
desktop = mcp = None
sequence = 0
identity = {}
endpoint = ''
checks = 0


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
    check(demo.returncode == 0, 'Rebuilt CLI creates a native 0.76 starting fixture')
    sample = json.loads(demo.stdout)
    check(sample['version'] == '0.76', 'Fixture is native version 0.76')
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
        grid = dict(id='source-grid', bounds=dict(x=20, y=20, width=760, height=560),
            columns=2, rows=2, column_gutter=20, row_gutter=20)
        setup = request({'op': 'apply', 'expected_revision': 0, 'commands': [
            dict(type='update_artboard', composition=composition_id, artboard=source),
            dict(type='add_artboard', composition=composition_id, artboard=board_a, index=1),
            dict(type='add_artboard', composition=composition_id, artboard=board_b, index=2),
            dict(type='add_artboard', composition=composition_id, artboard=board_c, index=3),
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
        check(authored['version'] == '0.76' and authored['compositions'][0]['id'] == composition_id and
            len(authored['compositions'][0]['artboards']) == 4 and
            authored['definitions'][0]['id'] == 'mcp-template-definition',
            'Native fixture readback contains the exact authored Composition, four Artboards and Definition')
        path.write_text(json.dumps(authored, separators=(',', ':')), encoding='utf-8')
        validation = run(CLI_EXE, '--validate', input=path.read_text(encoding='utf-8'))
        check(validation.returncode == 0 and json.loads(validation.stdout)['native_version'] == '0.76',
            'Persisted canonical Session readback validates as native 0.76 before Desktop Host open')
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
                    'detach_artboard_template'):
                check(operation in description, 'Formal MCP description advertises Template lifecycle operation ' + operation)
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

            commands = [
                dict(type='create_artboard_template', composition=composition_id,
                    id='mcp-template-main', name='Shared source', source_artboard=source_id,
                    definition='mcp-template-definition'),
                dict(type='create_artboard_template', composition=composition_id,
                    id='mcp-template-probe', name='Probe source', source_artboard=source_id, definition=None),
                dict(type='rename_artboard_template', composition=composition_id,
                    template='mcp-template-probe', name='Renamed probe'),
                dict(type='assign_artboard_template', composition=composition_id, artboard='target-a',
                    template='mcp-template-main', content_instance='content-a'),
                dict(type='assign_artboard_template', composition=composition_id, artboard='target-b',
                    template='mcp-template-main', content_instance='content-b'),
                dict(type='assign_artboard_template', composition=composition_id, artboard='target-c',
                    template='mcp-template-probe', content_instance=None),
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
                'All seven lifecycle tags and frame/Margin/Grid set/reset execute through one formal MCP mutation')
            revision = created['revision']

            inspect = compare('inspect')['result']
            comp = next(item for item in inspect['compositions'] if item['id'] == composition_id)
            templates = {item['id']: item for item in comp['templates']}
            check(templates['mcp-template-main']['source_artboard'] == source_id and
                templates['mcp-template-main']['definition'] == 'mcp-template-definition' and
                templates['mcp-template-probe']['name'] == 'Renamed probe',
                'Typed readback retains exact source Artboard, Definition and renamed stable Template IDs')
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
                    target=dict(object='mcp-template-rectangle', point='', field='transform.tx'), value=33)])
            check(geometry_edit['ok'] and geometry_edit['revision'] == revision + 1,
                'The two bounded descendant geometry Scalars apply through the formal MCP: ' + repr(geometry_edit))
            revision = geometry_edit['revision']
            geometry_projection = compare('export_svg', composition=composition_id, artboard='target-a')['result']
            geometry_paths = svg_source_paths(geometry_projection, 'Shared mark')
            expected_bounds = (912.0, -70.0, 954.0, 70.0)
            check(geometry_projection != before_projection and any(
                all(abs(actual-expected) < 1e-7 for actual, expected in zip(path['bounds'], expected_bounds))
                and path['fill'] == 'rgb(80%,30%,10%)' for path in geometry_paths),
                'Actual Host SVG numeric geometry oracle mismatch: ' + repr(geometry_paths))

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

            before_refusal = compare('inspect')
            history_before = compare('history')
            current_native = source_path.read_bytes()
            current_hash = hashlib.sha256(current_native).hexdigest()
            rejected = compare('apply', expected_revision=revision, commands=[dict(
                type='set_artboard_template_override', composition=composition_id, artboard='target-a',
                field='object.opacity', value=.5)])
            check(not rejected['ok'] and rejected['error']['code'] == 'UNSUPPORTED_TEMPLATE_OVERRIDE' and
                rejected['revision'] == revision and compare('inspect') == before_refusal and
                compare('history') == history_before and source_path.read_bytes() == current_native and
                hashlib.sha256(source_path.read_bytes()).hexdigest() == current_hash,
                'Unsupported MCP domain refusal preserves typed native state, revision, History and source bytes')
            stale = compare('apply', expected_revision=revision - 1, commands=[dict(
                type='set_artboard_template_override', composition=composition_id, artboard='target-a',
                field='frame.width', value=950)])
            check(not stale['ok'] and stale['error']['code'] == 'REVISION_CONFLICT' and stale['revision'] == revision and
                compare('inspect') == before_refusal and compare('history') == history_before and
                source_path.read_bytes() == current_native,
                'Stale MCP revision refusal is identical to canonical API and preserves native bytes/History')
            in_use = compare('apply', expected_revision=revision, commands=[dict(
                type='delete_artboard_template', composition=composition_id, template='mcp-template-main')])
            check(not in_use['ok'] and in_use['error']['code'] == 'ARTBOARD_TEMPLATE_IN_USE' and
                in_use['revision'] == revision and compare('inspect') == before_refusal and
                compare('history') == history_before and source_path.read_bytes() == current_native,
                'Assigned A/B source cannot be deleted and failure is atomic across API, History and native bytes')

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

            destination_saved = tool('nect_file', dict(identity, op='save', expected_revision=revision,
                path=str(destination_path)))
            check(destination_saved['ok'] and destination_path.exists(),
                'Formal MCP nect_file performs Host Save As to a separate owned scratch native destination')
            saved_bytes = destination_path.read_bytes()
            saved_sha = hashlib.sha256(saved_bytes).hexdigest()
            check(source_path.read_bytes() == original_bytes and
                hashlib.sha256(source_path.read_bytes()).hexdigest() == original_sha,
                'Host Save As preserves the original input native bytes exactly')
            saved_native = json.loads(saved_bytes.decode('utf-8'))
            schema = json.loads((ROOT / 'schemas/native-v0.76.schema.json').read_text(encoding='utf-8'))
            check(schema['$id'] == 'urn:nect:native:0.76' and schema['title'] == 'Nect native v0.76',
                'Current native schema parses and identifies version 0.76')
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
            check(saved_native['version'] == '0.76' and
                saved_boards['target-a']['template_assignment']['content_instance'] == 'content-a' and
                saved_boards['target-b']['template_assignment']['content_instance'] == 'content-b' and
                saved_boards['target-c'].get('template_assignment') is None and
                {item['id'] for item in saved_comp['templates']} == {'mcp-template-main'} and
                {entry['target']['field'] for entry in saved_objects['content-a']['instance']['overrides']} == override_fields and
                saved_objects['mcp-template-rectangle']['stack'][0]['operation']['parameters']['r']['literal'] == .25,
                'Save As bytes retain assigned A/B, detached C, source Fill and exact R04 local override fields')
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
            cold_frames=compare('artboards',composition=composition_id)['result']
            cold_by_id={item['authored']['id']:item for item in cold_frames}
            check(cold_by_id['target-a']['evaluated']==frames['target-a']['evaluated'] and
                cold_by_id['target-b']['evaluated']==frames['target-b']['evaluated'] and
                'template_assignment' not in cold_by_id['target-c']['authored'],
                'Fresh Host reopen preserves authored/evaluated layout for A/B and detached C state')
            cold_projection=compare('export_svg',composition=composition_id,artboard='target-a')['result']
            cold_paths=svg_source_paths(cold_projection,'Shared mark')
            check(cold_projection == after_projection,
                'Fresh Host returns the exact same projected SVG after native cold reopen')
            check(any(
                all(abs(actual-expected)<1e-7 for actual,expected in zip(path['bounds'],expected_bounds)) and
                path['fill']=='rgb(25%,30%,10%)' for path in cold_paths),
                'Fresh Host projects fixed numeric geometry and propagated source Fill from assigned content: ' + repr(cold_paths))
            check(destination_path.read_bytes()==saved_bytes and
                hashlib.sha256(destination_path.read_bytes()).hexdigest()==saved_sha,
                'Cold open leaves the exact saved destination bytes unchanged')
            print(f'PASS formal MCP Template/API/Save As/cold Host parity ({checks} assertions)')
        finally:
            stop(mcp);mcp=None
            stop(desktop);desktop=None


if __name__ == '__main__':
    main()

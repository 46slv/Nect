"""Focused actual desktop / formal MCP adoption, edit, Undo and cold restart."""
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
from session_client import call

exe = str(Path(sys.argv[1]).resolve())
checks = 0


def check(value, message):
    global checks
    assert value, message
    checks += 1


class Live:
    def __init__(self, directory, native=None):
        self.endpoint = 'nect-contour-' + uuid.uuid4().hex
        ready = directory / (self.endpoint + '.json')
        args = [exe, '--automation-endpoint', self.endpoint, '--ready-file', str(ready),
                '--recovery-dir', str(directory / self.endpoint)]
        if native:
            args.append(str(native))
        self.desktop = subprocess.Popen(args, env=dict(os.environ, QT_QPA_PLATFORM='offscreen'),
                                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.mcp = None
        try:
            deadline = time.monotonic() + 20
            while not ready.exists():
                check(self.desktop.poll() is None and time.monotonic() < deadline, 'desktop load')
                time.sleep(.05)
            self.mcp = subprocess.Popen([sys.executable, str(ROOT / 'scripts/mcp_server.py'),
                                         '--endpoint', self.endpoint], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
            self.seq = 0
            self.rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={},
                                       clientInfo=dict(name='contour-smoke', version='1')))
            self.mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', method='notifications/initialized')) + '\n')
            self.mcp.stdin.flush()
            live = self.tool('nect_session', {})
            self.identity = {key: live[key] for key in ('session_id', 'document_id')}
        except BaseException:
            self.close()
            raise

    def rpc(self, method, params=None):
        self.seq += 1
        self.mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', id=self.seq, method=method, params=params or {})) + '\n')
        self.mcp.stdin.flush()
        result = json.loads(self.mcp.stdout.readline())
        assert 'error' not in result, result
        return result['result']

    def tool(self, name, arguments):
        result = self.rpc('tools/call', dict(name=name, arguments=arguments))
        value = result['structuredContent']
        assert json.loads(result['content'][0]['text']) == value
        return value

    def core(self, op, **values):
        result = self.tool('nect_command', dict(self.identity, request=dict(op=op, **values)))
        assert result['ok'], result
        return result

    def close(self):
        if self.mcp:
            self.mcp.stdin.close()
            try:
                self.mcp.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.mcp.kill(); self.mcp.wait()
        if self.desktop.poll() is None:
            self.desktop.kill()
        self.desktop.wait(timeout=5)


with tempfile.TemporaryDirectory(prefix='nect-contour-') as name:
    directory = Path(name)
    live = Live(directory)
    try:
        tools = live.rpc('tools/list')['tools']
        check(any(t['name'] == 'nect_adopt_analysis_contour' for t in tools), 'MCP discovery')
        comp = live.core('inspect')['result']['compositions'][0]
        def point(i, x, y):
            return dict(id='source-p' + str(i), **{k: dict(literal=v) for k, v in
                        dict(x=x, y=y, in_angle=0, out_angle=0, in_length=0, out_length=0).items()})
        command = dict(type='create_path', composition=comp['id'], parent='', id='source', name='Source',
                       contours=[dict(id='source-c', closed=True,
                                      points=[point(i, *xy) for i, xy in enumerate([(20, 20), (40, 20), (40, 35), (20, 35)])])])
        revision = live.core('apply', expected_revision=0, commands=[command])['revision']
        source = live.core('inspect')['result']
        query = dict(live.identity, op='analyze_regions', expected_revision=revision,
                     composition=comp['id'], artboard=comp['artboards'][0]['id'], scale=1, threshold=128)
        analysis = live.tool('nect_analyze_regions', query)
        check(analysis['ok'], analysis)
        check(analysis == call(live.endpoint, query), 'API / formal MCP exact analysis parity')
        check(source == live.core('inspect')['result'], 'Read-only analysis')
        contour = analysis['result']['outer_contours'][0]
        adopt = dict(query, op='adopt_analysis_contour', analysis_id=analysis['result']['analysis_id'],
                     contour_id=contour['id'], name='Adopted contour')
        failure = live.tool('nect_adopt_analysis_contour', dict(adopt, contour_id='wrong'))
        check(not failure['ok'] and source == live.core('inspect')['result'], 'Mismatched selector is atomic')
        adopted = live.tool('nect_adopt_analysis_contour', adopt)
        check(adopted['ok'] and adopted['revision'] == revision + 1, adopted)
        document = live.core('inspect')['result']
        object_id = adopted['result']['object_id']
        point_id = adopted['result']['point_ids'][0]
        source_after = next(o for o in document['objects'] if o['id'] == 'source')
        check(source_after == source['objects'][0], 'Original artwork exact preservation')
        ref = dict(object=object_id, point=point_id, field='x')
        value = live.core('get', ref=ref)['result']['evaluated']
        edited = live.core('apply', expected_revision=adopted['revision'],
                           commands=[dict(type='set', ref=ref, value=value+1)])
        check(live.core('get', ref=ref)['result']['evaluated'] == value+1, 'Point edit')
        undone_edit = live.core('undo', expected_revision=edited['revision'])
        check(document == live.core('inspect')['result'], 'Undo point edit')
        undone_adoption = live.core('undo', expected_revision=undone_edit['revision'])
        check(source == live.core('inspect')['result'], 'One adoption Undo')
        restored = live.core('redo', expected_revision=undone_adoption['revision'])
        check(document == live.core('inspect')['result'], 'Redo retains IDs')
        native = directory / 'adopted.nect'
        saved = live.tool('nect_file', dict(live.identity, op='save', expected_revision=restored['revision'], path=str(native)))
        check(saved['ok'] and native.exists(), 'Save native')
        saved_bytes = native.read_bytes()
    finally:
        live.close()
    cold = Live(directory, native)
    try:
        check(document == cold.core('inspect')['result'], 'Fresh desktop process exact cold reopen')
        check(native.read_bytes() == saved_bytes, 'Cold reopen preserves native bytes')
        check(cold.core('get', ref=ref)['result']['evaluated'] == value, 'Stable authored Ref survives restart')
        cold.core('apply', expected_revision=0, commands=[dict(type='set', ref=ref, value=value+2)])
        check(cold.core('get', ref=ref)['result']['evaluated'] == value+2, 'Cold reopened Path remains editable')
    finally:
        cold.close()
print(f'PASS {checks} desktop/MCP contour checks (including startup observations)')

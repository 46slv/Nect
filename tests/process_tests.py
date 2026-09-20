"""Black-box CLI tests with hand-specified expectations."""
import json
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

exe = str(Path(sys.argv[1]).resolve())
checks = 0

def run(mode, value=None):
    return subprocess.run([exe, mode], input=None if value is None else json.dumps(value),
                          text=True, capture_output=True, timeout=10)

def check(value, message):
    global checks
    if not value:
        raise AssertionError(message)
    checks += 1

sample = json.loads(run('--demo').stdout)
check(run('--validate', sample).returncode == 0, 'demo validates in new process')

future = dict(sample, version='999')
check(run('--validate', future).returncode == 2, 'future schema rejected')

unknown = dict(sample, secret_extension={})
check('UNKNOWN_FIELD' in run('--validate', unknown).stderr, 'unknown field rejected')

wrong_color = dict(sample, color_space='cmyk')
check('UNSUPPORTED_COLOR_OR_UNIT' in run('--validate', wrong_color).stderr, 'CMYK not pretended supported')

svg = run('--svg', sample)
root = ET.fromstring(svg.stdout)
paths = root.findall('.//{http://www.w3.org/2000/svg}path')
check(len(paths) == 2, 'independent XML parser finds both paths')
check(paths[0].attrib['d'].startswith('M 100 150 C '), 'exact first anchor')
check(root.attrib['viewBox'] == '0 0 640 480', 'explicit artboard extent')

with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / 'drawing.nect.json'
    source.write_text(json.dumps(sample), encoding='utf-8')
    ref = dict(object='path-A', point='point-A1', field='x')
    requests = [
        dict(op='capabilities'),
        dict(op='apply', expected_revision=0, commands=[dict(type='set', ref=ref, value=180)]),
        dict(op='inspect'),
        dict(op='undo', expected_revision=1),
        dict(op='inspect'),
        dict(op='execute_shell', command='not-allowed'),
        dict(op='inspect'),
    ]
    p = subprocess.run([exe, '--serve', str(source)],
        input='\n'.join(json.dumps(r) for r in requests)+'\n',
        text=True, capture_output=True, timeout=10)

    check(p.returncode == 0, 'serve process exits normally')
    responses = [json.loads(x) for x in p.stdout.splitlines()]
    check(len(responses)==7, 'one response per request')
    check(responses[0]['result']['mcp'] is False, 'JSON transport does not pretend to be MCP')
    check(responses[2]['result']['objects'][0]['contours'][0]['points'][0]['x']['literal']==180,
          'mutation visible through API')
    check(responses[4]['result']['objects'][0]['contours'][0]['points'][0]['x']['literal']==100,
          'undo visible through API')
    check(responses[5]['error']['code']=='UNSUPPORTED_OPERATION', 'no universal shell')
    check(responses[6]['result']==responses[4]['result'], 'rejected request has no mutation')

    saved = responses[2]['result']
    check(run('--validate', saved).returncode==0, 'edited document reloads in fresh process')
    check('M 180 150' in run('--svg', saved).stdout, 'fresh process export uses persisted edit')

duplicate = subprocess.run(
    [exe, '--serve'], input='{"op":"inspect","op":"apply"}\n',
    text=True, capture_output=True, timeout=10)
check(json.loads(duplicate.stdout)['error']['code']=='DUPLICATE_KEY', 'duplicate JSON keys rejected')

duplicate_escaped = subprocess.run(
    [exe, '--serve'], input=r'{"op":"inspect","o\u0070":"apply"}'+'\n',
    text=True, capture_output=True, timeout=10)
check(json.loads(duplicate_escaped.stdout)['error']['code']=='DUPLICATE_KEY',
      'escaped duplicate keys rejected')

print(f'PASS {checks} process checks')

# Real pre-migration bytes: linked freeform geometry authored by the 0.1 binary.
legacy = json.loads((Path(__file__).parent / 'fixtures/native-v0.1-linked.nect').read_text(encoding='utf-8'))
check(legacy['version'] == '0.1', 'fixture is the historical format')
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / 'legacy.nect'
    path.write_text(json.dumps(legacy), encoding='utf-8')
    requests = [dict(op='inspect'), dict(op='apply', expected_revision=0, commands=[
        dict(type='set', ref=dict(object='path-A', point='point-A1', field='x'), value=321)]),
        dict(op='get', ref=dict(object='path-B', point='point-B1', field='x')),dict(op='inspect')]
    result = subprocess.run([exe,'--serve',str(path)], input='\n'.join(map(json.dumps,requests))+'\n',
        capture_output=True,text=True,timeout=10)
    replies = [json.loads(line) for line in result.stdout.splitlines()]
    check(all(r['ok'] for r in replies), 'legacy migration and edit succeed')
    migrated = replies[0]['result']
    check(migrated == dict(legacy, version='0.2'), 'migration preserves every authored legacy value and reference')
    check(replies[2]['result']['evaluated'] == 341, 'legacy stable binding still evaluates after editing')
    saved = replies[3]['result']
    path.write_text(json.dumps(saved), encoding='utf-8')
    reopened = subprocess.run([exe,'--serve',str(path)],input='{"op":"inspect"}\n',
        capture_output=True,text=True,timeout=10)
    check(json.loads(reopened.stdout)['result'] == saved, 'upgraded native bytes reopen without authored drift')
    check(run('--validate',saved).returncode == 0,'upgraded file validates in fresh process')
print(f'PASS {checks} process and native migration checks')

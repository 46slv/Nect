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

"""Black-box CLI tests with hand-specified expectations."""
import json
import re
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

def remove_migrated_compositing_defaults(document):
    for obj in document['objects']:
        check(obj.pop('visible') is True, 'legacy artwork remains visible')
        check(obj.pop('compositing') == dict(version=1,opacity=dict(literal=1),blend='normal',isolated=False,mask=None), 'legacy appearance stays neutral')
    return document


def remove_migrated_anchor_defaults(document):
    remove_migrated_compositing_defaults(document)
    for obj in document['objects']:
        check(obj.pop('anchor') == [{'literal':0},{'literal':0}], 'legacy anchor defaults to local origin without changing the affine matrix')
        check(obj.pop('transform_parent') is None, 'legacy transform follows structure')
    return document


sample = json.loads(run('--demo').stdout)
check(run('--validate', sample).returncode == 0, 'demo validates in new process')
check(json.loads(run('--validate',sample).stdout)['native_version']==sample['version'],'validation reports the current writer version')

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
    projected = json.loads(json.dumps(migrated))
    projected['version'] = '0.1'
    remove_migrated_anchor_defaults(projected)
    check(projected.pop('named_colors')==[], 'legacy migration does not invent named colors')
    for obj in projected['objects']:
        if obj['kind'] != 'path':
            continue
        paint = obj.pop('stack')[0]
        check(paint['id'] == obj.pop('legacy_stroke'), 'legacy address refers to stable migrated stroke')
        obj['fill'] = 'none'
        obj['stroke'] = dict(rgba=[paint['parameters'][k] for k in ('r','g','b','a')], width=paint['parameters']['width'])
    check(projected == legacy, 'migration preserves every authored legacy value and reference')
    check(replies[2]['result']['evaluated'] == 341, 'legacy stable binding still evaluates after editing')
    saved = replies[3]['result']
    path.write_text(json.dumps(saved), encoding='utf-8')
    reopened = subprocess.run([exe,'--serve',str(path)],input='{"op":"inspect"}\n',
        capture_output=True,text=True,timeout=10)
    check(json.loads(reopened.stdout)['result'] == saved, 'upgraded native bytes reopen without authored drift')
    check(run('--validate',saved).returncode == 0,'upgraded file validates in fresh process')

    primitive = json.loads((Path(__file__).parent / 'fixtures/native-v0.2-primitive.nect').read_text(encoding='utf-8'))
    # Force an ID collision with the default migration paint name, and retain a
    # legacy stroke dependency; migration must allocate, not steal an old ID.
    primitive['objects'][1]['contours'][0]['id'] = 'path-A-stroke'
    primitive['objects'][2]['stroke']['width']['binding'] = dict(
        source=dict(object='path-A',point='',field='stroke.width'),scale=2,offset=1,mode='copy_local_value')
    path.write_text(json.dumps(primitive),encoding='utf-8')
    migrated_run = subprocess.run([exe,'--serve',str(path)],input='{"op":"inspect"}\n'+
        '{"op":"get","ref":{"object":"path-B","point":"","field":"stroke.width"}}\n',
        capture_output=True,text=True,timeout=10)
    result = [json.loads(line) for line in migrated_run.stdout.splitlines()]
    check(all(x['ok'] for x in result),'retained primitive file migrates')
    new = result[0]['result']
    old_circle = next(x for x in primitive['objects'] if x['id']=='circle')
    new_circle = next(x for x in new['objects'] if x['id']=='circle')
    check(new_circle['source']==old_circle['source'] and new_circle['point_edit']==old_circle['point_edit'],
        '0.2 migration preserves generator and corrections verbatim')
    a = next(x for x in new['objects'] if x['id']=='path-A')
    check(a['legacy_stroke']!='path-A-stroke' and a['contours'][0]['id']=='path-A-stroke',
        'new paint identity avoids all existing document identities')
    check(result[1]['result']['evaluated']==5,'legacy stroke binding survives stack migration')
    check(run('--validate',new).returncode==0,'migrated source, correction and stack reopen')
# Real 0.3 production scene must retain its complete procedural state in 0.4.
ornament = Path(__file__).parent.parent / 'examples/radial-ornament.nect'
old = json.loads(ornament.read_text(encoding='utf-8'))
check(old['version']=='0.3','production fixture remains historical 0.3')
migrated_run = subprocess.run([exe,'--serve',str(ornament)],input='{"op":"inspect"}\n',
    capture_output=True,text=True,timeout=10)
new=json.loads(migrated_run.stdout)['result'];remove_migrated_anchor_defaults(new);new['version']='0.3'
check(new.pop('named_colors')==[], '0.3 migration starts with no named colors')
check(new==old,'0.3 migration retains all paint, repeat, binding and correction state')
check(run('--svg',old).stdout==(ornament.with_suffix('.svg')).read_text(encoding='utf-8'),
    'solid 0.3 scene exports identical SVG after migration')
gradient_path=ornament.with_name('gradient-ornament.nect')
old=json.loads(gradient_path.read_text(encoding='utf-8'))
check(old['version']=='0.4','gradient fixture remains historical 0.4')
upgraded=subprocess.run([exe,'--serve',str(gradient_path)],input='{"op":"inspect"}\n',capture_output=True,text=True,timeout=10)
new=json.loads(upgraded.stdout)['result'];remove_migrated_anchor_defaults(new);new['version']='0.4'
check(new.pop('named_colors')==[], '0.4 migration starts with no named colors')
check(new==old,'0.4 migration preserves gradients and their linked stable stops')
check(run('--svg',old).stdout==gradient_path.with_suffix('.svg').read_text(encoding='utf-8'),
    'gradient 0.4 scene exports identical SVG after frame migration')
frames=json.loads(json.dumps(sample));frames['version']='0.5'
frames.pop('named_colors')
remove_migrated_anchor_defaults(frames)
composition=frames['compositions'][0]
composition['artboards'].append(dict(id='requested-crop',name='Crop',x=100,y=50,width=300,height=250))
requested=subprocess.run([exe,'--svg',composition['id'],'requested-crop'],input=json.dumps(frames),capture_output=True,text=True,timeout=10)
check(requested.returncode==0 and ET.fromstring(requested.stdout).attrib['viewBox']=='100 50 300 250',
    'CLI can export a non-first frame by stable Composition/Artboard IDs')
frames_path=ornament.with_name('artboard-studies.nect')
old=json.loads(frames_path.read_text(encoding='utf-8'))
check(old['version']=='0.5','frame fixture remains historical 0.5')
upgraded=subprocess.run([exe,'--serve',str(frames_path)],input='{"op":"inspect"}\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
new=json.loads(upgraded.stdout)['result'];remove_migrated_anchor_defaults(new);new['version']='0.5'
check(new.pop('named_colors')==[], '0.5 migration starts with no named colors')
check(new==old,'0.5 migration preserves ordered frames, inheritance and all authored artwork')
text_path=ornament.with_name('typography-poster.nect')
old=json.loads(text_path.read_text(encoding='utf-8'))
check(old['version']=='0.6','Text fixture remains historical 0.6')
upgraded=subprocess.run([exe,'--serve',str(text_path)],input='{"op":"inspect"}\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
new=json.loads(upgraded.stdout)['result'];remove_migrated_anchor_defaults(new);new['version']='0.6'
check(new.pop('named_colors')==[], '0.6 migration starts with no named colors')
check(new==old,'0.6 migration preserves all editable Text and shape inputs')
color_path=ornament.with_name('named-color-poster.nect')
old=json.loads(color_path.read_text(encoding='utf-8'))
check(old['version']=='0.7','named-color fixture remains historical 0.7')
upgraded=subprocess.run([exe,'--serve',str(color_path)],input='{"op":"inspect"}\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
new=json.loads(upgraded.stdout)['result'];check(new['version']=='0.11','current writer uses native 0.11')
remove_migrated_anchor_defaults(new);new['version']='0.7';check(new==old,'0.7 migration preserves named colors, links, Text and authored geometry')
polystar_path=ornament.with_name('polystar-field.nect')
old=json.loads(polystar_path.read_text(encoding='utf-8'))
check(old['version']=='0.8','Polystar fixture remains historical 0.8')
upgraded=subprocess.run([exe,'--serve',str(polystar_path)],input='{"op":"inspect"}\n'+
    '{"op":"properties"}\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
replies=[json.loads(line) for line in upgraded.stdout.splitlines()]
new=replies[0]['result'];remove_migrated_anchor_defaults(new);new['version']='0.8'
check(new==old,'0.8 migration preserves linked count, angular correction, all paints and text')
# Catch the documented field vocabulary falling behind real numeric properties.
# This checks that specific schema boundary; the native codec remains the validator.
schema=json.loads((polystar_path.parent.parent/'schemas/native-v0.11.schema.json').read_text())
field_rules=schema['$defs']['ref']['properties']['field']['anyOf']
for property_ in replies[1]['result']:
    if property_['type']!='number': continue
    name=property_['ref']['field']
    check(any(name in rule.get('enum',[]) or ('pattern' in rule and re.fullmatch(rule['pattern'],name)) for rule in field_rules),
          'schema accepts emitted numeric field '+name)
# Native expressions remain authored and are forbidden in all earlier versions.
expression_doc=json.loads(json.dumps(sample))
target=next(o for o in expression_doc['objects'] if o['id']=='path-B')
formula='ref("path-A","point-A1","x") * 2 + 3'
target['transform'][4]['expression']={'source':formula,'version':1}
check(run('--validate',expression_doc).returncode==0,'expression document validates')
for version in ('0.8','0.9'):
    legacy_expr=json.loads(json.dumps(expression_doc));legacy_expr['version']=version
    if version=='0.8': remove_migrated_anchor_defaults(legacy_expr)
    else: remove_migrated_compositing_defaults(legacy_expr)
    check('UNKNOWN_FIELD' in run('--validate',legacy_expr).stderr,'legacy '+version+' rejects expression source')
for invalid in ({'source':'1','version':2},{'source':'1','version':1,'javascript':True}):
    bad=json.loads(json.dumps(expression_doc));next(o for o in bad['objects'] if o['id']=='path-B')['transform'][4]['expression']=invalid
    check(run('--validate',bad).returncode==2,'unknown formula semantic/field rejects')
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'expression.nect';path.write_text(json.dumps(expression_doc),encoding='utf-8')
    ref={'object':'path-B','point':'','field':'transform.tx'}
    commands=[{'op':'inspect'},{'op':'get','ref':ref},{'op':'expression_language'},
              {'op':'apply','expected_revision':0,'commands':[{'type':'set_expression','targets':[ref],
                 'expression':{'source':formula+' + 7','version':1},'replace_binding':False}]},
              {'op':'get','ref':ref},{'op':'undo','expected_revision':1},{'op':'inspect'}]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(json.dumps(c) for c in commands)+'\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
    replies=[json.loads(line) for line in proc.stdout.splitlines()]
    check(all(r['ok'] for r in replies),'formula discovery/apply/readback/undo succeeds')
    check(replies[0]['result']==expression_doc and replies[-1]['result']==expression_doc,'native formula source and literals survive exact roundtrip and Undo')
    check(replies[1]['result']['evaluated']==203 and replies[4]['result']['evaluated']==210,'formula command uses the shared evaluator')
    check(replies[2]['result']['trigonometry']=='degrees','formula language is discoverable')
# Native0.10 remains an exact authored source after default compositing migration.
expression_path=ornament.with_name('phase-form.nect')
old=json.loads(expression_path.read_text(encoding='utf-8'));check(old['version']=='0.10','expression fixture stays historical0.10')
upgraded=subprocess.run([exe,'--serve',str(expression_path)],input='{"op":"inspect"}\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
new=json.loads(upgraded.stdout)['result'];remove_migrated_compositing_defaults(new);new['version']='0.10'
check(new==old,'0.10 migration retains exact formula source and all authored inputs')
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'masked.nect';path.write_text(json.dumps(sample),encoding='utf-8')
    comp=sample['compositions'][0]
    cmds=[dict(type='mask_objects',composition=comp['id'],parent='',members=comp['roots'],id='mask-group',mask_id='geometry-mask',name='Mask',top=True),
          dict(type='set',ref=dict(object='mask-group',point='',field='composite.opacity'),value=.5),
          dict(type='set_compositing',object='mask-group',blend='multiply',isolated=False)]
    requests=[dict(op='apply',expected_revision=0,commands=cmds),dict(op='inspect'),dict(op='compositing_plan',composition=comp['id']),
              dict(op='export_svg',composition=comp['id'],artboard=comp['artboards'][0]['id']),dict(op='undo',expected_revision=1),dict(op='inspect')]
    proc=subprocess.run([exe,'--serve',str(path)],input='\n'.join(json.dumps(r) for r in requests)+'\n',capture_output=True,text=True,encoding='utf-8',timeout=10)
    replies=[json.loads(line) for line in proc.stdout.splitlines()];check(all(r['ok'] for r in replies),'mask authoring/plan/SVG/Undo succeeds')
    native=replies[1]['result'];group=next(o for o in native['objects'] if o['id']=='mask-group')
    check(group['compositing']['mask']['source']==comp['roots'][-1] and group['compositing']['opacity']==dict(literal=.5),'mask uses final paint order and ordinary opacity')
    plan=replies[2]['result'];check(plan['backdrop']=='transparent' and plan['roots'][0]['isolated'],'mask/blend aggregate is explicitly isolated')
    svg_root=ET.fromstring(replies[3]['result']);ns={'s':'http://www.w3.org/2000/svg'}
    clip=svg_root.find('.//s:clipPath',ns);check(clip.attrib['id']=='geometry-mask' and clip.attrib['clipPathUnits']=='userSpaceOnUse','SVG clip uses stable identity and Composition coordinates')
    group_svg=svg_root.find("s:g[@id='mask-group']",ns);check(group_svg.attrib['opacity']=='0.5' and 'mix-blend-mode:multiply' in group_svg.attrib['style'],'SVG retains aggregate opacity and declared blend')
    check(svg_root.find(".//s:g[@id='"+comp['roots'][-1]+"']",ns) is None,'hidden mask source not exported as artwork')
    check(replies[-1]['result']==sample,'one Undo restores all masked-group inputs')
    check(run('--validate',native).returncode==0,'masked native reopens in a fresh process')
    native['version']='0.10';check('UNKNOWN_FIELD' in run('--validate',native).stderr,'old format rejects new compositing fields')
print(f'PASS {checks} process and native migration checks')

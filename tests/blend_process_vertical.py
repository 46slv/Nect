"""Targeted real cold-process native/Session/SVG safety; no desktop socket."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from blend_mcp_vertical import MODES, IDS, rectangle

exe=str(Path(sys.argv[1]).resolve())
checks=0

def check(value,why):
    global checks
    if not value: raise AssertionError(why)
    checks+=1

def run(mode,native):
    return subprocess.run([exe,mode],input=json.dumps(native),text=True,capture_output=True,timeout=10)

sample=json.loads(subprocess.check_output([exe,'--demo'],text=True))
check(sample['version']=='0.80','Current writer0.80')
comp=sample['compositions'][0]['id'];board=sample['compositions'][0]['artboards'][0]['id']
sample['compositions'][0]['artboards'][0].update(x=0,y=0,width=64,height=64)
sample['objects']=[rectangle('backdrop',0,0,64,64,(51,102,153,255))]+[
    rectangle(mode,8+32*(i%2),8+32*(i//2),16,16,(204,51,102,255)) for i,mode in enumerate(MODES)]
sample['compositions'][0]['roots']=[o['id'] for o in sample['objects']]
sample['objects'].sort(key=lambda o:o['id'])
with tempfile.TemporaryDirectory(prefix='nect-blend-process-') as tmp:
    path=Path(tmp)/'source.nect';path.write_text(json.dumps(sample))
    commands=[dict(op='compositing_types'),dict(op='apply',expected_revision=0,commands=[dict(type='set_compositing',object=m,blend=m,isolated=False) for m in MODES]),dict(op='inspect'),dict(op='undo',expected_revision=1),dict(op='inspect'),dict(op='redo',expected_revision=2),dict(op='inspect'),dict(op='apply',expected_revision=3,commands=[dict(type='set_compositing',object='hue',blend='normal',isolated=False),dict(type='set_compositing',object='color',blend='Unknown',isolated=False)]),dict(op='inspect'),dict(op='apply',expected_revision=3,commands=[dict(type='set_compositing',object='hue',blend='hue',isolated=False,profile='srgb16')]),dict(op='inspect')]
    served=subprocess.run([exe,'--serve',str(path)],input='\n'.join(map(json.dumps,commands))+'\n',text=True,capture_output=True,timeout=10)
    check(served.returncode==0,served.stderr)
    r=[json.loads(line) for line in served.stdout.splitlines()]
    check(r[0]['result']['blends']==IDS,'Cold API registry16 IDs')
    check(r[1]['ok'] and r[1]['revision']==1,'Cold Session installs allfour modes once')
    authored=r[2]['result']
    check([o['compositing']['blend'] for o in authored['objects'] if o['id'] in MODES]==sorted(MODES),'Cold authored exactIDs')
    check(r[3]['ok'] and r[4]['result']==sample,'Cold Undo exact native source')
    check(r[5]['ok'] and r[6]['result']==authored,'Cold Redo exact native source')
    check(not r[7]['ok'] and r[7]['error']['code']=='UNSUPPORTED_BLEND' and r[8]['result']==authored,'Cold later failure rolls back first blend edit')
    check(not r[9]['ok'] and r[9]['error']['code']=='UNSUPPORTED_BLEND_PROFILE' and r[10]['result']==authored,'Cold unavailable profile rejection atomically preserves source')
    normalized=run('--normalize',authored)
    check(normalized.returncode==0 and json.loads(normalized.stdout)==authored,'Independent cold process normalizes exact current authored source')
    svg=run('--svg',authored)
    check(svg.returncode==0,'Cold SVG exports supported CSS projection')
    for mode in MODES: check('mix-blend-mode:'+mode in svg.stdout and 'isolation:isolate' in svg.stdout,'Cold SVG exact ID/isolation')
    for mode in MODES:
        lie=copy.deepcopy(authored);lie['version']='0.78'
        for obj in lie['objects']:
            if obj['id'] in MODES and obj['id']!=mode: obj['compositing']['blend']='normal'
        result=run('--validate',lie)
        check(result.returncode==2 and 'NATIVE_VERSION_MISMATCH' in result.stderr,'Cold0.78 lie rejects '+mode)
    for mode in IDS[:12]:
        older=copy.deepcopy(sample);older['version']='0.78';older['objects'][0]['compositing']['blend']=mode
        result=run('--normalize',older);current=copy.deepcopy(older);current['version']='0.80'
        check(result.returncode==0 and json.loads(result.stdout)==current,'Cold0.78 exact old-mode readback '+mode)
    # Preserve enabled-mask SVG refusals under the same new-blend candidate.
    for mode,code in [('alpha','UNSUPPORTED_SVG_ALPHA_MASK'),('luma','UNSUPPORTED_SVG_LUMA_MASK')]:
        masked=copy.deepcopy(authored);target=next(o for o in masked['objects'] if o['id']=='hue')
        target['compositing']['mask']=dict(id='mask',source='saturation',version=1,enabled=True,fill_rule='nonzero',mode=mode,invert=False,mask_color_space='srgb')
        result=run('--svg',masked)
        check(result.returncode==2 and code in result.stderr,'Cold SVG honest '+mode+' refusal')
print(json.dumps(dict(status='PASS',checks=checks,cold_processes=22,native='0.80',live_desktop_ipc=False,scope='cold native/API/SVG safety'),sort_keys=True))

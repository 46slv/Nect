"""Export a bounded blend/mask study and an independent browser pixel check page.

Run this with the built CLI, then serve the output directory on localhost and
open index.html in a browser. The page reports actual SVG-image rasterization;
generating this page alone is not a passing renderer result.
"""
import argparse
import json
import math
from pathlib import Path
import subprocess


def blend(mode, b, s):
    if mode == 'normal': return s
    if mode == 'multiply': return b * s
    if mode == 'screen': return b + s - b * s
    if mode == 'overlay': return 2 * b * s if b <= .5 else 1 - 2 * (1 - b) * (1 - s)
    if mode == 'darken': return min(b, s)
    if mode == 'lighten': return max(b, s)
    if mode == 'color-dodge': return 0 if b == 0 else 1 if s == 1 else min(1, b / (1 - s))
    if mode == 'color-burn': return 1 if b == 1 else 0 if s == 0 else 1 - min(1, (1 - b) / s)
    if mode == 'hard-light': return 2 * b * s if s <= .5 else 1 - 2 * (1 - b) * (1 - s)
    if mode == 'soft-light':
        d = ((16 * b - 12) * b + 4) * b if b <= .25 else math.sqrt(b)
        return b - (1 - 2 * s) * b * (1 - b) if s <= .5 else b + (2 * s - 1) * (d - b)
    if mode == 'difference': return abs(b - s)
    if mode == 'exclusion': return b + s - 2 * b * s
    raise ValueError(mode)


def create_probe(exe, output):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    with subprocess.Popen([str(Path(exe).resolve()), '--serve'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, encoding='utf-8') as process:
        def core(op, **args):
            process.stdin.write(json.dumps(dict(op=op, **args)) + '\n')
            process.stdin.flush()
            response = json.loads(process.stdout.readline())
            if not response['ok']: raise RuntimeError(response)
            return response

        original = core('inspect')['result']
        comp = original['compositions'][0]
        board = dict(comp['artboards'][0], x=0, y=0, width=900, height=540)
        commands = [dict(type='delete_objects', objects=[o['id'] for o in original['objects']]),
                    dict(type='update_artboard', composition=comp['id'], artboard=board)]
        fill_template = next(x['template'] for x in core('operator_types')['result'] if x['type'] == 'nect.paint.fill')
        ref = lambda owner, field: dict(object=owner, point='', field=field)
        scalar = lambda v: dict(literal=v)

        def rect(id_, x, y, w, h, rgb):
            points = [dict(id=id_ + '-p' + str(i), **{k: scalar(v) for k, v in
                      dict(x=px, y=py, in_angle=0, in_length=0, out_angle=0, out_length=0).items()})
                      for i, (px, py) in enumerate(((x, y), (x+w, y), (x+w, y+h), (x, y+h)))]
            commands.append(dict(type='create_path', composition=comp['id'], parent='', id=id_, name=id_,
                                 contours=[dict(id=id_ + '-contour', closed=True, points=points)]))
            fill = json.loads(json.dumps(fill_template));fill['id'] = id_ + '-fill'
            for key, value in zip(('r', 'g', 'b'), rgb): fill['parameters'][key] = scalar(value / 255)
            commands.extend([dict(type='enable_operation', object=id_, operation=id_ + '-stroke', enabled=False),
                             dict(type='add_operation', object=id_, operation=fill, index=1)])

        checks = []
        modes = ['normal', 'multiply', 'screen', 'overlay', 'darken', 'lighten', 'color-dodge',
                 'color-burn', 'hard-light', 'soft-light', 'difference', 'exclusion']
        b, s = [51, 102, 204], [204, 179, 77]
        for i, (mode, alpha) in enumerate((m, a) for a in (1, .5) for m in modes):
            x, y = i % 6 * 150 + 10, i // 6 * 100 + 10
            rect('back-' + str(i), x, y, 130, 80, b)
            id_ = 'front-' + str(i);rect(id_, x+15, y+10, 100, 60, s)
            commands.extend([dict(type='set_compositing', object=id_, blend=mode, isolated=False),
                             dict(type='set', ref=ref(id_, 'composite.opacity'), value=alpha)])
            expected = [round(255 * (alpha * blend(mode, bv/255, sv/255) + (1-alpha) * bv/255)) for bv, sv in zip(b, s)]
            checks.append(dict(name=f'{mode} / {alpha}', x=x+50, y=y+40, expected=expected+[255]))
        # Aggregate opacity, not per-child opacity; one hidden source clips in
        # world space. A transformed source is independent of the target's basis.
        rect('red-a', 10, 410, 100, 100, [255, 0, 0]);rect('red-b', 50, 410, 100, 100, [255, 0, 0])
        commands += [dict(type='group_contiguous', composition=comp['id'], parent='', members=['red-a', 'red-b'], id='alpha-group', name='Aggregate alpha'),
                     dict(type='set', ref=ref('alpha-group','composite.opacity'), value=.5)]
        checks += [dict(name='group opacity single', x=30, y=460, expected=[255,0,0,128]),
                   dict(name='group opacity overlap', x=75, y=460, expected=[255,0,0,128])]
        rect('masked', 170, 410, 140, 100, [0,255,0]);rect('source', 0, 0, 50, 100, [0,0,0])
        commands += [dict(type='set_visibility', object='source', visible=False),
                     dict(type='set', ref=ref('source','transform.tx'), value=200),
                     dict(type='set', ref=ref('source','transform.ty'), value=410),
                     dict(type='set_mask', object='masked', mask=dict(id='geometry-clip', source='source', version=1, enabled=True, fill_rule='nonzero'))]
        checks += [dict(name='world mask inside',x=225,y=460,expected=[0,255,0,255]),
                   dict(name='world mask outside',x=185,y=460,expected=[0,0,0,0])]
        for index, isolated in enumerate((False, True)):
            x = 330+index*160
            rect(f'blue-{index}',x,410,130,100,[0,0,255]);rect(f'red-{index}',x+10,420,110,80,[255,0,0])
            commands += [dict(type='set_compositing',object=f'red-{index}',blend='multiply',isolated=False),
                         dict(type='group_contiguous',composition=comp['id'],parent='',members=[f'red-{index}'],id=f'isolation-{index}',name='Backdrop scope'),
                         dict(type='set_compositing',object=f'isolation-{index}',blend='normal',isolated=isolated)]
            checks.append(dict(name='isolated' if isolated else 'pass through',x=x+60,y=460,expected=([255,0,0,255] if isolated else [0,0,0,255])))
        rect('first-screen',680,410,120,100,[255,0,0])
        commands += [dict(type='set_compositing',object='first-screen',blend='screen',isolated=False)]
        checks.append(dict(name='transparent SVG backdrop',x=740,y=460,expected=[255,0,0,255]))
        core('apply',expected_revision=0,commands=commands)
        native = core('inspect')['result']
        svg = core('export_svg',composition=comp['id'],artboard=board['id'])['result']
        process.stdin.close();process.wait(timeout=5)
    (output/'probe.nect').write_text(json.dumps(native),encoding='utf-8')
    (output/'probe.svg').write_text(svg,encoding='utf-8')
    (output/'expected.json').write_text(json.dumps(checks,indent=2),encoding='utf-8')
    html = '''<!doctype html><meta charset="utf-8"><title>Nect SVG compositing probe</title>
<style>body{font:15px system-ui;background:#eef1f4;color:#192a39;margin:24px}canvas{border:1px solid #9ab;background:white;max-width:100%}pre{white-space:pre-wrap}</style>
<h1>Nect SVG compositing probe</h1><p id="status">Rendering exported SVG…</p><canvas width="900" height="540"></canvas><pre id="results"></pre>
<script>
const checks=CHECK_DATA;
(async()=>{try{const img=new Image();img.src='probe.svg';await img.decode();const ctx=document.querySelector('canvas').getContext('2d',{willReadFrequently:true});ctx.drawImage(img,0,0);
const results=checks.map(c=>{const actual=[...ctx.getImageData(c.x,c.y,1,1).data];const difference=Math.max(...actual.map((v,i)=>Math.abs(v-c.expected[i])));return {...c,actual,difference,pass:difference<=3}});
const passed=results.filter(r=>r.pass).length;document.getElementById('status').textContent=`${passed===results.length?'PASS':'FAIL'} ${passed}/${results.length} exported SVG pixel checks (tolerance 3/255)`;
document.getElementById('results').textContent=JSON.stringify({userAgent:navigator.userAgent,passed,total:results.length,results},null,2);
}catch(error){document.getElementById('status').textContent='FAIL '+error.message}})();
</script>'''.replace('CHECK_DATA',json.dumps(checks))
    (output/'index.html').write_text(html,encoding='utf-8')
    return dict(page=str(output/'index.html'),checks=len(checks),status='Generated; browser execution required')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',required=True);parser.add_argument('--output',required=True)
    args=parser.parse_args()
    print(json.dumps(create_probe(args.exe,args.output),indent=2))

"""Link a named palette in an opened typography-poster.nect live document.

All edits use the desktop-owned Session. The source file is preserved by saving
the study under the explicitly supplied output path.
"""
import argparse
import json
from pathlib import Path
from session_client import call


def create_palette(endpoint, output):
    live=call(endpoint, {'op':'hello'});identity={k:live[k] for k in ('session_id','document_id')}
    revision=live['revision']
    def core(op, **args):
        response=call(endpoint,dict(identity,op='core',request=dict(op=op,**args)))
        if not response['ok']:
            raise RuntimeError(response)
        return response
    doc=core('inspect')['result']
    if doc['named_colors'] or not {'japanese-title','english-title','petal','heart'}.issubset({x['id'] for x in doc['objects']}):
        raise RuntimeError('Open the original typography poster without a named palette first.')
    def ref(owner,field): return dict(object=owner,point='',field=field)
    def paint(owner): return ref(owner,f'op.{owner}-fill.color')
    petal=ref('petal','op.petal-fill.gradient.petal-gradient.stop.petal-color-2.color')
    heart=ref('heart','op.heart-fill.gradient.heart-gradient.stop.heart-color-2.color')
    definitions=[('palette-ivory','Ivory typography',paint('japanese-title'),[paint(x) for x in ('japanese-title','english-title','description')]),
                 ('palette-mist','Mist typography',paint('edition'),[paint(x) for x in ('edition','japanese-subtitle','dates','fictional-note')]),
                 ('palette-warm','Warm petal and core',petal,[petal,heart])]
    commands=[]
    for id_,name,source,targets in definitions:
        value=core('get',ref=source)['result']['evaluated']
        commands.append(dict(type='create_named_color',color=dict(value,id=id_,name=name,rgba=[{'literal':v} for v in value['rgba']])))
        commands.extend(dict(type='link_color',target=target,source=ref(id_,'color')) for target in targets)
    revision=core('apply',expected_revision=revision,commands=commands)['revision']
    linked=core('inspect')['result']
    assert len(linked['named_colors'])==3
    commands=[]
    for id_,hex_ in [('palette-ivory','FFF4D6'),('palette-mist','97DDDA'),('palette-warm','FFC098')]:
        commands.append(dict(type='set_color',ref=ref(id_,'color'),value=dict(space='srgb',profile='srgb',alpha='straight',
            rgba=[int(hex_[i:i+2],16)/255 for i in (0,2,4)]+[1])))
    changed=core('apply',expected_revision=revision,commands=commands);revision=changed['revision']
    assert {'palette-ivory','palette-mist','palette-warm','japanese-title','english-title','description','edition','japanese-subtitle','dates','fictional-note','petal','heart'}.issubset(changed['result']['changed_ids'])
    output=Path(output).resolve()
    result=call(endpoint,dict(identity,op='save',path=str(output),expected_revision=revision))
    if not result['ok']:
        raise RuntimeError(result)
    comp=doc['compositions'][0]
    svg=core('export_svg',composition=comp['id'],artboard=comp['artboards'][0]['id'])['result']
    output.with_suffix('.svg').write_text(svg,encoding='utf-8')
    inventory=core('used_colors')['result']
    return dict(native=str(output),revision=revision,named_colors=3,linked_targets=9,
        used_color_count=len(inventory['colors']),inventory_scope=inventory['scope'])


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint',required=True);parser.add_argument('--output',required=True)
    args=parser.parse_args();print(json.dumps(create_palette(args.endpoint,args.output),ensure_ascii=True,indent=2))

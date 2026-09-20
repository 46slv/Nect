"""Author a source-preserving ornament in an empty live Nect desktop Session.

Run the desktop with --automation-endpoint nect-demo, then:
python scripts/create_radial_demo.py --endpoint nect-demo --output build/radial.nect
This uses production semantic commands only; it refuses to replace existing art.
"""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET
from session_client import call


def create(endpoint, output):
    live = call(endpoint, {'op': 'hello'})
    identity = {k: live[k] for k in ('session_id', 'document_id')}
    revision = live['revision']

    def core(op, **args):
        response = call(endpoint, dict(identity, op='core', request=dict(op=op, **args)))
        if not response['ok']:
            raise RuntimeError(response)
        return response

    def apply(commands):
        nonlocal revision
        revision = core('apply', expected_revision=revision, commands=commands)['revision']

    document = core('inspect')['result']
    if document['objects']:
        raise RuntimeError('Start with an empty document; this demo preserves existing artwork.')
    comp = document['compositions'][0]
    scalar = lambda value: {'literal': value}

    def primitive(id_, kind, **parameters):
        return dict(type='create_primitive', composition=comp['id'], parent='', id=id_, name=id_.replace('-', ' ').title(),
            source=dict(id=id_+'-source', type='nect.shape.'+kind, version=1,
                        parameters={k: scalar(v) for k, v in parameters.items()}))

    apply([
        primitive('paper', 'rectangle', center_x=480, center_y=320, width=960, height=640),
        primitive('petal', 'circle', center_x=480, center_y=237, radius=68),
        primitive('ray', 'rectangle', center_x=480, center_y=64, width=4, height=18),
        primitive('orbit', 'circle', center_x=480, center_y=320, radius=230),
        primitive('heart', 'circle', center_x=480, center_y=320, radius=33),
    ])
    objects = {o['id']: o for o in core('inspect')['result']['objects']}
    templates = {d['type']: d['template'] for d in core('operator_types')['result']}

    def operation(id_, type_, **parameters):
        base = templates[type_]
        return dict(base, id=id_, parameters={k:scalar(parameters.get(k,v['literal'])) for k,v in base['parameters'].items()})

    def fill(id_, color, alpha=1):
        return operation(id_, 'nect.paint.fill', r=int(color[0:2],16)/255, g=int(color[2:4],16)/255,
                         b=int(color[4:6],16)/255, a=alpha)

    commands=[]
    for id_, color in [('paper','F2ECDC'),('petal','D99D45'),('ray','19485C'),('heart','F2ECDC')]:
        commands.append(dict(type='add_operation',object=id_,index=1,operation=fill(id_+'-fill',color)))
    for id_, color, width in [('paper','F2ECDC',0),('petal','19485C',2),('ray','19485C',0),('orbit','19485C',1.5),('heart','19485C',3)]:
        opid=objects[id_]['legacy_stroke']
        for field,value in dict(width=width,r=int(color[0:2],16)/255,g=int(color[2:4],16)/255,b=int(color[4:6],16)/255).items():
            commands.append(dict(type='set',ref=dict(object=id_,point='',field=f'op.{opid}.{field}'),value=value))
    commands.append(dict(type='set',ref=dict(object='petal',point='petal-source-north',field='y'),value=143))
    commands += [dict(type='add_operation',object='petal',index=2,operation=operation('petal-repeat','nect.shape.repeater',
                    copies=8,position_x=0,position_y=0,anchor_x=480,anchor_y=320,rotation=45)),
                 dict(type='add_operation',object='ray',index=2,operation=operation('ray-repeat','nect.shape.repeater',
                    copies=32,position_x=0,position_y=0,anchor_x=480,anchor_y=320,rotation=11.25))]
    commands.append(dict(type='link',target=dict(object='ray',point='',field='op.ray-repeat.rotation'),
        binding=dict(source=dict(object='petal',point='',field='op.petal-repeat.rotation'),scale=.25,offset=0,mode='copy_local_value')))
    commands.append(dict(type='group_contiguous',composition=comp['id'],parent='',members=['petal','ray','orbit','heart'],id='ornament',name='Radial ornament'))
    apply(commands)
    plan=core('render_plan',object='petal')['result']
    assert plan['path_instances']==8 and len(plan['paint_layers'])==16
    output=Path(output).resolve()
    saved=call(endpoint,dict(identity,op='save',path=str(output),expected_revision=revision))
    if not saved['ok']:
        raise RuntimeError(saved)
    svg=core('export_svg',composition=comp['id'],artboard=comp['artboards'][0]['id'])['result']
    paths=ET.fromstring(svg).findall('.//{http://www.w3.org/2000/svg}path')
    assert len(paths)==85
    output.with_suffix('.svg').write_text(svg,encoding='utf-8')
    return dict(native=str(output),svg=str(output.with_suffix('.svg')),revision=revision,paint_layers=len(paths),source_preserved=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint',required=True)
    parser.add_argument('--output',required=True)
    args=parser.parse_args()
    print(create(args.endpoint,args.output))

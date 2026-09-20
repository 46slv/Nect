"""Author a source-preserving ornament in an empty live Nect desktop Session.

Run the desktop with --automation-endpoint nect-demo, then:
python scripts/create_radial_demo.py --endpoint nect-demo --output build/radial.nect
This uses production semantic commands only; it refuses to replace existing art.
"""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET
from session_client import call


def create(endpoint, output, gradients=False, persist=True):
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
    if gradients:
        templates={item['type']:item['template'] for item in core('gradient_types')['result']}

        def gradient(object_, kind, start, end, colors):
            base=templates[kind]
            stops=[]
            for index,(offset,color) in enumerate(colors):
                stops.append(dict(id=f'{object_}-color-{index}',offset=scalar(offset),
                    rgba=[scalar(int(color[i:i+2],16)/255) for i in (0,2,4)]+[scalar(1)]))
            return dict(type='set_gradient',object=object_,operation=object_+'-fill',
                gradient=dict(base,id=object_+'-gradient',start_x=scalar(start[0]),start_y=scalar(start[1]),
                    end_x=scalar(end[0]),end_y=scalar(end[1]),stops=stops))

        commands=[gradient('paper','linear',(0,0),(960,640),[(0,'091D3B'),(.55,'173D63'),(1,'3E355F')]),
            gradient('petal','linear',(480,140),(480,310),[(0,'62DDD8'),(.52,'708BEB'),(1,'EC8FAA')]),
            gradient('heart','radial',(468,310),(509,324),[(0,'FFF9C4'),(.6,'F6C77E'),(1,'E9889C')])]
        for object_ in ('petal','orbit','ray','heart'):
            opid=objects[object_]['legacy_stroke']
            for channel,value in zip('rgb',(.70,.83,.91)):
                commands.append(dict(type='set',ref=dict(object=object_,point='',field=f'op.{opid}.{channel}'),value=value))
        for channel in 'rgb':
            commands.append(dict(type='link',target=dict(object='heart',point='',
                field=f'op.heart-fill.gradient.heart-gradient.stop.heart-color-2.{channel}'),
                binding=dict(source=dict(object='petal',point='',
                    field=f'op.petal-fill.gradient.petal-gradient.stop.petal-color-2.{channel}'),
                    scale=1,offset=0,mode='copy_local_value')))
        for channel,value in zip('rgb',(.70,.83,.91)):
            commands.append(dict(type='set',ref=dict(object='ray',point='',field=f'op.ray-fill.{channel}'),value=value))
        apply(commands)
    plan=core('render_plan',object='petal')['result']
    assert plan['path_instances']==8 and len(plan['paint_layers'])==16
    if not persist:
        return dict(revision=revision,source_preserved=True,gradients=gradients)
    output=Path(output).resolve()
    saved=call(endpoint,dict(identity,op='save',path=str(output),expected_revision=revision))
    if not saved['ok']:
        raise RuntimeError(saved)
    svg=core('export_svg',composition=comp['id'],artboard=comp['artboards'][0]['id'])['result']
    paths=ET.fromstring(svg).findall('.//{http://www.w3.org/2000/svg}path')
    assert len(paths)==85
    output.with_suffix('.svg').write_text(svg,encoding='utf-8')
    return dict(native=str(output),svg=str(output.with_suffix('.svg')),revision=revision,paint_layers=len(paths),source_preserved=True,gradients=gradients)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint',required=True)
    parser.add_argument('--output',required=True)
    parser.add_argument('--gradients',action='store_true',help='Author the linked linear/radial gradient variant')
    args=parser.parse_args()
    print(create(args.endpoint,args.output,args.gradients))

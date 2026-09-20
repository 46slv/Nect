"""Build a pivot/follow composition through an empty live Nect Session."""
import argparse
import copy
import json
from pathlib import Path
import xml.etree.ElementTree as ET
from session_client import call


def create_study(endpoint,output):
    hello=call(endpoint,{'op':'hello'});identity={k:hello[k] for k in ('session_id','document_id')};revision=hello['revision']
    def core(op,**args):
        result=call(endpoint,dict(identity,op='core',request=dict(op=op,**args)))
        if not result['ok']:raise RuntimeError(result)
        return result
    def apply(commands):
        nonlocal revision
        revision=core('apply',expected_revision=revision,commands=commands)['revision']
    document=core('inspect')['result']
    if document['objects'] or document['named_colors']:raise RuntimeError('Use an empty document; existing artwork is preserved.')
    output=Path(output).resolve();saved=call(endpoint,dict(identity,op='save',path=str(output),expected_revision=revision))
    if not saved['ok']:raise RuntimeError(saved)
    comp=document['compositions'][0];board=dict(comp['artboards'][0],name='Pivot and follow',width=960,height=640)
    sources={x['type']:x['template'] for x in core('primitive_types')['result']}
    operations={x['type']:x['template'] for x in core('operator_types')['result']}
    scalar=lambda v:dict(literal=v)
    def ref(id_,field):return dict(object=id_,point='',field=field)
    def set_(id_,field,v):return dict(type='set',ref=ref(id_,field),value=v)
    def color(hex_,alpha=1):return dict(space='srgb',profile='srgb',alpha='straight',rgba=[int(hex_[i:i+2],16)/255 for i in (0,2,4)]+[alpha])
    commands=[dict(type='update_artboard',composition=comp['id'],artboard=board)]
    def shape(id_,kind,ink,**parameters):
        source=copy.deepcopy(sources['nect.shape.'+kind]);source['id']=id_+'-source';source['parameters'].update({k:scalar(v) for k,v in parameters.items()})
        commands.append(dict(type='create_primitive',composition=comp['id'],parent='',id=id_,name=id_.replace('-',' ').title(),source=source))
        fill=copy.deepcopy(operations['nect.paint.fill']);fill['id']=id_+'-fill'
        commands.extend([dict(type='add_operation',object=id_,operation=fill,index=1),dict(type='set_color',ref=ref(id_,'op.'+id_+'-fill.color'),value=color(ink)),
                         dict(type='enable_operation',object=id_,operation=id_+'-stroke',enabled=False)])
    shape('paper','rectangle','F0E9DB',center_x=480,center_y=320,width=960,height=640)
    shape('beam','rectangle','1C4143',center_x=480,center_y=270,width=490,height=10)
    shape('pivot','circle','D76B45',center_x=480,center_y=270,radius=18)
    shape('left-line','rectangle','1C4143',center_x=280,center_y=333,width=2,height=126)
    shape('right-line','rectangle','1C4143',center_x=680,center_y=324,width=2,height=108)
    shape('left-weight','circle','D76B45',center_x=280,center_y=419,radius=68)
    shape('right-weight','polygon','6C9890',center_x=680,center_y=399,radius=92,points=3,rotation=-90)
    shape('center-line','rectangle','1C4143',center_x=480,center_y=208,width=2,height=100)
    text_template=core('text_defaults')['result']
    for id_,content,x,y,size in [('edition','NECT / FORM STUDIES  04',48,32,12),('title','PIVOT / FOLLOW',48,67,40),
        ('caption','One anchor. Independent structure. A shared motion.',48,541,18),
        ('footer','EDIT THE MOBILE GROUP / ROTATE BY / ALL LINKED PIECES FOLLOW',48,598,10)]:
        text=copy.deepcopy(text_template);text.update(id=id_+'-source',content=content,family='Segoe UI')
        text['parameters'].update({k:scalar(v) for k,v in dict(origin_x=x,origin_y=y,font_size=size,tracking=2).items()})
        commands.extend([dict(type='create_text',composition=comp['id'],parent='',id=id_,name=content,source=text),
            dict(type='set_color',ref=ref(id_,'op.'+id_+'-fill.color'),value=color('1C4143'))])
    apply(commands)
    gradient=copy.deepcopy(next(x['template'] for x in core('gradient_types')['result'] if x['type']=='linear'))
    gradient.update(id='weight-gradient',start_x=scalar(610),start_y=scalar(325),end_x=scalar(730),end_y=scalar(455))
    for stop,ink in zip(gradient['stops'],('AEC4AA','487A75')):stop['rgba']=[scalar(v) for v in color(ink)['rgba']]
    children=['left-line','right-line','left-weight','right-weight']
    apply([dict(type='set_gradient',object='right-weight',operation='right-weight-fill',gradient=gradient),
        dict(type='group_contiguous',composition=comp['id'],parent='',members=['beam','pivot'],id='mobile',name='Mobile / rotate around Anchor'),
        dict(type='group_contiguous',composition=comp['id'],parent='',members=children,id='ornaments',name='Ornaments / independent structure'),
        set_('mobile','transform.anchor_x',480),set_('mobile','transform.anchor_y',270)]+
        [dict(type='set_transform_parent',object=id_,parent='beam',preserve_world=True) for id_ in children])
    before={x['object']:x['world'] for x in core('transforms')['result']}
    # Structural translation must not be applied a second time to these followers.
    apply([set_('ornaments','transform.tx',100)])
    after={x['object']:x['world'] for x in core('transforms')['result']}
    assert all(before[id_]==after[id_] for id_ in children)
    apply([dict(type='transform_around_anchor',object='mobile',rotation=-12,scale_x=1,scale_y=1)])
    transforms={x['object']:x for x in core('transforms')['result']}
    assert all(abs(a-b)<1e-8 for a,b in zip(transforms['mobile']['world_anchor'],[480,270]))
    assert all(transforms[id_]['world']==transforms['beam']['world'] for id_ in children)
    saved=call(endpoint,dict(identity,op='save',path=str(output),expected_revision=revision))
    if not saved['ok']:raise RuntimeError(saved)
    native=core('inspect')['result'];assert json.loads(output.read_text(encoding='utf-8'))==native
    svg=core('export_svg',composition=comp['id'],artboard=board['id'])['result'];root=ET.fromstring(svg);output.with_suffix('.svg').write_text(svg,encoding='utf-8')
    assert len(root.findall('.//{http://www.w3.org/2000/svg}path'))==12
    return dict(native=str(output),revision=revision,paint_layers=12,followers=children,anchor=transforms['mobile']['world_anchor'],structural_transform_not_doubled=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--endpoint',required=True);parser.add_argument('--output',required=True)
    args=parser.parse_args();print(json.dumps(create_study(args.endpoint,args.output),indent=2))

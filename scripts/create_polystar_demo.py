"""Author a retained Polygon/Star study through an empty live desktop Session."""
import argparse
import copy
import json
from pathlib import Path
import xml.etree.ElementTree as ET
from session_client import call


def create_study(endpoint, output):
    live=call(endpoint,{'op':'hello'});identity={k:live[k] for k in ('session_id','document_id')};revision=live['revision']
    def core(op,**args):
        reply=call(endpoint,dict(identity,op='core',request=dict(op=op,**args)))
        if not reply['ok']:
            raise RuntimeError(reply)
        return reply
    def apply(commands):
        nonlocal revision
        revision=core('apply',expected_revision=revision,commands=commands)['revision']
    document=core('inspect')['result']
    if document['objects'] or document['named_colors']:
        raise RuntimeError('Start with an empty document; existing artwork is preserved.')
    output=Path(output).resolve()
    # Select the explicit target before any edit; live save must never mutate an
    # unrelated originally opened file while this study is being assembled.
    saved=call(endpoint,dict(identity,op='save',path=str(output),expected_revision=revision))
    if not saved['ok']:
        raise RuntimeError(saved)
    comp=document['compositions'][0];board=dict(comp['artboards'][0],name='Polystar field',width=960,height=640)
    templates={x['type']:x['template'] for x in core('primitive_types')['result']}
    operations={x['type']:x['template'] for x in core('operator_types')['result']}
    scalar=lambda value:{'literal':value}
    def ref(owner,field,point=''):return dict(object=owner,point=point,field=field)
    def binding(source):return dict(source=source,scale=1,offset=0,mode='copy_local_value')
    def primitive(id_,kind,**parameters):
        source=copy.deepcopy(templates['nect.shape.'+kind]);source['id']=id_+'-source'
        source['parameters'].update({k:scalar(v) for k,v in parameters.items()})
        return dict(type='create_primitive',composition=comp['id'],parent='',id=id_,name=id_.replace('-',' ').title(),source=source)
    star=primitive('star','star',center_x=680,center_y=332,outer_radius=170,inner_radius=75)
    count=ref('polygon','generator.points');star['source']['parameters']['points']=dict(literal=5,binding=binding(count))
    apply([dict(type='update_artboard',composition=comp['id'],artboard=board),
           primitive('paper','rectangle',center_x=480,center_y=320,width=960,height=640),
           primitive('polygon','polygon',center_x=280,center_y=332,radius=155,points=6),star])
    commands=[]
    def set_(owner,field,value):commands.append(dict(type='set',ref=ref(owner,field),value=value))
    def rgba(hex_,alpha=1):return [int(hex_[i:i+2],16)/255 for i in (0,2,4)]+[alpha]
    def color(hex_,alpha=1):return dict(space='srgb',profile='srgb',alpha='straight',rgba=rgba(hex_,alpha))
    def operation(owner,id_,type_,index,**parameters):
        op=copy.deepcopy(operations[type_]);op['id']=id_;op['parameters'].update({k:scalar(v) for k,v in parameters.items()})
        commands.append(dict(type='add_operation',object=owner,operation=op,index=index))
    for id_,hex_,alpha in [('mist','96E1D8',1),('mist-glass','96E1D8',.13),('ivory','FFEFD6',1)]:
        commands.append(dict(type='create_named_color',color=dict(color(hex_,alpha),id='palette-'+id_,name=id_.replace('-',' ').title(),rgba=[scalar(x) for x in rgba(hex_,alpha)])))
    for id_ in ('paper','polygon','star'):
        operation(id_,id_+'-fill','nect.paint.fill',1)
    commands.append(dict(type='set_color',ref=ref('paper','op.paper-fill.color'),value=color('111D2F')))
    commands.append(dict(type='enable_operation',object='paper',operation='paper-stroke',enabled=False))
    for owner,op,palette in [('polygon','polygon-fill','mist-glass'),('polygon','polygon-stroke','mist'),('star','star-stroke','ivory')]:
        commands.append(dict(type='link_color',target=ref(owner,'op.'+op+'.color'),source=ref('palette-'+palette,'color')))
    set_('polygon','op.polygon-stroke.width',1.3);set_('star','op.star-stroke.width',1.2)
    operation('polygon','polygon-repeat','nect.shape.repeater',2,copies=6,position_x=0,position_y=0,anchor_x=280,anchor_y=332,
              rotation=15,scale_x=.86,scale_y=.86,start_opacity=1,end_opacity=.3)
    operation('star','star-repeat','nect.shape.repeater',2,copies=2,position_x=0,position_y=0,anchor_x=680,anchor_y=332,
              rotation=30,scale_x=.76,scale_y=.76,start_opacity=.85,end_opacity=1)
    gradient=copy.deepcopy(next(x['template'] for x in core('gradient_types')['result'] if x['type']=='linear'))
    gradient.update(id='star-gradient',start_x=scalar(570),start_y=scalar(200),end_x=scalar(795),end_y=scalar(470),
        stops=[dict(id='star-warm',offset=scalar(0),rgba=[scalar(x) for x in rgba('FFD494')]),
               dict(id='star-coral',offset=scalar(1),rgba=[scalar(x) for x in rgba('E98C99')])])
    commands.append(dict(type='set_gradient',object='star',operation='star-fill',gradient=gradient))
    corrected=ref('star','x','star-source-outer-1-6');commands.append(dict(type='set',ref=corrected,value=846))
    text=core('text_defaults')['result']
    for id_,content,x,y,size,ink in [('edition','NECT / FORM STUDIES  03',48,32,13,'96E1D8'),
            ('title','POLYSTAR FIELD',48,67,40,'FFEFD6'),('left-note','01 / LINKED SYMMETRY',120,537,14,'96E1D8'),
            ('right-note','02 / RETAINED POINT EDIT',525,537,14,'FFEFD6'),
            ('footer','POLYGON + STAR  /  LIVE SOURCES  /  SHARED PALETTE',48,598,11,'A5B5CC')]:
        source=copy.deepcopy(text);source.update(id=id_+'-source',content=content,family='Segoe UI')
        source['parameters'].update({k:scalar(v) for k,v in dict(origin_x=x,origin_y=y,font_size=size,tracking=2).items()})
        commands.append(dict(type='create_text',composition=comp['id'],parent='',id=id_,name=content,source=source))
        commands.append(dict(type='set_color',ref=ref(id_,'op.'+id_+'-fill.color'),value=color(ink)))
    apply(commands)
    authored=core('inspect')['result']
    for points in (12,6):
        apply([dict(type='set',ref=count,value=points)])
        assert core('get',ref=corrected)['result']['evaluated']==846
        assert core('get',ref=ref('star','generator.points'))['result']['evaluated']==points
    assert core('inspect')['result']==authored
    rejected=call(endpoint,dict(identity,op='core',request=dict(op='apply',expected_revision=revision,
        commands=[dict(type='set',ref=count,value=7)])))
    assert not rejected['ok'] and rejected['error']['code']=='UNRESOLVED_POINT_EDIT' and rejected['revision']==revision
    saved=call(endpoint,dict(identity,op='save',path=str(output),expected_revision=revision))
    if not saved['ok']:
        raise RuntimeError(saved)
    svg=core('export_svg',composition=comp['id'],artboard=board['id'])['result'];root=ET.fromstring(svg)
    output.with_suffix('.svg').write_text(svg,encoding='utf-8')
    assert json.loads(output.read_text(encoding='utf-8'))==authored
    return dict(native=str(output),revision=revision,paint_layers=len(root.findall('.//{http://www.w3.org/2000/svg}path')),
                count_link=True,angular_edit_preserved=True,unmapped_count_change_rejected=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--endpoint',required=True);parser.add_argument('--output',required=True)
    args=parser.parse_args();print(json.dumps(create_study(args.endpoint,args.output),indent=2))

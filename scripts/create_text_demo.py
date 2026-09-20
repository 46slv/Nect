"""Create an editable Japanese typography poster through the live Session API.

All artwork is authored by this script; installed fonts remain local references.
The example event is fictional. Requires an empty desktop document.
"""
import argparse
import copy
from pathlib import Path
import xml.etree.ElementTree as ET
from create_radial_demo import create
from session_client import call


def create_poster(endpoint, output):
    create(endpoint, output, gradients=True, persist=False)
    live=call(endpoint, {'op':'hello'})
    identity={k:live[k] for k in ('session_id','document_id')}
    def core(op, **args):
        reply=call(endpoint, dict(identity,op='core',request=dict(op=op,**args)))
        if not reply['ok']:
            raise RuntimeError(reply)
        return reply
    doc=core('inspect')['result'];comp=doc['compositions'][0]
    frame=dict(comp['artboards'][0],name='Japanese typography poster',width=720,height=960)
    commands=[dict(type='update_artboard',composition=comp['id'],artboard=frame)]
    def set_(object_, field, value):
        commands.append(dict(type='set',ref=dict(object=object_,point='',field=field),value=value))
    for name,value in dict(center_x=360,center_y=480,width=720,height=960).items():
        set_('paper','generator.'+name,value)
    for name,value in dict(a=.78,d=.78,tx=55.6,ty=225.4).items():
        set_('ornament','transform.'+name,value)
    set_('paper','op.paper-fill.gradient.paper-gradient.end_x',720)
    set_('paper','op.paper-fill.gradient.paper-gradient.end_y',960)
    template=core('text_defaults')['result']
    def text_(id_,content,x,y,size,family='Yu Gothic',direction='horizontal',color='F4EDDF',**params):
        source=copy.deepcopy(template)
        source.update(id=id_+'-source',content=content,family=family,direction=direction)
        for key,value in dict(origin_x=x,origin_y=y,font_size=size,**params).items():
            source['parameters'][key]={'literal':value}
        commands.append(dict(type='create_text',composition=comp['id'],parent='',id=id_,name=id_.replace('-',' ').title(),source=source))
        for channel,start in zip('rgb',(0,2,4)):
            set_(id_,f'op.{id_}-fill.{channel}',int(color[start:start+2],16)/255)
        return source
    text_('edition','NECT / FORM STUDIES   01',52,45,13,'Segoe UI',tracking=3,color='AED8E8')
    text_('japanese-title','形と光の庭',72,118,54,'Yu Mincho','vertical',tracking=5)
    text_('japanese-subtitle','小さなかたち、重なる色。',157,122,19,'Yu Gothic','vertical',tracking=3,color='AED8E8')
    text_('english-title','Forms in bloom',50,728,48,'Segoe UI',color='F4EDDF')
    text_('description','線を育て、色を重ねる。\n日本語の縦書きと、編集できる幾何学。',54,807,16,line_spacing=28)
    text_('dates','2026.10.03 — 11.08',54,898,18,'Segoe UI',color='AED8E8')
    text_('fictional-note','FICTIONAL EXHIBITION STUDY\nNATIVE TEXT + PROCEDURAL SHAPES',423,888,10,'Segoe UI',tracking=.5,line_spacing=16,color='AED8E8')
    commands.append(dict(type='link',target=dict(object='japanese-subtitle',point='',field='text.font_size'),
        binding=dict(source=dict(object='japanese-title',point='',field='text.font_size'),scale=19/54,offset=0,mode='copy_local_value')))
    result=core('apply',expected_revision=live['revision'],commands=commands)
    report=core('export_plan',composition=comp['id'],artboard=frame['id'])['result']
    assert len(report['text'])==7 and not any(x['overflow'] for x in report['text'])
    output=Path(output).resolve()
    saved=call(endpoint,dict(identity,op='save',path=str(output),expected_revision=result['revision']))
    if not saved['ok']:
        raise RuntimeError(saved)
    svg=core('export_svg',composition=comp['id'],artboard=frame['id'])['result']
    assert ET.fromstring(svg).attrib['viewBox']=='0 0 720 960'
    output.with_suffix('.svg').write_text(svg,encoding='utf-8')
    return dict(native=str(output),svg=str(output.with_suffix('.svg')),revision=result['revision'],text=report['text'])


if __name__=='__main__':
    import json
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint',required=True);parser.add_argument('--output',required=True)
    args=parser.parse_args();print(json.dumps(create_poster(args.endpoint,args.output),ensure_ascii=True,indent=2))

"""Create an editable multi-crop study in an empty live desktop Session.

The original, square and banner frames share one procedural artwork. Page order
changes do not relocate it. Banner width follows Square; banner height overrides.
"""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET
from create_radial_demo import create
from session_client import call


def create_studies(endpoint, output):
    create(endpoint,output,gradients=True,persist=False)
    live=call(endpoint,{'op':'hello'})
    identity={k:live[k] for k in ('session_id','document_id')}
    def core(op,**args):
        response=call(endpoint,dict(identity,op='core',request=dict(op=op,**args)))
        if not response['ok']:
            raise RuntimeError(response)
        return response
    doc=core('inspect')['result'];comp=doc['compositions'][0]
    original=comp['artboards'][0]['id']
    square=dict(id='square-frame',name='Square crop',x=180,y=20,width=600,height=600)
    banner=dict(id='banner-frame',name='Banner — shared width',x=180,y=170,width=480,height=300,
        parent_size=dict(artboard=square['id'],width=True,height=False))
    response=core('apply',expected_revision=live['revision'],commands=[
        dict(type='add_artboard',composition=comp['id'],artboard=square,index=1),
        dict(type='add_artboard',composition=comp['id'],artboard=banner,index=2),
        dict(type='reorder_artboards',composition=comp['id'],order=[square['id'],banner['id'],original])])
    frames=core('artboards',composition=comp['id'])['result']
    assert frames[0]['evaluated']['id']=='square-frame'
    assert frames[1]['authored']['width']==480 and frames[1]['evaluated']['width']==600
    output=Path(output).resolve()
    saved=call(endpoint,dict(identity,op='save',path=str(output),expected_revision=response['revision']))
    if not saved['ok']:
        raise RuntimeError(saved)
    exports=[]
    for number,frame in enumerate(frames,1):
        board=frame['evaluated']
        svg=core('export_svg',composition=comp['id'],artboard=board['id'])['result']
        assert list(map(float,ET.fromstring(svg).attrib['viewBox'].split()))==[board[k] for k in ('x','y','width','height')]
        path=output.with_name(f'{output.stem}-{number}.svg');path.write_text(svg,encoding='utf-8');exports.append(str(path))
    return dict(native=str(output),exports=exports,revision=saved['revision'],artboards=len(frames))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint',required=True);parser.add_argument('--output',required=True)
    args=parser.parse_args();print(create_studies(args.endpoint,args.output))

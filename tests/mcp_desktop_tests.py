"""Seeded formal MCP client -> sidecar -> live Qt Session, including restart.

Uses a temporary document and offscreen desktop; this is semantic/persistence
evidence, not a viewport performance or visual quality claim.
"""
import json
import struct
import zlib
import base64
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import time
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
EXE = str(Path(sys.argv[1]).resolve())
desktop = None
mcp = None


def start(endpoint, temp, native=None):
    ready = temp / ('ready-' + uuid.uuid4().hex + '.json')
    args = [EXE, '--automation-endpoint', endpoint, '--recovery-dir', str(temp / 'recovery'), '--ready-file', str(ready)]
    if native:
        args.append(str(native))
    process = subprocess.Popen(args, env=dict(os.environ, QT_QPA_PLATFORM='offscreen'))
    deadline = time.monotonic() + 10
    while not ready.exists():
        if process.poll() is not None or time.monotonic() >= deadline:
            process.kill()
            raise AssertionError('Desktop did not start')
        time.sleep(.02)
    return process, json.loads(ready.read_text(encoding='utf-8'))


seq = 0
def rpc(method, params=None):
    global seq
    seq += 1
    message = dict(jsonrpc='2.0', id=seq, method=method)
    if params is not None:
        message['params'] = params
    mcp.stdin.write(json.dumps(message) + '\n')
    mcp.stdin.flush()
    reply = json.loads(mcp.stdout.readline())
    assert reply['id'] == seq and reply['jsonrpc'] == '2.0', reply
    return reply


def tool(name, arguments=None):
    reply = rpc('tools/call', dict(name=name, arguments=arguments or {}))
    assert 'result' in reply, reply
    result = reply['result']
    assert json.loads(result['content'][0]['text']) == result['structuredContent']
    assert result['isError'] == (not result['structuredContent']['ok'])
    return result['structuredContent']


identity = {}
def core(op, **kwargs):
    return tool('nect_command', dict(identity, request=dict(op=op, **kwargs)))


def apply(commands, revision):
    result = core('apply', expected_revision=revision, commands=commands)
    assert result['ok'], result
    assert result['revision'] == revision + 1
    assert result['result']['changed_ids']
    return result['revision']


def point(id_, x, y):
    return dict(id=id_, x={'literal': x}, y={'literal': y}, in_angle={'literal': 180},
                in_length={'literal': 20}, out_angle={'literal': 0}, out_length={'literal': 20})


try:
    with tempfile.TemporaryDirectory(prefix='nect-mcp-') as directory:
        temp = Path(directory)
        endpoint = 'nect-test-' + uuid.uuid4().hex
        desktop, _ = start(endpoint, temp)
        mcp = subprocess.Popen([sys.executable, str(ROOT / 'scripts/mcp_server.py'), '--endpoint', endpoint],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, encoding='utf-8')
        assert rpc('tools/list')['error']['code'] == -32002
        init = rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={}, clientInfo=dict(name='nect-scenario', version='1')))
        assert init['result']['protocolVersion'] == '2025-06-18'
        mcp.stdin.write(json.dumps(dict(jsonrpc='2.0', method='notifications/initialized')) + '\n'); mcp.stdin.flush()
        assert {t['name'] for t in rpc('tools/list')['result']['tools']} == {'nect_session', 'nect_command', 'nect_file', 'nect_image', 'nect_export_png', 'nect_import_svg'}
        live = tool('nect_session')
        identity = {key: live[key] for key in ('session_id', 'document_id')}
        comp = core('inspect')['result']['compositions'][0]
        rng = random.Random(7821)
        commands = []
        for i in range(24):
            commands.append(dict(type='create_path', composition=comp['id'], parent='', id=f'path-{i}', name=f'Motif {i}',
                contours=[dict(id=f'contour-{i}', closed=False, points=[point(f'p-{i}-0', rng.randrange(40, 850), rng.randrange(40, 570)),
                                                                     point(f'p-{i}-1', rng.randrange(40, 850), rng.randrange(40, 570))])]))
        rev = apply(commands, 0)
        source = dict(object='path-0', point='p-0-0', field='x')
        target = dict(object='path-1', point='p-1-0', field='x')
        rev = apply([dict(type='link', target=target, binding=dict(source=source, scale=1, offset=12, mode='copy_local_value'))], rev)
        for i in range(36):
            value = 120 + i
            rev = apply([dict(type='set', ref=source, value=value)], rev)
            assert core('get', ref=target)['result']['evaluated'] == value + 12
        before = core('inspect')['result']
        failed = core('apply', expected_revision=rev, commands=[dict(type='set', ref=source, value=999),
            dict(type='set', ref=target, value=2)])
        assert not failed['ok'] and failed['error']['code'] == 'DRIVEN_PROPERTY'
        assert core('inspect')['result'] == before and failed['revision'] == rev
        stale = core('apply', expected_revision=rev-1, commands=[dict(type='set', ref=source, value=2)])
        assert stale['error']['code'] == 'REVISION_CONFLICT'
        rev = apply([dict(type='rename', object='path-0', name='Renamed source'),
            dict(type='reorder_points', object='path-0', contour='contour-0', order=['p-0-1', 'p-0-0']),
            dict(type='reorder_objects', composition=comp['id'], parent='', order=[f'path-{i}' for i in reversed(range(24))])], rev)
        assert core('get', ref=target)['result']['evaluated'] == 167
        assert core('resolve_name', name='Renamed source', point='p-0-0', field='x')['result'] == source
        assert core('undo', expected_revision=rev)['ok']; rev += 1
        assert core('redo', expected_revision=rev)['ok']; rev += 1
        definitions = core('operator_types')['result']
        duplicate_before = core('inspect')['result']
        duplicated = core('apply', expected_revision=rev, commands=[dict(type='duplicate_objects', objects=['path-0','path-1'], prefix='mcp-copy')])
        assert duplicated['ok'], duplicated
        rev = duplicated['revision']
        assert duplicated['result']['created_ids'] == ['mcp-copy-1','mcp-copy-2']
        copied_objects = {o['id']: o for o in core('inspect')['result']['objects']}
        copied_target = next(p for p in copied_objects['mcp-copy-2']['contours'][0]['points'] if p['x'].get('binding'))
        copied_source = copied_target['x']['binding']['source']
        assert copied_source['object'] == 'mcp-copy-1'
        assert copied_source['point'] != 'p-0-0'
        assert core('undo', expected_revision=rev)['ok']; rev += 1
        assert core('inspect')['result'] == duplicate_before
        fill = next(x['template'] for x in definitions if x['type']=='nect.paint.fill')
        repeat = next(x['template'] for x in definitions if x['type']=='nect.shape.repeater')
        fill['id']='motif-fill';fill['parameters']['r']['literal']=.8
        repeat['id']='motif-repeat'
        rev = apply([dict(type='add_operation',object='path-0',index=0,operation=fill),
            dict(type='add_operation',object='path-0',index=2,operation=repeat)],rev)
        copies=3
        for _ in range(16):
            copies=rng.choice([n for n in range(2,8) if n!=copies])
            response=core('apply',expected_revision=rev,commands=[dict(type='set',
                ref=dict(object='path-0',point='',field='op.motif-repeat.copies'),value=copies)])
            assert response['ok'] and 'path-0' in response['result']['changed_ids']
            rev=response['revision']
            plan=core('render_plan',object='path-0')['result']
            assert plan['path_instances']==copies and len(plan['paint_layers'])==2*copies
        expected_svg_paths=23+2*copies
        gradients=core('gradient_types')['result']
        gradient=next(x['template'] for x in gradients if x['type']=='linear')
        gradient['id']='motif-gradient'
        rev=apply([dict(type='set_gradient',object='path-0',operation='motif-fill',gradient=gradient)],rev)
        stop_ref=dict(object='path-0',point='',field='op.motif-fill.gradient.motif-gradient.stop.start-stop.r')
        rev=apply([dict(type='set',ref=stop_ref,value=.75),dict(type='link',
            target=dict(object='path-1',point='',field='stroke.r'),
            binding=dict(source=stop_ref,scale=1,offset=0,mode='copy_local_value'))],rev)
        assert core('get',ref=dict(object='path-1',point='',field='stroke.r'))['result']['evaluated']==.75
        plan=core('render_plan',object='path-0')['result']
        assert sum('gradient' in x for x in plan['paint_layers'])==copies
        first=comp['artboards'][0]
        child=dict(id='alternate-frame',name='Alternate crop',x=100,y=50,width=160,height=240,
            parent_size=dict(artboard=first['id'],width=True,height=False))
        rev=apply([dict(type='add_artboard',composition=comp['id'],artboard=child,index=1)],rev)
        first=dict(first,width=700)
        changed=core('apply',expected_revision=rev,commands=[dict(type='update_artboard',composition=comp['id'],artboard=first)])
        assert changed['ok'] and first['id'] in changed['result']['changed_ids'] and child['id'] in changed['result']['changed_ids']
        rev=changed['revision']
        rev=apply([dict(type='reorder_artboards',composition=comp['id'],order=[child['id'],first['id']])],rev)
        boards=core('artboards',composition=comp['id'])['result']
        assert boards[0]['authored']['id']==child['id'] and boards[0]['authored']['width']==160
        assert boards[0]['evaluated']['width']==700 and boards[0]['evaluated']['height']==240
        crop_svg=core('export_svg',composition=comp['id'],artboard=child['id'])['result']
        assert ET.fromstring(crop_svg).attrib['viewBox']=='100 50 700 240'
        text_source=core('text_defaults')['result'];text_source.update(id='title-source',content='\u82b1\u306e\u5f62\nNect 2026',direction='vertical')
        rev=apply([dict(type='create_text',composition=comp['id'],parent='',id='title',name='Editable title',source=text_source)],rev)
        text_source['content']='\u82b1\u306e\u8a18\u61b6\nNect 2026'
        rev=apply([dict(type='update_text',object='title',source=text_source),dict(type='link',
            target=dict(object='title',point='',field='text.font_size'),binding=dict(source=source,scale=.2,offset=0,mode='copy_local_value'))],rev)
        assert core('get',ref=dict(object='title',point='',field='text.font_size'))['result']['evaluated']==31
        peer_source=core('text_defaults')['result'];peer_source.update(id='typed-peer-source',content='Peer text',family='Different Test Family',
            locale='fr-FR',layout='frame',direction='vertical',alignment='center')
        rev=apply([dict(type='create_text',composition=comp['id'],parent='',id='typed-peer',name='Second typed source',source=peer_source)],rev)
        readonly_fields=['text.content','text.family','text.locale','text.layout','text.direction','text.alignment']
        text_values={
            'title':[text_source['content'],text_source['family'],text_source['locale'],text_source['layout'],text_source['direction'],text_source['alignment']],
            'typed-peer':[peer_source['content'],peer_source['family'],peer_source['locale'],peer_source['layout'],peer_source['direction'],peer_source['alignment']],
        }
        properties_before=core('properties')['result']
        for object_id,expected_values in text_values.items():
            entries=[entry for entry in properties_before if entry['ref']['object']==object_id and entry['ref']['field'] in readonly_fields]
            assert len(entries)==6 and {entry['ref']['field'] for entry in entries}==set(readonly_fields)
            for field,value in zip(readonly_fields,expected_values):
                ref=dict(object=object_id,point='',field=field)
                entry=next(item for item in entries if item['ref']==ref)
                result=core('get',ref=ref)['result']
                kind='string' if field in readonly_fields[:3] else 'enum'
                assert result==entry and result['type']==kind and result['authored']==dict(literal=value)
                assert result['evaluated']==value and result['link'] is False and result['expression'] is False
                if kind=='enum':
                    choices={'text.layout':['auto','frame'],'text.direction':['horizontal','vertical'],
                        'text.alignment':['start','center','end']}[field]
                    assert result['choices']==choices
        assert core('resolve_name',name='Editable title',point='',field='text.content')['result']==dict(object='title',point='',field='text.content')
        before_reads=tool('nect_session')
        properties_after=core('properties')['result']
        core('get',ref=dict(object='title',point='',field='text.content'))
        core('resolve_name',name='Editable title',point='',field='text.content')
        assert properties_after==properties_before and tool('nect_session')['revision']==before_reads['revision']==rev
        invalid=core('get',ref=dict(object='title',point='',field='text.unregistered'))
        assert not invalid['ok'] and invalid['error']['code']=='UNKNOWN_TEXT_PROPERTY'
        invalid=core('get',ref=dict(object='title',point='a-point',field='text.content'))
        assert not invalid['ok'] and invalid['error']['code']=='INVALID_TEXT_REF'
        invalid=core('get',ref=dict(object='path-0',point='',field='text.content'))
        assert not invalid['ok'] and invalid['error']['code']=='TYPE_MISMATCH'
        rejected=core('apply',expected_revision=rev,commands=[dict(type='link_properties',targets=[dict(object='title',point='',field='text.font_size')],
            source=dict(object='title',point='',field='text.content'),relative=False)])
        assert not rejected['ok'] and rejected['error']['code']=='MISSING_REFERENCE' and rejected['revision']==rev
        rejected=core('apply',expected_revision=rev,commands=[dict(type='set_expression',targets=[dict(object='title',point='',field='text.content')],
            expression=dict(source='1',version=1),replace_binding=False)])
        assert not rejected['ok'] and rejected['revision']==rev
        current_document=core('inspect')['result'];current_comp=next(item for item in current_document['compositions'] if item['id']==comp['id'])
        rev=apply([dict(type='rename',object='title',name='Renamed typed title'),dict(type='reorder_objects',composition=comp['id'],parent='',
            order=['title']+[object_id for object_id in current_comp['roots'] if object_id!='title'])],rev)
        assert core('resolve_name',name='Renamed typed title',point='',field='text.content')['result']==dict(object='title',point='',field='text.content')
        text_source['content']='Edited through UpdateText'
        rev=apply([dict(type='update_text',object='title',source=text_source)],rev)
        assert core('get',ref=dict(object='title',point='',field='text.content'))['result']['evaluated']=='Edited through UpdateText'
        rev=apply([dict(type='delete_objects',objects=['typed-peer'])],rev)
        layout=core('text_layout',object='title')['result'];assert layout['glyph_count']>0 and layout['used_fonts']
        assert core('export_plan',composition=comp['id'],artboard=first['id'])['result']['text_policy']=='outlines'
        expected_svg_paths+=1
        brand=dict(id='brand-color',name='Brand accent',space='srgb',profile='srgb',alpha='straight',
            rgba=[{'literal':v} for v in (.2,.4,.6,.8)])
        brand_ref=dict(object='brand-color',point='',field='color')
        title_color=dict(object='title',point='',field='op.title-fill.color')
        gradient_color=dict(object='path-0',point='',field='op.motif-fill.gradient.motif-gradient.stop.start-stop.color')
        independent_color=dict(object='path-2',point='',field='op.path-2-stroke.color')
        rev=apply([dict(type='create_named_color',color=brand),dict(type='link_color',target=title_color,source=brand_ref),
            dict(type='link_color',target=gradient_color,source=brand_ref),dict(type='set_color',ref=independent_color,
            value=dict(space='srgb',profile='srgb',alpha='straight',rgba=[.2,.4,.6,.8]))],rev)
        changed=core('apply',expected_revision=rev,commands=[dict(type='rename_named_color',color='brand-color',name='Linked accent'),
            dict(type='set_color',ref=brand_ref,value=dict(space='srgb',profile='srgb',alpha='straight',rgba=[.7,.3,.2,.9]))])
        assert changed['ok'] and {'brand-color','title','path-0','path-1'}.issubset(changed['result']['changed_ids'])
        assert 'path-2' not in changed['result']['changed_ids'];rev=changed['revision']
        discovered_color=core('resolve_name',name='Linked accent',point='',field='color')['result']
        assert discovered_color==brand_ref
        assert core('get',ref=discovered_color)['result']['evaluated']['rgba']==[.7,.3,.2,.9]
        assert core('get',ref=title_color)['result']['evaluated']['rgba']==[.7,.3,.2,.9]
        assert core('get',ref=independent_color)['result']['evaluated']['rgba']==[.2,.4,.6,.8]
        assert core('get',ref=gradient_color)['result']['link']==brand_ref
        blocked=core('apply',expected_revision=rev,commands=[dict(type='delete_named_color',color='brand-color')])
        assert not blocked['ok'] and blocked['error']['code']=='MISSING_REFERENCE' and blocked['revision']==rev
        assert core('used_colors')['result']['equal_values_imply_link'] is False
        primitives={item['type']:item['template'] for item in core('primitive_types')['result']}
        polygon=primitives['nect.shape.polygon'];polygon['id']='linked-polygon-source'
        polygon['parameters']['points']={'literal':6}
        star=primitives['nect.shape.star'];star['id']='linked-star-source'
        polygon_count=dict(object='linked-polygon',point='',field='generator.points')
        star_count=dict(object='linked-star',point='',field='generator.points')
        star['parameters']['points']={'literal':5,'binding':dict(source=polygon_count,scale=1,offset=0,mode='copy_local_value')}
        rev=apply([dict(type='create_primitive',composition=comp['id'],parent='',id='linked-polygon',name='Linked polygon',source=polygon),
                   dict(type='create_primitive',composition=comp['id'],parent='',id='linked-star',name='Linked star',source=star)],rev)
        assert core('get',ref=star_count)['result']['evaluated']==6
        corrected_vertex=dict(object='linked-star',point='linked-star-source-outer-1-6',field='x')
        rev=apply([dict(type='set',ref=corrected_vertex,value=123)],rev)
        before_topology=core('inspect')['result']
        blocked=core('apply',expected_revision=rev,commands=[dict(type='set',ref=polygon_count,value=7)])
        assert not blocked['ok'] and blocked['error']['code']=='UNRESOLVED_POINT_EDIT' and blocked['revision']==rev
        assert core('inspect')['result']==before_topology
        rev=apply([dict(type='clear_point_edit',object='linked-star'),dict(type='set',ref=polygon_count,value=7)],rev)
        assert core('get',ref=star_count)['result']['evaluated']==7
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==before_topology and core('get',ref=corrected_vertex)['result']['evaluated']==123
        expected_svg_paths+=2
        # Structure and transform following remain separate across GUI/MCP/native.
        rev=apply([dict(type='group_contiguous',composition=comp['id'],parent='',members=['path-3','path-2'],id='transform-group',name='Structure group'),
            dict(type='set',ref=dict(object='transform-group',point='',field='transform.tx'),value=100),
            dict(type='set',ref=dict(object='path-3',point='',field='transform.tx'),value=20),
            dict(type='set',ref=dict(object='path-2',point='',field='transform.tx'),value=30),
            dict(type='set_transform_parent',object='path-2',parent='path-3',preserve_world=False)],rev)
        transforms=lambda:{x['object']:x for x in core('transforms')['result']}
        assert transforms()['path-2']['world'][4]==150
        followed=core('apply',expected_revision=rev,commands=[dict(type='set',ref=dict(object='path-3',point='',field='transform.tx'),value=40)])
        assert followed['ok'] and {'path-3','path-2'}.issubset(followed['result']['changed_ids']);rev=followed['revision']
        assert transforms()['path-2']['world'][4]==170
        rev=apply([dict(type='center_anchor',object='path-3'),dict(type='transform_around_anchor',object='path-3',rotation=90,scale_x=1,scale_y=1),
            dict(type='set_position',object='path-3',x=320,y=180)],rev)
        assert all(abs(v-e)<1e-8 for v,e in zip(transforms()['path-3']['position'],[320,180]))
        before_world=transforms()['path-2']['world']
        for parent in (None,'path-3'):
            rev=apply([dict(type='set_transform_parent',object='path-2',parent=parent,preserve_world=True)],rev)
            assert all(abs(v-e)<1e-8 for v,e in zip(transforms()['path-2']['world'],before_world))
        before_cycle=core('inspect')['result']
        rejected=core('apply',expected_revision=rev,commands=[dict(type='set_transform_parent',object='path-3',parent='path-2',preserve_world=False)])
        assert not rejected['ok'] and rejected['error']['code']=='TRANSFORM_CYCLE' and rejected['revision']==rev
        assert core('inspect')['result']==before_cycle
        # One snapshot serves every batch target, including relative links.
        batch_refs=[dict(object=f'path-{i}',point='',field='transform.tx') for i in (10,11,12)]
        rev=apply([dict(type='set',ref=ref,value=value) for ref,value in zip(batch_refs,(100,200,300))],rev)
        batch=core('apply',expected_revision=rev,commands=[dict(type='edit_properties',targets=batch_refs,value=10,relative=True)])
        assert batch['ok'] and {'path-10','path-11','path-12'}.issubset(batch['result']['changed_ids']);rev=batch['revision']
        assert [core('get',ref=ref)['result']['evaluated'] for ref in batch_refs]==[110,210,310]
        before_batch=core('inspect')['result']
        rev=apply([dict(type='edit_properties',targets=batch_refs,value=400,relative=False)],rev)
        assert [core('get',ref=ref)['result']['evaluated'] for ref in batch_refs]==[400,400,400]
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==before_batch
        batch_source=dict(object='path-9',point='',field='transform.tx')
        rev=apply([dict(type='link_properties',targets=batch_refs,source=batch_source,relative=True)],rev)
        rev=apply([dict(type='set',ref=batch_source,value=10)],rev)
        assert [core('get',ref=ref)['result']['evaluated'] for ref in batch_refs]==[120,220,320]
        before_rejected=core('inspect')['result']
        rejected=core('apply',expected_revision=rev,commands=[dict(type='edit_properties',targets=batch_refs,value=500,relative=False)])
        assert not rejected['ok'] and rejected['error']['code']=='DRIVEN_PROPERTY' and core('inspect')['result']==before_rejected
        rev=apply([dict(type='unlink_properties',targets=batch_refs)],rev)
        rev=apply([dict(type='set',ref=batch_source,value=20)],rev)
        assert [core('get',ref=ref)['result']['evaluated'] for ref in batch_refs]==[120,220,320]
        old_transforms=transforms();before_translation=core('inspect')['result']
        rev=apply([dict(type='translate_objects',objects=['path-3','path-2'],dx=25,dy=-15)],rev)
        new_transforms=transforms()
        for id_ in ('path-3','path-2'):
            expected_world=old_transforms[id_]['world'][:];expected_world[4]+=25;expected_world[5]-=15
            assert all(abs(v-e)<1e-8 for v,e in zip(new_transforms[id_]['world'],expected_world))
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==before_translation
        old_transforms=transforms()
        rev=apply([dict(type='transform_objects',objects=['path-3','path-2'],rotation=90,scale_x=1,scale_y=1,pivot=[0,0])],rev)
        rotated_transforms=transforms()
        for id_ in ('path-3','path-2'):
            a,b,c,d,tx,ty=old_transforms[id_]['world']
            assert all(abs(v-e)<1e-8 for v,e in zip(rotated_transforms[id_]['world'],[-b,a,-d,c,-ty,tx]))
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==before_translation
        stroke_before=core('inspect')['result']
        rev=apply([dict(type='stroke_style',object='path-2',operation='path-2-stroke',line_cap='round',line_join='bevel',miter_limit=6)],rev)
        styled=next(o for o in core('inspect')['result']['objects'] if o['id']=='path-2')
        stroke=next(op for op in styled['stack'] if op['id']=='path-2-stroke')
        assert stroke['version']==2 and stroke['line_cap']=='round' and stroke['line_join']=='bevel'
        assert core('get',ref=dict(object='path-2',point='',field='op.path-2-stroke.miter_limit'))['result']['evaluated']==6
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==stroke_before
        # Formulas use the live Session and survive its normal save/restart path.
        formula='ref("path-9","","transform.tx") * 2 + 5'
        rev=apply([dict(type='set_expression',targets=batch_refs,expression=dict(source=formula,version=1),replace_binding=False)],rev)
        assert all(core('get',ref=ref)['result']['evaluated']==45 for ref in batch_refs)
        formula_before=core('inspect')['result']
        rejected=core('apply',expected_revision=rev,commands=[dict(type='set_expression',targets=batch_refs,expression=dict(source='1 / 0',version=1),replace_binding=False)])
        assert not rejected['ok'] and core('inspect')['result']==formula_before
        changed=core('apply',expected_revision=rev,commands=[dict(type='set',ref=batch_source,value=30)])
        assert changed['ok'] and {'path-9','path-10','path-11','path-12'}.issubset(changed['result']['changed_ids']);rev=changed['revision']
        assert all(core('get',ref=ref)['result']['evaluated']==65 for ref in batch_refs)
        assert core('expression_language')['result']['source_bytes']==4096
        history_before=core('history')['result'];history_state=history_before['current_id']
        historical_document=core('inspect')['result']
        for step in range(80):
            rev=apply([dict(type='set',ref=source,value=180+step)],rev)
        long_history=core('history')['result'];later_state=long_history['current_id']
        assert len(long_history['states'])>80 and long_history['cross_restart'] is False
        assert long_history['retained_bytes']<=long_history['max_bytes']
        restored=core('restore_history',state_id=history_state,expected_revision=rev)
        assert restored['ok'] and restored['revision']==rev+1 and {'path-0','path-1'}.issubset(restored['result']['changed_ids']);rev+=1
        assert core('inspect')['result']==historical_document
        assert core('restore_history',state_id=later_state,expected_revision=rev)['ok'];rev+=1
        assert core('get',ref=target)['result']['evaluated']==271
        assert core('restore_history',state_id=history_state,expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==historical_document
        unchanged=core('restore_history',state_id=history_state,expected_revision=rev)
        assert unchanged['ok'] and unchanged['revision']==rev and unchanged['result']['changed'] is False
        native = temp / 'scenario.nect'
        saved = tool('nect_file', dict(identity, op='save', path=str(native), expected_revision=rev))
        assert saved['ok'], saved
        expected = core('inspect')['result']
        png = tool('nect_export_png',dict(identity,op='export_png',expected_revision=rev,
            path=str(temp/'output.png'),composition=comp['id'],artboard=comp['artboards'][0]['id'],scale=1,background='transparent'))
        assert png['ok'] and png['revision']==rev,png
        png_bytes=(temp/'output.png').read_bytes()
        assert png_bytes[:8]==b'\x89PNG\r\n\x1a\n'
        assert struct.unpack('>II',png_bytes[16:24])==(png['result']['width'],png['result']['height'])
        svg = core('export_svg', composition=comp['id'], artboard=comp['artboards'][0]['id'])['result']
        assert len(ET.fromstring(svg).findall('.//{http://www.w3.org/2000/svg}path')) == expected_svg_paths
        svg_root=ET.fromstring(svg);ns='{http://www.w3.org/2000/svg}'
        exported_follower=next(g for g in svg_root.iter(ns+'g') if g.attrib.get('id')=='path-2')
        matrix=[float(v) for v in exported_follower.attrib['transform'].removeprefix('matrix(').removesuffix(')').split()]
        assert all(abs(v-e)<1e-8 for v,e in zip(matrix,transforms()['path-2']['world']))
        structural_group=next(g for g in svg_root.iter(ns+'g') if g.attrib.get('id')=='transform-group')
        assert 'transform' not in structural_group.attrib, 'SVG must not double-apply structural transforms to external followers'
        gradient_defs=svg_root.findall('.//'+ns+'linearGradient')
        assert len(gradient_defs)==copies and len({x.attrib['id'] for x in gradient_defs})==copies
        assert all(x.attrib['gradientUnits']=='userSpaceOnUse' for x in gradient_defs)
        assert float(gradient_defs[0].findall(ns+'stop')[0].attrib['offset'])==0
        assert core('inspect')['result'] == expected
        assert json.loads(native.read_text(encoding='utf-8')) == expected
        # Geometry masks and common compositing share the formal MCP Session,
        # including a hidden editable source and retained native/recovery state.
        mask_source=dict(primitives['nect.shape.circle'])
        mask_source['id']='mcp-mask-source'
        # Templates were only modified for Polygon/Star above.
        rev=apply([dict(type='create_primitive',composition=comp['id'],parent='',id='mcp-mask',
                        name='Hidden mask',source=mask_source),
                   dict(type='mask_objects',composition=comp['id'],parent='',members=['linked-star','mcp-mask'],
                        id='mcp-masked-group',mask_id='mcp-geometry-clip',name='Masked star',top=True),
                   dict(type='set_compositing',object='mcp-masked-group',blend='screen',isolated=False),
                   dict(type='set',ref=dict(object='mcp-masked-group',point='',field='composite.opacity'),value=.65)],rev)
        masked=core('inspect')['result']
        assert next(o for o in masked['objects'] if o['id']=='mcp-mask')['visible'] is False
        mask_ref=dict(object='mcp-mask',point='',field='generator.radius')
        changed=core('apply',expected_revision=rev,commands=[dict(type='set',ref=mask_ref,value=75)])
        assert changed['ok'] and {'mcp-mask','mcp-masked-group'}.issubset(changed['result']['changed_ids']);rev=changed['revision']
        plan=core('compositing_plan',composition=comp['id'])['result']
        group=next(n for n in plan['roots'] if n['object']=='mcp-masked-group')
        assert group['isolated'] and group['opacity']==.65 and group['mask']['source']=='mcp-mask'
        assert group['blend']=='screen' and plan['backdrop']=='transparent'
        before_bad=core('inspect')['result']
        bad=core('apply',expected_revision=rev,commands=[dict(type='set_visibility',object='mcp-mask',visible=True),
            dict(type='set_compositing',object='mcp-masked-group',blend='unsupported-add',isolated=False)])
        assert not bad['ok'] and bad['revision']==rev and core('inspect')['result']==before_bad
        mask_svg=core('export_svg',composition=comp['id'],artboard=comp['artboards'][0]['id'])['result']
        assert 'id="mcp-geometry-clip"' in mask_svg and 'mix-blend-mode:screen' in mask_svg
        assert 'id="mcp-mask"' not in mask_svg
        # Retained Offset also shapes a hidden mask source through formal MCP.
        offset_template=next(v['template'] for v in core('operator_types')['result'] if v['type']=='nect.shape.offset')
        offset_template['id']='mcp-offset';offset_template['line_join']='round'
        before_offset=core('inspect')['result']
        mask_object=next(o for o in before_offset['objects'] if o['id']=='mcp-mask')
        rev=apply([dict(type='add_operation',object='mcp-mask',operation=offset_template,index=len(mask_object['stack']))],rev)
        amount=dict(object='mcp-mask',point='',field='op.mcp-offset.amount')
        changed=core('apply',expected_revision=rev,commands=[dict(type='set_expression',targets=[amount],
            expression=dict(version=1,source='ref("mcp-mask","","generator.radius") / 5'),replace_binding=False)])
        assert changed['ok'] and {'mcp-mask','mcp-masked-group'}.issubset(changed['result']['changed_ids']);rev=changed['revision']
        assert core('get',ref=amount)['result']['evaluated']==15
        after_offset=core('inspect')['result']
        bad=core('apply',expected_revision=rev,commands=[dict(type='operation_options',object='mcp-mask',operation='mcp-offset',composite='below',fill_rule='nonzero',line_join='unknown')])
        assert not bad['ok'] and core('inspect')['result']==after_offset
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert core('get',ref=amount)['result']['evaluated']==10
        assert core('redo',expected_revision=rev)['ok'];rev+=1
        assert core('inspect')['result']==after_offset
        # Local image lifecycle uses the same Session through formal MCP.
        def png(rgb):
            def chunk(kind, data):
                return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
            return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',2,2,8,2,0,0,0))+chunk(b'IDAT',zlib.compress((b'\0'+bytes(rgb)*2)*2))+chunk(b'IEND',b'')
        image_path=temp/'linked.png';original_image=png((220,80,30));image_path.write_bytes(original_image)
        def image(action,**kwargs):
            return tool('nect_image',dict(identity,op='asset',asset='mcp-image-asset',action=action,expected_revision=rev,**kwargs))
        imported=tool('nect_image',dict(identity,op='import_image',expected_revision=rev,path=str(image_path),mode='linked',
            composition=comp['id'],parent='',asset='mcp-image-asset',id='mcp-image',name='Linked artwork',x=40,y=60))
        assert imported['ok'],imported
        rev=imported['revision'];assert image('status')['result']['state']=='current'
        rev=apply([dict(type='set',ref=dict(object='mcp-image',point='',field='image.width'),value=160),
                   dict(type='set',ref=dict(object='mcp-image',point='',field='image.height'),value=120),
                   dict(type='create_image',composition=comp['id'],parent='',id='mcp-image-copy',name='Shared image',
                        source=dict(asset='mcp-image-asset',width=dict(literal=80),height=dict(literal=60))),
                   dict(type='set_mask',object='mcp-image',mask=dict(id='mcp-image-mask',source='mcp-mask',version=1,enabled=True,fill_rule='nonzero')),
                   dict(type='set_compositing',object='mcp-image',blend='multiply',isolated=False)],rev)
        accepted=core('inspect')['result'];assert base64.b64decode(accepted['raster_assets'][0]['bytes'])==original_image
        image_path.write_bytes(png((20,180,220)))
        assert image('check')['result']['state']=='changed' and core('inspect')['result']==accepted
        reload=image('reload');assert reload['ok'] and {'mcp-image','mcp-image-copy','mcp-image-asset'}.issubset(reload['result']['changed_ids']);rev=reload['revision']
        assert core('get',ref=dict(object='mcp-image',point='',field='image.width'))['result']['evaluated']==160
        assert core('undo',expected_revision=rev)['ok'];rev+=1;assert core('inspect')['result']==accepted
        assert core('redo',expected_revision=rev)['ok'];rev+=1
        linked_before_missing=core('inspect')['result'];image_path.unlink()
        assert image('check')['result']['state']=='missing'
        failed=image('reload');assert not failed['ok'] and failed['revision']==rev and core('inspect')['result']==linked_before_missing
        relink=temp/'replacement.png';relink.write_bytes(original_image)
        changed=image('relink',path=str(relink));assert changed['ok'];rev=changed['revision']
        embedded=image('embed');assert embedded['ok'];rev=embedded['revision'];relink.unlink()
        assert image('status')['result']['state']=='embedded'
        assert core('undo',expected_revision=rev)['ok'];rev+=1
        assert image('check')['result']['state']=='missing'
        # Save a missing link intentionally: restart/recovery must keep accepted pixels.
        image_svg=core('export_svg',composition=comp['id'],artboard=comp['artboards'][0]['id'])
        assert image_svg['ok'] and image_svg['result'].count('data:image/png;base64,')==1
        assert 'bytes' not in core('assets')['result'][0]
        # Automatic protection must reach a verified receipt without an explicit
        # Save/recover call, through the real desktop event loop and worker.
        rev=apply([dict(type='set',ref=source,value=156)],rev)
        expected=core('inspect')['result']
        deadline=time.monotonic()+8
        while time.monotonic()<deadline:
            durable=tool('nect_session')['persistence']
            if durable['saved_revision']==rev and durable['recovery_revision']==rev:
                break
            time.sleep(.05)
        else:
            raise AssertionError(('Live save did not finish',durable))
        assert json.loads(native.read_text(encoding='utf-8'))==expected
        recovery = temp / 'recovery' / (identity['session_id'] + '.nect')
        # Actual abnormal termination: recover the exact committed snapshot in a new process.
        desktop.kill();desktop.wait(timeout=5)
        desktop, _ = start(endpoint, temp, native)
        assert core('apply', expected_revision=0, commands=[dict(type='set', ref=source, value=1)])['error']['code'] == 'SESSION_CONFLICT'
        live = tool('nect_session'); identity = {key: live[key] for key in ('session_id', 'document_id')}
        assert core('inspect')['result'] == expected
        assert tool('nect_image',dict(identity,op='asset',asset='mcp-image-asset',action='status',expected_revision=0))['result']['state']=='unchecked'
        assert core('get', ref=target)['result']['evaluated'] == 168
        assert len(core('history')['result']['states'])==1
        # Recovery is opened through the same formal MCP surface as an unnamed
        # document, so subsequent live saves cannot replace the recovery source.
        assert tool('nect_file',dict(identity,op='open_recovery',path=str(recovery),expected_revision=0))['ok']
        live=tool('nect_session');identity={key:live[key] for key in ('session_id','document_id')}
        assert live['file']=='' and live['persistence']['saved_revision'] is None
        assert core('inspect')['result']==expected
        align_commands=[]
        for index,x in enumerate((10,110,270)):
            align_commands.append(dict(type='create_path',composition=comp['id'],parent='',id=f'align-{index}',name=f'Align {index}',
                contours=[dict(id=f'align-contour-{index}',closed=False,points=[point(f'align-{index}-a',x,5),point(f'align-{index}-b',x+20,5)])]))
        alignment_rev=apply(align_commands,0)
        alignment_before=core('inspect')['result']
        alignment_rev=apply([dict(type='align_objects',objects=['align-0','align-1'],axis='x',alignment='min',artboard=None)],alignment_rev)
        assert core('get',ref=dict(object='align-1',point='',field='transform.tx'))['result']['evaluated']==-100
        assert core('undo',expected_revision=alignment_rev)['ok'] and core('inspect')['result']==alignment_before
        spacing_rev=apply([dict(type='distribute_objects',objects=['align-2','align-0','align-1'],axis='x')],alignment_rev+1)
        assert core('get',ref=dict(object='align-1',point='',field='transform.tx'))['result']['evaluated']==30
        assert core('undo',expected_revision=spacing_rev)['ok'] and core('inspect')['result']==alignment_before
        svg_input=temp/'original-vector.svg'
        svg_input.write_text('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 40 30"><g fill="#c04020"><path d="M2 2h30v20h-30zM10 10A5 5 0 0 1 20 10"/><circle cx="20" cy="15" r="4"/></g></svg>',encoding='utf-8')
        vector_before=core('inspect')['result']
        vector=tool('nect_import_svg',dict(identity,op='import_svg',expected_revision=spacing_rev+1,path=str(svg_input),composition=comp['id'],prefix='mcp-vector',name='Vector',x=10,y=20))
        assert vector['ok'] and vector['result']['paths']==2 and vector['result']['root']=='mcp-vector'
        assert any(o['id']=='mcp-vector' and o['kind']=='group' for o in core('inspect')['result']['objects'])
        vector_document=core('inspect')['result']
        ungroup_revision=apply([dict(type='ungroup',composition=comp['id'],parent='',group='mcp-vector')],vector['revision'])
        assert not any(o['id']=='mcp-vector' for o in core('inspect')['result']['objects'])
        assert core('undo',expected_revision=ungroup_revision)['ok'] and core('inspect')['result']==vector_document
        assert core('undo',expected_revision=ungroup_revision+1)['ok'] and core('inspect')['result']==vector_before
        receipt = dict(status='PASS', seed=7821, paths=24, semantic_mutations=rev,
            mcp_initialize_list_call=True, same_live_desktop_session=True, atomic_failure=True, independent_duplication=True, geometric_alignment_undo=True, equal_gap_spacing_undo=True, editable_svg_undo=True,
            stale_session_rejected=True, native_restart=True, abnormal_exit_recovery=True,
            independent_svg_parser_paths=expected_svg_paths, ordered_stack_readback=True,
            automatic_native_and_recovery_receipts=True, recovery_op_detaches_source=True, image_lifecycle_native_recovery=True, gui_performance_claim=False)
        print(json.dumps(receipt, indent=2))
finally:
    if mcp:
        mcp.stdin.close()
        try:
            mcp.wait(timeout=5)
        except subprocess.TimeoutExpired:
            mcp.kill();mcp.wait()
    if desktop and desktop.poll() is None:
        desktop.kill();desktop.wait(timeout=5)

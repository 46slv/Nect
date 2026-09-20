"""Seeded formal MCP client -> sidecar -> live Qt Session, including restart.

Uses a temporary document and offscreen desktop; this is semantic/persistence
evidence, not a viewport performance or visual quality claim.
"""
import json
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
        assert {t['name'] for t in rpc('tools/list')['result']['tools']} == {'nect_session', 'nect_command', 'nect_file'}
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
        svg = core('export_svg', composition=comp['id'], artboard=comp['artboards'][0]['id'])['result']
        assert len(ET.fromstring(svg).findall('.//{http://www.w3.org/2000/svg}path')) == expected_svg_paths
        svg_root=ET.fromstring(svg);ns='{http://www.w3.org/2000/svg}'
        gradient_defs=svg_root.findall('.//'+ns+'linearGradient')
        assert len(gradient_defs)==copies and len({x.attrib['id'] for x in gradient_defs})==copies
        assert all(x.attrib['gradientUnits']=='userSpaceOnUse' for x in gradient_defs)
        assert float(gradient_defs[0].findall(ns+'stop')[0].attrib['offset'])==0
        assert core('inspect')['result'] == expected
        assert json.loads(native.read_text(encoding='utf-8')) == expected
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
        assert core('get', ref=target)['result']['evaluated'] == 168
        assert len(core('history')['result']['states'])==1
        # Recovery is opened through the same formal MCP surface as an unnamed
        # document, so subsequent live saves cannot replace the recovery source.
        assert tool('nect_file',dict(identity,op='open_recovery',path=str(recovery),expected_revision=0))['ok']
        live=tool('nect_session');identity={key:live[key] for key in ('session_id','document_id')}
        assert live['file']=='' and live['persistence']['saved_revision'] is None
        assert core('inspect')['result']==expected
        receipt = dict(status='PASS', seed=7821, paths=24, semantic_mutations=rev,
            mcp_initialize_list_call=True, same_live_desktop_session=True, atomic_failure=True,
            stale_session_rejected=True, native_restart=True, abnormal_exit_recovery=True,
            independent_svg_parser_paths=expected_svg_paths, ordered_stack_readback=True,
            automatic_native_and_recovery_receipts=True, recovery_op_detaches_source=True, gui_performance_claim=False)
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

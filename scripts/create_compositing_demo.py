"""Author COLOUR / CUT through a fresh live Session and probe its retained masks.

Creates an editable native poster plus vector SVG. Existing output files and
nonempty Sessions are preserved; all artwork changes use ordinary commands.
"""
import argparse
import copy
import json
from pathlib import Path
import xml.etree.ElementTree as ET

from session_client import call


def create_study(endpoint, output):
    hello = call(endpoint, {'op': 'hello'})
    identity = {key: hello[key] for key in ('session_id', 'document_id')}
    revision = hello['revision']

    def require(condition, message):
        if not condition:
            raise RuntimeError(message)

    def core(op, **arguments):
        response = call(endpoint, dict(identity, op='core', request=dict(op=op, **arguments)))
        require(response['ok'], str(response))
        return response

    def apply(commands):
        nonlocal revision
        response = core('apply', expected_revision=revision, commands=commands)
        revision = response['revision']
        return response

    output = Path(output).resolve()
    document = core('inspect')['result']
    require(not document['objects'] and not document['named_colors'] and not document['collections'],
            'Use an empty Session; existing artwork is preserved.')
    require(len(document['compositions']) == 1 and len(document['compositions'][0]['artboards']) == 1,
            'Use a fresh Session with one Composition and Artboard.')
    require(output.suffix.lower() == '.nect' and not output.exists() and not output.with_suffix('.svg').exists(),
            'Choose unused .nect and .svg destinations.')
    require(not hello.get('file') or Path(hello['file']).resolve() != output, 'Preserve the opened source file.')

    def save():
        result = call(endpoint, dict(identity, op='save', path=str(output), expected_revision=revision))
        require(result['ok'], str(result))

    save()
    primitives = {item['type']: item['template'] for item in core('primitive_types')['result']}
    operators = {item['type']: item['template'] for item in core('operator_types')['result']}
    text_template = core('text_defaults')['result']
    require('Segoe UI' in core('text_fonts')['result'], 'This study requires installed Segoe UI.')
    composition = document['compositions'][0]
    board = dict(composition['artboards'][0], name='Colour / Cut', x=0, y=0, width=960, height=640)
    board.pop('parent_size', None)
    scalar = lambda value: dict(literal=value)
    ref = lambda owner, field: dict(object=owner, point='', field=field)
    set_ = lambda owner, field, value: dict(type='set', ref=ref(owner, field), value=value)
    palette = [('paper', 'Warm paper', 'F6EEDD'), ('navy', 'Midnight blue', '213447'),
               ('coral', 'Vermilion', 'E86C4A'), ('teal', 'Petrol blue', '4BA5A1'),
               ('rule', 'Pale rule', 'D9CCB4')]
    commands = [dict(type='update_artboard', composition=composition['id'], artboard=board)]
    for ink, name, hex_ in palette:
        rgba = [int(hex_[start:start + 2], 16) / 255 for start in (0, 2, 4)] + [1]
        commands.append(dict(type='create_named_color', color=dict(id='ink-' + ink, name=name,
            space='srgb', profile='srgb', alpha='straight', rgba=[scalar(v) for v in rgba])))
    paints, shape_ids, text_ids = {}, [], []

    def shape(id_, kind, ink, name=None, **parameters):
        source = copy.deepcopy(primitives['nect.shape.' + kind])
        source['id'] = id_ + '-source'
        source['parameters'].update({key: scalar(value) for key, value in parameters.items()})
        commands.append(dict(type='create_primitive', composition=composition['id'], parent='',
                             id=id_, name=name or id_.replace('-', ' ').title(), source=source))
        paints[id_] = ink
        shape_ids.append(id_)

    def text_(id_, content, x, y, size, tracking=0, weight=400):
        source = copy.deepcopy(text_template)
        source.update(id=id_ + '-source', content=content, family='Segoe UI', weight=weight)
        source['parameters'].update({key: scalar(value) for key, value in
            dict(origin_x=x, origin_y=y, font_size=size, tracking=tracking).items()})
        commands.append(dict(type='create_text', composition=composition['id'], parent='',
                             id=id_, name=content, source=source))
        paints[id_] = 'navy'
        text_ids.append(id_)

    shape('paper', 'rectangle', 'paper', center_x=480, center_y=320, width=960, height=640)
    shape('footer-rule', 'rectangle', 'rule', center_x=480, center_y=583, width=864, height=1)
    # The first three siblings become one retained circular clipping group.
    shape('left-field', 'rectangle', 'coral', center_x=353, center_y=343, width=350, height=340)
    shape('left-stripe', 'rectangle', 'navy', center_x=189, center_y=343, width=8, height=365)
    shape('left-mask', 'circle', 'navy', 'Left circle / hidden editable source',
          center_x=353, center_y=343, radius=157)
    shape('right-field', 'rectangle', 'teal', center_x=564, center_y=343, width=340, height=340)
    shape('right-ring', 'circle', 'paper', center_x=600, center_y=305, radius=68)
    shape('right-dot', 'circle', 'navy', center_x=630, center_y=416, radius=24)
    shape('right-mask', 'circle', 'navy', 'Right circle / hidden editable source',
          center_x=564, center_y=343, radius=157)
    text_('edition', 'NECT / FORM STUDIES', 48, 33, 11, tracking=2)
    text_('title', 'COLOUR / CUT', 45, 77, 62, tracking=.5, weight=700)
    text_('subtitle', 'Two fields. One shared edge.', 49, 158, 18)
    text_('left-caption', '01 / RETAINED GEOMETRY', 196, 517, 10, tracking=1)
    text_('right-caption', '02 / MULTIPLY', 552, 517, 10, tracking=1)
    text_('note', 'A circle contains the field. A second field changes its colour.', 196, 544, 12)
    text_('footer', 'EDITABLE MASKS / ORDERED PAINT / GROUP COMPOSITING', 48, 606, 9, tracking=.8)
    text_('folio', 'STUDY 06', 838, 605, 10, tracking=1)
    apply(commands)
    created = {item['id']: item for item in core('inspect')['result']['objects']}
    commands = []
    for id_ in shape_ids:
        for operation in created[id_]['stack']:
            commands.append(dict(type='enable_operation', object=id_, operation=operation['id'], enabled=False))
        fill = copy.deepcopy(operators['nect.paint.fill'])
        fill['id'] = id_ + '-fill'
        commands.append(dict(type='add_operation', object=id_, operation=fill, index=len(created[id_]['stack'])))
        commands.append(dict(type='link_color', target=ref(id_, f'op.{fill["id"]}.color'),
                             source=ref('ink-' + paints[id_], 'color')))
    for id_ in text_ids:
        fill = next(op for op in created[id_]['stack'] if op['type'] == 'nect.paint.fill')
        commands.append(dict(type='link_color', target=ref(id_, f'op.{fill["id"]}.color'), source=ref('ink-navy', 'color')))
    repeat = copy.deepcopy(operators['nect.shape.repeater'])
    repeat['id'] = 'stripe-repeat'
    repeat['parameters'].update({key: scalar(value) for key, value in dict(copies=16, position_x=22, position_y=0).items()})
    commands.append(dict(type='add_operation', object='left-stripe', operation=repeat,
                         index=len(created['left-stripe']['stack']) + 1))
    for side, members in [('left', ['left-field', 'left-stripe', 'left-mask']),
                          ('right', ['right-field', 'right-ring', 'right-dot', 'right-mask'])]:
        commands.append(dict(type='mask_objects', composition=composition['id'], parent='', members=members,
                             id=side + '-group', mask_id=side + '-clip', name=side.title() + ' / masked field', top=True))
    commands += [dict(type='set_compositing', object='right-group', blend='multiply', isolated=False),
                 set_('right-group', 'composite.opacity', .88)]
    apply(commands)
    initial = core('inspect')['result']
    initial_svg = core('export_svg', composition=composition['id'], artboard=board['id'])['result']
    nodes = {item['id']: item for item in initial['objects']}
    require(all(not nodes[side + '-mask']['visible'] for side in ('left', 'right')), 'Mask source leaked into normal artwork.')
    require(nodes['left-stripe']['source']['type'] == 'nect.shape.rectangle', 'Repeating primitive was flattened.')
    # Ordinary source/Group property edits propagate through the same Session.
    changed = apply([set_('left-mask', 'generator.radius', 124), set_('right-group', 'composite.opacity', .55)])
    require({'left-mask', 'left-group', 'right-group'}.issubset(changed['result']['changed_ids']),
            'Mask dependency owner missing from changed IDs.')
    require(core('export_svg', composition=composition['id'], artboard=board['id'])['result'] != initial_svg,
            'Mask/opacity edits did not affect exported artwork.')
    revision = core('undo', expected_revision=revision)['revision']
    require(core('inspect')['result'] == initial, 'One Undo did not restore exact authored artwork.')
    require(core('export_svg', composition=composition['id'], artboard=board['id'])['result'] == initial_svg,
            'One Undo did not restore the exact SVG projection.')
    plan = core('compositing_plan', composition=composition['id'])['result']
    require(plan['requires_compositing'] and plan['backdrop'] == 'transparent', 'Unexpected compositing contract.')
    export = core('export_plan', composition=composition['id'], artboard=board['id'])['result']
    require(not any(item['overflow'] or item['warnings'] for item in export['text']), 'Text needs visual/layout correction.')
    root = ET.fromstring(initial_svg)
    ns = '{http://www.w3.org/2000/svg}'
    require(len(root.findall('.//' + ns + 'clipPath')) == 2, 'SVG omitted a retained geometry mask.')
    require(not any(g.get('id') in ('left-mask', 'right-mask') for g in root.findall('.//' + ns + 'g')),
            'A hidden mask source was exported as visible artwork.')
    save()
    require(json.loads(output.read_text(encoding='utf-8')) == initial, 'Native readback differs from live authored state.')
    output.with_suffix('.svg').write_text(initial_svg, encoding='utf-8')
    return dict(native=str(output), svg=str(output.with_suffix('.svg')), revision=revision,
                masks=2, repeated_stripes=16, named_colors=len(palette), editable_text=len(text_ids),
                opacity=.88, blend='multiply', undo_restored_exact=True, native_readback_exact=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    print(json.dumps(create_study(args.endpoint, args.output), ensure_ascii=True, indent=2))

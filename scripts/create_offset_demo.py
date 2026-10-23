"""Create an editable Offset Paths study through a fresh live desktop Session."""
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

    def require(ok, why):
        if not ok:
            raise RuntimeError(why)

    def core(op, **arguments):
        result = call(endpoint, dict(identity, op='core', request=dict(op=op, **arguments)))
        require(result['ok'], str(result))
        return result

    def apply(commands):
        nonlocal revision
        result = core('apply', expected_revision=revision, commands=commands)
        revision = result['revision']
        return result

    output = Path(output).resolve()
    require(output.suffix.lower() == '.nect' and not output.exists() and not output.with_suffix('.svg').exists(),
            'Choose unused native/SVG destinations.')
    initial = core('inspect')['result']
    require(not initial['objects'] and not initial['named_colors'] and not initial['collections'], 'Use a fresh empty Session.')
    require(len(initial['compositions']) == 1 and len(initial['compositions'][0]['artboards']) == 1, 'Use one initial Composition/frame.')
    result = call(endpoint, dict(identity, op='save', path=str(output), expected_revision=revision))
    require(result['ok'], str(result))
    primitives = {v['type']: v['template'] for v in core('primitive_types')['result']}
    operators = {v['type']: v['template'] for v in core('operator_types')['result']}
    text_template = core('text_defaults')['result']
    comp = initial['compositions'][0]
    board = dict(comp['artboards'][0], name='Contour / 07', x=0, y=0, width=960, height=640)
    board.pop('parent_size', None)
    scalar = lambda value: dict(literal=value)
    ref = lambda owner, field: dict(object=owner, point='', field=field)
    commands = [dict(type='update_artboard', composition=comp['id'], artboard=board)]
    palette = [('paper', 'Midnight', '172C35'), ('type', 'Warm white', 'F7EEDC'),
               ('orange', 'Signal orange', 'FFB16D'), ('cyan', 'Ice blue', '77CFD5')]
    for ink, name, hex_ in palette:
        rgba = [int(hex_[i:i + 2], 16) / 255 for i in (0, 2, 4)] + [1]
        commands.append(dict(type='create_named_color', color=dict(id='ink-' + ink, name=name,
            space='srgb', profile='srgb', alpha='straight', rgba=[scalar(v) for v in rgba])))

    def primitive(id_, kind, **parameters):
        source = copy.deepcopy(primitives['nect.shape.' + kind])
        source['id'] = id_ + '-source'
        source['parameters'].update({k: scalar(v) for k, v in parameters.items()})
        commands.append(dict(type='create_primitive', composition=comp['id'], parent='', id=id_,
                             name=id_.replace('-', ' ').title(), source=source))

    primitive('paper', 'rectangle', center_x=480, center_y=320, width=960, height=640)
    shapes = []
    for i in range(7):
        circle = 'circle-' + str(i)
        star = 'star-' + str(i)
        primitive(circle, 'circle', center_x=269, center_y=353, radius=129)
        primitive(star, 'star', center_x=698, center_y=353, points=6, outer_radius=145, inner_radius=83, rotation=-90)
        shapes.extend([(circle, 'orange', -18 * i), (star, 'cyan', -11 * i)])

    texts = []

    def text_(id_, content, x, y, size, weight=400, tracking=0):
        source = copy.deepcopy(text_template)
        source.update(id=id_ + '-source', content=content, family='Segoe UI', weight=weight)
        source['parameters'].update({k: scalar(v) for k, v in dict(origin_x=x, origin_y=y, font_size=size, tracking=tracking).items()})
        commands.append(dict(type='create_text', composition=comp['id'], parent='', id=id_, name=content, source=source))
        texts.append(id_)

    text_('edition', 'NECT / FORM STUDIES', 48, 32, 11, tracking=2)
    text_('title', 'CONTOUR / 07', 45, 75, 62, 700, .5)
    text_('subtitle', 'One source. A family of outlines.', 49, 155, 18)
    text_('circle-label', '01 / EXPAND + CONTRACT', 123, 529, 11, tracking=1)
    text_('star-label', '02 / ROUND JOINS', 556, 529, 11, tracking=1)
    text_('footer', 'RETAINED SOURCES / LINKED AMOUNTS / OFFSET PATHS', 48, 602, 10, tracking=1)
    text_('folio', 'STUDY 07', 838, 602, 10, tracking=1)
    apply(commands)
    created = {o['id']: o for o in core('inspect')['result']['objects']}
    commands = []
    paper = created['paper']
    for operation in paper['stack']:
        commands.append(dict(type='enable_operation', object='paper', operation=operation['id'], enabled=False))
    fill = copy.deepcopy(operators['nect.paint.fill']);fill['id'] = 'paper-fill'
    commands.extend([dict(type='add_operation', object='paper', operation=fill, index=len(paper['stack'])),
                     dict(type='link_color', target=ref('paper', 'op.paper-fill.color'), source=ref('ink-paper', 'color'))])
    for id_, ink, delta in shapes:
        stroke = created[id_]['stack'][0]
        commands.extend([dict(type='set', ref=ref(id_, 'op.' + stroke['id'] + '.width'), value=2.1),
                         dict(type='link_color', target=ref(id_, 'op.' + stroke['id'] + '.color'), source=ref('ink-' + ink, 'color'))])
        offset = copy.deepcopy(operators['nect.shape.offset'])
        offset['id'] = id_ + '-offset';offset['line_join'] = 'round'
        offset['parameters']['amount'] = scalar(18 + delta)
        if id_ != 'circle-0':
            offset['parameters']['amount']['expression'] = dict(version=1,
                source='ref("circle-0","","op.circle-0-offset.amount") + ' + str(delta))
        commands.append(dict(type='add_operation', object=id_, operation=offset, index=len(created[id_]['stack'])))
    for id_ in texts:
        fill_id = created[id_]['stack'][0]['id']
        commands.append(dict(type='link_color', target=ref(id_, 'op.' + fill_id + '.color'), source=ref('ink-type', 'color')))
    apply(commands)
    authored = core('inspect')['result']
    svg = core('export_svg', composition=comp['id'], artboard=board['id'])['result']
    controller = ref('circle-0', 'op.circle-0-offset.amount')
    changed = apply([dict(type='set', ref=controller, value=26)])
    require({id_ for id_, _, _ in shapes}.issubset(changed['result']['changed_ids']), 'Dependent Offset owners were omitted.')
    for id_, _, delta in shapes:
        require(core('get', ref=ref(id_, 'op.' + id_ + '-offset.amount'))['result']['evaluated'] == 26 + delta,
                'A linked outline did not follow the controller.')
    require(core('export_svg', composition=comp['id'], artboard=board['id'])['result'] != svg, 'Offset change did not reach SVG.')
    revision = core('undo', expected_revision=revision)['revision']
    require(core('inspect')['result'] == authored, 'One Undo did not restore exact source/stack.')
    require(core('export_svg', composition=comp['id'], artboard=board['id'])['result'] == svg, 'Undo did not restore exact contours.')
    plan = core('export_plan', composition=comp['id'], artboard=board['id'])['result']
    require(not any(t['overflow'] or t['warnings'] for t in plan['text']), 'Typography requires repair.')
    require(len(ET.fromstring(svg).findall('.//{http://www.w3.org/2000/svg}path')) == 22, 'Unexpected SVG paint count.')
    result = call(endpoint, dict(identity, op='save', path=str(output), expected_revision=revision));require(result['ok'], str(result))
    require(json.loads(output.read_text(encoding='utf-8')) == authored, 'Native readback differs.')
    output.with_suffix('.svg').write_text(svg, encoding='utf-8')
    return dict(native=str(output), svg=str(output.with_suffix('.svg')), revision=revision,
                retained_sources=14, offsets=14, expressions=13, named_colors=4, editable_text=7,
                controller_readback=True, exact_undo=True, exact_native=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint', required=True);parser.add_argument('--output', required=True)
    args = parser.parse_args()
    print(json.dumps(create_study(args.endpoint, args.output), indent=2))

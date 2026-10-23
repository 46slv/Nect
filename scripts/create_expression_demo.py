"""Create PHASE / FORM through a fresh live Nect Session, then verify its links.

The requested native destination is selected before artwork mutation. Expressions
and colors use ordinary Session commands; no private document state is written.
The controller probe is undone exactly, leaving the original finished poster.
"""
import argparse
import copy
import json
import math
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

    def save():
        response = call(endpoint, dict(identity, op='save', path=str(output), expected_revision=revision))
        require(response['ok'], str(response))

    document = core('inspect')['result']
    require(not document['objects'] and not document['named_colors'] and not document['collections'],
            'Use a fresh empty document; existing artwork is preserved.')
    require(len(document['compositions']) == 1 and len(document['compositions'][0]['artboards']) == 1,
            'Use a fresh document with one Composition and one Artboard.')
    output = Path(output).resolve()
    require(output.suffix.lower() == '.nect', 'The output must be a native .nect file.')
    require(not output.exists() and not output.with_suffix('.svg').exists(),
            'Choose unused native and SVG destinations; this script does not overwrite existing studies.')
    require(not hello.get('file') or Path(hello['file']).resolve() != output,
            'Choose a separate output to preserve the opened file.')
    save()

    primitives = {entry['type']: entry['template'] for entry in core('primitive_types')['result']}
    operators = {entry['type']: entry['template'] for entry in core('operator_types')['result']}
    text_template = core('text_defaults')['result']
    language = core('expression_language')['result']
    require(language['version'] == 1 and language['trigonometry'] == 'degrees',
            'This study needs version 1 expressions with degree trigonometry.')
    require('Segoe UI' in core('text_fonts')['result'], 'This poster requires the installed Segoe UI family.')

    composition = document['compositions'][0]
    board = dict(composition['artboards'][0], name='Phase / Form', x=0, y=0, width=960, height=640)
    board.pop('parent_size', None)
    scalar = lambda value: dict(literal=value)

    def ref(owner, field):
        return dict(object=owner, point='', field=field)

    def set_(owner, field, value):
        return dict(type='set', ref=ref(owner, field), value=value)

    def rgba(hex_):
        return [int(hex_[start:start + 2], 16) / 255 for start in (0, 2, 4)] + [1]

    palette = [('ink-paper', 'Warm paper', 'F4EBDD'), ('ink-navy', 'Deep navy', '1B3040'),
               ('ink-coral', 'Coral', 'E66B52'), ('ink-teal', 'Teal', '297F7B'),
               ('ink-rule', 'Paper rule', 'D9CCB9')]
    commands = [dict(type='update_artboard', composition=composition['id'], artboard=board)]
    for id_, name, hex_ in palette:
        commands.append(dict(type='create_named_color', color=dict(id=id_, name=name,
            space='srgb', profile='srgb', alpha='straight', rgba=[scalar(v) for v in rgba(hex_)])))
    paints = {}
    shape_ids = []
    text_ids = []

    def shape(id_, kind, ink, name=None, **parameters):
        source = copy.deepcopy(primitives['nect.shape.' + kind])
        source['id'] = id_ + '-source'
        source['parameters'].update({key: scalar(value) for key, value in parameters.items()})
        commands.append(dict(type='create_primitive', composition=composition['id'], parent='',
                             id=id_, name=name or id_.replace('-', ' ').title(), source=source))
        paints[id_] = ink
        shape_ids.append(id_)

    def text_(id_, content, x, y, size, tracking=0, weight=400, ink='ink-navy'):
        source = copy.deepcopy(text_template)
        source.update(id=id_ + '-source', content=content, family='Segoe UI', weight=weight)
        source['parameters'].update({key: scalar(value) for key, value in
            dict(origin_x=x, origin_y=y, font_size=size, tracking=tracking).items()})
        commands.append(dict(type='create_text', composition=composition['id'], parent='',
                             id=id_, name=content, source=source))
        paints[id_] = ink
        text_ids.append(id_)

    # Artwork order is deliberate: paper and quiet rules sit behind the motifs.
    shape('paper', 'rectangle', 'ink-paper', center_x=480, center_y=320, width=960, height=640)
    shape('wave-axis', 'rectangle', 'ink-rule', center_x=460, center_y=330, width=800, height=.8)
    shape('lower-rule', 'rectangle', 'ink-rule', center_x=480, center_y=470, width=864, height=1)
    shape('footer-rule', 'rectangle', 'ink-rule', center_x=480, center_y=588, width=864, height=1)
    shape('phase', 'star', 'ink-coral', name='Phase / Rotation + Outer Radius + Inner Radius',
          center_x=841, center_y=156, points=5, outer_radius=45, inner_radius=22, rotation=15)
    wave_ids = [f'wave-{index}' for index in range(16)]
    for index, id_ in enumerate(wave_ids):
        shape(id_, 'circle', 'ink-coral' if index % 2 == 0 else 'ink-teal',
              name=f'Wave {index + 1:02d} / phase + {index * 24} degrees',
              center_x=100 + index * 48, center_y=330, radius=16)

    text_('edition', 'NECT / FORM STUDIES', 48, 33, 11, tracking=2)
    text_('title', 'PHASE / FORM', 45, 76, 62, tracking=.5, weight=700)
    text_('subtitle', 'One small rule. Sixteen different forms.', 48, 158, 18)
    text_('controller-label', 'PHASE / 15°', 785, 218, 12, tracking=.7, weight=600)
    text_('controller-note', 'ANGLE / AMPLITUDE', 754, 239, 9, tracking=1)
    text_('count', '16', 45, 483, 66, weight=700)
    text_('caption', 'Select Phase to vary angle / amplitude.', 172, 492, 20, weight=600)
    text_('instructions', 'Rotation drives the wave. Outer Radius controls its height.\n'
          'Inner Radius controls the pulse of each circle.', 174, 530, 12)
    text_('footer', 'RETAINED SHAPES / LIVE EXPRESSIONS / ONE SHARED SOURCE', 48, 608, 9, tracking=.8)
    text_('folio', 'STUDY 05', 839, 607, 10, tracking=1)
    apply(commands)

    # Discover actual generated paint identities rather than addressing a row.
    created = {item['id']: item for item in core('inspect')['result']['objects']}
    commands = []
    for id_ in shape_ids:
        for operation in created[id_]['stack']:
            commands.append(dict(type='enable_operation', object=id_, operation=operation['id'], enabled=False))
        fill = copy.deepcopy(operators['nect.paint.fill'])
        fill['id'] = id_ + '-fill'
        commands.append(dict(type='add_operation', object=id_, operation=fill, index=len(created[id_]['stack'])))
        commands.append(dict(type='link_color', target=ref(id_, f'op.{fill["id"]}.color'), source=ref(paints[id_], 'color')))
    for id_ in text_ids:
        fill = next(operation for operation in created[id_]['stack'] if operation['type'] == 'nect.paint.fill')
        commands.append(dict(type='link_color', target=ref(id_, f'op.{fill["id"]}.color'), source=ref(paints[id_], 'color')))
    for index, id_ in enumerate(wave_ids):
        phase = f'ref("phase","","generator.rotation") + {index * 24}'
        formulas = {
            'center_y': f'330 + sin({phase}) * ref("phase","","generator.outer_radius")',
            'radius': f'11 + (cos({phase}) + 1) * ref("phase","","generator.inner_radius") * .24',
        }
        for parameter, source in formulas.items():
            commands.append(dict(type='set_expression', targets=[ref(id_, 'generator.' + parameter)],
                                 expression=dict(source=source, version=1), replace_binding=False))
    apply(commands)
    initial = core('inspect')['result']

    def evaluated():
        return {(entry['ref']['object'], entry['ref']['point'], entry['ref']['field']): entry['value']
                for entry in core('evaluate')['result']}

    def verify_wave(values, angle, amplitude, pulse):
        for index, id_ in enumerate(wave_ids):
            radians = math.radians(angle + index * 24)
            expected_y = 330 + math.sin(radians) * amplitude
            expected_radius = 11 + (math.cos(radians) + 1) * pulse * .24
            require(values[(id_, '', 'generator.center_x')] == 100 + index * 48, 'A fixed circle X changed.')
            require(abs(values[(id_, '', 'generator.center_y')] - expected_y) < 1e-9, f'{id_}: phase position is incorrect.')
            require(abs(values[(id_, '', 'generator.radius')] - expected_radius) < 1e-9, f'{id_}: pulse radius is incorrect.')

    original_values = evaluated()
    verify_wave(original_values, 15, 45, 22)
    probe = apply([set_('phase', 'generator.rotation', 57), set_('phase', 'generator.outer_radius', 72)])
    verify_wave(evaluated(), 57, 72, 22)
    required_changes = {'phase', *wave_ids}
    require(required_changes.issubset(set(probe['result']['changed_ids'])), 'changed_ids omitted a dependent wave object.')
    revision = core('undo', expected_revision=revision)['revision']
    require(core('inspect')['result'] == initial, 'One Undo did not restore the exact original authored poster.')
    require(evaluated() == original_values, 'One Undo did not restore exact evaluated values.')

    plan = core('export_plan', composition=composition['id'], artboard=board['id'])['result']
    require(not plan['expressions_preserved'] and plan['native_source_preserved'], 'SVG/native expression preservation contract changed.')
    require(len(plan['text']) == len(text_ids) and not any(item['overflow'] or item['warnings'] for item in plan['text']),
            'Text layout has overflow or font warnings; inspect the live poster before accepting it.')
    save()
    require(json.loads(output.read_text(encoding='utf-8')) == initial, 'Native save does not exactly match live authored state.')
    svg = core('export_svg', composition=composition['id'], artboard=board['id'])['result']
    root = ET.fromstring(svg)
    paint_count = len(root.findall('.//{http://www.w3.org/2000/svg}path'))
    require(root.attrib['viewBox'] == '0 0 960 640', 'SVG frame differs from the poster.')
    require(paint_count == len(shape_ids) + len(text_ids), 'SVG omitted or duplicated an expected paint layer.')
    output.with_suffix('.svg').write_text(svg, encoding='utf-8')
    return dict(native=str(output), svg=str(output.with_suffix('.svg')), revision=revision,
                artboard=[960, 640], retained_wave_sources=len(wave_ids), live_expressions=32,
                controller='phase', named_colors=len(palette), svg_paint_layers=paint_count,
                controller_probe=dict(rotation=57, outer_radius=72, dependent_changed_ids=len(wave_ids)),
                undo_restored_exact=True, native_readback_exact=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint', required=True)
    parser.add_argument('--output', required=True)
    arguments = parser.parse_args()
    print(json.dumps(create_study(arguments.endpoint, arguments.output), ensure_ascii=True, indent=2))

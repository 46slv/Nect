"""Author a raster/vector editorial study through the live desktop Session.

The two source textures are original procedural artwork, generated with Pillow
only for this fixture. Pillow is not an application/runtime dependency.
"""
import argparse
import copy
import io
import json
import math
import random
from pathlib import Path
import xml.etree.ElementTree as ET

from PIL import Image
from session_client import call


def sources(directory):
    directory.mkdir(parents=True, exist_ok=True)
    rng = random.Random(811)
    landscape = Image.new('RGB', (640, 480))
    data = []
    for y in range(480):
        for x in range(640):
            hill = 265 + 40 * math.sin(x / 145) + 23 * math.sin(x / 63)
            dune = 368 + 55 * math.sin(x / 185 + .8)
            if y > dune:
                color = (60, 92, 103)
            elif y > hill:
                color = (200 + 20 * (y - hill) / 200, 115, 71)
            else:
                t = y / 480
                color = (235 - t * 25, 208 - t * 38, 165 - t * 15)
            if (x - 392) ** 2 + (y - 157) ** 2 < 69 ** 2:
                color = (247, 229, 190)
            grain = rng.gauss(0, 3.5)
            data.append(tuple(max(0, min(255, round(v + grain))) for v in color))
    landscape.putdata(data)
    out = io.BytesIO(); landscape.save(out, format='JPEG', quality=94, subsampling=0)
    jpeg = directory / 'material-landscape.jpg'
    alpha = Image.new('RGBA', (384, 384))
    data = []
    for y in range(384):
        for x in range(384):
            dx, dy = x - 192, y - 192
            radius = math.hypot(dx, dy)
            ring = 0 < radius < 178 and int((radius + 7 * math.sin(math.atan2(dy, dx) * 8)) / 16) % 2 == 0
            data.append((30, 124, 145, 200 if ring else 0))
    alpha.putdata(data); stream = io.BytesIO(); alpha.save(stream, format='PNG')
    png = directory / 'material-rings.png'
    for path, encoded in ((jpeg, out.getvalue()), (png, stream.getvalue())):
        if path.exists() and path.read_bytes() != encoded:
            raise RuntimeError('Existing source differs; choose a new output directory: ' + str(path))
        path.write_bytes(encoded)
    return jpeg, png


def create(endpoint, output):
    output = Path(output).resolve()
    if output.exists() or output.with_suffix('.svg').exists():
        raise RuntimeError('Use unused native/SVG destinations.')
    live = call(endpoint, dict(op='hello'))
    identity = {key: live[key] for key in ('session_id', 'document_id')}
    revision = live['revision']

    def core(op, **args):
        reply = call(endpoint, dict(identity, op='core', request=dict(op=op, **args)), timeout=30)
        if not reply['ok']:
            raise RuntimeError(str(reply))
        return reply

    def apply(commands):
        nonlocal revision
        reply = core('apply', expected_revision=revision, commands=commands)
        revision = reply['revision']
        return reply

    doc = core('inspect')['result']
    if doc['objects'] or doc['raster_assets']:
        raise RuntimeError('Use a fresh empty Session.')
    comp = doc['compositions'][0]
    board = dict(comp['artboards'][0], width=960, height=640, name='Material / 08')
    scalar = lambda v: dict(literal=v)
    ref = lambda owner, field: dict(object=owner, point='', field=field)
    types = {x['type']: x['template'] for x in core('primitive_types')['result']}
    operators = {x['type']: x['template'] for x in core('operator_types')['result']}
    commands = [dict(type='update_artboard', composition=comp['id'], artboard=board)]
    for id_, name, color in [('paper', 'Warm paper', 'F2E9D7'), ('ink', 'Charcoal', '293E45'), ('rust', 'Clay', 'BA6748')]:
        commands.append(dict(type='create_named_color', color=dict(id='color-' + id_, name=name, space='srgb', profile='srgb', alpha='straight',
            rgba=[scalar(int(color[i:i+2], 16) / 255) for i in (0, 2, 4)] + [scalar(1)])))

    def primitive(id_, kind, **parameters):
        source = copy.deepcopy(types['nect.shape.' + kind]); source['id'] = id_ + '-source'
        source['parameters'].update({k: scalar(v) for k, v in parameters.items()})
        commands.append(dict(type='create_primitive', composition=comp['id'], parent='', id=id_, name=id_.replace('-', ' ').title(), source=source))

    primitive('paper', 'rectangle', center_x=480, center_y=320, width=960, height=640)
    apply(commands)
    paper = next(o for o in core('inspect')['result']['objects'] if o['id'] == 'paper')
    fill = copy.deepcopy(operators['nect.paint.fill']); fill['id'] = 'paper-fill'
    apply([dict(type='set', ref=ref('paper', 'stroke.width'), value=0), dict(type='add_operation', object='paper', operation=fill, index=0),
           dict(type='link_color', target=ref('paper', 'op.paper-fill.color'), source=ref('color-paper', 'color'))])
    jpeg, png = sources(output.parent / 'assets')
    for asset, object_, path, mode, x, y in [('landscape', 'hero', jpeg, 'linked', 26, 175), ('rings', 'overlay', png, 'embedded', 245, 308)]:
        reply = call(endpoint, dict(identity, op='import_image', expected_revision=revision, path=str(path), mode=mode,
            composition=comp['id'], parent='', asset=asset, id=object_, name='Landscape / linked' if asset == 'landscape' else 'Rings / embedded', x=x, y=y))
        if not reply['ok']:
            raise RuntimeError(str(reply))
        revision = reply['revision']
    commands = [dict(type='set', ref=ref('hero', 'image.width'), value=480), dict(type='set', ref=ref('hero', 'image.height'), value=360),
                dict(type='set', ref=ref('overlay', 'image.width'), value=240), dict(type='set', ref=ref('overlay', 'image.height'), value=240),
                dict(type='set_compositing', object='overlay', blend='multiply', isolated=False)]
    primitive('hero-crop', 'circle', center_x=266, center_y=355, radius=174)
    commands.extend([dict(type='set_visibility', object='hero-crop', visible=False),
        dict(type='set_mask', object='hero', mask=dict(id='hero-mask', source='hero-crop', version=1, enabled=True, fill_rule='nonzero'))])
    for index, x in enumerate((549, 683, 817)):
        commands.extend([dict(type='create_image', composition=comp['id'], parent='', id=f'detail-{index}', name=f'Shared landscape / {index+1}',
            source=dict(asset='landscape', width=scalar(120), height=scalar(210))),
            dict(type='set', ref=ref(f'detail-{index}', 'transform.tx'), value=x), dict(type='set', ref=ref(f'detail-{index}', 'transform.ty'), value=230)])
    apply(commands)
    texts = [('edition', 'NECT / MATERIAL STUDIES', 45, 30, 11, 400, 1.8),
             ('title', 'MATERIAL / 08', 41, 68, 65, 700, .5),
             ('subtitle', 'A landscape, four placements. One accepted source.', 46, 149, 16, 400, 0),
             ('caption', '01  /  LINKED JPEG + GEOMETRY MASK', 72, 555, 10, 400, .8),
             ('right-label', '02  /  SHARED SOURCE, LOCAL SIZES', 549, 198, 10, 400, .6),
             ('body', 'Edit the image once.\nKeep every placement.\n\nTransparent rings stay embedded.', 550, 464, 15, 400, 0),
             ('footer', 'ACCEPTED PIXELS / EDITABLE VECTORS / RETAINED LINKS', 45, 608, 9, 400, 1)]
    commands = []
    for id_, content, x, y, size, weight, tracking in texts:
        source = copy.deepcopy(core('text_defaults')['result']); source.update(id=id_+'-source', content=content, family='Segoe UI', weight=weight)
        source['parameters'].update({k: scalar(v) for k, v in dict(origin_x=x, origin_y=y, font_size=size, tracking=tracking).items()})
        commands.append(dict(type='create_text', composition=comp['id'], parent='', id=id_, name=content.split('\n')[0], source=source))
    apply(commands)
    current = {o['id']: o for o in core('inspect')['result']['objects']}
    apply([dict(type='link_color', target=ref(id_, 'op.'+current[id_]['stack'][0]['id']+'.color'), source=ref('color-ink', 'color')) for id_, *_ in texts])
    accepted = core('inspect')['result']
    # A real downstream edit must preserve raster bytes and all shared placements.
    apply([dict(type='set', ref=ref('hero', 'image.width'), value=460)])
    revision = core('undo', expected_revision=revision)['revision']
    if core('inspect')['result'] != accepted:
        raise AssertionError('Image sizing Undo did not restore the exact document.')
    svg = core('export_svg', composition=comp['id'], artboard=board['id'])['result']
    if svg.count('data:image/png;base64,') != 2 or len(ET.fromstring(svg).findall('.//{http://www.w3.org/2000/svg}use')) != 5:
        raise AssertionError('Shared asset SVG projection differs.')
    plan = core('export_plan', composition=comp['id'], artboard=board['id'])['result']
    if any(t['overflow'] or t['warnings'] for t in plan['text']):
        raise AssertionError('Text layout needs repair.')
    result = call(endpoint, dict(identity, op='save', path=str(output), expected_revision=revision))
    if not result['ok'] or json.loads(output.read_text(encoding='utf-8')) != accepted:
        raise AssertionError('Exact native save failed.')
    output.with_suffix('.svg').write_text(svg, encoding='utf-8')
    return dict(native=str(output), svg=str(output.with_suffix('.svg')), revision=revision, linked_assets=1, embedded_assets=1,
                image_placements=5, editable_text=7, named_colors=3, image_mask=True, multiply=True, exact_resize_undo=True, exact_native=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint', required=True); parser.add_argument('--output', required=True)
    args = parser.parse_args(); print(json.dumps(create(args.endpoint, args.output), indent=2))

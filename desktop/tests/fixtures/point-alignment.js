import { node, parameter, connect } from '../../src/core.js';
import { directMarqueeFixture } from './direct-anchor-marquee.js';
import { compoundFixture } from './contour-navigation.js';
import { guideSettings } from '../../src/guides.js';

export function alignmentFixture(remapped = false) {
  if (!remapped) {
    const f = directMarqueeFixture({ transformed: true });
    return {
      ...f,
      owner: f.curve,
      source: f.curve,
      ids: f.curve.points.slice(0, 3).map((p) => p.id),
      region: '.inspector-body',
    };
  }
  const f = compoundFixture([4, 4, 4, 2]),
    owner = node('Path Remap'),
    transform = node('Transform');
  owner.id = 'alignment-remap';
  transform.id = 'alignment-transform';
  f.doc.width = 600;
  f.doc.height = 500;
  f.doc.schemaVersion = 49;
  f.doc.editor = { ...guideSettings(f.doc), snap: false };
  Object.assign(transform.params, {
    uniformScale: { ...parameter(transform, 'uniformScale'), value: false },
    scaleWidth: { ...parameter(transform, 'scaleWidth'), value: 1.5 },
    position: { ...parameter(transform, 'position'), value: [300, 240] },
  });
  transform.params.anchor.value = [150, 150];
  transform.params.scale.value = 0.7;
  transform.params.rotation.value = 35;
  f.fill.params.path.source = null;
  f.layer.nodes = [f.path, owner, transform, f.fill, f.output];
  for (const [i, n] of f.layer.nodes.entries()) {
    n.position = [40 + 310 * i, 40];
    n.collapsed = n !== owner;
  }
  connect(f.layer, f.path.id, owner.id, 'path');
  connect(f.layer, owner.id, transform.id, 'path');
  connect(f.layer, transform.id, f.fill.id, 'path');
  return {
    ...f,
    owner,
    source: f.path,
    transform,
    ids: f.path.contours[0].points.slice(0, 3),
    region: '.graph',
  };
}

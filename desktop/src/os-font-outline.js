// OS preparation is read-only. Only CommandSession may adopt the returned geometry.
export function osTextIntent(n) {
  if (n?.type !== 'Text' || n.locked || n.bypassed)
    throw Error('Select an unlocked, enabled Text node');
  if (!n.text?.trim() || n.text.length > 1024 || /[^\x20-\x7e]/.test(n.text))
    throw Error(
      'OS font outlines currently support one printable ASCII line (up to 1024 characters)',
    );
  const record = n.characterPositions;
  if (
    n.variation ||
    n.pathAttachment ||
    n.textWrap ||
    n.characterArrangement ||
    n.writingMode === 'vertical-rl' ||
    n.referenceBounds !== 'visible' ||
    n.kerning === false ||
    ['offsets', 'styles', 'transforms', 'kerningPairs'].some(
      (key) => Object.keys(record?.[key] || {}).length,
    )
  )
    throw Error(
      'OS outlines require plain horizontal Text with visible bounds; remove wrap, path and character adjustments first',
    );
  for (const q of Object.values(n.params))
    if (q.locked || q.source || q.expression)
      throw Error('Unlock and disconnect Text parameters before OS conversion');
  const size = n.params.size.value;
  if (!Number.isFinite(size) || size < 0.01 || size > 10000 || n.params.letterSpacing.value !== 0)
    throw Error('OS outlines require size 0.01–10000 px and zero tracking');
  return { text: n.text, size };
}

export function osOutlineGeometry(raw, source, uid) {
  const intent = osTextIntent(source);
  if (
    !raw ||
    !Number.isFinite(raw.size) ||
    raw.text !== intent.text ||
    Math.fround(raw.size) !== Math.fround(intent.size) ||
    typeof raw.family !== 'string' ||
    !raw.family.trim() ||
    raw.family.length > 256 ||
    !Number.isInteger(raw.style) ||
    raw.style < 0 ||
    raw.style > 3 ||
    !Number.isFinite(raw.ascent) ||
    raw.ascent < 0 ||
    raw.ascent > intent.size * 10 ||
    !Array.isArray(raw.points) ||
    !Array.isArray(raw.types) ||
    !raw.points.length ||
    raw.points.length > 30000 ||
    raw.points.length !== raw.types.length
  )
    throw Error('Invalid or stale OS outline response');
  const xy = (v) =>
    Array.isArray(v) &&
    v.length === 2 &&
    v.every((x) => Number.isFinite(x) && Math.abs(x) <= 10000000);
  if (
    !raw.points.every(xy) ||
    // GDI+ glyph-boundary markers (0x20) carry no geometry; retain closure (0x80).
    !raw.types.every((t) => Number.isInteger(t) && t >= 0 && t <= 255 && !(t & 0x58))
  )
    throw Error('Invalid OS outline coordinates or point types');
  const contours = [],
    group = uid('os-outline');
  let current = null,
    total = 0;
  const anchor = (position) => ({
    id: uid('point'),
    position: [...position],
    handleA: [0, 0],
    handleB: [0, 0],
  });
  for (let i = 0; i < raw.points.length; i++) {
    const kind = raw.types[i] & 7;
    if (kind === 0) {
      if (current) throw Error('Unclosed OS outline contour');
      current = { id: uid('contour'), group, closed: true, points: [anchor(raw.points[i])] };
    } else if (kind === 1 && current) current.points.push(anchor(raw.points[i]));
    else if (
      kind === 3 &&
      current &&
      i + 2 < raw.points.length &&
      (raw.types[i + 1] & 7) === 3 &&
      (raw.types[i + 2] & 7) === 3 &&
      !(raw.types[i] & 128) &&
      !(raw.types[i + 1] & 128)
    ) {
      const prior = current.points.at(-1),
        end = anchor(raw.points[i + 2]);
      prior.handleB = raw.points[i].map((x, k) => x - prior.position[k]);
      end.handleA = raw.points[i + 1].map((x, k) => x - end.position[k]);
      current.points.push(end);
      i += 2;
    } else throw Error('Unsupported OS outline segment');
    if (raw.types[i] & 128) {
      if (!current || current.points.length < 2) throw Error('Empty OS outline contour');
      const first = current.points[0],
        last = current.points.at(-1);
      if (last.position.every((v, k) => v === first.position[k])) {
        first.handleA = last.handleA;
        current.points.pop();
      }
      total += current.points.length;
      if (total > 10000 || contours.length >= 10000)
        throw Error('OS outline exceeds the 10000 vertex/contour budget');
      contours.push(current);
      current = null;
    }
  }
  if (current || !contours.length) throw Error('Unclosed or empty OS outline');
  return { contours };
}

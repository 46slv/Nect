// Handles stay relative Vector2 parameters. In document coordinates +Y is down.
export function handlePolar(vector) {
  if (!Array.isArray(vector) || vector.length !== 2 || !vector.every(Number.isFinite))
    throw Error('Expected a finite handle');
  const length = Math.hypot(...vector);
  if (!Number.isFinite(length)) throw Error('Expected a finite handle length');
  return { angle: length ? (Math.atan2(vector[1], vector[0]) * 180) / Math.PI : 0, length };
}

export function polarHandleValue(vector, component, value) {
  const polar = handlePolar(vector);
  if (!['angle', 'length'].includes(component) || !Number.isFinite(value))
    throw Error('Expected a finite handle value');
  if (component === 'length' && value < 0) throw Error('Length must be non-negative');
  // A zero vector has no direction; growing it starts along +X. No hidden angle is saved.
  if (component === 'angle' && !polar.length) throw Error('Set a non-zero length first');
  if (value === polar[component]) return [...vector];
  const length = component === 'length' ? value : polar.length;
  if (!length) return [0, 0];
  if (component === 'length' && polar.length) return vector.map((v) => (v / polar.length) * length);
  const degrees = (((component === 'angle' ? value : polar.angle) % 360) + 360) % 360;
  const radians = (degrees * Math.PI) / 180;
  return [Math.cos(radians), Math.sin(radians)].map((v) => (Math.abs(v) < 1e-15 ? 0 : v * length));
}

// The Canvas gesture owns validation, coupling, atomic commit, and History.
export function commitHandlePolar(session, target, component, value) {
  const layer = session.snapshot().layers.find((l) => l.id === target.layerId),
    node = layer?.nodes.find((n) => n.id === target.nodeId),
    point = node?.points?.find((p) => p.id === target.pointId),
    q = point?.[target.key];
  if (
    target.kind !== 'curve-point' ||
    !['Curve', 'Multi Path'].includes(node?.type) ||
    !['handleA', 'handleB'].includes(target.key) ||
    !q
  )
    throw Error('Missing owned handle');
  const next = polarHandleValue(q.value, component, value);
  const send = (type, payload) => session.dispatch({ ...session.state, type, payload });
  let token;
  try {
    token = send('begin-gesture', { target });
    token = send('update-gesture', {
      ...token,
      update: { delta: next.map((v, i) => v - q.value[i]) },
    });
    return send('commit-gesture', token);
  } catch (error) {
    if (token) send('cancel-gesture', { gestureId: token.gestureId });
    throw error;
  }
}

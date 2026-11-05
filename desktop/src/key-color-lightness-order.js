// Advisory for exact numeric model-L order among active derived slots only.
// No appearance, contrast, tolerances, or document mutations are implied.
const clamp = (value) => Math.min(1, Math.max(0, value));
const validId = (id) => typeof id === 'string' && /^[A-Za-z0-9_-]{1,128}$/.test(id);
const validRole = (role) => typeof role === 'string' && !!role.trim() && role.length <= 128;

export function compareModelLightnessOrder(snapshot) {
  const key = snapshot?.intent?.keyColor;
  const plans = snapshot?.intent?.preset?.slots;
  const intents = snapshot?.intent?.slots;
  const resolved = snapshot?.resolved;
  if (
    !validId(key?.id) ||
    !validRole(key?.role) ||
    !Array.isArray(plans) ||
    !Array.isArray(intents) ||
    !Array.isArray(resolved) ||
    plans.length > 255 ||
    intents.length > 255 ||
    resolved.length > 256
  )
    return null;
  function unique(rows) {
    const map = new Map();
    for (const row of rows) {
      if (!row || !validId(row.id) || map.has(row.id)) return null;
      map.set(row.id, row);
    }
    return map;
  }
  const byIntent = unique(intents),
    byResolved = unique(resolved),
    byPlan = unique(plans);
  if (
    !byIntent ||
    !byResolved ||
    !byPlan ||
    byIntent.has(key.id) ||
    byPlan.has(key.id) ||
    byResolved.size !== byIntent.size + 1
  )
    return null;
  const keyRow = byResolved.get(key.id);
  if (
    !keyRow ||
    keyRow.role !== key.role ||
    keyRow.active !== true ||
    !Number.isFinite(keyRow.L) ||
    keyRow.L < 0 ||
    keyRow.L > 1
  )
    return null;
  for (const [id, intent] of byIntent) {
    const row = byResolved.get(id);
    if (
      !validRole(intent.role) ||
      !row ||
      row.role !== intent.role ||
      row.active !== byPlan.has(id)
    )
      return null;
  }
  const rows = [];
  for (const [id, plan] of byPlan) {
    const intent = byIntent.get(id),
      row = byResolved.get(id);
    if (
      !intent ||
      !row ||
      plan.role !== intent.role ||
      !Number.isFinite(intent.deltaL) ||
      !Number.isFinite(plan.lightnessScale) ||
      !Number.isFinite(plan.lightnessOffset) ||
      !Number.isFinite(row.baseL) ||
      !Number.isFinite(row.L) ||
      row.L < 0 ||
      row.L > 1 ||
      !Number.isFinite(row.baseL + intent.deltaL) ||
      row.baseL !== keyRow.L * plan.lightnessScale + plan.lightnessOffset ||
      row.L !== clamp(row.baseL + intent.deltaL)
    )
      return null;
    rows.push({ id, role: intent.role, base: clamp(row.baseL), value: row.L });
  }
  rows.sort((a, b) => (a.id < b.id ? -1 : a.id > b.id ? 1 : 0));
  const pairs = [];
  for (let i = 0; i < rows.length; i++)
    for (let j = i + 1; j < rows.length; j++) {
      const a = rows[i],
        b = rows[j],
        before = Math.sign(a.base - b.base),
        after = Math.sign(a.value - b.value);
      const state =
        before === 0
          ? after === 0
            ? 'both equal'
            : 'separated'
          : after === 0
            ? 'equalized'
            : before === after
              ? 'maintained'
              : 'reversed';
      pairs.push({ a, b, state });
    }
  return pairs;
}

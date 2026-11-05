// Display labels never own identity. Duplicate names get deterministic ID
// suffixes; singleton names and reserved control labels remain distinguishable.
export function displayColorLabels(rows, reservedLabels = []) {
  const visibleKey = (label) => label.normalize('NFC');
  const groups = new Map();
  for (const entry of rows) {
    const key = visibleKey(entry.label);
    if (!groups.has(key)) groups.set(key, []);
    groups.get(key).push(entry);
  }
  const labels = new Map();
  const used = new Set(reservedLabels.map(visibleKey));
  for (const [key, entries] of groups)
    if (entries.length === 1 && !used.has(key)) {
      labels.set(entries[0].id, entries[0].label);
      used.add(key);
    }
  const duplicates = rows
    .filter((entry) => !labels.has(entry.id))
    .sort((a, b) => (a.id < b.id ? -1 : a.id > b.id ? 1 : 0));
  for (const entry of duplicates) {
    const peers = groups.get(visibleKey(entry.label)).filter((row) => row.id !== entry.id);
    let length = Math.min(4, entry.id.length);
    const candidate = () =>
      `${entry.label} · ${length < entry.id.length ? '…' : ''}${entry.id.slice(-length)}`;
    while (
      length < entry.id.length &&
      (peers.some((row) => row.id.endsWith(entry.id.slice(-length))) ||
        used.has(visibleKey(candidate())))
    )
      length++;
    let label = candidate();
    let suffix = 2;
    while (used.has(visibleKey(label))) label = `${candidate()} (${suffix++})`;
    labels.set(entry.id, label);
    used.add(visibleKey(label));
  }
  return labels;
}

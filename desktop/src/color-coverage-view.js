// Derived view state. Never writes the document, history, or color registry.
export function createColorCoverageView({ app, getKey, isOpen, measure }) {
  let settled = null;
  let running = false;
  const current = () => (settled?.key === getKey() ? settled : null);
  function paint() {
    const result = current();
    for (const output of app.querySelectorAll('[data-color-coverage-id]')) {
      const id = output.dataset.colorCoverageId;
      const row = result?.value?.slots.find((slot) => slot.id === id);
      output.textContent = row ? `${row.percent}%` : '—';
    }
    const status = app.querySelector('[data-color-coverage-status]');
    if (status) {
      status.textContent = result?.error
        ? `Coverage unavailable: ${result.error}`
        : result?.value
          ? `Visible area · approx. ${result.value.sampleSize.width} × ${result.value.sampleSize.height} sample · Other ${result.value.other.percent}%`
          : 'Measuring visible area…';
      status.title = status.textContent;
    }
  }
  function refresh() {
    if (!isOpen()) return;
    paint();
    if (running || current()) return;
    const key = getKey();
    const valid = () => isOpen() && getKey() === key;
    running = true;
    Promise.resolve()
      .then(() => {
        if (!valid()) return;
        return measure(valid);
      })
      .then(
        (value) => {
          if (valid()) settled = value ? { key, value } : { key, error: 'No coverage result' };
        },
        (error) => {
          if (valid()) settled = { key, error: String(error?.message || error) };
        },
      )
      .finally(() => {
        running = false;
        if (isOpen()) refresh();
      });
  }
  return {
    refresh,
    snapshot() {
      const result = isOpen() ? current() : null;
      return result?.value ? { key: result.key, value: structuredClone(result.value) } : null;
    },
  };
}

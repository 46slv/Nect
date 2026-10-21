// Editor-only suggestions for the bounded grammar in core.expression. Never evaluates code.
const functions = ['sin', 'cos', 'tan', 'abs', 'sqrt', 'min', 'max', 'round'];
const word = /[\p{L}\p{N}_.-]/u;
function colorScalarArgument(value, end) {
  const stack = [];
  let quoted = false;
  for (let i = 0; i < end; i++) {
    const char = value[i];
    if (char === '"') quoted = !quoted;
    if (quoted) continue;
    if (char === '(')
      stack.push({
        name: value
          .slice(0, i)
          .match(/[a-zA-Z]+\s*$/)?.[0]
          .trim(),
        argument: 0,
      });
    else if (char === ')') stack.pop();
    else if (char === ',' && stack.length) stack.at(-1).argument++;
  }
  const color = stack.findLast((frame) => ['mix', 'alpha'].includes(frame.name));
  return color && color.argument === (color.name === 'mix' ? 2 : 1);
}
export function expressionReferenceType(value, end, type) {
  return type === 'Color' && colorScalarArgument(value, end) ? 'Scalar' : type;
}

export function expressionCompletionContext(value, start, end = start) {
  if (start !== end) return null;
  let quoted = false,
    escaped = false;
  for (const char of value.slice(0, start)) {
    if (escaped) {
      escaped = false;
      continue;
    }
    if (char === '\\' && quoted) {
      escaped = true;
      continue;
    }
    if (char === '"') quoted = !quoted;
  }
  if (quoted) return null;
  let from = start,
    to = start;
  // Minus is an arithmetic operator, never part of an editor prefix.
  const token = (char) => char && char !== '-' && word.test(char);
  while (from > 0 && token(value[from - 1])) from--;
  while (to < value.length && token(value[to])) to++;
  const prefix = value.slice(from, start).toLocaleLowerCase();
  if (!prefix || !/^[\p{L}_]/u.test(prefix)) return null;
  return { from, to, prefix };
}

export function expressionCompletions(value, start, end, targets = [], parameterType = 'Scalar') {
  const context = expressionCompletionContext(value, start, end);
  if (!context) return [];
  const weight = parameterType === 'Color' && colorScalarArgument(value, start);
  const color = parameterType === 'Color' && !weight;
  const axis = value.slice(0, start).split(';').length === 1 ? 'X' : 'Y';
  const candidates = [
    ...(!weight
      ? [
          {
            id: 'value',
            label: 'value',
            text: 'value',
            terms: ['value'],
            detail:
              parameterType === 'Vector2'
                ? `Authored base ${axis} component → Scalar`
                : `Authored base → ${parameterType === 'Color' ? 'Color' : 'Scalar'}`,
          },
        ]
      : []),
    ...(color
      ? [
          {
            id: 'mix',
            label: 'mix(Color, Color, Scalar)',
            detail: 'Linear-light blend → Color',
            text: 'mix()',
            cursorOffset: 4,
            terms: ['mix'],
          },
          {
            id: 'alpha',
            label: 'alpha(Color, Scalar)',
            detail: 'Replace alpha → Color',
            text: 'alpha()',
            cursorOffset: 6,
            terms: ['alpha'],
          },
        ]
      : []),
    ...(color ? [] : functions).map((name) => ({
      id: name,
      label: `${name}(…)`,
      detail: 'Function → Scalar',
      text: `${name}()`,
      cursorOffset: name.length + 1,
      terms: [name],
    })),
    ...(color ? [] : ['pi', 'e']).map((name) => ({
      id: name,
      label: name,
      detail: 'Constant · Scalar',
      text: name,
      terms: [name],
    })),
    ...targets
      .filter((target) => (color ? target.type === 'Color' : target.type !== 'Color'))
      .map((target) => ({
        id: target.expression,
        label: target.label,
        detail: `${target.type === 'Color' ? 'Color' : target.component ? `Vector2.${target.component.toUpperCase()} → Scalar` : 'Scalar'} · ${target.nodeId}`,
        text: target.expression,
        terms: [
          target.name,
          target.key,
          `${target.key}.${target.component || ''}`,
          'ref',
          ...target.name.split(/\s+/),
        ],
      })),
  ];
  return candidates
    .filter((candidate) =>
      candidate.terms.some((term) => term.toLocaleLowerCase().startsWith(context.prefix)),
    )
    .slice(0, 12)
    .map((candidate) => ({ ...candidate, ...context }));
}

let nextId = 0;
export function attachExpressionCompletion(input, getTargets, parameterType = 'Scalar', onAccept) {
  const doc = input.ownerDocument;
  const list = doc.createElement('div');
  list.className = 'expression-completions';
  list.id = `expression-completions-${++nextId}`;
  list.setAttribute('role', 'listbox');
  list.setAttribute('aria-label', 'Expression suggestions');
  list.hidden = true;
  input.after(list);
  input.setAttribute('role', 'combobox');
  input.setAttribute('aria-autocomplete', 'list');
  input.setAttribute('aria-controls', list.id);
  input.setAttribute('aria-expanded', 'false');
  let items = [],
    active = 0,
    composing = false;
  const hide = () => {
    items = [];
    list.hidden = true;
    list.replaceChildren();
    input.setAttribute('aria-expanded', 'false');
    input.removeAttribute('aria-activedescendant');
  };
  const current = () =>
    expressionCompletions(
      input.value,
      input.selectionStart,
      input.selectionEnd,
      getTargets(),
      parameterType,
    );
  const paint = () => {
    list.replaceChildren();
    list.hidden = !items.length;
    input.setAttribute('aria-expanded', String(!!items.length));
    input.removeAttribute('aria-activedescendant');
    items.forEach((item, index) => {
      const option = doc.createElement('div');
      option.id = `${list.id}-${index}`;
      option.setAttribute('role', 'option');
      option.setAttribute('aria-selected', String(index === active));
      const label = doc.createElement('strong'),
        detail = doc.createElement('small');
      label.textContent = item.label;
      detail.textContent = item.detail;
      option.append(label, detail);
      option.addEventListener('mousedown', (event) => event.preventDefault());
      option.addEventListener('click', () => accept(item));
      list.append(option);
      if (index === active) input.setAttribute('aria-activedescendant', option.id);
    });
    list.children[active]?.scrollIntoView?.({ block: 'nearest' });
  };
  const refresh = () => {
    if (composing) return hide();
    items = current();
    active = 0;
    paint();
  };
  const accept = (item) => {
    // Re-resolve before insertion: renamed/deleted nodes or moved selection may invalidate a row.
    const fresh = current().find(
      (candidate) =>
        candidate.id === item.id && candidate.from === item.from && candidate.to === item.to,
    );
    if (!fresh) {
      hide();
      return false;
    }
    const value = input.value.slice(0, fresh.from) + fresh.text + input.value.slice(fresh.to);
    if (value.length > 4096) {
      hide();
      return false;
    }
    input.value = value;
    const caret = fresh.from + (fresh.cursorOffset ?? fresh.text.length);
    input.setSelectionRange(caret, caret);
    input.focus();
    hide();
    onAccept?.();
    return true;
  };
  input.addEventListener('input', refresh);
  input.addEventListener('click', refresh);
  input.addEventListener('keyup', (event) => {
    if (['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) refresh();
  });
  input.addEventListener('blur', hide);
  input.addEventListener('compositionstart', () => {
    composing = true;
    hide();
  });
  input.addEventListener('compositionend', () => {
    composing = false;
    refresh();
  });
  input.addEventListener('keydown', (event) => {
    if (
      composing ||
      event.isComposing ||
      event.keyCode === 229 ||
      event.ctrlKey ||
      event.metaKey ||
      event.altKey
    )
      return;
    if (!items.length) return;
    if (event.key === 'Escape') {
      event.preventDefault();
      event.stopPropagation();
      hide();
    } else if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
      event.preventDefault();
      event.stopPropagation();
      const selectedId = items[active]?.id;
      items = current();
      if (!items.length) return hide();
      active = Math.max(
        0,
        items.findIndex((item) => item.id === selectedId),
      );
      active = (active + (event.key === 'ArrowDown' ? 1 : -1) + items.length) % items.length;
      paint();
    } else if (event.key === 'Enter' || (event.key === 'Tab' && !event.shiftKey)) {
      if (accept(items[active])) {
        event.preventDefault();
        event.stopPropagation();
      }
    }
  });
  return { refresh, hide };
}

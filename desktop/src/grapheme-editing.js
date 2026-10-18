// Editing boundaries in the original textarea UTF-16 string. Do not normalize
// here: authored character IDs/NFC and OpenType shaping clusters are separate.
let segmenter;

export function graphemeBoundaries(text) {
  if (typeof text !== 'string') throw new TypeError('Expected text');
  if (typeof Intl.Segmenter !== 'function') throw new Error('Grapheme segmentation is unavailable');
  segmenter ??= new Intl.Segmenter(undefined, { granularity: 'grapheme' });
  return [...segmenter.segment(text)].map((part) => part.index).concat(text.length);
}

function validateRange(text, start, end = start) {
  if (typeof text !== 'string') throw new TypeError('Expected text');
  if (
    !Number.isInteger(start) ||
    !Number.isInteger(end) ||
    start < 0 ||
    end < start ||
    end > text.length
  )
    throw new RangeError('Invalid UTF-16 text range');
}

function neighbors(boundaries, index) {
  const at = boundaries.findIndex((boundary) => boundary >= index);
  return { before: boundaries[at] === index ? index : boundaries[at - 1], after: boundaries[at] };
}

function snap(boundaries, index, bias) {
  const { before, after } = neighbors(boundaries, index);
  if (bias === 'backward') return before;
  if (bias === 'forward') return after;
  // A pointer exactly halfway between UTF-16 positions resolves forward.
  return index - before < after - index ? before : after;
}

export function graphemeCaretIndex(text, index, bias = 'nearest') {
  validateRange(text, index);
  if (!['nearest', 'backward', 'forward'].includes(bias)) throw new TypeError('Invalid caret bias');
  return snap(graphemeBoundaries(text), index, bias);
}

// Desired range for a native content deletion. This function never edits text,
// cancels an event, or decides whether an already-safe native path needs help.
// A caller may use this range to extend an unsafe selection BEFORE capturing
// the draft intent, while leaving the browser responsible for input and Undo.
export function graphemeDeletionRange(text, { start, end, inputType, composing = false }) {
  if (composing || !['deleteContentBackward', 'deleteContentForward'].includes(inputType))
    return null;
  validateRange(text, start, end);
  const boundaries = graphemeBoundaries(text);
  if (start !== end)
    return { start: snap(boundaries, start, 'backward'), end: snap(boundaries, end, 'forward') };
  const { before, after } = neighbors(boundaries, start);
  if (before !== after) return { start: before, end: after };
  const at = boundaries.indexOf(start);
  return inputType === 'deleteContentBackward'
    ? { start: boundaries[Math.max(0, at - 1)], end }
    : { start, end: boundaries[Math.min(boundaries.length - 1, at + 1)] };
}

// Inspect the selection AFTER a native movement. logicalDirection is inferred
// by the adapter from the observed UTF-16 focus movement, not from ArrowLeft /
// ArrowRight: visual arrow direction can differ in bidi text. Return null when
// no correction is needed. Word/line navigation is left to the native adapter.
export function graphemeMovementCorrection(
  text,
  { start, end, selectionDirection = 'none', logicalDirection, extend = false, composing = false },
) {
  if (composing) return null;
  validateRange(text, start, end);
  if (!['backward', 'forward'].includes(logicalDirection))
    throw new TypeError('Expected logical movement direction');
  if (!['none', 'backward', 'forward'].includes(selectionDirection))
    throw new TypeError('Invalid selection direction');
  if (!extend && start !== end) return null;
  const boundaries = graphemeBoundaries(text);
  if (boundaries.includes(start) && boundaries.includes(end)) return null;
  if (!extend) {
    const index = snap(boundaries, start, logicalDirection);
    return { start: index, end: index, selectionDirection: 'none' };
  }
  const backward = selectionDirection === 'backward';
  const focus = snap(boundaries, backward ? start : end, logicalDirection);
  // A pre-existing interior anchor is expanded outwards so the selection does
  // not leave part of its grapheme behind. Legal anchors remain exact.
  const anchor = snap(
    boundaries,
    backward ? end : start,
    logicalDirection === 'backward' ? 'forward' : 'backward',
  );
  return {
    start: Math.min(anchor, focus),
    end: Math.max(anchor, focus),
    selectionDirection: focus === anchor ? 'none' : focus < anchor ? 'backward' : 'forward',
  };
}

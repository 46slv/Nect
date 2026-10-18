const MAX_INTERVALS = 512;

function validateInputs(length, options) {
  if (!Number.isFinite(length) || length < 0) {
    throw new Error('Dash length must be finite and nonnegative');
  }
  if (!options || typeof options !== 'object' || Array.isArray(options)) {
    throw new Error('Dash options must be an object');
  }

  const { dashLength, gapLength, offset } = options;
  if (!Number.isFinite(dashLength) || dashLength < 0.0001 || dashLength > 100000) {
    throw new Error('Dash length must be finite and between 0.0001 and 100000');
  }
  if (!Number.isFinite(gapLength) || gapLength < 0 || gapLength > 100000) {
    throw new Error('Dash gap must be finite and between 0 and 100000');
  }
  if (!Number.isFinite(offset) || offset < -100000 || offset > 100000) {
    throw new Error('Dash offset must be finite and between -100000 and 100000');
  }
  return { dashLength, gapLength, offset };
}

function normalizeOffset(offset, period) {
  let normalized = offset % period;
  if (normalized < 0) normalized += period;
  // Avoid a rounded value of period, which is equivalent to phase zero.
  if (normalized >= period) normalized = 0;
  return normalized;
}

function checkedIntervalCount(length, firstStart, period, additionalIntervals = 0) {
  if (firstStart >= length) return additionalIntervals;

  const remaining = length - firstStart;
  if (!Number.isFinite(remaining) || remaining <= 0) {
    throw new Error('Dash interval count is not finite');
  }
  const quotient = remaining / period;
  if (!Number.isFinite(quotient)) throw new Error('Dash interval count is not finite');

  // Starts are strictly before length, so an exact integer quotient contributes
  // that many starts. This is the estimate that bounds work before allocation.
  const count = Math.ceil(quotient) + additionalIntervals;
  if (!Number.isSafeInteger(count) || count > MAX_INTERVALS) {
    throw new Error(`Dash exceeds the ${MAX_INTERVALS}-interval geometry budget`);
  }
  return count;
}

/**
 * Plan dash segments by distance along a path.
 *
 * A positive offset advances the repeating pattern by that many document units:
 * after reducing it modulo dashLength + gapLength, the first un-clipped dash
 * starts at the negative of that normalized value. Negative offsets wrap into
 * the same period. The returned ranges are clipped to the path's [0, length]
 * domain and contain only positive-width intervals.
 */
export function dashIntervals(length, options) {
  const { dashLength, gapLength, offset } = validateInputs(length, options);
  if (length === 0) return [];
  if (gapLength === 0) return [[0, length]];

  const period = dashLength + gapLength;
  if (!Number.isFinite(period) || period <= 0) {
    throw new Error('Dash period is not finite');
  }

  const phase = normalizeOffset(offset, period);
  if (!Number.isFinite(phase) || phase < 0 || phase >= period) {
    throw new Error('Dash phase is not finite');
  }

  const intervals = [];
  let nextStart;

  if (phase < dashLength) {
    // The initial dash began before the path origin and is clipped at zero.
    const firstWidth = Math.min(dashLength - phase, length);
    const firstEnd = firstWidth;
    if (!Number.isFinite(firstEnd) || firstEnd <= 0) {
      throw new Error('Dash interval calculation made no progress');
    }
    intervals.push([0, firstEnd]);
    nextStart = period - phase;

    const additional = nextStart < length ? checkedIntervalCount(length, nextStart, period) : 0;
    if (additional + 1 > MAX_INTERVALS) {
      throw new Error(`Dash exceeds the ${MAX_INTERVALS}-interval geometry budget`);
    }
  } else {
    // The origin lies in a gap; the first visible dash starts at the next period.
    nextStart = period - phase;
    checkedIntervalCount(length, nextStart, period);
  }

  while (nextStart < length) {
    if (intervals.length >= MAX_INTERVALS) {
      throw new Error(`Dash exceeds the ${MAX_INTERVALS}-interval geometry budget`);
    }
    const available = length - nextStart;
    if (!Number.isFinite(available) || available <= 0) {
      throw new Error('Dash interval calculation made no progress');
    }
    const width = Math.min(dashLength, available);
    const endCandidate = width === available ? length : nextStart + width;
    const end = Math.min(length, endCandidate);
    if (!Number.isFinite(end) || end <= nextStart) {
      throw new Error('Dash interval calculation made no progress');
    }
    intervals.push([nextStart, end]);

    const followingStart = nextStart + period;
    if (!Number.isFinite(followingStart) || followingStart <= nextStart) {
      throw new Error('Dash interval calculation made no progress');
    }
    nextStart = followingStart;
  }

  return intervals;
}

import { OffsetError } from './closed-offset.js';

// Shared by authored-value validation and the evaluated node binding.
export function closedOffsetSettings(read) {
  const amount = read('amount'),
    miterLimit = read('miterLimit');
  if (!Number.isFinite(amount) || (amount !== 0 && Math.abs(amount) < 0.0001))
    throw new OffsetError(
      'OFFSET_AMOUNT',
      'Offset amount must be zero or at least 0.0001 px in magnitude',
    );
  if (!Number.isFinite(miterLimit) || miterLimit < 1 || miterLimit > 100)
    throw new OffsetError('OFFSET_OPTIONS', 'Offset miter limit must be between 1 and 100');
  return { amount, miterLimit };
}

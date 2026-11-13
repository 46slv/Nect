import { validateTonePower } from './luminance-tone.js';

export function luminanceModifierSettings(read) {
  const pitch = read('pitch'),
    invert = read('invert'),
    gridOrigin = read('gridOrigin'),
    tonePower = validateTonePower(read('tonePower'));
  if (!Number.isFinite(pitch) || pitch < 1 || pitch > 1024)
    throw Error('Luminance pitch must be from 1 to 1024 px');
  if (typeof invert !== 'boolean') throw Error('Luminance invert must be boolean');
  if (
    !Array.isArray(gridOrigin) ||
    gridOrigin.length !== 2 ||
    !gridOrigin.every((value) => Number.isFinite(value) && Math.abs(value) <= 100000)
  )
    throw Error('Luminance grid origin components must be finite and within ±100000 px');
  return { pitch, invert, gridOrigin, tonePower };
}

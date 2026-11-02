import { scrubNumericValues } from './numeric-scrub.js';

// Input behavior owns a pending/active gesture, never a document or its history.
// Bindings keep their existing Session or staged-draft commit/cancel authority.
export function createNumericControlBehavior(binding) {
  let phase = 'pending',
    originals,
    values;
  const same = (a, b) => a.every((value, index) => value === b[index]);
  const cancel = () => {
    if (phase === 'active') binding.cancel();
    phase = 'closed';
  };
  return {
    get active() {
      return phase === 'active';
    },
    move(dx) {
      if (phase === 'closed') return { cancelled: true };
      if (!binding.valid()) {
        cancel();
        return { cancelled: true };
      }
      if (phase === 'pending' && Math.abs(dx) < 4) return { started: false };
      if (phase === 'pending') {
        originals = binding.read();
        if (!binding.valid()) {
          cancel();
          return { cancelled: true };
        }
        // Validate the existing calculation domain before reserving the owner.
        scrubNumericValues(originals, dx, binding.step(), ...binding.bounds());
        binding.begin();
        values = [...originals];
        phase = 'active';
      }
      const next = scrubNumericValues(originals, dx, binding.step(), ...binding.bounds());
      if (!same(next, values)) {
        binding.preview(next);
        values = next;
      }
      return { started: true };
    },
    finish() {
      if (phase !== 'active') {
        phase = 'closed';
        return false;
      }
      if (!binding.valid() || same(values, originals)) {
        cancel();
        return false;
      }
      try {
        binding.commit();
        return true;
      } catch (error) {
        binding.cancel();
        throw error;
      } finally {
        phase = 'closed';
      }
    },
    cancel,
  };
}

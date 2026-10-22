import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { RecoveryAutosave } from '../src/recovery-autosave.js';
// Full New-document routing is checked in the production-CSP browser fixture.
// Here the exact production render function is exercised as a local DOM component.
test('failure selector preserves its target, distinguishes generations, and enables retry only after a failed write settles', async () => {
  const win = new Window();
  const document = win.document;
  document.body.innerHTML =
    '<button data-recovery-status></button><span data-recovery-failures hidden><select data-recovery-failed-target></select><button data-recovery-retry-target></button></span>';
  let fail = true,
    release;
  const writes = [];
  const recovery = new RecoveryAutosave({
    begin: async () => String(writes.length),
    write: async (w) => {
      writes.push(w);
      if (fail) throw Error('fixture disk');
      await new Promise((r) => (release = r));
      return 1;
    },
    status: () => updateRecoveryStatus(),
  });
  const esc = (value) =>
    String(value)
      .replaceAll('&', '&amp;')
      .replaceAll('<', '&lt;')
      .replaceAll('>', '&gt;')
      .replaceAll('"', '&quot;');
  const main = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  const code = main.slice(
    main.indexOf('function updateRecoveryStatus()'),
    main.indexOf('async function showRecoveryCopies('),
  );
  const updateRecoveryStatus = new Function(
    'document',
    'recovery',
    'esc',
    code + ';return updateRecoveryStatus;',
  )(document, recovery, esc);
  const snap = (generation) => ({
    generation,
    revision: 1,
    name: 'Same <name>',
    contents: JSON.stringify({ generation }),
    dirty: true,
  });
  try {
    recovery.observe(snap(1));
    await recovery.flush();
    recovery.observe(snap(2));
    await recovery.flush();
    const [A, B] = recovery.failedDocuments,
      select = document.querySelector('select'),
      button = document.querySelector('[data-recovery-retry-target]');
    assert.equal(document.querySelector('[data-recovery-failures]').hidden, false);
    assert.equal(select.options.length, 2);
    assert.match(select.options[0].textContent, /document 1/);
    assert.match(select.options[1].textContent, /document 2/);
    assert.equal(select.querySelector('name'), null);
    select.value = A.id;
    updateRecoveryStatus();
    assert.equal(select.value, A.id);
    assert.equal(button.dataset.recoveryRetryId, A.id);
    assert.equal(button.disabled, false);
    fail = false;
    const retry = recovery.retry(A.id, A.failedRevision);
    await new Promise((r) => setTimeout(r, 0));
    assert.equal(select.value, A.id);
    assert.equal(button.disabled, true);
    assert.match(button.textContent, /Retrying/);
    release();
    await retry;
    assert.equal(select.value, B.id);
    assert.equal(button.dataset.recoveryRetryId, B.id);
    assert.equal(button.disabled, false);
    assert.equal(recovery.state.phase, 'failed');
    const b = recovery.retry(B.id, B.failedRevision);
    await new Promise((r) => setTimeout(r, 0));
    release();
    await b;
    assert.equal(document.querySelector('[data-recovery-failures]').hidden, true);
    assert.equal(button.disabled, true);
    assert.equal(button.dataset.recoveryRetryId, undefined);
  } finally {
    await win.happyDOM.close();
  }
});

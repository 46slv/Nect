// Emitted only by the explicit test frontend, paired with the native test feature.
// No caller-supplied commands, documents, paths, failure modes or writable API.
import { document as makeDocument, serialize } from './core.js';
const A_NAME = 'NECT_RECOVERY_ACCEPTANCE_A',
  B_NAME = 'NECT_RECOVERY_ACCEPTANCE_B';
const ERROR = 'NECT_RECOVERY_TEST_ONLY_A_WRITE_FAILURE';
const delay = (ms) => new Promise((r) => setTimeout(r, ms));
const assert = (value, message) => {
  if (!value) throw Error(message);
};
const equal = (a, b, message) => assert(JSON.stringify(a) === JSON.stringify(b), message);
const frames = () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r)));
async function until(fn, label) {
  const end = Date.now() + 15000;
  while (!fn()) {
    if (Date.now() > end) throw Error('Acceptance timeout ' + label);
    await delay(20);
  }
}
export async function runRecoveryAcceptance(recovery, api) {
  const result = {
    Phase: 'starting',
    Input: 'Test-build native WebView DOM synthetic; simulated A writes only',
    Writes: [],
    Stages: [],
    WriterRestored: false,
  };
  Object.defineProperty(window, '__NECT_RECOVERY_ACCEPTANCE_RESULT__', {
    get: () => structuredClone(result),
  });
  const original = recovery.write;
  let releaseA = null,
    tokenA = null;
  const injected = new Set();
  const snapshot = () => ({
    project: api.projectSnapshot(),
    command: api.commandState,
    file: api.fileState,
    svg: api.export(),
    undoDisabled: document.querySelector('[data-action="undo"]').disabled,
    redoDisabled: document.querySelector('[data-action="redo"]').disabled,
  });
  try {
    assert(
      window.__NECT_RECOVERY_TEST_INSTANCE__ === true && window.__TAURI_INTERNALS__,
      'Isolated native test instance required',
    );
    assert(
      recovery.current?.generation === 0 &&
        api.commandState.expectedRevision === 0 &&
        !api.fileState.binding,
      'Fresh unbound test document required',
    );
    assert(!document.querySelector('[data-recovery-dialog]'), 'Unexpected Recovery dialog');
    recovery.write = async (copy) => {
      const item = { id: copy.id, name: copy.name, revision: copy.revision, outcome: 'started' };
      result.Writes.push(item);
      if (copy.name === A_NAME && copy.revision === 0 && tokenA === null) tokenA = copy.id;
      if (
        copy.name === A_NAME &&
        copy.id === tokenA &&
        JSON.parse(copy.contents).name === A_NAME &&
        [1, 2].includes(copy.revision) &&
        !injected.has(copy.revision)
      ) {
        injected.add(copy.revision);
        if (copy.revision === 2) await new Promise((r) => (releaseA = r));
        item.outcome = 'simulated-failure';
        throw Error(ERROR);
      }
      try {
        const savedAt = await original(copy);
        item.outcome = 'native-success';
        item.savedAt = savedAt;
        return savedAt;
      } catch (error) {
        item.outcome = 'native-error';
        item.error = String(error);
        throw error;
      }
    };
    const A = makeDocument(),
      B = makeDocument();
    A.name = A_NAME;
    B.name = B_NAME;
    const circle = A.layers[0].nodes[0];
    api.load(serialize(A));
    await until(
      () => recovery.current?.name === A_NAME && recovery.state.phase === 'saved',
      'A baseline',
    );
    assert(tokenA !== null, 'A native token missing');
    result.ABaselineProject = snapshot().project;
    const edit = (value) => {
      const el = document.querySelector(
        '.inspector-body [data-node="' + circle.id + '"][data-key="radius"] [data-value]',
      );
      assert(el, 'Radius UI missing');
      el.value = String(value);
      el.dispatchEvent(new Event('change', { bubbles: true }));
    };
    edit(75);
    await until(
      () =>
        recovery.failedDocuments.some(
          (f) => f.name === A_NAME && f.failedRevision === 1 && f.phase === 'failed',
        ),
      'A r1 failure',
    );
    const failure1 = recovery.failedDocuments[0],
      oldContents = recovery.current.contents;
    edit(90);
    await until(() => releaseA !== null, 'A r2 held');
    const latestA = snapshot().project;
    api.load(serialize(B));
    await until(() => recovery.current?.name === B_NAME, 'B lifetime');
    releaseA();
    releaseA = null;
    await until(
      () =>
        recovery.state.phase === 'saved' &&
        recovery.failedDocuments.some(
          (f) => f.name === A_NAME && f.failedRevision === 2 && f.phase === 'failed',
        ),
      'B native write and A r2 failure',
    );
    await frames();
    const failed = recovery.failedDocuments[0],
      before = snapshot(),
      writeCount = result.Writes.length;
    assert(
      (await recovery.retry(failed.id, failure1.failedRevision)) === false,
      'Stale failure revision accepted',
    );
    assert(
      recovery.observe({
        generation: failure1.generation,
        revision: 1,
        name: A_NAME,
        contents: oldContents,
        dirty: true,
      }) === false,
      'Stale lifetime observation accepted',
    );
    assert(result.Writes.length === writeCount, 'Stale request wrote');
    equal(snapshot(), before, 'Stale request changed B');
    const group = document.querySelector('[data-recovery-failures]');
    assert(group && !group.hidden, 'Failure group missing');
    group.querySelector('summary').click();
    await frames();
    const select = group.querySelector('[data-recovery-failed-target]');
    select.value = failed.id;
    select.dispatchEvent(new Event('change', { bubbles: true }));
    const button = group.querySelector('[data-recovery-retry-target]');
    assert(
      button.dataset.recoveryRetryId === failed.id &&
        Number(button.dataset.recoveryRetryRevision) === 2 &&
        !button.disabled,
      'Explicit A target missing',
    );
    const rect = button.getBoundingClientRect();
    assert(
      rect.width > 0 && rect.height > 0 && getComputedStyle(button).visibility !== 'hidden',
      'Retry UI not visible',
    );
    result.FailureUI = {
      HTML: group.outerHTML,
      Target: failed,
      Rect: { x: rect.x, y: rect.y, width: rect.width, height: rect.height },
    };
    result.LatestAProject = latestA;
    result.BBefore = before;
    result.Phase = 'failure-visible';
    result.Stages.push({ Phase: result.Phase, UTC: new Date().toISOString() });
    await delay(4000);
    button.click();
    await until(() => recovery.failedDocuments.length === 0, 'A UI retry success');
    await recovery.flush();
    await frames();
    equal(snapshot(), before, 'Retry changed B project/file/history/SVG');
    const retried = result.Writes.filter(
      (w) => w.id === tokenA && w.revision === 2 && w.outcome === 'native-success',
    );
    assert(retried.length === 1, 'A latest revision not written natively exactly once');
    assert(
      result.Writes.filter((w) => w.name === B_NAME).every((w) => w.outcome === 'native-success'),
      'B write was mocked/failed',
    );
    const count = result.Writes.length;
    assert((await recovery.retry(failed.id, 2)) === false, 'Resolved target accepted');
    assert(result.Writes.length === count, 'Resolved target wrote');
    result.BAfter = snapshot();
    result.ANativeToken = tokenA;
    result.BNativeToken = recovery.current.token;
    result.StaleRevisionRejected = true;
    result.StaleGenerationRejected = true;
    result.ResolvedTargetRejected = true;
    result.BUnchanged = true;
    result.UIRetryNativeSuccess = true;
    result.NativeEquivalentLimits =
      'Fixed test build adds gate/marker/automatic fixture/wrapper; normal writer, Session and renderer remain original. Not a real disk failure or physical input.';
    result.Passed = true;
  } catch (error) {
    result.Passed = false;
    result.Failure = { Message: error.message, Stack: error.stack };
  } finally {
    if (releaseA) releaseA();
    recovery.write = original;
    result.WriterRestored = recovery.write === original;
    try {
      await recovery.flush();
    } catch (error) {
      result.FinalFlushError = String(error);
    }
    result.Phase = result.Passed && result.WriterRestored ? 'passed' : 'failed';
    result.Stages.push({ Phase: result.Phase, UTC: new Date().toISOString() });
  }
}

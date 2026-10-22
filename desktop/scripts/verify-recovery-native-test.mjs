// Existing local CDP route. Never replaces the bridge or opens OS dialogs.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { chromium } from 'playwright-core';
import { exportSVG } from '../src/core.js';
const [root, kind] = process.argv.slice(2);
assert.ok(root && path.isAbsolute(root));
assert.ok(['normal', 'test'].includes(kind));
const read = async (p) => JSON.parse((await fs.readFile(p, 'utf8')).replace(/^\uFEFF/, ''));
const hash = (b) => createHash('sha256').update(b).digest('hex');
const launch = await read(path.join(root, 'evidence/launch.json')),
  identity = await read(path.join(root, 'evidence/identity.json'));
assert.equal(launch.PID, identity.PID);
assert.equal(identity.ExactStartTicksAndExeMatch, true);
assert.equal(identity.ProfileAndPortMatched, true);
assert.equal(launch.TestBuild, kind === 'test');
assert.equal(hash(await fs.readFile(launch.Executable)), launch.SHA256);
const output = path.join(root, 'native-cases');
await fs.mkdir(output);
const browser = await chromium.connectOverCDP(launch.CDP),
  errors = [];
let page,
  owned = false,
  persisted = false,
  failure = null,
  scenario = null;
const receipt = {
  Kind: kind,
  Input: 'Native WebView2; test build fixed DOM synthetic scenario',
  PID: launch.PID,
  StartUTC: launch.StartUTC,
  EXE: launch.Executable,
  SHA256: launch.SHA256,
  Profile: launch.Profile,
  RecoveryRoot: launch.RecoveryRoot,
  CloseRequests: 0,
  OSDialogs: [],
  Errors: errors,
};
try {
  await new Promise((r) => setTimeout(r, 100));
  let pages = [];
  const deadline = Date.now() + 15000;
  while (!pages.length && Date.now() < deadline) {
    pages = browser
      .contexts()[0]
      .pages()
      .filter((p) => /tauri\.localhost/.test(p.url()));
    if (!pages.length) await new Promise((r) => setTimeout(r, 50));
  }
  assert.equal(pages.length, 1);
  [page] = pages;
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  assert.equal(await page.locator('[data-recovery-dialog]:visible').count(), 0);
  owned = true;
  receipt.Native = await page.evaluate(() => ({
    URL: location.href,
    UserAgent: navigator.userAgent,
    Viewport: [innerWidth, innerHeight],
    NativeBridge: !!window.__TAURI_INTERNALS__,
  }));
  if (kind === 'normal') {
    const flags = await page.evaluate(() => ({
      Marker: typeof window.__NECT_RECOVERY_TEST_INSTANCE__,
      Result: typeof window.__NECT_RECOVERY_ACCEPTANCE_RESULT__,
      Name: window.nect.document.name,
      Revision: window.nect.commandState.expectedRevision,
    }));
    assert.equal(flags.Marker, 'undefined');
    assert.equal(flags.Result, 'undefined');
    assert.equal(flags.Revision, 0);
    receipt.NormalInjectionAbsent = flags;
  } else {
    await page.waitForFunction(
      () =>
        ['failure-visible', 'passed', 'failed'].includes(
          window.__NECT_RECOVERY_ACCEPTANCE_RESULT__?.Phase,
        ),
      null,
      { timeout: 60000 },
    );
    if (
      await page.evaluate(
        () => window.__NECT_RECOVERY_ACCEPTANCE_RESULT__.Phase === 'failure-visible',
      )
    )
      await page.screenshot({ path: path.join(output, 'failure-visible.png') });
    await page.waitForFunction(
      () => ['passed', 'failed'].includes(window.__NECT_RECOVERY_ACCEPTANCE_RESULT__?.Phase),
      null,
      { timeout: 60000 },
    );
    scenario = await page.evaluate(() => window.__NECT_RECOVERY_ACCEPTANCE_RESULT__);
    await fs.writeFile(path.join(output, 'scenario.json'), JSON.stringify(scenario, null, 2), {
      flag: 'wx',
    });
    assert.equal(scenario.WriterRestored, true);
    assert.equal(scenario.Passed, true, scenario.Failure?.Message);
    assert.equal(scenario.Phase, 'passed');
    assert.equal(scenario.Writes.filter((w) => w.outcome === 'simulated-failure').length, 2);
    assert.ok(
      scenario.Writes.filter((w) => w.name === 'NECT_RECOVERY_ACCEPTANCE_B').every(
        (w) => w.outcome === 'native-success',
      ),
    );
    assert.equal(scenario.StaleRevisionRejected, true);
    assert.equal(scenario.StaleGenerationRejected, true);
    assert.equal(scenario.ResolvedTargetRejected, true);
    assert.equal(scenario.BUnchanged, true);
    assert.equal(scenario.UIRetryNativeSuccess, true);
    assert.deepEqual(scenario.BBefore, scenario.BAfter);
    receipt.NativeScenario = {
      Passed: true,
      WriterRestored: true,
      MockFailures: 2,
      BRealWriter: true,
      StaleRevisionGenerationRejected: true,
      BProjectFileHistorySVGUnchanged: true,
    };
    await fs.writeFile(path.join(output, 'failure-ui.html'), scenario.FailureUI.HTML, {
      flag: 'wx',
    });
  }
  const current = await page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    doc: window.nect.document,
    svg: window.nect.export(),
    file: window.nect.fileState,
  }));
  assert.equal(current.svg, exportSVG(current.doc));
  const saved = path.join(launch.Profile, 'documents/native-recovery-' + kind + '-final.json'),
    text = JSON.stringify(current.project, null, 2);
  await fs.writeFile(saved, text, { flag: 'wx' });
  assert.equal(await fs.readFile(saved, 'utf8'), text);
  await fs.writeFile(path.join(output, 'final-project.json'), text, { flag: 'wx' });
  if (scenario) {
    const a = JSON.stringify(scenario.LatestAProject, null, 2);
    const f = path.join(launch.Profile, 'documents/native-recovery-A.json');
    await fs.writeFile(f, a, { flag: 'wx' });
    assert.equal(await fs.readFile(f, 'utf8'), a);
  }
  await page.screenshot({ path: path.join(output, 'final-runtime.png') });
  receipt.CLISavedFixture = {
    Path: saved,
    Bytes: Buffer.byteLength(text),
    SHA256: hash(text),
    ReadbackExact: true,
  };
  receipt.FileStateBeforeClose = current.file;
  assert.deepEqual(errors, []);
  persisted = true;
} catch (error) {
  failure = error;
  receipt.Failure = { Message: error.message, Stack: error.stack };
  if (page && owned) {
    try {
      scenario ??= await page.evaluate(() => window.__NECT_RECOVERY_ACCEPTANCE_RESULT__ || null);
      const text = await page.evaluate(() =>
        JSON.stringify(window.nect.projectSnapshot(), null, 2),
      );
      const f = path.join(launch.Profile, 'documents/native-recovery-failure.json');
      await fs.writeFile(f, text, { flag: 'wx' });
      assert.equal(await fs.readFile(f, 'utf8'), text);
      persisted = true;
      receipt.FailureBackup = f;
    } catch (e) {
      receipt.BackupError = String(e);
    }
  }
} finally {
  if (page && owned && persisted && (kind === 'normal' || scenario?.WriterRestored === true)) {
    receipt.CloseRequests = 1;
    const closed = page.waitForEvent('close', { timeout: 10000 });
    try {
      await page.evaluate(() => {
        void window.__TAURI_INTERNALS__.invoke('finish_close');
        return 'requested';
      });
      receipt.FinishCloseRequested = true;
    } catch (e) {
      receipt.CloseEvaluation = String(e);
    }
    try {
      await closed;
      receipt.PageClosed = true;
    } catch (e) {
      receipt.PageClosed = false;
      receipt.CloseUncertain = String(e);
    }
  } else
    receipt.CloseNotAttempted =
      'Ownership, readable backup or writer-restored confirmation missing';
  receipt.NotRun = ['Physical/OS input', 'OS Save/Open/PNG dialogs', 'Real disk failure'];
  await fs.writeFile(path.join(output, 'receipt.json'), JSON.stringify(receipt, null, 2), {
    flag: 'wx',
  });
  await browser.close();
}
console.log(JSON.stringify(receipt, null, 2));
if (failure) throw failure;

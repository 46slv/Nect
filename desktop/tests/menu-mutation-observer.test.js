import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';

// Exercise real Happy DOM delivery and the production menu builder with GC
// enabled only in a subprocess. Do not replace MutationObserver callbacks or
// invoke the menu builder as a substitute for a missing observer delivery.
test('synthetic menu observers retain active delivery across GC and release it on disconnect', () => {
  const source = `
    import assert from 'node:assert/strict';
    import { Window } from ${JSON.stringify(import.meta.resolve('happy-dom'))};
    import { penMenuMutationObserver } from ${JSON.stringify(new URL('./helpers/pen-menu-mutation-observer.js', import.meta.url).href)};
    import { ensureCommandMenus } from ${JSON.stringify(new URL('../src/command-menus.js', import.meta.url).href)};
    const win = new Window();
    const app = win.document.createElement('div');
    win.document.body.append(app);
    const Observer = penMenuMutationObserver(win.MutationObserver);
    let deliveries = 0;
    const observer = new Observer(() => {
      deliveries++;
      ensureCommandMenus(app, win.document);
    });
    observer.observe(app, { childList: true });
    const markup = '<header><span class="brand">Nect</span>' +
      ['new', 'open', 'save', 'save-as', 'import-svg', 'export', 'export-png']
        .map(action => '<button data-action="' + action + '">' + action + '</button>').join('') +
      '</header>';
    for (let phase = 0; phase < 3; phase++) {
      // WeakRef targets remain live through their creation turn. Cross a task
      // boundary before forcing GC, then structurally replace the header.
      await new Promise(resolve => setImmediate(resolve));
      globalThis.gc();
      app.innerHTML = markup;
      await win.happyDOM.waitUntilComplete();
      assert.equal(deliveries, phase + 1);
      assert.equal(observer.retainedCallbackCount, 1);
      assert.ok(app.querySelector('.command-menu [data-action="import-svg"]'), 'real command was rebuilt');
      const settings = app.querySelector('[data-output-settings]');
      assert.ok(settings && settings.closest('.command-menu'), 'settings remain nested in an actual menu');
    }
    observer.disconnect();
    assert.equal(observer.retainedCallbackCount, 0);
    app.innerHTML = markup;
    await win.happyDOM.waitUntilComplete();
    assert.equal(deliveries, 3);
    assert.equal(app.querySelector('.command-menu'), null);
    await win.happyDOM.close();
    console.log(JSON.stringify({ deliveries, retainedAfterDisconnect: observer.retainedCallbackCount }));
  `;
  const result = JSON.parse(
    execFileSync(process.execPath, ['--expose-gc', '--input-type=module', '-e', source], {
      encoding: 'utf8',
    }),
  );
  assert.deepEqual(result, { deliveries: 3, retainedAfterDisconnect: 0 });
});

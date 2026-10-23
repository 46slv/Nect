import assert from 'node:assert/strict';

const panelCommands = new WeakMap();

const authoredState = (page) =>
  page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    revision: window.nect.commandState.expectedRevision,
    activeComposition: window.nect.activeCompositionId,
    layers: [...document.querySelectorAll('.layer.selected')].map((el) => el.dataset.layer),
    nodes: [...document.querySelectorAll('.node.selected')].map((el) => el.dataset.id),
    undo: document.querySelector('[data-action="undo"]').disabled,
    redo: document.querySelector('[data-action="redo"]').disabled,
  }));

// Artwork acceptance queries transient controls after canvas/graph/history actions.
export async function openArtworkPanel(page, action = 'align-panel') {
  const toggle = page.locator(`.canvas-toolbar [data-action="${action}"]`);
  const attribute = action === 'align-panel' ? 'aria-pressed' : 'aria-expanded';
  if ((await toggle.getAttribute(attribute)) !== 'true') {
    const before = await authoredState(page);
    await toggle.click();
    assert.deepEqual(
      await authoredState(page),
      before,
      'opening a panel preserves authored state, history and selection',
    );
  }
  await page
    .locator(
      action === 'align-panel'
        ? '.alignment-panel'
        : `.guides-panel[aria-label="${action === 'layout-panel' ? 'Layout Grid' : 'Guides and snapping'}"]`,
    )
    .waitFor({ state: 'visible' });
  if (action === 'align-panel') {
    const commands = await page
      .locator('.alignment-panel button')
      .evaluateAll((buttons) =>
        buttons.map((button) =>
          Object.fromEntries([...button.attributes].map((a) => [a.name, a.value])),
        ),
      );
    panelCommands.set(page, commands);
  }
}

// Exercise the delegated app command even when an outside click removed its panel.
// Do not reopen UI during a modal, IME, Pen draft or captured pointer gesture.
export async function forceArtworkPanelCommand(page, attribute, action) {
  const attributes = panelCommands
    .get(page)
    ?.find((attributes) => attributes[attribute] === action);
  assert.ok(attributes, 'force uses the real panel command and its selection/reference metadata');
  await page.evaluate(
    ({ attribute, action, attributes }) => {
      const existingButton = document.querySelector(`.alignment-panel [${attribute}="${action}"]`);
      if (existingButton) {
        existingButton.disabled = false;
        existingButton.click();
        return;
      }
      const button = document.createElement('button');
      for (const [name, value] of Object.entries(attributes)) button.setAttribute(name, value);
      button.disabled = false;
      const existingPanel = document.querySelector('.alignment-panel');
      const panel = existingPanel || document.createElement('section');
      panel.className = 'alignment-panel';
      if (!existingPanel) document.querySelector('#app').append(panel);
      panel.append(button);
      button.click();
      button.remove();
      if (!existingPanel) panel.remove();
    },
    { attribute, action, attributes },
  );
}

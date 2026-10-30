import assert from 'node:assert/strict';
import fs from 'node:fs/promises';

// Exercise the production Add picker and Source selector, not direct commands.
export async function verifyCompositionReferenceAuthoring(page, addByEnter, pageErrors) {
  const receipt = { passed: false, stage: 'setup', checks: [], errors: [] };
  const project = () => page.evaluate(() => window.nect.projectSnapshot());
  const checkProject = async (expected) => assert.deepEqual(await project(), expected);
  try {
    const ids = await page.evaluate(async () => {
      const { blankDocument, layer } = await import('/src/core.js');
      const document = blankDocument(),
        target = document.layers[0],
        source = layer('Reference source');
      document.layers.push(source);
      window.nect.load(JSON.stringify(document));
      return { targetId: target.id, sourceId: source.id };
    });
    await page.locator(`[data-select-layer="${ids.targetId}"]`).click();
    const before = await project();
    receipt.stage = 'create';
    await addByEnter('Composition Reference');
    const references = await page.evaluate(() =>
      window.nect.document.layers[0].nodes.filter((node) => node.type === 'Composition Reference'),
    );
    assert.equal(references.length, 1, 'Add must retain the newly created unbound Reference');
    const nodeId = references[0].id;
    assert.equal(references[0].compositionReference, null);
    const selector = () => page.locator(`.inspector-body [data-composition-target="${nodeId}"]`);
    assert.equal(await selector().inputValue(), '');
    assert.equal(await selector().isEnabled(), true);
    const created = await project();
    receipt.checks.push('picker creates editable exact-null Reference');
    await page.locator('[data-action="undo"]').click();
    await checkProject(before);
    await page.locator('[data-action="redo"]').click();
    await checkProject(created);
    receipt.checks.push('creation is one Undo/Redo step');

    receipt.stage = 'bind';
    await selector().selectOption(ids.sourceId);
    const bound = await project();
    assert.deepEqual(
      bound.compositions[0].layers[0].nodes.find((node) => node.id === nodeId).compositionReference,
      { compositionId: ids.sourceId },
    );
    receipt.checks.push('source selector binds the same-outer Layer ID');

    receipt.stage = 'clear';
    await selector().selectOption('');
    const cleared = await project();
    assert.equal(
      cleared.compositions[0].layers[0].nodes.find((node) => node.id === nodeId)
        .compositionReference,
      null,
    );
    assert.equal(await selector().inputValue(), '');
    assert.deepEqual(cleared.compositions[0].layers[1], before.compositions[0].layers[1]);
    receipt.checks.push('clearing restores exact null and preserves source artwork');
    await page.locator('[data-action="undo"]').click();
    await checkProject(bound);
    await page.locator('[data-action="redo"]').click();
    await checkProject(cleared);
    receipt.checks.push('clear is one Undo/Redo step');

    receipt.stage = 'reopen';
    await page.evaluate((saved) => window.nect.load(JSON.stringify(saved)), cleared);
    await checkProject(cleared);
    await page.locator(`.graph .node[data-id="${nodeId}"] .node-head strong`).click();
    assert.equal(await selector().inputValue(), '');
    await selector().selectOption(ids.sourceId);
    await checkProject(bound);
    await page.locator('[data-action="undo"]').click();
    await checkProject(cleared);
    receipt.checks.push('browser project-JSON reopen retains unbound source and further editing');
    assert.deepEqual(pageErrors, []);
    await page.screenshot({
      path: 'test-results/workflow-composition-reference-unbound.png',
      fullPage: true,
    });
    receipt.passed = true;
    receipt.stage = 'complete';
    console.log('PASS Composition Reference: create/bind/clear, Undo/Redo, project JSON reopen');
  } catch (error) {
    receipt.errors.push(error.stack || String(error));
    receipt.visibleText = await page.locator('body').innerText();
    receipt.project = await project();
    await page.screenshot({
      path: 'test-results/workflow-composition-reference-failed.png',
      fullPage: true,
    });
    throw error;
  } finally {
    receipt.pageErrors = [...pageErrors];
    await fs.writeFile(
      'test-results/workflow-composition-reference.json',
      JSON.stringify(receipt, null, 2),
    );
  }
}

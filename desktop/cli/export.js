#!/usr/bin/env node
import fs from 'node:fs/promises';
import { parseArgs } from 'node:util';
import { parseFont, MAX_PROJECT_BYTES } from '../src/fonts.js';
import { exportSVG, createVariableDocumentPreparer } from '../src/core.js';
import { VARIABLE_KIND } from '../src/variable-font/document-bridge.js';
import { deserializeExportDocument } from './project-input.js';
const usage = 'Usage: node cli/export.js [--composition <id>] project.json artwork.svg';
let bridge, transport;
try {
  const { values, positionals, tokens } = parseArgs({
    options: { composition: { type: 'string' }, help: { type: 'boolean' } },
    allowPositionals: true,
    tokens: true,
  });
  if (tokens.filter((token) => token.kind === 'option' && token.name === 'composition').length > 1)
    throw Error('Specify --composition only once');
  if (values.composition !== undefined && !values.composition)
    throw Error('Composition ID must not be empty');
  if (values.help) {
    console.log(usage);
  } else {
    if (positionals.length !== 2) throw Error(usage);
    const [input, output] = positionals;
    if ((await fs.stat(input)).size > MAX_PROJECT_BYTES) throw Error('Project exceeds 50 MB');
    let project = deserializeExportDocument(await fs.readFile(input, 'utf8'), values.composition);
    const bytes = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
    const fonts = {
      default: parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
    };
    // Static projects keep the existing synchronous renderer and never load worker_threads.
    if (Object.values(project.fontAssets || {}).some((asset) => asset.kind === VARIABLE_KIND)) {
      const { createNodeVariableWorkerTransport } = await import('./variable-font-node.js');
      transport = createNodeVariableWorkerTransport();
      bridge = createVariableDocumentPreparer({ workerFactory: transport.workerFactory });
      const ticket = await bridge.prepareDocument(project);
      project = bridge.preparedDocument(ticket);
      bridge.activate(ticket);
      Object.assign(fonts, bridge.fonts());
    }
    const svg = exportSVG(project, fonts);
    await fs.writeFile(output, svg, { flag: 'wx' });
    console.log(
      `Exported ${project.layers.filter((x) => x.visible && !x.guide).length} visible layers to ${output}`,
    );
  }
} catch (e) {
  console.error(e.message);
  process.exitCode = 1;
} finally {
  bridge?.dispose();
  await transport?.dispose();
}

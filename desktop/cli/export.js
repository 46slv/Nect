#!/usr/bin/env node
import fs from 'node:fs/promises';
import { parseFont, MAX_PROJECT_BYTES } from '../src/fonts.js';
import { deserialize, exportSVG, createVariableDocumentPreparer } from '../src/core.js';
import { VARIABLE_KIND } from '../src/variable-font/document-bridge.js';
const [input, output] = process.argv.slice(2);
if (!input || !output) {
  console.error('Usage: node cli/export.js project.json artwork.svg');
  process.exit(1);
}
let bridge, transport;
try {
  if ((await fs.stat(input)).size > MAX_PROJECT_BYTES) throw Error('Project exceeds 50 MB');
  let project = deserialize(await fs.readFile(input, 'utf8'));
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
    `Exported ${project.layers.filter((x) => x.visible && !x.guide).length} visible compositions to ${output}`,
  );
} catch (e) {
  console.error(e.message);
  process.exitCode = 1;
} finally {
  bridge?.dispose();
  await transport?.dispose();
}

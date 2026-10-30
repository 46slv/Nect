#!/usr/bin/env node
import fs from 'node:fs/promises';
import { resolve } from 'node:path';
import { parseSvgPngArgs, SVG_MAX_BYTES, SVG_PNG_USAGE } from './svg-to-png-options.js';
import { publishPng, readSvgInput, requireNewOutput } from './png-output.js';

const origin = 'https://nect-png.invalid';
const modules = new Map([
  ['/cli/svg-png-source.js', new URL('./svg-png-source.js', import.meta.url)],
  ['/cli/svg-to-png-options.js', new URL('./svg-to-png-options.js', import.meta.url)],
  ['/src/raster.js', new URL('../src/raster.js', import.meta.url)],
  ['/src/images.js', new URL('../src/images.js', import.meta.url)],
  ['/src/artboard-units.js', new URL('../src/artboard-units.js', import.meta.url)],
]);
async function withinDeadline(promise) {
  let timer;
  try {
    return await Promise.race([
      promise,
      new Promise((_, reject) => {
        timer = setTimeout(() => reject(Error('SVG rendering exceeded 30 seconds')), 30_000);
      }),
    ]);
  } finally {
    clearTimeout(timer);
  }
}

let browser;
try {
  const options = parseSvgPngArgs(process.argv.slice(2));
  if (options.help) console.log(SVG_PNG_USAGE);
  else {
    const output = resolve(options.output);
    await requireNewOutput(output);
    const source = await readSvgInput(options.input, SVG_MAX_BYTES);
    if (!(await fs.stat(options.chromiumPath)).isFile())
      throw Error('Chrome executable must be an existing file');
    const sources = new Map();
    for (const [path, url] of modules) sources.set(origin + path, await fs.readFile(url, 'utf8'));
    const { chromium } = await import('playwright-core');
    browser = await chromium.launch({ executablePath: options.chromiumPath, headless: true });
    const context = await browser.newContext({ serviceWorkers: 'block' }),
      blocked = [],
      errors = [];
    await context.route('**/*', async (route) => {
      const request = route.request(),
        url = request.url();
      if (request.method() === 'GET' && url === origin + '/')
        return route.fulfill({
          contentType: 'text/html',
          headers: {
            'Content-Security-Policy':
              "default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; img-src data: blob:; connect-src 'none'; font-src 'none'",
          },
          body: '<!doctype html><title>Nect PNG adapter</title><link rel="icon" href="data:,">',
        });
      if (request.method() === 'GET' && sources.has(url))
        return route.fulfill({ contentType: 'text/javascript', body: sources.get(url) });
      blocked.push(url);
      await route.abort('blockedbyclient');
    });
    const page = await context.newPage();
    page.on('pageerror', (error) => errors.push(error.message));
    page.setDefaultTimeout(30_000);
    await page.goto(origin + '/');
    const result = await withinDeadline(
      page.evaluate(
        async ({ source, sizing, ppi }) => {
          const { prepareSvgSource } = await import('/cli/svg-png-source.js'),
            { pngDimensions, rasterizeSVG } = await import('/src/raster.js'),
            prepared = await prepareSvgSource(source),
            size = pngDimensions(prepared.size, sizing),
            blob = await rasterizeSVG(prepared.svg, size, ppi),
            bytes = new Uint8Array(await blob.arrayBuffer());
          let binary = '';
          for (let i = 0; i < bytes.length; i += 8192)
            binary += String.fromCharCode(...bytes.subarray(i, i + 8192));
          return { data: btoa(binary), ...size };
        },
        { source, sizing: options.sizing, ppi: options.ppi },
      ),
    );
    if (blocked.length || errors.length)
      throw Error('SVG rendering attempted unsupported resources or failed');
    // Finish browser teardown before publishing: teardown failure leaves no output.
    await browser.close();
    browser = null;
    await publishPng(output, Buffer.from(result.data, 'base64'));
    console.log(
      `Exported ${result.width}x${result.height} PNG${options.ppi === null ? '' : ` at ${options.ppi} PPI`} to ${output}`,
    );
  }
} catch (error) {
  console.error(`SVG to PNG failed: ${error.message}`);
  process.exitCode = 1;
} finally {
  await browser?.close();
}

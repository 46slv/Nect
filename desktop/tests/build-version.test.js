import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { checkedBuildVersion } from '../scripts/build-version.mjs';
import config from '../vite.config.mjs';

test('frontend version and static version marker come from the checked Cargo source', () => {
  for (const mode of ['production', 'recovery-acceptance-test']) {
    const build = config({ mode });
    const version = checkedBuildVersion(fileURLToPath(new URL('..', import.meta.url)), {
      tauriConfig: undefined,
    });
    assert.equal(JSON.parse(build.define.__NECT_APP_VERSION__), version);
    const assets = [];
    build.plugins[0].generateBundle.call({ emitFile: (asset) => assets.push(asset) });
    assert.equal(
      assets.find((asset) => asset.fileName === 'nect-build-version.txt').source,
      version,
    );
  }
});

test('checked build sources reject every version drift, effective overlay and malformed overlay', () => {
  const root = mkdtempSync(join(tmpdir(), 'nect-build-versions-'));
  const native = join(root, 'src-tauri');
  mkdirSync(native);
  const write = (path, value) =>
    writeFileSync(path, typeof value === 'string' ? value : JSON.stringify(value));
  function reset() {
    write(
      join(native, 'Cargo.toml'),
      '[package]\nname = "fixture"\nversion = "0.1.0"\n[dependencies]\n',
    );
    write(join(root, 'package.json'), { version: '0.1.0' });
    write(join(native, 'tauri.conf.json'), {
      version: '0.1.0',
      build: { frontendDist: '../dist' },
    });
  }
  const check = (tauriConfig = undefined) =>
    checkedBuildVersion(root, { platform: 'windows', tauriConfig });
  try {
    reset();
    assert.equal(check(), '0.1.0');
    for (const value of ['0.2.0', '0.1.0+001', '0.1', null, 1]) {
      reset();
      write(join(root, 'package.json'), { version: value });
      assert.throws(() => check(), /VERSION_DRIFT|INVALID_VERSION/);
      reset();
      write(join(native, 'tauri.conf.json'), { version: value });
      assert.throws(() => check(), /VERSION_DRIFT|INVALID_VERSION/);
      reset();
      write(join(native, 'Cargo.toml'), `[package]\nversion = "${value}"\n`);
      assert.throws(() => check(), /VERSION_DRIFT|INVALID_VERSION/);
      reset();
      assert.throws(
        () => check(JSON.stringify({ version: value })),
        /VERSION_DRIFT|INVALID_VERSION/,
      );
      reset();
      write(join(native, 'tauri.windows.conf.json'), { version: value });
      assert.throws(() => check(), /VERSION_DRIFT|INVALID_VERSION/);
      rmSync(join(native, 'tauri.windows.conf.json'));
    }
    reset();
    assert.equal(check('{"build":{"frontendDist":"../dist-recovery-acceptance-test"}}'), '0.1.0');
    write(join(native, 'tauri.windows.conf.json'), {
      build: { frontendDist: '../dist-recovery-acceptance-test' },
    });
    assert.equal(check(), '0.1.0');
    rmSync(join(native, 'tauri.windows.conf.json'));
    for (const text of ['{', 'null', '[]', '"0.1.0"', '{"version":{}}', '{"version":"v0.1.0"}'])
      assert.throws(() => check(text));
    write(join(native, 'tauri.windows.conf.json'), '{');
    assert.throws(() => check());
  } finally {
    rmSync(root, { recursive: true });
  }
});

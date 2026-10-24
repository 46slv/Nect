import { readFileSync, existsSync } from 'node:fs';
import { join } from 'node:path';
import { validateVersionSet } from '../src/update-version.js';

const platformNames = {
  win32: 'windows',
  darwin: 'macos',
  linux: 'linux',
  android: 'android',
  ios: 'ios',
};
function merge(base, overlay) {
  if (!overlay || typeof overlay !== 'object' || Array.isArray(overlay)) return overlay;
  const value = base && typeof base === 'object' && !Array.isArray(base) ? { ...base } : {};
  for (const [key, item] of Object.entries(overlay)) {
    if (item === null) delete value[key];
    else
      Object.defineProperty(value, key, {
        value: merge(value[key], item),
        enumerable: true,
        configurable: true,
        writable: true,
      });
  }
  return value;
}
const readJSON = (path) => JSON.parse(readFileSync(path, 'utf8'));
export function checkedBuildVersion(
  root,
  {
    platform = process.env.TAURI_ENV_PLATFORM ?? platformNames[process.platform],
    tauriConfig = process.env.TAURI_CONFIG,
  } = {},
) {
  const native = join(root, 'src-tauri');
  const cargoText = readFileSync(join(native, 'Cargo.toml'), 'utf8');
  const packageTable = cargoText.match(/^\[package\]\s*\n([\s\S]*?)(?=^\[|$(?![\s\S]))/m)?.[1];
  const cargo = packageTable?.match(/^version\s*=\s*"([^"\r\n]+)"\s*$/m)?.[1];
  const npm = readJSON(join(root, 'package.json')).version;
  const base = readJSON(join(native, 'tauri.conf.json'));
  let effective = base;
  const overlayPath = platform && join(native, `tauri.${platform}.conf.json`);
  if (overlayPath && existsSync(overlayPath)) effective = merge(effective, readJSON(overlayPath));
  if (tauriConfig !== undefined) {
    const overlay = JSON.parse(tauriConfig);
    if (!overlay || typeof overlay !== 'object' || Array.isArray(overlay))
      throw Error('Invalid TAURI_CONFIG overlay');
    effective = merge(effective, overlay);
  }
  const validation = validateVersionSet({
    cargo,
    npm,
    tauriBase: base.version,
    tauriEffective: effective?.version,
  });
  if (!validation.ok) throw Error(`Build version ${validation.reason}`);
  return cargo;
}

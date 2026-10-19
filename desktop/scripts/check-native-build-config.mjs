import { readFile } from 'node:fs/promises';

const [config, cargo, packageJson] = await Promise.all([
  readFile('src-tauri/tauri.conf.json', 'utf8'),
  readFile('src-tauri/Cargo.toml', 'utf8'),
  readFile('package.json', 'utf8'),
]);
const tauri = JSON.parse(config);
const pkg = JSON.parse(packageJson);
const checks = {
  frontendDist: tauri.build?.frontendDist === '../dist',
  customProtocolFeature: /custom-protocol\s*=\s*\[\s*"tauri\/custom-protocol"\s*\]/.test(cargo),
  releaseCommandUsesTauriCli:
    /desktop:build/.test(JSON.stringify(pkg.scripts)) &&
    /tauri/.test(pkg.scripts['desktop:build'] || ''),
};
for (const [name, pass] of Object.entries(checks)) console.log(`${pass ? 'PASS' : 'FAIL'} ${name}`);
if (Object.values(checks).some((pass) => !pass)) process.exitCode = 1;

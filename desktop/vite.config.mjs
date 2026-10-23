import { defineConfig } from 'vite';
import { fileURLToPath } from 'node:url';
import { checkedBuildVersion } from './scripts/build-version.mjs';
export default defineConfig(({ mode }) => {
  const appVersion = checkedBuildVersion(fileURLToPath(new URL('.', import.meta.url)));
  const acceptance = mode === 'recovery-acceptance-test';
  const profile = acceptance ? 'recovery-acceptance-test' : 'normal';
  return {
    define: {
      __NECT_RECOVERY_ACCEPTANCE_BUILD__: JSON.stringify(acceptance),
      __NECT_APP_VERSION__: JSON.stringify(appVersion),
    },
    build: { outDir: acceptance ? 'dist-recovery-acceptance-test' : 'dist' },
    plugins: [
      {
        name: 'nect-build-profile',
        generateBundle() {
          this.emitFile({
            type: 'asset',
            fileName: 'nect-build-profile.txt',
            source: profile + '\n',
          });
          this.emitFile({
            type: 'asset',
            fileName: 'nect-build-version.txt',
            source: appVersion,
          });
        },
      },
    ],
  };
});

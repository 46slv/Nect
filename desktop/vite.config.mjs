import { defineConfig } from 'vite';
export default defineConfig(({ mode }) => {
  const acceptance = mode === 'recovery-acceptance-test';
  const profile = acceptance ? 'recovery-acceptance-test' : 'normal';
  return {
    define: { __NECT_RECOVERY_ACCEPTANCE_BUILD__: JSON.stringify(acceptance) },
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
        },
      },
    ],
  };
});

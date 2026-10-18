import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const scenarioKey = 'NECT_DOM_SCENARIO';

/**
 * Run each DOM scenario in its own Node process so Window/module allocations
 * are reclaimed between scenarios. The child re-enters the same test file and
 * executes only the selected nested test callback.
 */
export function isolatedDomScenarios(rawTestContext, sourceUrl) {
  const selected = process.env[scenarioKey];
  const sourcePath = fileURLToPath(sourceUrl);

  return {
    test(name, callback, options) {
      if (selected) {
        return name === selected ? rawTestContext.test(name, callback, options) : Promise.resolve();
      }

      return rawTestContext.test(
        name,
        () => {
          const env = { ...process.env, [scenarioKey]: name };
          delete env.NODE_TEST_CONTEXT;
          const result = spawnSync(
            process.execPath,
            ['--test', '--test-reporter=tap', sourcePath],
            {
              cwd: process.cwd(),
              env,
              encoding: 'utf8',
              maxBuffer: 8 * 1024 * 1024,
              timeout: 60_000,
            },
          );
          if (result.error) throw result.error;
          const output = [result.stdout, result.stderr].filter(Boolean).join('\n');
          assert.equal(result.status, 0, output || `Isolated scenario failed: ${name}`);
          assert.ok(
            output.includes(`# Subtest: ${name}`),
            `Isolated scenario did not run: ${name}\n${output}`,
          );
        },
        options,
      );
    },
  };
}

# Variable-font dependency notices

Captured 2026-10-17 for fontkit 2.0.4. `manifest.json` records exact npm versions,
registry tarball URLs and integrity strings, notice SHA-256 hashes, and which
packages occur in the isolated browser build. `browser-inputs.json` records that
build's inputs. Package metadata is retained verbatim. Pako is included in the
installed runtime dependency closure although absent from that browser build.
Harfbuzzjs was a separate experiment and is not part of this dependency closure.

## MIT declarations and scoped notice-packaging uncertainty

Exact npm gitHead source audits confirm that fontkit 2.0.4, dfa 1.2.0 and
brotli 1.3.3 explicitly declare MIT in their pinned package.json and README.
They do not ship a complete project-level copyright/license notice. Their
PROVENANCE.md files retain commit IDs, exact declarations and source inventories.
No copyright holder or year has been invented; synthetic MIT files were removed.

An absent LICENSE file does not establish absent licensing. [npm's package.json
license documentation](https://docs.npmjs.com/cli/v8/configuring-npm/package-json/#license)
supports SPDX identifiers. [SPDX MIT](https://spdx.org/licenses/MIT) supplies the
canonical terms. These references explain the declaration format; they are not
claimed as notices authored by the three package maintainers. The remaining
review is scoped to carrying declared terms and available exact attribution and
embedded notices in the distributed application. It is not a definitive legal
prohibition or a global development stop. Preserve actual notices from each
component; do not substitute unrelated fixture/vendor copyrights for port authors.

## Embedded and generated material

- Fontkit's base64 decoder attributes base64-arraybuffer. Its upstream MIT license
  is retained separately with source URL and hash.
- The exact pinned Google Brotli submodule LICENSE has also been recovered with
  its original Brotli Authors copyright; it does not clear the port-level gap.
- Brotli decoder source files carry Google Apache-2.0 headers despite the npm
  package's MIT label. All distinct headers and their source paths are retained,
  together with full Apache-2.0 terms (copied from @swc/helpers' license).
- Unicode data terms are retained separately for generated property/shaper data.
- Roboto's unchanged binary, full OFL, and original provenance live in
  `tests/fixtures/variable-font/`. It is a test fixture, not a system font or a
  production font asset. The OFL applies to that font, not Nect's application code.

These notices document the inspected pinned dependency set; re-audit upon updates.
A release packager must include applicable notices with distributed application
copies. This directory alone does not establish that a binary release includes them.

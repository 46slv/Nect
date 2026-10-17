# Brotli 1.3.3 exact-source notice audit

Retrieved 2026-10-17. npm release metadata https://registry.npmjs.org/brotli/1.3.3
identifies gitHead `e36051345f6d27a56e5f39ea70d2f789b8574046` and tarball
https://registry.npmjs.org/brotli/-/brotli-1.3.3.tgz . Its integrity is retained in
../manifest.json. Official repository devongovett/brotli.js redirects to
https://github.com/foliojs/brotli.js (repository ID 24569923).

`upstream-tree.json` preserves the complete recursive, nontruncated GitHub tree:
https://api.github.com/repos/foliojs/brotli.js/git/trees/e36051345f6d27a56e5f39ea70d2f789b8574046?recursive=1
There is no project LICENSE, COPYING, NOTICE, or other named license file in this
tree. `upstream-readme.md` contains only the project-level declaration `MIT`:
https://raw.githubusercontent.com/foliojs/brotli.js/e36051345f6d27a56e5f39ea70d2f789b8574046/readme.md
The npm package's package.json names Devon Govett as author and MIT as license;
this is not a recovered full grant/copyright notice. **MIT is explicitly declared; a full project notice is absent.**
This is a bundling-notice review item, not a finding of absent licensing or
a legal prohibition. npm supports SPDX license declarations; see ../README.md. Synthetic MIT
terms have been removed, and no copyright year has been inferred.

## Actual vendored upstream notice recovered

The pinned tree's vendor/brotli gitlink is
`5ce9bf11b3fe0924d87b2a2d47eb7a53a76a4421`. `upstream-gitmodules.txt` binds that
submodule to google/brotli:
https://raw.githubusercontent.com/foliojs/brotli.js/e36051345f6d27a56e5f39ea70d2f789b8574046/.gitmodules
`VENDOR-GOOGLE-LICENSE.txt` is the exact full MIT grant from that pinned upstream:
https://raw.githubusercontent.com/google/brotli/5ce9bf11b3fe0924d87b2a2d47eb7a53a76a4421/LICENSE
Git blob SHA1: 33b7cdd2dbaeddce1e35aa1a13f63d71154dc42f.
It contains “Copyright (c) 2009, 2010, 2013-2016 by the Brotli Authors.” This is
attribution for that vendor source, not an invented grant for the JavaScript port.

`DECODER-NOTICES.txt` retains verbatim distinct Apache-2.0 source headers from
installed 1.3.3 dec/*.js, including the original Google 2013 copyright notices.
`Apache-2.0.txt` retains the full terms, originally copied from the identical
license in @swc/helpers. These decoder headers likewise do not supply a complete
project-level notice for unheaded port/wrapper files.

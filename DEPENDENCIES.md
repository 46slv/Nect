# Dependencies / distribution boundary

M0 uses:
- C++20 standard library
- Boost.JSON headers/implementation
- CMake 3.24+
- Python 3 for black-box process tests

Qt 6 is intentionally not an M0 dependency; it enters with the M1 desktop client.

No font binaries, proprietary SDKs, OpenFX plugins, credentials or paid API entitlements are committed.

## Rules

- Do not auto-download large dependencies during normal configure.
- Record dependency role, version and license when adding one.
- Keep protocol/host SDKs at adapter boundaries.
- Do not make Adobe/Resolve/OFX availability a requirement for creating/editing a standalone native document.

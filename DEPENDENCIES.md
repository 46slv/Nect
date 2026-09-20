# Dependencies / distribution boundary

M0 uses:
- C++20 standard library
- Boost.JSON headers/implementation
- CMake 3.24+
- Python 3 for black-box process tests

Qt 6 is intentionally not an M0 dependency; it enters with the M1 desktop client.

Windows development baseline (2026-09-20): MSVC 19.29.30137 (VS 2019),
Boost 1.85.0 (Boost Software License 1.0), Qt 6.5.3 MSVC 2019 x64
(Core/Gui/Widgets/Network/Test, LGPLv3/GPLv3/commercial upstream options).
These are local development dependencies; no distribution license decision is made.
Archives are kept under ignored `build/deps`; configure never downloads them.
Qt is dynamically linked outside the core. Boost.JSON is compiled once from
the installed headers; `BOOST_ALL_NO_LIB` prevents MSVC from also requesting
separately built Boost libraries via automatic linking.

No font binaries, proprietary SDKs, OpenFX plugins, credentials or paid API entitlements are committed.

Editable Text uses the Windows system DirectWrite runtime (`dwrite.dll`, linked
through the installed Windows SDK import library). Development uses the existing
Windows SDK 10.0.16299.0 headers, `IDWriteTextLayout2`, `IDWriteTextRenderer1` and
glyph orientation transforms. This adds no downloaded or redistributed library;
fonts remain installed-system references subject to their own licenses.
Non-Windows authoring/codec builds retain text metadata but explicitly reject its
geometry projection until a shaping backend is provided.

## Rules

- Do not auto-download large dependencies during normal configure.
- Record dependency role, version and license when adding one.
- Keep protocol/host SDKs at adapter boundaries.
- Do not make Adobe/Resolve/OFX availability a requirement for creating/editing a standalone native document.

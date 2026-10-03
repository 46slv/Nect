# R06-C2A developer qualification

This small spine implements the [frozen G15 packet](https://app.notion.com/p/3eafd279a6f3812ca7fac315c3a1def7)
and its size/lifetime/threading amendment. G15 narrows the earlier
[G9 architecture](https://app.notion.com/p/3eafd279a6f38163b2c1f54be68e6383)
to `test_internal` qualification. It does not implement REQ-68 production
capability, a real-domain evaluator, native ExtensionInstance preservation,
Effects registration, enable/install UI or Document/Session commands. C2B stays
behind the separate real-domain ABI gate.

`extension_abi.h` uses MSVC Windows x86_64 C linkage, `__cdecl`, 8-byte maximum
member alignment, and size/major/minor headers on every extensible public struct.
The provider checks size before reading later fields and writes only the v1
prefix. Larger trailing structs are accepted; undersized prefixes are rejected.
All C2A headers require API 1.0. The only evaluator takes a finite scalar and a
finite factor in [0,4]; overflow returns a status without publishing output.
No C++/Qt/STL/Document object, exception or variable-size allocation crosses the
valid ABI. Provider handles use provider allocation and matching destruction.
Provider descriptors/UTF-8 strings remain immutable until unload; the host keeps
copied manifest data after checking exact descriptor agreement. This profile
needs no cross-module buffer allocator or arbitrary host services.

Explicit roots contain `extension.json` or immediate package directories.
Discovery sorts paths, validates a bounded 64 KiB manifest, rejects duplicate
JSON members/IDs and unknown fields, resolves final Windows handle paths to
reject junction/symlink escapes, and verifies SHA-256 without loading code.
Optional top-level metadata uses `metadata:org.example.name`; it grants no
capability. C2A accepts one Windows x86_64 binary, one test_internal scalar type
BehaviorVersion 1, one factor schema (default 1, bounds 0..4), native=true and
SVG=unsupported. No version migration or multi-version resolution is inferred.

`DeveloperHarness::load_owned_test` is the only execution boundary. Its caller
explicitly asserts ownership and supplies trust bound to canonical package root,
package ID, package version, binary SHA-256 and platform/architecture. It checks
the snapshot again and holds a read-only file lock across hash/load/library
lifetime. DLL search excludes the working directory/PATH. Discovery roots grant
no execution trust; changed hash/version requires fresh exact trust. There is no
product or document-open call site. The CLI further restricts loading to the
owned `org.example.nect.testop.multiply` fixture.

Native DLL code is trusted in-process code with the Nect process/user's OS
authority, **not a sandbox**. Hash checks establish identity/integrity, not
safety. A native crash, invalid pointer or undefined behavior can corrupt/crash
the host; catching C++ boundary violations is only a defensive diagnostic.
Callbacks across all harnesses are serialized/non-reentrant. Stable instances
may evaluate/close concurrently; moving an object or ending its C++ lifetime
while another thread uses it requires caller coordination. Every live instance
pins its matching library. Destroy runs before final unload; a throwing destroy
is never retried and quarantines its DLL/read-only file lock to process exit.
Explicit `close()` reports that boundary error; the destructor cannot throw.
Unsafe hot unload/reload and a general extension framework are absent.

Build the standalone focused project using existing dependencies only:

```powershell
cmake -S tests/extension_harness -B build-c2a -G "Visual Studio 16 2019" -A x64 -DBOOST_INCLUDE_DIR=D:/Documents/Nect/build/deps/boost_1_85_0
cmake --build build-c2a --config Release --parallel 1
ctest --test-dir build-c2a -C Release -V -j 1
```

Coordinate Windows builds with the shared `Local\NectBuild` mutex in a `finally`
block. The root owner may use the OFF-by-default snippet in `integration.cmake`;
this worker does not edit root CMake. No product target links this developer
library. The owned provider is plain C without Qt/Boost; the intentionally broken
exception fixtures compile the same tiny provider as C++. No binary is committed
or installed. Generated fixtures and junctions remain inside fresh test scratch.

The focused contract executes manifest-only discovery, exact identity refusals,
deterministic evaluation, descriptor/callback/status/exception/size/parameter
negatives, guarded-page bounded prefix checks, provider allocation/free counts,
concurrent serialization and lifetime/unload checks. It is not full regression
or product native-envelope acceptance.

The generated `build-c2a/owned-package` contains the DLL and SHA-bound manifest.
Inspecting it does not execute the DLL. Explicit execution requires copying the
exact ID/version/SHA from inspection into a deliberate owned-test command:

```powershell
build-c2a/Release/extension_harness.exe inspect build-c2a/owned-package
build-c2a/Release/extension_harness.exe evaluate-owned-test build-c2a/owned-package org.example.nect.testop 1.0.0 <EXACT_SHA256> 3 2
```

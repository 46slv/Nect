# Image alpha Mask verification

Base PR71: `8d657b744a1b5259eab16ab0ed19fbda920ac865`.
Product commit: `deac549164728800d9fd7188943c489f6bf56f69`.

The Mask material socket accepts Image-typed output alongside closed Path,
through the existing source picker, revisioned Session and shared renderer.
Only authored Image-mask connections advance schema51. The old Path socket
representation, geometry and paint semantics are preserved.

- 37 focused tests passed, no skips: 8 new Image-alpha tests plus existing
  Mask/Feather model/DOM, Image effects and Composition Reference. The 8 new
  model tests were rechecked after the final test edits and passed.
- PR71's 9 saved JSON/SVG fixtures remained byte-identical. PR70 goldens also
  passed in the focused suite. No Path/default-Feather migration was introduced.
- Production Web bundle/CSP with headless Edge: 12 JSON/SVG/PNG documents passed
  actual Web Export PNG download and full RGBA equality with the same SVG.
  Source-picker Image/Keyer switching, one Undo/Redo and repeated no-op passed.
- Alpha0/128/255, partial source alpha product64, RGB independence, unchanged
  image detail, Keyer+Feather, Invert complement, placement/size, later nonuniform
  scale/rotation and Composition stroke alpha passed. Complement error was zero
  over normal and feathered fixtures. External requests/page errors were zero.
- Editing examples use embedded synthetic assets only. Their Graph positions
  were separated for readability; exported SVG stayed byte-identical to the
  checked fixture. PNGs are outside Git. Example instructions identify the raw
  Image versus processed Keyer alpha and existing coordinate/transform limits.
- Frontend/config checks and D-target offline custom-protocol native release
  compile passed (40.80 seconds). No native EXE was launched and no installer
  was built. The existing fixed frontendDist is still on C; target/TEMP/output
  are on D. All 28 prior EXEs and 423 sealed files matched before/after building.
- Exact PID139120 read-only query returned absent. A live start-time/profile
  match therefore was unavailable. No other PID, shutdown request or old
  profile was used to complete this task.

Working evidence is under
`D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\mask-image-alpha-20261022`.
The separate `mask-image-alpha-artifact-deac549` directory contains the native
EXE, bundle, examples/PNGs, headless cases, logs, source delta and SHA-256 manifest.

Not verified: native GUI for this feature, physical input, OS Open/Save/PNG
dialogs and normal OS cold reopen. Arbitrary pre-Mask Image Transform routes
remain outside current Image graph support; Image Position/Size and later
appearance transforms are covered. No main merge, release or Library upload.

## Native runtime follow-up

The same EXE subsequently passed five dedicated native Tauri/WebView2 states,
including Canvas-versus-SVG full RGBA, Image/Keyer alpha, Feather/Invert,
Undo/Redo, document placement/appearance transform and JSON project reload.
The dedicated process exited through one `finish_close` request after CLI saving
the fixture; parent/browser PIDs and CDP listener were absent. See
[native runtime acceptance](mask-native-runtime-verification.md). Physical/OS
input, native file dialogs and normal Save/close/cold-reopen remain unverified.

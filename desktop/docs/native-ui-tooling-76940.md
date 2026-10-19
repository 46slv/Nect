# Native UI tooling evidence

Source: `76940ea316a58add594d745d6203fdb180af9aa2` (EXE artifact).

The bundled Computer Use inventory initially exposed no Windows app/window. A
workspace-local Python 3.12 virtual environment was therefore used for a
non-persistent UIA probe, after confirming pywinauto is not installed globally.
`pywinauto==0.6.9` was installed from PyPI into `.ui-venv` only; no PATH,
system setting, remote access, credential, or permission change was made.

With `NECT_SVG_ACCEPTANCE_ROOT` set to a fresh child of the EXE directory, the
EXE launched and pywinauto UIA enumerated two windows for its PID: `Nect SVG`
and one child window. Calling `set_focus()` on `Nect SVG` returned successfully;
window rectangle was `(L32, T32, R2050, B1204)`.

This proves native window acquisition and one safe focus operation. It does not
prove save/reload, context-menu, numeric expression, preset, or Japanese IME
acceptance; those remain pending a fixture-specific interaction pass.

The captured isolated window showed the Tauri shell, but WebView2 displayed
`ERR_NETWORK_ACCESS_DENIED` for the editor content. No localhost/network
restriction was bypassed, and save/reload, context-menu, numeric, preset, and
IME flows are therefore `NOT_RUN`.

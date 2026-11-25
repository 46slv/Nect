# Authoring and verification

Run commands from `desktop/` with that checkout's dependencies installed (see the repository README). Write to an existing directory you control; do not overwrite the reference project or a user's delivery.

## Editable baseline

- Open `examples/live-blend.json` in the Windows editor with **Project → Open JSON**. Follow `examples/live-blend.md`: change Steps or an endpoint, check the live result, Undo/Redo in the same document, use **Save As**, and reopen the copy. Verify that the source endpoints and Blend remain editable, not flattened into a baked SVG.
- Pixel and editable-project evidence differ. Before handing off a novel artwork, reopen it using the intended product surface. For user-authored text or linked assets, check the fonts or assets on the receiving machine; the baseline example does not exercise those.

## SVG smoke test without the desktop

```shell
node cli/export.js examples/live-blend.json path/to/verified-live-blend.svg
```

Use a valid, distinct output path with an existing parent. Inspect the generated SVG (XML root, viewBox, visible paths); compare it with the expected example and look for missing geometry. If an exact byte match is expected, compare hashes and record the specific revision. Native Save As, Undo, layout, and external MCP are outside this test.

## Runtime language in reports

- **CLI verified**: export ran against the given project and the output was checked.
- **Browser verified**: editing was exercised in the browser harness, not the native app.
- **Native verified**: editing, reopen, or export was actually executed in the Windows app.
- **MCP verified**: the supported server and client were used end to end for the named command. Neither a direct import of a test helper nor a CLI pass suffices.

## Hermes discovery (opt-in)

Keep the skill at the Git root for trusted project discovery. The Nect commands above still run from desktop/.

Hermes does not need this entire repository in its skill directory. Point its
`skills.external_dirs` configuration at the absolute path to this checkout's
`.agents/skills` directory in the intended profile, keeping the other
configured entries. For example (replace the path):

```yaml
skills:
  external_dirs:
    - 'D:/work/Nect/.agents/skills'
```

Use the profile's skill list to confirm that `nect-artwork-workflow` appears.
Then open the skill by name, and ask its skill reader for
`references/authoring-and-verification.md` only when the command is needed.
Loading the skill is not proof that a model selected it in a real request.

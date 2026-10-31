# UI sound source handoff

This is an isolated Nect development prototype, not a wired-in product feature.
The module, tests, documentation and selected audition WAV were created for this
Nect request by an OpenAI assistant. The WAV was synthesized from generated
noise and a small damped resonance; it is not a recording, third-party sound
sample, or copied external audio asset. The JavaScript embeds the selected PCM;
it does not load the WAV at runtime. The WAV is included as the test reference.
No external audio library or asset is bundled. No new third-party license terms
are asserted here; apply the repository's terms where applicable.

The standalone README references optional earlier tonal archives and generators
that are not part of this narrow handoff. The included reference WAV and test
file are sufficient for the standalone `node --test ui-sound.test.mjs` check.
The production integration owner must review the source, ensure accessibility and
preference behavior, and verify packaged Windows Tauri/WebView2 behavior.

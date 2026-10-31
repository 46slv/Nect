# Nect UI feedback sound — isolated prototype

Not integrated into Nect. The user-selected v2 audition is a short, dry mechanical transient (~6 ms). Its 48 kHz PCM is embedded directly in the module, and linearly resampled once to the AudioContext sample rate; no external audio assets, libraries, network requests, or user data collection. At master gain 0.12, the 48 kHz buffer matches the selected offline WAV to Float32 precision. Other sample rates are interpolated and are not bit-identical. The packaged app output has not been auditioned or validated.

## Suggested integration

1. Import `installUiSound` once at the application/document shell, not in repeatedly rendered components. Mark *specific* eligible controls with `data-ui-sound="tick"` or provide a narrow selector. This deliberately does not make arbitrary elements audible.
2. Read the persisted sound setting and proposed volume; instantiate once. A default `enabled: true` and `volume: 0.12` are **proposals**, not evidence that 0.12 sounds right. Persist updates in the app's existing preference system. Call `setEnabled(false)` as soon as sound is turned off; call `setVolume(value)` when volume changes. Values are finite-number-converted, clamped to 0–1; invalid values become 0. Disable should remain a separate preference from volume.
3. Dispose on document/app teardown. Repeated installation on the same document returns the existing controller (and ignores later options); after `dispose()`, installation may start afresh. Do not use this as a per-component hook.

```js
import { installUiSound } from './ui-sound.mjs';
const uiSound = installUiSound({ enabled: preferences.uiSounds, volume: preferences.uiSoundVolume });
// On preference change:
uiSound.setEnabled(preferences.uiSounds);
uiSound.setVolume(preferences.uiSoundVolume);
// At application teardown:
uiSound.dispose();
```

The single shared context is created only on a trusted pointerdown or non-repeating keydown when enabled, visible and focused. A document without a callable, successful `hasFocus()` is treated as unfocused (fail silent). A suspended context is resumed on that gesture; failure is swallowed, and a later gesture can retry. Hover before/during unlock is silent and never queued or replayed. Sound is generated only for trusted **mouse** pointer entry into a matching, connected, rendered control. Pointer-down/drag, touch and coarse-primary-pointer devices, hidden or unfocused documents, disabled/inert/hidden ancestors and keyboard navigation do not make a sound. Distinct adjacent controls can make a rapid sequence; movement within one matching control is deduplicated. Window blur or document hiding stops active voices; focus permits later eligible entries, with no replay. Mouse re-entry reconciles a missed release using the trusted event’s `buttons` state, and native dragstart/dragend independently suppress sound. Turning sound off immediately sets the shared master gain to zero, stops active voices and prevents new voices. A shared AudioBuffer holds the ~6 ms sound, while each tick uses a separate one-shot source, disconnected on `ended` and stopped/disconnected on dispose. Audio failures are swallowed rather than breaking UI interaction.

## Verify before production

- Audition the actual packaged Windows Tauri 2.9 / WebView2 environment with real speakers and headphones at varied system levels, including subtlety, click harshness, rapid icon traversal, CPU and voice accumulation. Unit tests cannot establish sound quality or loudness.
- Confirm actual app selector mapping, nested controls and overlays, disabled/inert logic, accessibility requirements, preference storage/migration and the desired default. Test window blur/background/foreground, multiple windows, resume/device changes, error handling and repeated mounting/disposal in the packaged app.
- Consider browser/OS autoplay policy and real trusted-gesture behavior, pointer capture, missed outside releases and native drag event behavior, touch/mouse hybrid devices, and shadow DOM or iframe boundaries. This prototype checks the document's *primary* pointer coarse media query, so a hybrid device with a coarse primary pointer suppresses even a connected mouse. Shadow-root traversal and iframes are not explicitly supported.
- No time-based rate limiting or global voice cap is implemented: rapid traversal of many distinct controls can produce many short overlapping clicks. Decide after listening whether a cap is desirable without removing the requested sequential ticks. No keyboard-navigation sound is currently implemented.
- Native event behavior (focus, pointer capture, outside release, and drag), actual gesture unlock, and audio output have not been exercised in a real browser or packaged Tauri/WebView2 app here. Errors degrade silently, so absence of sound does not itself identify the cause.

## Tests

Node 24, no installation: `node --test ui-sound.test.mjs`. Fakes verify event filtering, lazy singleton context, gesture unlock/no queue, errors, gain updates and disposal. They are not an audio-quality, device or integration test.

## Selected audition and references

The selected v2 offline WAVs use a 6 ms dry noise transient and small rapidly damped resonance. `make_hard_tick_candidate.py` and `hard-tick-candidate-metadata.json` document and reproduce them; no WAV is loaded at runtime. The earlier 1.9 and 2.6 kHz tonal versions remain in `archive-1900hz/` and `archive-2600hz/`. Buffer interpolation, audio device processing and the packaged WebView may affect the audible result and require real-device testing.

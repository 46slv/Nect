# HEX-derived HSL saturation

`hexToHsl` canonicalizes only its computed saturation output. Six-digit HEX is
validated first. All in-range S precision, H and L are retained; a characterized
overshoot above 100 becomes 100. Nonfinite S, negative S or a value above the
measured ceiling throws an internal-domain error.

`hslToHex` keeps its exact finite/range validation. `updateHexHsl` replaces the
requested channel **after** HEX analysis, so a user-entered S above 100 still
rejects even when it has the same number as an accepted derived overshoot.
There is no general input clamp or epsilon allowance for user input.

## Finite characterization

The raw implementation at `d8420660215674816a5b3492ffe455fa2b753eb7`, blob
`4a965fe88e5d00e8da568eaf2c116c9cbd1315e1`, was evaluated over all 16,777,216
RGB triplets in Node 24.14.1 / V8 13.6 and independently in Chrome 156.0.8078.47.
Both produced the same extrema and excess histogram:

| Measurement                                     | Result                                           |
| ----------------------------------------------- | ------------------------------------------------ |
| S below zero, nonfinite H/S/L, out-of-range H/L | 0                                                |
| S above 100                                     | 48,336 triplets                                  |
| Maximum S                                       | 100.00000000000071                               |
| ULP at 100                                      | 2^-46 = 1.4210854715202004e-14 percentage points |
| Maximum excess                                  | 50 ULP = 7.105427357601002e-13 percentage points |
| Inputs attaining the maximum                    | 24, including `#fbfbff`                          |

The guard uses this exact measured ceiling, with no extra headroom. The near-white
S ratio amplifies cancellation in `1 - abs(2*L - 1)`; H and L need no correction.
The full extrema, histogram and worst-input set are retained in
`tests/fixtures/hex-hsl-derived-characterization.json`.

Reproduce the baseline measurement from `desktop/` with:

```sh
node scripts/characterize-hex-hsl.mjs
node scripts/characterize-hex-hsl.mjs --current
```

The default script loads the pinned Git implementation rather than duplicating
its formula; that commit must be available locally, and the script never fetches.
`--current` checks the current public output and exact RGB roundtrip over the same
full domain. Algorithm/domain changes require updating the raw measurement harness
and data before changing the ceiling. This is a UI numerical boundary correction, with no schema, solver,
color-semantics or palette-spec revision adoption.

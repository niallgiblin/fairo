# Fairo — Pharaoh-style fuzz pedal emulation (JUCE 8 plugin)

A guitar-effects plugin (VST3 / AU / Standalone) that emulates the
**Black Arts Toneworks Pharaoh** fuzz pedal — the Ram's Head-era Big Muff
Pi variant with switchable clipping and the dual Tone/High tonestack.

Built on the scaffolding conventions of the author's **fuzzyband** project
(JUCE 8, C++20, FetchContent, Catch2 tests).

> **Status**: Phases 0–2, Phase 3 (SPICE + neural), Phase 4 basics done (see
> [PLAN.md](PLAN.md)). A full ngspice model of the Pharaoh is built from the
> schematic-trace values (`spice/`), validated, and used to generate a
> synthetic dry/wet training corpus (`training/data/`, 4 min per branch from
> the author's own DI recordings). Per-branch WaveNet-class models are
> trained, exported to ONNX (`assets/fuzz_*.onnx`) and run through ONNX
> Runtime at <1 ms/512-block (p95) behind a **DSP/Neural engine switch** in
> the UI - DSP-only remains the fallback.

## Signal path

```
guitar ─➤ [Input: Hi/Lo R (39k/390k) + C1 10uF] ─➤ [Fuzz drive R24]
       ─➤ [Clip 1: fixed 2× 1N914 @ collector (Newton-Raphson)]
       ─➤ [Interstage RC: 47nF + Q2 bias]
       ─➤ [Stage-2 gain] ─➤ [Clip 2: Si 1N4001 / Ge 1N34A / Bypass branch]
       ─➤ [Coupled Tone/High stack (C9/R5/R8/C8/R25/C3 — one MNA network)]
       ─➤ [Make-up gain + soft limit] ─➤ [Volume R26] ─➤ out
```

Key design decisions, per PLAN.md:

- **Diode clippers use the Shockley equation solved per-sample** (Newton +
  bisection hybrid), not static tanh/atan waveshaping — preserves the
  level-dependent sustain/punch of real diode clipping.
- **The tonestack is one coupled RC network** solved node-by-node (MNA +
  trapezoidal integration, MPT-style), *not* two cascaded filters — the AAU
  thesis's pitfall #1.
- **2× oversampling** of the nonlinear chain via JUCE polyphase filters
  (Phase 4 baseline).
- All circuit values live in `src/dsp/CircuitValues.h` for easy tuning.

## Parameters

| Parameter   | Type   | Range / choices          | Notes |
|-------------|--------|--------------------------|-------|
| Fuzz        | float  | 0..1                     | drive into clip stage 1 |
| Volume      | float  | 0..1                     | output level |
| Tone        | float  | 0..1                     | bass ↔ treble balance |
| High        | float  | 0..1                     | treble restore |
| Hi/Lo       | switch | Lo / Hi                  | ~15 dB input-level shift |
| Clip mode   | choice | Silicon / Germanium / Bypass | second-clipper branch |

## Building

Requires CMake ≥ 3.22, a C++20 compiler, and Xcode CLT on macOS.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release   # add -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/juce to reuse a checkout
cmake --build build --config Release --parallel
ctest --test-dir build --output-on-failure   # unit tests
```

Formats: VST3 everywhere, AU on macOS, optional Standalone
(`-DFA_BUILD_STANDALONE=OFF` to skip).

## Offline sandbox

`python/` contains NumPy/SymPy prototypes of the DSP math and exports golden
data into `tests/data/`, which the C++ tests validate against:

```sh
python3 python/diode_clipper.py   # transfer curves -> tests/data/diode_curves.csv
python3 python/tonestack.py       # impulse responses -> tests/data/tonestack_impulse.csv
```

`research/` holds schematic references and extracted component evidence.

## Neural engine (Phase 3, optional)

Build with `-DFA_ENABLE_ONNX=ON -DONNXRUNTIME_ROOT=/path/to/onnxruntime`
(ARM64 macOS release matching the CI step), then use the **Engine** switch
(DSP / Neural) in the editor for a like-for-like A/B. The neural models are
trained per clip-branch on SPICE-simulated pairs with randomized control
settings across all Hi/Lo × Si/Ge/Bypass combinations; DSB-only builds ship
unchanged.

## Roadmap

- [x] Phase 0 — scaffolding, parameter layout, Python sandbox
- [x] Phase 1 — input stage + fixed first clipper (Newton-Raphson)
- [x] Phase 2 — switchable second clipper + coupled tonestack + output stage
- [x] Phase 3 — ngspice netlist, SPICE-generated synthetic data, conditioned
      WaveNet models + ONNX Runtime swap (DSP/Neural engine switch)
- [~] Phase 4 — UI basics, automatable params, 2× oversampling (neural path
      runs native-rate; DSP path oversampled)
- [ ] Phase 5 — A/B validation against real-pedal references, CPU budget, release

See [PLAN.md](PLAN.md) for full details, open questions, and sources.

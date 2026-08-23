# PLAN.md — Pharaoh Fuzz Clone (JUCE Plugin)

**Goal:** Build a JUCE-based guitar effects plugin (VST3/AU/Standalone) that
emulates the Black Arts Toneworks **Pharaoh** fuzz pedal as closely as
possible — not just "a fuzz pedal in the spirit of," but matching its
specific switchable-clipping topology, dual tone stack, and dynamic
behavior (sustain, attack punch).

This plan follows the project pattern already established by the author's
existing plugin **fuzzyband** ("Metal Accompaniment") — JUCE 8, VST3/AU/
Standalone targets, ONNX Runtime for on-audio-thread ML inference at
sub-1ms p99 latency. Reuse that scaffolding and CI/build setup where
possible rather than starting a new JUCE project from scratch.

---

## 1. What we're modeling (circuit summary)

The Pharaoh is a **Ram's Head-era Big Muff Pi variant**, not an original
topology. Signal path, in order:

1. **Input stage** — a **Hi/Lo switch** selects between two input series
   resistors. This is primarily a *gain/headroom* shift into the clipping
   stages (~15 dB signal-level difference per SPICE analysis in source
   [1]), not a tone filter in itself. A **Fuzz** pot sets signal level
   feeding the first clipper.
2. **First clipping stage** — one transistor (2N5089) in common-emitter
   config + **fixed** symmetrical clipping via two 1N914 silicon diodes.
   Not user-switchable.
3. **Second clipping stage** — another transistor feeding a **3-way
   switch**:
   - **Silicon**: 1N4001 diodes — harsher/brighter.
   - **Germanium**: three 1N34A diodes wired *asymmetrically* (two in
     series, opposing one single diode) — positive half-cycle clips at
     ~0.3 V, negative half-cycle at ~0.6 V. This asymmetry is what gives
     the "warm, tubey" character. Large volume drop vs. the other two
     modes.
   - **Bypass**: no diode clipper — signal relies on transistor-only
     clipping. The most "open"/hairy fuzz setting.
4. **Tonestack** — the pedal's key differentiator from a stock Muff.
   Instead of the classic passive RC tone stack (which has a built-in
   mid-scoop and real volume loss), the Pharaoh uses a **Tone** pot
   (full-range bass↔treble balance) plus an added **High** pot that
   restores treble cut by the Tone control when boosting lows.
5. **Signal caps** — 470 nF (vs. the stock Muff's 47 nF), so bass passes
   mostly uncut. This is why bassists and doom/stoner players like it —
   no squashed low end.
6. **Output stage** — make-up gain transistor + Volume pot.

Component values / part numbers (Q1 = MPSA18, Q2–4 = 2N5089) and the
canonical schematic are in sources [2]–[4] below. **Note:** community
sources disagree on some coupling-cap values (47 nF vs. 470 nF) — verify
against source [3]'s comment thread and, if possible, direct measurement,
before finalizing filter coefficients.

---

## 2. Known pitfalls (from prior art — read before implementing)

Source [1] is a 2017 master's thesis that did a full gray-box emulation
of *this exact pedal* (SPICE + recorded analysis signals + real-time
Pure Data/Bela implementation + blind-ish listening evaluation with two
guitarists). Two findings should directly shape this implementation:

- **Do not cascade the Tone and High filters as two independent biquads.**
  The thesis measured each filter's frequency response independently via
  white-noise excitation and got accurate per-filter curves — but
  cascading them naively produced "accurate filter design, inaccurate
  sound." The real circuit's RC networks interact through the pot in a
  way that doesn't reduce to two filters in series. Model the tonestack
  as one coupled network (see Rod Elliot / AMZ tonestack analysis
  methodology), or validate empirically against real recordings rather
  than trusting the frequency-response match alone.
- **Static/memoryless waveshaping undersells sustain and attack "punch."**
  The thesis's transfer-function approach (`V1 / (V2 + |V1|)`) matched
  waveform *shape* well but both test guitarists independently flagged
  the real pedal as having more gain, more punch, and longer sustain —
  because real diode clipping is level-dependent (with the Pharaoh,
  documented sustain of up to 25s with only 3dB decay) in a way a fixed
  nonlinear function isn't. This is the gap between "sounds similar" and
  "sounds just like it" — see Phase 3/4 below for how to close it.

---

## 3. Recommended architecture: hybrid DSP + optional neural refinement

Do **not** pick pure static-waveshaping DSP *or* pure black-box neural in
isolation. Build in this order, each phase independently useful:

```
Guitar in → [Hi/Lo + Fuzz gain stage] → [Clip Stage 1: fixed silicon]
          → [Clip Stage 2: switchable Si/Ge/Bypass branch]
          → [Coupled Tone/High stack] → [Output gain] → out
```

- **Phases 0–2** (below) build a physically-informed DSP model. This is
  the baseline and must ship working end-to-end before anything else.
- **Phase 3** is optional and can run in parallel/later: replace the
  Phase 2 nonlinear clipping stages with small conditioned neural models
  (one per Si/Ge/Bypass branch, or a single model conditioned on a
  branch-select parameter) trained on real hardware recordings, reusing
  the ONNX Runtime pipeline already proven in fuzzyband. This is the
  most direct fix for the "punch and sustain" gap identified in Section 2.
- **Phase 4** integrates whichever path(s) are complete behind a common
  plugin parameter interface, so DSP-only and neural-augmented builds can
  coexist / be A-B'd.

---

## Phase 0 — Project scaffolding

- [ ] Fork/reuse the fuzzyband JUCE 8 project scaffold (CMake setup,
      VST3/AU/Standalone targets, plugin processor/editor boilerplate).
- [ ] New project name/target (e.g. `PharaohClone`); keep fuzzyband
      untouched.
- [ ] Stand up parameter layout for: Fuzz, Volume, Tone, High, Hi/Lo
      switch (bool), Clip mode switch (enum: Si / Ge / Bypass).
- [ ] Set up an offline SPICE/Python sandbox (e.g. ngspice + Python, or
      just NumPy) for prototyping filter/clipper math before porting to
      C++ — faster iteration loop than testing inside JUCE directly.

## Phase 1 — Input stage + fixed first clipper

- [ ] Implement Hi/Lo switch as a gain-stage + first-order low-cut
      selector per source [1]/[2] values (confirm exact R values from
      schematic in source [2]).
- [ ] Implement Fuzz pot as a variable gain stage feeding Clip Stage 1.
- [ ] Implement Clip Stage 1 (fixed 1N914 symmetrical clipper) using a
      proper diode I-V model (Shockley equation) solved per-sample via
      Newton-Raphson (DK-method style), **not** a static tanh/atan
      approximation — this is what preserves level-dependent sustain
      behavior flagged in Section 2.
- [ ] Validate against source [1]'s recorded waveforms (Appendix D of the
      thesis has recorded-vs-settings comparisons) if accessible; if not,
      validate qualitatively via ear + oscilloscope-style waveform view
      in a debug build.

## Phase 2 — Switchable second clipper + coupled tonestack

- [ ] Implement Clip Stage 2 as three selectable branches:
      - Silicon (1N4001, same Newton-Raphson approach as Stage 1)
      - Germanium (asymmetric 3-diode network — model the two clipping
        thresholds separately for positive/negative half-cycles)
      - Bypass (transistor-only saturation — model as the gain stage's
        own soft clipping, not diode clipping)
- [ ] Implement the Tone/High tonestack as **one coupled two-port RC
      network** (not two cascaded filters — see Section 2 pitfall).
      Reference: AMZ/Rod Elliot tonestack analysis method, or
      transcribe the coupled transfer function directly from the
      schematic in source [2].
- [ ] Implement output gain stage + Volume pot.
- [ ] Wire clip-mode switch to hot-swap between the three Stage-2
      branches without zipper noise (crossfade or sample-accurate switch
      with denormal/DC-offset guards).

## Phase 3 — Neural refinement via SPICE-generated synthetic data

**No physical Pharaoh is available for this project.** Real-hardware
recording (dry-in/wet-out pairs from an actual unit) is therefore not
the data source — use it only later, opportunistically, for validation
(see "Optional validation" below), never as the primary training path.

Instead, generate paired training data from a **SPICE simulation of the
circuit** built from the schematic (source [2]) — this is a published,
well-established technique, not a workaround: sources [7]–[9] all train
neural distortion/amp models on data generated purely from SPICE
simulation of the target circuit (no physical unit involved), and
source [7] (Neural DSP) found the resulting neural models can match the
subjective quality of the SPICE model itself. This also means the SPICE
model built for Phase 1–2's DK-method clipping stages is directly
reusable here — no separate circuit-modeling effort required.

- [ ] Build a full SPICE netlist of the Pharaoh (LTSpice or ngspice)
      from the schematic in source [2], using component values
      cross-checked against sources [3]–[4]. This should already exist
      in some form from Phase 0's offline sandbox — extend it to cover
      the complete signal path (all 6 Hi/Lo × Si/Ge/Bypass branches),
      not just the individual stages used for Phase 1–2 validation.
- [ ] Generate a training corpus by running a library of clean DI guitar
      recordings (single notes, chords, riffs — real playing, not just
      swept tones, to capture dynamic/sustain behavior per Section 2's
      pitfall) through the SPICE simulation at randomized/swept
      parameter settings across all 6 branches. Source [8]'s finding
      that a parameter-sampling resolution of five settings per control
      is sufficient for generalization is a reasonable starting budget;
      source [5] found ~3 minutes of audio per target sufficient for
      this model class.
      - DI source material: use royalty-free/self-recorded DI guitar
        libraries or record your own (any clean DI works, since it's
        run through the simulation — this sidesteps the copyright
        concerns that using song stems or YouTube audio would raise).
- [ ] Train conditioned models per branch (LSTM/GRU or WaveNet-style,
      per source [5]'s architecture comparison), targeting the existing
      fuzzyband ONNX export pipeline for consistency — no need to adopt
      RTNeural; ONNX Runtime already handles this at sub-1ms p99 in
      fuzzyband. Source [6] (GuitarML/Proteus) and source [9] (the
      hyperconditioned-biquad paper, which trains directly against
      SPICE-generated BOSS MT-2 data) are useful architecture
      references.
- [ ] Swap Phase 2's DK-method clipper branches for the trained models
      behind a build flag or runtime toggle, so DSP-only mode remains
      available as a fallback / for A-B comparison. Note: since both
      the DSP (Phase 1-2) and neural (Phase 3) paths are trained/derived
      from the *same* SPICE model, expect this to mainly improve
      dynamic/sustain feel (Section 2's second pitfall) rather than
      introduce fundamentally new tonal information the DSP path lacks.

### Optional validation (not a data source)

If real-hardware comparison becomes possible later — borrowing, a local
pedal-rental service, a DIY community member willing to record a
standardized test file (try r/diypedals or freestompboxes.org), or a
response from Black Arts Toneworks directly — use it only to **validate**
the SPICE model and trained outputs by ear/spectrogram, not to retrain
from. Demo videos of the Pharaoh (source [10]) are useful for the same
narrow purpose — a qualitative "does this sound in the right ballpark"
check — but are not usable as training data: no isolated dry reference,
no sample alignment to a known input, and YouTube's lossy audio encoding
degrades exactly the harmonic detail a distortion model needs to learn.
Song/stem audio from commercial releases has the same alignment problem
even before considering the rights issues of training on it.

## Phase 4 — Plugin integration & UI

- [ ] Expose all pedal controls as automatable JUCE parameters (Fuzz,
      Volume, Tone, High, Hi/Lo, Clip mode).
- [ ] Match fuzzyband's UI/branding conventions if there's an intent to
      release both under the same portfolio identity.
- [ ] Add a bypass/dry toggle and true-bypass-style latency compensation
      if needed for DAW use.
- [ ] Oversample the nonlinear stages (2x–4x) to control aliasing from
      the diode clippers, consistent with standard VA practice.

## Phase 5 — Validation & release prep

- [ ] Qualitative A/B against real pedal recordings or reference clips
      if available.
- [ ] Sanity-check CPU load (target: comparable to fuzzyband's existing
      sub-1ms p99 budget per instance) across all clip-mode branches,
      DSP-only and neural-augmented builds separately.
- [ ] Follow the same distribution plan already in place for fuzzyband
      (GitHub Releases + KVR Audio + portfolio) once stable.

---

## Open questions to resolve during implementation

- Exact R/C values for the Hi/Lo switch and tonestack — cross-reference
  sources [2]–[4], since community schematics disagree on some values
  (notably the 47 nF vs. 470 nF coupling caps).
- No physical Pharaoh is available for this project — Phase 3 is
  therefore scoped around SPICE-generated synthetic training data (see
  Phase 3), not real-hardware recording. Overall fidelity ceiling is
  bounded by SPICE model accuracy rather than hardware access; treat
  refining the SPICE netlist (component tolerances, verifying the
  47nF/470nF coupling-cap discrepancy, etc.) as the highest-leverage
  place to spend extra effort if the output isn't convincing enough.
- If real-hardware validation becomes available later (see Phase 3's
  "Optional validation"), budget time to compare against it and patch
  the SPICE model / retrain — but don't block shipping on it.

---

## Sources / References

[1] Hagensen, T. L. (2017). *Analog Emulation of the Black Arts Toneworks
    Pharaoh Fuzz Guitar Effect Pedal* — AAU master's thesis. Full circuit
    analysis, SPICE data, recorded analysis signals, real-time PD/Bela
    implementation, listening evaluation.
    https://projekter.aau.dk/projekter/files/259990799/Analog_Emulation_of_the_Black_Arts_Toneworks_Pharaoh_Fuzz_Guitar_Effect_Pedal___Troels_Lunde_Hagensen.pdf

[2] *Big Muff Pi Versions and Schematics* (bigmuffpage.com) — source
    schematic for the Pharaoh, used by the AAU thesis and most DIY
    builders.
    http://www.bigmuffpage.com/Big_Muff_Pi_versions_schematics_part4.html

[3] *Black Arts Toneworks Pharaoh Fuzz clone* — Coda Effects. Diffs
    against a stock Big Muff PCB (no mid pot, switched input resistor,
    switched diodes, High pot replacing R8); comment thread flags
    coupling-cap value discrepancies (47nF vs 470nF).
    https://www.coda-effects.com/2016/04/black-arts-toneworks-pharaoh-fuzz-clone.html?m=0

[4] *Black Arts Toneworks Pharaoh* — Perf and PCB Effects Layouts.
    Verified vero/PCB layout with transistor part numbers (Q1 MPSA18,
    Q2–4 2N5089) and switch-wiring notes.
    http://effectslayouts.blogspot.com/2016/02/black-arts-toneworks-pharaoh.html

[5] Wright, A., Damskägg, E.-P., Juvela, L., Välimäki, V. (2020).
    *Real-Time Guitar Amplifier Emulation with Deep Learning*. Applied
    Sciences 10(3), 766. Benchmarks WaveNet vs. RNN black-box models
    against, among others, an Electro-Harmonix Big Muff Pi — direct
    evidence for the neural-capture approach on this circuit family, with
    a real-time JUCE reference implementation.
    https://www.mdpi.com/2076-3417/10/3/766

[6] GuitarML/Proteus — LSTM-based amp/pedal capture plugin (JUCE).
    Reference architecture for real-time neural pedal capture; uses
    RTNeural rather than ONNX Runtime, so treat as a pattern reference
    rather than a direct dependency given the existing fuzzyband ONNX
    pipeline.
    https://github.com/GuitarML/Proteus

[7] Juvela, L., Damskägg, E.-P., Peussa, A., Mäkinen, J., Sherson, T.,
    Mimilakis, S. I., Gotsopoulos, A. (2024). *End-to-End Amp Modeling:
    From Data to Controllable Guitar Amplifier Models* (Neural DSP
    Technologies). Compares a neural model against an offline SPICE
    circuit simulation; found the neural model can match the SPICE
    model's subjective quality.
    https://arxiv.org/abs/2403.08559

[8] *Sampling the user controls in neural modeling of audio devices*
    (2024), EURASIP J. Audio, Speech, and Music Processing. Trains RNNs
    on synthetic datasets generated entirely from SPICE simulation of
    an analog distortion pedal and an analog EQ; studies how densely the
    control-parameter space needs to be sampled (finds a resolution of
    five settings per control sufficient).
    https://link.springer.com/article/10.1186/s13636-024-00347-5

[9] *Lightweight and interpretable neural modeling of an audio
    distortion effect using hyperconditioned differentiable biquads*
    (2021). Trains against a BOSS MT-2 dataset generated by running DI
    guitar clips through a SPICE model of the circuit at randomized
    parameter settings — same data-generation pattern recommended for
    Phase 3.
    https://arxiv.org/pdf/2103.08709

[10] *Demos* — Black Arts Toneworks Pharaoh demo videos. Useful only
     for qualitative validation/ear-check, not as training data (no
     isolated dry reference, no sample alignment, lossy audio encoding).
     https://www.blackartstoneworks.com/category/pharaoh-demos/

---

## Context for the implementing agent

This plan builds on an existing project, **fuzzyband** ("Metal
Accompaniment"): a JUCE 8 VST3/AU/Standalone guitar plugin already doing
live CNN inference via ONNX Runtime on the audio thread (95.6% test
accuracy, sub-1ms p99 latency). Reuse its build scaffolding, plugin
boilerplate, and ONNX inference pipeline where relevant rather than
re-solving already-solved problems.

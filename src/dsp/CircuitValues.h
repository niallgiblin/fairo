#pragma once

/**
 * @file
 * @brief All circuit component values, from the Pharaoh schematic trace.
 *
 * Values sourced from research/ (see research/report.md): the bigmuffpage
 * "Pharaoh Big Muff Clone" trace (the AAU thesis's cited source, values
 * OCR-verified at 3400 px + zoom crops), the AAU thesis analysis, and
 * corroborated against King Tut v1.1 BOM, tagboard/effectslayouts layouts.
 *
 * Resolved open questions (PLAN.md):
 *  - Coupling caps C5/C6/C7/C13 = 0.047 uF (47 nF) — the trace's value; the
 *    470 nF variant is a second floating DIY build. 47 nF used here.
 *  - Input cap C1 = 10 uF tantalum (not 470 nF).
 *  - Hi/Lo: R2 = 39k (Hi) / R27 = 390k (Lo); the ~15 dB level difference
 *    comes from the divider against the Q4 base-load, modelled explicitly
 *    with kInputStageLoadR.
 *  - Tonestack: real network (C9 10nF / R5 470k / R8 25k / C8 22nF / tone
 *    pot 250k / C3 47nF) — see ToneStack.h.
 *
 * Still tunable / not published: diode SPICE parameters (thesis publishes no
 * Is/N) and the amplifier gain blocks (bias-point unknowns). Those are marked
 * MODEL below and are the Phase 1 validation targets.
 */

namespace fairo::circuit
{

// ── Input stage (research B) ────────────────────────────────────────────────
// Jack -> SPDT on-on -> R2 39k (Hi) | R27 390k (Lo) -> C1 10uF -> Q4 (MPSA18)
// base. R1 = 2M pulldown. The switch is purely a series resistance; the
// ~15 dB level difference (thesis 3.2.1) arises from the divider against the
// Q4 base-load, modelled as kInputStageLoadR (fitted so the divider ratio
// between the two positions is exactly 15 dB).
constexpr double kInputResistanceHi = 39.0e3;   // ohm — "Hi" (more level, more fuzz)
constexpr double kInputResistanceLo = 390.0e3;  // ohm — "Lo" (less level)
constexpr double kInputDecouplingCap = 10.0e-6; // F — C1, 10 uF tantalum
constexpr double kInputStageLoadR = 37.0e3;     // ohm — fitted Q4 base-load (=> 15 dB Hi/Lo)

// ── Clip stage 1: fixed 2x 1N914 silicon, symmetrical (research D) ──────────
// D3/D4 antiparallel in the collector circuit; the collector load R21 (100k)
// is the equivalent source resistance of the clip node.
constexpr double kClip1SeriesR = 100.0e3;       // ohm — R21 collector load
// Q4 amplification per thesis 3.2; the Fuzz pot (R24 DIST, 100k) is a
// signal-strength control between Q4 and clip stage 1 ("in- or decrease the
// signal strength within an interval" — thesis 3.2.2). Mapped to gain.
constexpr double kGainStage1Min = 1.5;
constexpr double kGainStage1Max = 18.0;         // MODEL: tunable, Phase 1 validation
// Raised from 1.0/12.0: at the reference DI level the signal fell *below* the
// clipping knee at mid Fuzz, so the clippers never engaged and the Hi/Lo input
// shift (~15 dB) passed straight through un-compressed. More drive saturates
// the stages (thesis §2 pitfall #2: "more gain, more punch, longer sustain").

// ── Interstage coupling (clip 1 -> stage 2) ──────────────────────────────────
constexpr double kInterstageCouplingCap = 47.0e-9;  // F — 0.047 uF (trace value)
constexpr double kInterstageBiasLeak = 470.0e3;     // ohm — Q2 bias R15 470k

// ── Clip stage 2 branches (research E) ───────────────────────────────────────
constexpr double kClip2SeriesR = 100.0e3;       // ohm — R10 collector load
constexpr double kGainStage2 = 16.0;            // Q2 CE: -R10/R11 = -100k/10k (MODEL)
// Raised from 10.0 so the second clipping stage engages on typical DIs instead
// of passing the input shift through un-clipped; this is what compresses the
// Hi/Lo output spread (reference: ~3-9 dB, not ~15 dB) and adds sustain/punch.
// Bypass branch: hard transistor-only clipping (thesis: "very hard and
// sudden"). Saturation knee ~ rail-limited collector swing.
constexpr double kBypassSaturationVoltage = 2.5;

// ── Tone / High stack (research F — the real network) ───────────────────────
// Q2 coll -> C9 0.01uF -> node A -> R5 470k -> GND            (fixed HP, ~34 Hz)
// node A -> R8 HIGH 25k (rheostat) -> node B -> C8 0.022uF -> GND  (variable LP)
// R25 TONE 250k: lug1 -> node A, lug3 -> node B, wiper -> C3 0.047uF -> Q1 base
constexpr double kToneHpCap = 10.0e-9;          // F  — C9
constexpr double kToneHpR = 470.0e3;            // ohm — R5
constexpr double kHighPotMax = 25.0e3;          // ohm — R8, 25k linear
constexpr double kToneLpCap = 22.0e-9;          // F  — C8
constexpr double kTonePotMax = 250.0e3;         // ohm — R25, 250k linear
constexpr double kToneOutCap = 47.0e-9;         // F  — C3 to Q1 base
constexpr double kToneOutLeak = 470.0e3;        // ohm — Q1 bias R7 470k

// ── Output stage (research I) ───────────────────────────────────────────────
constexpr double kMakeupGain = 3.2;             // Q1 CE (100k/2.2k, bias-point MODEL)
// Lowered slightly (was 4.0): the clip stages now saturate harder, so the
// pre-limit signal is hotter — less post makeup keeps the output from bricking
// at the soft limit and keeps the stage-2 level gap honest.
constexpr double kOutputSoftLimit = 1.8;        // V — gentle output saturation

// ── Master ───────────────────────────────────────────────────────────────────
constexpr double kVolumeMax = 2.0;              // R26 100k VOL pot (unity at ~0.5)

// ── Oversampling ─────────────────────────────────────────────────────────────
constexpr int kOversamplingFactor = 2;          // 2x, per PLAN.md Phase 4 (2x-4x)

// ── Presence lift (post-clip brightness, clip-mode-dependent) ────────────────
// The AAU thesis (3.5) measured the real pedal's tonestack carrying genuine
// treble out to ~10 kHz, and characterizes the three clip modes differently:
//   Silicon   = "harsher"/brighter clipper (3.4.2)  → most top lift.
//   Germanium = "warmer, rounded" clipper     (3.4.1) → modest, stays warm.
//   Bypass    = open/hairy (3.3.1)                  → light cut, avoid fizz.
// A single fixed shelf voiced all three the same (Fairo's clip modes were
// tonally flat in validation); the per-mode gain lets each read its own
// character, matching the thesis. Corner is the "air/attack" band. The NAM
// reference is itself dark (passive DI at capture), so tune by ear.
constexpr double kPresenceShelfFc = 5000.0;               // Hz — corner (air/attack band)
constexpr double kPresenceShelfGainDbSilicon   = 11.0;    // dB — bright/harsh
constexpr double kPresenceShelfGainDbGermanium =  5.0;    // dB — warm
constexpr double kPresenceShelfGainDbBypass    = -1.0;    // dB — tame the fizz

// ── Parked Phase 3 neural "body/fat" enhancer (not used by the plugin) ──────
// Measured when the WaveNet path was still wired: DSP sustains/compresses
// (RMS-peak gap ~2.8 dB, bass-heavy); WaveNet stayed peaky/thin (gap ~14 dB)
// because its ~23 ms receptive field under-captures long-term compression.
// Restore as post-model makeup if inference is rewired:
//   kNeuralBodyFc = 160 Hz, kNeuralBodyBoostDb = 4.5 dB, kNeuralSatDrive = 1.8

} // namespace fairo::circuit

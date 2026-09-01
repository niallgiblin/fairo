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
constexpr double kGainStage1Min = 1.0;
constexpr double kGainStage1Max = 12.0;         // MODEL: tunable, Phase 1 validation

// ── Interstage coupling (clip 1 -> stage 2) ──────────────────────────────────
constexpr double kInterstageCouplingCap = 47.0e-9;  // F — 0.047 uF (trace value)
constexpr double kInterstageBiasLeak = 470.0e3;     // ohm — Q2 bias R15 470k

// ── Clip stage 2 branches (research E) ───────────────────────────────────────
constexpr double kClip2SeriesR = 100.0e3;       // ohm — R10 collector load
constexpr double kGainStage2 = 10.0;            // Q2 CE: -R10/R11 = -100k/10k (MODEL)
// Bypass branch: hard transistor-only clipping (thesis: "very hard and
// sudden"). Saturation knee ~ rail-limited collector swing.
constexpr double kBypassSaturationVoltage = 3.0;

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
constexpr double kMakeupGain = 4.0;             // Q1 CE (100k/2.2k, bias-point MODEL)
constexpr double kOutputSoftLimit = 2.0;        // V — gentle output saturation

// ── Master ───────────────────────────────────────────────────────────────────
constexpr double kVolumeMax = 2.0;              // R26 100k VOL pot (unity at ~0.5)

// ── Oversampling ─────────────────────────────────────────────────────────────
constexpr int kOversamplingFactor = 2;          // 2x, per PLAN.md Phase 4 (2x-4x)

// ── Presence lift (post-clip brightness) ─────────────────────────────────────
// Fairo's distortion generates fewer high harmonics than the real pedal /
// reference capture, leaving the top darker (measured ~-7..-14 dB vs the
// MAXED_SIL capture). The tonestack is flat above ~500 Hz, so this is fixed
// with a gentle high-shelf after the soft-clip, not a tonestack change.
// Tunable: raise/lower kPresenceShelfGainDb to taste.
constexpr double kPresenceShelfFc = 3000.0;     // Hz — shelf corner
constexpr double kPresenceShelfGainDb = 6.0;    // dB boost above the corner

// ── Parked Phase 3 neural "body/fat" enhancer (not used by the plugin) ──────
// Measured when the WaveNet path was still wired: DSP sustains/compresses
// (RMS-peak gap ~2.8 dB, bass-heavy); WaveNet stayed peaky/thin (gap ~14 dB)
// because its ~23 ms receptive field under-captures long-term compression.
// Restore as post-model makeup if inference is rewired:
//   kNeuralBodyFc = 160 Hz, kNeuralBodyBoostDb = 4.5 dB, kNeuralSatDrive = 1.8

} // namespace fairo::circuit

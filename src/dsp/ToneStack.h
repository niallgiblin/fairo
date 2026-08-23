#pragma once

/**
 * @file
 * @brief Coupled Tone/High tonestack ("one network, not two cascaded filters").
 * PLAN.md Section 2 pitfall #1: the thesis measured each filter separately and
 * cascading them sounded wrong, because the pot couples the RC networks.
 * This class solves the *entire* passive network per sample with @ref
 * RcNetwork (MNA + trapezoidal), so pot interactions are exact — the same
 * technique SPICE uses.
 *
 * NETLIST — the real circuit, from the schematic trace (research/report.md):
 *
 *   clip2 ── C9 (10n) ──● A ── R5 (470k) ── GND        fixed HP ~34 Hz
 *                        │
 *                    R8 HIGH (0..25k rheostat)
 *                        │
 *                       ● B ── C8 (22n) ── GND        variable LP
 *                        │
 *   R25 TONE 250k: lug1 ─ A, lug3 ─ B, wiper ─● W
 *                                             │ C3 (47n)
 *                                             ● OUT ── R7 (470k) ── GND (Q1 base)
 *
 * The Tone pot *weighs the balance* between the HP and LP sections through
 * the pot itself — exactly why this must be one solved network, not two
 * cascaded filters (PLAN.md pitfall #1).
 */

#include "RcNetwork.h"

namespace fairo
{

class ToneStack
{
public:
    ToneStack();

    /** @brief Prepare at a sample rate (call once in prepareToPlay). */
    void prepare(double sampleRate);

    /** @brief Tone pot, 0..1 (0 = full bass, 1 = full treble). */
    void setTonePot(float v) noexcept;
    /** @brief High pot, 0..1 (0 = cut, 1 = restored treble). */
    void setHighPot(float v) noexcept;

    /** @brief Process one sample at the internal rate. */
    float processSample(float input) noexcept;

    /** @brief Clear filter state. */
    void reset() noexcept;

private:
    void rebuild();

    RcNetwork network_;
    float tonePot = 0.5f;
    float highPot = 0.5f;
};

} // namespace fairo

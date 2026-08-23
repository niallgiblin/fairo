#pragma once

/**
 * @file
 * @brief Shockley-equation diode shunt clipper, solved per-sample with
 *        Newton-Raphson (PLAN.md Phase 1: "DK-method style").
 *
 * Topology modelled (classic Big Muff / Pharaoh clipper):
 *
 *   Vin ──R──●── Vx ── out
 *            │
 *        [fwd stack]    (f diodes in series, anode at Vx)
 *        [rev stack]    (b diodes in series, cathode at Vx)
 *            │
 *           GND
 *
 * KCL at Vx:  (Vin - Vx)/R = Id(Vx)
 *
 *   Id(Vx) = Is * (exp(Vx / (f*n*Vt)) - 1)          (forward stack)
 *          - Is * (exp(-Vx / (b*n*Vt)) - 1)         (reverse stack)
 *
 * which is solved for Vx with Newton-Raphson, warm-started from the previous
 * sample's node voltage (the signal changes by <1 mV/sample, so 2-4
 * iterations converge). This preserves the *level-dependent* behaviour of
 * real diode clipping (PLAN.md Section 2 pitfall 2) that static tanh/atan
 * waveshapers lose.
 *
 * Branch presets (asymmetric Ge wiring per PLAN.md Phase 2):
 *  - Silicon:   1N914 anti-parallel pair (f=1, b=1), symmetric.
 *  - Germanium: three 1N34A, single diode clipping one half-cycle at ~0.3 V
 *               and the series pair clipping the other at ~0.6 V (f=1, b=2).
 *  - Bypass:    no diode stack (f=0, b=0) — signal passes unclipped; handled
 *               by the engine's transistor-saturation model instead.
 */

namespace fairo
{

struct DiodeClipConfig
{
    /** Diodes in series in the forward stack (conducts when Vx > 0); 0 = none. */
    int forwardCount = 1;
    /** Diodes in series in the reverse stack (conducts when Vx < 0); 0 = none. */
    int reverseCount = 1;

    /** Saturation current Is (A). */
    double saturationCurrentIs = 2.52e-9;   // 1N914
    /** Diode ideality factor N. */
    double emissionN = 1.752;               // 1N914
    /** Series resistance R between input and the clip node (ohms). */
    double seriesResistance = 1.0e3;

    /** Thermal voltage n*k*T/q (V); 25.85 mV at 300 K. Fixed for an audio plugin. */
    double thermalVoltageVt = 0.02585;

    // ── Factory presets (values are tunable constants; see CircuitValues.h) ──

    /** Stage 1: fixed symmetrical 2× 1N914 silicon (PLAN.md Phase 1). */
    static DiodeClipConfig silicon1N914(double seriesR);
    /** Stage 2, Si branch: 1N4001 anti-parallel pair. */
    static DiodeClipConfig silicon1N4001(double seriesR);
    /** Stage 2, Ge branch: single 1N34A vs. series pair, asymmetric. */
    static DiodeClipConfig germanium1N34A(double seriesR);
    /** Stage 2, Bypass branch: no diodes (transistor-only saturation). */
    static DiodeClipConfig bypass();
};

class DiodeClipper
{
public:
    /** @brief Configure the branch. Also resets state. */
    void setConfig(const DiodeClipConfig& cfg);

    /** @brief Process one sample at the internal (oversampled) rate.
     *  Warm-starts the Newton solve from the previous node voltage. */
    float processSample(float input) noexcept;

    /** @brief Warm-start-free processing; used right after reset/parameter jumps. */
    void reset() noexcept { lastVx = 0.0; }

    /** @brief True when both stacks are empty (Bypass) — output == input. */
    bool isBypass() const noexcept { return cfg.forwardCount == 0 && cfg.reverseCount == 0; }

private:
    /** Newton-Raphson solve of KCL for the clip-node voltage. Returns NaN only
     *  on non-convergence (defensive; callers scrub with a clamp). */
    double solveNodeVoltage(double vin, double initialVx) const noexcept;

    DiodeClipConfig cfg;
    double lastVx = 0.0;
};

} // namespace fairo

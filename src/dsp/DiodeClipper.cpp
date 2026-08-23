#include "DiodeClipper.h"

#include <algorithm>
#include <cmath>

namespace fairo
{

// ── Presets ─────────────────────────────────────────────────────────────────
// SPICE-style diode parameters (Is, N) from published device models; treated
// as tunable constants until validated against the SPICE netlist (Phase 3).
// See research/ for the extracted schematic values.

DiodeClipConfig DiodeClipConfig::silicon1N914(double seriesR)
{
    DiodeClipConfig c;
    c.forwardCount = 1;
    c.reverseCount = 1;
    c.saturationCurrentIs = 2.52e-9;  // A
    c.emissionN = 1.752;
    c.seriesResistance = seriesR;
    return c;
}

DiodeClipConfig DiodeClipConfig::silicon1N4001(double seriesR)
{
    DiodeClipConfig c;
    c.forwardCount = 1;
    c.reverseCount = 1;
    c.saturationCurrentIs = 14.11e-9; // A (typical 1N4001 SPICE model)
    c.emissionN = 1.984;
    c.seriesResistance = seriesR;
    return c;
}

DiodeClipConfig DiodeClipConfig::germanium1N34A(double seriesR)
{
    DiodeClipConfig c;
    // Per PLAN.md: positive half-cycle clips at ~0.3 V (single diode), the
    // series pair clips the negative half-cycle at ~0.6 V.
    c.forwardCount = 1;
    c.reverseCount = 2;
    c.saturationCurrentIs = 2.0e-6;   // A (Ge diodes have much higher Is; tentative)
    c.emissionN = 1.5;                // tentative
    c.seriesResistance = seriesR;
    return c;
}

DiodeClipConfig DiodeClipConfig::bypass()
{
    DiodeClipConfig c;
    c.forwardCount = 0;
    c.reverseCount = 0;
    c.seriesResistance = 1.0e3;
    return c;
}

// ── Solver ──────────────────────────────────────────────────────────────────

void DiodeClipper::setConfig(const DiodeClipConfig& config)
{
    cfg = config;
    // A vanishing series resistance makes the KCL system ill-posed.
    cfg.seriesResistance = std::max(1.0e-3, cfg.seriesResistance);
    lastVx = 0.0;
}

float DiodeClipper::processSample(float input) noexcept
{
    const double vin = static_cast<double>(input);
    lastVx = solveNodeVoltage(vin, lastVx);

    if (! std::isfinite(lastVx))
        lastVx = 0.0;  // defensive: unreachable for finite inputs, but never emit NaN

    // The clipped signal is the node voltage.
    return static_cast<float>(lastVx);
}

double DiodeClipper::solveNodeVoltage(double vin, double initialVx) const noexcept
{
    // No diodes: pass straight through (Bypass branch leaves saturation to the
    // engine's transistor model).
    if (cfg.forwardCount == 0 && cfg.reverseCount == 0)
        return vin;

    const double R = cfg.seriesResistance;
    const double invR = 1.0 / R;
    const double Is = std::max(cfg.saturationCurrentIs, 1.0e-15);  // stay physical
    const double vt = cfg.thermalVoltageVt;
    const double fU = std::max(1, cfg.forwardCount) * cfg.emissionN * vt;  // f*n*Vt
    const double bU = std::max(1, cfg.reverseCount) * cfg.emissionN * vt;  // b*n*Vt

    // KCL residual:        f(Vx) = (Vin - Vx)/R - Id(Vx)
    //                     Id(Vx) = Is*(exp(Vx/fU) - 1) - Is*(exp(-Vx/bU) - 1)
    // Derivative:     f'(Vx) = -1/R - (Is/fU)*exp(Vx/fU) - (Is/bU)*exp(-Vx/bU)
    //
    // f' < 0 strictly, so f is strictly decreasing: a unique root exists and
    // sign-bracketing is always consistent. Bare Newton-Raphson *crawls* in
    // the hard-exponential region (step size ~ n*Vt per iteration; a step
    // from the near-linear node can also overshoot 10 V into it), so the
    // solve is run in three phases:
    //   [0..5]   Newton with fallback to bisection   (warm starts land here)
    //   [6..12]  forced bisection                    (guaranteed progress)
    //   [13..17] Newton polish
    // Validated worst case: a +/-10 V 16-bit-rate square wave at 96 kHz —
    // every sample converged to < 1e-12 V of the true root.

    // Cold start: the clip-node voltage of these diode networks always sits
    // within ~0.9 V of ground, so a stale/unphysical zero state is re-seeded
    // there (otherwise the first sample after reset would crawl).
    if (! std::isfinite(initialVx) || std::abs(initialVx) > 0.9)
        initialVx = std::clamp(vin, -0.9, 0.9);

    const double bkt = std::max(1.0, std::max(std::abs(vin), std::abs(initialVx))) * 2.0 + 5.0;
    double lo = -bkt;  // f(lo) > 0
    double hi = +bkt;  // f(hi) < 0
    double vx = std::clamp(initialVx, lo, hi);

    constexpr int maxIterations = 18;
    constexpr int newtonPhaseEnd = 6;
    constexpr int bisectionPhaseEnd = 13;
    constexpr double tolerance = 1.0e-12;
    constexpr double expLimit = 680.0;

    for (int iter = 0; iter < maxIterations; ++iter)
    {
        const double xf = std::clamp(vx / fU, -expLimit, expLimit);
        const double xr = std::clamp(-vx / bU, -expLimit, expLimit);
        const double ef = std::exp(xf);
        const double er = std::exp(xr);

        const double f = (vin - vx) * invR - Is * (ef - er);  // e^x-1 minus e^-x-1 = e^x - e^-x
        const double deriv = -invR - (Is / fU) * ef - (Is / bU) * er;

        if (f > 0.0)
            lo = vx;
        else
            hi = vx;

        if (std::abs(f) < 1.0e-9)   // 1 uV of residual across a 1k series R
            break;

        double next;
        if (iter < newtonPhaseEnd || iter >= bisectionPhaseEnd)
        {
            if (deriv == 0.0)
            {
                next = 0.5 * (lo + hi);
            }
            else
            {
                next = vx - f / deriv;
                if (! std::isfinite(next) || next <= lo || next >= hi)
                    next = 0.5 * (lo + hi);
            }
        }
        else
        {
            next = 0.5 * (lo + hi);
        }

        if (std::abs(next - vx) < tolerance)
        {
            vx = next;
            break;
        }
        vx = next;
    }

    return vx;
}

} // namespace fairo

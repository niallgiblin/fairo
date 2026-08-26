#include "ToneStack.h"

#include "CircuitValues.h"
#include "RcNetwork.h"

#include <algorithm>
#include <cmath>

namespace fairo
{

namespace
{
// Linear pot (per the trace: "Linear taper potentiometers").
float potSplit(float v) noexcept
{
    return std::clamp(v, 0.0f, 1.0f);
}
} // namespace

ToneStack::ToneStack() = default;

void ToneStack::prepare(double sampleRate)
{
    rebuild();
    network_.prepare(sampleRate);
}

void ToneStack::setTonePot(float v) noexcept
{
    const float clamped = potSplit(v);
    if (std::abs(clamped - tonePot) < 1.0e-5f)
        return;
    tonePot = clamped;
    rebuild();
}

void ToneStack::setHighPot(float v) noexcept
{
    const float clamped = potSplit(v);
    if (std::abs(clamped - highPot) < 1.0e-5f)
        return;
    highPot = clamped;
    rebuild();
}

void ToneStack::rebuild()
{
    using B = RcNetwork::Branch;
    using T = RcNetwork::Type;

    // Real network from the schematic trace (research F / report.md):
    //
    //   clip2 ── C9 (10n) ──● A ── R5 (470k) ── GND        fixed HP ~34 Hz
    //                        │
    //                    R8 HIGH (0..25k rheostat)
    //                        │
    //                       ● B ── C8 (22n) ── GND        variable LP
    //                        │
    //   R25 TONE 250k: lug1 ─ A, lug3 ─ B, wiper ─● W
    //                                             │ C3 (47n)
    //                                             ● OUT ── R7 (470k) ── GND (Q1 base)
    //
    // The Tone pot *weighs the balance* between the HP and LP sections
    // ("Both filters are connected to the 'Tone' potentiometer, which weighs
    // the balance between the two RC filters" — thesis 6.2). That coupling
    // through the pot is exactly why the stack must be one solved network
    // (PLAN.md pitfall #1), not two cascaded filters.

    // High pot: series resistance between node A and the LP section.
    // R8 25k is in series with the LP cap C8 to ground: MORE resistance =
    // weaker LP = BRIGHTER. high=1 -> 25k (treble restored); high=0 -> ~0.
    const double rHigh = circuit::kHighPotMax * static_cast<double>(highPot)
                       + 1.0;  // never fully open-circuit

    // Tone pot: 250k. t is the wiper fraction, 1 = toward node A (the bright /
    // full-range side), 0 = toward node B (the low-passed/dark side).
    // FIX (vs. the original): the old mapping had rSegA = t*max, so tone=1
    // selected the dark node B and the knob read backwards vs. the real Pharaoh
    // (tone maxed = muddy). Swapped so tone=1 -> bright node A, tone=0 -> dark.
    const double t = static_cast<double>(tonePot);
    const double rSegA = std::max((1.0 - t) * circuit::kTonePotMax, 1.0);
    const double rSegB = std::max(t * circuit::kTonePotMax, 1.0);

    // Netlist: node 0 = input (fed through the clip-2 source resistance),
    // node 1 = A, node 2 = B, node 3 = W (tone wiper), node 4 = OUT.
    network_.setTopology(circuit::kClip2SeriesR, {
        B{ T::Capacitor, 0, 1, circuit::kToneHpCap },   // C9 10nF
        B{ T::Resistor, 1, -1, circuit::kToneHpR },     // R5 470k
        B{ T::Resistor, 1, 2, rHigh },                  // R8 HIGH 0..25k
        B{ T::Capacitor, 2, -1, circuit::kToneLpCap },  // C8 22nF
        B{ T::Resistor, 1, 3, rSegA },                  // R25 lug1 -> wiper
        B{ T::Resistor, 2, 3, rSegB },                  // R25 lug3 -> wiper
        B{ T::Capacitor, 3, 4, circuit::kToneOutCap },  // C3 47nF -> Q1 base
        B{ T::Resistor, 4, -1, circuit::kToneOutLeak }, // R7 470k bias
    }, 4);
}

float ToneStack::processSample(float input) noexcept
{
    return network_.processSample(input);
}

void ToneStack::reset() noexcept
{
    network_.reset();
}

} // namespace fairo

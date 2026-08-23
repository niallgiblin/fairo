#pragma once

/**
 * @file
 * @brief Generic passive linear R/C network solver (MNA + trapezoidal).
 *
 * This is the foundation for every *coupled* RC block in the pedal:
 *  - the input stage (Hi/Lo series resistance + 470 nF coupling cap),
 *  - interstage coupling (C + bias leak),
 *  - the Tone/High tonestack as a single coupled network.
 *
 * PLAN.md Section 2 pitfall #1 forbids splitting the tonestack into two
 * independent cascaded filters; solving the *whole* network per sample (as
 * SPICE does) keeps the pot interactions exact. Pot positions are just
 * resistor values that can be updated between blocks.
 *
 * Model: Thevenin input u with input resistance Rth, arbitrary resistor and
 * capacitor branches between nodes, one output node. Capacitors integrate
 * with the trapezoidal companion model (unconditionally stable for passive
 * networks); resistor stamps are pure conductances. Per sample this solves a
 * dense NxN linear system (N <= kMaxNodes) with a small heap-free Gaussian
 * elimination — trivial CPU cost (N is <= 4 for every block in this pedal).
 */

#include <array>
#include <cstddef>

namespace fairo
{

class RcNetwork
{
public:
    static constexpr int kMaxNodes = 7;  // internal nodes only (ground excluded)

    enum class Type { Resistor, Capacitor };

    /** Two-terminal branch. a/b are node indices in [0, kMaxNodes); -1 = ground. */
    struct Branch
    {
        Type type = Type::Resistor;
        int a = -1;
        int b = -1;
        double value = 0.0;  // ohms for Resistor, farads for Capacitor
    };

    /** @brief (Re)build the topology. Clears all state.
     *  @param inputResistance Thevenin series resistance from the input source
     *         to node 0 (the first internal node is the "input node").
     *  @param branches        R/C branch list (may be empty).
     *  @param outputNode      Node to read as output (0-based internal index). */
    void setTopology(double inputResistance,
                     std::initializer_list<Branch> branches,
                     int outputNode);

    /** @brief Prepare at a sample rate; clears history (call before processing). */
    void prepare(double sampleRate);

    /** @brief Switch the Thevenin input resistance (Hi/Lo switch). */
    void setInputResistance(double ohms) noexcept;

    /** @brief Update the value of resistor branch @a branchIndex (pot movement).
     *  Must be a Resistor branch. */
    void setResistorValue(int branchIndex, double ohms) noexcept;

    /** @brief Process one sample. Returns the output-node voltage. */
    float processSample(float input) noexcept;

    /** @brief Clear history (capacitor states) without rebuilding topology. */
    void reset() noexcept;

    /** @brief Number of internal nodes in the current topology (0 = clear). */
    int numNodes() const noexcept { return numNodes_; }

private:
    void assemble() noexcept;
    bool solveLinearSystem(double* A, double* b, double* x) const noexcept;

    std::array<Branch, 16> branches{};
    int numBranches = 0;
    int numNodes_ = 0;
    int outputNode_ = 0;
    double inputResistance_ = 1.0e3;
    double sampleRate_ = 44100.0;
    double h_ = 1.0 / 44100.0;

    // Capacitor companion-model state (per branch index in branches_).
    std::array<double, 16> capVPrev{};  // last voltage across the cap (Va - Vb)
    std::array<double, 16> capIPrev{};  // last current a->b through the cap
};

} // namespace fairo

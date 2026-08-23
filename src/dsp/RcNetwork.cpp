#include "RcNetwork.h"

#include <algorithm>
#include <cmath>

namespace fairo
{

void RcNetwork::setTopology(double inputResistance,
                            std::initializer_list<Branch> branchList,
                            int outputNode)
{
    const int count = static_cast<int>(branchList.size());
    numBranches = std::min(count, static_cast<int>(branches.size()));

    int i = 0;
    for (const auto& b : branchList)
    {
        if (i >= numBranches)
            break;
        branches[static_cast<size_t>(i)] = b;
        ++i;
    }

    int maxNode = -1;
    for (int k = 0; k < numBranches; ++k)
    {
        maxNode = std::max(maxNode, branches[static_cast<size_t>(k)].a);
        maxNode = std::max(maxNode, branches[static_cast<size_t>(k)].b);
    }
    numNodes_ = std::clamp(maxNode + 1, 0, kMaxNodes);
    outputNode_ = std::clamp(outputNode, 0, std::max(0, numNodes_ - 1));
    inputResistance_ = std::max(inputResistance, 1.0);
    reset();
}

void RcNetwork::prepare(double sampleRate)
{
    sampleRate_ = (sampleRate > 0.0) ? sampleRate : 44100.0;
    h_ = 1.0 / sampleRate_;
    reset();
}

void RcNetwork::setInputResistance(double ohms) noexcept
{
    inputResistance_ = std::max(ohms, 1.0);
}

void RcNetwork::setResistorValue(int branchIndex, double ohms) noexcept
{
    if (branchIndex < 0 || branchIndex >= numBranches)
        return;
    auto& b = branches[static_cast<size_t>(branchIndex)];
    if (b.type != Type::Resistor)
        return;
    b.value = std::max(ohms, 1.0e-3);
}

void RcNetwork::reset() noexcept
{
    capVPrev.fill(0.0);
    capIPrev.fill(0.0);
}

float RcNetwork::processSample(float input) noexcept
{
    if (numNodes_ == 0)
        return input;

    const int n = numNodes_;
    const double gIn = 1.0 / inputResistance_;  // Norton conductance of the source

    // Stamped system: G * v = rhs  (dense, small).
    std::array<double, kMaxNodes * kMaxNodes> A{};
    std::array<double, kMaxNodes> rhs{};

    // Thevenin source (u, Rth) as Norton: current u/Rth injected at node 0,
    // conductance 1/Rth from node 0 to ground.
    A[static_cast<size_t>(0) * kMaxNodes + 0] += gIn;
    rhs[0] += gIn * static_cast<double>(input);

    for (int k = 0; k < numBranches; ++k)
    {
        const auto& br = branches[static_cast<size_t>(k)];
        const int a = br.a;
        const int b = br.b;  // -1 = ground

        if (br.type == Type::Resistor)
        {
            const double g = 1.0 / std::max(br.value, 1.0e-6);
            if (a >= 0)
                A[static_cast<size_t>(a) * kMaxNodes + a] += g;
            if (b >= 0)
                A[static_cast<size_t>(b) * kMaxNodes + b] += g;
            if (a >= 0 && b >= 0)
            {
                A[static_cast<size_t>(a) * kMaxNodes + b] -= g;
                A[static_cast<size_t>(b) * kMaxNodes + a] -= g;
            }
        }
        else
        {
            // Trapezoidal capacitor companion: I = Gc*(Va - Vb) - Ieq with
            // Gc = 2C/h and Ieq = Gc*(Va_prev - Vb_prev) + I_prev.
            const double gc = 2.0 * std::max(br.value, 1.0e-15) / h_;
            const double ieq = gc * capVPrev[static_cast<size_t>(k)]
                             + capIPrev[static_cast<size_t>(k)];

            if (a >= 0)
                A[static_cast<size_t>(a) * kMaxNodes + a] += gc;
            if (b >= 0)
                A[static_cast<size_t>(b) * kMaxNodes + b] += gc;
            if (a >= 0 && b >= 0)
            {
                A[static_cast<size_t>(a) * kMaxNodes + b] -= gc;
                A[static_cast<size_t>(b) * kMaxNodes + a] -= gc;
            }
            // Move Ieq to the RHS: +Ieq at node a, -Ieq at node b.
            if (a >= 0)
                rhs[static_cast<size_t>(a)] += ieq;
            if (b >= 0)
                rhs[static_cast<size_t>(b)] -= ieq;
        }
    }

    std::array<double, kMaxNodes> v{};
    if (! solveLinearSystem(A.data(), rhs.data(), v.data()))
        return 0.0f;  // degenerate singular system: stay silent, never NaN

    // Update capacitor histories with the new voltages.
    for (int k = 0; k < numBranches; ++k)
    {
        const auto& br = branches[static_cast<size_t>(k)];
        if (br.type != Type::Capacitor)
            continue;

        const double va = (br.a >= 0)
            ? v[static_cast<size_t>(br.a)]
            : 0.0;
        const double vb = (br.b >= 0)
            ? v[static_cast<size_t>(br.b)]
            : 0.0;
        const double vc = va - vb;

        const double gc = 2.0 * std::max(br.value, 1.0e-15) / h_;
        const double ieq = gc * capVPrev[static_cast<size_t>(k)]
                         + capIPrev[static_cast<size_t>(k)];

        capVPrev[static_cast<size_t>(k)] = vc;
        capIPrev[static_cast<size_t>(k)] = gc * vc - ieq;
    }

    return static_cast<float>(v[static_cast<size_t>(outputNode_)]);
}

bool RcNetwork::solveLinearSystem(double* A, double* b, double* x) const noexcept
{
    const int n = numNodes_;
    if (n == 0)
        return true;

    // Gaussian elimination with partial pivoting (no heap allocations).
    for (int col = 0; col < n; ++col)
    {
        // Pivot
        int pivotRow = col;
        double pivotMag = std::abs(A[static_cast<size_t>(col) * kMaxNodes + col]);
        for (int row = col + 1; row < n; ++row)
        {
            const double mag = std::abs(A[static_cast<size_t>(row) * kMaxNodes + col]);
            if (mag > pivotMag)
            {
                pivotMag = mag;
                pivotRow = row;
            }
        }
        if (pivotMag < 1.0e-18)
            return false;  // singular

        if (pivotRow != col)
        {
            for (int c = col; c < n; ++c)
                std::swap(A[static_cast<size_t>(col) * kMaxNodes + c],
                          A[static_cast<size_t>(pivotRow) * kMaxNodes + c]);
            std::swap(b[col], b[pivotRow]);
        }

        const double pivot = A[static_cast<size_t>(col) * kMaxNodes + col];
        for (int row = col + 1; row < n; ++row)
        {
            const double factor = A[static_cast<size_t>(row) * kMaxNodes + col] / pivot;
            if (factor == 0.0)
                continue;
            for (int c = col; c < n; ++c)
                A[static_cast<size_t>(row) * kMaxNodes + c]
                    -= factor * A[static_cast<size_t>(col) * kMaxNodes + c];
            b[row] -= factor * b[col];
        }
    }

    // Back substitution
    for (int row = n - 1; row >= 0; --row)
    {
        double sum = b[row];
        for (int c = row + 1; c < n; ++c)
            sum -= A[static_cast<size_t>(row) * kMaxNodes + c] * x[c];
        x[row] = sum / A[static_cast<size_t>(row) * kMaxNodes + row];
    }
    return true;
}

} // namespace fairo

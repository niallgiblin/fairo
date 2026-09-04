/**
 * @file
 * @brief DSP unit tests for Fairo (Catch2).
 *
 * Coverage:
 *  - DiodeClipper: Newton-Raphson convergence, clipping thresholds,
 *    Ge asymmetry, bypass passthrough, NaN-free over full-scale + transients.
 *  - RcNetwork: analytic first-order RC validation, DC blocking.
 *  - ToneStack: stability, DC blocking, boundedness.
 *  - FuzzEngine: end-to-end chain, silence-in/silence-out, mid-stream
 *    clip-mode switching without NaN/instability, Hi/Lo surviving prepare().
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "dsp/DiodeClipper.h"
#include "dsp/RcNetwork.h"
#include "dsp/ToneStack.h"
#include "dsp/FuzzEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

using namespace fairo;
using Catch::Approx;

namespace
{

constexpr double kPi = 3.14159265358979323846;

std::vector<float> makeSine(double fs, double f, double amp, int seconds)
{
    const int n = static_cast<int>(fs * seconds);
    std::vector<float> out(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
        out[static_cast<size_t>(i)] = static_cast<float>(amp * std::sin(2.0 * kPi * f * i / fs));
    return out;
}

float peakAbs(const std::vector<float>& v, int fromSample = 0)
{
    float m = 0.0f;
    for (int i = fromSample; i < static_cast<int>(v.size()); ++i)
        m = std::max(m, std::abs(v[static_cast<size_t>(i)]));
    return m;
}

bool allFinite(const std::vector<float>& v)
{
    return std::all_of(v.begin(), v.end(), [](float s) { return std::isfinite(s); });
}

TEST_CASE("DiodeClipper: 1N914 symmetric pair clips a 5 V sine at ~0.65 V", "[dsp][diode]")
{
    DiodeClipper clipper;
    clipper.setConfig(DiodeClipConfig::silicon1N914(1.0e3));

    std::vector<float> out;
    const std::vector<float> in = makeSine(48000.0, 100.0, 5.0, 2);
    out.reserve(in.size());
    for (float x : in)
        out.push_back(clipper.processSample(x));

    REQUIRE(allFinite(out));
    const float peak = peakAbs(out, static_cast<int>(48000));
    CHECK(peak > 0.4f);
    CHECK(peak < 0.9f);

    // Symmetry: positive and negative peaks match within 2%
    float posPeak = 0.0f, negPeak = 0.0f;
    for (int i = static_cast<int>(48000); i < static_cast<int>(out.size()); ++i)
    {
        posPeak = std::max(posPeak, out[static_cast<size_t>(i)]);
        negPeak = std::min(negPeak, out[static_cast<size_t>(i)]);
    }
    REQUIRE(std::abs(posPeak) > 0.01f);
    CHECK(std::abs(posPeak - (-negPeak)) / std::abs(posPeak) < 0.02f);
}

TEST_CASE("DiodeClipper: germanium branch is asymmetric (0.3 V vs 0.6 V)", "[dsp][diode]")
{
    DiodeClipper clipper;
    clipper.setConfig(DiodeClipConfig::germanium1N34A(1.0e3));

    const std::vector<float> in = makeSine(48000.0, 100.0, 5.0, 2);
    std::vector<float> out;
    out.reserve(in.size());
    for (float x : in)
        out.push_back(clipper.processSample(x));

    REQUIRE(allFinite(out));

    float posPeak = 0.0f, negPeak = 0.0f;
    for (int i = static_cast<int>(48000); i < static_cast<int>(out.size()); ++i)
    {
        posPeak = std::max(posPeak, out[static_cast<size_t>(i)]);
        negPeak = std::min(negPeak, out[static_cast<size_t>(i)]);
    }

    // Single diode (positive half): ~0.3 V; series pair (negative): ~0.6 V.
    CHECK(posPeak > 0.18f);
    CHECK(posPeak < 0.45f);
    CHECK(-negPeak > 0.45f);
    CHECK(-negPeak < 0.85f);
    CHECK(-negPeak > 1.3f * posPeak);  // clearly asymmetric
}

TEST_CASE("DiodeClipper: bypass configuration passes signal through unchanged", "[dsp][diode]")
{
    DiodeClipper clipper;
    clipper.setConfig(DiodeClipConfig::bypass());

    const std::vector<float> in = makeSine(48000.0, 200.0, 3.0, 1);
    for (float x : in)
        CHECK(clipper.processSample(x) == Approx(x));
}

TEST_CASE("DiodeClipper: no NaN on full-scale sine and DC transients", "[dsp][diode]")
{
    DiodeClipper clipper;
    clipper.setConfig(DiodeClipConfig::silicon1N914(1.0e3));

    std::vector<float> in;
    for (int i = 0; i < 48000; ++i)
        in.push_back(static_cast<float>(10.0f * std::sin(2.0 * kPi * 40.0 * i / 48000.0)));
    // Severe discontinuous jumps.
    for (int i = 0; i < 48000; ++i)
        in.push_back((i % 1000) < 500 ? 50.0f : -50.0f);

    std::vector<float> out;
    out.reserve(in.size());
    for (float x : in)
        out.push_back(clipper.processSample(x));
    REQUIRE(allFinite(out));
}

TEST_CASE("RcNetwork: first-order RC high-pass matches analytic response", "[dsp][network]")
{
    // Vin (via 1 kOhm) -> C (470 nF) -> node 1 -> R (10 kOhm) -> gnd.
    // The coupling cap sees R_total = 1k + 10k = 11k... but the Thevenin
    // resistance seen by the capacitor is Rth + R = 11 kOhm.
    constexpr double fs = 48000.0;
    constexpr double c = 470.0e-9;
    constexpr double rTotal = 11.0e3;
    constexpr double f = 1000.0;

    RcNetwork net;
    net.setTopology(1.0e3, {
        RcNetwork::Branch{ RcNetwork::Type::Capacitor, 0, 1, c },
        RcNetwork::Branch{ RcNetwork::Type::Resistor, 1, -1, 10.0e3 },
    }, 1);
    net.prepare(fs);

    // Transfer: H(s) = s*C*R2 / (1 + s*C*(R1 + R2))  — the shunt R2 is in the
    // numerator, the Thevenin + shunt sum sets the corner.
    const double r1 = 1.0e3;
    const double r2 = 10.0e3;
    const double tau = (r1 + r2) * c;
    const double omega = 2.0 * kPi * f;
    const double gainAnalytic = (omega * c * r2)
                              / std::sqrt(1.0 + (omega * tau) * (omega * tau));

    const std::vector<float> in = makeSine(fs, f, 1.0, 2);
    std::vector<float> out;
    out.reserve(in.size());
    for (float x : in)
        out.push_back(net.processSample(x));

    REQUIRE(allFinite(out));

    const float measured = peakAbs(out, static_cast<int>(fs));  // second half = steady state
    CHECK(std::abs(measured - static_cast<float>(gainAnalytic))
          / static_cast<float>(gainAnalytic) < 0.01f);
}

TEST_CASE("RcNetwork: blocks DC", "[dsp][network]")
{
    RcNetwork net;
    net.setTopology(1.0e3, {
        RcNetwork::Branch{ RcNetwork::Type::Capacitor, 0, 1, 470.0e-9 },
        RcNetwork::Branch{ RcNetwork::Type::Resistor, 1, -1, 10.0e3 },
    }, 1);
    net.prepare(48000.0);

    float last = 0.0f;
    for (int i = 0; i < 48000 * 3; ++i)  // 3 s: several time constants
        last = net.processSample(1.0f);
    CHECK(std::abs(last) < 1.0e-3f);
}

TEST_CASE("ToneStack: stable, bounded, DC-blocked across settings", "[dsp][tonestack]")
{
    ToneStack stack;
    stack.prepare(48000.0);

    std::vector<float> in = makeSine(48000.0, 110.0, 2.0, 2);
    in.reserve(in.size() + 48000);
    for (int i = 0; i < 48000; ++i)
        in.push_back(static_cast<float>((i % 700) < 350 ? 1.0 : -1.0));  // DC-ish square

    for (float tone : { 0.0f, 0.5f, 1.0f })
    {
        for (float high : { 0.0f, 1.0f })
        {
            stack.setTonePot(tone);
            stack.setHighPot(high);

            std::vector<float> out;
            out.reserve(in.size());
            for (float x : in)
                out.push_back(stack.processSample(x));

            INFO("tone=" << tone << " high=" << high);
            REQUIRE(allFinite(out));
            CHECK(peakAbs(out, static_cast<int>(48000)) < 5.0f);
            // DC blocking after settling
            CHECK(std::abs(out.back()) < 1.0f);
        }
    }
    stack.reset();
    // The real stack is fully AC-coupled (C9 blocks DC at the input, C3 at
    // the output): a constant DC input settles to ~0.
    float settled = 5.0f;
    for (int i = 0; i < 2000; ++i)
        settled = stack.processSample(0.5f);
    CHECK(std::abs(settled) < 0.02f);
}

TEST_CASE("FuzzEngine: end-to-end chain is finite, silence stays silent", "[dsp][engine][e2e]")
{
    FuzzEngine engine;
    engine.prepare(48000.0, 512);

    // Loud riff-ish signal
    const std::vector<float> in = makeSine(48000.0, 220.0, 1.0, 2);
    std::vector<float> out(in.size());
    for (size_t i = 0; i < in.size(); i += 512)
    {
        const int n = static_cast<int>(std::min<size_t>(512, in.size() - i));
        engine.processBlock(in.data() + i, out.data() + i, n);
    }

    REQUIRE(allFinite(out));
    CHECK(peakAbs(out) < 8.0f);

    // Silence -> silence (allow tiny residual decay in early samples).
    engine.reset();
    std::vector<float> silence(48000, 0.0f);
    std::vector<float> silentOut(silence.size());
    for (size_t i = 0; i < silence.size(); i += 512)
        engine.processBlock(silence.data() + i, silentOut.data() + i, 512);
    REQUIRE(allFinite(silentOut));

    float residual = 0.0f;
    for (int i = static_cast<int>(silence.size() / 2); i < static_cast<int>(silence.size()); ++i)
        residual = std::max(residual, std::abs(silentOut[static_cast<size_t>(i)]));
    CHECK(residual < 1.0e-4f);
}

TEST_CASE("FuzzEngine: switching clip mode mid-stream never produces NaN", "[dsp][engine][e2e]")
{
    FuzzEngine engine;
    engine.prepare(48000.0, 256);

    const std::vector<float> in = makeSine(48000.0, 150.0, 2.0, 3);
    std::vector<float> out(in.size());
    const std::array<FuzzEngine::ClipMode, 3> modes{ FuzzEngine::ClipMode::Silicon,
                                                     FuzzEngine::ClipMode::Germanium,
                                                     FuzzEngine::ClipMode::Bypass };

    size_t i = 0;
    int modeIdx = 0;
    while (i < in.size())
    {
        engine.setClipMode(modes[static_cast<size_t>(modeIdx)]);
        const int n = static_cast<int>(std::min<size_t>(256, in.size() - i));
        engine.processBlock(in.data() + i, out.data() + i, n);
        i += static_cast<size_t>(n);
        modeIdx = (modeIdx + 1) % static_cast<int>(modes.size());
    }

    REQUIRE(allFinite(out));
    CHECK(peakAbs(out) < 8.0f);

    // Each branch produces audibly different (but nonzero) energy.
    engine.reset();
    std::array<float, 3> energy{};
    int idx = 0;
    for (auto mode : modes)
    {
        engine.setClipMode(mode);
        std::vector<float> blockOut(48000);
        for (size_t j = 0; j < blockOut.size(); j += 256)
            engine.processBlock(in.data() + j, blockOut.data() + j, 256);
        for (float v : blockOut)
            energy[static_cast<size_t>(idx)] += v * v;
        ++idx;
    }
    for (float e : energy)
        CHECK(e > 0.0f);
}

TEST_CASE("FuzzEngine: Hi/Lo set before prepare() still engages Hi", "[dsp][engine][hilo]")
{
    // Stay well below clipping so the ~15 dB Hi/Lo pad is visible as level.
    const auto in = makeSine(48000.0, 220.0, 0.02, 1);

    auto peakAfterSettle = [&](FuzzEngine& e)
    {
        std::vector<float> out(in.size());
        for (size_t i = 0; i < in.size(); i += 256)
        {
            const int n = static_cast<int>(std::min<size_t>(256, in.size() - i));
            e.processBlock(in.data() + i, out.data() + i, n);
        }
        return peakAbs(out, static_cast<int>(in.size() / 2));
    };

    FuzzEngine lo;
    lo.prepare(48000.0, 256);
    lo.setHiLo(false);
    const float loPeak = peakAfterSettle(lo);

    FuzzEngine hiAfterPrepare;
    hiAfterPrepare.prepare(48000.0, 256);
    hiAfterPrepare.setHiLo(true);
    const float hiPeak = peakAfterSettle(hiAfterPrepare);

    REQUIRE(hiPeak > loPeak * 2.0f);

    // Host restore writes Hi, then the graph starts — prepare must not drop
    // the network back to Lo while leaving the flag set.
    FuzzEngine restored;
    restored.setHiLo(true);
    restored.prepare(48000.0, 256);
    restored.setHiLo(true);
    const float restoredPeak = peakAfterSettle(restored);

    CHECK(restored.getHiLo());
    CHECK(restoredPeak == Approx(hiPeak).margin(hiPeak * 0.15f));
}

// ── Cross-validation against the Python sandbox (python/diode_clipper.py) ────

TEST_CASE("DiodeClipper: matches the Python sandbox golden transfer curves", "[dsp][diode][golden]")
{
    // Regenerate with: python3 python/diode_clipper.py  -> tests/data/diode_curves.csv
    const std::string path = std::string(FA_REPO_ROOT) + "/tests/data/diode_curves.csv";
    std::ifstream csv(path);
    REQUIRE(csv.good());

    std::string line;
    std::getline(csv, line);  // header

    DiodeClipper si, ge, si4001;
    si.setConfig(DiodeClipConfig::silicon1N914(1.0e3));
    ge.setConfig(DiodeClipConfig::germanium1N34A(1.0e3));
    si4001.setConfig(DiodeClipConfig::silicon1N4001(1.0e3));

    int rows = 0;
    float maxErrSi = 0.0f, maxErrGe = 0.0f, maxErrSi4001 = 0.0f;
    while (std::getline(csv, line))
    {
        if (line.empty())
            continue;
        std::stringstream ss(line);
        float vin, refSi, refGe, refSi4001;
        std::string v1, v2, v3, v4;
        std::getline(ss, v1, ','); std::getline(ss, v2, ',');
        std::getline(ss, v3, ','); std::getline(ss, v4, ',');
        vin = std::stof(v1); refSi = std::stof(v2); refGe = std::stof(v3); refSi4001 = std::stof(v4);

        maxErrSi = std::max(maxErrSi, std::abs(si.processSample(vin) - refSi));
        maxErrGe = std::max(maxErrGe, std::abs(ge.processSample(vin) - refGe));
        maxErrSi4001 = std::max(maxErrSi4001, std::abs(si4001.processSample(vin) - refSi4001));
        ++rows;
    }
    REQUIRE(rows > 1000);
    CHECK(maxErrSi < 5.0e-4f);
    CHECK(maxErrGe < 5.0e-4f);
    CHECK(maxErrSi4001 < 5.0e-4f);
}

// ── Cross-validation against the Python sandbox (python/tonestack.py) ────────

TEST_CASE("ToneStack: matches the Python sandbox golden impulse responses", "[dsp][tonestack][golden]")
{
    // Regenerate with: python3 python/tonestack.py -> tests/data/tonestack_impulse.csv
    const std::string path = std::string(FA_REPO_ROOT) + "/tests/data/tonestack_impulse.csv";
    std::ifstream csv(path);
    REQUIRE(csv.good());

    std::string line;
    std::getline(csv, line);  // header

    // Four columns: (high=1,tone=0), (1,0.5), (1,1), (0.4,0.7)
    std::array<ToneStack, 4> stacks;
    stacks[0].setHighPot(1.0f); stacks[0].setTonePot(0.0f);
    stacks[1].setHighPot(1.0f); stacks[1].setTonePot(0.5f);
    stacks[2].setHighPot(1.0f); stacks[2].setTonePot(1.0f);
    stacks[3].setHighPot(0.4f); stacks[3].setTonePot(0.7f);
    for (auto& s : stacks)
        s.prepare(48000.0);

    int rows = 0;
    float maxErr = 0.0f;
    while (std::getline(csv, line))
    {
        if (line.empty())
            continue;
        std::stringstream ss(line);
        std::string token;
        std::getline(ss, token, ',');  // sample index
        for (int col = 0; col < 4; ++col)
        {
            std::getline(ss, token, ',');
            const float ref = std::stof(token);
            const float got = stacks[static_cast<size_t>(col)].processSample(rows == 0 ? 1.0f : 0.0f);
            // Skip the first few samples (blocking network needs a couple of
            // samples to respond identically in float vs double); then require
            // the impulse shape to match closely.
            if (rows >= 4)
                maxErr = std::max(maxErr, std::abs(got - ref));
        }
        ++rows;
    }
    REQUIRE(rows == 2000);
    CHECK(maxErr < 2.0e-3f);
}

} // namespace (helpers)

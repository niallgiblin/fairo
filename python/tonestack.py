#!/usr/bin/env python3
"""
Fairo offline sandbox — coupled Tone/High tonestack.

Implements the SAME netlist as src/dsp/ToneStack.cpp (MNA + trapezoidal
integration, identical math to src/dsp/RcNetwork.cpp) and exports reference
data for the C++ unit tests:

  tests/data/tonestack_impulse.csv  — impulse responses at several pot settings
  research/tonestack_response.png   — frequency-response family plot

Run:  python3 python/tonestack.py
"""

import csv
import math
import os
import sys

import numpy as np

# ── Component values (keep in sync with CircuitValues.h) ──
C9 = 10.0e-9       # tonestack HP coupling cap
R5 = 470.0e3       # tonestack HP resistor
R_HIGH_MAX = 25.0e3   # R8 HIGH pot
C8 = 22.0e-9       # tonestack LP cap
R_TONE_MAX = 250.0e3  # R25 TONE pot
C3 = 47.0e-9       # tonestack wiper cap to Q1 base
R7 = 470.0e3       # Q1 base bias
R_SRC = 100.0e3    # Q2 collector load (R10), Thevenin source of the stack
FS = 48000.0


def make_network(r_high, tone):
    """Netlist identical to ToneStack::rebuild(). Returns branches.
    Branch format: (type, a, b, value); -1 = ground. Nodes:
    0 = input, 1 = A, 2 = B, 3 = W (tone wiper), 4 = OUT.
    r_high = R8 resistance: more = weaker LP = brighter."""
    r_high = max(r_high, 1.0)  # never fully open-circuit
    t = float(tone)
    # t=1 -> wiper toward node A (bright); t=0 -> node B (dark). (FIX: swapped
    # from the original so the knob reads like the real Pharaoh, where tone up =
    # brighter, not muddy.)
    r_seg_a = max((1.0 - t) * R_TONE_MAX, 1.0)
    r_seg_b = max(t * R_TONE_MAX, 1.0)
    return [
        ("C", 0, 1, C9),        # C9 -> node A
        ("R", 1, -1, R5),       # R5 470k to ground
        ("R", 1, 2, r_high),    # R8 HIGH rheostat A->B
        ("C", 2, -1, C8),       # C8 LP to ground
        ("R", 1, 3, r_seg_a),   # TONE lug1 -> wiper
        ("R", 2, 3, r_seg_b),   # TONE lug3 -> wiper
        ("C", 3, 4, C3),        # C3 -> Q1 base
        ("R", 4, -1, R7),       # Q1 bias leak
    ], R_SRC


def solve(branches, r_in, u, cap_v, cap_i):
    """One sample of the network, mirroring RcNetwork::processSample."""
    n = 5  # nodes 0..4
    A = np.zeros((n, n))
    rhs = np.zeros(n)
    h = 1.0 / FS

    g_in = 1.0 / max(r_in, 1.0)
    A[0, 0] += g_in
    rhs[0] += g_in * u

    for idx, (typ, a, b, val) in enumerate(branches):
        if typ == "R":
            g = 1.0 / max(val, 1.0e-6)
            if a >= 0:
                A[a, a] += g
            if b >= 0:
                A[b, b] += g
            if a >= 0 and b >= 0:
                A[a, b] -= g
                A[b, a] -= g
        else:
            gc = 2.0 * max(val, 1.0e-15) / h
            ieq = gc * cap_v[idx] + cap_i[idx]
            if a >= 0:
                A[a, a] += gc
            if b >= 0:
                A[b, b] += gc
            if a >= 0 and b >= 0:
                A[a, b] -= gc
                A[b, a] -= gc
            if a >= 0:
                rhs[a] += ieq
            if b >= 0:
                rhs[b] -= ieq

    v = np.linalg.solve(A, rhs)
    new_cv = cap_v.copy()
    new_ci = cap_i.copy()
    for idx, (typ, a, b, val) in enumerate(branches):
        if typ != "C":
            continue
        va = v[a] if a >= 0 else 0.0
        vb = v[b] if b >= 0 else 0.0
        vc = va - vb
        gc = 2.0 * max(val, 1.0e-15) * FS
        ieq = gc * cap_v[idx] + cap_i[idx]
        new_cv[idx] = vc
        new_ci[idx] = gc * vc - ieq
    return v[4], new_cv, new_ci


def freq_response(branches, r_in, freqs):
    """Magnitude (dB) via steady-state sine with the discrete model."""
    mags = []
    for f in freqs:
        n = int(0.2 * FS)
        steps = int(0.05 * FS)
        cap_v = np.zeros(len(branches))
        cap_i = np.zeros(len(branches))
        ys = np.zeros(steps)
        for i in range(n):
            u = math.sin(2.0 * math.pi * f * i / FS)
            y, cap_v, cap_i = solve(branches, r_in, u, cap_v, cap_i)
            if i >= n - steps:
                ys[i - (n - steps)] = y
        mags.append(20.0 * math.log10(max(np.max(np.abs(ys)), 1e-9)))
    return mags


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_dir = os.path.join(root, "tests", "data")
    os.makedirs(data_dir, exist_ok=True)

    freqs = np.logspace(1, 4.5, 48)  # 10 Hz .. ~31.6 kHz

    print("Frequency response (dB) at 100 Hz / 1 kHz / 10 kHz:")
    settings = [(1.0, 0.0), (1.0, 0.5), (1.0, 1.0), (0.2, 0.5)]
    for high, tone in settings:
        branches, r_in = make_network(R_HIGH_MAX * high + 1.0, tone)
        mags = freq_response(branches, r_in, freqs)
        picks = [mags[i] for i in (7, 16, 30)]  # ~100 Hz, ~1 kHz, ~10 kHz
        print(f"  high={high:.1f} tone={tone:.1f}: 100Hz={picks[0]:6.1f} dB  "
              f"1kHz={picks[1]:6.1f} dB  10kHz={picks[2]:6.1f} dB")

    # Export impulse responses for the C++ test.
    with open(os.path.join(data_dir, "tonestack_impulse.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["sample", "high1_tone0", "high1_tone0.5", "high1_tone1", "high0.4_tone0.7"])
        best = {}
        for high, tone in [(1.0, 0.0), (1.0, 0.5), (1.0, 1.0), (0.4, 0.7)]:
            branches, r_in = make_network(R_HIGH_MAX * high + 1.0, tone)
            cap_v = np.zeros(len(branches))
            cap_i = np.zeros(len(branches))
            imp = []
            for n_ in range(2000):
                u = 1.0 if n_ == 0 else 0.0
                y, cap_v, cap_i = solve(branches, r_in, u, cap_v, cap_i)
                imp.append(round(y, 9))
            best[(high, tone)] = imp
        for i in range(2000):
            w.writerow([i] + [str(best[k][i]) for k in [(1.0, 0.0), (1.0, 0.5), (1.0, 1.0), (0.4, 0.7)]])

    print(f"wrote {os.path.join(data_dir, 'tonestack_impulse.csv')}")

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig, ax = plt.subplots()
        for high, tone in settings:
            branches, r_in = make_network(R_HIGH_MAX * high + 1.0, tone)
            mags = freq_response(branches, r_in, freqs)
            ax.semilogx(freqs, mags, label=f"high={high}, tone={tone}")
        ax.set_xlabel("Hz")
        ax.set_ylabel("dB")
        ax.set_title("Fairo coupled Tone/High stack (MNA + trapezoidal)")
        ax.legend()
        ax.grid(alpha=0.3)
        out = os.path.join(root, "research", "tonestack_response.png")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        fig.savefig(out, dpi=120)
        print(f"wrote {out}")
    except ImportError:
        print("matplotlib not installed; skipping plot")


if __name__ == "__main__":
    sys.exit(main())

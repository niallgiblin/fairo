#!/usr/bin/env python3
"""
Fairo offline sandbox — diode clipper reference.

Mirrors src/dsp/DiodeClipper.cpp exactly (Shockley + Newton-Raphson, warm
start) so the C++ build can be cross-validated against this reference:
   1. plots the input->output transfer curves for all three branch types,
   2. writes tests/data/diode_curves.csv (input, si, ge, bypass outputs).

Run:  python3 python/diode_clipper.py
"""

import csv
import math
import os
import sys

# ── Device parameters (keep in sync with CircuitValues.h / DiodeClipper.cpp) ──
VT = 0.02585
SIL = {"is": 2.52e-9, "n": 1.752}   # 1N914
SI4 = {"is": 14.11e-9, "n": 1.984}  # 1N4001
GE = {"is": 2.0e-6, "n": 1.5}       # 1N34A (tentative)


def solve_node(vin, vx, f, b, is_, n, r, max_iter=18):
    """Two-phase solve (6 Newton -> 7 forced bisection -> 5 Newton polish).

    Mirrors src/dsp/DiodeClipper.cpp exactly. f(Vx) is strictly decreasing so
    sign-bracketing is always valid; forced bisection guarantees progress in
    the hard-exponential region where Newton crawls at ~n*Vt per iteration.
    """
    if f == 0 and b == 0:
        return vin
    if not math.isfinite(vx) or abs(vx) > 0.9:
        vx = max(-0.9, min(0.9, vin))
    inv_r = 1.0 / r
    fu = max(1, f) * n * VT
    bu = max(1, b) * n * VT

    bkt = max(1.0, max(abs(vin), abs(vx))) * 2.0 + 5.0
    lo, hi = -bkt, bkt
    vx = max(lo, min(hi, vx))
    exp_limit = 680.0

    for it in range(max_iter):
        xf = max(-exp_limit, min(exp_limit, vx / fu))
        xr = max(-exp_limit, min(exp_limit, -vx / bu))
        ef, er = math.exp(xf), math.exp(xr)
        res = (vin - vx) * inv_r - is_ * (ef - er)
        deriv = -inv_r - (is_ / fu) * ef - (is_ / bu) * er

        if res > 0.0:
            lo = vx
        else:
            hi = vx

        if abs(res) < 1e-9:
            break

        if it < 6 or it >= 13:
            if deriv == 0.0:
                nxt = 0.5 * (lo + hi)
            else:
                nxt = vx - res / deriv
                if not math.isfinite(nxt) or nxt <= lo or nxt >= hi:
                    nxt = 0.5 * (lo + hi)
        else:
            nxt = 0.5 * (lo + hi)

        if abs(nxt - vx) < 1e-12:
            vx = nxt
            break
        vx = nxt
    return vx


def branch_curve(vin, f, b, is_, n, r):
    """Memorised transfer curve with warm-started solve."""
    vx = 0.0
    vals = []
    for v in vin:
        vx = solve_node(v, vx, f, b, is_, n, r)
        vals.append(round(vx, 6))
    return vals


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_dir = os.path.join(root, "tests", "data")
    os.makedirs(data_dir, exist_ok=True)

    vin = [-10.0 + 20.0 * i / 2000.0 for i in range(2001)]

    labels = {
        "si": ("1N914 pair", 1, 1, SIL["is"], SIL["n"], 1.0e3),
        "ge": ("1N34A asym", 1, 2, GE["is"], GE["n"], 1.0e3),
        "si4001": ("1N4001 pair", 1, 1, SI4["is"], SI4["n"], 1.0e3),
    }

    curves = {k: branch_curve(vin, *v[1:]) for k, v in labels.items()}

    # Report clip asymptotes
    for k, v in labels.items():
        pos = max(curves[k])
        neg = min(curves[k])
        print(f"{k:8s} {v[0]:14s}  pos clip ~{pos:+.3f} V   neg clip ~{neg:+.3f} V")

    with open(os.path.join(data_dir, "diode_curves.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["vin", "si", "ge", "si4001"])
        for i, v in enumerate(vin):
            w.writerow([f"{v:.6f}"] + [str(curves[k][i]) for k in ("si", "ge", "si4001")])
    print(f"wrote {os.path.join(data_dir, 'diode_curves.csv')}")

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        fig, ax = plt.subplots()
        for k, v in labels.items():
            ax.plot(vin, curves[k], label=f"{k} ({v[0]})")
        ax.set_xlabel("input (V)")
        ax.set_ylabel("output (V)")
        ax.set_title("Fairo diode clipper transfer curves (Shockley + Newton-Raphson)")
        ax.legend()
        ax.grid(alpha=0.3)
        out = os.path.join(root, "research", "diode_curves.png")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        fig.savefig(out, dpi=120)
        print(f"wrote {out}")
    except ImportError:
        print("matplotlib not installed; skipping plot")


if __name__ == "__main__":
    sys.exit(main())

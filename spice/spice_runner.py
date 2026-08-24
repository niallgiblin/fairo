#!/usr/bin/env python3
"""Run an ngspice deck (batch) and return the parsed binary raw output.

Parses ngspice binary rawfiles (real plots only) into numpy arrays keyed by
variable name. Used by the Fairo Phase 3 pipeline:
  - validation (sine sweeps, DC op points)
  - training-corpus generation (DI guitar -> SPICE -> paired wet audio)
"""

import os
import re
import subprocess
import tempfile

import numpy as np

# Header forms seen from ngspice-47. A rawfile header looks like:
#   Title: pharaoh
#   Date: ...
#   Plotname: Transient Analysis
#   Flags: real
#   No. Variables: 3
#   No. Points: 441
#   Variables:
#   	0	v(in)	voltage
#   	1	v(out)	voltage
#   Values: binary
# (binary doubles follow; interleaved per point for real plots)


def parse_raw(path: str):
    with open(path, "rb") as fh:
        content = fh.read()

    marker = content.find(b"Binary:\n")
    if marker < 0:
        marker = content.find(b"Values: binary\n")
    if marker < 0:
        raise ValueError("no binary data marker in rawfile")
    head = content[:marker]
    text = head.decode("ascii", errors="replace")
    n_vars = int(re.search(r"No. Variables:\s+(\d+)", text).group(1))
    n_points = int(re.search(r"No. Points:\s+(\d+)", text).group(1))
    names = [
        m.group(1)
        for m in re.finditer(r"^\s*\d+\s+(\S+)\s+", text, flags=re.M)
    ]

    raw = np.frombuffer(content, dtype=np.float64, offset=marker + len(b"Binary:\n"))
    n_total = raw.size - (raw.size % (n_vars * n_points))
    if n_total < n_vars * n_points:
        raise ValueError(f"raw data too short: {raw.size} vs {n_vars * n_points}")
    data = raw[: n_vars * n_points].reshape(n_points, n_vars)  # real plots: interleaved
    return {name: data[:, i] for i, name in enumerate(names)}, n_points


def run_deck(deck: str, workdir: str | None = None, timeout=600):
    """Write deck to a temp file, run ngspice -b, return (raw_data, ngspice_log)."""
    with tempfile.TemporaryDirectory() as td:
        cir = os.path.join(td, "pharaoh.cir")
        raw = os.path.join(td, "pharaoh.raw")
        with open(cir, "w") as fh:
            fh.write(deck)
        proc = subprocess.run(
            ["ngspice", "-b", "-r", raw, cir],
            capture_output=True, text=True, timeout=timeout,
            cwd=workdir or td,
        )
        log = proc.stdout + proc.stderr
        if proc.returncode != 0:
            raise RuntimeError(f"ngspice failed ({proc.returncode}):\n{log[-3000:]}")
        if not os.path.exists(raw):
            raise RuntimeError(f"no raw output:\n{log[-3000:]}")
        data, _ = parse_raw(raw)
        return data, log


if __name__ == "__main__":
    from pharaoh_netlist import build_netlist
    deck = build_netlist(mode="silicon", amp=1.0, freq=1000.0, tfinal=0.05)
    data, log = run_deck(deck)
    print("vars:", list(data.keys()))
    out = data.get("v(out)", data[list(data.keys())[-1]])
    print(f"output: n={out.size} min={out.min():.4f} max={out.max():.4f} rms={np.sqrt(np.mean(out**2)):.4f}")

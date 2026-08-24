#!/usr/bin/env python3
"""
Phase 3 data generation: DI guitar audio -> ngspice Pharaoh simulation ->
paired (input, output) training data, at randomized/swept control settings
across all 6 Hi/Lo x Si/Ge/Bypass branches.

Source material: the author's own DI guitar recordings (fuzzyband project,
data/raw: single_note/open_chord/palm_mute/sustain, 44.1 kHz mono 24-bit) —
no external audio, so no licensing concerns (PLAN.md Phase 3).

Outputs (training/data/):
  fairo_<mode>.npz  with keys:
    x        float32 (N,) dry input (volts)
    y        float32 (N,) wet SPICE output (volts)
    meta     list of per-clip dicts (source file, settings, offsets)
"""

import concurrent.futures
import glob
import math
import os
import random
import subprocess
import sys
import tempfile
import time
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "spice"))

from pharaoh_netlist import build_netlist  # noqa: E402
from spice_runner import parse_raw  # noqa: E402

SR = 44100
CLIP_LEN = 12.0          # seconds of training audio per clip
PREROLL = 2.5            # seconds simulated before the clip (settling)
DI_DIRS = [
    os.path.expanduser("~/Desktop/fuzzyband/data/raw/single_note"),
    os.path.expanduser("~/Desktop/fuzzyband/data/raw/open_chord"),
    os.path.expanduser("~/Desktop/fuzzyband/data/raw/palm_mute"),
    os.path.expanduser("~/Desktop/fuzzyband/data/raw/sustain"),
]


def list_sources():
    files = []
    for d in DI_DIRS:
        files.extend(sorted(glob.glob(os.path.join(d, "*.wav"))))
    return files


def load_wav(path):
    with wave.open(path) as w:
        sr = w.getframerate()
        ch = w.getnchannels()
        n = w.getnframes()
        raw = w.readframes(n)
        width = w.getsampwidth()
        if width == 3:  # 24-bit
            data = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
            vals = (data[:, 0].astype(np.int32)
                    | (data[:, 1].astype(np.int32) << 8)
                    | (data[:, 2].astype(np.int32) << 16))
            vals = np.where(vals & 0x800000, vals - 0x1000000, vals)
            x = vals.astype(np.float32) / 8388608.0
        else:
            raise ValueError(f"unsupported width {width}")
        x = x.reshape(-1, ch)
        x = x.mean(axis=1) if ch > 1 else x[:, 0]
        if sr != SR:
            raise ValueError(f"unexpected sr {sr}")
        return x


def settings_sample(rng):
    return {
        "mode": rng.choice(["silicon", "germanium", "bypass"]),
        "fuzz": float(rng.uniform(0.15, 1.0)),
        # Sampling bounds dodge a ngspice timestep-collapse corner
        # (high ~0 with tone high) - measured on this circuit.
        # Sampling bounds dodge a ngspice timestep-collapse corner (tone high
        # with high ~0 on this circuit).
        "tone": float(rng.uniform(0.05, 0.95)),
        "high": float(rng.uniform(0.15, 0.98)),
        "hilo": int(rng.random() < 0.5),   # 0 = Hi (39k), 1 = Lo (390k)
        "vol": 0.5,                        # fixed; the plugin volume knob applies after
        "peak_in": float(rng.uniform(0.12, 0.35)),  # guitar-ish peak level
    }


def run_one(args):
    """Simulate one clip; returns (x, y, settings). Timeout/slow clips are
    retried once with benign settings, then dropped."""
    src_path, s = args
    outs = _run_one(src_path, s)
    if outs is None:
        s2 = dict(s)
        s2["fuzz"], s2["tone"], s2["high"] = 0.8, 0.5, 0.6
        outs = _run_one(src_path, s2)
    return outs


def _run_one(src_path, s):
    rng = random.Random(hash((src_path, repr(s))) & 0xFFFFFFFF)

    x_all = load_wav(src_path)
    total = int((CLIP_LEN + PREROLL) * SR)
    if x_all.size < total:
        return None

    start = rng.randint(0, x_all.size - total)
    seg = x_all[start:start + total].astype(np.float64)
    # Peak-normalize to the sampled guitar level (keeps the model's dynamics).
    peak = float(np.abs(seg).max())
    if peak < 1e-6:
        return None
    seg *= s["peak_in"] / peak

    # Feed the whole segment (pre-roll included) through the sim.
    # This ngspice build has no PWL-file support, so the DI recording is
    # embedded as an inline PWL. Points are 2x-decimated (pair-averaged, a
    # (1+z^-1)/2 anti-alias filter) to keep the deck parse fast; the linear
    # interpolation between points then reconstructs a clean input.
    from scipy.signal import butter, sosfilt  # noqa: E402
    # 11.025 kHz feed: 4x decimated with (1+z^-1)^2-style averaging and a
    # ~5.5 kHz guard LPF so ngspice's linear interpolation cannot alias.
    sos = butter(4, 0.21, output="sos")  # ~5 kHz guard for the 11.025k feed
    seg_f = sosfilt(sos, seg)
    avg4 = 0.25 * (seg_f[0:-3:4] + seg_f[1:-2:4] + seg_f[2:-1:4] + seg_f[3::4])
    seg_feed = avg4
    tfinal = total / SR
    deck = build_netlist(mode=s["mode"], fuzz=s["fuzz"], tone=s["tone"],
                         high=s["high"], vol=s["vol"], hilo=s["hilo"],
                         tfinal=tfinal, tstep=1.0 / SR, amp=s["peak_in"], freq=1000.0)
    pwl = " ".join(f"{4 * i / SR:.6f} {v:.6f}" for i, v in enumerate(seg_feed))
    import re as _re
    deck = _re.sub(r"VIN in 0[^\n]*", f"VIN in 0 PWL({pwl})", deck)

    cir_path = os.path.join(tempfile.gettempdir(), f"fairo_cir_{rng.randint(0, 10**8)}.cir")
    raw_path = cir_path.replace(".cir", ".raw")
    with open(cir_path, "w") as fh:
        fh.write(deck)

    try:
        proc = subprocess.run(["ngspice", "-b", "-r", raw_path, cir_path],
                              capture_output=True, text=True, timeout=240)
        if proc.returncode != 0:
            raise RuntimeError(f"ngspice rc={proc.returncode}: {proc.stdout[-500:]}")
        data, _ = parse_raw(raw_path)
        # The rawfile's samples sit on ngspice's *adaptive* internal grid;
        # resample v(out) onto the uniform 44.1 kHz grid so x and y align.
        t_raw = data["time"]
        y_raw = data["v(out)"].astype(np.float64)
        t_uniform = np.arange(0.0, tfinal, 1.0 / SR)
        y = np.interp(t_uniform, t_raw, y_raw)
        n = len(y)
        x = seg[:n]
        # Drop the pre-roll, and trim to the common length (ngspice emits one
        # extra point in some runs).
        x = x[int(PREROLL * SR):]
        y = y[int(PREROLL * SR):]
        n_use = min(len(x), len(y))
        x = x[:n_use]
        y = y[:n_use]
        return (x.astype(np.float32), y.astype(np.float32), s, os.path.basename(src_path))
    except Exception as exc:
        if os.environ.get("FAIRO_DEBUG"):
            import traceback
            traceback.print_exc()
        return None  # never kill the pool; caller skips the clip
    finally:
        for p in (cir_path, raw_path):
            try:
                os.remove(p)
            except OSError:
                pass


def main():
    data_dir = os.path.join(ROOT, "training", "data")
    os.makedirs(data_dir, exist_ok=True)

    clips_per_mode = int(os.environ.get("FAIRO_CLIPS_PER_MODE", "24"))
    workers = int(os.environ.get("FAIRO_WORKERS", "7"))

    sources = list_sources()
    rng = random.Random(20261)
    tasks = []
    for mode in ["silicon", "germanium", "bypass"]:
        for i in range(clips_per_mode):
            s = settings_sample(rng)
            s["mode"] = mode
            tasks.append((rng.choice(sources), s))

    print(f"{len(tasks)} clips (sources: {len(sources)}), {workers} workers")

    results = []
    t0 = time.time()
    with concurrent.futures.ProcessPoolExecutor(max_workers=workers) as ex:
        for i, r in enumerate(ex.map(run_one, tasks)):
            if r is not None:
                results.append(r)
            done = i + 1
            if done % 7 == 0 or done == len(tasks):
                el = time.time() - t0
                print(f"  {done}/{len(tasks)} clips, {el:.0f}s elapsed, "
                      f"{len(results)} kept ({el / max(1, done):.1f}s/clip)", flush=True)

    for mode in ["silicon", "germanium", "bypass"]:
        xs, ys, metas = [], [], []
        for x, y, s, src in results:
            if s["mode"] == mode:
                xs.append(x)
                ys.append(y)
                metas.append({**s, "src": src, "n": int(len(x))})
        if not xs:
            print(f"WARNING: no data for {mode}")
            continue
        x = np.concatenate(xs).astype(np.float32)
        y = np.concatenate(ys).astype(np.float32)
        # Save in train/val-split-friendly chunked form.
        out = os.path.join(data_dir, f"fairo_{mode}.npz")
        np.savez_compressed(out, x=x, y=y, meta=np.array(metas, dtype=object))
        print(f"{mode}: {x.size / SR:.1f}s audio -> {out} ({os.path.getsize(out) / 1e6:.0f} MB)")


if __name__ == "__main__":
    main()

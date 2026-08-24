#!/usr/bin/env python3
"""
Train the Phase 3 conditioned neural pedal model (WaveNet-family).

Why WaveNet over the RNN line: torch's CPU/MPS RNN paths are unoptimized
(>=50 s per 12 s clip), while causal convolutions are well optimized; the
plan's [5] comparison found WaveNet-class architectures matched RNN models
on distortion emulation. The model:

  x (B,1,T) -> 1x1 conv -> N=12 dilated causal conv blocks (k=2,
  dilations 1..2048, residual + FiLM conditioning from [fuzz,tone,high,hilo])
  -> skip sum -> ReLU -> 1x1 -> y (B,1,T)

Receptive field: 1 + sum(2^l) = 4096 samples (~93 ms at 44.1 kHz) - covers
the fuzz envelope/sustain dynamics that static waveshapers miss; the gross
1 s output-coupling time constant is handled by the model's learned
DC-blocking within its receptive field plus the plugin's own output stage.

Trained directly on SPICE-generated dry/wet pairs (training/data/).

Run: training/.venv/bin/python training/train_neural.py [mode ...]
"""

import os
import sys
import time

import numpy as np
import torch
import torch.nn as nn

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "training", "data")
ART = os.path.join(ROOT, "training", "artifacts")
os.makedirs(ART, exist_ok=True)

SR = 44100
N_LAYERS = int(os.environ.get("FAIRO_LAYERS", "10"))
CHANNELS = int(os.environ.get("FAIRO_CHANNELS", "12"))
K = 2
RF = 1 + sum(2 ** l for l in range(N_LAYERS))  # 1024 samples
EPOCHS = int(os.environ.get("FAIRO_EPOCHS", "10"))
BATCH = int(os.environ.get("FAIRO_BATCH", "8"))
T_BLOCK = int(os.environ.get("FAIRO_TBLOCK", "4096"))
LR = 1.0e-3


def pad_left(x, d):
    """Causal left padding for a dilation d."""
    return torch.nn.functional.pad(x, (d, 0))


class WNBlock(nn.Module):
    def __init__(self, d, channels=CHANNELS):
        super().__init__()
        self.d = d
        self.conv_a = nn.Conv1d(channels, channels, K, dilation=d)
        self.conv_b = nn.Conv1d(channels, channels, K, dilation=d)
        # FiLM conditioning (padding-aware): scale+bias per channel
        self.film_scale = nn.Linear(4, channels)
        self.film_bias = nn.Linear(4, channels)

    def forward(self, x, cond):
        a = self.conv_a(pad_left(x, self.d))
        b = self.conv_b(pad_left(a, self.d))
        gate = torch.tanh(a) * torch.sigmoid(b)
        scale = self.film_scale(cond).unsqueeze(-1)
        bias = self.film_bias(cond).unsqueeze(-1)
        out = gate * scale + bias
        return x + out, out  # residual, skip


class FuzzWaveNet(nn.Module):
    def __init__(self, n_cond=4, channels=CHANNELS, layers=N_LAYERS):
        super().__init__()
        self.blocks = nn.ModuleList([WNBlock(2 ** l, channels) for l in range(layers)])
        self.in_conv = nn.Conv1d(1, channels, 1)
        self.skip_conv = nn.Conv1d(channels, channels, 1)
        self.head = nn.Conv1d(channels, 1, 1)

    def forward_seq(self, x, cond, h0=None):
        """x: (B, T); cond: (B, C); h0 unused (keeps interface parity)."""
        z = self.in_conv(x.unsqueeze(1))            # (B, C, T)
        skips = 0.0
        for blk in self.blocks:
            z, s = blk(z, cond)
            skips = skips + s
        y = self.head(torch.relu(self.skip_conv(skips)))
        return y.squeeze(1), None


def load_clips(mode):
    d = np.load(os.path.join(DATA, f"fairo_{mode}.npz"), allow_pickle=True)
    x, y, metas = d["x"], d["y"], list(d["meta"])
    bounds = np.cumsum([int(m["n"]) for m in metas])
    prev, clips = 0, []
    for m, b in zip(metas, bounds):
        clips.append({
            "x": x[prev:b].astype(np.float32),
            "y": y[prev:b].astype(np.float32),
            "params": np.array([m["fuzz"], m["tone"], m["high"], float(m["hilo"])],
                               dtype=np.float32),
            "src": m["src"],
        })
        prev = b
    return clips


def make_blocks(clip, rng, block=T_BLOCK, with_context=True):
    """Split a clip into blocks; each block input carries RF samples of
    context before the target region (padding with zeros at clip start)."""
    x, y = clip["x"], clip["y"]
    n = (len(x) // block) * block
    outs = []
    pad = RF if with_context else 0
    for i in range(0, n, block):
        start = max(0, i - pad)
        ctx = np.concatenate([np.zeros(i - start, dtype=np.float32), x[start:i + block]])
        outs.append((ctx.astype(np.float32), y[i:i + block], clip["params"]))
    return outs  # (with_context is kept for interface parity; always used)


def evaluate_blocks(model, clips, device, rng):
    model.eval()
    total, count = 0.0, 0
    with torch.no_grad():
        for clip in clips:
            for ctx, tgt, params in make_blocks(clip, rng, with_context=True):
                bx = torch.from_numpy(ctx).unsqueeze(0).to(device)
                by = torch.from_numpy(tgt).unsqueeze(0).to(device)
                cond = torch.from_numpy(params).unsqueeze(0).to(device)
                yhat, _ = model.forward_seq(bx, cond)
                total += float(torch.nn.functional.l1_loss(
                    yhat[:, -by.shape[1]:], by).detach())
                count += 1
    return total / max(count, 1)


def main():
    modes = sys.argv[1:] or ["silicon", "germanium", "bypass"]
    device = "mps" if (torch.backends.mps.is_available() and
                       os.environ.get("FAIRO_FORCE_CPU") != "1") else "cpu"
    print(f"device: {device}")

    for mode in modes:
        print(f"\n=== {mode} ===", flush=True)
        clips = load_clips(mode)
        rngv = np.random.RandomState(7)
        val_idx = sorted(rngv.choice(len(clips), size=max(2, len(clips) // 5),
                                     replace=False).tolist())
        train_idx = [i for i in range(len(clips)) if i not in val_idx]
        print(f"{len(clips)} clips: train={len(train_idx)} val={len(val_idx)}", flush=True)

        blocks = []
        for i in train_idx:
            blocks.extend(make_blocks(clips[i], np.random.RandomState(i)))
        print(f"{len(blocks)} training blocks", flush=True)

        model = FuzzWaveNet().to(device)
        opt = torch.optim.AdamW(model.parameters(), lr=LR, weight_decay=1e-4)
        best = float("inf")

        for epoch in range(EPOCHS):
            t0 = time.time()
            model.train()
            order = np.random.RandomState(epoch * 31).permutation(len(blocks))
            total, count = 0.0, 0
            for i in range(0, len(order), BATCH):
                sel = order[i:i + BATCH]
                picked = [blocks[int(j)] for j in sel]
                n = max(len(c[0]) for c in picked)
                X = torch.zeros(len(sel), n)
                Y = torch.zeros(len(sel), T_BLOCK)
                C = torch.zeros(len(sel), 4)
                for k, (ctx, tgt, prm) in enumerate(picked):
                    X[k, -len(ctx):] = torch.from_numpy(ctx)
                    Y[k, :len(tgt)] = torch.from_numpy(tgt)
                    C[k] = torch.from_numpy(prm)
                X, Y, C = X.to(device), Y.to(device), C.to(device)
                yhat, _ = model.forward_seq(X, C)
                loss = torch.nn.functional.l1_loss(yhat[:, -T_BLOCK:], Y)
                opt.zero_grad()
                loss.backward()
                torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
                opt.step()
                total += float(loss)
                count += 1
            vl = evaluate_blocks(model, [clips[i] for i in val_idx], device, rngv)
            print(f" epoch {epoch+1}/{EPOCHS}: train={total/max(count,1):.5f} "
                  f"val={vl:.5f} ({time.time()-t0:.0f}s)", flush=True)
            if vl < best:
                best = vl
                torch.save(model.state_dict(), os.path.join(ART, f"{mode}_best.pt"))
        print(f"best val L1: {best:.5f}", flush=True)


if __name__ == "__main__":
    main()

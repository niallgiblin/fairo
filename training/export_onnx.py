#!/usr/bin/env python3
"""
Export the trained FuzzGRU as a stateful ONNX model per clip branch.

Graph contract (used by src/inference/FuzzNeuralInference):
  inputs:
    x    float32 [1, T]    audio block
    cond float32 [1, 4]    [fuzz, tone, high, hilo]
    h    float32 [2*LAYERS, 1, HIDDEN]   stacked per-layer GRU state
  outputs:
    y    float32 [1, T]
    h'   float32 [2*LAYERS, 1, HIDDEN]   updated state

Run: training/.venv/bin/python training/export_onnx.py [mode ...]
"""

import os
import sys

import numpy as np
import torch
import torch.nn as nn

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "training", "artifacts")
MODELS = os.path.join(ROOT, "assets")
os.makedirs(MODELS, exist_ok=True)

sys.path.insert(0, os.path.join(ROOT, "training"))
from train_neural import FuzzWaveNet, RF  # noqa: E402


def export(mode):
    ckpt = torch.load(os.path.join(ART, f"{mode}_best.pt"), map_location="cpu")
    # Derive the architecture from the checkpoint (channels/layers can differ
    # between training runs).
    channels = ckpt["in_conv.weight"].shape[0]
    layers = len([k for k in ckpt if k.startswith("blocks.") and k.endswith(".d")])
    if layers == 0:  # blocks store no 'd' param; count via conv_a weights
        layers = max(int(k.split(".")[1]) for k in ckpt if k.startswith("blocks.")) + 1
    net = FuzzWaveNet(channels=channels, layers=layers)
    print(f"{mode}: inferred channels={channels} layers={layers}")
    net.load_state_dict(ckpt)
    net.eval()

    T = 512
    x = torch.zeros(1, RF + T)   # context + target region
    cond = torch.zeros(1, 4)

    class Causal(nn.Module):
        def __init__(self, inner):
            super().__init__()
            self.inner = inner

        def forward(self, x, cond):
            y, _ = self.inner.forward_seq(x, cond)
            # Full-length causal output (NOT frozen to 512): the host feeds
            # [context + block] and takes the last `block` samples, so any DAW
            # block size works. The old export sliced y[:, -512:], which only
            # ever ran at a 512-sample block and otherwise fell back to dry.
            return y

    model = Causal(net)
    torch.onnx.export(
        model,
        (x, cond),
        os.path.join(MODELS, f"fuzz_{mode}.onnx"),
        input_names=["x", "cond"],
        output_names=["y"],
        dynamic_axes={"x": {1: "T"}, "y": {1: "T"}},
        opset_version=17,
    )

    # Inline external weight data (single-file model for BinaryData embedding).
    import onnx
    path_onnx = os.path.join(MODELS, f"fuzz_{mode}.onnx")
    onnx_model = onnx.load(path_onnx)
    for init in onnx_model.graph.initializer:
        if init.data_location == 1:  # EXTERNAL
            init.data_location = 0
            init.ClearField("external_data")
    onnx.save(onnx_model, path_onnx)
    try:
        os.remove(os.path.join(MODELS, f"fuzz_{mode}.onnx.data"))
    except OSError:
        pass

    # sanity: run it through onnxruntime
    import onnxruntime as ort
    sess = ort.InferenceSession(os.path.join(MODELS, f"fuzz_{mode}.onnx"),
                                providers=["CPUExecutionProvider"])
    xv = np.random.randn(1, RF + 256).astype(np.float32) * 0.1
    cv = np.array([[0.5, 0.5, 0.7, 0.0]], dtype=np.float32)
    y = sess.run(None, {"x": xv, "cond": cv})[0]
    print(f"{mode}: y shape {y.shape} rms {np.sqrt((y**2).mean()):.5f} "
          f"-> assets/fuzz_{mode}.onnx")


if __name__ == "__main__":
    for m in sys.argv[1:] or ["silicon", "germanium", "bypass"]:
        export(m)

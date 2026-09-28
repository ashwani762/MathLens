#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ashwani
"""
Prepares the on-device math recognition model used by the app.

Source: pix2text-MFR 1.5 by breezedeus (MIT License)
        https://huggingface.co/breezedeus/pix2text-mfr-1.5
The upstream repository already ships ONNX exports of the encoder and decoder. This script
downloads them, quantises the weights to int8 (dynamic quantisation, roughly 4x smaller with the
same accuracy on our tests) and writes the token vocabulary the app's decoder needs.

Output (copied into the app):
    android/app/src/main/assets/ocr/mfr_encoder.onnx
    android/app/src/main/assets/ocr/mfr_decoder.onnx
    android/app/src/main/assets/ocr/mfr_vocab.json      (list: token id -> token string)

Usage:
    python3 -m venv .venv && . .venv/bin/activate
    pip install huggingface_hub onnx onnxruntime
    python3 tools/model/prepare_model.py
"""
import argparse
import json
import pathlib

from huggingface_hub import snapshot_download
from onnxruntime.quantization import QuantType, quantize_dynamic

REPO = "breezedeus/pix2text-mfr-1.5"
ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "android/app/src/main/assets/ocr"


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=pathlib.Path, default=OUT, help="output folder (default: the app's assets/ocr)")
    out = ap.parse_args().out
    src = pathlib.Path(snapshot_download(REPO, allow_patterns=["*.onnx", "*.json", "README.md"]))
    out.mkdir(parents=True, exist_ok=True)

    for part in ("encoder", "decoder"):
        quantize_dynamic(
            model_input=str(src / f"{part}_model.onnx"),
            model_output=str(out / f"mfr_{part}.onnx"),
            weight_type=QuantType.QInt8,
        )
        print(f"mfr_{part}.onnx  {(out / f'mfr_{part}.onnx').stat().st_size / 1e6:.1f} MB")

    # tokenizer.json holds token -> id; the app wants the list indexed by id
    vocab = json.loads((src / "tokenizer.json").read_text())["model"]["vocab"]
    tokens = [""] * (max(vocab.values()) + 1)
    for token, i in vocab.items():
        tokens[i] = token
    (out / "mfr_vocab.json").write_text(json.dumps(tokens, ensure_ascii=False))
    print(f"mfr_vocab.json  {len(tokens)} tokens")

    # keep the attribution next to the model files
    (out / "LICENSE-pix2text-mfr.txt").write_text(
        "pix2text-mfr-1.5 (https://huggingface.co/breezedeus/pix2text-mfr-1.5)\n"
        "Copyright (c) breezedeus. Licensed under the MIT License.\n"
        "Quantized to int8 for on-device use.\n"
    )


if __name__ == "__main__":
    main()

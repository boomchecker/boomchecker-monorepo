#!/usr/bin/env python3
"""PlotNeuralNet diagram for the CNN described in article/article_main.tex.

Expected default layout, either:
  scripts/external/PlotNeuralNet/
or:
  external/PlotNeuralNet/
  scripts/generate_cnn_architecture.py

You can also set PLOTNEURALNET_ROOT to point at another local checkout.
"""

import os
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_DIR = Path(__file__).resolve().parent


def find_plotneuralnet_root():
    candidates = [
        os.environ.get("PLOTNEURALNET_ROOT"),
        SCRIPT_DIR / "external" / "PlotNeuralNet",
        PROJECT_ROOT / "external" / "PlotNeuralNet",
    ]

    for candidate in candidates:
        if not candidate:
            continue
        root = Path(candidate).resolve()
        if (root / "pycore" / "tikzeng.py").is_file():
            return root

    checked = "\n".join(f"  - {Path(path).resolve()}" for path in candidates if path)
    raise SystemExit(
        "PlotNeuralNet was not found. Expected pycore/tikzeng.py in one of:\n"
        f"{checked}\n"
        "Set PLOTNEURALNET_ROOT if your checkout is somewhere else."
    )


PLOTNEURALNET_ROOT = find_plotneuralnet_root()

sys.path.insert(0, str(PLOTNEURALNET_ROOT))

from pycore.tikzeng import *  # noqa: E402,F403


plot_root_from_output = os.path.relpath(
    PLOTNEURALNET_ROOT, PROJECT_ROOT / "article" / "figs"
).replace("\\", "/")


# CNN topology from article/article_main.tex.
arch = [
    to_head(plot_root_from_output),
    to_cor(),
    to_begin(),
    to_Conv(
        "input",
        12,
        1,
        offset="(0,0,0)",
        to="(0,0,0)",
        height=58,
        depth=12,
        width=1,
        caption="Input",
    ),
    to_Conv(
        "conv1",
        10,
        32,
        offset="(1,0,0)",
        to="(input-east)",
        height=56,
        depth=10,
        width=3,
        caption="Conv 32",
    ),
    to_connection("input", "conv1"),
    to_Pool(
        "pool1",
        offset="(0,0,0)",
        to="(conv1-east)",
        height=28,
        depth=5,
        width=1
    ),
    to_Conv(
        "conv2",
        3,
        64,
        offset="(1,0,0)",
        to="(pool1-east)",
        height=26,
        depth=3,
        width=4,
        caption="Conv 64",
    ),
    to_connection("pool1", "conv2"),
    to_Pool(
        "pool2",
        offset="(0,0,0)",
        to="(conv2-east)",
        height=13,
        depth=1,
        width=1
    ),
    to_Conv(
        "flatten",
        "",
        "",
        offset="(1,0,0)",
        to="(pool2-east)",
        height=24,
        depth=2,
        width=1,
        caption="Flatten",
    ),
    to_connection("pool2", "flatten"),
    to_Conv(
        "dense",
        "",
        64,
        offset="(2,0,0)",
        to="(flatten-east)",
        height=24,
        depth=24,
        width=2,
        caption="Dense 64",
    ),
    to_connection("flatten", "dense"),
    to_Conv(
        "dropout",
        "",
        "",
        offset="(1,0,0)",
        to="(dense-east)",
        height=20,
        depth=20,
        width=1,
        caption="Dropout",
    ),
    to_connection("dense", "dropout"),
    to_SoftMax(
        "output",
        1,
        offset="(1.5,0,0)",
        to="(dropout-east)",
        width=1,
        height=1,
        depth=1
    ),
    to_connection("dropout", "output"),
    to_end(),
]


def main():
    output = PROJECT_ROOT / "article" / "figs" / "sec3_cnn_architecture.tex"
    to_generate(arch, str(output))


if __name__ == "__main__":
    main()

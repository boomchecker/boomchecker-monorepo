from __future__ import annotations

import argparse
import struct
import time
from pathlib import Path

import numpy as np
import pandas as pd
import serial


PROJECT_ROOT = Path(__file__).resolve().parents[1]
FEATURE_ROOT = PROJECT_ROOT / "generated" / "features"
FEATURE_INDEX = FEATURE_ROOT / "features_manifest.csv"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--split", default="test")
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--timeout", type=float, default=3.0)
    args = parser.parse_args()

    index = pd.read_csv(FEATURE_INDEX)
    rows = index[(index["split"] == args.split) & (index["variant"] == "clean")]
    if args.limit:
        rows = rows.head(args.limit)

    with serial.Serial(args.port, args.baud, timeout=args.timeout) as uart:
        time.sleep(1.0)
        for row in rows.itertuples(index=False):
            mfcc = np.load(FEATURE_ROOT / row.feature_path).astype(np.float32).flatten()
            if len(mfcc) != 696:
                print(f"skip {row.recording_id}: expected 696 floats, got {len(mfcc)}")
                continue
            uart.write(struct.pack(f"<{len(mfcc)}f", *mfcc))
            uart.flush()
            response = uart.readline().decode("utf-8", errors="replace").strip()
            print(f"{row.recording_id},expected={row.class_id},response={response}")


if __name__ == "__main__":
    main()

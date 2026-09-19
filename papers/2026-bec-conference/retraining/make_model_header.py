"""Generate the firmware C header (model_data.h) from a retrained int8 TFLite model.

Overwrites firmware/esp32s3/main/model_data.h (committed — git restores the archived
model if needed). The symbol name must stay `gunshot_model_int8_tflite` to match
main.cpp.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

RETRAINING_ROOT = Path(__file__).resolve().parent
# This paper lives under papers/; the training data, the ml/ pipeline and the
# generated/ feature caches belong to the Zelinjak project. Resolve the repository
# root by walking up to the .git marker so the paths survive further moves.
_HERE = Path(__file__).resolve()
REPO_ROOT = next(p for p in _HERE.parents if (p / ".git").exists())
PROJECT_ROOT = REPO_ROOT / "projects" / "2026-zelinjak-artillery-detection"
BEC_ROOT = REPO_ROOT / "papers" / "2026-bec-conference"
sys.path.insert(0, str(PROJECT_ROOT / "ml"))

from convert_model import write_c_header  # noqa: E402


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--tflite",
        type=Path,
        default=RETRAINING_ROOT / "retrain-waveform2/models/retrained_wf2_new_seed44.tflite",
    )
    parser.add_argument("--header", type=Path, default=PROJECT_ROOT / "firmware/esp32s3/main/model_data.h")
    parser.add_argument("--symbol", default="gunshot_model_int8_tflite")
    args = parser.parse_args()

    if not args.tflite.exists():
        raise SystemExit(f"Model not found: {args.tflite}")
    write_c_header(args.tflite, args.header, args.symbol)
    print(f"Wrote {args.header} from {args.tflite} ({args.tflite.stat().st_size} B)")
    print("Rebuild + flash the firmware before validating (task firmware:build / firmware:flash).")


if __name__ == "__main__":
    main()

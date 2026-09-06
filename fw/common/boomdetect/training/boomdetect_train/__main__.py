"""`bdtrain`: the command line over the package.

    bdtrain manifest              enumerate every dataset present -> manifest.parquet
    bdtrain features [SOURCE..]   run the front end, fill the frame cache
    bdtrain baseline              score the two shipped models on every suite
    bdtrain train                 train every family x layout, save under runs/
    bdtrain compare RUN           evaluate a run's models next to the baseline
    bdtrain export RUN            write model headers + parity vectors into the C tree

Every command reads and writes under the data root (paths.data_root()), never
inside the repository, except `export`, whose whole point is the C tree.
"""

from __future__ import annotations

import argparse
import json
import sys

import pandas as pd

from boomdetect_train import paths
from boomdetect_train.datasets.cache import FrameCache, build_source_cache
from boomdetect_train.datasets.manifest import load_manifest, save_manifest, summarize
from boomdetect_train.datasets.sources import build_rows
from boomdetect_train.dsp.mfcc import Frontend


def cmd_manifest(args: argparse.Namespace) -> int:
    df = build_rows()
    if df.empty:
        print(
            "no datasets found; see datasets/sources.py for where they are expected",
            file=sys.stderr,
        )
        return 1
    p = save_manifest(df)
    print(f"wrote {len(df)} clips to {p}")
    with pd.option_context("display.width", 160, "display.max_rows", 200):
        print(summarize(df).to_string(index=False))
    return 0


def cmd_features(args: argparse.Namespace) -> int:
    df = load_manifest()
    cache = FrameCache()
    frontend = Frontend()
    sources = args.sources or sorted(df["source"].unique())
    for src in sources:
        if cache.has(src) and not args.force:
            print(f"{src}: cached, skipping (use --force to rebuild)")
            continue
        out = build_source_cache(df, src, frontend)
        print(f"{src}: wrote {out}")
    return 0


def cmd_baseline(args: argparse.Namespace) -> int:
    from boomdetect_train.report import baseline_report

    text = baseline_report(load_manifest(), FrameCache(), rule=args.rule)
    print(text)
    out = paths.runs_dir() / "baseline.md"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text, encoding="utf-8")
    print(f"\nwritten to {out}")
    return 0


def cmd_train(args: argparse.Namespace) -> int:
    from boomdetect_train.run import train_all

    run_dir = train_all(
        load_manifest(),
        FrameCache(),
        families=args.families,
        layouts=args.layouts,
        run_name=args.name,
        max_neg_windows_per_clip=args.max_neg_windows,
    )
    print(f"run written to {run_dir}")
    return 0


def cmd_compare(args: argparse.Namespace) -> int:
    from boomdetect_train.report import compare_report

    run_dir = paths.runs_dir() / args.run
    text = compare_report(load_manifest(), FrameCache(), run_dir, rule=args.rule)
    print(text)
    (run_dir / "report.md").write_text(text, encoding="utf-8")
    return 0


def cmd_export(args: argparse.Namespace) -> int:
    from boomdetect_train.run import export_run

    run_dir = paths.runs_dir() / args.run
    written = export_run(run_dir, load_manifest(), FrameCache(), models=args.models)
    for p in written:
        print(f"wrote {p}")
    return 0


def cmd_fixtures(args: argparse.Namespace) -> int:
    from boomdetect_train.fixtures import write_extractor_fixture

    print(f"wrote {write_extractor_fixture()}")
    return 0


def cmd_paths(args: argparse.Namespace) -> int:
    info = {
        "data_root": str(paths.data_root()),
        "raw": str(paths.raw_dir()),
        "cache": str(paths.cache_dir()),
        "runs": str(paths.runs_dir()),
        "mfcc_tables_h": str(paths.MFCC_TABLES_H),
        "models_dir": str(paths.MODELS_DIR),
    }
    print(json.dumps(info, indent=2))
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="bdtrain", description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("paths", help="print the directories in use").set_defaults(fn=cmd_paths)
    sub.add_parser(
        "fixtures", help="regenerate tests/vectors/extractor_expected.h from the Python spec"
    ).set_defaults(fn=cmd_fixtures)
    sub.add_parser("manifest", help="enumerate datasets into manifest.parquet").set_defaults(
        fn=cmd_manifest
    )

    p = sub.add_parser("features", help="fill the frame cache")
    p.add_argument("sources", nargs="*", help="sources to (re)build; default all")
    p.add_argument("--force", action="store_true")
    p.set_defaults(fn=cmd_features)

    p = sub.add_parser("baseline", help="score the shipped models")
    p.add_argument("--rule", default="2of4", help="K-of-N alarm rule, e.g. 2of4, or 'none'")
    p.set_defaults(fn=cmd_baseline)

    p = sub.add_parser("train", help="train the model families")
    p.add_argument("--name", default=None, help="run name (default: timestamp)")
    p.add_argument("--families", nargs="*", default=None, help="mlp svm gbt cnn ...")
    p.add_argument("--layouts", nargs="*", type=int, default=None, help="1 2 3")
    p.add_argument("--max-neg-windows", type=int, default=None, help="cap per negative clip")
    p.set_defaults(fn=cmd_train)

    p = sub.add_parser("compare", help="evaluate a run against the baseline")
    p.add_argument("run")
    p.add_argument("--rule", default="2of4")
    p.set_defaults(fn=cmd_compare)

    p = sub.add_parser("export", help="write C headers and parity vectors for a run")
    p.add_argument("run")
    p.add_argument("--models", nargs="*", default=None, help="model names to export; default all")
    p.set_defaults(fn=cmd_export)

    args = ap.parse_args(argv)
    return int(args.fn(args))


if __name__ == "__main__":
    raise SystemExit(main())

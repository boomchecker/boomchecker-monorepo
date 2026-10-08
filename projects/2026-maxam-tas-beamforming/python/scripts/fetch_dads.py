"""CLI for ``task sim:fetch``: cache a fixed selection of DADS drone clips."""

from __future__ import annotations

import argparse
from pathlib import Path

from beamforming import dads


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--n", type=int, default=100, help="number of clips")
    parser.add_argument("--out", type=Path, default=dads.DEFAULT_OUT)
    parser.add_argument("--shard", type=int, default=dads.DEFAULT_SHARD, help="drone shard")
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--row-groups", type=int, default=20)
    args = parser.parse_args()
    manifest = dads.fetch(args.n, args.out, args.shard, args.seed, args.row_groups)
    print(f"saved {len(manifest['clips'])} clips to {args.out}")


if __name__ == "__main__":
    main()

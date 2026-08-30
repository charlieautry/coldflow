"""Command line front door.

    python -m teststand run sequences/hold_10psi.yaml --fake
    python -m teststand run sequences/hold_10psi.yaml --port COM5 --log data/run1.csv
    python -m teststand review data/run1.csv --plot data/run1.png
"""

from __future__ import annotations

import argparse
import sys
from datetime import datetime
from pathlib import Path

from teststand import review as review_mod
from teststand.driver import TestStand
from teststand.fakeboard import FakeBoard
from teststand.sequence import SequenceRunner, load_sequence


def _open_stand(args, meta: dict) -> TestStand:
    if args.port:
        return TestStand.open(args.port, log_path=args.log, log_meta=meta)
    if not args.fake:
        print("no --port given, using the fake board (pass --fake to hush this)", file=sys.stderr)
    return TestStand(FakeBoard(), log_path=args.log, log_meta=meta)


def cmd_run(args) -> int:
    seq = load_sequence(args.sequence)
    if args.log is None:
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        args.log = Path("data") / f"{seq['name']}_{stamp}.csv"
    meta = {"test": seq["name"], "port": args.port or "FAKE", "sequence_file": str(args.sequence)}

    with _open_stand(args, meta) as stand:
        result = SequenceRunner(stand).run(seq)
    print(result.summary())
    print(f"log: {args.log}")
    return 0 if result.passed else 1


def cmd_review(args) -> int:
    log = review_mod.load_log(args.csv)
    print(review_mod.summarize(log))
    if args.plot:
        out = review_mod.plot(log, args.plot)
        print(f"plot: {out}")
    return 0


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="teststand")
    sub = p.add_subparsers(dest="cmd", required=True)

    run = sub.add_parser("run", help="execute a yaml sequence")
    run.add_argument("sequence")
    run.add_argument("--port", help="serial port of the board (omit for the fake)")
    run.add_argument("--fake", action="store_true", help="use the simulated board explicitly")
    run.add_argument("--log", type=Path, help="csv path (default: data/<name>_<stamp>.csv)")
    run.set_defaults(fn=cmd_run)

    rev = sub.add_parser("review", help="summarize a session csv")
    rev.add_argument("csv")
    rev.add_argument("--plot", type=Path, help="also write a png here")
    rev.set_defaults(fn=cmd_review)

    args = p.parse_args(argv)
    return args.fn(args)


if __name__ == "__main__":
    raise SystemExit(main())

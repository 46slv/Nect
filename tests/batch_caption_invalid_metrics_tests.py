"""Explicit diagnostic regression for Qt's unresolved-font metric backend.

This is separate from the normal positive caption-fit suites. It requires an
observed invalid-origin backend and checks that reservation succeeds while the
existing historical 98x80 raster-fit assertion still reports its real failure.
Example: python3 tests/batch_caption_invalid_metrics_tests.py \
    build/d0-core/batch_point_angle_ui_tests --platform minimal
"""
import argparse
import math
import os
from pathlib import Path
import re
import subprocess
import sys


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def verify(output, returncode, platform):
    check(f"BATCH_QPA_EFFECTIVE platform={platform} " in output,
          "the explicitly requested QPA backend must actually run")
    rows = [line for line in output.splitlines()
            if line.startswith("CAPTION_RESERVE kind=all-resolved ")]
    check(len(rows) == 27, "all 27 actual-widget reservation samples must be measured")
    expected = 98
    old_expected = 98
    for row in rows:
        check(" bounds=100000,100000 " in row,
              "this regression requires the invalid logical-origin backend")
        advance = float(re.search(r"\badvance=(\S+) ", row)[1])
        tight = re.search(r"\btight=\S+,\S+ \S+x\S+ \[left=(\S+) right=(\S+)\]", row)
        left, right = map(float, tight.groups())
        check(all(math.isfinite(value) for value in (advance, left, right))
              and advance >= 0 and right >= left,
              "advance and tight ink bounds must be usable despite the loose origin")
        span = max(advance, right) - min(0, left)
        check(float(re.search(r" span=(\S+) ", row)[1]) == span,
              "reported reservation span must preserve advance and both ink overhangs")
        expected = max(expected, math.ceil(span))
        old_expected = max(old_expected, math.ceil(float(
            re.search(r"\blogical-span=(\S+) ", row)[1])))
    summary = re.search(r"CAPTION_RESERVE_SUMMARY samples=27 actual-minimum=(\d+) "
                        r"replay-minimum=(\d+) .* matches=(\d+)", output)
    check(summary is not None, "actual reservation and replay must both be reported")
    actual, replay, matches = map(int, summary.groups())
    check(actual == expected == replay and matches == 1,
          "the product reservation must match measured ink, not the invalid logical origin")
    check(old_expected - actual == 100000,
          "the regression must remove the observed 100000-origin inflation without a width cap")
    check(f"CAPTION_RESERVATION_CHECK actual={actual} expected={expected} matches=1" in output,
          "the actual-widget reservation assertion must pass before raster testing")
    check(returncode == 1, "the unsupported-font positive fit suite must still fail")
    failures = [line for line in output.splitlines() if line.startswith("FAIL ")]
    check(failures == ["FAIL shared point-batch caption samples fit 98×80px with the active UI font metrics"],
          "only the original historical 98x80 positive fit assertion may fail")
    check(re.search(r"CAPTION_FIT fits=0 .* allocated=98x80 .* outside=[1-9]\d* truncated=0 ", output),
          "unclipped raster evidence must retain real 98x80 overflow")
    print(f"PASS invalid-origin reservation: {old_expected} -> {actual}; "
          "all 27 samples; original 98x80 fit failure preserved")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--platform", required=True, choices=("minimal", "offscreen"),
                        help="explicit backend known to reproduce unresolved font metrics")
    args = parser.parse_args()
    env = os.environ.copy()
    env.update(NECT_TEST_QPA_PLATFORM=args.platform, NECT_GEOMETRY_ONLY="1",
               NECT_GEOMETRY_DIAGNOSTICS="1")
    result = subprocess.run([str(args.executable.resolve())], env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            encoding="utf-8", errors="replace", timeout=30)
    print(result.stdout, end="")
    verify(result.stdout, result.returncode, args.platform)


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, subprocess.TimeoutExpired) as error:
        print(f"FAIL invalid-origin regression: {error}", file=sys.stderr)
        sys.exit(1)

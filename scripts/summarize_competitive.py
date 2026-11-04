#!/usr/bin/env python3
"""Summarize competitive_paths_benchmark CSV without weighting large cases more.

Usage: python3 scripts/summarize_competitive.py data/competitive_trial_results.csv
Ratios use each row's seven-run median. The score first averages log speedups
within each family, then across families, so each family has equal weight.
"""

import argparse
import csv
import math
from collections import defaultdict


def summarize(path, baseline):
    cases = defaultdict(dict)
    with open(path, newline="", encoding="utf-8") as source:
        for row in csv.DictReader(source):
            key = (row["family"], row["seed"], row["n"], row["m"], row["source"])
            solver = row["solver"]
            if solver in cases[key]:
                raise ValueError(f"duplicate solver result: {key}, {solver}")
            elapsed = float(row["median_ms"])
            if not math.isfinite(elapsed) or elapsed <= 0:
                raise ValueError(f"nonpositive or nonfinite elapsed time: {key}, {solver}")
            cases[key][solver] = elapsed
    if not cases:
        raise ValueError("CSV contains no results")
    solvers = set(next(iter(cases.values())))
    if baseline not in solvers:
        raise ValueError(f"baseline {baseline!r} not present")
    if any(set(timings) != solvers for timings in cases.values()):
        raise ValueError("each case must contain the same solver set")

    print(f"{len(cases)} cases; {len({key[0] for key in cases})} families; baseline={baseline}")
    print("Solver                     Family-weighted speedup   Worst slowdown    Wins")
    for solver in sorted(solvers):
        logs = defaultdict(list)
        worst_slowdown = 0.0
        wins = 0
        for key, timings in cases.items():
            logs[key[0]].append(math.log(timings[baseline] / timings[solver]))
            worst_slowdown = max(worst_slowdown, timings[solver] / timings[baseline])
            wins += timings[solver] == min(timings.values())
        family_logs = [sum(values) / len(values) for values in logs.values()]
        score = math.exp(sum(family_logs) / len(family_logs))
        print(f"{solver:<26} {score:>20.3f}x {worst_slowdown:>16.3f}x {wins:>7}")
    print("Wins count fastest medians, including exact ties; small differences may be noise.")
    print("Worst slowdown is relative to the fixed baseline, not the per-case winner.")
    print("This synthetic suite cannot establish universal optimality or scientific novelty.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path")
    parser.add_argument("--baseline", default="previous_portfolio")
    arguments = parser.parse_args()
    summarize(arguments.csv_path, arguments.baseline)


if __name__ == "__main__":
    main()

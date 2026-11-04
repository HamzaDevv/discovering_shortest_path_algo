# Competitive portfolio trial

Date: 4 November 2026. Implementation:
[`competitive_paths.hpp`](../include/competitive_paths.hpp).

This iteration establishes a finite-portfolio work bound and adds a broader
evaluation. It **does not establish a universally best shortest-path algorithm**.
The earlier portfolio remains faster overall in our measurements. Geometric
restarts and algorithm portfolios are established techniques; this composition
has no verified scientific novelty.

## Problem and output

Inputs are directed multigraphs with signed 64-bit integer edge weights and one
source. Self loops, parallel edges, and disconnected components are allowed.
The result contains either exact finite/unreachable distances or original edge
indices forming a source-reachable negative cycle. It does not classify every
vertex whose distance is minus infinity. Internal arithmetic is checked signed
128-bit arithmetic; overflow throws rather than silently changing an answer.
Floating-point and arbitrary real weights are outside this implementation.

## Algorithm

Build adjacency and check for a DAG in linear time. A DAG is solved directly in
topological order. Otherwise set `B = n + m` and try four independent algorithms,
each with a budget of `B`:

1. In-place Bellman-Ford, with periodic predecessor cycle checks.
2. FIFO label-correcting relaxation, with the same checks.
3. An indexed binary heap, allowing vertices to reopen on signed graphs.
4. BranchCore, with an optional scan limit.

On globally nonnegative graphs, try heap, Bellman-Ford, FIFO, then BranchCore;
omit predecessor checks in the first three engines because no negative cycle
can exist. On other graphs use the order above. If an attempt completes, return
its answer. Otherwise discard its private labels. After four exhausted attempts,
double `B` and restart. This is sequential execution, with no worker threads.

Each edge scan costs one budget unit. Heap insert/decrease/extract operations
also cost `2 * (1 + bit_width(heap_size)) + 1` units. This conservatively covers
the logarithmic sifts, including two key comparisons per downward level.
The heap holds at most one pending entry per vertex, keeping storage linear
even when signed relaxations repeatedly reopen vertices.

Predecessor checks occur every `n` edge scans and explicitly sum original edge
weights. A budget cutoff never supplies a cycle verdict. Bellman-Ford also
checks for a witness after a changed nth full pass. FIFO and heap return finite
distances only when their worklists are empty; BranchCore uses its existing
structural proof and expands its cycle witness back into original edges.

## Correctness argument

Every finite label is the weight of an actual source walk: labels start at the
source and change only through an original-edge relaxation. A returned negative
cycle is connected, source reachable, and has a checked strictly negative sum.

An unchanged complete Bellman-Ford pass leaves no relaxable reachable edge.
An empty FIFO or heap worklist has the same property: every improvement
schedules its vertex for processing, including improvements to a vertex being
processed. Walk realizability together with these edge inequalities proves
shortest distances. Unreachable vertices retain absent labels. The DAG branch
uses the standard topological argument. BranchCore's contraction argument is
in [SIGNED_CORE.md](SIGNED_CORE.md).

Discarding an unfinished attempt cannot produce an answer. Bellman-Ford and
BranchCore have finite completion work, so geometrically increasing budgets
eventually allow a correct engine to finish, subject to arithmetic, counter,
allocation, and budget representability limits. No termination assumption for
signed FIFO or heap is needed for this argument.

## Precisely what the work bound says

Use an arithmetic RAM model in which checked 128-bit operations and vertex/edge
index operations cost constant time. Let `S = n + m`. For each of the four
engines, let `q_i` be its metered work when run independently to completion on
this input, using the same scheduling and certificate cadence. Set `q_i` to
infinity if that engine would not finish. These are engine work measures,
**not measured milliseconds**. In particular, heap charges conservatively
estimate sift work and need not equal the comparisons actually performed.

An attempt with budget `B` costs `O(S + B)`:

- Initialization, adjacency/SCC preparation, and structural contraction cost
  `O(S)`, including preprocessing that is not charged as an edge scan.
- FIFO pushes/pops are bounded by successful relaxations plus initialization.
- Heap work is bounded by its conservative logarithmic charges; vector growth
  is amortized over insertions.
- Each predecessor inspection costs `O(n)` and happens once per `n` scans;
  total inspection work is `O(n + B)`, including a final witness check.
- BranchCore's repeated core relaxation and recovery scans are budgeted;
  its remaining preparation and witness expansion are linear in `S`.
- Discarding a partial attempt also costs at most `O(S)`.

Let `q* = min_i q_i`, and consider the first budget at least `q*`. An included
engine must complete by that round. With `B_j = 2^j S`, the total work is

```text
O(S) + sum_j 4 * O(S + B_j) = O(S + q*).
```

The geometric sum absorbs repeated initialization because every budget is at
least `S`. The DAG specialization costs `O(S)` and preserves the same bound.
Auxiliary space is `O(S)` because attempts run sequentially and the indexed
heap has at most `n` entries.

Consequences include `O(n+m+nm)` on general inputs, the BranchCore bound
`O(n+m+sum_C k_C*t_C)` when it is smaller, linear time on DAGs, and
`O(n+m+(n+m)*log(n+1))` on nonnegative graphs through the heap candidate.
The bound is relative to these four particular engines. It is not relative to
every possible algorithm, does not establish a best constant factor, and does
not improve the strongest known general asymptotic bounds.

## Evaluation and results

The benchmark covers 17 graph families at `n=300` and `n=1500`, with three
development seeds and three fresh holdout seeds: 102 graph/source instances per
split, five solvers per instance. Families include ordered/shuffled signed and
negative rings, zero-sum cycles, signed DAGs, an adversarial queue/heap DAG,
sparse/dense potential-generated graphs, nonnegative graphs, many SCCs, a
reverse bidirectional chain, reachable/unreachable negative cycles, sparse
branching rings, and subdivided kernels. Vertex labels and sources vary.

Each method gets one warmup and seven timed runs with rotated execution order.
Timing includes solver preprocessing and statistics capture. Exact comparison
with Bellman-Ford, reachability, cycle-witness validation, and finite edge
inequality checks occur outside timing. CSV files contain medians, minima,
maxima, and work counters. The independent Floyd-Warshall test suite supplies
additional correctness evidence.

The aggregate score is a geometric mean of speedups against the fixed
`previous_portfolio` baseline, first within each family, then across families.
Higher is better; below 1 means slower. Worst slowdown is relative to that same
baseline. Wins count the fastest median among all five measured solvers.

| Solver | Development score | Holdout score | Holdout worst slowdown | Holdout wins / 102 |
|---|---:|---:|---:|---:|
| Previous portfolio | 1.000 | 1.000 | 1.000x | 43 |
| Competitive portfolio | 0.661 | 0.638 | 8.087x | 15 |
| Adaptive Core | 0.516 | 0.512 | 81.887x | 10 |
| Bellman-Ford | 0.454 | 0.442 | 105.190x | 37 |
| BranchCore | 0.366 | 0.359 | 78.048x | 0 |

The competitive candidate is about **1.25x faster than Adaptive Core** by the
holdout aggregate score, but **1.57x slower than the previous portfolio**. It
reduces the largest measured slowdown versus Adaptive Core substantially.
Neither the theorem nor these results support a universally fastest claim.

An initial development run scored 0.534. After inspecting that run, we added
the DAG specialization and changed nonnegative dispatch/checking. That initial
CSV is preserved as `competitive_trial_initial_results.csv`. The final
development run is in `competitive_trial_results.csv`; fresh-seed evaluation
is in `competitive_holdout_results.csv`. No performance changes were made
after examining holdout results. This holdout changes seeds within the same
synthetic families, so it does not measure generalization to arbitrary real
workloads. Microsecond cases and differences near 1x are noisy; min/max columns
show sample spread, not confidence intervals. Measurements used Apple Clang
21.0.0 on macOS arm64, C++17, `-O2`, with no simultaneous test/compile run.

## Research checked during this iteration

These primary papers place the experiment in context; their algorithms were
not implemented or timed here. Their assumptions and output requirements differ.

- [Universal Optimality of Dijkstra via Beyond-Worst-Case Heaps](https://arxiv.org/abs/2311.11793):
  universal optimality has a precise meaning involving distance ordering and
  worst-case weights on a fixed topology. It does not mean fastest elapsed
  time on every weighted input, or cover our signed-cycle problem directly.
- [Deterministic Negative-Weight Shortest Paths in Nearly Linear Time via Path Covers](https://arxiv.org/abs/2511.08551):
  gives a deterministic near-linear result for bounded integer weights using
  path covers. Our general `O(nm)` envelope is weaker.
- [A Faster Directed Single-Source Shortest Path Algorithm](https://arxiv.org/abs/2602.07868):
  gives a deterministic improvement for nonnegative real weights in the
  comparison-addition model. Improving on a particular binary-heap program
  does not imply improving on this theoretical result.
- [Bellman-Ford in Almost-Linear Time](https://arxiv.org/abs/2607.19346):
  reports `m^(1+o(1))` time for directed SSSP with possibly negative real weights.
- [Size-Sensitive Padded Decompositions for Faster Deterministic Negative-Weight Shortest Paths](https://arxiv.org/abs/2609.05590):
  reports `O((m+n log log n) log^2 n log(nW))` for integer weights at least `-W`.

This search is not an exhaustive novelty audit. The portfolio proof is a
standard geometric-budget argument, and no claim of first discovery is made.

## Reproduce

```sh
make run-tests benchmarks
build/competitive_paths_benchmark > data/competitive_trial_results.csv
build/competitive_paths_benchmark --holdout > data/competitive_holdout_results.csv
python3 scripts/summarize_competitive.py data/competitive_trial_results.csv
python3 scripts/summarize_competitive.py data/competitive_holdout_results.csv
```

Sanitizer tests passed: 4,096 exhaustive three-vertex graphs for every source,
20,000 seeded random multigraphs across nine configurations, focused budget
and indexed-heap regressions, and 2,000 bounded-engine/metamorphic trials.
The existing suite also checks 500 subdivided cores and the separate portfolio
suite checks 12,000 random graphs. Metamorphic checks shuffle edges, relabel
vertices, and apply `w'(u,v)=w(u,v)+h(u)-h(v)`, preserving cycle weights and
transforming finite distances to `d'[v]=d[v]+h[source]-h[v]`.

# Research session: from visit counters to BranchCore

Recorded on 4 November 2026 after the repository refactor. The research,
implementations, and saved benchmark measurements below were produced during
the 3 November 2026 session. This record preserves the reasoning and outcomes;
the linked technical reports contain the detailed arguments and literature.

## Starting question

Could repeated visits in a priority queue reveal a negative cycle in just a few
iterations, making shortest paths with signed weights as fast as Dijkstra on
nonnegative weights? We also explored alternatives beyond heap scheduling.

A reachable negative cycle permits indefinitely decreasing walk costs when
vertices can be reopened. However, repeated improvements do not certify a
cycle. In the original four-vertex DAG, vertex 3 improves four times through
different paths despite there being no cycle:

```text
0 -> 1 : -10    0 -> 2 : -20    0 -> 3 : -25
1 -> 2 : -20    1 -> 3 : -22    2 -> 3 : -10
```

Its correct distance from 0 to 3 is -40. Reducing the visit threshold cannot
repair this rule. A work budget can select a fallback, but cannot serve as a
negative-cycle certificate. See [the counterexample and initial solver](ALGORITHM.md).

## Implemented progression

| Implementation | Session result |
|---|---|
| [Portfolio solver](../include/shortest_paths.hpp) | Selects topological relaxation for DAGs, Dijkstra for nonnegative graphs, and bounded heap reopening with checked predecessor cycles for mixed weights. Bellman-Ford provides the fallback guarantee. |
| [Signed-core candidate](../include/signed_core.hpp) | Processes SCCs separately, tries a tree potential, and combines nonnegative Dijkstra closures with bounded negative-edge phases. Historical walks allow certificate recovery; Bellman-Ford remains an alternative backend. |
| [BranchCore candidate](../include/branch_core.hpp) | Compresses forced continuations inside SCCs, solves the retained branching core with multi-source Bellman-Ford, and reconstructs original distances or a negative-cycle witness. |

BranchCore became the main structural experiment. All cycles lie inside SCCs.
Within an SCC, a vertex with one internal outgoing edge has a forced
continuation. Retain branching vertices and replace each outgoing edge followed
by its forced suffix with one edge of the same total weight. A pure directed
ring becomes a one-vertex core with a self-loop. Process SCCs in condensation
order so their incoming labels are final before solving them.

The reduction preserves multiple incoming labels, including a source inside a
removed chain. Memoized suffix costs avoid repeatedly expanding shared paths.
After solving the core, a topological sweep recovers distances to removed
vertices. A reported core cycle is expanded into original edge indices and
its original total weight is checked. The complete correctness argument is in
[the BranchCore report](SIGNED_CORE.md#correctness-argument).

## Bound and scope

Let `k_C` be the retained vertex count and `t_C` the compressed edge count in
reachable SCC `C`, including the one-vertex ring convention. BranchCore takes

```text
O(n + m + sum_C k_C * t_C) time, O(n + m) auxiliary space.
```

For a branching SCC, let `b_C` be its branching vertex count and
`q_C = m_C - n_C + 1` its internal excess. Then `k_C=b_C`,
`t_C=q_C-1+b_C`, `b_C<=q_C-1`, and `t_C<=2(q_C-1)`. Consequently:

```text
O(n + m + sum_{C:q_C>=2} min(n_C*m_C, 2*(q_C-1)^2)).
```

This gives linear total time when each SCC has bounded internal excess,
including arbitrary signed weights and negative-cycle detection. For general
graphs the core can be the original graph and time remains `O(n+m+nm)`.
These are exact arithmetic operation bounds; the implementation accepts
signed 64-bit weights and uses checked signed 128-bit arithmetic.

The API returns finite/unreachable distances when no source-reachable negative
cycle exists, or one such cycle as an original-edge certificate. It does not
classify every vertex affected by negative cycles as having distance minus
infinity. An unreachable negative cycle does not invalidate the source's
finite distances.

## Research conclusions

The [eight-paper prior-art audit](SIGNED_CORE.md#prior-art-review-and-novelty-audit)
covers predecessor-cycle detection, Bellman-Ford/Dijkstra hybrids, SCC
preprocessing, degree-one elimination, restricted relaxation lower bounds,
integrated cycle finding, and 2026 near/almost-linear shortest-path preprints.
Those papers and their primary-source links are recorded in the report.

The experiment produced a concrete implementation, correctness argument,
structural bound, and reproducible measurements. We did **not** establish
scientific novelty, a better general asymptotic bound, or universal practical
dominance. Modern general algorithms use techniques beyond queue visit counts;
their theoretical bounds are not measurements of this prototype. Our
literature search is not an exhaustive novelty review.

## Evidence retained in the repository

- [Portfolio tests](../tests/shortest_paths_test.cpp): regressions and 12,000
  seeded random graphs, three configurations checked against Floyd-Warshall.
- [Signed-core and BranchCore tests](../tests/signed_core_test.cpp): 4,096
  exhaustive three-vertex graphs for all three sources, 20,000 seeded random
  multigraphs, seven configurations, and 500 additional subdivided cores.
  Returned cycles are checked for connectivity, source reachability, and a
  strictly negative original weight sum.
- [BranchCore benchmark source](../benchmarks/signed_core_benchmark.cpp) and
  [saved CSV](../data/signed_core_results.csv): eight graph families, three
  sizes, six methods, seven-run medians with rotated order. Solver timings
  include preprocessing; input generation and answer checking are excluded.
- [Earlier benchmark source](../benchmarks/shortest_paths_benchmark.cpp) and
  [saved CSV](../data/verified_benchmark_results.csv) retain the initial
  portfolio comparison separately from the legacy experiments.

Saved measurements show both gains and losses. For 1,000 vertices, BranchCore
compresses a negative ring to 1 vertex/1 edge and the sparse branching family
to 10 vertices/20 edges. It beats the Bellman-Ford baseline on those recorded
instances, while the earlier portfolio is faster still. Bellman-Ford wins
several small random families and the positive-sum ring. See the
[timing table and limitations](SIGNED_CORE.md#tests-and-reproducibility).
An edge-scan count excludes preprocessing, so one core scan does not imply
constant total work on an original ring.

## Reproduce after the refactor

Run from the repository root with a C++17 compiler supporting `__int128_t`:

```sh
make all benchmarks
make run-tests
printf '3 3 0\n0 1 1\n1 2 -5\n2 0 2\n' | build/branch_core_demo

# Keep historical measurements intact; record a new run separately.
build/shortest_paths_benchmark > /tmp/verified_benchmark_results.csv
build/signed_core_benchmark > /tmp/signed_core_results.csv
```

The Makefile compiles tests with AddressSanitizer, UndefinedBehaviorSanitizer,
and warnings treated as errors. Both suites passed again on 4 November after
the refactor; all demos and benchmark programs compiled successfully with
Apple Clang 21.0.0 on macOS arm64. Historical CSV timings remain the original
session's measurements.

## Open research work

1. Compare the reduction and bound with existing parameterized shortest-path
   results before making a novelty claim.
2. Benchmark against strong public implementations, including Goldberg-Radzik,
   on larger and real-world graphs as well as adverse synthetic families.
3. Investigate an adaptive choice between direct solving and compression,
   accounting for preprocessing costs and graphs where almost every vertex
   branches. Any performance claim needs corresponding analysis and evidence.

# Shortest paths with negative weights: findings and a verified implementation

The follow-up [BranchCore research experiment](SIGNED_CORE.md) adds structural
compression, proofs, an expanded prior-art review, and new verified benchmarks.

Reviewed 3 November 2026. The original `Main.cpp`, `local.cpp`,
`ap_spfa.cpp`, and `benchmark.cpp` are historical experiments. Their raw
relaxation counters do **not** correctly certify negative cycles. Use
`shortest_paths.hpp` and the new demo/tests/benchmark for the verified work.

## What the loop hypothesis gets right

A source-reachable negative cycle allows indefinitely decreasing walk costs.
A heap-based label-correcting algorithm that permits vertices to be reopened
will keep finding improvements, assuming exact arithmetic and no cutoff.

However, the heap need not stay on one cycle: improvements can propagate through
many branches. Repeated visits also occur without a negative cycle. Standard
Dijkstra finalizes vertices; permitting reopening changes the algorithm and
removes its usual running-time proof.

A negative cycle makes distances **minus infinity** only for vertices reachable
from that cycle, when the cycle itself is reachable from the source. Other
vertices can have finite distances or be unreachable. This implementation
returns either all distances or one reachable negative-cycle witness; it does
not classify every vertex as finite/unreachable/minus infinity when a cycle exists.
To detect a cycle anywhere in a graph, add a new source with zero-weight edges
to all original vertices.

## Concrete counterexample to the existing rule

Source 0; four vertices; these six directed edges, in this adjacency order:

```text
0 -> 1 : -10
0 -> 2 : -20
0 -> 3 : -25
1 -> 2 : -20
1 -> 3 : -22
2 -> 3 : -10
```

Every edge increases the vertex number: this is a DAG, so it has no cycle.
The heap processes current labels in this order:

```text
pop 0: d[1]=-10, d[2]=-20, d[3]=-25
pop 3
pop 2: d[3]=-30
pop 3
pop 1: d[2]=-30, d[3]=-32
pop 3
pop 2: d[3]=-40
```

Vertex 3 improves four times. `Main.cpp` returns an empty vector at the fourth
improvement and falsely reports a negative cycle. This was reproduced against
the actual function, with Bellman-Ford returning distance -40. The deque SLF
rule in `ap_spfa.cpp` also follows this order on these edges.

The earlier proof confused **number of improvements** with **edges in one walk**.
The V-1-edge property of simple paths does not bound the number of different
paths that can successively improve a label. A smaller fixed visit threshold
cannot repair that argument. A work cutoff must trigger a different algorithm,
not a negative-cycle verdict.

## Practical implementation

`shortest_paths.hpp` combines established methods; this is not a claimed new
asymptotic algorithm or an implementation of the research papers below.

1. Validate endpoints and try a topological ordering. A DAG is solved in one
   pass, including negative weights.
2. On a cyclic graph with only nonnegative edges, run binary-heap Dijkstra.
3. On a mixed-weight cyclic graph, allow heap reopenings, storing the original
   edge responsible for every label improvement.
4. After every n edge scans, inspect the predecessor graph in O(n) time. Extract
   candidate cycles and sum their actual edge weights. Report a cycle only when
   its sum is strictly negative.
5. After 4(n+m) edge scans, restart with Bellman-Ford if necessary. The cutoff is
   a performance budget, never a mathematical certificate.

For an n-vertex negative ring, the predecessor edges form the ring after the
first n scans, and a certificate is returned immediately. This improves the
original counter's n circuits on that family. It does not prove that all graphs
can be solved in one circuit or a few iterations.

Correctness:

- Every finite label is the weight of a source-rooted walk, by induction on
  relaxations. Every predecessor edge therefore belongs to the reached region.
- A reported witness lists actual connected edges in forward cycle order, with
  an explicitly checked negative sum. Parallel edges are identified by index.
- If heap processing finishes, every reachable edge satisfies the relaxation
  inequality. Labels are realizable walk weights and no path can improve them;
  these are shortest distances. A reachable negative cycle would contradict
  those inequalities when summed around the cycle.
- If the budget expires, ordinary Bellman-Ford provides the correctness and
  termination guarantee. An improvement in its nth complete pass proves a
  reachable negative cycle; its predecessor graph supplies a checked witness.

For the default budget, including preprocessing:

| Graph/route | Time bound | Auxiliary space |
|---|---|---|
| DAG | O(n+m) | O(n+m) |
| Nonnegative, lazy binary heap | O(n + m log(m+1)) | O(n+m) |
| Mixed heap phase and cycle checks | O((n+m) log(n+m)) | O(n+m) |
| Full mixed case including fallback | O(nm + (n+m) log(n+m)) | O(n+m) |

For simple graphs the usual binary-heap expression is O((n+m) log n).
Periodic checks cost O(n) per n scans, so their total cost is linear in the
phase's scan budget. The fallback retains Bellman-Ford's worst case. An explicit
custom budget changes the corresponding phase bound.

Weights are signed 64-bit integers; distances use GCC/Clang signed 128-bit
arithmetic with checked addition. Unreachable is an optional value, not a finite
sentinel. Overflow throws instead of silently changing cycle signs. This API
handles integer weights, not arbitrary floating-point/real inputs.

## Why the old benchmark cannot establish the claim

`benchmark.cpp` omits negative-cycle detection from its Bellman-Ford baseline,
but stops both queue methods after an arbitrary 20*n*n pop limit. It does not
compare answers. These algorithms can terminate for different reasons, so those
measurements do not compare equivalent tasks. Random signed graphs can contain
negative cycles. SLF is an existing queue ordering heuristic, and a smaller
measured operation count on one distribution is not a better worst-case bound.

`Main.cpp` also uses signed int distances with -1,000,000,000-weight ring edges,
which can overflow after only a few extensions. Its first loop iteration sets
`edge_max` to -1 and requests an invalid graph, making that driver unsuitable
as a correctness benchmark. `local.cpp` has additional unproved coloring rules.

The new benchmark uses seeded graph families, checks returned distances/status,
checks cycle weights and edge connectivity, alternates timing order, and reports
five-run medians plus edge scans. Potential-generated weights
`w(u,v)=base(u,v)-p(u)+p(v)` guarantee no negative cycle because the potentials
telescope around every cycle. Timings include solver preprocessing but exclude
graph generation and validation of returned answers.

The local results in `verified_benchmark_results.csv` show substantial scan
reductions for negative rings and reverse-ordered chains. Bellman-Ford is faster
in wall time on several small random families despite scanning more edges.
This hybrid does not win on every input. These small benchmarks are diagnostic,
not a publication-grade performance study or a comparison with modern research
implementations.

## Research checked as of 3 November 2026

The strongest relevant results located in this review include:

| Weight domain | Result | Source |
|---|---|---|
| Nonnegative real | Deterministic O(m sqrt(log n) + sqrt(mn log n log log n)) | [Duan, Mao, Shu, Yin, ICALP 2026](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.ICALP.2026.81) |
| Signed integer | Deterministic near-linear time, with a log(nW) factor in the path-cover result | [Haeupler, Jiang, Saranurak, 2025 preprint](https://arxiv.org/abs/2511.08551); [Li, STOC 2026](https://arxiv.org/abs/2511.07859) |
| Signed integer | O((m+n log log n) log(nW) log n log log n), using randomized decomposition | [Li, Mowry, Rao, SODA 2026](https://epubs.siam.org/doi/10.1137/1.9781611978971.104) |
| Arbitrary real, possibly negative | Las Vegas randomized m^(1+o(1)) time with high probability | [Hair, Li, Li, Zhang, July 2026 preprint, Theorem 1.1](https://arxiv.org/html/2607.19346v1) |

The July paper uses sparse shortcutting and potentials to reduce negative hops;
its unfolding lemma controls growth of the augmented graph. It is a theoretical
result, not evidence that a short practical heap implementation is fastest.
Almost-linear m^(1+o(1)) and near-linear bounds hiding logarithms are not the same
as O(m log n).

Potentials give an important direction: replace each weight by
`w'(u,v)=w(u,v)+p(u)-p(v)`. All paths with the same endpoints shift by the same
amount. If all reweighted edges are nonnegative, Dijkstra becomes valid.
But cycle weights are preserved, so a negative cycle prevents such a potential
on the entire relevant graph. Finding useful potentials quickly is a central
part of the problem; paying O(nm) to obtain them does not yield a Dijkstra-time
single-query algorithm.

A credible next research experiment is SCC decomposition with local cycle
checks and local signed-weight solvers. Every cycle is inside one strongly
connected component, and the condensation is a DAG. The parameter to measure
is work inside difficult components, not just repeated visits globally. A graph
with one large SCC still needs a general solver, so an improved bound requires
a new argument there. Any proposed discovery needs a precise model, proof,
adversarial tests, and comparison with these results before a novelty claim.

## Build and reproduce

```sh
clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined shortest_paths_test.cpp -o /tmp/shortest_paths_test
/tmp/shortest_paths_test
clang++ -std=c++17 -O2 -Wall -Wextra -Werror shortest_paths_demo.cpp -o /tmp/shortest_paths_demo
printf '3 3 0\n0 1 1\n1 2 -5\n2 0 2\n' | /tmp/shortest_paths_demo
clang++ -std=c++17 -O2 -Wall -Wextra -Werror shortest_paths_benchmark.cpp -o /tmp/shortest_paths_benchmark
/tmp/shortest_paths_benchmark > verified_benchmark_results.csv
```

Tests compare three solver configurations with an independent Floyd-Warshall
oracle on 12,000 seeded small random graphs. Regressions cover the false counter
verdict, early ring detection, forced fallback, disconnected negative cycles,
self-loops, parallel edges, zero/positive cycles, and large signed weights.

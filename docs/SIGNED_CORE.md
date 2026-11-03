# BranchCore: an exact structural shortest-path experiment

Research date: 3 November 2026. Main implementation:
[`include/branch_core.hpp`](../include/branch_core.hpp).
See also the [session record](SESSION_NOTES.md) for the progression from the
original hypothesis, validation evidence, and open research questions.

We built and tested a candidate algorithm, derived a structural running-time
bound, and checked related research. This is not a verified first discovery or
a new general asymptotic record. SCC decomposition, path contraction,
reweighting, and Bellman-Ford/Dijkstra hybrids have substantial prior art.
Giving a composition a new name does not establish scientific novelty.

The useful result here is concrete: arbitrary signed weights can be handled
with repeated work confined to a small branching core. The algorithm returns
finite/unreachable distances, or an original-edge source-reachable negative
cycle. It does not classify all vertices with distance minus infinity.

## Main idea: remove forced continuation, not repeated visits

Process strongly connected components (SCCs) in condensation topological order.
All cycles are inside SCCs. Negative edges between components need just one
relaxation once their source component has been solved.

Inside a nontrivial SCC every vertex has an internal outgoing edge. If a vertex
has exactly one, a walk continuing beyond it has no choice. Follow that forced
continuation until the next vertex with multiple outgoing edges. Keep the
branching vertices and replace each branch's outgoing edge plus its forced
suffix by one weighted edge. We sum signed weights exactly; we do not assume
these sums are nonnegative.

If there are no branching vertices, strong connectivity implies the component
is a single directed ring. Retain one anchor vertex; the ring becomes one
self-loop with its total weight. An isolated edgeless vertex needs no core.

Unlike the initial heap experiment, this main solver uses no Dijkstra or heap.
It runs multi-source Bellman-Ford on the compressed core.

### Multiple entries and distances to removed vertices

An SCC can receive different finite entry labels at many vertices. Its source
can itself be a removed vertex. Both cases must be preserved.

For each removed vertex `v`, precompute its first retained successor `t(v)` and
the signed cost `L(v)` of that forced suffix. Initialize the core label of
`t(v)` with `entry(v) + L(v)` for every finite entry label, taking the minimum.
For retained vertices use `t(v)=v`, `L(v)=0`.

After solving the core, retain the original entry labels at removed vertices.
Assign the final core distances to retained vertices, relax their original
outgoing edges, then sweep removed vertices in topological order, relaxing
their sole outgoing edge. This reconstructs every distance, including paths
that stop before reaching their first retained vertex.

### Linear construction, without expanding every shortcut

The graph induced by removed vertices is acyclic: a cycle made entirely of
forced vertices could not escape to a retained vertex, contradicting strong
connectivity. Kahn's algorithm gives its topological ordering. A reverse sweep
computes all successor/cost pairs once, even where several paths merge.

Each compressed edge stores only its first original edge. Expansion follows
the original unique successors and is needed only for a returned cycle.
Materializing every compressed path during construction could duplicate long
shared suffixes; this implementation avoids that cost.

## Correctness argument

1. **Core walk equivalence.** Every walk between consecutive retained vertices
   takes one outgoing edge and then its unique forced suffix. This corresponds
   to exactly one compressed edge of equal weight. Conversely every compressed
   edge expands to such a walk. This establishes equal walk costs between
   retained vertices.

2. **Entry equivalence.** Any local walk reaching its first retained vertex
   follows the forced suffix from its entry, or starts at a retained vertex.
   Aggregating `entry(v)+L(v)` therefore preserves all possibilities. No local
   walk can leave an SCC and return to it, because the condensation is a DAG.

3. **Cycle equivalence.** Every original local cycle meets a retained vertex;
   the pure-ring case is covered by its anchor. Compressing its segments gives
   an equal-weight closed core walk. A negative closed walk contains a negative
   simple cycle. Conversely a negative core cycle expands to a negative
   original closed walk. For a simple core cycle, expanded forced suffixes
   cannot share an interior vertex: shared suffixes end at the same retained
   successor, forcing that successor to be visited twice. Thus the extracted
   simple core cycle expands to a simple original cycle. Its weight is summed
   and checked again before returning it.

4. **Recovery equivalence.** A walk ending at a removed vertex either has not
   reached a retained vertex yet, or has a last retained vertex. Its remaining
   portion is in the forced DAG. Preserved original entry labels and finalized
   outgoing core labels seed exactly these two cases; one topological sweep
   computes their minimum.

5. **Global induction.** Earlier SCCs have already finalized all incoming
   labels. The local argument finalizes the current SCC or provides a reachable
   negative-cycle witness. One relaxation of each outgoing condensation edge
   establishes the invariant for later SCCs. Unreachable SCCs are skipped.

## Structural bound we derived for this implementation

For each reachable component C let `n_C,m_C` count original vertices/internal
edges, `b_C` count vertices with internal outdegree greater than one, and `t_C`
count original outgoing edges of those branching vertices.

For a component with branches, the compressed graph has `b_C` vertices and
`t_C` edges. Construction and distance recovery are linear. Bellman-Ford costs
`O(b_C*t_C)`. Rings have a one-vertex, one-edge core; isolated vertices are
handled directly. Total time is therefore

```
O(n + m + sum_C retained_vertices_C * core_edges_C)
```

and auxiliary space is `O(n+m)`, including an expanded certificate.

There is a sharper interpretation using internal excess `q_C=m_C-n_C+1`.
For an SCC with branches, all other vertices have exactly one outgoing edge:

```
t_C = m_C - (n_C-b_C) = q_C-1+b_C
2*b_C <= t_C, hence b_C <= q_C-1
t_C <= 2*(q_C-1)
b_C*t_C <= 2*(q_C-1)^2
```

Also `b_C<=n_C` and `t_C<=m_C`. Thus another bound is

```
O(n + m + sum_{C:q_C>=2} min(n_C*m_C, 2*(q_C-1)^2))
```

Ring components have `q_C=1` and are already covered by linear preprocessing.
In particular, constant internal excess per SCC gives **linear total time**,
even with signed weights and negative cycles. This is a structural guarantee,
not linear time for arbitrary graphs. If nearly every vertex branches, the
core can be the original graph and worst-case time remains `O(n+m+nm)`.

The bound counts exact arithmetic operations. The C++ implementation accepts
signed 64-bit integer edge weights and uses checked signed 128-bit arithmetic,
including compressed weights. It throws on overflow. It is not a certified
floating-point implementation of the real-weight problem.

## Earlier candidate retained for comparison

`signed_core.hpp` implements a different experiment: local tree potentials and
negative-edge phases. For a directed out-tree define `h(v)` as its root-to-v
weight; `w'(u,v)=w(u,v)+h(u)-h(v)` makes its tree edges zero. Compare this with
zero potential and choose the one with fewer negative-edge tail vertices.

If `r_C` tails remain, each simple path uses at most `r_C` negative edges.
Perform an initial nonnegative-edge Dijkstra closure. Each subsequent round
relaxes negative edges from a **frozen** previous distance vector, then performs
another nonnegative closure. By induction the labels after round h are the
best entry-to-vertex walk costs with at most h negative reduced edges.

Without negative cycles, deleting nonnegative cycles leaves a simple optimal
path, so round `r_C` suffices. A further improvement at round `r_C+1` therefore
proves a negative cycle exists. This counts negative edges on represented
walks, not arbitrary visits or improvements.

For a witness, completed layers save their nonnegative parent forest and each
negative seed's reference to the previous layer. Recover the historical
improving walk. If all its simple cycles were nonnegative, erasing them would
produce a path using at most `r_C` negative edges with no larger cost,
contradicting improvement over round `r_C`. Erase nonnegative cycles until a
negative one is found. Optional current-predecessor scans can find a witness
earlier; they explicitly check the cycle's original weight.

The phase backend takes
`O((r_C+2)*(n_C+m_C)*log(n_C+m_C+2))` time and
`O(m_C+(r_C+2)*n_C)` space for history. Tree zeroing gives
`r_C<=min(n_C,q_C)`; it does not guarantee small r in general. A static comparison
of these work estimates with local Bellman-Ford selects the latter when its
worst-case bound is better. There is no claim that this predicts wall time.
The default retains the `O(n+m+nm)` general worst-case option.

This first candidate has substantial overhead in our measurements. BranchCore
removes its heaps and history and has the cleaner structural bound above.

## Prior-art review and novelty audit

These are primary papers inspected during this experiment. The modern results
in the last two rows are reported claims in preprints, not independently
verified implementations or comparisons with our prototype.

| Work | Relevance and consequence for novelty |
|---|---|
| [Cherkassky and Goldberg, Negative-cycle detection algorithms, 1999](https://www.imada.sdu.dk/u/jbj/DM817/Negativicyclefinding.pdf) | Studies shortest-path labeling combined with cycle detection, including amortized predecessor searches. Periodic parent-cycle checks are prior art. |
| [Dinitz and Itzhak, Hybrid Bellman–Ford–Dijkstra algorithm, 2017](https://www.sciencedirect.com/science/article/pii/S1570866717300011) | Repeated Dijkstra with few negative edges on shortest paths is already an established technique. |
| [Cassis et al., Algorithm Engineering of SSSP with Negative Edge Weights, SEA 2025](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.SEA.2025.10) | Includes SCC preprocessing, potentials, and LazyDijkstra; SCC plus hybrid solving is not itself new. It also compares against Goldberg–Radzik, which our small benchmark does not implement. |
| [Dietzfelbinger and Jaberi, On testing single connectedness in directed graphs and some related problems, 2014/2015](https://arxiv.org/abs/1412.1639) | Uses SCC contraction and elimination of indegree/outdegree-one vertices for a different graph problem. Degree-one contraction itself is established. |
| [Atalig et al., Lower Bounds for Adaptive Relaxation-Based Algorithms for SSSP, ISAAC 2024](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.ISAAC.2024.8) | Proves cubic lower bounds in a restricted query/relaxation model. This is not an unconditional lower bound for all shortest-path algorithms. It warns against assuming queue scheduling alone gives a general breakthrough. |
| [Li and Mowry, A Bottom-Up Algorithm for Negative-Weight SSSP with Integrated Negative Cycle Finding, 2024](https://arxiv.org/abs/2411.19449) | Already integrates negative-cycle finding into a Bellman-Ford/Dijkstra hybrid; integrated certification is not a novelty claim here. |
| [Hair, Li, Li, Zhang, Bellman-Ford in Almost-Linear Time, July 2026](https://arxiv.org/html/2607.19346v1) | Claims Las Vegas randomized `m^(1+o(1))` time for real-weighted directed SSSP. Sparse shortcutting and its unfolding lemma control graph growth. Our worst-case bound does not improve this result. |
| [Duong, Size-Sensitive Padded Decompositions for Faster Deterministic Negative-Weight Shortest Paths, September 2026](https://arxiv.org/html/2609.05590v2) | Theorem 1.1 claims deterministic `O((m+n log log n) log^2 n log(nW))` time for integral weights at least -W, returning distances or a negative cycle. Our structural algorithm is not a better general integer-weight bound. |

The derived bounds, multi-entry reduction, recovery procedure and certificate
expansion are the contributions of this project experiment. We have not
established that these have never appeared in prior work. The search found
related primitives and is not an exhaustive novelty review. A defensible
paper would need a closer comparison with existing reductions and parameterized
shortest-path results, a distinctive theorem or engineering advantage, and a
broader evaluation against strong public implementations.

## Tests and reproducibility

```sh
make build/signed_core_test build/branch_core_demo build/signed_core_benchmark
build/signed_core_test
printf '3 3 0\n0 1 1\n1 2 -5\n2 0 2\n' | build/branch_core_demo
build/signed_core_benchmark > /tmp/signed_core_results.csv
```

Run these commands from the repository root. The Makefile enables sanitizers
for tests; the `/tmp` output preserves the original measurements in `data/`.

Input uses zero-based vertices: `n m source`, then m lines `from to weight`.

Tests use an independent Floyd-Warshall oracle on 4,096 exhaustive three-vertex
graphs for every source, plus 20,000 seeded random multigraphs with self-loops.
Seven solver configurations are compared, including BranchCore and ablations
of the earlier candidate. Another 500 randomly subdivided cores test long
forced paths, conservative potential-generated weights and cycle expansion.
Regressions cover multiple SCC entries, unreachable negative cycles, zero
cycles, extreme integer weights, the original false visit-counter verdict,
a 20,000-vertex condensation chain and a 1,000-vertex ring.

All passed under AddressSanitizer and UndefinedBehaviorSanitizer with compiler
warnings treated as errors. Tests support the implementation; the proof gives
the general argument.

The benchmark uses seed 20261103, seven-run medians, rotated execution order,
and complete preprocessing in solver time. Generation and answer validation
are excluded. It checks all distances/status against Bellman-Ford and checks
original-edge witness connectivity, reachability and negativity. CSV edge
scans count relaxations/candidate scans, not all preprocessing operations;
one ring-core scan still requires linear input processing and expansion.
The recorded build used Apple Clang 21.0.0 with `-O2` on Darwin arm64.

Selected measured medians for n=1,000, in milliseconds (local machine):

| Family | Retained core vertices/edges | BranchCore | Earlier portfolio | Bellman-Ford |
|---|---:|---:|---:|---:|
| Negative ring | 1 / 1 | 0.153500 | 0.047250 | 3.032458 |
| Sparse branching core | 10 / 20 | 0.144750 | 0.070083 | 1.814375 |
| Subdivided random kernel | 10 / 40 | 0.246500 | 0.132916 | 0.339584 |
| Signed random potential | 1,000 / 6,000 | 1.198416 | 1.498000 | 0.206959 |
| Nonnegative random | 1,000 / 6,000 | 0.986041 | 0.424125 | 0.172416 |
| Positive-sum signed ring | 1 / 1 | 0.133709 | 0.040416 | 0.006709 |

In particular, compressing a ring does not imply beating Bellman-Ford on it:
the positive-sum ring is already solved in two ordered passes by the reference.
Preprocessing dominates our prototype there. The general structural bound is
an advantage on an input class, not a prediction of every individual timing.

[`data/signed_core_results.csv`](../data/signed_core_results.csv) contains all measured families, solvers and sizes,
including losses. These synthetic tests are diagnostic, not evidence of
universal dominance. The earlier portfolio is faster on many of them;
Bellman-Ford wins several small random instances. BranchCore's defensible
advantage is its structural bound and reduced core size, with the measured
wall-time scope recorded in the CSV.

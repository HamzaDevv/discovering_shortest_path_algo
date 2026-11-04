# Adaptive core trial

This experiment adds an inexpensive probe before BranchCore. The solver in
[`include/adaptive_core.hpp`](../include/adaptive_core.hpp) runs up to two
complete, in-place Bellman-Ford passes. If a pass makes no changes, it returns
the distances. If the nth pass changes a label (possible within the probe for
one or two vertices), it returns a checked predecessor-cycle certificate.
Otherwise it restarts using BranchCore; exhausting the probe is not a cycle
verdict. The result's `method` and `used_fallback` expose the chosen route.

Every label represents a source-rooted walk. When a full pass makes no change,
the edge relaxation inequalities hold, so the labels are shortest distances;
a reachable negative cycle would contradict those inequalities. The nth-pass
condition is the usual Bellman-Ford cycle criterion. BranchCore supplies the
existing guarantee after a cutoff. Checked 128-bit arithmetic and the original
edge-index certificate are shared with the other solvers.

The probe costs O(n+m) time and O(n) auxiliary space. The combined worst-case
time and space bounds remain those of BranchCore:
`O(n+m+sum_C k_C*t_C)` and `O(n+m)`. The speedup depends on input edge order;
two passes add work when the fallback is needed. This is an engineering trial,
not a claim of a new asymptotic record or a proof of scientific novelty.

## Results

The [saved measurements](../data/adaptive_trial_results.csv) use the existing
benchmark's seeded eight graph families, sizes 100/500/1000, seven methods,
seven-run medians, rotated execution order, and answer and cycle validation.
Solver timing includes preprocessing. Apple Clang 21.0.0, `-O2`, macOS arm64;
times vary by machine and run. Selected n=1000 medians in milliseconds:

| Family | Adaptive | BranchCore | Earlier portfolio | Bellman-Ford |
|---|---:|---:|---:|---:|
| Positive-sum signed ring | 0.005833 | 0.107875 | 0.032083 | 0.005833 |
| Negative ring | 0.117417 | 0.115792 | 0.037000 | 2.309625 |
| Signed random potential | 0.613083 | 0.575709 | 0.797209 | 0.105958 |
| Nonnegative random | 0.633417 | 0.574792 | 0.262667 | 0.109333 |
| Sparse branching core | 0.106750 | 0.112000 | 0.053208 | 1.225167 |

The probe substantially improves the favorable ordered ring, while adding
overhead to some cases that fall back. Small timing differences should not be
interpreted as significant; the earlier portfolio or Bellman-Ford remains
faster on several families. The benchmark does not compare against modern
research implementations or establish a universally best solver.

`make run-tests` passed with AddressSanitizer and UndefinedBehaviorSanitizer.
The independent Floyd-Warshall oracle checks the adaptive solver alongside the
existing configurations on all 4,096 exhaustive three-vertex graphs for every
source and 20,000 seeded random multigraphs. Focused tests also cover early
convergence, fallback, and one- and two-vertex negative cycles. The separate
500 subdivided-core checks remain specific to BranchCore.

To reproduce without replacing the recorded data:

```sh
make run-tests benchmarks
build/signed_core_benchmark > /tmp/adaptive_trial_results.csv
```

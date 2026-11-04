#include "adaptive_core.hpp"
#include "competitive_paths.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <random>

using namespace shortest_paths;

// Independent all-pairs oracle. Validate original-edge cycle certificates,
// including connectivity and reachability, rather than trusting solver status.
void verify(int n, const std::vector<Edge>& edges, int source, const Result& r) {
    std::vector<std::vector<std::optional<Distance>>> d(
        n, std::vector<std::optional<Distance>>(n));
    for (int u = 0; u < n; ++u) d[u][u] = 0;
    for (auto e : edges)
        if (!d[e.from][e.to] || *d[e.from][e.to] > e.weight)
            d[e.from][e.to] = e.weight;
    for (int k = 0; k < n; ++k)
        for (int u = 0; u < n; ++u)
            for (int v = 0; v < n; ++v)
                if (d[u][k] && d[k][v]) {
                    auto x = *d[u][k] + *d[k][v];
                    if (!d[u][v] || x < *d[u][v]) d[u][v] = x;
                }
    bool negative = false;
    for (int u = 0; u < n; ++u)
        negative = negative || (d[source][u] && *d[u][u] < 0);
    assert(negative == !r.negative_cycle.empty());
    if (!negative) { assert(r.distances == d[source]); return; }
    assert(r.distances.empty());
    Distance sum = 0;
    for (std::size_t j = 0; j < r.negative_cycle.size(); ++j) {
        const auto& e = edges.at(r.negative_cycle[j]);
        const auto& next = edges.at(r.negative_cycle[(j+1)%r.negative_cycle.size()]);
        assert(e.to == next.from);
        assert(d[source][e.from]);
        sum += e.weight;
    }
    assert(sum < 0);
}

const std::array<signed_core::Options, 6> configurations = {{
    {}, {true,true,true,false}, {true,true,false,false},
    {true,false,false,false}, {false,false,false,false}, {false,false,true,true}
}};
void all(int n, const std::vector<Edge>& edges, int source) {
    for (auto options : configurations)
        verify(n, edges, source, signed_core::solve(n, edges, source, options).answer);
    verify(n, edges, source, branch_core::solve(n, edges, source).answer);
    verify(n, edges, source, adaptive_core::solve(n, edges, source));
    verify(n, edges, source, competitive_paths::solve(n, edges, source));
}

// Exercise every bounded engine even when another engine would win the race.
// A cutoff is permitted; a returned result must pass the independent oracle.
void bounded_engines(int n, const std::vector<Edge>& edges, int source,
                     std::size_t budget) {
    competitive_paths::detail::Adjacency adj(n);
    for (std::size_t i = 0; i < edges.size(); ++i) adj[edges[i].from].push_back(i);
    for (int engine = 0; engine < 4; ++engine) {
        competitive_paths::detail::Meter meter{budget};
        Result r;
        try {
            if (engine == 0)
                competitive_paths::detail::bellman_ford(n, edges, source, meter, r);
            if (engine == 1)
                competitive_paths::detail::fifo(n, edges, adj, source, meter, r);
            if (engine == 2)
                competitive_paths::detail::heap(n, edges, adj, source, meter, r);
            if (engine == 3) r = branch_core::solve(n, edges, source, budget).answer;
            verify(n, edges, source, r);
        } catch (const competitive_paths::detail::BudgetExceeded&) {
            assert(meter.used <= budget);
            assert(r.edge_scans <= budget);
        } catch (const branch_core::ScanBudgetExceeded& exhausted) {
            assert(exhausted.edge_scans == budget);
        }
    }
}

void competitive_regressions() {
    // Empty graph and singleton: no edge work, even with a zero scan budget.
    all(1, {}, 0);
    bounded_engines(1, {}, 0, 0);
    // Indexed decrease-key while queued, then reopening a previously popped
    // vertex on a signed graph. Self loops and parallel edges are distinct.
    for (const auto& edges : std::vector<std::vector<Edge>>{
            {{0,1,5},{0,2,1},{2,1,1},{1,3,3}},
            {{0,1,1},{0,2,5},{2,1,-10},{1,3,0}},
            {{0,0,-1},{0,1,0},{0,1,2},{1,2,0},{2,3,0}},
            {{0,1,2},{1,0,-2},{2,3,-3},{3,2,1}}}) {
        all(4, edges, 0);
        for (auto budget : {std::size_t(0), std::size_t(1), std::size_t(10000)})
            bounded_engines(4, edges, 0, budget);
    }
    const std::vector<Edge> ring = {{0,1,-1},{1,2,-1},{2,0,3}};
    auto full = branch_core::solve(3, ring, 0).answer;
    auto exact = branch_core::solve(3, ring, 0, full.edge_scans).answer;
    assert(exact.distances == full.distances);
    bool exhausted = false;
    try { branch_core::solve(3, ring, 0, full.edge_scans - 1); }
    catch (const branch_core::ScanBudgetExceeded& e) {
        exhausted = true;
        assert(e.edge_scans == full.edge_scans - 1);
    }
    assert(exhausted);

    // Repeated improvements on a DAG must trigger another engine, never a
    // cycle verdict. Reverse chain order also forces geometric budget growth.
    std::vector<Edge> dag;
    constexpr int n = 30;
    for (int u = n - 1; u > 0; --u) dag.push_back({0,u,-u});
    for (int u = n - 2; u > 0; --u) dag.push_back({u,u+1,-2});
    competitive_paths::Statistics stats;
    auto answer = competitive_paths::solve(n, dag, 0, &stats);
    verify(n, dag, 0, answer);
    assert(answer.negative_cycle.empty() && answer.method == "competitive-dag");
    std::vector<Edge> cyclic;
    for (int u = n - 2; u >= 0; --u) {
        cyclic.push_back({u,u+1,-1});
        cyclic.push_back({u+1,u,2});
    }
    answer = competitive_paths::solve(n, cyclic, 0, &stats);
    verify(n, cyclic, 0, answer);
    assert(answer.negative_cycle.empty() && stats.budget_exhaustions > 0);
    assert(answer.edge_scans <= stats.metered_work);
    assert(stats.attempts <= 4 * stats.rounds);

    // Indexed heap has O(n) pending entries even after repeated decreases.
    std::vector<std::optional<Distance>> labels(7);
    competitive_paths::detail::Meter meter{100000};
    competitive_paths::detail::IndexedHeap heap(7, labels, meter);
    for (int u = 0; u < 7; ++u) { labels[u] = 100 + u; heap.decrease(u); }
    for (int u = 6; u >= 0; --u) { labels[u] = -u; heap.decrease(u); }
    for (int u = 6; u >= 0; --u) assert(heap.pop() == u);
    assert(heap.empty());
    labels[6] = -100;
    heap.decrease(6);
    assert(heap.pop() == 6 && heap.empty());
}

int main() {
    competitive_regressions();
    all(4, {{0,1,-10},{0,2,-20},{0,3,-25},
            {1,2,-20},{1,3,-22},{2,3,-10}}, 0);
    {
        auto fast = adaptive_core::solve(3, {{0,1,1},{1,2,2}}, 0);
        assert(!fast.used_fallback && *fast.distances[2] == 3);
        auto fallback = adaptive_core::solve(4, {{2,3,1},{1,2,1},{0,1,1}}, 0);
        assert(fallback.used_fallback && *fallback.distances[3] == 3);
        all(1, {{0,0,-1}}, 0);
        all(2, {{0,1,-1},{1,0,0}}, 0);
    }
    all(6, {{0,1,3},{1,2,-7},{2,1,2},{3,4,-10},{4,3,1}}, 0);
    all(6, {{0,1,3},{1,2,7},{2,1,-2},{3,4,-10},{4,3,1}}, 0);
    all(4, {{0,0,0},{0,1,-7},{0,1,-2},{1,0,7},{1,2,-8},{2,3,1},{3,2,0}}, 1);
    const auto high = std::numeric_limits<std::int64_t>::max();
    all(4, {{0,1,high},{1,2,high},{2,3,-1}}, 0);
    all(3, {{0,1,std::numeric_limits<std::int64_t>::min()},
            {1,2,-1},{2,0,high}}, 0);
    // Multiple external entry labels into a forced ring and a merging core.
    all(5, {{0,2,-20},{0,4,7},{1,2,-1},{2,3,-1},{3,4,-1},{4,1,4}}, 0);
    all(7, {{0,2,-8},{0,6,2},{1,2,-1},{1,3,2},{2,3,1},{3,4,-2},
            {4,5,3},{4,6,1},{5,1,1},{6,1,4}}, 0);
    bool rejected = false;
    try { signed_core::solve(0, {}, 0); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try { competitive_paths::solve(2, {{0,2,1}}, 0); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try { signed_core::solve(2, {{0,2,1}}, 0); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    // Reweighting concentrates a completely negative ring into one tail.
    std::vector<Edge> ring;
    for (int u = 0; u < 1000; ++u) ring.push_back({u,(u+1)%1000,-1});
    auto output = signed_core::solve(1000, ring, 0);
    assert(output.statistics.raw_negative_tails == 1000);
    assert(output.statistics.remaining_negative_tails == 1);
    assert(output.statistics.negative_rounds == 1);
    assert(output.answer.negative_cycle.size() == 1000);
    auto compressed = branch_core::solve(1000, ring, 0);
    assert(compressed.statistics.core_vertices == 1);
    assert(compressed.statistics.core_edges == 1);
    assert(compressed.answer.negative_cycle.size() == 1000);
    // Disable the early parent scan to exercise the historical walk proof.
    output = signed_core::solve(1000, ring, 0, {true,true,false,false});
    assert(output.statistics.negative_rounds == 2);
    assert(output.statistics.history_certificates == 1);
    assert(output.answer.negative_cycle.size() == 1000);

    // Same topology, positive cycle sum: many raw negative edges, zero after
    // tree reweighting. No negative-edge round is necessary.
    ring.back().weight = 1000;
    output = signed_core::solve(1000, ring, 0);
    assert(output.statistics.remaining_negative_tails == 0);
    assert(output.statistics.negative_rounds == 0);
    assert(output.answer.negative_cycle.empty());
    for (int u = 0; u < 1000; ++u) assert(*output.answer.distances[u] == -u);
    compressed = branch_core::solve(1000, ring, 0);
    for (int u = 0; u < 1000; ++u) assert(*compressed.answer.distances[u] == -u);

    // Deep condensation: iterative DFS must not overflow the call stack.
    std::vector<Edge> chain;
    for (int u = 19999; u > 0; --u) chain.push_back({u-1,u,-1});
    output = signed_core::solve(20000, chain, 0);
    assert(output.statistics.components == 20000);
    assert(*output.answer.distances.back() == -19999);

    // Exhaust all loop-free directed 3-vertex graphs with each edge absent,
    // -1, 0, or +1, for every source: 4096 graphs, 12288 source problems.
    for (int encoded = 0; encoded < 4096; ++encoded) {
        int x = encoded;
        std::vector<Edge> edges;
        for (int u = 0; u < 3; ++u) for (int v = 0; v < 3; ++v) {
            if (u == v) continue;
            int digit = x%4; x /= 4;
            if (digit) edges.push_back({u,v,digit-2});
        }
        for (int source = 0; source < 3; ++source) all(3, edges, source);
    }
    std::mt19937 random(20261103);
    std::mt19937 transformations(20261107);
    bool observed_restart = false;
    for (int trial = 0; trial < 20000; ++trial) {
        int n = 1 + random()%10;
        int m = random()%(2*n*n+1);
        std::vector<Edge> edges;
        for (int j = 0; j < m; ++j)
            edges.push_back({int(random()%n), int(random()%n), int(random()%41)-20});
        all(n, edges, random()%n);
        if (trial < 2000) {
            int source = transformations()%n;
            bounded_engines(n, edges, source, 64 * n * (n + edges.size() + 1));
            competitive_paths::Statistics stats;
            auto original = competitive_paths::solve(n, edges, source, &stats);
            observed_restart = observed_restart || stats.rounds > 1;
            assert(stats.metered_work <= 8 * stats.final_budget);
            std::vector<int> permutation(n);
            for (int u = 0; u < n; ++u) permutation[u] = u;
            std::shuffle(permutation.begin(), permutation.end(), transformations);
            std::vector<std::int64_t> potential(n);
            for (auto& p : potential) p = int(transformations()%2001) - 1000;
            auto transformed = edges;
            for (auto& e : transformed) {
                e.weight += potential[e.from] - potential[e.to];
                e.from = permutation[e.from];
                e.to = permutation[e.to];
            }
            std::shuffle(transformed.begin(), transformed.end(), transformations);
            auto answer = competitive_paths::solve(n, transformed, permutation[source]);
            verify(n, transformed, permutation[source], answer);
            assert(original.negative_cycle.empty() == answer.negative_cycle.empty());
            if (original.negative_cycle.empty()) for (int u = 0; u < n; ++u) {
                assert(original.distances[u].has_value() ==
                       answer.distances[permutation[u]].has_value());
                if (original.distances[u])
                    assert(*answer.distances[permutation[u]] == *original.distances[u] +
                           potential[source] - potential[u]);
            }
        }
    }
    assert(observed_restart);
    // Random subdivisions explicitly stress long forced paths and expansion.
    // Half are conservative by potential generation; half may contain cycles.
    for (int trial = 0; trial < 500; ++trial) {
        int roots = 2 + random()%4, n = roots;
        std::vector<Edge> edges;
        for (int u = 0; u < roots; ++u) for (int j = 0; j < 2; ++j) {
            int end = j == 0 ? (u+1)%roots : int(random()%roots);
            int previous = u, length = 1 + random()%4;
            for (int k = 0; k < length; ++k) {
                int next = k+1 == length ? end : n++;
                edges.push_back({previous,next,int(random()%21)-10});
                previous = next;
            }
        }
        if (trial%2 == 0) {
            std::vector<std::int64_t> potential(n);
            for (auto& p : potential) p = int(random()%101)-50;
            for (auto& e : edges)
                e.weight = 1 + int(random()%10) + potential[e.to] - potential[e.from];
        }
        int source = random()%n;
        verify(n, edges, source, branch_core::solve(n, edges, source).answer);
    }
    std::cout << "PASS: regressions, 4096 exhaustive graphs (all sources), "
                 "20000 seeded random graphs; nine configurations vs Floyd-Warshall; "
                 "500 random subdivided cores; 2000 bounded-engine and metamorphic trials\n";
}

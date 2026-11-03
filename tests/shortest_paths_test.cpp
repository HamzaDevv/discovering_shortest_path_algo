#include "shortest_paths.hpp"
#include <cassert>
#include <iostream>
#include <random>

using namespace shortest_paths;

// Independent Floyd-Warshall oracle: small graphs only.
void check(int n, const std::vector<Edge>& edges, int source,
           const Result& result) {
    using Cell = std::optional<Distance>;
    std::vector<std::vector<Cell>> d(n, std::vector<Cell>(n));
    for (int u = 0; u < n; ++u) d[u][u] = 0;
    for (auto e : edges)
        if (!d[e.from][e.to] || e.weight < *d[e.from][e.to])
            d[e.from][e.to] = e.weight;
    for (int k = 0; k < n; ++k)
        for (int u = 0; u < n; ++u)
            for (int v = 0; v < n; ++v)
                if (d[u][k] && d[k][v]) {
                    Distance candidate = *d[u][k] + *d[k][v];
                    if (!d[u][v] || candidate < *d[u][v]) d[u][v] = candidate;
                }
    bool negative = false;
    for (int u = 0; u < n; ++u)
        if (d[source][u] && *d[u][u] < 0) negative = true;
    assert(negative == !result.negative_cycle.empty());
    if (!negative) {
        assert(result.distances == d[source]);
        return;
    }
    assert(result.distances.empty());
    Distance weight = 0;
    const auto& cycle = result.negative_cycle;
    for (std::size_t i = 0; i < cycle.size(); ++i) {
        assert(cycle[i] < edges.size());
        const auto& e = edges[cycle[i]];
        const auto& next = edges[cycle[(i + 1) % cycle.size()]];
        assert(e.to == next.from);
        assert(d[source][e.from]);
        weight += e.weight;
    }
    assert(weight < 0);
}

int main() {
    // Four improvements of vertex 3, but no cycle: regression for raw counters.
    std::vector<Edge> dag = {{0,1,-10},{0,2,-20},{0,3,-25},
                             {1,2,-20},{1,3,-22},{2,3,-10}};
    auto r = solve(4, dag, 0);
    assert(r.method == "dag");
    assert(*r.distances[3] == -40);
    check(4, dag, 0, r);
    // Reachable, unreachable, zero, and positive cycles; parallel edges.
    std::vector<std::vector<Edge>> cases = {
        {{0,1,1},{1,2,-5},{2,0,2}},
        {{0,1,7},{2,3,-4},{3,2,1}},
        {{0,1,-1},{1,0,1}},
        {{0,1,-1},{1,0,2}},
        {{0,1,9},{0,1,4},{0,1,-2},{1,0,1}},
        {{0,0,-1}}, {{0,0,0}}, {}
    };
    for (const auto& edges : cases) {
        check(4, edges, 0, solve(4, edges, 0));
        check(4, edges, 0, solve(4, edges, 0, 0));
        check(4, edges, 0, bellman_ford(4, edges, 0));
    }
    assert(solve(2, {{0,1,2},{1,0,2}}, 0).method == "dijkstra");
    assert(solve(2, {{0,1,-1},{1,0,2}}, 0, 0).used_fallback);
    const auto high = std::numeric_limits<std::int64_t>::max();
    auto large = solve(3, {{0,1,high},{1,2,high}}, 0);
    assert(decimal(*large.distances[2]) == "18446744073709551614");
    check(2, {{0,1,std::numeric_limits<std::int64_t>::min()},{1,0,-1}},
          0, solve(2, {{0,1,std::numeric_limits<std::int64_t>::min()},
                       {1,0,-1}}, 0));
    bool rejected = false;
    try { solve(1, {{0,1,0}}, 0); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    // Long negative ring: one circuit, not n circuits, before certification.
    std::vector<Edge> ring;
    for (int u = 0; u < 1000; ++u) ring.push_back({u,(u+1)%1000,-1});
    r = solve(1000, ring, 0);
    assert(r.negative_cycle.size() == 1000);
    assert(r.edge_scans == 1000 && !r.used_fallback);

    std::mt19937 random(20261103);
    for (int trial = 0; trial < 12000; ++trial) {
        int n = 1 + random()%9;
        int m = random()%(2*n*n+1);
        std::vector<Edge> edges;
        for (int i = 0; i < m; ++i)
            edges.push_back({int(random()%n), int(random()%n),
                             int(random()%31)-15});
        int source = random()%n;
        check(n, edges, source, solve(n, edges, source));
        check(n, edges, source, solve(n, edges, source, random()%20));
        check(n, edges, source, bellman_ford(n, edges, source));
    }
    std::cout << "PASS: regressions and 12000 seeded random graphs; "
                 "three solver configurations checked against Floyd-Warshall\n";
}

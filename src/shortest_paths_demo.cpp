#include "shortest_paths.hpp"
#include <iostream>

int main() {
    using namespace shortest_paths;
    int n, m, source;
    // Input: n m source, followed by m lines: from to signed_integer_weight.
    if (!(std::cin >> n >> m >> source) || m < 0) {
        std::cerr << "Expected: n m source, then m edges (zero-based vertices)\n";
        return 1;
    }
    std::vector<Edge> edges;
    for (int i = 0; i < m; ++i) {
        Edge e;
        if (!(std::cin >> e.from >> e.to >> e.weight)) return 1;
        edges.push_back(e);
    }
    try {
        auto r = solve(n, edges, source);
        std::cout << "Method: " << r.method << "; edge scans: " << r.edge_scans
                  << "; fallback: " << r.used_fallback << '\n';
        if (!r.negative_cycle.empty()) {
            Distance weight = 0;
            std::cout << "Reachable negative cycle (edge indices):";
            for (auto i : r.negative_cycle) {
                std::cout << ' ' << i;
                weight = add(weight, edges[i].weight);
            }
            std::cout << "; total weight: " << decimal(weight) << '\n';
        } else {
            for (int v = 0; v < n; ++v)
                std::cout << v << ": " << (r.distances[v] ?
                    decimal(*r.distances[v]) : "INF") << '\n';
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

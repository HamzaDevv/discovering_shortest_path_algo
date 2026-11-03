#include "branch_core.hpp"
#include <iostream>

int main() {
    int n, m, source;
    if (!(std::cin >> n >> m >> source) || m < 0) return 1;
    std::vector<shortest_paths::Edge> edges;
    for (int j = 0; j < m; ++j) {
        shortest_paths::Edge e;
        if (!(std::cin >> e.from >> e.to >> e.weight)) return 1;
        edges.push_back(e);
    }
    try {
        auto output = branch_core::solve(n, edges, source);
        auto& r = output.answer;
        auto& s = output.statistics;
        std::cout << "Method: " << r.method << "; compressed core: " << s.core_vertices
                  << " vertices, " << s.core_edges << " edges; edge scans: "
                  << r.edge_scans << '\n';
        if (!r.negative_cycle.empty()) {
            shortest_paths::Distance sum = 0;
            std::cout << "Reachable negative cycle; original edge indices:";
            for (auto i : r.negative_cycle) {
                std::cout << ' ' << i;
                sum = shortest_paths::add(sum, edges[i].weight);
            }
            std::cout << "; weight: " << shortest_paths::decimal(sum) << '\n';
        } else {
            for (int u = 0; u < n; ++u)
                std::cout << u << ": " << (r.distances[u]
                    ? shortest_paths::decimal(*r.distances[u]) : "unreachable") << '\n';
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

#pragma once

#include "branch_core.hpp"

// Two inexpensive Bellman-Ford passes can settle graphs whose edge ordering
// already propagates shortest paths quickly. Otherwise use the structural
// solver. A cutoff is never itself a negative-cycle certificate.
namespace adaptive_core {
inline shortest_paths::Result solve(int n, const std::vector<shortest_paths::Edge>& edges,
                                    int source) {
    using namespace shortest_paths;
    detail::validate(n, edges, source);
    Result result;
    result.method = "adaptive-early-bellman-ford";
    result.distances.resize(n);
    result.distances[source] = 0;
    std::vector<std::size_t> pred(n, detail::none);
    const int limit = std::min(n, 2);
    for (int pass = 0; pass < limit; ++pass) {
        bool changed = false;
        for (std::size_t i = 0; i < edges.size(); ++i) {
            ++result.edge_scans;
            const auto& e = edges[i];
            if (!result.distances[e.from]) continue;
            auto candidate = add(*result.distances[e.from], e.weight);
            if (!result.distances[e.to] || candidate < *result.distances[e.to]) {
                result.distances[e.to] = candidate;
                pred[e.to] = i;
                changed = true;
            }
        }
        if (!changed) return result;
        if (pass + 1 == n) {
            ++result.cycle_checks;
            result.negative_cycle = detail::certificate(edges, pred);
            if (result.negative_cycle.empty())
                throw std::logic_error("Bellman-Ford failed to produce its cycle witness");
            result.distances.clear();
            return result;
        }
    }
    auto fallback = branch_core::solve(n, edges, source).answer;
    fallback.method = "adaptive-branch-core";
    fallback.used_fallback = true;
    fallback.edge_scans += result.edge_scans;
    return fallback;
}
} // namespace adaptive_core

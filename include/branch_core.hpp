#pragma once

// Shares the checked arithmetic, result type, and iterative SCC primitive with
// the earlier experiment. No heap or potential computation is used here.
#include "signed_core.hpp"

namespace branch_core {
using shortest_paths::Distance;
using shortest_paths::Edge;
using shortest_paths::Result;
using shortest_paths::add;
using Cell = std::optional<Distance>;
constexpr std::size_t none = shortest_paths::detail::none;

struct Statistics {
    std::size_t components = 0, reached_components = 0, branch_vertices = 0;
    std::size_t core_vertices = 0, core_edges = 0, forced_vertices = 0;
};
struct Output { Result answer; Statistics statistics; };

namespace detail {
struct CoreEdge { int from, to; Distance weight; std::size_t first; };

// Exact multi-source SCC solver. Vertices with one internal outgoing edge have
// forced continuations; retain all other vertices, or one anchor on a ring.
inline std::vector<std::size_t> local(
        int n, const std::vector<Edge>& edges, std::vector<Cell>& distance,
        Output& output) {
    if (edges.empty()) {
        if (n != 1) throw std::logic_error("edgeless nontrivial SCC");
        return {};
    }
    std::vector<std::vector<std::size_t>> adj(n);
    for (std::size_t i = 0; i < edges.size(); ++i) adj[edges[i].from].push_back(i);
    std::vector<int> core_id(n, -1), branches;
    for (int u = 0; u < n; ++u) {
        if (adj[u].empty()) throw std::logic_error("SCC vertex has no continuation");
        if (adj[u].size() > 1) {
            core_id[u] = static_cast<int>(branches.size());
            branches.push_back(u);
        }
    }
    output.statistics.branch_vertices += branches.size();
    // A strongly connected functional graph is exactly one directed ring.
    if (branches.empty()) { branches.push_back(0); core_id[0] = 0; }
    const int b = static_cast<int>(branches.size());
    output.statistics.core_vertices += b;
    output.statistics.forced_vertices += n - b;

    // The non-core induced graph is a DAG: a closed forced ring could never
    // reach a core vertex, contradicting strong connectivity.
    std::vector<int> indegree(n, 0), order;
    for (int u = 0; u < n; ++u) if (core_id[u] < 0) {
        int v = edges[adj[u][0]].to;
        if (core_id[v] < 0) ++indegree[v];
    }
    std::queue<int> ready;
    for (int u = 0; u < n; ++u)
        if (core_id[u] < 0 && indegree[u] == 0) ready.push(u);
    while (!ready.empty()) {
        int u = ready.front(); ready.pop(); order.push_back(u);
        int v = edges[adj[u][0]].to;
        if (core_id[v] < 0 && --indegree[v] == 0) ready.push(v);
    }
    if (order.size() + branches.size() != static_cast<std::size_t>(n))
        throw std::logic_error("forced continuation failed to reach core");
    std::vector<int> target(n, -1);
    std::vector<Distance> suffix(n, 0);
    for (int u : branches) target[u] = core_id[u];
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        auto e = edges[adj[*it][0]];
        target[*it] = target[e.to];
        suffix[*it] = add(e.weight, suffix[e.to]);
    }
    std::vector<Cell> core_distance(b);
    for (int u = 0; u < n; ++u) if (distance[u]) {
        auto candidate = add(*distance[u], suffix[u]);
        auto& d = core_distance[target[u]];
        if (!d || candidate < *d) d = candidate;
    }
    std::vector<CoreEdge> core_edges;
    for (int u : branches) for (auto i : adj[u]) {
        auto e = edges[i];
        core_edges.push_back({core_id[u], target[e.to], add(e.weight, suffix[e.to]), i});
    }
    output.statistics.core_edges += core_edges.size();

    std::vector<std::size_t> pred(b, none);
    int changed = -1;
    for (int pass = 0; pass < b; ++pass) {
        changed = -1;
        for (std::size_t i = 0; i < core_edges.size(); ++i) {
            ++output.answer.edge_scans;
            const auto& e = core_edges[i];
            if (!core_distance[e.from]) continue;
            auto candidate = add(*core_distance[e.from], e.weight);
            if (!core_distance[e.to] || candidate < *core_distance[e.to]) {
                core_distance[e.to] = candidate;
                pred[e.to] = i;
                changed = e.to;
            }
        }
        if (changed < 0) break;
    }
    if (changed >= 0) {
        ++output.answer.cycle_checks;
        int u = changed;
        for (int step = 0; step < b; ++step) {
            if (pred[u] == none) throw std::logic_error("missing core predecessor");
            u = core_edges[pred[u]].from;
        }
        int start = u;
        std::vector<std::size_t> compressed_cycle;
        do {
            compressed_cycle.push_back(pred[u]);
            u = core_edges[pred[u]].from;
        } while (u != start);
        std::reverse(compressed_cycle.begin(), compressed_cycle.end());
        std::vector<std::size_t> expanded;
        Distance sum = 0;
        for (auto ci : compressed_cycle) {
            auto i = core_edges[ci].first;
            expanded.push_back(i);
            sum = add(sum, edges[i].weight);
            int v = edges[i].to;
            while (core_id[v] < 0) {
                auto next = adj[v][0];
                expanded.push_back(next);
                sum = add(sum, edges[next].weight);
                v = edges[next].to;
            }
        }
        if (sum >= 0 || expanded.size() > static_cast<std::size_t>(n))
            throw std::logic_error("invalid expanded negative cycle");
        return expanded;
    }

    // Preserve all original entry labels. Seed forced paths from finalized
    // branch distances, then propagate once in the forced DAG's order. This
    // also recovers vertices reached before the first branch on a source path.
    for (int u : branches) distance[u] = core_distance[core_id[u]];
    auto relax = [&](std::size_t i) {
        ++output.answer.edge_scans;
        const auto& e = edges[i];
        if (!distance[e.from]) return;
        auto candidate = add(*distance[e.from], e.weight);
        if (!distance[e.to] || candidate < *distance[e.to]) distance[e.to] = candidate;
    };
    for (int u : branches) for (auto i : adj[u]) relax(i);
    for (int u : order) relax(adj[u][0]);
    return {};
}
} // namespace detail

// Integer weights, exact checked 128-bit internal arithmetic. Returns finite /
// unreachable distances OR an original-edge reachable negative-cycle witness.
inline Output solve(int n, const std::vector<Edge>& edges, int source) {
    shortest_paths::detail::validate(n, edges, source);
    Output output;
    output.answer.method = "branch-core";
    output.answer.distances.resize(n);
    output.answer.distances[source] = 0;
    auto id = signed_core::detail::components(n, edges);
    int count = *std::max_element(id.begin(), id.end()) + 1;
    output.statistics.components = count;
    std::vector<std::vector<int>> vertices(count);
    std::vector<std::vector<std::size_t>> internal(count), outgoing(count);
    std::vector<int> local_id(n);
    for (int u = 0; u < n; ++u) {
        local_id[u] = static_cast<int>(vertices[id[u]].size());
        vertices[id[u]].push_back(u);
    }
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const auto& e = edges[i];
        if (id[e.from] == id[e.to]) internal[id[e.from]].push_back(i);
        else outgoing[id[e.from]].push_back(i);
    }
    for (int c = 0; c < count; ++c) {
        std::vector<Cell> entry;
        bool reachable = false;
        for (int u : vertices[c]) {
            entry.push_back(output.answer.distances[u]);
            reachable = reachable || entry.back().has_value();
        }
        if (!reachable) continue;
        ++output.statistics.reached_components;
        std::vector<Edge> local_edges;
        for (auto i : internal[c]) {
            const auto& e = edges[i];
            local_edges.push_back({local_id[e.from], local_id[e.to], e.weight});
        }
        auto cycle = detail::local(static_cast<int>(entry.size()), local_edges, entry, output);
        if (!cycle.empty()) {
            for (auto i : cycle) output.answer.negative_cycle.push_back(internal[c][i]);
            output.answer.distances.clear();
            return output;
        }
        for (std::size_t j = 0; j < entry.size(); ++j)
            output.answer.distances[vertices[c][j]] = entry[j];
        for (auto i : outgoing[c]) {
            ++output.answer.edge_scans;
            const auto& e = edges[i];
            if (id[e.to] <= c) throw std::logic_error("SCC ordering failure");
            if (!output.answer.distances[e.from]) continue;
            auto candidate = add(*output.answer.distances[e.from], e.weight);
            auto& d = output.answer.distances[e.to];
            if (!d || candidate < *d) d = candidate;
        }
    }
    return output;
}
} // namespace branch_core

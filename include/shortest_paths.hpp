#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

// Exact integer arithmetic on GCC/Clang. No floating-point tolerance is used.
namespace shortest_paths {
using Distance = __int128_t;
struct Edge { int from, to; std::int64_t weight; };
struct Result {
    // Empty on negative-cycle status; otherwise nullopt means unreachable.
    std::vector<std::optional<Distance>> distances;
    // Original edge indices, in forward cycle order. Nonempty iff a reachable
    // negative cycle was certified. Parallel edges remain distinguishable.
    std::vector<std::size_t> negative_cycle;
    std::string method;
    std::size_t edge_scans = 0, cycle_checks = 0;
    bool used_fallback = false;
};
inline Distance add(Distance a, Distance b) {
    Distance out;
    if (__builtin_add_overflow(a, b, &out))
        throw std::overflow_error("distance exceeds signed 128-bit range");
    return out;
}
inline std::string decimal(Distance x) {
    if (x == 0) return "0";
    bool negative = x < 0;
    __uint128_t magnitude = negative ? __uint128_t(-(x + 1)) + 1 : __uint128_t(x);
    std::string out;
    while (magnitude) {
        out.push_back(char('0' + magnitude % 10));
        magnitude /= 10;
    }
    if (negative) out.push_back('-');
    std::reverse(out.begin(), out.end());
    return out;
}

namespace detail {
constexpr std::size_t none = std::numeric_limits<std::size_t>::max();
inline void validate(int n, const std::vector<Edge>& edges, int source) {
    if (n <= 0 || source < 0 || source >= n)
        throw std::invalid_argument("invalid vertex count or source");
    for (const auto& e : edges)
        if (e.from < 0 || e.from >= n || e.to < 0 || e.to >= n)
            throw std::invalid_argument("edge endpoint out of range");
}

// O(n) scan of the predecessor functional graph, without recursion.
// A repetition is only a candidate: explicitly sum original edge weights.
inline std::vector<std::size_t> certificate(
        const std::vector<Edge>& edges, const std::vector<std::size_t>& pred) {
    const int n = static_cast<int>(pred.size());
    std::vector<unsigned char> color(n, 0);
    for (int start = 0; start < n; ++start) {
        if (color[start]) continue;
        int u = start;
        while (u >= 0 && color[u] == 0) {
            color[u] = 1;
            u = pred[u] == none ? -1 : edges[pred[u]].from;
        }
        if (u >= 0 && color[u] == 1) {
            std::vector<std::size_t> cycle;
            Distance weight = 0;
            int v = u;
            do {
                cycle.push_back(pred[v]);
                weight = add(weight, edges[pred[v]].weight);
                v = edges[pred[v]].from;
            } while (v != u);
            if (weight < 0) {
                std::reverse(cycle.begin(), cycle.end());
                return cycle;
            }
        }
        u = start;
        while (u >= 0 && color[u] == 1) {
            color[u] = 2;
            u = pred[u] == none ? -1 : edges[pred[u]].from;
        }
    }
    return {};
}
} // namespace detail

// Deterministic reference and fallback. Returns distances or a cycle witness.
inline Result bellman_ford(int n, const std::vector<Edge>& edges, int source) {
    detail::validate(n, edges, source);
    Result r;
    r.method = "bellman-ford";
    r.distances.resize(n);
    r.distances[source] = 0;
    std::vector<std::size_t> pred(n, detail::none);
    for (int pass = 0; pass < n; ++pass) {
        bool changed = false;
        for (std::size_t i = 0; i < edges.size(); ++i) {
            ++r.edge_scans;
            const auto& e = edges[i];
            if (!r.distances[e.from]) continue;
            Distance candidate = add(*r.distances[e.from], e.weight);
            if (!r.distances[e.to] || candidate < *r.distances[e.to]) {
                r.distances[e.to] = candidate;
                pred[e.to] = i;
                changed = true;
            }
        }
        if (!changed) return r;
    }
    ++r.cycle_checks;
    r.negative_cycle = detail::certificate(edges, pred);
    if (r.negative_cycle.empty())
        throw std::logic_error("Bellman-Ford failed to produce its cycle witness");
    r.distances.clear();
    return r;
}

// Portfolio of established techniques, not a new asymptotic SSSP algorithm.
// mixed_budget bounds edge scans before restarting with Bellman-Ford.
// Zero forces fallback on mixed cyclic graphs; nullopt uses 4*(n+m).
inline Result solve(int n, const std::vector<Edge>& edges, int source,
                    std::optional<std::size_t> mixed_budget = std::nullopt) {
    detail::validate(n, edges, source);
    std::vector<std::vector<std::size_t>> adj(n);
    bool nonnegative = true;
    std::vector<std::size_t> indegree(n, 0);
    for (std::size_t i = 0; i < edges.size(); ++i) {
        adj[edges[i].from].push_back(i);
        ++indegree[edges[i].to];
        nonnegative = nonnegative && edges[i].weight >= 0;
    }
    std::queue<int> ready;
    for (int u = 0; u < n; ++u) if (!indegree[u]) ready.push(u);
    std::vector<int> order;
    while (!ready.empty()) {
        int u = ready.front(); ready.pop(); order.push_back(u);
        for (auto i : adj[u]) if (--indegree[edges[i].to] == 0) ready.push(edges[i].to);
    }
    Result r;
    r.distances.resize(n);
    r.distances[source] = 0;
    auto relax = [&](std::size_t i) {
        const auto& e = edges[i];
        Distance candidate = add(*r.distances[e.from], e.weight);
        if (!r.distances[e.to] || candidate < *r.distances[e.to]) {
            r.distances[e.to] = candidate;
            return true;
        }
        return false;
    };
    if (order.size() == static_cast<std::size_t>(n)) {
        r.method = "dag";
        for (int u : order) if (r.distances[u])
            for (auto i : adj[u]) { ++r.edge_scans; relax(i); }
        return r;
    }
    const auto max = std::numeric_limits<std::size_t>::max();
    if (edges.size() > max / 4 - static_cast<std::size_t>(n))
        throw std::length_error("graph too large for default work budget");
    const std::size_t budget = mixed_budget.value_or(4 * (edges.size() + n));
    r.method = nonnegative ? "dijkstra" : "bounded-heap";
    using Item = std::pair<Distance, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap;
    std::vector<std::size_t> pred(n, detail::none);
    heap.push({0, source});
    std::size_t since_check = 0;
    while (!heap.empty()) {
        auto [distance, u] = heap.top(); heap.pop();
        if (distance != *r.distances[u]) continue;
        for (auto i : adj[u]) {
            if (!nonnegative && r.edge_scans >= budget) {
                // Restart: a cut-off is not evidence of a negative cycle.
                auto fallback = bellman_ford(n, edges, source);
                fallback.used_fallback = true;
                fallback.edge_scans += r.edge_scans;
                fallback.cycle_checks += r.cycle_checks;
                return fallback;
            }
            ++r.edge_scans;
            if (relax(i)) {
                pred[edges[i].to] = i;
                heap.push({*r.distances[edges[i].to], edges[i].to});
            }
            if (!nonnegative && ++since_check >= static_cast<std::size_t>(n)) {
                since_check = 0;
                ++r.cycle_checks;
                r.negative_cycle = detail::certificate(edges, pred);
                if (!r.negative_cycle.empty()) {
                    r.distances.clear();
                    return r;
                }
            }
        }
    }
    return r;
}
} // namespace shortest_paths

#pragma once

#include "shortest_paths.hpp"

// Experimental composition of established primitives. See SIGNED_CORE.md for
// the proof, structural bound, prior art, and the limits of the novelty claim.
namespace signed_core {
using shortest_paths::Distance;
using shortest_paths::Edge;
using shortest_paths::Result;
using shortest_paths::add;
using Cell = std::optional<Distance>;
constexpr std::size_t none = shortest_paths::detail::none;

struct Options {
    bool components = true;
    bool tree_potential = true;
    bool predecessor_checks = true;
    // Select local Bellman-Ford when its worst-case work estimate is smaller.
    bool select_bellman_ford = true;
};
struct Statistics {
    std::size_t components = 0, reached_components = 0, tree_components = 0;
    std::size_t raw_negative_tails = 0, remaining_negative_tails = 0;
    std::size_t max_remaining_tails = 0, dijkstra_runs = 0, negative_rounds = 0;
    std::size_t bellman_components = 0, history_certificates = 0;
};
struct Output { Result answer; Statistics statistics; };

namespace detail {
inline Distance subtract(Distance a, Distance b) {
    Distance out;
    if (__builtin_sub_overflow(a, b, &out))
        throw std::overflow_error("potential exceeds signed 128-bit range");
    return out;
}

// Iterative Kosaraju. IDs are a topological ordering of the condensation.
inline std::vector<int> components(int n, const std::vector<Edge>& edges) {
    std::vector<std::vector<int>> adj(n), reverse(n);
    for (auto e : edges) {
        adj[e.from].push_back(e.to);
        reverse[e.to].push_back(e.from);
    }
    std::vector<unsigned char> seen(n, 0);
    std::vector<int> finish;
    for (int start = 0; start < n; ++start) {
        if (seen[start]) continue;
        std::vector<std::pair<int, std::size_t>> stack = {{start, 0}};
        seen[start] = 1;
        while (!stack.empty()) {
            int u = stack.back().first;
            auto& next = stack.back().second;
            if (next == adj[u].size()) {
                finish.push_back(u);
                stack.pop_back();
            } else {
                int v = adj[u][next++];
                if (!seen[v]) { seen[v] = 1; stack.push_back({v, 0}); }
            }
        }
    }
    std::vector<int> id(n, -1);
    int current = 0;
    for (auto it = finish.rbegin(); it != finish.rend(); ++it) {
        if (id[*it] >= 0) continue;
        std::vector<int> stack = {*it};
        id[*it] = current;
        while (!stack.empty()) {
            int u = stack.back(); stack.pop_back();
            for (int v : reverse[u]) if (id[v] < 0) {
                id[v] = current;
                stack.push_back(v);
            }
        }
        ++current;
    }
    return id;
}

struct Layer {
    std::vector<std::size_t> positive_parent, negative_seed;
    explicit Layer(int n) : positive_parent(n, none), negative_seed(n, none) {}
};

// Recover the historical walk, rather than trusting a mutable parent pointer
// to represent a path from an earlier phase. Erase nonnegative simple cycles;
// the first negative simple cycle is an independently checkable certificate.
inline std::vector<std::size_t> history_cycle(
        int n, const std::vector<Edge>& edges,
        const std::vector<Layer>& history, int vertex) {
    std::vector<std::size_t> walk;
    for (int t = static_cast<int>(history.size()) - 1; t >= 0; --t) {
        std::size_t steps = 0;
        while (history[t].positive_parent[vertex] != none) {
            if (++steps > static_cast<std::size_t>(n))
                throw std::logic_error("nonnegative closure parent cycle");
            auto i = history[t].positive_parent[vertex];
            walk.push_back(i);
            vertex = edges[i].from;
        }
        if (history[t].negative_seed[vertex] != none) {
            auto i = history[t].negative_seed[vertex];
            walk.push_back(i);
            vertex = edges[i].from;
        }
    }
    std::reverse(walk.begin(), walk.end());
    std::vector<int> position(n, -1), vertices = {vertex};
    std::vector<std::size_t> path;
    std::vector<Distance> prefix = {0};
    position[vertex] = 0;
    for (auto i : walk) {
        const auto& e = edges[i];
        if (e.from != vertices.back()) throw std::logic_error("broken walk");
        Distance next = add(prefix.back(), e.weight);
        int p = position[e.to];
        if (p < 0) {
            path.push_back(i);
            position[e.to] = static_cast<int>(vertices.size());
            vertices.push_back(e.to);
            prefix.push_back(next);
        } else {
            if (subtract(next, prefix[p]) < 0) {
                std::vector<std::size_t> cycle(path.begin() + p, path.end());
                cycle.push_back(i);
                return cycle;
            }
            for (std::size_t j = p + 1; j < vertices.size(); ++j)
                position[vertices[j]] = -1;
            path.resize(p);
            vertices.resize(p + 1);
            prefix.resize(p + 1);
        }
    }
    throw std::logic_error("negative-tail phase bound lacked a negative cycle");
}

inline std::size_t tail_count(int n, const std::vector<Edge>& edges,
                              const std::vector<Distance>& weight) {
    std::vector<unsigned char> negative(n, 0);
    for (std::size_t i = 0; i < edges.size(); ++i)
        if (weight[i] < 0) negative[edges[i].from] = 1;
    return std::count(negative.begin(), negative.end(), 1);
}

// Multi-source local problem: entry labels have already been finalized by
// earlier SCCs. Endpoints and all predecessor arrays use local vertex IDs.
inline std::vector<std::size_t> local(
        int n, const std::vector<Edge>& edges, std::vector<Cell>& distance,
        const Options& options, Output& out) {
    std::vector<std::vector<std::size_t>> adj(n);
    std::vector<Distance> weight;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        adj[edges[i].from].push_back(i);
        weight.push_back(edges[i].weight);
    }
    auto raw = tail_count(n, edges, weight);
    out.statistics.raw_negative_tails += raw;
    std::vector<Distance> potential(n, 0);
    std::size_t remaining = raw;
    if (options.tree_potential && options.components && n > 1) {
        std::vector<unsigned char> seen(n, 0);
        std::vector<int> bfs = {0};
        std::vector<Distance> tree(n, 0), reduced;
        seen[0] = 1;
        for (std::size_t j = 0; j < bfs.size(); ++j)
            for (auto i : adj[bfs[j]]) if (!seen[edges[i].to]) {
                const auto& e = edges[i];
                seen[e.to] = 1;
                tree[e.to] = add(tree[e.from], e.weight);
                bfs.push_back(e.to);
            }
        if (bfs.size() != static_cast<std::size_t>(n))
            throw std::logic_error("component is not strongly connected");
        for (auto e : edges)
            reduced.push_back(subtract(add(e.weight, tree[e.from]), tree[e.to]));
        auto count = tail_count(n, edges, reduced);
        if (count < remaining) {
            remaining = count;
            potential = std::move(tree);
            weight = std::move(reduced);
            ++out.statistics.tree_components;
        }
    }
    out.statistics.remaining_negative_tails += remaining;
    out.statistics.max_remaining_tails = std::max(
        out.statistics.max_remaining_tails, remaining);
    for (int u = 0; u < n; ++u)
        if (distance[u]) distance[u] = subtract(*distance[u], potential[u]);
    auto restore = [&] {
        for (int u = 0; u < n; ++u)
            if (distance[u]) distance[u] = add(*distance[u], potential[u]);
    };
    std::vector<std::size_t> pred(n, none);

    // Compare upper bounds, not measured running times. This preserves the
    // O(n*m) worst-case option; a phase-only hybrid can pay an extra logarithm.
    std::size_t log = 1;
    for (std::size_t x = edges.size() + static_cast<std::size_t>(n) + 1;
         x > 1; x >>= 1) ++log;
    bool use_bf = options.select_bellman_ford &&
        static_cast<Distance>(remaining + 2) * log >= n;
    if (use_bf) {
        ++out.statistics.bellman_components;
        for (int pass = 0; pass < n; ++pass) {
            bool changed = false;
            for (std::size_t i = 0; i < edges.size(); ++i) {
                ++out.answer.edge_scans;
                const auto& e = edges[i];
                if (!distance[e.from]) continue;
                Distance candidate = add(*distance[e.from], weight[i]);
                if (!distance[e.to] || candidate < *distance[e.to]) {
                    distance[e.to] = candidate;
                    pred[e.to] = i;
                    changed = true;
                }
            }
            if (!changed) { restore(); return {}; }
        }
        ++out.answer.cycle_checks;
        auto cycle = shortest_paths::detail::certificate(edges, pred);
        if (cycle.empty()) throw std::logic_error("local BF certificate missing");
        return cycle;
    }

    std::vector<std::size_t> negative;
    for (std::size_t i = 0; i < edges.size(); ++i)
        if (weight[i] < 0) negative.push_back(i);
    auto closure = [&](Layer& layer) {
        ++out.statistics.dijkstra_runs;
        using Item = std::pair<Distance, int>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap;
        for (int u = 0; u < n; ++u) if (distance[u]) heap.push({*distance[u], u});
        while (!heap.empty()) {
            auto [d, u] = heap.top(); heap.pop();
            if (d != *distance[u]) continue;
            for (auto i : adj[u]) {
                ++out.answer.edge_scans;
                if (weight[i] < 0) continue;
                int v = edges[i].to;
                Distance candidate = add(d, weight[i]);
                if (!distance[v] || candidate < *distance[v]) {
                    distance[v] = candidate;
                    pred[v] = i;
                    layer.positive_parent[v] = i;
                    heap.push({candidate, v});
                }
            }
        }
    };
    std::vector<Layer> history;
    Layer initial(n);
    closure(initial);
    if (remaining == 0) { restore(); return {}; }
    history.push_back(std::move(initial));
    for (std::size_t round = 1; round <= remaining + 1; ++round) {
        ++out.statistics.negative_rounds;
        auto previous = distance;
        Layer layer(n);
        int improved = -1;
        for (auto i : negative) {
            ++out.answer.edge_scans;
            const auto& e = edges[i];
            if (!previous[e.from]) continue;
            Distance candidate = add(*previous[e.from], weight[i]);
            if (!distance[e.to] || candidate < *distance[e.to]) {
                distance[e.to] = candidate;
                pred[e.to] = i;
                layer.negative_seed[e.to] = i;
                improved = e.to;
            }
        }
        if (improved < 0) { restore(); return {}; }
        closure(layer);
        if (options.predecessor_checks) {
            ++out.answer.cycle_checks;
            auto cycle = shortest_paths::detail::certificate(edges, pred);
            if (!cycle.empty()) return cycle;
        }
        history.push_back(std::move(layer));
        if (round == remaining + 1) {
            ++out.statistics.history_certificates;
            return history_cycle(n, edges, history, improved);
        }
    }
    throw std::logic_error("unreachable phase exit");
}
} // namespace detail

// Returns all finite/unreachable distances, OR a source-reachable negative
// cycle using original edge indices. Does not classify all -infinity vertices.
inline Output solve(int n, const std::vector<Edge>& edges, int source,
                    Options options = {}) {
    shortest_paths::detail::validate(n, edges, source);
    Output out;
    out.answer.method = "signed-core";
    out.answer.distances.resize(n);
    out.answer.distances[source] = 0;
    auto id = options.components ? detail::components(n, edges) : std::vector<int>(n, 0);
    int count = *std::max_element(id.begin(), id.end()) + 1;
    out.statistics.components = count;
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
            entry.push_back(out.answer.distances[u]);
            reachable = reachable || entry.back().has_value();
        }
        if (!reachable) continue;
        ++out.statistics.reached_components;
        std::vector<Edge> local_edges;
        for (auto i : internal[c]) {
            const auto& e = edges[i];
            local_edges.push_back({local_id[e.from], local_id[e.to], e.weight});
        }
        auto cycle = detail::local(static_cast<int>(entry.size()), local_edges,
                                   entry, options, out);
        if (!cycle.empty()) {
            for (auto i : cycle) out.answer.negative_cycle.push_back(internal[c][i]);
            out.answer.distances.clear();
            return out;
        }
        for (std::size_t j = 0; j < entry.size(); ++j)
            out.answer.distances[vertices[c][j]] = entry[j];
        for (auto i : outgoing[c]) {
            ++out.answer.edge_scans;
            const auto& e = edges[i];
            if (id[e.to] <= c) throw std::logic_error("SCC ordering failure");
            if (!out.answer.distances[e.from]) continue;
            auto candidate = add(*out.answer.distances[e.from], e.weight);
            auto& to = out.answer.distances[e.to];
            if (!to || candidate < *to) to = candidate;
        }
    }
    return out;
}
} // namespace signed_core

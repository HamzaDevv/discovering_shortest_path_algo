#pragma once

#include "branch_core.hpp"

// A geometric-budget race of established algorithms. The guarantee is relative
// to this fixed portfolio's metered work, not universal instance optimality or
// a new general asymptotic record. See docs/COMPETITIVE_TRIAL.md for the model.
namespace competitive_paths {
using shortest_paths::Distance;
using shortest_paths::Edge;
using shortest_paths::Result;

struct Statistics {
    std::size_t rounds = 0, attempts = 0, budget_exhaustions = 0;
    std::size_t metered_work = 0, final_budget = 0;
};

namespace detail {
using Adjacency = std::vector<std::vector<std::size_t>>;
struct BudgetExceeded {};

struct Meter {
    std::size_t limit, used = 0;
    void spend(std::size_t amount = 1) {
        if (amount > limit - used) throw BudgetExceeded{};
        used += amount;
    }
};

inline void accumulate(std::size_t& total, std::size_t amount) {
    if (amount > std::numeric_limits<std::size_t>::max() - total)
        throw std::length_error("portfolio work counter overflow");
    total += amount;
}

inline bool certificate(const std::vector<Edge>& edges,
                        const std::vector<std::size_t>& pred, Result& result) {
    ++result.cycle_checks;
    result.negative_cycle = shortest_paths::detail::certificate(edges, pred);
    if (result.negative_cycle.empty()) return false;
    result.distances.clear();
    return true;
}

inline bool relax(const std::vector<Edge>& edges, std::size_t i,
                  std::vector<std::size_t>& pred, Result& result) {
    const auto& e = edges[i];
    if (!result.distances[e.from]) return false;
    auto candidate = shortest_paths::add(*result.distances[e.from], e.weight);
    if (result.distances[e.to] && candidate >= *result.distances[e.to]) return false;
    result.distances[e.to] = candidate;
    if (!pred.empty()) pred[e.to] = i;
    return true;
}

// Every n scans, an O(n) original-edge predecessor check. Over a bounded
// attempt these checks take O(n + scans) work, including a final certificate.
inline bool checkpoint(int n, const std::vector<Edge>& edges,
                       const std::vector<std::size_t>& pred, Result& result,
                       std::size_t& since_check) {
    if (++since_check < static_cast<std::size_t>(n)) return false;
    since_check = 0;
    return certificate(edges, pred, result);
}

inline void bellman_ford(int n, const std::vector<Edge>& edges, int source,
                         Meter& meter, Result& result, bool detect_cycles = true) {
    result.distances.resize(n);
    result.distances[source] = 0;
    std::vector<std::size_t> pred(detect_cycles ? n : 0, shortest_paths::detail::none);
    std::size_t since_check = 0;
    for (int pass = 0; pass < n; ++pass) {
        bool changed = false;
        for (std::size_t i = 0; i < edges.size(); ++i) {
            meter.spend();
            ++result.edge_scans;
            changed = relax(edges, i, pred, result) || changed;
            if (detect_cycles && checkpoint(n, edges, pred, result, since_check)) return;
        }
        if (!changed) return;
    }
    if (!certificate(edges, pred, result))
        throw std::logic_error("Bellman-Ford failed to produce its cycle witness");
}

inline void fifo(int n, const std::vector<Edge>& edges, const Adjacency& adj,
                 int source, Meter& meter, Result& result, bool detect_cycles = true) {
    result.distances.resize(n);
    result.distances[source] = 0;
    std::vector<std::size_t> pred(detect_cycles ? n : 0, shortest_paths::detail::none);
    std::vector<unsigned char> queued(n, 0);
    std::queue<int> queue;
    queue.push(source);
    queued[source] = 1;
    std::size_t since_check = 0;
    while (!queue.empty()) {
        int u = queue.front(); queue.pop();
        queued[u] = 0;
        for (auto i : adj[u]) {
            meter.spend();
            ++result.edge_scans;
            if (relax(edges, i, pred, result)) {
                int v = edges[i].to;
                if (!queued[v]) { queue.push(v); queued[v] = 1; }
            }
            if (detect_cycles && checkpoint(n, edges, pred, result, since_check)) return;
        }
    }
}

// Conservative logarithmic charge for sift-up and sift-down, including the
// latter's two key comparisons per level. Vector growth is amortized.
inline std::size_t heap_charge(std::size_t size) {
    std::size_t levels = 1;
    for (; size; size >>= 1) ++levels;
    return 2 * levels + 1;
}

// One pending entry per vertex, including on signed graphs that reopen an
// extracted vertex. This keeps heap storage O(n) throughout a bounded attempt.
class IndexedHeap {
    const std::vector<std::optional<Distance>>& distances;
    Meter& meter;
    std::vector<int> vertices, position;
    bool earlier(int u, int v) const {
        if (distances[u] != distances[v]) return *distances[u] < *distances[v];
        return u < v;
    }
    void exchange(int a, int b) {
        std::swap(vertices[a], vertices[b]);
        position[vertices[a]] = a;
        position[vertices[b]] = b;
    }
public:
    IndexedHeap(int n, const std::vector<std::optional<Distance>>& labels, Meter& work)
        : distances(labels), meter(work), position(n, -1) {}
    bool empty() const { return vertices.empty(); }
    void decrease(int vertex) {
        meter.spend(heap_charge(vertices.size()));
        int p = position[vertex];
        if (p < 0) {
            p = static_cast<int>(vertices.size());
            vertices.push_back(vertex);
            position[vertex] = p;
        }
        while (p > 0 && earlier(vertices[p], vertices[(p - 1) / 2])) {
            int parent = (p - 1) / 2;
            exchange(p, parent);
            p = parent;
        }
    }
    int pop() {
        meter.spend(heap_charge(vertices.size()));
        int vertex = vertices.front();
        exchange(0, static_cast<int>(vertices.size()) - 1);
        vertices.pop_back();
        position[vertex] = -1;
        std::size_t p = 0;
        while (2 * p + 1 < vertices.size()) {
            std::size_t child = 2 * p + 1;
            if (child + 1 < vertices.size() && earlier(vertices[child + 1], vertices[child]))
                ++child;
            if (!earlier(vertices[child], vertices[p])) break;
            exchange(static_cast<int>(p), static_cast<int>(child));
            p = child;
        }
        return vertex;
    }
};

inline void heap(int n, const std::vector<Edge>& edges, const Adjacency& adj,
                 int source, Meter& meter, Result& result, bool detect_cycles = true) {
    result.distances.resize(n);
    result.distances[source] = 0;
    std::vector<std::size_t> pred(detect_cycles ? n : 0, shortest_paths::detail::none);
    IndexedHeap queue(n, result.distances, meter);
    queue.decrease(source);
    std::size_t since_check = 0;
    while (!queue.empty()) {
        int u = queue.pop();
        for (auto i : adj[u]) {
            meter.spend();
            ++result.edge_scans;
            if (relax(edges, i, pred, result)) {
                int v = edges[i].to;
                queue.decrease(v);
            }
            if (detect_cycles && checkpoint(n, edges, pred, result, since_check)) return;
        }
    }
}
} // namespace detail

// Returns exact finite/unreachable distances OR an original-edge reachable
// negative cycle. All input weights are int64; checked arithmetic is int128.
// statistics is optional and counts all attempts, including discarded prefixes.
inline Result solve(int n, const std::vector<Edge>& edges, int source,
                    Statistics* statistics = nullptr) {
    shortest_paths::detail::validate(n, edges, source);
    const auto maximum = std::numeric_limits<std::size_t>::max();
    if (edges.size() > maximum - static_cast<std::size_t>(n))
        throw std::length_error("graph too large for portfolio work budget");
    detail::Adjacency adj(n);
    std::vector<std::size_t> indegree(n, 0);
    bool nonnegative = true;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        adj[edges[i].from].push_back(i);
        ++indegree[edges[i].to];
        nonnegative = nonnegative && edges[i].weight >= 0;
    }
    // A linear-time specialization preserves the race bound, whose additive
    // term already includes n+m. Cyclic graphs continue to the bounded race.
    std::vector<int> order;
    for (int u = 0; u < n; ++u) if (indegree[u] == 0) order.push_back(u);
    for (std::size_t p = 0; p < order.size(); ++p)
        for (auto i : adj[order[p]]) if (--indegree[edges[i].to] == 0)
            order.push_back(edges[i].to);
    if (order.size() == static_cast<std::size_t>(n)) {
        Result result;
        result.method = "competitive-dag";
        result.distances.resize(n);
        result.distances[source] = 0;
        for (int u : order) if (result.distances[u]) for (auto i : adj[u]) {
            ++result.edge_scans;
            const auto& e = edges[i];
            auto candidate = shortest_paths::add(*result.distances[u], e.weight);
            auto& destination = result.distances[e.to];
            if (!destination || candidate < *destination) destination = candidate;
        }
        if (statistics) *statistics = {1, 1, 0, result.edge_scans, edges.size() + n};
        return result;
    }
    Statistics stats;
    std::size_t budget = edges.size() + n, scans = 0, checks = 0;
    const char* names[] = {"competitive-bellman-ford", "competitive-fifo",
                           "competitive-heap", "competitive-branch-core"};
    for (;;) {
        ++stats.rounds;
        stats.final_budget = budget;
        for (int slot = 0; slot < 4; ++slot) {
            // Prefer the heap for nonnegative input; all four engines still
            // get an attempt each round, preserving the metered-work bound.
            int engine = nonnegative && slot < 3 ? (slot == 0 ? 2 : slot - 1) : slot;
            ++stats.attempts;
            Result result;
            detail::Meter meter{budget};
            bool completed = false;
            try {
                if (engine == 0)
                    detail::bellman_ford(n, edges, source, meter, result, !nonnegative);
                if (engine == 1)
                    detail::fifo(n, edges, adj, source, meter, result, !nonnegative);
                if (engine == 2)
                    detail::heap(n, edges, adj, source, meter, result, !nonnegative);
                if (engine == 3) {
                    result = branch_core::solve(n, edges, source, budget).answer;
                    meter.used = result.edge_scans;
                }
                completed = true;
            } catch (const detail::BudgetExceeded&) {
                ++stats.budget_exhaustions;
            } catch (const branch_core::ScanBudgetExceeded& exhausted) {
                result.edge_scans = exhausted.edge_scans;
                result.cycle_checks = exhausted.cycle_checks;
                meter.used = exhausted.edge_scans;
                ++stats.budget_exhaustions;
            }
            detail::accumulate(stats.metered_work, meter.used);
            detail::accumulate(scans, result.edge_scans);
            detail::accumulate(checks, result.cycle_checks);
            if (completed) {
                result.method = names[engine];
                result.used_fallback = stats.budget_exhaustions != 0;
                result.edge_scans = scans;
                result.cycle_checks = checks;
                if (statistics) *statistics = stats;
                return result;
            }
        }
        if (budget == maximum) throw std::length_error("portfolio work budget exhausted");
        budget = budget > maximum / 2 ? maximum : 2 * budget;
    }
}
} // namespace competitive_paths

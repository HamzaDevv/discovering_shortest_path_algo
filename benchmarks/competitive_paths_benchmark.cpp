#include "competitive_paths.hpp"
#include "adaptive_core.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>

using namespace shortest_paths;
using Clock = std::chrono::steady_clock;

namespace {
constexpr int methods = 5;
constexpr int repetitions = 7;
const std::array<std::string, methods> names = {
    "previous_portfolio", "bellman_ford", "branch_core", "adaptive_core",
    "competitive_paths"
};

struct Instance {
    std::string family;
    int n;
    int source;
    std::vector<Edge> edges;
};

Result solve(int method, const Instance& graph,
             competitive_paths::Statistics* statistics = nullptr) {
    switch (method) {
    case 0: return shortest_paths::solve(graph.n, graph.edges, graph.source);
    case 1: return bellman_ford(graph.n, graph.edges, graph.source);
    case 2: return branch_core::solve(graph.n, graph.edges, graph.source).answer;
    case 3: return adaptive_core::solve(graph.n, graph.edges, graph.source);
    case 4: return competitive_paths::solve(graph.n, graph.edges, graph.source, statistics);
    default: throw std::logic_error("unknown benchmark method");
    }
}

// Relabeling changes vertex tie breaking without changing the graph problem.
// Deliberately ordered edge lists retain their ordering after this mapping.
void relabel(Instance& graph, std::mt19937& random) {
    std::vector<int> permutation(graph.n);
    std::iota(permutation.begin(), permutation.end(), 0);
    std::shuffle(permutation.begin(), permutation.end(), random);
    graph.source = permutation[graph.source];
    for (auto& edge : graph.edges) {
        edge.from = permutation[edge.from];
        edge.to = permutation[edge.to];
    }
}

std::vector<unsigned char> reachable(const Instance& graph) {
    std::vector<std::vector<int>> adjacency(graph.n);
    for (auto edge : graph.edges) adjacency[edge.from].push_back(edge.to);
    std::vector<unsigned char> seen(graph.n, 0);
    std::vector<int> stack = {graph.source};
    seen[graph.source] = 1;
    while (!stack.empty()) {
        int vertex = stack.back();
        stack.pop_back();
        for (int next : adjacency[vertex]) if (!seen[next]) {
            seen[next] = 1;
            stack.push_back(next);
        }
    }
    return seen;
}

// Compare exact answers with the reference, and independently check the
// original-edge negative-cycle witness or finite relaxation inequalities.
// All validation and graph generation are excluded from solver timing.
void validate(const Instance& graph, const Result& result,
              const Result& reference, const std::vector<unsigned char>& seen) {
    if (result.distances != reference.distances ||
        result.negative_cycle.empty() != reference.negative_cycle.empty())
        throw std::logic_error("benchmark answer differs from Bellman-Ford");
    if (!result.negative_cycle.empty()) {
        if (!result.distances.empty())
            throw std::logic_error("negative-cycle result also contains distances");
        Distance sum = 0;
        for (std::size_t i = 0; i < result.negative_cycle.size(); ++i) {
            const auto& edge = graph.edges.at(result.negative_cycle[i]);
            const auto& next = graph.edges.at(
                result.negative_cycle[(i + 1) % result.negative_cycle.size()]);
            if (edge.to != next.from || !seen[edge.from])
                throw std::logic_error("invalid or unreachable cycle witness");
            sum = add(sum, edge.weight);
        }
        if (sum >= 0) throw std::logic_error("cycle witness is not negative");
        return;
    }
    if (result.distances.size() != static_cast<std::size_t>(graph.n) ||
        !result.distances[graph.source] || *result.distances[graph.source] != 0)
        throw std::logic_error("invalid finite-distance result");
    for (int vertex = 0; vertex < graph.n; ++vertex)
        if (result.distances[vertex].has_value() != bool(seen[vertex]))
            throw std::logic_error("finite reachability mismatch");
    for (auto edge : graph.edges) if (result.distances[edge.from]) {
        if (!result.distances[edge.to] ||
            *result.distances[edge.to] > add(*result.distances[edge.from], edge.weight))
            throw std::logic_error("finite result violates an edge inequality");
    }
}

void run(const Instance& graph, unsigned seed) {
    auto reference = bellman_ford(graph.n, graph.edges, graph.source);
    auto seen = reachable(graph);
    std::array<std::vector<double>, methods> samples;
    std::array<Result, methods> outputs;
    std::array<competitive_paths::Statistics, methods> statistics;
    for (int method = 0; method < methods; ++method)
        validate(graph, solve(method, graph), reference, seen);
    for (int repeat = 0; repeat < repetitions; ++repeat) {
        for (int slot = 0; slot < methods; ++slot) {
            int method = (slot + repeat) % methods;
            auto start = Clock::now();
            competitive_paths::Statistics captured;
            auto result = solve(method, graph, &captured);
            statistics[method] = captured;
            auto stop = Clock::now();
            samples[method].push_back(
                std::chrono::duration<double, std::milli>(stop - start).count());
            validate(graph, result, reference, seen);
            outputs[method] = std::move(result);
        }
    }
    for (int method = 0; method < methods; ++method) {
        std::sort(samples[method].begin(), samples[method].end());
        const auto& result = outputs[method];
        std::cout << graph.family << ',' << seed << ',' << graph.n << ','
                  << graph.edges.size() << ',' << graph.source << ',' << names[method]
                  << ',' << result.edge_scans << ',' << result.cycle_checks << ','
                  << result.used_fallback << ',' << result.method << ','
                  << samples[method][repetitions / 2] << ','
                  << samples[method].front() << ',' << samples[method].back() << ','
                  << statistics[method].rounds << ',' << statistics[method].attempts << ','
                  << statistics[method].metered_work << '\n';
    }
}

std::vector<Instance> instances(int n, std::mt19937& random) {
    std::vector<Instance> graphs;
    auto append = [&](std::string family, std::vector<Edge> edges, int source,
                      bool shuffle = true) {
        if (shuffle) std::shuffle(edges.begin(), edges.end(), random);
        Instance graph{std::move(family), n, source, std::move(edges)};
        relabel(graph, random);
        graphs.push_back(std::move(graph));
    };
    std::vector<Edge> signed_ring, negative_ring, zero_ring;
    for (int vertex = 0; vertex < n; ++vertex) {
        signed_ring.push_back({vertex, (vertex + 1) % n, vertex + 1 == n ? n : -1});
        negative_ring.push_back({vertex, (vertex + 1) % n, -1});
        zero_ring.push_back({vertex, (vertex + 1) % n, vertex + 1 == n ? n - 1 : -1});
    }
    // The ordered rings start at vertex zero, exposing the edge-order benefit
    // of in-place Bellman-Ford. Their shuffled counterparts remove that benefit.
    append("ordered_signed_ring", signed_ring, 0, false);
    append("shuffled_signed_ring", signed_ring, int(random() % n));
    append("ordered_negative_ring", negative_ring, 0, false);
    append("shuffled_negative_ring", negative_ring, int(random() % n));
    append("zero_sum_signed_ring", zero_ring, int(random() % n));

    std::vector<Edge> dag, adversarial;
    for (int vertex = 0; vertex + 1 < n; ++vertex) {
        dag.push_back({vertex, vertex + 1, int(random() % 201) - 100});
        for (int j = 0; j < 5; ++j) {
            int next = vertex + 1 + int(random() % (n - vertex - 1));
            dag.push_back({vertex, next, int(random() % 201) - 100});
        }
    }
    append("shuffled_signed_dag", std::move(dag), int(random() % (n / 5)));
    // No cycle exists. Descending source adjacency produces repeated FIFO
    // waves; negative direct labels also make heap reopening quadratic.
    for (int vertex = n - 1; vertex > 0; --vertex)
        adversarial.push_back({0, vertex, -vertex});
    for (int vertex = 1; vertex + 1 < n; ++vertex)
        adversarial.push_back({vertex, vertex + 1, -2});
    append("queue_heap_adversarial_dag", std::move(adversarial), 0, false);

    for (int degree : {6, 32}) {
        std::vector<std::int64_t> potential(n);
        for (auto& value : potential) value = int(random() % 20001) - 10000;
        std::vector<Edge> signed_graph, positive_graph;
        for (int vertex = 0; vertex < n; ++vertex) {
            for (int j = 0; j < degree; ++j) {
                int next = j == 0 ? (vertex + 1) % n : int(random() % n);
                std::int64_t base = 1 + random() % 100;
                signed_graph.push_back({vertex, next, base + potential[next] - potential[vertex]});
                positive_graph.push_back({vertex, next, base});
            }
        }
        const std::string density = degree == 6 ? "sparse" : "dense";
        append("signed_" + density + "_potential", std::move(signed_graph), int(random() % n));
        append("nonnegative_" + density, std::move(positive_graph), int(random() % n));
    }

    std::vector<Edge> small_components, bidirectional;
    for (int vertex = 0; vertex < n; ++vertex) {
        if (vertex % 5 != 4) small_components.push_back({vertex, vertex + 1, -1});
        else {
            small_components.push_back({vertex, vertex - 4, 5});
            if (vertex + 1 < n) small_components.push_back({vertex, vertex + 1, -1});
        }
    }
    append("many_small_scc", std::move(small_components), int(random() % 5));
    for (int vertex = n - 2; vertex >= 0; --vertex) {
        bidirectional.push_back({vertex, vertex + 1, -1});
        bidirectional.push_back({vertex + 1, vertex, 2});
    }
    append("reverse_bidirectional_chain", std::move(bidirectional), 0, false);

    std::vector<Edge> reachable_negative;
    for (int vertex = 0; vertex + 1 < n; ++vertex)
        reachable_negative.push_back({vertex, vertex + 1, 1});
    reachable_negative.push_back({n / 2 + 2, n / 2, -3});
    append("reachable_negative_cycle", std::move(reachable_negative), int(random() % (n / 4)));
    // Negative edges in an unreachable component must not alter source status.
    const int split = n / 2;
    std::vector<std::int64_t> potential(split);
    for (auto& value : potential) value = int(random() % 2001) - 1000;
    std::vector<Edge> unreachable_negative;
    for (int vertex = 0; vertex < split; ++vertex) {
        for (int j = 0; j < 4; ++j) {
            int next = j == 0 ? (vertex + 1) % split : int(random() % split);
            unreachable_negative.push_back({vertex, next, 1 + potential[next] - potential[vertex]});
        }
    }
    for (int vertex = split; vertex < n; ++vertex)
        unreachable_negative.push_back({vertex, vertex + 1 == n ? split : vertex + 1, -1});
    append("unreachable_negative_cycle", std::move(unreachable_negative), int(random() % split));

    auto sparse_core = signed_ring;
    constexpr int roots = 10;
    for (int j = 0; j < roots; ++j) {
        int vertex = j * n / roots, next = ((j + 3) % roots) * n / roots;
        sparse_core.push_back({vertex, next, 5 + vertex - next});
    }
    append("sparse_branching_ring", std::move(sparse_core), int(random() % n));

    constexpr int kernel_edges = roots * 4;
    int next_vertex = roots;
    std::vector<Edge> subdivided;
    for (int vertex = 0; vertex < roots; ++vertex) for (int j = 0; j < 4; ++j) {
        int number = vertex * 4 + j;
        int end = j == 0 ? (vertex + 1) % roots : int(random() % roots);
        int forced = (n - roots) / kernel_edges + (number < (n - roots) % kernel_edges);
        int previous = vertex;
        for (int k = 0; k <= forced; ++k) {
            int next = k == forced ? end : next_vertex++;
            subdivided.push_back({previous, next, k == 0 ? 1 + int(random() % 100) : 0});
            previous = next;
        }
    }
    if (next_vertex != n) throw std::logic_error("incorrect subdivision size");
    std::vector<std::int64_t> subdivision_potential(n);
    for (auto& value : subdivision_potential) value = int(random() % 20001) - 10000;
    for (auto& edge : subdivided)
        edge.weight += subdivision_potential[edge.to] - subdivision_potential[edge.from];
    append("subdivided_random_kernel", std::move(subdivided), int(random() % n));
    return graphs;
}
} // namespace

int main(int argc, char** argv) {
    if (argc > 2 || (argc == 2 && std::string(argv[1]) != "--holdout")) {
        std::cerr << "usage: " << argv[0] << " [--holdout]\n";
        return 2;
    }
    const std::array<unsigned, 3> seeds = argc == 2
        ? std::array<unsigned, 3>{20261111u, 20261112u, 20261113u}
        : std::array<unsigned, 3>{20261104u, 20261105u, 20261106u};
    try {
        std::cout << "family,seed,n,m,source,solver,edge_scans,cycle_checks,"
                     "used_fallback,method,median_ms,min_ms,max_ms,"
                     "competitive_rounds,competitive_attempts,competitive_work\n";
        std::cout << std::fixed << std::setprecision(6);
        for (unsigned seed : seeds) {
            for (int n : {300, 1500}) {
                std::mt19937 random(seed ^ (unsigned(n) * 2654435761u));
                for (const auto& graph : instances(n, random)) {
                    try {
                        run(graph, seed);
                    } catch (const std::exception& error) {
                        throw std::logic_error(graph.family + " seed=" + std::to_string(seed) +
                                               " n=" + std::to_string(n) + ": " + error.what());
                    }
                }
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "benchmark failed: " << error.what() << '\n';
        return 1;
    }
}

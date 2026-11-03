#include "branch_core.hpp"
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

using namespace shortest_paths;
using Clock = std::chrono::steady_clock;

void run(const std::string& family, int n, const std::vector<Edge>& edges) {
    constexpr int methods = 6, repetitions = 7;
    const std::array<std::string, methods> names = {
        "signed_core", "without_tree", "global_phases", "previous_portfolio", "bellman_ford",
        "branch_core"
    };
    std::array<std::vector<double>, methods> times;
    std::array<signed_core::Output, methods> outputs;
    std::array<branch_core::Statistics, methods> branch_statistics;
    auto reference = bellman_ford(n, edges, 0);
    std::vector<std::vector<int>> adj(n);
    for (auto e : edges) adj[e.from].push_back(e.to);
    std::vector<unsigned char> reachable(n, 0);
    std::vector<int> stack = {0}; reachable[0] = 1;
    while (!stack.empty()) {
        int u = stack.back(); stack.pop_back();
        for (int v : adj[u]) if (!reachable[v]) {
            reachable[v] = 1; stack.push_back(v);
        }
    }
    for (int repeat = 0; repeat < repetitions; ++repeat) {
        // Rotate execution order. Include all solver preprocessing in timing.
        for (int slot = 0; slot < methods; ++slot) {
            int method = (slot + repeat)%methods;
            auto start = Clock::now();
            signed_core::Output output;
            if (method == 0) output = signed_core::solve(n, edges, 0);
            if (method == 1) output = signed_core::solve(n, edges, 0, {true,false,true,true});
            if (method == 2) output = signed_core::solve(n, edges, 0, {false,false,true,false});
            if (method == 3) output.answer = shortest_paths::solve(n, edges, 0);
            if (method == 4) output.answer = bellman_ford(n, edges, 0);
            if (method == 5) {
                auto compressed = branch_core::solve(n, edges, 0);
                output.answer = std::move(compressed.answer);
                branch_statistics[method] = compressed.statistics;
            }
            double ms = std::chrono::duration<double, std::milli>(Clock::now()-start).count();
            auto& r = output.answer;
            if (r.distances != reference.distances ||
                r.negative_cycle.empty() != reference.negative_cycle.empty())
                throw std::logic_error("benchmark answer mismatch");
            if (!r.negative_cycle.empty()) {
                Distance sum = 0;
                for (std::size_t j = 0; j < r.negative_cycle.size(); ++j) {
                    auto e = edges.at(r.negative_cycle[j]);
                    auto next = edges.at(r.negative_cycle[(j+1)%r.negative_cycle.size()]);
                    if (e.to != next.from || !reachable[e.from])
                        throw std::logic_error("invalid benchmark cycle");
                    sum = add(sum, e.weight);
                }
                if (sum >= 0) throw std::logic_error("nonnegative benchmark witness");
            }
            times[method].push_back(ms);
            outputs[method] = std::move(output);
        }
    }
    for (int method = 0; method < methods; ++method) {
        std::sort(times[method].begin(), times[method].end());
        auto& r = outputs[method].answer;
        auto& s = outputs[method].statistics;
        std::cout << family << ',' << n << ',' << edges.size() << ',' << names[method]
                  << ',' << r.edge_scans << ',' << s.reached_components
                  << ',' << s.raw_negative_tails << ',' << s.remaining_negative_tails
                  << ',' << s.dijkstra_runs << ',' << s.negative_rounds
                  << ',' << s.bellman_components << ',' << branch_statistics[method].core_vertices
                  << ',' << branch_statistics[method].core_edges
                  << ',' << times[method][repetitions/2] << '\n';
    }
}

int main() {
    std::cout << "family,n,m,solver,edge_scans,reached_components,raw_negative_tails,"
                 "remaining_negative_tails,dijkstra_runs,negative_rounds,bf_components,"
                 "core_vertices,core_edges,median_ms\n";
    std::cout << std::fixed << std::setprecision(6);
    std::mt19937 random(20261103);
    for (int n : {100, 500, 1000}) {
        std::vector<Edge> negative_ring, signed_ring, chain, small_scc, mixed, positive;
        for (int u = 0; u < n; ++u) {
            negative_ring.push_back({u,(u+1)%n,-1});
            signed_ring.push_back({u,(u+1)%n,u == n-1 ? n : -1});
            if (u%5 != 4) small_scc.push_back({u,u+1,-1});
            else {
                small_scc.push_back({u,u-4,5});
                if (u+1 < n) small_scc.push_back({u,u+1,-1});
            }
        }
        for (int u = n-2; u >= 0; --u) {
            chain.push_back({u,u+1,-1});
            chain.push_back({u+1,u,2});
        }
        std::reverse(small_scc.begin(), small_scc.end());
        std::vector<std::int64_t> potential(n);
        for (auto& p : potential) p = int(random()%2001)-1000;
        for (int u = 0; u < n; ++u) for (int j = 0; j < 6; ++j) {
            int v = j == 0 ? (u+1)%n : int(random()%n);
            std::int64_t base = 1 + random()%100;
            mixed.push_back({u,v,base-potential[u]+potential[v]});
            positive.push_back({u,v,base});
        }
        run("negative_ring", n, negative_ring);
        run("positive_sum_signed_ring", n, signed_ring);
        run("reverse_bidirectional_chain", n, chain);
        run("many_small_scc", n, small_scc);
        run("signed_random_potential", n, mixed);
        run("nonnegative_random", n, positive);
        // Ten branching vertices; the remaining ring segments are forced.
        auto sparse_core = signed_ring;
        for (int j = 0; j < 10; ++j) {
            int u = j*n/10, v = ((j+3)%10)*n/10;
            sparse_core.push_back({u,v,5+u-v});
        }
        std::reverse(sparse_core.begin(), sparse_core.end());
        run("sparse_branch_core", n, sparse_core);
        // Subdivide a fixed-size random kernel. Independent potentials at
        // forced vertices obscure the nonnegative underlying path costs.
        std::vector<Edge> subdivided;
        constexpr int roots = 10, kernel_edges = 40;
        int next_vertex = roots;
        for (int u = 0; u < roots; ++u) for (int j = 0; j < 4; ++j) {
            int edge_number = u*4+j;
            int v = j == 0 ? (u+1)%roots : int(random()%roots);
            int forced = (n-roots)/kernel_edges + (edge_number < (n-roots)%kernel_edges);
            int previous = u;
            for (int k = 0; k <= forced; ++k) {
                int next = k == forced ? v : next_vertex++;
                subdivided.push_back({previous,next,k == 0 ? 1+int(random()%100) : 0});
                previous = next;
            }
        }
        for (auto& p : potential) p = int(random()%200001)-100000;
        for (auto& e : subdivided) e.weight += potential[e.to]-potential[e.from];
        std::reverse(subdivided.begin(), subdivided.end());
        run("subdivided_random_kernel", n, subdivided);
    }
}

#include "shortest_paths.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

using namespace shortest_paths;
using Clock = std::chrono::steady_clock;

void run(const std::string& family, int n, const std::vector<Edge>& edges) {
    std::vector<double> hybrid_times, reference_times;
    Result hybrid, reference;
    for (int repeat = 0; repeat < 5; ++repeat) {
        // Alternate order to reduce systematic warm-cache bias.
        auto measure = [&](bool adaptive) {
            auto start = Clock::now();
            auto result = adaptive ? solve(n, edges, 0) : bellman_ford(n, edges, 0);
            double ms = std::chrono::duration<double, std::milli>(Clock::now()-start).count();
            if (adaptive) { hybrid = std::move(result); hybrid_times.push_back(ms); }
            else { reference = std::move(result); reference_times.push_back(ms); }
        };
        measure(repeat%2 == 0);
        measure(repeat%2 != 0);
        if (hybrid.negative_cycle.empty() != reference.negative_cycle.empty() ||
            hybrid.distances != reference.distances)
            throw std::logic_error("benchmark result mismatch");
        for (const auto* result : {&hybrid, &reference}) {
            if (result->negative_cycle.empty()) continue;
            Distance weight = 0;
            const auto& cycle = result->negative_cycle;
            for (std::size_t j = 0; j < cycle.size(); ++j) {
                const auto& e = edges.at(cycle[j]);
                if (e.to != edges.at(cycle[(j+1)%cycle.size()]).from)
                    throw std::logic_error("invalid cycle witness");
                weight = add(weight, e.weight);
            }
            if (weight >= 0) throw std::logic_error("nonnegative witness");
        }
    }
    std::sort(hybrid_times.begin(), hybrid_times.end());
    std::sort(reference_times.begin(), reference_times.end());
    std::cout << family << ',' << n << ',' << edges.size() << ','
              << hybrid.method << ',' << hybrid.used_fallback << ','
              << hybrid.edge_scans << ',' << reference.edge_scans << ','
              << hybrid_times[2] << ',' << reference_times[2] << '\n';
}

int main() {
    std::cout << "family,n,m,method,fallback,hybrid_scans,bf_scans,hybrid_median_ms,bf_median_ms\n";
    std::cout << std::fixed << std::setprecision(6);
    std::mt19937 random(20261103);
    for (int n : {100, 500, 1000}) {
        std::vector<Edge> positive, mixed, dag, ring, reversed;
        std::vector<std::int64_t> potential(n);
        for (auto& p : potential) p = int(random()%2001)-1000;
        for (int u = 0; u < n; ++u) {
            ring.push_back({u,(u+1)%n,-1});
            // Reverse edge order forces one useful chain extension per BF pass.
            if (u > 0) reversed.push_back({n-u-1,n-u,-1});
            for (int j = 0; j < 6; ++j) {
                int v = j == 0 ? (u+1)%n : int(random()%n);
                std::int64_t base = 1 + random()%100;
                positive.push_back({u,v,base});
                // Every cycle telescopes to sum(base)>0, so no negative cycle.
                mixed.push_back({u,v,base-potential[u]+potential[v]});
                if (v > u) dag.push_back({u,v,int(random()%201)-100});
            }
        }
        run("nonnegative", n, positive);
        run("signed_potential", n, mixed);
        run("signed_dag", n, dag);
        run("negative_ring", n, ring);
        run("reverse_order_chain", n, reversed);
    }
}

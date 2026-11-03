// Historical experiment: raw relaxation counts can falsely report cycles.
// See ALGORITHM.md and shortest_paths.hpp for the verified implementation.
#include <iostream>
#include <vector>
#include <deque>
#include <limits>
#include <numeric>
#include <fstream>
#include <iomanip>

using namespace std;

const long long INF = numeric_limits<long long>::max();

struct Edge {
    int to;
    int weight;
};

// Result structure to return both distance and status
struct SpfaResult {
    vector<long long> distances;
    bool has_negative_cycle;
    long long visit_count;
};

/**
 * Adaptive Priority SPFA (AP-SPFA)
 * 
 * Features:
 * - Uses a Deque for graph traversal (SLF optimization)
 * - Detects negative cycles via relaxation count
 * - Handles negative weights
 */
SpfaResult ap_spfa(int V, const vector<vector<Edge>>& adj, int source) {
    vector<long long> dist(V, INF);
    vector<int> relax_count(V, 0);
    vector<bool> in_queue(V, false);
    deque<int> dq;
    
    long long total_visits = 0;

    dist[source] = 0;
    dq.push_back(source);
    in_queue[source] = true;

    while (!dq.empty()) {
        int u = dq.front();
        dq.pop_front();
        in_queue[u] = false;
        total_visits++;

        for (const auto& edge : adj[u]) {
            int v = edge.to;
            int weight = edge.weight;

            if (dist[u] != INF && dist[u] + weight < dist[v]) {
                dist[v] = dist[u] + weight;
                relax_count[v]++;

                // Negative cycle detection
                if (relax_count[v] >= V) {
                    return { {}, true, total_visits };
                }

                if (!in_queue[v]) {
                    // SLF Optimization (Small Label First)
                    // If the new distance to v is smaller than the distance 
                    // of the node at the front of the queue, push to front.
                    if (!dq.empty() && dist[v] < dist[dq.front()]) {
                        dq.push_front(v);
                    } else {
                        dq.push_back(v);
                    }
                    in_queue[v] = true;
                }
            }
        }
    }

    return { dist, false, total_visits };
}

// Helper to add edges
void add_edge(vector<vector<Edge>>& adj, int u, int v, int w) {
    adj[u].push_back({v, w});
}

// Simple test runner
int main() {
    int V = 5;
    vector<vector<Edge>> adj(V);

    // Graph with negative edges but no cycle
    // 0 -> 1 (-10)
    // 0 -> 2 (40)
    // 1 -> 3 (30)
    // 2 -> 3 (10)
    // 3 -> 4 (5)
    // Path 0->1->3->4 = -10 + 30 + 5 = 25
    // Path 0->2->3->4 = 40 + 10 + 5 = 55
    add_edge(adj, 0, 1, -10);
    add_edge(adj, 0, 2, 40);
    add_edge(adj, 1, 3, 30);
    add_edge(adj, 2, 3, 10);
    add_edge(adj, 3, 4, 5);

    cout << "Running AP-SPFA on test graph..." << endl;
    SpfaResult result = ap_spfa(V, adj, 0);

    if (result.has_negative_cycle) {
        cout << "Negative cycle detected!" << endl;
    } else {
        cout << "Shortest distances from source 0:" << endl;
        for (int i = 0; i < V; ++i) {
            cout << "Node " << i << ": " << (result.distances[i] == INF ? "INF" : to_string(result.distances[i])) << endl;
        }
        cout << "Total queue operations: " << result.visit_count << endl;
    }

    // Test Negative Cycle
    cout << "\nRunning Negative Cycle Test..." << endl;
    vector<vector<Edge>> adj_cycle(3);
    add_edge(adj_cycle, 0, 1, 1);
    add_edge(adj_cycle, 1, 2, -5);
    add_edge(adj_cycle, 2, 0, 2); // Cycle 0->1->2->0 weight: 1 - 5 + 2 = -2
    
    SpfaResult cycle_result = ap_spfa(3, adj_cycle, 0);
    if (cycle_result.has_negative_cycle) {
        cout << "PASSED: Negative cycle correctly detected." << endl;
    } else {
        cout << "FAILED: Negative cycle NOT detected." << endl;
    }

    return 0;
}

// Historical timings compare different stopping conditions; see ALGORITHM.md.
// Use shortest_paths_benchmark.cpp for checked, reproducible comparisons.
#include <algorithm>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <vector>

using namespace std;

const long long INF = numeric_limits<long long>::max();

struct Edge {
  int to;
  int weight;
};

// ================= ALGORITHMS =================

// 1. Standard Bellman-Ford
vector<long long> bellman_ford(int V, const vector<vector<Edge>> &adj,
                               int source) {
  vector<long long> dist(V, INF);
  dist[source] = 0;

  // V-1 iterations
  for (int i = 0; i < V - 1; ++i) {
    bool changed = false;
    for (int u = 0; u < V; ++u) {
      if (dist[u] == INF)
        continue;
      for (const auto &edge : adj[u]) {
        if (dist[u] + edge.weight < dist[edge.to]) {
          dist[edge.to] = dist[u] + edge.weight;
          changed = true;
        }
      }
    }
    if (!changed)
      break;
  }
  return dist; // Negative cycle check omitted for speed in this benchmark
               // unless needed
}

// 2. Standard SPFA (Queue-based)
vector<long long> spfa(int V, const vector<vector<Edge>> &adj, int source,
                       long long &ops) {
  vector<long long> dist(V, INF);
  vector<bool> in_queue(V, false);
  queue<int> q;

  dist[source] = 0;
  q.push(source);
  in_queue[source] = true;
  ops = 0;

  while (!q.empty()) {
    int u = q.front();
    q.pop();
    in_queue[u] = false;
    ops++;

    for (const auto &edge : adj[u]) {
      int v = edge.to;
      int weight = edge.weight;
      if (dist[u] != INF && dist[u] + weight < dist[v]) {
        dist[v] = dist[u] + weight;
        if (!in_queue[v]) {
          q.push(v);
          in_queue[v] = true;
        }
      }
    }
    // Safety break for negative cycles in random graphs
    if (ops > 20LL * V * V)
      return {};
  }
  return dist;
}

// 3. Adaptive Priority SPFA (AP-SPFA)
vector<long long> ap_spfa(int V, const vector<vector<Edge>> &adj, int source,
                          long long &ops) {
  vector<long long> dist(V, INF);
  vector<bool> in_queue(V, false);
  deque<int> dq; // Deque for SLF

  dist[source] = 0;
  dq.push_back(source);
  in_queue[source] = true;
  ops = 0;

  while (!dq.empty()) {
    int u = dq.front();
    dq.pop_front();
    in_queue[u] = false;
    ops++;

    for (const auto &edge : adj[u]) {
      int v = edge.to;
      int weight = edge.weight;

      if (dist[u] != INF && dist[u] + weight < dist[v]) {
        dist[v] = dist[u] + weight;
        if (!in_queue[v]) {
          // SLF Optimization
          if (!dq.empty() && dist[v] < dist[dq.front()]) {
            dq.push_front(v);
          } else {
            dq.push_back(v);
          }
          in_queue[v] = true;
        }
      }
    }
    // Safety break
    if (ops > 20LL * V * V)
      return {};
  }
  return dist;
}

// ================= UTILS =================

vector<vector<Edge>> generate_random_graph(int V, int E, bool allow_negative) {
  vector<vector<Edge>> adj(V);
  int added = 0;
  // Ensure connectivity from 0? simpler: random edges
  for (int i = 0; i < E; ++i) {
    int u = rand() % V;
    int v = rand() % V;
    int w = rand() % 1000;
    if (allow_negative && rand() % 5 == 0)
      w = -w; // 20% negative edges
    adj[u].push_back({v, w});
  }
  return adj;
}

int main() {
  srand(time(0));
  ofstream res("benchmark_results.txt");

  cout << "Running benchmarks..." << endl;
  cout << setw(10) << "V" << setw(10) << "E" << setw(15) << "Bellman(s)"
       << setw(15) << "SPFA(s)" << setw(15) << "AP-SPFA(s)" << endl;
  res << "V E Bellman SPFA AP-SPFA" << endl;

  // Benchmark loop
  for (int V = 1000; V <= 10000; V += 1000) {
    int E = V * 10;                               // Sparse graph
    auto adj = generate_random_graph(V, E, true); // True = has negative edges

    // 1. Bellman-Ford
    clock_t start = clock();
    bellman_ford(V, adj, 0);
    double time_bf = double(clock() - start) / CLOCKS_PER_SEC;

    // 2. Standard SPFA
    long long ops_spfa = 0;
    start = clock();
    spfa(V, adj, 0, ops_spfa);
    double time_spfa = double(clock() - start) / CLOCKS_PER_SEC;

    // 3. AP-SPFA
    long long ops_ap = 0;
    start = clock();
    ap_spfa(V, adj, 0, ops_ap);
    double time_ap = double(clock() - start) / CLOCKS_PER_SEC;

    cout << setw(10) << V << setw(10) << E << setw(15) << time_bf << setw(15)
         << time_spfa << setw(15) << time_ap << endl;

    res << V << " " << E << " " << time_bf << " " << time_spfa << " " << time_ap
        << endl;
  }

  cout << "\nBenchmark complete. Results saved to benchmark_results.txt"
       << endl;

  // Quick adversarial test
  cout << "\nComparing Operation Counts on Dense Graph (V=2000, E=100000)..."
       << endl;
  int V = 2000;
  int E = 100000;
  auto adj = generate_random_graph(V, E, true);

  long long ops_spfa = 0;
  spfa(V, adj, 0, ops_spfa);

  long long ops_ap = 0;
  ap_spfa(V, adj, 0, ops_ap);

  cout << "Standard SPFA Operations: " << ops_spfa << endl;
  cout << "AP-SPFA Operations:       " << ops_ap << endl;
  double improvement = 100.0 * (double(ops_spfa - ops_ap) / ops_spfa);
  cout << "Improvement:              " << improvement
       << "% reduction in operations" << endl;

  return 0;
}

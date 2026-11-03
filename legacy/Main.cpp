// Historical experiment: relaxation counts can falsely report negative cycles.
// See ALGORITHM.md and shortest_paths.hpp for the verified implementation.
#include <iostream>
#include <vector>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <limits>
#include <unordered_set>
#include <algorithm>
#include <queue>


using namespace std;


// Function declarations
vector<vector<int>> generate_graph_W(int vertices);
vector<vector<int>> generate_graph(int vertices, int edges);
vector<int> bellman_ford(int vertices, const vector<vector<int>>& edges, int source);
vector<int> bellman_ford_optimized(int vertices, const vector<vector<int>>& edges, int source);
void write_to_file(const vector<pair<int, double>>& plot_data, const string& filename);
bool check_comp(int V, vector<vector<int>>& edges, int S);
void DFS(int node,vector<vector<int>>& adj,vector<bool>& vis);

int main()
{
    int plot_size, plot_gap;
    char mode='d';
    while(mode!='A' && mode!='W'){
    cout<<"Run average case (A) or worst case (W)\n";
    cin>>mode;
    }
    cout << "Enter plot size and gap respectively\n";
    cin >> plot_size >> plot_gap;
    // 1000 10

    vector<pair<int, double>> plot1, plot2; // { size of graph V+E , time to compute shortest distance }
    vector<vector<int>> edges;
    double elapsed;
    int S = 0;  // Source vertex

    for (int i = 1; i <= plot_size+6; i += plot_gap) {
        int edge_max = (i * (i - 1) / 2) - 1;
        int V = i, E = i+rand()%edge_max;
        edges = (mode=='W')?generate_graph_W(V):generate_graph(V, E);

        if(check_comp(V,edges,S)){
        cout<<"More than one Component\n";
        continue;
        }

        // Measure time for optimized Bellman-Ford
        clock_t curr_time = clock();
        vector<int> shortest_path1 = bellman_ford_optimized(V, edges, S);
        curr_time = clock() - curr_time;
        elapsed = double(curr_time) / CLOCKS_PER_SEC;
        plot1.push_back({V + E, elapsed});
        cout<<V+E<<" "<<elapsed<<" -- ";

        // Measure time for regular Bellman-Ford
        curr_time = clock();
        vector<int> shortest_path2 = bellman_ford(V, edges, S);
        curr_time = clock() - curr_time;
        elapsed = double(curr_time) / CLOCKS_PER_SEC;
        plot2.push_back({V + E, elapsed});

        if (shortest_path1 != shortest_path2) {
            cout << "Error in code for a test case" << endl;
            for(auto i:edges){
                cout<<i[0]<<"--"<<i[1]<<"-- W ->"<<i[2]<<" | ";
            }
            cout<<endl;
            for(int i=0;i<shortest_path1.size();++i){
                cout<<shortest_path1[i]<<" -- ";
            }
            cout<<endl;
            for(int i=0;i<shortest_path2.size();++i){
                cout<<shortest_path2[i]<<" -- ";
            }
            cout<<endl;
            return 1;
        }
        cout<<V+E<<" "<<elapsed<<endl;
    }
    sort(plot1.begin(),plot1.end());
    sort(plot2.begin(),plot2.end());

    // Write plot data to files
    write_to_file(plot1, "plot1.txt"); // optimized one
    write_to_file(plot2, "plot2.txt"); // normal one

    return 0;
}


// Function to generate a random graph with a guaranteed connection from the source
vector<vector<int>> generate_graph(int vertices, int edges) {
    
    vector<vector<int>> edges_list;

    if(vertices>edges-1 || edges> (vertices * (vertices-1))/2)
    return edges_list;
    unordered_set<int> existing_edges;
    int count = 0;

    // Ensure at least one edge for the source node
    for (int i = 1; i < vertices && count < edges; ++i) {
        int w = rand() % 2001 - 1000; // Random weight between -1000 and 1000
        edges_list.push_back({i-1, i, w});
        existing_edges.insert((i-1)*(vertices+1)+i);
        ++count;
    }

    // Add remaining edges randomly, avoiding duplicates and self-loops
    while (count < edges) {
        int u = rand() % vertices;
        int v = rand() % vertices;

        if (u != v && !existing_edges.count(u*(vertices+1)+ v) && !existing_edges.count(v*(vertices+1)+u)) {
            int w = rand() % 2001 - 1000; // Random weight between -1000 and 1000
            edges_list.push_back({u, v, w});
            existing_edges.insert(u*(vertices+1)+ v);
            ++count;
        }
    }
    return edges_list;
}

// Function to generate a worst case graph with a guaranteed connection from the source
// case where graph is linked list cycle with negative weigth cycle
vector<vector<int>> generate_graph_W(int V) {
    vector<vector<int>> edges_list;
    for(int i=1;i<V;++i){
        edges_list.push_back({i-1,i,-1000000000});
    }
    edges_list.push_back({V-1,0,-1000000000});
    return edges_list;
}

// Standard Bellman-Ford algorithm
vector<int> bellman_ford(int V, const vector<vector<int>>& edges, int S) {
        vector <int> dist(V, numeric_limits<int>::max()) ; 
        dist[S]= 0 ; 
        for (int i = 0 ; i< V-1 ; i++){
            for (auto it : edges){
                int node = it[0]; 
                int des = it[1] ; 
                int weight = it[2]; 
                if (dist[node] != numeric_limits<int>::max() && dist[node]+ weight < dist[des]){
                    dist[des] = dist[node]+ weight ; 
                }
            }
        }
        // for negative cycle 
        for (auto it : edges){
                int node = it[0]; 
                int des = it[1] ; 
                int weight = it[2]; 
                if (dist[node] !=numeric_limits<int>::max()  && dist[node]+ weight < dist[des]){
                   return {} ;  
                }
                
        } 
        return dist ; 
}

// Optimized Bellman-Ford 
vector<int> bellman_ford_optimized(int V, const vector<vector<int>>& edges, int S) {
    vector<vector<pair<int, int>>> adj(V);
        vector<int> count_vis(V,0);
        vector<int> dist(V, numeric_limits<int>::max());  // Initialize distances to infinity
        priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;

        for (const auto& edge : edges) {
            adj[edge[0]].push_back({edge[1], edge[2]});
        }
        
        dist[S] = 0;
        pq.push({0, S});  // {distance, node}
    
        while (!pq.empty()) {
            int d = pq.top().first;  // Current node distance
            int u = pq.top().second;  // Current node
            pq.pop();
    
            if (d > dist[u]) {
                continue;
            }
    
            // Explore neighbors
            for (auto& edge : adj[u]) {
                int v = edge.first;    // Neighbor node
                int weight = edge.second;  // Weight of the edge

                
                // Relax the edge
                if (dist[u] + weight < dist[v]) {
                    ++count_vis[v];
                    if(count_vis[v]==V) {
                    return {};
                    }
                    dist[v] = dist[u] + weight;
                    pq.push({dist[v], v});
                }
            }
        }
        return dist;
}

// Write plot data to a file
void write_to_file(const vector<pair<int, double>>& plot_data, const string& filename) {
    ofstream file(filename);
    if (!file.is_open()) {
        cout << "Error opening file: " << filename << endl;
        return;
    }
    for (const auto& data : plot_data) {
        file << data.first << " " << data.second << endl;
    }
    file.close();
}


void DFS(int node,vector<vector<int>>& adj,vector<bool>& vis){
    vis[node]=true;
    for(int &i:adj[node]){
        if(vis[i]==false)
        DFS(i,adj,vis);
    }
}
bool check_comp(int V, vector<vector<int>>& edges, int S){
    vector<bool> vis(V);
    vector<vector<int>> adj(V);
        for (const auto& edge : edges) {
            adj[edge[0]].push_back(edge[1]);
        }
    DFS(S,adj,vis);
    for(int i=0;i<V;++i){
        if(vis[i]==false)
        return true;
    }
    return false;
    
}

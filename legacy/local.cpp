// Historical experiment: coloring/counting rules are not cycle certificates.
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
vector<int> bellman_ford(int vertices, const vector<vector<int>>& edges, int source,int& count);
vector<int> bellman_ford_optimized(int vertices, const vector<vector<int>>& edges, int source,int& count);
vector<int> bellman_ford_optimized2(int V, const vector<vector<int>>& edges, int S,int& count);
void write_to_file(const vector<pair<int, double>>& plot_data, const string& filename);
bool check_comp(int V, vector<vector<int>>& edges, int S);
void DFS(int node,vector<vector<int>>& adj,vector<bool>& vis);


// for best graph choose A , 50000,500,i%5=-ve,C=6;
int main()
{
    int plot_size, plot_gap;
    char mode='d',density='a';
    while(mode!='A' && mode!='W'){
    cout<<"Run average case (A) or worst case (W)\n";
    cin>>mode;
    }
    cout << "Enter plot size and gap respectively\n";
    cin >> plot_size >> plot_gap;
    // 1000 10

    if(mode=='A')
    while(density!='S' && density!='D' && density!='C'){
    cout<<"choose graph dense(D) or sparse(S) or choose Density(C)\n";
    cin>>density;
    }
    vector<pair<int, double>> plot1, plot2; // { size of graph V+E , time to compute shortest distance }
    vector<pair<int, double>> plot3, plot4; // { size of graph V+E , node visited }
    
    vector<vector<int>> edges;
    double elapsed;
    int S = 0;  // Source vertex
    int density_factor=3;
    if(density=='C'){

        cin>>density_factor;
    } 
    for (int i = 5; i <= plot_size+6; i += plot_gap) {
        int edge_max = (i * (i - 1) / 2) - 1;
        int V = i,E=density_factor*i;

        edges = (mode=='W')?generate_graph_W(V):generate_graph(V, E);

        if(check_comp(V,edges,S)){
        cout<<"More than one Component\n";
        continue;
        }

        // Measure time for optimized Bellman-Ford
        int count=0;
        clock_t curr_time = clock();
        vector<int> shortest_path1 = bellman_ford(V, edges, S,count);
        curr_time = clock() - curr_time;
        elapsed = double(curr_time) / CLOCKS_PER_SEC;
        plot1.push_back({E, elapsed});
        plot3.push_back({E,count});
        cout<<V+E<<" "<<elapsed<<" -- ";

        // Measure time for regular Bellman-Ford
        count=0;
        curr_time = clock();
        vector<int> shortest_path2 = bellman_ford_optimized2(V, edges, S,count);
        curr_time = clock() - curr_time;
        elapsed = double(curr_time) / CLOCKS_PER_SEC;
        plot2.push_back({E, elapsed});
        plot4.push_back({E, count});


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
    write_to_file(plot3, "plot3.txt"); // optimized one
    write_to_file(plot4, "plot4.txt"); // normal one


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
        int w = rand() % 2001 ; // Random weight between -1000 and 1000
        if(i%5==0){
        edges_list.push_back({i-1, i, -300});
        } else {
        edges_list.push_back({i-1, i, w});
        }
        existing_edges.insert((i-1)*(vertices+1)+i);
        ++count;
    }

    // Add remaining edges randomly, avoiding duplicates and self-loops
    while (count < edges) {
        int u = rand() % vertices;
        int v = rand() % vertices;

        if (u != v && !existing_edges.count(u*(vertices+1)+ v) && !existing_edges.count(v*(vertices+1)+u)) {
            int w = rand() % 2001 ; // Random weight between -1000 and 1000
            edges_list.push_back({u, v, w});
            existing_edges.insert(u*(vertices+1)+ v);
            ++count;
        }
    }
    int n=edges_list.size();
    for(int i=0;i<100;i++){
        int idx=rand()%n;
        edges_list[idx][2]=-100;
    }
    return edges_list;
}

// Function to generate a worst case graph with a guaranteed connection from the source
// case where graph is linked list cycle with negative weigth cycle
vector<vector<int>> generate_graph_W(int V) {
    vector<vector<int>> edges_list;
    for(int i=1;i<V;++i){
        edges_list.push_back({i-1,i,-100});
    }
    edges_list.push_back({V-1,0,-100});
    return edges_list;
}

// Standard Bellman-Ford algorithm
vector<int> bellman_ford(int V, const vector<vector<int>>& edges, int S,int& count1) {
     vector<vector<pair<int, int>>> adj(V);
    for (const auto& edge : edges) {
        adj[edge[0]].push_back({edge[1], edge[2]});
    }
    
    vector<int> color(V,0); // initially all colors are blue
    vector<int> dist(V, 1e8);  // Initialize distances to infinity
    int curr_color=1;
    dist[S] = 0;
    // Priority queue to pick the node with the smallest distance
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
    pq.push({0, S});  // {distance, node}

    while (!pq.empty()) {
        int d = pq.top().first;  // Current node distance
        int node = pq.top().second;  // Current node
        pq.pop();

        if (d > dist[node]) {
            continue;
        }
        
        // color change 
        if(color[node]!=curr_color)
        color[node]=++curr_color;
        
        // Explore neighbors
        for (auto& edge : adj[node]) {
            int neigh = edge.first;    // Neighbor node
            int weight = edge.second;  // Weight of the edge
            
            // Relax the edge
            int min_neigh=V,dis=1e8;
            if (dist[node] + weight < dist[neigh]) {
                if(color[neigh]==color[node]){
                    cout<<" N ";
                    cout<<"color :"<<color[node]<<endl;
                    return {};
                }
                if(dis>dist[neigh]){
                    min_neigh=neigh;
                    dis=dist[neigh];
                }

                // color[neigh]=color[node];
                dist[neigh] = dist[node] + weight;
                pq.push({dist[neigh], neigh});
            }

            if(dis!=1e8){
                color[min_neigh]=color[node];
            } else {
                ++curr_color;
            }
        }
    }
    
    return dist;
}

// Optimized Bellman-Ford 
vector<int> bellman_ford_optimized(int V, const vector<vector<int>>& edges, int S,int& count) {
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
            ++count;
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
                        cout<<" N ";
                    return {};
                    }
                    dist[v] = dist[u] + weight;
                    pq.push({dist[v], v});
                }
            }
        }
        return dist;
}

// Optimized Bellman-Ford using normal queue <<more optimized>>
vector<int> bellman_ford_optimized2(int V, const vector<vector<int>>& edges, int S,int& count) {
    vector<vector<pair<int, int>>> adj(V);
        vector<int> count_vis(V,0);
        vector<int> dist(V, numeric_limits<int>::max());  // Initialize distances to infinity
        queue<int> pq;

        for (const auto& edge : edges) {
            adj[edge[0]].push_back({edge[1], edge[2]});
        }
        
        dist[S] = 0;
        pq.push(S);  // {distance, node}
    
        while (!pq.empty()) {
            ++count;
            int u = pq.front();  // Current node
            pq.pop();
    
            // Explore neighbors
            for (auto& edge : adj[u]) {
                int v = edge.first;    // Neighbor node
                int weight = edge.second;  // Weight of the edge

                
                // Relax the edge
                if (dist[u] + weight < dist[v]) {
                    ++count_vis[v];
                    if(count_vis[v]==V) {
                        cout<<" N ";
                    return {};
                    }
                    dist[v] = dist[u] + weight;
                    pq.push(v);
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

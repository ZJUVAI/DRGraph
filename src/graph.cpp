#include "graph.h"

Graph::Graph(){}

Graph::Graph(int n_vertices, int n_edges): adjlist(n_vertices){
    this->n_vertices = n_vertices;
    this->n_edges = n_edges;
    // initialize vector<int>* adjlist;
    //this->adjlist = new vector<int>[n_vertices];
}

Graph::~Graph(){
    vector<vector<int>>().swap(adjlist);
}

int Graph::V(){
    return n_vertices;
}

int Graph::E(){
    return n_edges;
}

void Graph::add_edge(int from, int to){
    return adjlist[from].push_back(to);
}

vector<int>& Graph::neighbors(int vertice){
	return adjlist[vertice];
}

void Graph::shortest_path_length(int max_dist, vector<vector<int>>& sim_adj, vector<vector<float>>& sim_dist){
    //vector<int>* adj = new vector<int>[n_vertices];
    //vector<int>* dist = new vector<int>[n_vertices];
    #pragma omp parallel for
    for(int i = 0; i < n_vertices; i++){
        BFS(i, max_dist, sim_adj[i], sim_dist[i]);
    }
}

void Graph::BFS(int source, int max_dist, vector<int>& adj, vector<float>& dist){
    unordered_map<int, int> visited_dist;
    visited_dist[source] = 0;
    queue<int> Q;
    Q.push(source);
    while(!Q.empty()){
        auto target = Q.front();
        Q.pop();
        if(visited_dist[target] + 1 > max_dist)
            break;
        else{
            for(auto target_neighbor : neighbors(target)){
                if(visited_dist.find(target_neighbor) == visited_dist.end()){
                    Q.push(target_neighbor);
                    visited_dist[target_neighbor] = visited_dist[target] + 1;
                    adj.push_back(target_neighbor);
                    dist.push_back(visited_dist[target_neighbor]);
                }
            }
        }
    }
}

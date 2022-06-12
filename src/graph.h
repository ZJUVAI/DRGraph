#ifndef GRAPH_H
#define GRAPH_H

#include <vector>
#include <queue>
#include <unordered_map>
#include "probabilistic_graph.h"

using namespace std;

class Graph{
    private:
        int n_vertices;
        int n_edges;
        vector<vector<int>> adjlist;

    public:
        Graph();
        Graph(int n_vertices, int n_edges);
        ~Graph();
        int V();
        int E();
        void add_edge(int from, int to);
        vector<int>& neighbors(int vertice);
        void BFS(int source, int max_dist, vector<int>& adj, vector<float>& dist);
        void shortest_path_length(int max_dist, vector<vector<int>>& sim_adj, vector<vector<float>>& sim_weight);
};

#endif

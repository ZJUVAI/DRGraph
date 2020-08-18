#ifndef DATA_H
#define DATA_H

#include <stdlib.h>
#include <tr1/unordered_map>
#include <map>
#include <set>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <math.h>
#include <vector>
#include "knn.h"
#include "util.h"
#include <string>
#include <omp.h>

#define INF 2 << 22


struct Edge{
    int from, to, next;
    float weight;
    Edge() {}
    Edge(int _from, int _to, int _next, float _weight):from(_from), to(_to), next(_next), weight(_weight) {}
};


class Data{
private:
    vector<int> reverse;
    void search_reverse_thread(int id);
    static void *search_reverse_thread_caller(void *arg);

    void reserver_data(int vertices, int edge);

    void bfs(int source, vector<int> *distance, vector<int> *adjtarget);
    
    static int findFather(int *fa, int x) {
        return (fa[x] == x) ? x : (fa[x] = findFather(fa, fa[x]));
    }

    void add_one_edge(int v1, int v2, float weight);
    
    void add_edge(int v1, int v2, float weight=0);

    void solve_reverse_edge(bool no_pair_edge);
public:
    int n_vertices, n_edges = 0, n_threads;
    int disMAX = 1;
    
    // int* headTmp;
    float* similarity_weight = nullptr;
    int* head = nullptr;
    float perplexity;
    vector<Edge> graph;
    std::set<std::pair<int, int>> edgeSet;
    
    Data();

    ~Data();

    float* load_vec(char *infile, int &vertices, int &out_dim);

    void load_from_knn(knn& d);

    Data(const Data &);

    void testConnect();
    
    void load_from_graph(string& infile);

    

    
#ifdef USE_CUDA
    void compute_similarity_GPU(int n_thre, float perp);
#endif

    static void *compute_similarity_thread_caller(void *arg);

    void compute_similarity(int n_thre, float perp, bool normalizeOrNot = true, bool no_pair_edge = true);
    void compute_similarity_thread(int id);

    void shortest_path_length (int dismax);
    


};

#endif

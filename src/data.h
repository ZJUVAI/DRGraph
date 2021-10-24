#ifndef DATA_H
#define DATA_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <memory>
#include <math.h>
#include <omp.h>
#include <indicators/progress_bar.hpp>
#include <indicators/progress_spinner.hpp>
#include <indicators/cursor_control.hpp>
#include "graph.h"
#include "multilevel.h"

struct pargs{
    int id;
    void *ptr;
    pargs(void *x, int y) :ptr(x), id(y){};                  
};

class Data{
private:
    int n_threads = std::thread::hardware_concurrency();
    bool norm = false;
    int max_dist = 1;
	float perplexity = 50;
public:
    // [Input] Graph data
    Graph* graph;
    int n_vertices, n_edges, n_dims;

    // [Input] High dimensional data
    float* vec;

    // [DRGraph]
    std::vector<Multilevel*> multilevel_graphs;

    // [Output] Embedding
    float* embedding;
    
    Data(const Data &);
    Data();
    ~Data();

    void load_graph(std::string& file);
    void graph2dist(int max_dist = 1);
    void dist2weight();
    static void* dist2weight_thread_caller(void *args);
    void dist2weight_thread(int id);
    void build_multilevel();
    
    void load_vector(std::string& file);
};

#endif

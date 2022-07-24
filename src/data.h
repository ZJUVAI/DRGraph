#ifndef DATA_H
#define DATA_H

#include <filesystem>
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
#include "probabilistic_graph.h"

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

    // intermediate representation
    std::vector<std::vector<int>> adj;
    std::vector<std::vector<float>> weight; 
    
    void load_graph_from_binary(std::string& file);
    void load_graph_from_txt(std::string& file);
    void save_graph_txt_as_binary(std::string& infile, std::string& outfile);
public:
    // [Input] Graph data
    Graph* graph;
    int n_vertices, n_edges, n_dims;

    // [Input] High dimensional data
    float* vec;

    // [DRGraph]
    std::vector<ProbabilisticGraph*> multilevels;

    // [Output] Embedding
    float* embedding;
    
    Data(const Data &);
    Data(int n_dims);
    ~Data();

    void load_graph(std::string& file);
    void gen_probabilistic_graph(std::string method);
    void graph2dist(int max_dist);
    void dist2weight();
    static void* dist2weight_thread_caller(void *args);
    void dist2weight_thread(int id);
    void build_multilevel();
    void init_embedding();
    
    void load_vector(std::string& file);
};

#endif

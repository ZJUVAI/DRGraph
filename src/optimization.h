#ifndef OPTIMIZATION_H
#define OPTIMIZATION_H

#include <iostream>
#include <vector>

#include "random.h"
#include "aliasSampler.h"
#include "probabilistic_graph.h"

class Optimization{
private:
    int n_negatives = 7;
    float* params;
public:
    AliasSampler* edge_sampler;
    AliasSampler* vertice_sampler;
    // Random rnd{1234567};
    Random rnd = Random(1234567);


    Optimization();
    ~Optimization();
    void init_edge_sampler(int n, std::vector<std::vector<float>>& weight);
    void init_vertice_sampler(int n, float* weight);
    void init_params(int n_vertices, int n_dims, ProbabilisticGraph* prob_graph);
    void run();
};
#endif

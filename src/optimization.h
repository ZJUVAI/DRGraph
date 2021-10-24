#ifndef OPTIMIZATION_H
#define OPTIMIZATION_H

#include <iostream>
#include <vector>
#include "aliasSampler.h"

class Optimization{
private:
    int n_threads = 10;
    int n_negatives = 7;
public:
    AliasSampler* edge_sampler;
    AliasSampler* vertice_sampler;

    Optimization();
    ~Optimization();
    void run();

    void init_edge_sampler(int n, std::vector<std::vector<float>>& weight);
    void init_vertice_sampler(int n, float* weight);
};
#endif

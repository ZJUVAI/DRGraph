#include "optimization.h"

Optimization::Optimization(){
}

Optimization::~Optimization(){
}

void Optimization::run(){
    std::cout<< "hello world" <<std::endl;
}

void Optimization::init_edge_sampler(int n, std::vector<std::vector<float>>& weight){
    edge_sampler = new AliasSampler(n, weight);
}

void Optimization::init_vertice_sampler(int n, float* weight){
    vertice_sampler = new AliasSampler(n, weight, 0.75);
}

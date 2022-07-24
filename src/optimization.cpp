#include "optimization.h"

Optimization::Optimization(){
    
}

Optimization::~Optimization(){
}

void Optimization::init_edge_sampler(int n, std::vector<std::vector<float>>& weight){
    edge_sampler = new AliasSampler(n, weight);
}

void Optimization::init_vertice_sampler(int n, float* weight){
    vertice_sampler = new AliasSampler(n, weight, 0.75);
}

void Optimization::init_params(int n_vertices, int n_dims, ProbabilisticGraph *prob_graph){
    params = new float[n_vertices * n_dims];
    for(int i = 0; i < prob_graph->get_V(); ++i) {
        for(int j = 0; j < n_dims; ++j){
            params[i * 2 + j] = (float)((rnd.uniform() - 0.5) * 0.0001);
        }
    }
}

void Optimization::run(){
    std::cout<< "hello world" <<std::endl;
    std::cout << vertice_sampler->sample(0.1, 0.1) << std::endl;
    std::cout << edge_sampler->sample(0.1, 0.1) << std::endl;
}

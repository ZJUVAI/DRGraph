#ifndef MULTILEVEL_h
#define MULTILEVEL_h

#include <iostream>
#include <vector>
#include <algorithm>
//#include <fmt/format.h>
//#include <fmt/ranges.h>
#include "random.h"

class Multilevel{
private:
    int level = 0;
    int V = 0;
    int E = 0;
    std::vector<std::vector<int>> adj;
    std::vector<std::vector<float>> weights;
    std::vector<std::pair<int, int>> edges;
    // std::vector<float> edge_weights;
    // index from this graph to next graph, e.g., [1,N] => [1,n]
    std::vector<int> vertice_mapping;
    float* masses = nullptr;
    // Multilevel* nextlevel = nullptr;
    
public:
    Multilevel();
    Multilevel(int level, int V);
    //Multilevel(std::vector<std::vector<int>>* adj, std::vector<std::vector<float>>* weight);
    ~Multilevel();
    int get_V();
    int get_E();
    int get_level();
    void add_edge(int from, int to);
    void add_edge(int from, int to, float weight);
    Multilevel* coarse();
    std::vector<int>& get_neighbors(int vertice);
    std::vector<std::vector<int>>& get_adj();
    std::vector<std::vector<float>>& get_weight();
};

#endif 

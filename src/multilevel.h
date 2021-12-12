#ifndef MULTILEVEL_h
#define MULTILEVEL_h

#include <iostream>
#include <vector>
#include <algorithm>
//#include <fmt/format.h>
//#include <fmt/ranges.h>
#include "random.h"

class Multilevel{
public:
    // int V=0;
    // int E=0;

private:
    std::vector<std::vector<int>> adj;
    std::vector<std::vector<float>> weight;
    std::vector<std::pair<int, int>> edges;
    // index from this graph to next graph, e.g., [1,N] => [1,n]
    std::vector<int> vertice_mapping;
    float* masses = nullptr;
    int level = 0;
    Multilevel* nextlevel = nullptr;
    

public:
    Multilevel();
    Multilevel(int level, int V);
    //Multilevel(std::vector<std::vector<int>>* adj, std::vector<std::vector<float>>* weight);
    ~Multilevel();
    void resize(int V);
    void build_index();
    Multilevel* coarse();
    int get_V();
    int get_E();
    void add_edge(int from, int to);
    void add_edge(int from, int to, float weight);
    std::vector<int>& get_neighbors(int vertice);
    std::vector<std::vector<int>>& get_adj();
    std::vector<std::vector<float>>& get_weight();
};

#endif 

#ifndef MULTILEVEL_h
#define MULTILEVEL_h

#include <iostream>
#include <vector>

class Multilevel{
private:
    int V;
    int E;
    float* masses = nullptr;
    std::vector<std::vector<int>> adj;
    std::vector<std::vector<float>> weight;
    std::vector<std::pair<int, int>> edges;
    Multilevel* nextlevel = nullptr;
    

public:
    Multilevel();
    //Multilevel(std::vector<std::vector<int>>* adj, std::vector<std::vector<float>>* weight);
    ~Multilevel();
    void resize(int V);
    void build_index();
    void coarse();
    //void set_V(int V);
    int get_V();
    //void set_E(int E);
    int get_E();
    void add_edge(int from, int to);
    void add_edge(int from, int to, float weight);
    std::vector<int>& get_neighbors(int vertice);
    std::vector<std::vector<int>>& get_adj();
    std::vector<std::vector<float>>& get_weight();
};

#endif 

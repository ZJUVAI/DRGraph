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

#define INF 2 << 22


struct Edge{
    int from, to, next;
    float weight;
    Edge() {}
    Edge(int _from, int _to, int _next, float _weight):from(_from), to(_to), next(_next), weight(_weight) {}
};


class Data{
private:
    void add_edge(int v1, int v2, float weight=0);

public:
    int n_vertices, n_edges, n_dims;

    // High dimensional data
    float *vec;

    
    std::vector<Edge> graph;
    std::set<std::pair<int, int>> edgeSet;
    
    Data(const Data &);
    
    Data();

    ~Data();

    void load_graph(std::string& file);
    
    void load_vector(std::string& file);
};

#endif

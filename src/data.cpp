#ifndef DATA_CPP
#define DATA_CPP

#include "data.h"
#include <map>
#include <iostream>
#include <malloc.h>
#include <cfloat>
#include <queue>


Data::Data() {}

Data::~Data() {
}

Data::Data(const Data &d) {
}


void Data::load_graph(std::string& file){
    /*
    vertices(n) edges(m)
    fromNode_1 toNode_1 weight_1
    fromNode_2 toNode_2 weight_2
    ...
    fromNode_m toNode_m weight_m
    */    
    int source, target;
    float weight;
    std::ifstream fin(file.c_str());
    std::string line;
    if (fin) {
        std::getline(fin, line);
        std::istringstream iss(line);
        iss >> n_vertices >> n_edges;
        std::cout << "[DATA] Reading graph edges from " << file << " with #V = " << n_vertices << " and #E = " << n_edges << std::endl;
        for (int i = 0; i < n_edges; i++) {
            std::getline(fin, line);
            std::istringstream iss(line);
            iss >> source >> target >> weight;
            add_edge(source, target, weight);
            add_edge(target, source, weight);
            if (i % 1000 == 0) {
                std::cout << "\r[DATA] Reading " << i << " edges" << std::flush;
            }
        }
        std::cout << "\r[DATA] Reading " << n_edges << " edges" << std::endl;
    } else {
        std::cout << "graph file not found!" << std::endl;
        exit(1);
    }
    fin.close();

    for(Edge e : graph){
        std::cout << e.from << " " << e.to << " " << e.weight << std::endl;
        break;
    }
}

void Data::add_edge(int v1, int v2, float weight) {
    graph.push_back(Edge(v1, v2, -1, weight));
}

void Data::load_vector(std::string& file){
    /*
    n_vertices n_dim
    n1_d1 n1_d2 n1_d3
    n2_d1 n2_d2 n2_d3
    ...
    */    
    std::ifstream fin(file.c_str());
    std::string line;
    float tmp;
    if (fin) {
        std::getline(fin, line);
        std::istringstream iss(line);
        iss >> n_vertices >> n_dims;
        std::cout << "[DATA] Reading vectors from " << file << " with #V = " << n_vertices << " and #D = " << n_dims << std::endl;
        vec = new float[n_vertices * n_dims];
        for (int i = 0; i < n_vertices; i++) {
            std::getline(fin, line);
            std::istringstream iss(line);
            for (int j = 0; j < n_dims; j++) {
                iss >> tmp;
                vec[i * n_dims + j] = tmp;
            }
            if (i % 1000 == 0) {
                std::cout << "\r[DATA] Reading " << i << " vectors" << std::flush;
            }
        }
        std::cout << "\r[DATA] Reading " << n_vertices << " vectors" << std::endl;
    } else {
        std::cout << "graph file not found!" << std::endl;
        exit(1);
    }
    fin.close();
}



#endif

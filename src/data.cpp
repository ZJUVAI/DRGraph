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
        std::cout << "[DATA] Loading graph edges from " << file << " with #V=" << n_vertices << " and #E=" << n_edges << std::endl;

        indicators::ProgressSpinner bar{
            indicators::option::PrefixText{"[DATA] "},
            indicators::option::MaxProgress{n_edges},
            indicators::option::SpinnerStates{std::vector<std::string>{"⠈", "⠐", "⠠", "⢀", "⡀", "⠄", "⠂", "⠁"}},
            indicators::option::ShowElapsedTime{true},
            indicators::option::ShowRemainingTime{true},
        };
        indicators::show_console_cursor(false);
        for (int i = 0; i < n_edges; i++) {
            std::getline(fin, line);
            std::istringstream iss(line);
            iss >> source >> target >> weight;
            add_edge(source, target, weight);
            add_edge(target, source, weight);
            if ((i + 1) % 1000 == 0 || i == n_edges - 1) {
                bar.set_option(indicators::option::PostfixText{"[" + std::to_string(i + 1) + "/" + std::to_string(n_edges) + "]"});
                bar.set_progress(i + 1);
            }
        }
        bar.set_option(indicators::option::PrefixText{"[DATA] ✔ "});
        bar.set_option(indicators::option::ShowSpinner{false});
        bar.set_option(indicators::option::ShowPercentage{false});
        bar.set_option(indicators::option::ShowElapsedTime{false});
        bar.set_option(indicators::option::ShowRemainingTime{false});
        bar.set_option(indicators::option::PostfixText{"Graph loaded " + std::to_string(n_edges) + "/" + std::to_string(n_edges) });
        bar.mark_as_completed();
        indicators::show_console_cursor(true);
    } else {
        std::cout << "[DATA] graph file not found!" << std::endl;
        exit(1);
    }
    fin.close();
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
        std::cout << "[DATA] Load vectors from " << file << " with #V=" << n_vertices << " and #D=" << n_dims << std::endl;
        vec = new float[n_vertices * n_dims];

        indicators::ProgressSpinner bar{
            indicators::option::PrefixText{"[DATA] "},
            indicators::option::MaxProgress{n_vertices},
            indicators::option::SpinnerStates{std::vector<std::string>{"⠈", "⠐", "⠠", "⢀", "⡀", "⠄", "⠂", "⠁"}},
            indicators::option::ShowElapsedTime{true},
            indicators::option::ShowRemainingTime{true},
        };
        indicators::show_console_cursor(false);
        for (int i = 0; i < n_vertices; i++) {
            std::getline(fin, line);
            std::istringstream iss(line);
            for (int j = 0; j < n_dims; j++) {
                iss >> tmp;
                vec[i * n_dims + j] = tmp;
            }
            if ((i + 1) % 1000 == 0 || i == n_vertices - 1) {
                bar.set_option(indicators::option::PostfixText{"[" + std::to_string(i + 1) + "/" + std::to_string(n_vertices) + "]"});
                bar.set_progress(i + 1);
            }
        }
        bar.set_option(indicators::option::PrefixText{"[DATA] ✔ "});
        bar.set_option(indicators::option::ShowSpinner{false});
        bar.set_option(indicators::option::ShowPercentage{false});
        bar.set_option(indicators::option::ShowElapsedTime{false});
        bar.set_option(indicators::option::ShowRemainingTime{false});
        bar.set_option(indicators::option::PostfixText{"Vectors loaded " + std::to_string(n_vertices) + "/" + std::to_string(n_vertices)});
        bar.mark_as_completed();
        indicators::show_console_cursor(true);
    } else {
        std::cout << "[DATA] vector file not found!" << std::endl;
        exit(1);
    }
    fin.close();
}



#endif

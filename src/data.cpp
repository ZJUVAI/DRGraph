#ifndef DATA_CPP
#define DATA_CPP

#include <map>
#include <iostream>
#include <malloc.h>
#include <cfloat>
#include <queue>

#include "data.h"


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
        //Graph graph2(n_vertices, n_edges);
        graph = new Graph(n_vertices, n_edges);
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
            graph->add_edge(source, target);
            graph->add_edge(target, source);
            if ((i + 1) % 10000 == 0 || i == n_edges - 1) {
                bar.set_option(indicators::option::PostfixText{"[" + std::to_string(i + 1) + "/" + std::to_string(n_edges) + "]"});
                bar.set_progress(i + 1);
            }
        }
        bar.set_option(indicators::option::PrefixText{"[DATA]"});
        bar.set_option(indicators::option::ShowSpinner{false});
        bar.set_option(indicators::option::ShowPercentage{false});
        bar.set_option(indicators::option::ShowElapsedTime{false});
        bar.set_option(indicators::option::ShowRemainingTime{false});
        bar.set_option(indicators::option::PostfixText{"Graph loaded ✔                                  "});
        bar.mark_as_completed();
        indicators::show_console_cursor(true);
    } else {
        std::cout << "[DATA] graph file not found!" << std::endl;
        exit(1);
    }
    fin.close();
}

void Data::graph2dist(int max_dist){
    //Get adj and weight of fisrt level graph by bfs
    auto multilevel = new Multilevel();
    multilevel->resize(n_vertices);
    graph->shortest_path_length(max_dist, multilevel->get_adj(), multilevel->get_weight());
    delete graph;
    multilevel_graphs.push_back(multilevel);
}

void Data::dist2weight(){
    pthread_t threads[n_threads];
    for(int i = 0; i < n_threads; i++ ){
        int rc = pthread_create(&threads[i], NULL, Data::dist2weight_thread_caller, new pargs(this, i));
    }
    for(int i = 0; i < n_threads; i++ ){pthread_join(threads[i], NULL);}
}


void* Data::dist2weight_thread_caller(void* args){
    pargs* arg = (pargs*)args;
    Data* ptr = (Data*)arg->ptr;
    int id = arg->id;
    ptr->dist2weight_thread(id);
    pthread_exit(NULL);
}

void Data::dist2weight_thread(int id){
    int low = id * n_vertices / n_threads;
    int high = (id + 1) * n_vertices / n_threads;
    std::vector<std::vector<int>>& adj = multilevel_graphs[0]->get_adj();
    std::vector<std::vector<float>>& weight = multilevel_graphs[0]->get_weight();
    if(norm){
        float beta, lo_beta, hi_beta, H, sum_weight, tmp;
        for(int i = low; i < high; ++i){
            beta = 1;
            lo_beta = hi_beta = -1;
            for(int iter = 0; iter < 200; ++iter){
                H = 0;
                sum_weight = FLT_MIN;
                for(int j = 0; j < (adj)[i].size(); ++j){
                    sum_weight += tmp = exp(-beta * (weight)[i][j]);
                    H += beta * ((weight)[i][j] * tmp);
                }
                H = (H / sum_weight) + log(sum_weight);
                if(fabs(H - log(perplexity)) < 1e-5){break;}
            	if (H > log(perplexity)){
                	lo_beta = beta;
                	if (hi_beta < 0) beta *= 2; else beta = (beta + hi_beta) / 2;
            	}else{
                	hi_beta = beta;
                	if (lo_beta < 0) beta /= 2; else beta = (lo_beta + beta) / 2;
            	}
            	if(beta > FLT_MAX) beta = FLT_MAX; 
            }
		}
	}else{
        float exparr[max_dist];
        for (int i = 0; i <= max_dist; ++i) {
            exparr[i] = exp(-i);//-i*i?                    
        }
        float sum_weight = 0;
        for(int i = low; i < high; ++i){
            float sum_i = 0;
            for(int j = 0; j < (adj)[i].size(); ++j){
                sum_i += (weight)[i][j] = exparr[(int)(weight)[i][j]];
            }
            //sum_weight += sum_i;
            //vertice_weight[i] = sum_i;
        }
        //for(int i = low; i < high; ++i){
        //    for(int j = 0; j < sim_adj[i].size(); ++j){
        //        sim_weight[i][j] = sim_weight[i][j] * n_vertices / sum_weight;
        //    }
        //}    
    }
}

void Data::build_multilevel(){
    multilevel_graphs[0]->build_index();
    multilevel_graphs[0]->coarse();
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
        std::filesystem::path filepath = file;
        std::cout << "[DATA] Load vectors from " << std::filesystem::absolute(filepath) << " with #V=" << n_vertices << " and #D=" << n_dims << std::endl;
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
        bar.set_option(indicators::option::PrefixText{"[DATA]"});
        bar.set_option(indicators::option::ShowSpinner{false});
        bar.set_option(indicators::option::ShowPercentage{false});
        bar.set_option(indicators::option::ShowElapsedTime{false});
        bar.set_option(indicators::option::ShowRemainingTime{false});
        bar.set_option(indicators::option::PostfixText{"Vectors loaded  ✔                           "});
        bar.mark_as_completed();
        indicators::show_console_cursor(true);
    } else {
        std::cout << "[DATA] vector file not found!" << std::endl;
        exit(1);
    }
    fin.close();
}


#endif

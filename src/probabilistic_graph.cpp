#include "probabilistic_graph.h"

ProbabilisticGraph::ProbabilisticGraph(){}

ProbabilisticGraph::ProbabilisticGraph(int level, int V){
    this->level = level;
    this->V = V;
    adj.resize(V);
    weights.resize(V);
    masses = new float[V]();
}

ProbabilisticGraph::~ProbabilisticGraph(){
    for(int i = 0; i < this->V; i++){
        std::vector<int>().swap((adj)[i]);
        std::vector<float>().swap((weights)[i]);
    }
    std::vector<std::vector<int>>().swap(adj);
    std::vector<std::vector<float>>().swap(weights);
    delete[] masses;
    //delete nextlevel;
}

ProbabilisticGraph* ProbabilisticGraph::coarse(){
    // init
    int V_new = 0;
    vertice_mapping.resize(V);
    std::vector<int> vertice_pool(V, 0); // in random order
    int vertice_pool_size = V;
    std::vector<int> vertice_pool_index(V, 0); // vector => index in pool
    std::vector<bool> vertice_visited(V, false);
    //std::vector<int> vertice_mapping(V, -1); // index in the next coarser graph
    for(int i = 0; i < V; i++){
        vertice_pool[i] = i;
        vertice_pool_index[i] = i;
        vertice_mapping[i] = -1;
    }
    
    Random rnd;
    auto remove_vertice_from_pool = [&] (int choosen_pool_index) {
        int choosen_vertice = vertice_pool[choosen_pool_index];
        // set choosen vertice as visited
        vertice_visited[choosen_vertice] = true;
        // swap the last element forward and remove the choosen element
        // also update pool index
        int last_vertice = vertice_pool[vertice_pool_size-1]; 
        vertice_pool[choosen_pool_index] = last_vertice;
        vertice_pool_index[last_vertice] = choosen_pool_index;
        vertice_pool[vertice_pool_size-1] = choosen_vertice;
        vertice_pool_index[choosen_vertice] = vertice_pool_size-1;
        // remove the choosen element by shinking pool size -- 
        vertice_pool_size--;
    };
    while(vertice_pool_size > 0){
        int choosen_pool_index = (int)floor(rnd.uniform() * vertice_pool_size);
        int vertice = vertice_pool[choosen_pool_index];
        //remove choosen vertice from pool
        remove_vertice_from_pool(choosen_pool_index);
        //set new index
        vertice_mapping[vertice] = V_new;
        // for neighbors of the choosen vertice
        for(auto neighbor : get_neighbors(vertice)){
            if(vertice_visited[neighbor] == false){
                //remove choosen vertice from pool
                choosen_pool_index = vertice_pool_index[neighbor];
                remove_vertice_from_pool(choosen_pool_index);
                // set new index
                vertice_mapping[neighbor] = V_new;
            }
        }
        V_new++;
    }

    // new nextlevel
    auto nextlevel = new ProbabilisticGraph(level+1, V_new);
    for(int i = 0; i < V; i++){
        for(int j = 0; j < adj[i].size(); j++){
            int vertice_from = vertice_mapping[i];
            int vertice_to = vertice_mapping[adj[i][j]];
            if(vertice_from == vertice_to){
                continue;
            }else if (vertice_from < vertice_to){
                nextlevel->add_edge(vertice_from, vertice_to);
                nextlevel->add_edge(vertice_to, vertice_from);
            } 
        }
    }
    return nextlevel;
}

int ProbabilisticGraph::get_V(){
    return V;
}

int ProbabilisticGraph::get_E(){
    return E;
}

int ProbabilisticGraph::get_level(){
    return level;
}

void ProbabilisticGraph::add_edge(int from, int to){
    if(std::find(adj[from].begin(), adj[from].end(), to) == adj[from].end()){
        adj[from].push_back(to);
        // edges.push_back(std::make_pair(from, to));
        E++;
    }
}

void ProbabilisticGraph::add_edge(int from, int to, float weight){
    if(std::find(adj[from].begin(), adj[from].end(), to) == adj[from].end()){
        this->adj[from].push_back(to);
        this->weights[from].push_back(weight);
        this->edges.push_back(std::make_pair(from, to));
        // this->edge_weights.push_back(weight);
        this->masses[from] += weight;
        E++;
    }
}

std::vector<int>& ProbabilisticGraph::get_neighbors(int vertice){
	return this->adj[vertice];
}

std::vector<std::vector<int>>& ProbabilisticGraph::get_adj(){
    return adj;
}

std::vector<std::vector<float>>& ProbabilisticGraph::get_weight(){
    return weights;
}

float* ProbabilisticGraph::get_masses(){
    return masses;
}
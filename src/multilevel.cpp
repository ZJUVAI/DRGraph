#include "multilevel.h"

Multilevel::Multilevel(){}

Multilevel::Multilevel(int level){
    adj.resize(V);
    weight.resize(V);
    // this->V = V;
    this->level = level;
}

//Multilevel::Multilevel(std::vector<std::vector<int>>* adj, std::vector<std::vector<float>>* weight){
//    this->V = (*adj).size();
//    int E = 0;
//    for(auto item : *adj){
//        E += item.size();
//    }
//    this->E = E;
//    std::cout<< E << std::endl;
//    //this->adj = adj;
//    //this->weight = weight;
//    //for(int i = 0; i < this->V; i++){
//    //    (this->adj)[i].reserve((*adj)[i].size());
//    //    (this->weight)[i].reserve((*weight)[i].size());
//    //    for(int j = 0; j < (*adj)[i].size(); j++){
//    //        this->add_edge(i, (*adj)[i][j], (*weight)[i][j]);
//    //    }
//    //    std::vector<int>().swap((*adj)[i]);
//    //    std::vector<float>().swap((*weight)[i]);
//    //}
//}

Multilevel::~Multilevel(){
    for(int i = 0; i < this->V; i++){
        std::vector<int>().swap((adj)[i]);
        std::vector<float>().swap((weight)[i]);
    }
    std::vector<std::vector<int>>().swap(adj);
    std::vector<std::vector<float>>().swap(weight);
    //delete[] masses;
    //delete nextlevel;
}

void Multilevel::resize(int V){
    adj.resize(V);
    weight.resize(V);
    // this->V = V;
}

void Multilevel::build_index(){
    this->V = adj.size();
    int E = 0;
    #pragma omp parallel for reduction (+:E)
    for(auto item : adj){
        E += item.size();
    }
    this->E = E;

    edges.reserve(E);
    for(int i = 0; i < V; i++){
        for(int j = 0; j < adj[i].size(); j++)
            edges.push_back(std::make_pair(i, adj[i][j]));
    }

    masses = new float[V]();
    #pragma omp parallel for reduction(+:masses[:V])
    for(int i = 0; i < V; i++)
        for(int j = 0; j < weight[i].size(); j++)
            masses[i] += weight[i][j];

    this->level = 0;
}

Multilevel* Multilevel::coarse(){
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

    // new nextlevel graph
    nextlevel = new Multilevel(level+1, V_new);
    for(auto edge : this->edges){
        int vertice_from = vertice_mapping[edge.first];
        int vertice_to = vertice_mapping[edge.second];
        if(vertice_from == vertice_to){
            continue;
        }else if (vertice_from < vertice_to){
            nextlevel->add_edge(vertice_from, vertice_to);
            nextlevel->add_edge(vertice_to, vertice_from);
        } 
    }
    
    std::cout<< "[DATA.MultiLevel] Level " << level << ": " << this->V << "," << this->E << "=>" << nextlevel->V << "," << nextlevel->E << std::endl;
    return nextlevel;
}

int Multilevel::get_V(){
    return adj.size();
}

int Multilevel::get_E(){
    return edges.size();
}

void Multilevel::add_edge(int from, int to){
    if(std::find(adj[from].begin(), adj[from].end(), to) == adj[from].end()){
        adj[from].push_back(to);
        edges.push_back(std::make_pair(from, to));
        E++;
    }
}

void Multilevel::add_edge(int from, int to, float weight){
    this->adj[from].push_back(to);
    this->weight[from].push_back(weight);
    this->edges.push_back(std::make_pair(from, to));
    //(*this->weight)[from].push_back(weight);
}

std::vector<int>& Multilevel::get_neighbors(int vertice){
	return this->adj[vertice];
}

std::vector<std::vector<int>>& Multilevel::get_adj(){
    return adj;
}

std::vector<std::vector<float>>& Multilevel::get_weight(){
    return weight;
}



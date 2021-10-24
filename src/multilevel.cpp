#include "multilevel.h"

Multilevel::Multilevel(){}

Multilevel::Multilevel(std::vector<std::vector<int>>* adj, std::vector<std::vector<float>>* weight){
    this->V = (*adj).size();
    int E = 0;
    for(auto item : *adj){
        E += item.size();
    }
    this->E = E;
    std::cout<< E << std::endl;
    //this->adj = adj;
    //this->weight = weight;
    //for(int i = 0; i < this->V; i++){
    //    (this->adj)[i].reserve((*adj)[i].size());
    //    (this->weight)[i].reserve((*weight)[i].size());
    //    for(int j = 0; j < (*adj)[i].size(); j++){
    //        this->add_edge(i, (*adj)[i][j], (*weight)[i][j]);
    //    }
    //    std::vector<int>().swap((*adj)[i]);
    //    std::vector<float>().swap((*weight)[i]);
    //}
}

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
}

//void Multilevel::set_V(int V){
//    this->V = V;
//}

int Multilevel::get_V(){
    return V;
}

//void Multilevel::set_E(int E){
//    this->E = E;
//}

int Multilevel::get_E(){
    return E;
}

void Multilevel::add_edge(int from, int to){
    adj[from].push_back(to);
}

void Multilevel::add_edge(int from, int to, float weight){
    this->adj[from].push_back(to);
    this->weight[from].push_back(weight);
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



#ifndef GRAPHLAYOUT_MULTILEVEL_H
#define GRAPHLAYOUT_MULTILEVEL_H

#include <assert.h>

#include <fstream>
#include <vector>
#include <cstdio>
#include <set>
#include <map>
#include <limits>

#include "data.h"
#include "knn.h"
#include "genrandom.h"

struct Link{
    int from, to, next;
    bool isMoon = false;
    float weight;
    Link() {}
    Link(int _from, int _to, int _next, float _weight):from(_from), to(_to), next(_next), weight(_weight) {}
};

struct Node{
    int mem;
    int sun;
    float distToSun;
    // int mass;
    int nodeType;
    bool isPlace;
    // float angle1;
    // float angle2;
    
    
    vector<int> moon_list;
    vector<float> lamba_list;
    vector<int> neighborSunNode;
    Node() {
        // isPlace = true;
        mem = -1;
        sun = -1;
        distToSun = -1;
        // mass = 0;
        nodeType = 0;
        isPlace = false;
        // angle1 = 0.0;
        // angle2 = 0.0;
        moon_list.clear();
        lamba_list.clear();
        neighborSunNode.clear();
    }
};

class Multilevel {
private:
    Multilevel(knn& gd, int k_level, int mode, float* similarity_weight);
    Multilevel(Data* gd);

    void add_edge(int from, int to, float weight);
    void add_one_edge(int v1, int v2, float weight);
    void deleteNode(int i, int& curV, int* v_index, int* v_pool);

    void multilevelFM3(Multilevel& ml,  bool first_round);
    void multilevelNormal(Multilevel& ml,  bool first_round);
    static bool edgenumbersum_of_all_levels_is_linear(float ratio, int level, int& bad_edgenr_counter);
public:
    // int multiMode;
    int n_vertices;
    int n_edges = 0;
    vector<Link> graph;
    int* head;
    int num_cluster = 0;
    

    Node* nodeAttribute = nullptr; // level - 1 more points memSize > num_cluster
    int* mass = nullptr;
    // int* du = nullptr;
    float *similarity = nullptr;
    int* memIter = nullptr;
    int memSize = -1;

    int *membership = nullptr; // always same length


    Multilevel();
    ~Multilevel();
    
    static vector<Multilevel*> gen_multilevel(knn& gd, int min_clusters, int k_level, int mode, float* similarity_weight);
    static vector<Multilevel*> gen_multilevel(Data* gd, int min_clusters, int k_level, int mode);

};


#endif 

#ifndef SPACEPARTITION_H
#define SPACEPARTITION_H

#include <vector>
#include "sp_evaluation.h"
#include "quadtree.h"

using namespace std;

class SpacePartition{
private:
    int n_vertices, out_dim;
    int num_cluster;
    SpEvaluation sp_evaluation;
    int start_hit_count = 0;
    void dfs(QuadTree * root, float *vis);

public:
    int *membership = NULL; // menbership[index] = numCluster
//    float **centers = NULL;
    float **offset = NULL; //clusters[numcluster][dim]
//    vector<float *> centers_temp;
//    vector<int> *clusters = NULL;
    vector<int*> *pairs; // the link of node in a cell
    vector<int> *knn_vec;
    bool fcflag = false; // fastcommunity's switch

    SpacePartition();
    ~SpacePartition();
    void k_means(float *vis);
    bool isStart(float *vis);
    bool isStop();
    void output(float *vis);
    void quad_tree (float *vis);

    void init(float *vis, int n_ver, int out_d, int num_clus, vector<int> *vec);

    void computeDistribution(QuadTree *qt, int *cell_count, int *knn_index_count, const int &knn_k, double *perc,
                             vector<int *> *pairs, int *numedge);
};

#endif //SPACEPARTITION_H

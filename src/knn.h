#ifndef KNN_H
#define KNN_H

#include <stdlib.h>
#include <math.h>

#include <iostream>
#include <vector>
#include <fstream>

#include <gsl/gsl_rng.h>

#include "ANNOY/annoylib.h"
#include "ANNOY/kissrandom.h"
#include "algorithm/efanna.hpp"
// #include "algorithm/base_index.hpp"
#include "timer.h"
#include "util.h"

class knn{
private:
    static const gsl_rng_type * gsl_T;
    static gsl_rng * gsl_r;
    
    int n_threads, n_trees, n_neighbors, n_propagations;
    float *vec;
    vector<int> *old_knn_vec;
    AnnoyIndex<int, float, Euclidean, Kiss64Random> *annoy_index;
    string knn_type;
    int knn_trees, epochs, mlevel, L, checkK, knn_k, S, build_trees;

    void run_annoy();
    void annoy_thread(int id);
    static void *annoy_thread_caller(void *arg);
    void run_propagation();
    void propagation_thread(int id);
    static void *propagation_thread_caller(void *arg);
    void test_accuracy();

public:
    int  n_dim, n_vertices;
    vector<int> *knn_vec;

    ~knn();
    knn();

    float CalcDist(int x, int y);
    void load_data(string& infile);
    void load_knn(string& infile);
    
    void normalize();
	void setParams(int n_tree, int n_neig, int n_thre, int n_prop, int knn_tre, int epo, int mle, int l, int che, int useK, int k, int s, int build_tre, string knn_tp);
    void construct_knn();
    void save_knn(string& outfile);
};

#endif

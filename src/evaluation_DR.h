#ifndef EVALUATION_H
#define EVALUATION_H

#include <iostream>
#include <set>
#include <float.h>
#include <iomanip>
#include "data.h"
#include "genrandom.h"
#include "timer.h"
#include <omp.h>
#include <pthread.h>
#include <algorithm>

using namespace std;

namespace DR {
    struct arg_evaluation {
        void *ptr;
        int id;

        arg_evaluation(void *x, int y) : ptr(x), id(y) {};
    };

    class evaluation {
    private:
        int Nei[7] = {1, 5, 10, 20, 30, 40, 50};
        vector<int> *knn;
        int *label;
        int n_class;
        float *vis;
        int n_vertices, out_dim, samp_num, cur_num;
        int *seq, *samp;

        float CalcDist2D(int x, int y);

        int k_neighbors = 1;
        int n_threads = 8;
        int *hit;
        int **Hit;

        int *load_label(string &infile, int vertices);

        vector<int> *load_knn(string &infile, int vertices);

        float *load_vis(string &infile, int &vertices, int &dim);

    public:
        evaluation();

        //void test_accuracy();
        void sampling();

        //void test_sample_accuracy();
        void test_accuracy1();

        void accuracy(int k_neighbors);

        void accuracy_thread(int id);

        static void *accuracy_thread_caller(void *arg);

        void load_data(string &labelfile, string &knnfile, string &visfile);

        void accuracy_All(int k_neighbors);

        void accuracy_thread_All(int id);

        static void *accuracy_thread_caller_All(void *arg);
    };

    class Dis {
    public:
        int index;
        float distance;

        Dis(int index, float dis) {
            this->index = index;
            this->distance = dis;
        }
    };

}


#endif

//
// Created by Suya Basa on 2020/3/14.
//

#ifndef VIS_EVALUATION_GL_H
#define VIS_EVALUATION_GL_H
#include <iostream>
#include <vector>
#include <algorithm>
#include <cfloat>
#include <list>
#include <pthread.h>
#include <iomanip>
#include <set>
#include <fstream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <cmath>
#include "data.h"
#include "timer.h"

using namespace std;

typedef float real;

namespace GL {

    struct arg_evaluation{
        void *ptr;
        int id;
        arg_evaluation(void *x, int y) :ptr(x), id(y){};
    };

    class evaluation{
    private:
        int N = 0;

        float *vis;
        float *sum;
        float average = 0;

        float alpha;
        float *l, *l2;
        int *choose, *count;

        // 存放loss计算中的每条边的（1 + LD^2）^-1
        float *l_dis;
        float sum_ld = .0; //总和
        float *th_sum; //每个线程的总和
        float *th_sum_loss; // 每个线程的误差和

        int n_vertices, out_dim;
        int n_threads = 8;

        Data d;

        real CalcDist2D(int x, int y);


    public:

        Data *ned = NULL;

        ~evaluation();
        evaluation(Data *gdata);

        void load_data(char *visfile);

        float jaccard();
        vector<int> vectors_intersection(vector<int> v1,vector<int> v2);
        vector<int> vectors_union(vector<int> v1,vector<int> v2);
        void accuracy_thread(int id);
        static void *accuracy_thread_caller(void* arg);

        float stress_neighbor();
        static void *stress_neighbor_thread_caller(void *arg);
        void stress_neighbor_thread(int id);
        static void *stress_neighbor_alpha_thread_caller(void *arg);
        void stress_neighbor_alpha_thread(int id);

        float stress();
        static void *stress_thread_caller(void *arg);
        void stress_thread(int id);
        static void *stress_alpha_thread_caller(void *arg);
        void stress_alpha_thread(int id);

        float loss();
        void static *layout_proximity_thread_caller(void *arg);
        void layout_proximity_thread(int id);
        void static *loss_thread_caller(void *arg);
        void loss_thread(int id);
    };

    class Dis{
    public:
        int index;
        float distance;
        Dis(int index, float dis){
            this->index = index;
            this->distance = dis;
        }
    };

    template <class T>
    int getArrayLen(T &array){
        return sizeof(array) / sizeof(array[0]);
    }




}

#endif //VIS_EVALUATION_GL_H

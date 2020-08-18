#ifndef VISUALIZE_H
#define VISUALIZE_H

#include "data.h"
#include "timer.h"
#include <gsl/gsl_rng.h>
#include "multilevel.h"
#include <pthread.h>
#include <cmath>
#include <set>
#include <assert.h>
#include <iostream>
#include <limits>
// #include <ogdf/basic/Graph.h>
// #include <ogdf/module/LayoutModule.h>
// #include <ogdf/energybased/PivotMDS.h>
// #include <ogdf/basic/GraphAttributes.h>


struct SPoint{
    float x, y;
    SPoint(float _x = 0, float _y = 0): x(_x), y(_y) {}
    float angle(SPoint q, SPoint r) const {
        const float dx1 = q.x - x, dy1 = q.y - y;
        const float dx2 = r.x - x, dy2 = r.y - y;

        // two vertices on the same place!
        if ((dx1 == 0 && dy1 == 0) || (dx2 == 0 && dy2 == 0)) {
            return 0.0;
        }

        float phi = std::atan2(dy2, dx2) - std::atan2(dy1, dx1);
        if (phi < 0) { phi += 2 * 3.14159265358979323846; }

        return phi;
    }
    bool operator==(const SPoint& r) const {
        if(r.x == x && r.y == y) return true;
        else return false;
    }
    
    bool operator!=(const SPoint& r) const {
        if(r.x != x || r.y != y) return true;
        else return false;
    }
    SPoint operator-(const SPoint& r) const {
        return SPoint(x - r.x, y - r.y);
    }
    SPoint operator+(const SPoint& r) const {
        return SPoint(x + r.x, y + r.y);
    }

    SPoint operator/(int r) const {
        return SPoint(x / r, y / r);
    }

    float norm() {
        return sqrt(x * x + y * y);
    }

};

class visualizemod {
private:
    pair<float, float> use_Mode;
    const float mathPi = 3.14159265358979323846;
    
    inline void updateMin(float& min, const float& newValue) {
        if(min > newValue) {
            min = newValue;
        }
    }

    Data *graphdata = NULL;
    const int out_dim = 2; // deprecated, we force to use 2 dim
    int n_threads, n_negatives;
    long long n_samples, per_samples, edge_count_actual;
    
    float initial_alpha, gamma;

    static const gsl_rng_type * gsl_T;
    static gsl_rng * gsl_r;
    // float *offset;
    float grad_clip;

    Multilevel *ml; // 引入层次计算结果
    bool ml_method = false;
    int *neg_table;
    long long neg_size;
    bool isout = false;
    // char outfolder[1000];

    void visualize_thread(int id, int iter);
    static void *visualize_thread_caller(void *arg);
    void init_alias_table(int n, float* weight, int* alias, float* prob);
    void init_edge_table();
    void init_vertice_table();
    void init_neg_table();
    int sample_an_vertice(float rand_value1, float rand_value2);
    int sample_an_edge(float rand_value1, float rand_value2);
    inline SPoint calculate_position(SPoint P, SPoint Q, float dist_P, float dist_Q);
    inline SPoint get_waggled_inbetween_position(SPoint s, SPoint t, float lambda);
    inline SPoint create_random_pos(SPoint center,float radius,float angle_1, float angle_2);
    inline SPoint getSPoint(int x);
    inline void setSPoint(int x, SPoint y);
    int *alias_edge, *alias_vertice;
    float *prob_edge, *prob_vertice, *vis, *vis_swap;

    void random_vis(Multilevel *ml);
    void makeNewVis(Multilevel* pre);

#ifdef USE_CUDA
    void visualize_GPU(Multilevel *m, long long _samples, float _alpha, float _gamma, bool method);
#endif
    void visualize(Multilevel *m, long long _samples, float _alpha, float _gamma, bool method); 

public:
    visualizemod();

    ~visualizemod();

    // void clean();
//    void visualize(Multilevel *ml);
    void run(Data *data1, int n_thre, int n_samp, float alph, int n_nega,
             float gamm, vector<Multilevel*>& mls, int multiMode, float firstGamma, pair<float, float> proSet = make_pair(-1.0, -1.0) );
    
    // void save_intermediate_result(float perc);

    void save(string& outfile);


    

};


#endif //VISUALIZE_H

#ifndef SPEVALUATION_H
#define SPEVALUATION_H

#include <iostream>
#include <math.h>
#include <random>
#include <functional> //for std::bind
#include <algorithm>
#include <math.h>
#include <stack>
#include "genrandom.h"

class SpEvaluation{
private:
    int data_size;
    int dim_size;
    int * samples;
    int  samples_size;
    int index = 0;
    float first_position_offset_sum;
    float *last_position;
    float last_position_offset_sum;
    void randomSampling();
    void set_init_position(float *data);
    float current_position_offset(float *data);
    float start_offset[5];
    //stop evluation
    int stop_samples_size;
    int *stop_samples;
    int cluster_size;
    float *cluster_last_offset;
    float stop_offset[5];
public:
    SpEvaluation();
    void init(int data_size, int dim_size);
    void generate_samples();
    void generate_stopSamples();
    float start_evaluation(float *data);
    void setStopParams(int cluster_size);


    //stop evluation
    int stop_index = 0;
    float stop_evaluation(float **offset);
    float stop_evaluation2(float **offset);
};


#endif //SPEVALUATION_H

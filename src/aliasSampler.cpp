#include "aliasSampler.h"

AliasSampler::AliasSampler(const int n, float *weight, float power) : n(n){
    alias = new int[n];
    prob = new float[n];

    auto *norm_prob = new float[n];
    auto *large_block = new int[n];
    auto *small_block = new int[n];

    float sum = 0;
    int cur_small_block, cur_large_block;
    int num_small_block = 0, num_large_block = 0;
    
    for(int k = 0; k < n; ++k) sum += norm_prob[k] = std::pow(weight[k], power);
    for(int k = 0; k < n; ++k) alias[k] = 0, norm_prob[k] = norm_prob[k] * n / sum;

    for(int k = n - 1; k >= 0; --k){
        if (norm_prob[k] < 1)
            small_block[num_small_block++] = k;
        else
            large_block[num_large_block++] = k;
    }

    while (num_small_block && num_large_block){
        cur_small_block = small_block[--num_small_block];
        cur_large_block = large_block[--num_large_block];
        prob[cur_small_block] = norm_prob[cur_small_block];
        alias[cur_small_block] = cur_large_block;
        norm_prob[cur_large_block] = norm_prob[cur_large_block] + norm_prob[cur_small_block] - 1;
        if (norm_prob[cur_large_block] < 1)
            small_block[num_small_block++] = cur_large_block;
        else
            large_block[num_large_block++] = cur_large_block;
    }

    while (num_large_block) prob[large_block[--num_large_block]] = 1;
    while (num_small_block) prob[small_block[--num_small_block]] = 1;

    delete[] norm_prob;
    delete[] small_block;
    delete[] large_block;
}

AliasSampler::AliasSampler(const int n, std::vector<std::vector<float>> &weight) : n(n){
    alias = new int[n];
    prob = new float[n];

    auto *norm_prob = new float[n];
    auto *large_block = new int[n];
    auto *small_block = new int[n];

    float sum = 0;
    int cur_small_block, cur_large_block;
    int num_small_block = 0, num_large_block = 0;
    
    for(auto vector : weight){for(auto item :vector){sum += item;}}
    int k = 0;
    for(auto vector : weight){for(auto item: vector){
            alias[k] = 0;
            norm_prob[k] = item * n / sum;
            ++k;
    }}

    for(int k = n - 1; k >= 0; --k){
        if (norm_prob[k] < 1)
            small_block[num_small_block++] = k;
        else
            large_block[num_large_block++] = k;
    }

    while (num_small_block && num_large_block){
        cur_small_block = small_block[--num_small_block];
        cur_large_block = large_block[--num_large_block];
        prob[cur_small_block] = norm_prob[cur_small_block];
        alias[cur_small_block] = cur_large_block;
        norm_prob[cur_large_block] = norm_prob[cur_large_block] + norm_prob[cur_small_block] - 1;
        if (norm_prob[cur_large_block] < 1)
            small_block[num_small_block++] = cur_large_block;
        else
            large_block[num_large_block++] = cur_large_block;
    }

    while (num_large_block) prob[large_block[--num_large_block]] = 1;
    while (num_small_block) prob[small_block[--num_small_block]] = 1;

    delete[] norm_prob;
    delete[] small_block;
    delete[] large_block;
}

AliasSampler::~AliasSampler(){
    delete[] alias;
    delete[] prob;
}

int AliasSampler::sample(float rand_value1, float rand_value2){
    int k = (int) n * rand_value1;
    return rand_value2 < prob[k] ? k : alias[k];
}




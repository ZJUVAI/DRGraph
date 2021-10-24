#ifndef ALIASSAMPLER_H
#define ALIASSAMPLER_H

#include <vector>
#include <cmath>

class AliasSampler {

private:
    int *alias;
    float *prob;
    int n;

public:
    AliasSampler(const int n, float *weight, float power = 1);
    AliasSampler(const int n, std::vector<std::vector<float>> &weight);
    ~AliasSampler();

    int sample(float rand_value1, float rand_value2);;
};

#endif

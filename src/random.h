#ifndef RANDOM_H
#define RANDOM_H

//#include <ctime>
//#include <unistd.h>
//#include <gsl/gsl_rng.h>
//#include <gsl/gsl_randist.h>

#include <random>

using namespace std;

class Random{
private:
    std::mt19937 gen;
    std::uniform_real_distribution<float> uniform_dis = std::uniform_real_distribution<float>(0, 1.0);
    //gsl_rng * r;
public:
    Random(long seed = 0);
    //void init_gsl(long tid);
    float uniform();
};

#endif

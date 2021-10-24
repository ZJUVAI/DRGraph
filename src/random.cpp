#ifndef RANDOM
#define RANDOM

#include "random.h"

Random::Random(long seed){
    //init_gsl(tid);
    std::random_device rd;
    gen.seed(rd() + seed);
}

//void Random::init_gsl(long tid){
//    const gsl_rng_type * T;
//    gsl_rng_env_setup();
//    T = gsl_rng_default;
//    r = gsl_rng_alloc(T);
//    gsl_rng_set(r, time (NULL) * getpid() + tid);
//}

float Random::uniform(){
    return uniform_dis(gen);
}

#endif

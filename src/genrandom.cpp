#ifndef GENRANDOM
#define GENRANDOM

#include "genrandom.h"

genrandom::genrandom() {}

//LargeVis gsl back[0,1)
gsl_rng * genrandom::init_gsl (){
    const gsl_rng_type * T;
    real u;
    gsl_rng_env_setup();
    T = gsl_rng_default;
    r = gsl_rng_alloc(T);
    gsl_rng_set(r, time (NULL) * getpid());
    //gsl_rng_set(r, 314159265);
}

real genrandom::gslRandom() {
    real u = gsl_rng_uniform (r);
    return u;
}

#endif

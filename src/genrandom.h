#ifndef GENRANDOM_H
#define GENRANDOM_H

#include <ctime>
#include <unistd.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

using namespace std;

class genrandom{
private:
    gsl_rng * r;
public:
    genrandom();
    void init_gsl ();
    float gslRandom();
};

#endif //GENRANDOM_H

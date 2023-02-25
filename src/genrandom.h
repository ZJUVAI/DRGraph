#ifndef GENRANDOM_H
#define GENRANDOM_H

#include <ctime>
#include <unistd.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

using namespace std;

typedef float real;

class genrandom{
private:
    gsl_rng * r;
public:
    genrandom();
    void init_gsl ();
    real gslRandom();
};

#endif //GENRANDOM_H

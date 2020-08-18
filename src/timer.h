#ifndef TIMER_H
#define TIMER_H

#include <ctime>
#include <sys/timeb.h>

using namespace std;

class timer{
private:
    clock_t cpu_s, cpu_e;
    struct timeb real_s,real_e;
public:
    timer();
    void start ();
    void end ();
    double cpu_time();
    double real_time();
    timer &operator+= (const timer &t);
    timer &operator- (const timer &t);
};

#endif //TIMER_H

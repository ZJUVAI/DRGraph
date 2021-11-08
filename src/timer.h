#ifndef TIMER_H
#define TIMER_H

#include <ctime>
#include <sys/timeb.h>
#include <chrono>

using namespace std;

class timer{
private:
    clock_t cpu_s, cpu_e;
    std::chrono::time_point<std::chrono::system_clock> real_s, real_e;
public:
    timer();
    void start();
    void end();
    double cpu_time();
    double real_time();
    timer &operator+= (const timer &t);
    timer &operator- (const timer &t);
};

#endif

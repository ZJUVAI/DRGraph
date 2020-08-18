#ifndef TIMER
#define TIMER

#include "timer.h"

timer::timer() {}

void timer::start(){
    cpu_s = clock();
    ftime(&real_s);
}

void timer::end(){
    cpu_e = clock();
    ftime(&real_e);
}

//Function: calculate cpu time
double timer::cpu_time() {
    double cpu_time = 0;
    cpu_time = (cpu_e - cpu_s) * 1.0 / CLOCKS_PER_SEC; //s
    return cpu_time;
}

//Function: calculate real time
double timer::real_time() {
    double real_time = 0;
    real_time = (real_e.time - real_s.time) * 1.0 +(real_e.millitm - real_s.millitm) / 1000.0; //s
    return real_time;
}

timer& timer::operator+= (const timer &t) {
    cpu_e += t.cpu_e - t.cpu_s;
    int mils = (t.real_e.time - t.real_s.time) * 1000 + t.real_e.millitm - t.real_s.millitm;
    real_e.time += (mils + real_e.millitm) / 1000;
    real_e.millitm = (mils + real_e.millitm) % 1000;
    return *this;
}

timer& timer::operator- (const timer &t) {
    cpu_e = cpu_e - (t.cpu_e - t.cpu_s);
    int t_mils = (t.real_e.time - t.real_s.time) * 1000 + t.real_e.millitm - t.real_s.millitm;
    int t1_mils = (real_e.time - real_s.time) * 1000 + real_e.millitm - real_s.millitm;
    int mils = t1_mils - t_mils;
    real_e.time = real_s.time + (mils + real_s.millitm) / 1000;
    real_e.millitm = (mils + real_s.millitm) % 1000;
    return *this;
}

#endif //TIMER

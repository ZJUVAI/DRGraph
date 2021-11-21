#ifndef TIMER
#define TIMER

#include "timer.h"

timer::timer() {}

void timer::start(){
    cpu_s = clock();
    real_s = std::chrono::high_resolution_clock::now();
}

void timer::end(){
    cpu_e = clock();
    real_e = std::chrono::high_resolution_clock::now();
}

double timer::cpu_time() {
    return (cpu_e - cpu_s) * 1.0 / CLOCKS_PER_SEC; //s
}

double timer::real_time() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(real_e - real_s).count() / 1000.0;
}

//timer& timer::operator+= (const timer &t) {
    //cpu_e += t.cpu_e - t.cpu_s;
    //int mils = (t.real_e.time - t.real_s.time) * 1000 + t.real_e.millitm - t.real_s.millitm;
    //real_e.time += (mils + real_e.millitm) / 1000;
    //real_e.millitm = (mils + real_e.millitm) % 1000;
    //return *this;
//}

//timer& timer::operator- (const timer &t) {
    //cpu_e = cpu_e - (t.cpu_e - t.cpu_s);
    //int t_mils = (t.real_e.time - t.real_s.time) * 1000 + t.real_e.millitm - t.real_s.millitm;
    //int t1_mils = (real_e.time - real_s.time) * 1000 + real_e.millitm - real_s.millitm;
    //int mils = t1_mils - t_mils;
    //real_e.time = real_s.time + (mils + real_s.millitm) / 1000;
    //real_e.millitm = (mils + real_s.millitm) % 1000;
    //return *this;
//}

#endif

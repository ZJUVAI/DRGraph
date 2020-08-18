#ifndef UTIL_H
#define UTIL_H

#include <algorithm>
#include <functional>
#include <thread>
#include <vector>

struct arg_struct{
    void *ptr;
    int id;
    int times;
    arg_struct(void *x, int y, int z) :ptr(x), id(y), times(z){};
    arg_struct(void *x, int y) :ptr(x), id(y){};
};

#define PARALLEL_FOR(nthreads,LOOP_END,O) {          			\
	if (nthreads >1 ) {						\
        std::vector<std::thread> threads(nthreads);		        \
		for (int t = 0; t < nthreads; t++) {			\
		    threads[t] = std::thread(std::bind(			\
		    [&](const int bi, const int ei, const int t) { 		\
			    for(int loop_i = bi;loop_i<ei;loop_i++) {   O;  }	\
		    },t*LOOP_END/nthreads,(t+1)==nthreads?LOOP_END:(t+1)*LOOP_END/nthreads,t)); \
		}							\
		std::for_each(threads.begin(),threads.end(),[](std::thread& x){x.join();});\
	}else{								\
		for (int loop_i=0; loop_i<LOOP_END; loop_i++) {		\
			O;						\
		}							\
	}								\
}



#endif
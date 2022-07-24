#include <iostream>
#include <fstream>
#include <ctime>
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <cstring>
#include <random>
#include <pthread.h>
#include <boost/program_options.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/progress_spinner.hpp>
#include "timer.h"
#include "data.h"
#include "aliasSampler.h"
#include "random.h"
#include "optimization.h"
namespace  bpo = boost::program_options;

double mem_usage() {
   double vm_usage = 0.0;
   double resident_set = 0.0;
   std::ifstream stat_stream("/proc/self/stat",ios_base::in); //get info from proc directory
   //create some variables to get info
   std::string pid, comm, state, ppid, pgrp, session, tty_nr;
   std::string tpgid, flags, minflt, cminflt, majflt, cmajflt;
   std::string utime, stime, cutime, cstime, priority, nice;
   std::string O, itrealvalue, starttime;
   unsigned long vsize;
   long rss;
   stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
   >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
   >> utime >> stime >> cutime >> cstime >> priority >> nice
   >> O >> itrealvalue >> starttime >> vsize >> rss; // don't care about the rest
   stat_stream.close();
   long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // for x86-64 is configured to use 2MB pages
   vm_usage = vsize / 1024.0; //KB
   resident_set = rss * page_size_kb;
   return resident_set;
}

// Params
std::string input, output;
std::string method;
int n_dims;

bool parse_params(int argc, char ** argv) {
    bpo::options_description generic("Generic options");
    generic.add_options()
        ("version,v", "version")
        ("help,h", "help message")
        ;
    bpo::options_description file("File options");
    file.add_options()  
        ("input,i", bpo::value<std::string>(&input)->required(), "Input graph, knn graph or high dimendional data")
        ("output,o", bpo::value<std::string>(&output)->required(), "Output embedding");
    bpo::options_description algo("Algorithms options");
    algo.add_options()
        ("method", bpo::value<std::string>(&method)->required(), "Choose method from GL or DR");
    algo.add_options()
        ("n_dims", bpo::value<int>(&n_dims)->default_value(2), "Dimension of the embedded space. Default 2.");
    bpo::options_description opts("Options");
    opts.add(generic).add(file).add(algo);

    bpo::variables_map vm;
    try{
        bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
        if(vm.count("help") ){
            std::cout << opts << std::endl;
            return false;
        }
        // throws on error, so do after help in case there are any problems 
        bpo::notify(vm);
    }catch(std::exception& e){
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }catch(...){
        std::cout << "Unknow error\n";
        return false;
    }
    return true;
}

struct arg_struct{
    void *ptr;
    int id;
    arg_struct(void *x, int y) :ptr(x), id(y){};
};      

void *PrintHello(void* argv) {
    arg_struct* arg = (arg_struct*)argv;
    AliasSampler* alias = (AliasSampler*)arg->ptr;
    long id = (long)arg->id;
    Random rnd(id);


    for(int i=0; i<1; i++){
        //cout << "Hello World! Thread ID, " << id << " " << rnd.uniform()<<endl;
    }
    for(int i=0; i<15000000; i++){
       //rnd.uniform();
        alias->sample(rnd.uniform(), rnd.uniform());
    }
    pthread_exit(NULL);
}

int main(int argc, char** argv){
    timer t;



    // Load params
    parse_params(argc, argv);
    std::cout << "[ARGS] input: " << input << std::endl;
    std::cout << "[ARGS] output: " << output << std::endl;
    std::cout << "[ARGS] method: " << method << std::endl;
  
    //Load Data
    Data data(n_dims); 
    if(method == "GL"){
        t.start();
        data.load_graph(input);
        t.end();
        std::cout << "[DATA.Loading] Real Time: "<<  t.real_time() <<"s" << " CPU Time: "<<  t.cpu_time() << "s" << std::endl;
        std::cout<< std::fixed << "[MONITOR] Memory: " << (long)mem_usage() << " KB" << std::endl;        
    }else if(method == "DR"){
        data.load_vector(input);
    }
    
    t.start();
    data.gen_probabilistic_graph(method);
    t.end();
    std::cout << "[DATA.GenProbabilisticGraph] Real Time: "<<  t.real_time() <<"s" << " CPU Time: "<<  t.cpu_time() << "s" << std::endl;
    std::cout<< std::fixed << "[MONITOR] Memory: " << (long)mem_usage() << " KB" << std::endl;

    t.start();
    data.build_multilevel();
    t.end();
    std::cout << "[DATA.MultiLevel] Real Time: "<<  t.real_time() <<"s" << " CPU Time: "<<  t.cpu_time() << "s" << std::endl;
    std::cout<< std::fixed << "[MONITOR] Memory: " << (long)mem_usage() << " KB" << std::endl;
    
    //Optimization
    Optimization optim;
    t.start();
    optim.init_vertice_sampler(data.multilevels[0]->get_V(), data.multilevels[0]->get_masses());
    optim.init_edge_sampler(data.multilevels[0]->get_E(), data.multilevels[0]->get_weight());
    // data.init_embedding();
    optim.init_params(data.multilevels[0]->get_V(), data.n_dims, data.multilevels.back());
    optim.run();
    
    t.end();
    std::cout << "[OPTIMIZATION] Real Time: "<<  t.real_time() <<"s" << " CPU Time: "<<  t.cpu_time() << "s" << std::endl;
    std::cout<< std::fixed << "[MONITOR] Memory: " << (long)mem_usage() << " KB" << std::endl;

    // int n = 5;
    // float weight[n]{0.1, 0.2, 0.3, 0.4, 0.5};
    // vector<int> count(n, 0);
    // AliasSampler alias(n, weight);
    // //std::cout << "Alias Sampling" << std::endl;
    // int NUM_THREADS = 10; 
    // pthread_t threads[NUM_THREADS];
    // for(int i = 0; i < NUM_THREADS; i++ ) {
    //   int rc = pthread_create(&threads[i], NULL, PrintHello, new arg_struct(&alias, i));
    //   if (rc) {
    //      cout << "Error:unable to create thread," << rc << endl;
    //      exit(-1);
    //   }
    // }
    // pthread_exit(NULL);

    return 0;
}





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




std::string input, output;
std::string method;

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
    }
    catch(std::exception& e){
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
    catch(...){
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
    parse_params(argc, argv);
 
    //std::cout << input << std::endl;
    //std::cout << output << std::endl;
    timer t;
  
    //Load Data
    Data data; 
    if(method == "GL"){
        t.start();
        data.load_graph(input);
        t.end();
        std::cout << "[DATA.Loading] Real Time: "<<  t.real_time() <<"s" << " CPU Time: "<<  t.cpu_time() << "s" << std::endl;
        t.start();
        data.graph2dist();
        data.dist2weight();
        t.end();
        std::cout << "[DATA.Graph2Weight] Real Time: "<<  t.real_time() <<"s" << " CPU Time: "<<  t.cpu_time() << "s" << std::endl;
        t.start();
        data.build_multilevel();
        t.end();
        std::cout << "[DATA.MultiLevel] Real Time: "<<  t.real_time() <<"s" << " CPU Time: "<<  t.cpu_time() << "s" << std::endl;
    }else if(method == "DR"){
        data.load_vector(input);
    }
    
    //Optimization
    //Optimization opt;
    //opt.init_edge_sampler(2 * data.n_edges, data.sim_weight);
    //std::cout << opt.edge_sampler->sample(0.1, 0.1) << std::endl;
    //opt.init_vertice_sampler(data.n_vertices, data.vertice_weight);
    //std::cout << opt.vertice_sampler->sample(0.1, 0.1) << std::endl;
    //opt.run();

    int n = 5;
    float weight[n]{0.1, 0.2, 0.3, 0.4, 0.5};
    vector<int> count(n, 0);
    AliasSampler alias(n, weight);
    //std::cout << "Alias Sampling" << std::endl;
    int NUM_THREADS = 10; 
    pthread_t threads[NUM_THREADS];
    for(int i = 0; i < NUM_THREADS; i++ ) {
      int rc = pthread_create(&threads[i], NULL, PrintHello, new arg_struct(&alias, i));
      if (rc) {
         cout << "Error:unable to create thread," << rc << endl;
         exit(-1);
      }
    }
    pthread_exit(NULL);

    return 0;
}





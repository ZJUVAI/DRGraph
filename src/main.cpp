#include <iostream>
#include <fstream>
#include <ctime>
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <cstring>
#include <boost/program_options.hpp>
#include "data.h"
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



int main(int argc, char** argv){
    parse_params(argc, argv);
 
    std::cout << input << std::endl;
    std::cout << output << std::endl;
  
    //Load Data
    Data data; 
    if(method == "GL"){
        data.load_graph(input);
    }else if(method == "DR"){
        data.load_vector(input);
    }



    return 0;
}

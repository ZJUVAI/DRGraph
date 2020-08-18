#include <iostream>
#include <fstream>
#include <ctime>
#include <unistd.h>
#include <malloc.h>
#include "data.h"
#include "knn.h"
#include "visualizemod.h"
#include "evaluation_DR.h"
#include "evaluation_GL.h"
#include "sp_evaluation.h"



int algoMode = 0; // 0 : dimension reduction 1: graph layout 
/**file path**/
string infile, labelfile, outfile, knnfile;
/****knn****/
string knn_type; // "largevis" or "efanna"
int n_neighbors = -1, n_trees = -1, n_propagation = -1, knn_trees = -1, epochs = -1, mlevel = -1, L = -1, checkK = -1, S = -1, build_trees = -1, useK = -1;
/**parameter**/
float alpha = 1.0, n_gamma = 0.01, perplexity = 50;
int n_samples = 500, n_threads = 8, n_negative = 5, knn_level = 2, multiMode = 1; // multiMode : 1(FM3) 0(normal)
int disMax = 1;
float parameterA = -1.0; float parameterB = -1.0;
bool useEvaluation = false;


int ArgPos(char *str, int argc, char **argv) {
    int a;
    for (a = 1; a < argc; a++)
    if (!strcmp(str, argv[a])) {
        if (a == argc - 1) {
            printf("Argument missing for %s\n", str);
            exit(1);
        }
        return a;
    }
    return -1;
}

void setParams(int argc, char ** argv) {
    int i;
    /**file path**/
    if ((i = ArgPos((char *) "-input", argc, argv)) > 0) infile = argv[i + 1];
    if ((i = ArgPos((char *) "-label", argc, argv)) > 0) labelfile = argv[i + 1];
    if ((i = ArgPos((char *) "-output", argc, argv)) > 0) outfile = argv[i + 1];
    if ((i = ArgPos((char *) "-knn", argc, argv)) > 0) knnfile = argv[i + 1];
    
    /****knn****/
    if ((i = ArgPos((char *) "-knn_type", argc, argv)) > 0) knn_type = argv[i + 1];
    if ((i = ArgPos((char *) "-knn_k", argc, argv)) > 0) n_neighbors = atoi(argv[i + 1]);
    //EFANNA knn
    if ((i = ArgPos((char *) "-knn_trees", argc, argv)) > 0) knn_trees = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-mlevel", argc, argv)) > 0) mlevel = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-epochs", argc, argv)) > 0) epochs = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-L", argc, argv)) > 0) L = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-checkK", argc, argv)) > 0) checkK = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-useK", argc, argv)) > 0) useK = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-S", argc, argv)) > 0) S = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-build_trees", argc, argv)) > 0) build_trees = atoi(argv[i + 1]);
    //LargeVis knn
    if ((i = ArgPos((char *) "-trees", argc, argv)) > 0) n_trees = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-prop", argc, argv)) > 0) n_propagation = atoi(argv[i + 1]);

    /**parameter**/
    if ((i = ArgPos((char *) "-multilevel", argc, argv)) > 0) multiMode = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-mode", argc, argv)) > 0) algoMode = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-klevel", argc, argv)) > 0) knn_level = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-samples", argc, argv)) > 0) n_samples = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-threads", argc, argv)) > 0) n_threads = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-neg", argc, argv)) > 0) n_negative = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-alpha", argc, argv)) > 0) alpha = atof(argv[i + 1]);
    if ((i = ArgPos((char *) "-gamma", argc, argv)) > 0) n_gamma = atof(argv[i + 1]);
    if ((i = ArgPos((char *) "-perp", argc, argv)) > 0) perplexity = atof(argv[i + 1]);

    if ((i = ArgPos((char *) "-A", argc, argv)) > 0) parameterA = atof(argv[i + 1]);
    if ((i = ArgPos((char *) "-B", argc, argv)) > 0) parameterB = atof(argv[i + 1]);
    if ((i = ArgPos((char *) "-dismax", argc, argv)) > 0) disMax = atoi(argv[i + 1]);
    if ((i = ArgPos((char *) "-evaluation", argc, argv)) > 0) useEvaluation = atoi(argv[i + 1]);

    cout << "[INPUT FILE] " << infile << endl;
    cout << "[LABEL FILE] " << labelfile << endl;
    cout << "[OUTPUT FILE] " << outfile << endl;
    cout << "[KNN FILE] " << knnfile << endl;
}

void solveDimensionReduction() {
    if(multiMode == 1) {
        cout << "[WARNINIG] we suggest to use normal multilevel mode for DimensionReduction !" << endl;
    }
    timer t, all;
    all.start();

    knn* knn_module = new knn();
    knn_module->load_data(infile);
    // printf("knn_level: %d\n", knn_level);
    knn_module->setParams(n_trees, n_neighbors, n_threads, n_propagation, knn_trees, mlevel, epochs, L, checkK, useK, n_neighbors, S, build_trees, knn_type);
    // knn_module->load_knn(knnfile);
    t.start();
    knn_module->construct_knn();
    t.end();

    cout << "[KNN] Real Time: "<<  t.real_time() <<"s" << endl;
    cout << "[KNN] CPU Time: "<<  t.cpu_time() << "s" << endl;
    
    
    t.start();
    Data data_model;
    data_model.load_from_knn(*knn_module);
#ifdef USE_CUDA
    data_model.compute_similarity_GPU(n_threads, perplexity);
#else
    data_model.compute_similarity(n_threads, perplexity, true, true);
#endif
    t.end();
    printf("[SIMILARITY] CPU Time: %.3f s Real Time: %.3f s\n", t.cpu_time(), t.real_time());    


    t.start();
    vector<Multilevel*> mls = Multilevel::gen_multilevel(*knn_module, 100, knn_level, multiMode, data_model.similarity_weight); // 100 means min cluster
    t.end();
    printf("[MULTILEVEL] CPU Time: %.3f s Real Time: %.3f s\n", t.cpu_time(), t.real_time());


    delete knn_module;

    visualizemod vis;
    t.start();
    vis.run(&data_model, n_threads, n_samples, alpha, n_negative, n_gamma, mls, multiMode, 1);
    all.end();
    t.end();
    printf("[VIS] CPU Time: %.3f s Real Time: %.3f s\n", t.cpu_time(), t.real_time());
    printf("[ALL] CPU Time: %.3f s Real Time: %.3f s\n", all.cpu_time(), all.real_time());
    
    
    vis.save(outfile);
    
    if(!useEvaluation) return;

    DR::evaluation evaluation_module;
    
    evaluation_module.load_data(labelfile, knnfile, outfile);
    evaluation_module.sampling();
    evaluation_module.accuracy_All(50);
}


void evaGraphLayout() {
    Data data_model;
    data_model.load_from_graph(infile);

    {
        GL::evaluation eva(&data_model);
        eva.load_data(const_cast<char*>(outfile.c_str()));
        printf("[Evaluation] stress = %.6f\n", eva.stress());
    }

//    {
//        evaluation eva(&data_model);
//        eva.load_data(outfile);
//        printf("[Evaluation] stress_neighbors = %.6f\n", eva.stress_neighbor());
//    }

    {
        data_model.shortest_path_length(2);
        GL::evaluation eva(&data_model);
        eva.load_data(const_cast<char*>(outfile.c_str()));
        printf("[Evaluation] jaccard = %.6f\n", eva.jaccard());
    }
}


void solveGraphLayout() {
    if(multiMode == 0) {
        cout << "[WARNINIG] we suggest to use FM3 multilevel mode for GraphLayout !" << endl;
    }
    timer t, all;
    printf("disMax: %d n_samples: %d n_gamma: %.3f n_negative: %d\n", disMax, n_samples, n_gamma, n_negative);

    Data data_model;
    data_model.load_from_graph(infile);
    //data_model.testConnect();
    //data_model.shortest_path_length(disMax);
    
    all.start();
    
    t.start();
    vector<Multilevel*> mls = Multilevel::gen_multilevel(&data_model, 100, -1, multiMode); // 100 means min cluster
    t.end();
    printf("[MULTILEVEL] CPU Time: %.3f s Real Time: %.3f s\n", t.cpu_time(), t.real_time());

    t.start();
    data_model.compute_similarity(n_threads, perplexity, false, false);
    t.end();
    
    printf("[SIMILARITY] CPU Time: %.3f s Real Time: %.3f s\n", t.cpu_time(), t.real_time());

    printf("use parameter A: %.3f  B: %.3f\n", parameterA, parameterB);
    
    
    t.start();
    visualizemod vis;
    vis.run(&data_model, n_threads, n_samples, alpha, n_negative, n_gamma, mls, multiMode, 0.01, make_pair(parameterA, parameterB));
    all.end();
    t.end();
    printf("[VIS] CPU Time: %.3f s Real Time: %.3f s\n", t.cpu_time(), t.real_time());
    printf("[ALL] CPU Time: %.3f s Real Time: %.3f s\n", all.cpu_time(), all.real_time());
    vis.save(outfile);
    
    if(!useEvaluation) return;

    evaGraphLayout();
}

int main(int argc, char** argv){
    setParams(argc,argv);
    
    if(algoMode == 0) {
        solveDimensionReduction();
    } else {
        solveGraphLayout();
    }

    return 0;
}

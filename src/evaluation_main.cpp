#include "data.h"
#include "evaluation_GL.h"
#include "timer.h"

enum TYPE {
	NP = 0, NS, NSN, KLD 
};

int disMax = 2;

char infile[1000], outfile[1000];

int ArgPos(char *str, int argc, char **argv) {
    int a;
    for (a = 1; a < argc; a++) if (!strcmp(str, argv[a])) {
            if (a == argc - 1) {
                printf("Argument missing for %s\n", str);
                exit(1);
            }
            return a;
        }
    return -1;
}

int main(int argc, char **argv)
{
    int i, type;
    if (argc < 2)
    {
        printf("-input: Input file of feature vectors or networks\n");
        printf("-output: Output file of low-dimensional representations.\n");
        printf("-disMax: The max distance user for breadth first search. Default is 2.\n");
        return 0;
    }
    if ((i = ArgPos((char *)"-input", argc, argv)) > 0) strcpy(infile, argv[i + 1]);
    if ((i = ArgPos((char *)"-output", argc, argv)) > 0) strcpy(outfile, argv[i + 1]);
    if ((i = ArgPos((char *)"-disMax", argc, argv)) > 0) disMax = atof(argv[i + 1]);
    if ((i = ArgPos((char *)"-type", argc, argv)) > 0) type = atoi(argv[i + 1]);
	// if ((i = ArgPos((char *)"-NS", argc, argv)) > 0) type = NS;
	// if ((i = ArgPos((char *)"-NSN", argc, argv)) > 0) type = NSN;
    
    if(type > 3) {
        printf("we haven't this type\n");
        exit(0);
    }
    timer t;
    t.start();

    Data data_model;

    string in_file = string(infile);

	data_model.load_from_graph(in_file);
    if (type == NP) {
		data_model.shortest_path_length(disMax);
		GL::evaluation eva(&data_model);
		eva.load_data(outfile);
		printf("[Evaluation] jaccard = %.6f\n", eva.jaccard());
	} else if (type == NS){
		GL::evaluation eva(&data_model);
        eva.load_data(outfile);
		printf("[Evaluation] stress = %.6f\n", eva.stress());
    } 
    else if (type == NSN){
		GL::evaluation eva(&data_model);
        eva.load_data(outfile);
        printf("[Evaluation] stress_neighbors = %.6f\n", eva.stress_neighbor());
    } else if(type == KLD){
        GL::evaluation eva(&data_model);
        eva.load_data(outfile);
        printf("[Evaluation] KL Distance = %.6f\n", eva.loss());
    }

    t.end();
    return 0;

}

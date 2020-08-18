#ifndef DATA_CPP
#define DATA_CPP

#include "data.h"
#include <map>
#include <iostream>
#include <fstream>
#include <malloc.h>
#include <sstream>
#include <cfloat>
#include "timer.h"
#include "knn.h"
#include <queue>
# ifdef USE_CUDA
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <thrust/for_each.h>
#include <thrust/iterator/zip_iterator.h>
#include <thrust/device_ptr.h>
#define BLOCK_NUM 32
#define THREADS_NUM 128
# endif


Data::Data() {}

Data::~Data() {
    if(head) { delete[] head; head = nullptr; }
    if(similarity_weight) { delete[] similarity_weight; similarity_weight = nullptr; }
}


Data::Data(const Data &d) {
    n_vertices = d.n_vertices;
    n_edges = d.n_edges;
    n_threads = d.n_threads;
    perplexity = d.perplexity;
    disMAX = d.disMAX;
    graph.assign(d.graph.begin(), d.graph.end());
    memcpy(head, d.head, d.n_vertices * sizeof(int));
}

float* Data::load_vec(char *infile, int &vertices, int &out_dim)
{
    float *res;
    string line;
    ifstream fin(infile);
    if (fin) {
        cout << "Read vec from " << infile << endl;
        getline(fin, line);
        istringstream inputstr(line);
        inputstr >> vertices >> out_dim;
        res = new float[vertices * out_dim];
        int i = 0;
        while (getline(fin, line) && i < vertices) {
            int cnt = 0;
            for(int j = 0; j < line.size(); ++j) {
                if(line[j] == ' ')
                    cnt ++;
            }
            istringstream inputline(line);

            float a ;

            if(cnt == 2) inputline >> a ;
            for (int j = 0; j < out_dim; j++) {
                inputline >> res[i * out_dim + j];
            }
            i++;
        }
    } else {
        cout << "File not found!\n" << endl;
        exit(1);
    }
    fin.close();
    return res;
}

void Data::load_from_graph(string& infile){
    /*
    vertices(n) edges(m)
    fromNode_1 toNode_1 weight_1
    fromNode_2 toNode_2 weight_2
    ...
    fromNode_m toNode_m weight_m
    */    
    int edgenum, x, y;
    float weight;
    ifstream fin(infile.c_str());
    string line;
    if (fin) {
        cout << "Reading graph edges from " << infile << endl;
        getline(fin, line);
        istringstream inputstr(line);
        inputstr >> n_vertices >> edgenum;
        reserver_data(n_vertices, edgenum);
        int i ;
        for ( i = 0; i < edgenum; i++) {
            getline(fin, line);
            istringstream inputline(line);
            inputline >> x >> y >> weight;
            add_edge(x, y, weight);
            if (n_edges % 5000 == 0) {
                cout << "\rReading " << n_edges / 1000 << "K edges" << flush;
            }
        }
        cout << endl;
    } else {
        cout << "input file not found!" << endl;
        exit(1);
    }
    fin.close();
}

void Data::load_from_knn(knn& d){
    n_vertices = d.n_vertices;
    reverse.clear();
    reserver_data(n_vertices, n_vertices * d.knn_vec[0].size());
    int cnt = 0;
    for (int x = 0; x < n_vertices; ++x)
    {
        // printf("knn size: %d\n", (int)d.knn_vec[x].size());
        for (int i = 0, len = d.knn_vec[x].size(); i < len; ++i)
        {
            int y = d.knn_vec[x][i];
            if(y < 0 || y >= n_vertices || x < 0 || x >= n_vertices || x == y) {
                cnt ++;
                continue;
            }
            
            add_one_edge(x, y, d.CalcDist(x, y));
            reverse.push_back(-1);
        }
    }

    printf("[KNN] find bug edges for %d times \n", cnt);

}



void Data::reserver_data(int n, int m) {
    graph.clear();
    graph.reserve(m * 2);
    head = new int[n];
    for(int i = 0; i < n; ++i) {
        head[i] = -1;
    }
    n_edges = 0;
    edgeSet.clear();
}

void Data::testConnect() {
    int *fa = new int[n_vertices];

    int *du = new int[n_vertices];

    for(int i = 0; i < n_vertices; ++i) {
        fa[i] = i;
    }
    for(int i = 0; i < n_vertices; ++i) {
        for(int j = head[i]; ~j; j = graph[j].next) {
            int to = graph[j].to; int fr = i;
            du[fr] ++; du[to] ++;
            int t1 = findFather(fa, fr); int t2 = findFather(fa, to);
            if(t1 != t2) {
                fa[t1] = t2;
            }
        }
    }
    map<int, int> mp;
    int maxDu = -1;
    for(int i = 0; i < n_vertices; ++i) {
        int tt = findFather(fa, i);
        mp[tt] ++;
        maxDu = max(maxDu, tt);
    }

    printf("components parts: %d\n", (int)mp.size());
    printf("Max du: %d vertices: %d edges: %d\n", maxDu, n_vertices, n_edges);
    
    delete[] fa;
}

void Data::add_edge(int v1, int v2, float weight) {
#ifdef CHECK_EDGE
    if(v1 > v2) {
        swap(v1, v2);
    } 
    if(v1 == v2) {
        printf("There is a one node ring\n");
    }
    if(edgeSet.find(make_pair(v1, v2)) != edgeSet.end()) {
        // printf("there is a repeat edge\n");
        return;
    }
    edgeSet.insert(make_pair(v1, v2));
#endif

    graph.push_back(Edge(v1, v2, head[v1], weight)); head[v1] = n_edges ++;
    graph.push_back(Edge(v2, v1, head[v2], weight)); head[v2] = n_edges ++;
}


void Data::add_one_edge(int v1, int v2, float weight) {
    graph.push_back(Edge(v1, v2, head[v1], weight)); head[v1] = n_edges ++;
}




#ifdef USE_CUDA
__global__ static void Kernel_similarity(
    float perplexity, int* head, int* next, float* edge_weight, float* similarity_weight, int data_size) {
    const size_t tID = size_t(threadIdx.x);
    const size_t bID = size_t(blockIdx.x);

    
    int p, x;

    for ( size_t idx = bID * THREADS_NUM + tID;
        idx < data_size;
        idx += BLOCK_NUM * THREADS_NUM )
    {
        x = idx;
        float lo_beta = -1, hi_beta = -1, H, tmp, sum_weight, beta = 1;
        for (int iter = 0; iter < 200; ++iter)
        {
            H = 0;
            sum_weight = FLT_MIN;

            for (p = head[x]; p >= 0; p = next[p])
            {
                sum_weight += tmp = exp(-beta * edge_weight[p]);
                H += beta * (edge_weight[p] * tmp);
            }
            H = (H / sum_weight) + log(sum_weight);
            if (fabs(H - log(perplexity)) < 1e-5) {
                break;
            }
            if (H > log(perplexity))
            {
                lo_beta = beta;
                if (hi_beta < 0) beta *= 2; else beta = (beta + hi_beta) / 2;
            }
            else{
                hi_beta = beta;
                if (lo_beta < 0) beta /= 2; else beta = (lo_beta + beta) / 2;
            }
            if(beta > FLT_MAX) beta = FLT_MAX;
        }

        for (p = head[x], sum_weight = FLT_MIN; p >= 0; p = next[p])
        {
            sum_weight += edge_weight[p] = exp(-beta * edge_weight[p]);
        }
        for (p = head[x]; p >= 0; p = next[p])
        {
            edge_weight[p] /= sum_weight;
        }
        similarity_weight[x] = sum_weight;
    }
}
void Data::compute_similarity_GPU(int n_thre, float perp)
{
    n_threads = n_thre;
    perplexity = perp;
    similarity_weight = new float[n_vertices];


    size_t size1 = n_vertices * sizeof(int);
    int *head_host = (int *)malloc(size1);
    size_t size2 = n_edges * sizeof(int);
    float *weight_host = (float *)malloc(size2);
    int *next_host = (int *)malloc(size2);
    for(int i = 0; i < n_vertices; ++i) {
        head_host[i] = head[i];
    }
    for(int i = 0; i < n_edges; ++i) {
        weight_host[i] = graph[i].weight;
        next_host[i] = graph[i].next;
    }
    int *head_device = NULL;
    float *weight_device = NULL;
    int *next_device = NULL;
    float* similarity_weight_device = NULL;
    cudaMalloc((void **)&head_device, size1);
    cudaMalloc((void **)&weight_device, size2);
    cudaMalloc((void **)&next_device, size2);
    cudaMalloc((void **)&similarity_weight_device, size1);
    cudaMemcpy(head_device, head_host, size1, cudaMemcpyHostToDevice);
    cudaMemcpy(weight_device, weight_host, size2, cudaMemcpyHostToDevice);
    cudaMemcpy(next_device, next_host, size2, cudaMemcpyHostToDevice);
    cudaMemcpy(similarity_weight_device, similarity_weight, size1, cudaMemcpyHostToDevice);

    Kernel_similarity<<<BLOCK_NUM, THREADS_NUM>>>(
        perplexity, 
        head_device, 
        next_device,
        weight_device,
        similarity_weight_device,
        n_vertices
    );
    cudaMemcpy(weight_host, weight_device, size2, cudaMemcpyDeviceToHost);
    cudaMemcpy(similarity_weight, similarity_weight_device, size1, cudaMemcpyDeviceToHost);
    
    for(int i = 0; i < n_edges; ++i) {
        graph[i].weight = weight_host[i];
    }
    
    cudaFree(head_device);
    cudaFree(weight_device);
    cudaFree(next_device);
    cudaFree(similarity_weight_device);
    free(head_host);
    free(weight_host);
    free(next_host);
    
    solve_reverse_edge(true);

}
#endif

void Data::compute_similarity(int n_thre, float perp, bool isDimensionReduction, bool no_pair_edge){
    similarity_weight = new float[n_vertices];
    n_threads = n_thre;
    perplexity = perp;
    
    if(isDimensionReduction) {
        pthread_t *pt = new pthread_t[n_threads];
        for (int j = 0; j < n_threads; ++j) pthread_create(&pt[j], NULL, Data::compute_similarity_thread_caller, new arg_struct(this, j));
        for (int j = 0; j < n_threads; ++j) pthread_join(pt[j], NULL);
        delete[] pt;

    } else {
        float exparr[disMAX + 2];
        for (int i = 0; i <= disMAX + 1; i++) {
            exparr[i] = exp(- i);
        }

        float sum_weight = 0;
        for (int x = 0; x < n_vertices; ++x){
            float sum = 0;
            for (int p = head[x]; p >= 0; p = graph[p].next){
                sum += graph[p].weight = exparr[(int)graph[p].weight];
            }
            sum_weight += sum;
            similarity_weight[x] = sum;
        }
        for (int x = 0; x < n_vertices; ++x){
            for (int p = head[x]; ~p; p = graph[p].next) {
                graph[p].weight = graph[p].weight * n_vertices / sum_weight;
            }
        }
    }

    solve_reverse_edge(no_pair_edge);
}

void Data::solve_reverse_edge(bool no_pair_edge) {
    if(!no_pair_edge) {
        PARALLEL_FOR(n_threads, n_vertices, {
            for (int p = head[loop_i]; ~p; p = graph[p].next) {
                int y = graph[p].to;
                if (loop_i > y){
                    graph[p].weight = graph[p ^ 1].weight = (graph[p].weight + graph[p ^ 1].weight) / 2;
                }
            }
        });
    } else {
        pthread_t *pt = new pthread_t[n_threads];
        for (int j = 0; j < n_threads; ++j) pthread_create(&pt[j], NULL, Data::search_reverse_thread_caller, new arg_struct(this, j));
        for (int j = 0; j < n_threads; ++j) pthread_join(pt[j], NULL);
        delete[] pt;

        for (int x = 0; x < n_vertices; ++x) {
            for (int p = head[x]; p >= 0; p = graph[p].next) {
                int y = graph[p].to;
                int q = reverse[p];
                if (q == -1)
                {
                    add_one_edge(y, x, 0);
                    reverse.push_back(p);
                    q = reverse[p] = n_edges - 1;
                }
                if (x > y){
                    graph[p].weight = graph[q].weight = (graph[p].weight + graph[q].weight) / 2;
                }
            }
        }
    }

    reverse.clear();


}
void *Data::search_reverse_thread_caller(void *arg)
{
    Data *ptr = (Data*)(((arg_struct*)arg)->ptr);
    ptr->search_reverse_thread(((arg_struct*)arg)->id);
    pthread_exit(NULL);
}

void Data::search_reverse_thread(int id)
{
    int lo = id * n_vertices / n_threads;
    int hi = (id + 1) * n_vertices / n_threads;
    int x, y, p, q;
    for (x = lo; x < hi; ++x) {
        for (p = head[x]; p >= 0; p = graph[p].next)
        {
            y = graph[p].to;
            // if(graph[p ^ 1].from == y && graph[p ^ 1].to == x) {
            //     reverse[p] = p ^ 1;
            //     continue;
            // }

            for (q = head[y]; q >= 0; q = graph[q].next)
            {
                if (graph[q].to == x) break;
            }
            reverse[p] = q;
        }
    }
}


void *Data::compute_similarity_thread_caller(void *arg){
    Data *ptr = (Data*)(((arg_struct*)arg)->ptr);
    ptr->compute_similarity_thread(((arg_struct*)arg)->id);
    pthread_exit(NULL);
}

void Data::compute_similarity_thread(int id) {
    int lo = id * n_vertices / n_threads;
    int hi = (id + 1) * n_vertices / n_threads;
    int x, iter, p;
    // float exparr[disMAX + 2];

    float beta, lo_beta, hi_beta, sum_weight, H, tmp;
    for (x = lo; x < hi; ++x)
    {
        beta = 1;
        lo_beta = hi_beta = -1;
        // http://www.datakit.cn/blog/2017/02/05/t_sne_full.html
        // binary search the /phi result
        for (iter = 0; iter < 200; ++iter)
        {
            H = 0;
            sum_weight = FLT_MIN;
            // for (int i = 0; i <= disMAX + 1; i++) {
            //     exparr[i] = exp(-beta * i * i);
            // }
            for (p = head[x]; ~p; p = graph[p].next)
            {
                // if(graph[p].weight == 0) continue;
                sum_weight += tmp = exp(-beta * graph[p].weight);
                H += beta * (graph[p].weight * tmp);
            }
            H = (H / sum_weight) + log(sum_weight);
            if (fabs(H - log(perplexity)) < 1e-5) {
                break;
            }
            if (H > log(perplexity))
            {
                lo_beta = beta;
                if (hi_beta < 0) beta *= 2; else beta = (beta + hi_beta) / 2;
            }
            else{
                hi_beta = beta;
                if (lo_beta < 0) beta /= 2; else beta = (lo_beta + beta) / 2;
            }
            if(beta > FLT_MAX) beta = FLT_MAX;
        }

        // for (int i = 0; i <= disMAX + 1; i++) {
        //     exparr[i] = exp(-beta * i * i);
        // }
        for (p = head[x], sum_weight = FLT_MIN; p >= 0; p = graph[p].next)
        {
            // if(graph[p].weight == 0) continue;
            sum_weight += graph[p].weight = exp(-beta * graph[p].weight);
        }

        for (p = head[x]; ~p; p = graph[p].next)
        {
            graph[p].weight /= sum_weight;
        }
        similarity_weight[x] = sum_weight;
    }
}


void Data::shortest_path_length(int dismax) {
    if (dismax == 1) return;
    disMAX = dismax;


    vector<int> *distances = new vector<int>[n_vertices + 1];
    vector<int> *adjtarget = new vector<int>[n_vertices + 1];

    #pragma omp parallel for
    for (int i = 0; i < n_vertices; i++ ) {
        bfs(i, &distances[i], &adjtarget[i]);
    }
    
    for (int i = 0; i < n_vertices; i++) {
        for (int j = 0; j < adjtarget[i].size(); j++) {
            add_edge(i, adjtarget[i][j], distances[i][j]);
        }
    }

    for (int i = 0; i < n_vertices; i++) {
        vector<int>().swap(distances[i]);
        vector<int>().swap(adjtarget[i]);
    }
    delete[] distances; distances = NULL;
    delete[] adjtarget; adjtarget = NULL;
}

void Data::bfs(int s, vector<int> *distance, vector<int> *adjtarget) {
    // using namespace std::tr1;
    std::unordered_map<int, int> dist;
    dist[s] = 0;
    queue<int> Q;
    Q.push(s);
    int level = 0;

    while( !Q.empty() ){
        int u = Q.front();
        Q.pop();
        if (dist[u] + 1 > disMAX) break;

        for(int i = head[u]; ~i; i = graph[i].next){
            int v = graph[i].to;
            unordered_map<int, int>::const_iterator got = dist.find(v);
            if( got == dist.end() ){
                Q.push(v);
                dist[v] = dist[u] + 1;
                level = dist[v];
                if(s < v && dist[v] > 1) {
                    (*distance).push_back(level);
                    (*adjtarget).push_back(v);
                }
            }
        }
    }
}




#endif

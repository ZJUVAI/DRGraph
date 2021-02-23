#ifndef KNN_CPP
#define KNN_CPP

#include "knn.h"

knn::~knn() {
    delete[] vec; vec = NULL;
    delete[] knn_vec; knn_vec = NULL;
}

knn::knn () {
}

const gsl_rng_type *knn::gsl_T = NULL;
gsl_rng *knn::gsl_r = NULL;

void knn::load_data(string& infile){

    FILE *fin = fopen(infile.c_str(), "rb");
    if (fin == NULL){
        printf("\nInput File Not Found!\n");
        exit(1);
    }
    // cout<<"[DATA] "<<"Read from "<<infile<<endl;
    auto ret = fscanf(fin, "%d%d", &n_vertices, &n_dim);
    
    if (ret == EOF){
        printf("Reading error\n");
        return;
    }

    vec = new float[n_vertices * n_dim];
    for (int i = 0; i < n_vertices; ++i){
    //    int tt = fscanf(fin, "%*s");
        for (int j = 0; j < n_dim; ++j){
            auto ret = fscanf(fin, "%f", &vec[i * n_dim + j]);
            if (ret == EOF){
                printf("Reading error\n");
            }
        }
    }
    fclose(fin);
    cout<<"[DATA] "<<"#vertices="<<n_vertices<<" "<<"#dim="<<n_dim<<endl;
    normalize();
    knn_vec = new vector<int>[this->n_vertices];
}

void knn::normalize() {
    cout << "[DATA] " << "Normalizing ......";
    float *mean = new float[n_dim];
    for (int i = 0; i < n_dim; ++i) mean[i] = 0;

    for (int i = 0, ll = 0; i < n_vertices; ++i, ll += n_dim){
        for (int j = 0; j < n_dim; ++j)
            mean[j] += vec[ll + j];
    }
    for (int j = 0; j < n_dim; ++j){
        mean[j] /= n_vertices;
    }
    float mX = 0;
    for (int i = 0, ll = 0; i < n_vertices; ++i, ll += n_dim){
        for (int j = 0; j < n_dim; ++j){
            vec[ll + j] -= mean[j];
            if (fabs(vec[ll + j]) > mX) mX = fabs(vec[ll + j]);
        }
    }
    for (int i = 0; i < n_vertices * n_dim; ++i){
        vec[i] /= mX;
    }
    delete[] mean;
    cout << " Done." << endl;
}



void knn::setParams(int n_tree, int n_neig, int n_thre, int n_prop,
                     int knn_tre, int epo, int mle, int l, int che, int use, int k, int s, int build_tre, string knn_tp) {
    gsl_rng_env_setup();
    gsl_T = gsl_rng_rand48;
    gsl_r = gsl_rng_alloc(gsl_T);
    gsl_rng_set(gsl_r, 314159265);

    knn_type = knn_tp;

    // largevis knn
    n_threads = n_thre < 0 ? 8 : n_thre;
    n_trees = n_tree;
    n_neighbors = n_neig < 0 ? 150 : n_neig;
    n_propagations = n_prop < 0 ? 3 : n_prop;
    if (n_trees < 0){
        if (n_vertices < 100000)
            n_trees = 10;
        else if (n_vertices < 1000000)
            n_trees = 20;
        else if (n_vertices < 5000000)
            n_trees = 50;
        else n_trees = 100;
    }

    if (knn_trees < 0){
        if (n_vertices < 100000)
            knn_trees = 8;
        else if (n_vertices < 1000000)
            knn_trees = 16;
        else if (n_vertices < 5000000)
            knn_trees = 64;
        else knn_trees = 128;
    }

    // efanna knn graph
    
    knn_trees = knn_tre < 0 ? 8 : knn_tre;
    mlevel = mle < 0 ? 8 : mle;
    epochs = epo < 0 ? 8 : epo;
    L = l < 0 ? 30 : l;
    checkK = che < 0 ? 25 : che;
    knn_k = k < 0 ? 10 : k;
    S = s < 0 ? 10 : s;
    build_trees = knn_trees;
    n_neighbors = knn_k;
}

void knn::load_knn(string& infile) {
    ifstream fin(infile);
    string line;

    knn_vec= new vector<int>[n_vertices];
    if (fin) {
        cout << "[DATA] " << "Read from " << infile << endl;
        int i = 0, temp;
        while (getline(fin, line) && i < n_vertices) {
            istringstream inputstr(line);
            while (inputstr >> temp) {
                knn_vec[i].push_back(temp);
            }
            i++;
        }
    } else {
        cout << "\nFile not found!\n" << endl;
        exit(1);
    }
    fin.close();
    return;
}


void knn::construct_knn () {
    if (knn_type.empty()) {
        cout << "[kNN Graph] " << "no specified the type of constructing knn." << endl;
        exit(2);
    } else {
        timer t;
        if (knn_type == "largevis") {
            t.start(); 
            run_annoy();
            run_propagation();
            t.end(); 
            cout<<"[kNN Graph] " << "Largevis kNN building CPU time : " << t.cpu_time() << " seconds" << endl;
            cout<< "[kNN Graph] " << "Largevis kNN building float time: " << t.real_time() << " seconds"<<endl;
        } else if (knn_type == "efanna") {
            t.start(); 
            efanna::Matrix<float> dataset(n_vertices, n_dim, vec);
            //generate knn graph
            printf("[kNN Graph] knn_trees: %d mlevel: %d epochs: %d checK: %d L: %d knn_k: %d build_trees: %d S: %d\n", knn_trees, mlevel, epochs, checkK, L, knn_k, build_trees, S);
            efanna::FIndex<float> index(dataset, new efanna::L2DistanceAVX<float>(), efanna::KDTreeUbIndexParams(true, knn_trees, mlevel, epochs, checkK, L, knn_k, build_trees, S));
            index.buildIndex();
            t.end(); 
            cout<< "[kNN Graph] " << "EFanna kNN building CPU time: " << t.cpu_time() << " seconds"<<endl;
            cout<<"[kNN Graph] " << "Efanna kNN building float time: " << t.real_time() << " seconds" << endl;
            index.getGraphResult(knn_vec);
        }
        test_accuracy();
        // for(int i = 0; i < n_vertices; ++i) {
        //     for(int j = 0; j < checkK - knn_k; ++j) {
        //         knn_vec[i].push_back(int(gsl_rng_uniform(gsl_r) * (n_vertices - 0.1)));
        //     }
        // }
    }
}

void *knn::annoy_thread_caller(void *arg)
{
    knn *ptr = (knn*)(((arg_struct*)arg)->ptr);
    ptr->annoy_thread(((arg_struct*)arg)->id);
    pthread_exit(NULL);
}

void knn::annoy_thread(int id)
{
    int lo = id * n_vertices / n_threads;
    int hi = (id + 1) * n_vertices / n_threads;
    AnnoyIndex<int, float, Euclidean, Kiss64Random> *cur_annoy_index = NULL;
    if (id > 0)
    {
        cur_annoy_index = new AnnoyIndex<int, float, Euclidean, Kiss64Random>(n_dim);
        cur_annoy_index->load("annoy_index_file");
    }
    else
        cur_annoy_index = annoy_index;
    for (int i = lo; i < hi; ++i)
    {
        cur_annoy_index->get_nns_by_item(i, n_neighbors + 1, (n_neighbors + 1) * n_trees, &knn_vec[i], NULL);
        for (int j = 0; j < knn_vec[i].size(); ++j)
            if (knn_vec[i][j] == i)
            {
                knn_vec[i].erase(knn_vec[i].begin() + j);
                break;
            }
    }
    if (id > 0) delete cur_annoy_index;
}

void knn::run_annoy() {
    printf("[kNN Graph] Running ANNOY ......"); fflush(stdout);
    annoy_index = new AnnoyIndex<int, float, Euclidean, Kiss64Random>(n_dim);
    for (int i = 0; i < n_vertices; ++i)
        annoy_index->add_item(i, &vec[i * n_dim]);
    annoy_index->build(n_trees);
    if (n_threads > 1) annoy_index->save("annoy_index_file");
    knn_vec = new std::vector<int>[n_vertices];

    pthread_t *pt = new pthread_t[n_threads];
    for (int j = 0; j < n_threads; ++j) pthread_create(&pt[j], NULL, knn::annoy_thread_caller, new arg_struct(this, j));
    for (int j = 0; j < n_threads; ++j) pthread_join(pt[j], NULL);
    delete[] pt;
    delete annoy_index; annoy_index = NULL;
    printf(" Done.\n");
}

void knn::propagation_thread(int id)
{
    int lo = id * n_vertices / n_threads;
    int hi = (id + 1) * n_vertices / n_threads;
    int *check = new int[n_vertices];
    std::priority_queue< pair<float, int> > heap;
    int x, y, i, j, l1, l2;
    for (x = 0; x < n_vertices; ++x) check[x] = -1;
    for (x = lo; x < hi; ++x)
    {
        check[x] = x;
        std::vector<int> &v1 = old_knn_vec[x];
        l1 = v1.size();
        for (i = 0; i < l1; ++i)
        {
            y = v1[i];
            check[y] = x;
            heap.push(std::make_pair(CalcDist(x, y), y));
            if (heap.size() == n_neighbors + 1) heap.pop();
        }
        for (i = 0; i < l1; ++i)
        {
            std::vector<int> &v2 = old_knn_vec[v1[i]];
            l2 = v2.size();
            for (j = 0; j < l2; ++j) if (check[y = v2[j]] != x)
                {
                    check[y] = x;
                    heap.push(std::make_pair(CalcDist(x, y), (int)y));
                    if (heap.size() == n_neighbors + 1) heap.pop();
                }
        }
        while (!heap.empty())
        {
            knn_vec[x].push_back(heap.top().second);
            heap.pop();
        }
    }
    delete[] check;
}

void *knn::propagation_thread_caller(void *arg)
{
    knn *ptr = (knn*)(((arg_struct*)arg)->ptr);
    ptr->propagation_thread(((arg_struct*)arg)->id);
    pthread_exit(NULL);
}

void knn::run_propagation()
{
    for (int i = 0; i < n_propagations; ++i)
    {
        printf("[kNN Graph] Running propagation %d/%d%c", (int)i + 1, (int)n_propagations, 13);
        fflush(stdout);
        old_knn_vec = knn_vec;
        knn_vec = new std::vector<int>[n_vertices];
        pthread_t *pt = new pthread_t[n_threads];
        for (int j = 0; j < n_threads; ++j) pthread_create(&pt[j], NULL, knn::propagation_thread_caller, new arg_struct(this, j));
        for (int j = 0; j < n_threads; ++j) pthread_join(pt[j], NULL);
        delete[] pt;
        delete[] old_knn_vec;
        old_knn_vec = NULL;
    }
    printf("\n");
}


float knn::CalcDist(int x, int y)
{
    float ret = 0;
    int i, lx = x * n_dim, ly = y * n_dim;
    for (i = 0; i < n_dim; ++i)
        ret += (vec[lx + i] - vec[ly + i]) * (vec[lx + i] - vec[ly + i]);
    return ret;
}

void knn::test_accuracy() {
    n_neighbors = knn_vec[0].size();
    int test_case = 100;
    std::priority_queue< pair<float, int> > *heap = new std::priority_queue< pair<float, int> >;
    int hit_case = 0, i, j, x, y;
    for (i = 0; i < test_case; ++i)
    {
        x = floor(gsl_rng_uniform(gsl_r) * (n_vertices - 0.1));
        for (y = 0; y < n_vertices; ++y) if (x != y)
            {
                heap->push(std::make_pair(CalcDist(x, y), y));
                if (heap->size() == n_neighbors + 1) heap->pop();
            }
        while (!heap->empty())
        {
            y = heap->top().second;
            heap->pop();
            for (j = 0; j < knn_vec[x].size(); ++j) if (knn_vec[x][j] == y)
                    ++hit_case;
        }
    }
    delete heap;
    heap = NULL;
    printf("[kNN Graph] Test efanna knn accuracy(use largevis test) : %.2f%% in knn: %d\n", hit_case * 100.0 / (test_case * n_neighbors), n_neighbors);
}

void knn::save_knn(string& outfile){
    ofstream myfile;
    myfile.open (outfile);
    if(myfile.is_open()){
        for (int i = 0; i < n_vertices; ++i){
            for(int j=0; j < knn_vec[i].size(); j++){
                myfile << knn_vec[i][j];
                if (j != knn_vec[i].size() - 1) myfile << " ";
            }
            myfile<<"\n";
        }
        myfile<< flush;
        myfile.close();
    }else
    cout<< "unable to write file"<<endl;
}

#endif

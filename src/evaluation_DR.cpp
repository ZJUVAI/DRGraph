#ifndef EVALUATION
#define EVALUATION

#include "evaluation_DR.h"
#include <cmath>
#include <queue>


namespace DR {



evaluation::evaluation() {}

void evaluation::load_data(string& labelfile, string& knnfile, string& visfile) {
    vis = load_vis(visfile, n_vertices, out_dim);
    //std::cout<< n_vertices <<" " << out_dim <<  std::endl;
    // knn = d.load_knn(knnfile, n_vertices);
    label = load_label(labelfile, n_vertices);
    n_class = 0;
    for(int i =0; i < n_vertices; i++){
        if(label[i] > n_class){
            n_class = label[i];        std::vector<Dis> distance;

        }
    }
    n_class++;
}

int* evaluation::load_label(string& infile, int vertices) {
    int *label = new int[vertices];
    ifstream fin(infile);
    if (fin) {
        cout << "[LABEL] " << "Read from " << infile << endl;
        for (int i = 0; i < vertices && !fin.eof(); i++) {
           fin >> label[i];
        }
    } else {
        cout << "\nLabel File not found!\n" << endl;
        exit(1);
    }
    fin.close();
    return label;
}

vector<int>* evaluation::load_knn(string& infile, int vertices) {
    ifstream fin(infile);
    string line;

    vector<int>* knn = new vector<int>[vertices];
    if (fin) {
        cout << "[KNN] " << "Read from " << infile << endl;
        int i = 0, temp;
        while (getline(fin, line) && i < vertices) {
            istringstream inputstr(line);
            while (inputstr >> temp) {
                knn[i].push_back(temp);
            }
            i++;
        }
    } else {
        cout << "\nFile not found!\n" << endl;
        exit(1);
    }
    fin.close();
    return knn;
}

float* evaluation::load_vis(string& infile, int &vertices, int &dim) {
    float *res;
    string line;
    ifstream fin(infile);
    if (fin) {
        cout << "[DATA] " << "Read from " << infile << endl;
        getline(fin, line);
        istringstream inputstr(line);
        inputstr >> vertices >> dim;
        res = new float[vertices * dim];
        int i = 0;
        while (getline(fin, line) && i < vertices) {
            istringstream inputline(line);
            for (int j = 0; j < dim; j++) {
                inputline >> res[i * dim + j];
            }
            i++;
        }
    } else {
        cout << "\nFile not found!\n" << endl;
        exit(1);
    }
    fin.close();
    return res;
}

real evaluation::CalcDist2D(int x, int y) {
    real ret = 0;
    int i, lx = x * out_dim, ly = y * out_dim;
    for (i = 0; i < out_dim; i++) {
        ret += (vis[lx + i] - vis[ly + i]) * (vis[lx + i] - vis[ly + i]);
    }
    return ret;
}

void evaluation::sampling() {
    genrandom r;
    r.init_gsl();
    //split data into  10 fold at subs
    seq = new int[n_vertices];
    for (int i = 0; i < n_vertices; i++) seq[i] = i;
    samp_num = n_vertices / 10;
    samp = new int[samp_num];
    for (int j = 0; j < samp_num; j++) {
        cur_num = n_vertices - j;
        int t = (int)(r.gslRandom()*1600000) % cur_num;       //gsl [0,1)
        samp[j] = seq[t];
        seq[t] = seq[cur_num - 1];
    }
}

void evaluation::test_accuracy1() {
    timer t;
    t.start();

    double res;
    int hit_case = 0;
    for (int i = 0; i < samp_num; i++) {// for each valid item
        int neighbor = -1;
        real dis = FLT_MAX;
        for(int j = 0; j < cur_num; j++){//for each training item
            real temp = CalcDist2D(samp[i], seq[j]);
            if (dis > temp) {
                neighbor = seq[j];
                dis = temp;
            }
        }
        if (label[neighbor] == label[samp[i]]) hit_case++;
    }
    res = hit_case * 1.0 / samp_num * 100;

    t.end();

    cout << "[EVALUATION] Test average accuracy: " << res << "%"
         << " Evaluation Real Time: "<<  t.real_time() <<"s"
         << " Evaluation CPU Time: "<<  t.cpu_time() << "s" << endl;

}

void evaluation::accuracy(int k_neighbors) {
    this->k_neighbors = k_neighbors;

    timer t;
    t.start();

    hit = new int[n_threads];
    for(int i = 0; i < n_threads; i++){
        hit[i] = 0;
    }

    //pthread ...
    pthread_t *pt = new pthread_t[n_threads];
    for (int j = 0; j < n_threads; ++j){
        pthread_create(&pt[j], NULL, evaluation::accuracy_thread_caller, new arg_evaluation(this, j));
    }
    for (int j = 0; j < n_threads; ++j){
        pthread_join(pt[j], NULL);
    }
    delete[] pt;

    double res;
    int hit_case = 0;
    for(int i = 0; i < n_threads; i++){
        hit_case += hit[i];
    }
    res = hit_case * 1.0 / samp_num * 100;

    t.end();

    cout.setf(ios::fixed);
    cout.precision(4);
    cout << "[EVALUATION]" << setw(4) << right << k_neighbors << "-NN Classifier average accuracy: " << setw(5) << right << res << "%" << endl
         << " Evaluation Real Time: "<<  setw(7) << right << t.real_time() <<"s"
         << " Evaluation CPU Time: "<< setw(7) << right << t.cpu_time() << "s" << endl;
}

void *evaluation::accuracy_thread_caller(void* arg){
    evaluation* ptr = (evaluation*)(((arg_evaluation*)arg)->ptr);
    ptr->accuracy_thread(((arg_evaluation*)arg)->id);
    pthread_exit(NULL);
}
// void evaluation::accuracy_thread(int id){
//     int hit_case = 0;
//     for (int i = id; i < samp_num; i = i + n_threads) {// for each valid item
//         //std::cout << '\r' << i << std::flush ;
//         std::vector<Dis> distance;
//         float dis_t = 0;
//         for(int j = 0; j < cur_num; j++){//for each training item
//             real temp = CalcDist2D(samp[i], seq[j]);
//             if(j < k_neighbors){
//                 distance.push_back(Dis(seq[j],temp));
//                 if(temp > dis_t) {
//                     dis_t = temp;
//                 }
//             }else{
//                 if(temp >= dis_t){
//                     continue;
//                 }else{
//                     //dis_t = temp;
//                     distance.push_back(Dis(seq[j],temp));
//                     std::sort(distance.begin(), distance.end());
//                     dis_t = distance[k_neighbors - 1].distance;
//                     distance.erase(distance.end());
//                 }
//             }
//         }
//         int count[n_class];
//         for(int j=0; j < n_class; j++){
//             count[j]=0;
//         }
//         for(int j =0; j < k_neighbors; j++){
//             count[label[distance[j].index]]++;
//         }
//         int test_label = -1;
//         long test_label_n = -1;
//         for(int j = 0; j < n_class; j++){
//             if(count[j] > test_label_n){
//                 test_label_n = count[j];
//                 test_label = j;
//             }
//         }

//         if (test_label == label[samp[i]]) hit_case++;
//     }
//     hit[id] = hit_case;
// }



void evaluation::accuracy_thread(int Id){
    int hit_case = 0;
    real* distance = new real[k_neighbors + 1];
    int* id = new int[k_neighbors + 1];
    int* sortArray = new int[k_neighbors + 1];
    auto cmp = [&](int a, int b) {
        return distance[a] < distance[b];
    };

    for (int i = Id; i < samp_num; i = i + n_threads) {// for each valid item

        float dis_t = 0;
        bool firstSort = false;
        for(int j = 0; j < cur_num; j++){//for each training item
            real temp = CalcDist2D(samp[i], seq[j]);
            if(j < k_neighbors){
                distance[j] = temp;
                id[j] = seq[j];
                sortArray[j] = j;

                if(temp > dis_t) {
                    dis_t = temp;
                }
            }else{
                if(temp >= dis_t){
                    continue;
                }else{
                    //dis_t = temp;
                    if(!firstSort) {
                        std::sort(sortArray, sortArray + k_neighbors, cmp);
                        firstSort = true;
                    }

                    distance[k_neighbors] = temp;

                    int pos = upper_bound(sortArray, sortArray + k_neighbors, k_neighbors, cmp) - sortArray;
                    int tag = sortArray[k_neighbors - 1];
                    for(int z = k_neighbors - 1; z >= pos + 1; --z) {
                        sortArray[z] = sortArray[z - 1];
                    }
                    distance[tag] = temp;
                    id[tag] = seq[j];
                    sortArray[pos] = tag;

                    dis_t = distance[sortArray[k_neighbors - 1]];
                }
            }
            // Dis tmpDis = Dis(seq[j],temp);
            // if(distance.size() < k_neighbors) {
            //     distance.push(tmpDis);
            // } else {
            //     Dis tt = distance.top();
            //     if(tt.distance > tmpDis.distance) {
            //         distance.pop();
            //         distance.push(tmpDis);
            //     }
            // }
        }
        int count[n_class];
        for(int j=0; j < n_class; j++){
            count[j]=0;
        }
        for(int j =0; j < k_neighbors; j++){
            // Dis tt = distance.top();
            // count[label[tt.index]]++;
            count[label[id[j]]]++;

        }
        int test_label = -1;
        long test_label_n = -1;
        for(int j = 0; j < n_class; j++){
            if(count[j] > test_label_n){
                test_label_n = count[j];
                test_label = j;
            }
        }

        if (test_label == label[samp[i]]) hit_case++;
    }
    hit[Id] = hit_case;
    delete []distance;
    delete []id;
    delete []sortArray;
}



// void evaluation::accuracy_thread(int Id){
//     int hit_case = 0;
//     real* distance = new real[k_neighbors];
//     int* id = new int[k_neighbors];
//     int* sortArray = new int[k_neighbors];
//     auto cmp = [&](int a, int b) {
//         return distance[a] < distance[b];
//     };

//     for (int i = Id; i < samp_num; i = i + n_threads) {// for each valid item

//         float dis_t = 0;
//         bool firstSort = false;
//         for(int j = 0; j < cur_num; j++){//for each training item
//             real temp = CalcDist2D(samp[i], seq[j]);
//             if(j < k_neighbors){
//                 distance[j] = temp;
//                 id[j] = seq[j];
//                 sortArray[j] = j;

//                 if(temp > dis_t) {
//                     dis_t = temp;
//                 }
//             }else{
//                 if(temp >= dis_t){
//                     continue;
//                 }else{
//                     //dis_t = temp;
//                     if(!firstSort) {
//                         std::sort(sortArray, sortArray + k_neighbors, cmp);
//                         firstSort = true;
//                     }

//                     int tag = sortArray[k_neighbors - 1];
//                     distance[tag] = temp;
//                     id[tag] = seq[j];
//                     std::sort(sortArray, sortArray + k_neighbors, cmp);

//                     dis_t = distance[sortArray[k_neighbors - 1]];
//                 }
//             }
//             // Dis tmpDis = Dis(seq[j],temp);
//             // if(distance.size() < k_neighbors) {
//             //     distance.push(tmpDis);
//             // } else {
//             //     Dis tt = distance.top();
//             //     if(tt.distance > tmpDis.distance) {
//             //         distance.pop();
//             //         distance.push(tmpDis);
//             //     }
//             // }
//         }
//         int count[n_class];
//         for(int j=0; j < n_class; j++){
//             count[j]=0;
//         }
//         for(int j =0; j < k_neighbors; j++){
//             // Dis tt = distance.top();
//             // count[label[tt.index]]++;
//             count[label[id[j]]]++;

//         }
//         int test_label = -1;
//         long test_label_n = -1;
//         for(int j = 0; j < n_class; j++){
//             if(count[j] > test_label_n){
//                 test_label_n = count[j];
//                 test_label = j;
//             }
//         }

//         if (test_label == label[samp[i]]) hit_case++;
//     }
//     hit[Id] = hit_case;
//     delete []distance;
//     delete []id;
//     delete []sortArray;
// }

void evaluation::accuracy_All(int k_neighbors) {
    this->k_neighbors = k_neighbors;

    timer t;
    t.start();

    Hit = new int*[n_threads]; 
    for(int i = 0; i < n_threads; ++i) 
        Hit[i] = new int[7];
    for(int i = 0; i < n_threads; i++){
        Hit[i][0] = 0; Hit[i][1] = 0; Hit[i][2] = 0; Hit[i][3] = 0; Hit[i][4] = 0;
    }

    //pthread ...
    pthread_t *pt = new pthread_t[n_threads];
    for (int j = 0; j < n_threads; ++j){
        pthread_create(&pt[j], NULL, evaluation::accuracy_thread_caller_All, new arg_evaluation(this, j));
    }
    for (int j = 0; j < n_threads; ++j){
        pthread_join(pt[j], NULL);
    }
    delete[] pt;

    double res;
    int hit_case = 0;
    

    t.end();

    cout.setf(ios::fixed);
    cout.precision(4);
    
    cout << " Evaluation Real Time: "<<  setw(7) << right << t.real_time() <<"s"
         << " Evaluation CPU Time: "<< setw(7) << right << t.cpu_time() << "s" << endl;
    
    for(int j = 0; j < 7; ++j) {
        hit_case = 0;
        for(int i = 0; i < n_threads; i++){
            hit_case += Hit[i][j];
        }
        res = hit_case * 1.0 / samp_num * 100;
        cout << "[EVALUATION]" << setw(4) << right << Nei[j] << "-NN Classifier average accuracy: " << setw(5) << right << res << "%" << endl;
    }
}

void *evaluation::accuracy_thread_caller_All(void* arg){
    evaluation* ptr = (evaluation*)(((arg_evaluation*)arg)->ptr);
    ptr->accuracy_thread_All(((arg_evaluation*)arg)->id);
    pthread_exit(NULL);
}

void evaluation::accuracy_thread_All(int Id){
    int hit_case[7] = {0, 0, 0, 0, 0, 0, 0};
    real* distance = new real[k_neighbors + 1];
    int* id = new int[k_neighbors + 1];
    int* sortArray = new int[k_neighbors + 1];
    auto cmp = [&](int a, int b) {
        return distance[a] < distance[b];
    };

    for (int i = Id; i < samp_num; i = i + n_threads) {// for each valid item
        float dis_t = 0;
                bool firstSort = false;
        for(int j = 0; j < cur_num; j++){//for each training item
            real temp = CalcDist2D(samp[i], seq[j]);
            if(j < k_neighbors){
                distance[j] = temp;
                id[j] = seq[j];
                sortArray[j] = j;

                if(temp > dis_t) {
                    dis_t = temp;
                }
            }else{
                if(temp >= dis_t){
                    continue;
                }else{
                    //dis_t = temp;
                    if(!firstSort) {
                        std::sort(sortArray, sortArray + k_neighbors, cmp);
                        firstSort = true;
                    }

                    distance[k_neighbors] = temp;

                    int pos = upper_bound(sortArray, sortArray + k_neighbors, k_neighbors, cmp) - sortArray;
                    int tag = sortArray[k_neighbors - 1];
                    for(int z = k_neighbors - 1; z >= pos + 1; --z) {
                        sortArray[z] = sortArray[z - 1];
                    }
                    distance[tag] = temp;
                    id[tag] = seq[j];
                    sortArray[pos] = tag;

                    dis_t = distance[sortArray[k_neighbors - 1]];
                }
            }
        }
        int count[n_class];
        for(int j=0; j < n_class; j++){
            count[j]=0;
        }
        int cntNum = 0; int cntEdge = 0;
        for(int j =0; j < k_neighbors; j++){
            cntNum ++;
            count[label[id[sortArray[j]]]]++;
            if(cntNum == Nei[cntEdge]) {
                int test_label = -1;
                long test_label_n = -1;
                for(int z = 0; z < n_class; z++){
                    if(count[z] > test_label_n){
                        test_label_n = count[z];
                        test_label = z;
                    }
                }
                if (test_label == label[samp[i]]) hit_case[cntEdge]++;
                cntEdge ++;
            }
        }
        
    }
    for(int i = 0; i < 7; ++i) {
        Hit[Id][i] = hit_case[i];
    }
}


bool operator < (const Dis &d1, const Dis &d2){
    return d1.distance < d2.distance;
};



}


#endif

#ifndef SPACEPARTITION
#define SPACEPARTITION

#include <fstream>
#include "space_partition.h"
#include "timer.h"

extern "C" {
#include "algorithm/kmeans.h"
#include "algorithm/fastcommunity_mh.cc"
}


SpacePartition::SpacePartition() {}

void SpacePartition::init(float *vis, int n_ver, int out_d, int num_clus, vector<int> *vec) {
    n_vertices = n_ver;
    out_dim = out_d;
    num_cluster = num_clus;
    knn_vec = vec;
    sp_evaluation.init(n_vertices, out_dim);
    sp_evaluation.generate_samples();
    sp_evaluation.start_evaluation(vis);
}

void SpacePartition::k_means (float *vis) {
    timer t;
    t.start();
    membership = new int[n_vertices];
    float **vis_temp =  new float*[n_vertices];
    for (int i = 0; i < n_vertices; i++) {
        vis_temp[i] = new float[out_dim];
    }
    for (int i = 0; i < n_vertices; i++) {
        for (int j = 0; j < out_dim; j++) {
            vis_temp[i][j] = vis[i * out_dim + j];
        }
    }

//    centers = omp_kmeans(0, vis_temp, out_dim, n_vertices, num_cluster, 0.01, membership, centers);

//    clusters = new vector<int>[num_cluster];
//    for (int i = 0; i < n_vertices; i++) {
//        clusters[membership[i]].push_back(i);
//    }

    offset = new float*[num_cluster]; //初始化偏移矩阵
    for (int j = 0; j < num_cluster; j++) offset[j] = new float[out_dim]();
    delete[] vis_temp;vis_temp = NULL;
    t.end();
    cout << "[kmeans] " << " real time: " << t.real_time() << " seconds"<<endl;
}

void SpacePartition::quad_tree (float *vis) {
    timer t;
    t.start();
    QuadTree *root = new QuadTree(vis, n_vertices, out_dim);
    membership = new int[n_vertices]();
//    cout << "membership[1000]:" << membership[1000] << endl;
//    centers = new float*[root->num_leaf];

    dfs(root, vis);
    delete root; root = NULL;

    sp_evaluation.setStopParams(num_cluster);
//    clusters = new vector<int>[num_cluster];
//    for (int i = 0; i < n_vertices; i++) {
//        clusters[membership[i]].push_back(i);
//    }
    t.end();
    cout << " QuadTree Real Time: "<<  t.real_time() <<"s"
         << " QuadTree CPU Time: "<<  t.cpu_time() << "s" << endl;

    offset = new float*[num_cluster]; //初始化偏移矩阵
    for (int j = 0; j < num_cluster; j++) offset[j] = new float[out_dim]();
}

bool SpacePartition::isStart(float *vis) {
    float start_eval = sp_evaluation.start_evaluation(vis);
    if(start_eval > 0.96 && start_eval< 0.99) start_hit_count++;
    if (start_hit_count > 0) return true;
    else return false;
}


bool SpacePartition::isStop() {
    float stop_eval = sp_evaluation.stop_evaluation(offset);
    if(stop_eval > 0.96 && stop_eval < 0.99) return true;
    else return false;
}

void SpacePartition::output(float *vis) {
    for (int j = 0; j < n_vertices; j++) {
        for (int k = 0; k < out_dim; k++) {
            vis[j * out_dim + k] += offset[membership[j]][k];
        }
    }
    // delete[] offset;
    // offset = new float*[num_cluster]; //初始化偏移矩阵
    // for (int j = 0; j < num_cluster; j++) offset[j] = new float[out_dim]();
}

void SpacePartition::dfs(QuadTree *root, float *vis) {
    num_cluster = 1;
    int knn_k = 100;
    double perc = .0;
    int cell_count = 1;
    int knn_index_count; // the distribution of knn
//    centers_temp.reserve(root->num_leaf);

    stack<QuadTree *> qt_stack;
    qt_stack.push(root);
    QuadTree *qt;
    while (!qt_stack.empty()) {
        qt = qt_stack.top();
        if (qt->is_leaf) {
            if (fcflag) {
                if (qt->size > 5) {
                    pairs = new vector<int*>();
                    int numedge = 0;
                    computeDistribution(qt, &cell_count, &knn_index_count, knn_k, &perc, pairs, &numedge);
//            centers[num_cluster] = qt->center_of_mass;

                    int *cluster = new int[qt->size];
                    for(int i = 0; i < qt->size; i++){
                        cluster[i] = i;
                    }
                    for(int i = 0; i < numedge; i++){
                        int index1 = (*pairs)[i][2];
                        int index2 = (*pairs)[i][3];
                        if(cluster[index1] != cluster[index2]){
                            int i_cluster = cluster[index1];
                            int j_cluster = cluster[index2];
                            for(int j = 0; j < qt->size; j++){
                                if(cluster[j] == j_cluster){
                                    cluster[j] = i_cluster;
                                }
                            }
                        }
                    }
                    for(int i = 0; i < qt->size; i++){
                         if(cluster[i] != -1){
                            int cur_cluster = cluster[i];
                            for(int j = 0; j < qt->size; j++){
                                if(cluster[j] == cur_cluster){
                                    membership[qt->index[j]] = num_cluster;
                                    cluster[j] = -1;
                                }
                            }
                            num_cluster++;
                        }
                    }


                   // if (numedge > 0) {

                   //     fc *fc_model = new fc();
                   //     (*fc_model).fastcommunity(&(*pairs)[0], numedge, &num_cluster, membership, vis);
                   //     delete fc_model;
                   // }
                   // for (int i = 0; i < numedge; i++) {
                   //     delete[] (*pairs)[i]; (*pairs)[i] = NULL;
                   // }
                   // vector<int*>().swap(*pairs);
                }
            } else {
                for (int i = 0; i < qt->size; i++) {
                    membership[qt->index[i]] = num_cluster;
                }
                num_cluster++;
            }
        }
        qt_stack.pop();
        for (auto item : qt->children) {
            qt_stack.push(item);
        }
    }
    perc /= cell_count;

    for (int i = 0; i < n_vertices; i++) {
        vector<int>().swap(knn_vec[i]);
    }
    delete[] knn_vec;
    knn_vec = NULL;

    cout << "cur num cluster:" << num_cluster << endl;
//    cout << "perc: " << perc << endl;
    for (int i = 0; i < n_vertices; i++) {
        if(membership[i] == 0) {
            membership[i] = num_cluster++;
//            float *point = new float[out_dim];
//            for (int j = 0; j < out_dim; j++) {
//                point[j] = vis[i * out_dim + j];
//            }
//            centers_temp.push_back(point);
        }
    }
    cout << "cur num cluster:" << num_cluster << endl;
//    centers = &centers_temp[0];
}

void SpacePartition::computeDistribution(QuadTree *qt, int *cell_count, int *knn_index_count, const int &knn_k, double *perc, vector<int*> *pairs, int *numedge) {
    // compute the distribution of knn

        (*cell_count)++;
        (*knn_index_count) = 0;

        for (int i = 0; i < qt->size; i++) {
            for (int j = i + 1; j < qt->size; j++) {
                bool isLinked = false;
                for (int k = 0; k < knn_vec[qt->index[i]].size() && k < knn_k; k++) {
                    if (knn_vec[qt->index[i]][k] == qt->index[j]){
                        (*knn_index_count)++;
                        isLinked = true;
                        break;
                    }

                }
                for (int k = 0; k < knn_vec[qt->index[j]].size() && k < knn_k; k++) {
                    if (knn_vec[qt->index[j]][k] == qt->index[i]){
                        (*knn_index_count)++;
                        isLinked = true;
                        break;
                    }

                }
                if (isLinked) {
                    int *arr = new int[4];
                    arr[0] = qt->index[i], arr[1] = qt->index[j];
                    arr[2] = i, arr[3] = j;
                    (*pairs).push_back(arr);
                    (*numedge)++;
                }
            }
        }
        *perc += (*knn_index_count) * 1.0 / (qt->size * (qt->size - 1));

}

SpacePartition::~SpacePartition() {
    delete[] membership; membership = NULL;
//    delete[] centers; centers = NULL;
    delete[] offset; offset = NULL;
//    for (vector<float*>::iterator it = centers_temp.begin(); it != centers_temp.end(); it++)
//        if (NULL != *it)
//        {
//            delete[] it;
//            it = NULL;
//        }
//    vector<float*>().swap(centers_temp);
//    for (int i = 0; i < num_cluster; i++) {
//        vector<int>().swap(clusters[i]);
//    }
//    delete[] clusters; clusters = NULL;


}


#endif //SPACEPARTITION

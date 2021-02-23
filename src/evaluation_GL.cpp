//
// Created by Suya Basa on 2020/3/14.
//
#ifndef VIS_EVALUATION_GL_CPP
#define VIS_EVALUATION_GL_CPP

#include "evaluation_GL.h"
#include "genrandom.h"
#include <queue>

#define SAMPLE_MAX (1e10) //1e10->500s
#define SAMPLE_RATIO 100



namespace GL {
    evaluation::~evaluation() {
        // d.free_vec();
    }

    evaluation::evaluation(Data *gdata)
    {
        ned = gdata;
        N = ned->n_vertices;
    }

    void evaluation::load_data(char *visfile)
    {
        vis =  d.load_vec(visfile, n_vertices, out_dim);
    }

    bool operator < (const Dis &d1, const Dis &d2){
        return d1.distance < d2.distance;
    };

    vector<int> evaluation::vectors_intersection(vector<int> v1,vector<int> v2){
        vector<int> v;
        sort(v1.begin(),v1.end());
        sort(v2.begin(),v2.end());
        set_intersection(v1.begin(),v1.end(),v2.begin(),v2.end(),back_inserter(v));
        return v;
    }

    vector<int> evaluation::vectors_union(vector<int> v1,vector<int> v2){
        vector<int> v;
        sort(v1.begin(),v1.end());
        sort(v2.begin(),v2.end());
        set_union(v1.begin(),v1.end(),v2.begin(),v2.end(),back_inserter(v));
        return v;
    }

    float evaluation::CalcDist2D(int x, int y) {
        float ret = 0;
        int i, lx = x * out_dim, ly = y * out_dim;
        for (i = 0; i < out_dim; i++) {
            ret += (vis[lx + i] - vis[ly + i]) * (vis[lx + i] - vis[ly + i]);
        }
        return ret;
    }

    void *evaluation::accuracy_thread_caller(void* arg){
        evaluation* ptr = (evaluation*)(((arg_evaluation*)arg)->ptr);
        ptr->accuracy_thread(((arg_evaluation*)arg)->id);
        pthread_exit(NULL);
    }

    void evaluation::accuracy_thread(int id){
        genrandom r;
        r.init_gsl();
        for (int i = id; i < N; i = i + n_threads)
        {
            if (r.gslRandom() > SAMPLE_MAX / (1.0 * N * N)){
                sum[i] = -1;
                continue;
            }

            int k_neighbors = 0;
            float temp = 0, dis_t = 0;

            std::vector<Dis> distance;

            vector<int> Ng;
            vector<int> Ny;

            vector<int> v_inter,v_uion;

            //Ng Graph
            int p, q;
            for (p = ned->head[i]; p >= 0; p = ned->graph[p].next)
            {
                q = ned->graph[p].to;
                Ng.push_back(q);
            }

            k_neighbors = (int)Ng.size();

            for(int j = 0; j < N; j++)
            {
                if(i != j)
                {
                    temp = CalcDist2D(i, j);
                    if(distance.size() < k_neighbors)
                    {
                        distance.push_back(Dis(j,temp));
                        if(temp > dis_t)
                        {
                            dis_t = temp; 
                        }
                        if(distance.size() == k_neighbors) {
                            sort(distance.begin(), distance.end());
                        }
                        if(distance.size() == k_neighbors) {
                            sort(distance.begin(), distance.end());
                        }
                    }
                    else
                    {
                        if(temp >= dis_t)
                        {
                            continue;
                        }
                        else
                        {
                            distance.push_back(Dis(j,temp));
                            for (int k = distance.size() - 2; k >= 0; k--){
                                if (distance[k + 1] < distance[k]){
                                    Dis t = distance[k + 1];
                                    distance[k + 1] = distance[k];
                                    distance[k] = t;
                                } else{
                                    break;
                                }
                            }
                            // distance.erase(distance.end());
                            distance.pop_back();
                            dis_t = distance[k_neighbors - 1].distance;
                        }
                    }
                }
                else
                    continue;
            }

            for(int t = 0; t < k_neighbors; ++t)
            {
                Ny.push_back(distance[t].index);
            }

            v_uion = vectors_union(Ng,Ny);
            v_inter = vectors_intersection(Ng,Ny);

            if (v_uion.size() != 0){
                sum[i] = v_inter.size() * 1.0 / v_uion.size();
            }
            else {
                sum[i] = -1;
            }
        }
    }

    float evaluation::jaccard()
    {
        sum = new float[N];
        //pthread ...
        pthread_t *pt = new pthread_t[n_threads];
        for (int j = 0; j < n_threads; ++j){
            pthread_create(&pt[j], NULL, evaluation::accuracy_thread_caller, new arg_evaluation(this, j));
        }
        for (int j = 0; j < n_threads; ++j){
            pthread_join(pt[j], NULL);
        }
        delete[] pt;

        int count = 0;
        double sum_all = 0.0;
        for (int i = 0; i < N; ++i)
        {
            if (sum[i] >= -1e-10){
                sum_all += sum[i];
                count++;
            }
        }
        average = sum_all / count;
        // cout << average << endl;

        delete []sum;
        return average;
    }

    void *evaluation::stress_neighbor_alpha_thread_caller(void* arg){
        evaluation* ptr = (evaluation*)(((arg_evaluation*)arg)->ptr);
        ptr->stress_neighbor_alpha_thread(((arg_evaluation*)arg)->id);
        pthread_exit(NULL);
    }

    void evaluation::stress_neighbor_alpha_thread(int id){
        for (int p = id; p < ned->n_edges / 2; p = p + n_threads) {
            float dis = sqrt(CalcDist2D(ned->graph[p * 2].from, ned->graph[p * 2].to));
            //cout << dis << dis * dis << endl;
            l[id] += dis;
            l2[id] += dis * dis;
        }
    }

    void *evaluation::stress_neighbor_thread_caller(void* arg){
        evaluation* ptr = (evaluation*)(((arg_evaluation*)arg)->ptr);
        ptr->stress_neighbor_thread(((arg_evaluation*)arg)->id);
        pthread_exit(NULL);
    }

    void evaluation::stress_neighbor_thread(int id){
        l[id] = 0;
        for (int p = id; p < ned->n_edges / 2; p = p + n_threads) {
            float dis = sqrt(CalcDist2D(ned->graph[p * 2].from, ned->graph[p * 2].to));
            //cout << alpha * dis - 1 << endl;
            l[id] += pow(alpha * dis - 1, 2);
        }
    }

    float evaluation::stress_neighbor()
    {
        // 每一个线程计算一个l l2的和
        l = new float[n_threads]();
        l2 = new float[n_threads]();

        // 用多线程计算每条边的距离
        pthread_t *pt = new pthread_t[n_threads];
        for (int j = 0; j < n_threads; ++j){
            pthread_create(&pt[j], NULL, evaluation::stress_neighbor_alpha_thread_caller, new arg_evaluation(this, j));
        }
        for (int j = 0; j < n_threads; ++j){
            pthread_join(pt[j], NULL);
        }
        delete[] pt;

        // 计算所有边的距离 距离平方之和
        float sum_l = 0;
        float sum_l2 = 0;
        for (int i = 0; i < n_threads; ++i)
        {
            sum_l += l[i];
            sum_l2 += l2[i];
        }
        alpha = sum_l / sum_l2;
        delete[] l2; l2 = NULL;

        pt = new pthread_t[n_threads];
        for (int j = 0; j < n_threads; ++j){
            pthread_create(&pt[j], NULL, evaluation::stress_neighbor_thread_caller, new arg_evaluation(this, j));
        }
        for (int j = 0; j < n_threads; ++j){
            pthread_join(pt[j], NULL);
        }
        delete[] pt;

        float sum = 0;
        for (int i = 0; i < n_threads; ++i)
        {
            sum += l[i];
        }
        delete[] l; l = NULL;

        // cout <<  << endl;
        return sum / (ned->n_edges / 2);
    }

    void *evaluation::stress_alpha_thread_caller(void* arg) {
        evaluation* ptr = (evaluation*)(((arg_evaluation*)arg)->ptr);
        ptr->stress_alpha_thread(((arg_evaluation*)arg)->id);
        pthread_exit(NULL);
    }

    void evaluation::stress_alpha_thread(int id) {
        int* dist = new int[N]();
        genrandom r;
        r.init_gsl();
        for (int p = id; p < N; p = p + n_threads) {
            if (r.gslRandom() > SAMPLE_MAX / (1.0 * N * (N + ned->n_edges))){
                choose[p] = 0;
                continue;
            }
            choose[p] = 1;
            count[id]++;
            for (int i = 0; i < N; i++) {
                dist[i] = INF;
            }
            dist[p] = 0;

            queue<int> Q;
            Q.push(p);

            while (!Q.empty()) {
                int u = Q.front();
                Q.pop();
                if (u != p) { // not itself
                    float dis_original = dist[u];
                    float dis_layout = sqrt(CalcDist2D(u, p));
                    float temp = dis_layout / dis_original;
                    l[id] += temp;
                    l2[id] += temp * temp;
                }

                int edge_id = ned->head[u];
                while (edge_id != -1) {
                    int v = ned->graph[edge_id].to;
                    if (dist[v] == INF) {
                        dist[v] = dist[u] + 1;
                        Q.push(v);
                    }
                    edge_id = ned->graph[edge_id].next;
                }
            }
        }
        delete[] dist;
    }

    void *evaluation::stress_thread_caller(void* arg) {
        evaluation* ptr = (evaluation*)(((arg_evaluation*)arg)->ptr);
        ptr->stress_thread(((arg_evaluation*)arg)->id);
        pthread_exit(NULL);
    }

    void evaluation::stress_thread(int id) {
        l[id] = 0;
        int* dist = new int[N]();
        for (int p = id; p < N; p = p + n_threads) {
            if (choose[p] == 0){
                continue;
            }
            for (int i = 0; i < N; i++) {
                dist[i] = INF;
            }
            dist[p] = 0;

            queue<int> Q;
            Q.push(p);

            while (!Q.empty()) {
                int u = Q.front();
                Q.pop();
                if (u != p) { // not itself
                    float dis_original = dist[u];
                    float dis_layout = sqrt(CalcDist2D(u, p));
                    float temp = (alpha * dis_layout - dis_original) / dis_original;
                    l[id] += temp * temp;
                }

                int edge_id = ned->head[u];
                while (edge_id != -1) {
                    int v = ned->graph[edge_id].to;
                    if (dist[v] == INF) {
                        dist[v] = dist[u] + 1;
                        Q.push(v);
                    }
                    edge_id = ned->graph[edge_id].next;
                }
            }
        }
        delete[] dist;
    }

    float evaluation::stress()
    {
        //cout << SAMPLE_MAX / (1.0 * N * (N + ned->n_edge)) << endl;
        // 每一个线程计算一个l l2的和
        l = new float[n_threads]();
        l2 = new float[n_threads]();
        count = new int[n_threads]();
        choose = new int[N]();

        // 用多线程计算每条边的距离
        pthread_t *pt = new pthread_t[n_threads];
        for (int j = 0; j < n_threads; ++j) {
            pthread_create(&pt[j], NULL, evaluation::stress_alpha_thread_caller, new arg_evaluation(this, j));
        }
        for (int j = 0; j < n_threads; ++j) {
            pthread_join(pt[j], NULL);
        }
        delete[] pt;

        // 计算所有边的距离 距离平方之和
        float sum_l = 0;
        float sum_l2 = 0;
        for (int i = 0; i < n_threads; ++i)
        {
            sum_l += l[i];
            sum_l2 += l2[i];
        }
        alpha = sum_l / sum_l2;
        delete[] l2; l2 = NULL;

        pt = new pthread_t[n_threads];
        for (int j = 0; j < n_threads; ++j) {
            pthread_create(&pt[j], NULL, evaluation::stress_thread_caller, new arg_evaluation(this, j));
        }
        for (int j = 0; j < n_threads; ++j) {
            pthread_join(pt[j], NULL);
        }
        delete[] pt;

        float sum = 0;
        int sum_count = 0;
        for (int i = 0; i < n_threads; ++i)
        {
            sum += l[i];
            sum_count += count[i];
        }
        delete[] l; l = NULL;
        delete[] choose;

        // cout <<  << endl;
        return sum / sum_count / N;
    }











    void *evaluation::layout_proximity_thread_caller(void* arg){
        evaluation* ptr = (evaluation*)(((arg_evaluation*)arg)->ptr);
        ptr->layout_proximity_thread(((arg_evaluation*)arg)->id);
        pthread_exit(NULL);
    }

    void evaluation::layout_proximity_thread(int id){
        for (int p = id; p < ned->n_edges; p = p + n_threads) {
            float dis2 = CalcDist2D(ned->graph[p].from, ned->graph[p].to);
            l_dis[p] = 1.0 / (1 + dis2);
            th_sum[id] += l_dis[p];
        }
    }


    void *evaluation::loss_thread_caller(void* arg){
        evaluation* ptr = (evaluation*)(((arg_evaluation*)arg)->ptr);
        ptr->loss_thread(((arg_evaluation*)arg)->id);
        pthread_exit(NULL);
    }

    void evaluation::loss_thread(int id){
        for (int p = id; p < ned->n_edges; p = p + n_threads) {
            // 由于在compute_similarity函数中计算的ns并没有除点的总数，这里要先除以v
            float ns = ned->graph[p].weight / ned->n_vertices;
            float lp = l_dis[p] / sum_ld;
            float edge_loss = ns * log(ns / lp);
            th_sum_loss[id] += edge_loss;
        }
    }

/*
 * 计算图距离与投影距离的误差
 * ns * log(ns/lp)
 */
    float evaluation::loss()
    {
        n_threads = 1;
        l_dis = new float[ned->n_edges];
        th_sum = new float[n_threads]();

        // 用多线程计算每条边的（1 + LD^2）^-1
        pthread_t *pt = new pthread_t[n_threads];
        for (int j = 0; j < n_threads; ++j){
            pthread_create(&pt[j], NULL, evaluation::layout_proximity_thread_caller, new arg_evaluation(this, j));
        }
        for (int j = 0; j < n_threads; ++j){
            pthread_join(pt[j], NULL);
        }
        delete[] pt;

        for (int i = 0; i < n_threads; ++i)
        {
            sum_ld += th_sum[i];
        }
        delete[] th_sum; th_sum = NULL;

        // 每条边的ns与lp都计算好了，接下来计算误差
        th_sum_loss = new float[n_threads]();
        pt = new pthread_t[n_threads];
        for (int j = 0; j < n_threads; ++j){
            pthread_create(&pt[j], NULL, evaluation::loss_thread_caller, new arg_evaluation(this, j));
        }
        for (int j = 0; j < n_threads; ++j){
            pthread_join(pt[j], NULL);
        }
        delete[] pt;

        // 计算所有边的距离 距离平方之和
        float sum_loss = 0;
        for (int i = 0; i < n_threads; ++i)
        {
            sum_loss += th_sum_loss[i];
        }
        delete[] l_dis; l_dis = NULL;
        delete[] th_sum_loss; th_sum_loss = NULL;


        cout << "loss:" << sum_loss << endl;
        return sum_loss;
    }



}


#endif //VIS_EVALUATION_GL_CPP

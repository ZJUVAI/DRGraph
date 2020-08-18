#ifndef VISUALIZE_CPP
#define VISUALIZE_CPP

#include "visualizemod.h"

#ifdef USE_CUDA
#include <cuda.h>
#include <curand.h>
#include <curand_kernel.h>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <thrust/for_each.h>
#include <thrust/iterator/zip_iterator.h>
#include <thrust/random.h>
#include <thrust/transform.h>
#define BLOCK_NUM 32
#define THREADS_NUM 128
#define CUDA_MIN_SAMPLE 256000 //must be larger than 64 * (1000) or there must be 2 warps write to the same global memory
#define CUDA_MAX_SAMPLE (BLOCK_NUM * THREADS_NUM * 20)
#endif

visualizemod::visualizemod(){};

visualizemod::~visualizemod() {
    // delete []alias_vertice;
    // delete []prob_vertice;
    delete []alias_edge;
    delete []prob_edge;
    // delete []offset;
    delete[] vis;
    delete[] neg_table;
}

const gsl_rng_type *visualizemod::gsl_T = nullptr;
gsl_rng *visualizemod::gsl_r = nullptr;


void visualizemod::run(Data *data, int n_thre, int n_samp , float alph, int n_nega,
                       float gamm, vector<Multilevel*>& mls, int multiMode, float firstGamma, pair<float, float> proSet)
{
    
    
    gsl_rng_env_setup();
    gsl_T = gsl_rng_rand48;
    gsl_r = gsl_rng_alloc(gsl_T);
    gsl_rng_set(gsl_r, 314159265);

    grad_clip = 1.0;

    use_Mode = proSet;

    
    initial_alpha = alph;
    n_threads = n_thre;
    int samp_ratio = n_samp;
    n_negatives = n_nega;
    float t_gamma = gamm;

    
    graphdata = data;
    int mlsSize = mls.size() - 1;


    random_vis(mls[mlsSize]);
    
    // if(multiMode == 0) {
    //     for(int i = 0; i < mls.size(); ++i) {
    //         delete[] (*mls[i]).head;
    //         vector<Link>().swap((*mls[i]).graph);
    //     }
    // }
    
    

    printf("gamma: %.3f alpha: %.3f nsamples: %d neg: %d\n", t_gamma, alph, samp_ratio, n_negatives);

    alias_edge = new int[graphdata->n_edges];
    prob_edge = new float[graphdata->n_edges];

    init_edge_table();
    // init_vertice_table();
    init_neg_table();

    // 开始不同层次的visualize
    
    


    for (int i = mlsSize; i >= 0; i--) {
        if(i == 0) n_samples =  1ll * (*mls[i]).num_cluster * samp_ratio;
        else if ((*mls[i]).num_cluster < 500)
            n_samples = 1ll * (*mls[i]).num_cluster * 1000;
        else n_samples = 1ll * (*mls[i]).num_cluster * 500;

        // float gamma = firstGamma + (mlsSize - i) * 1.0 / mlsSize * (t_gamma - firstGamma);
        float gamma = i == 0 ? t_gamma : firstGamma;
        std::printf("round %d gamma: %.3f samples: %lld\n", i, gamma, n_samples);
        // save_intermediate_result(mls.size() - 1 - i);
#ifdef USE_CUDA
        visualize_GPU(mls[i], n_samples, 1, gamma, i != 0);
#else
        visualize(mls[i], n_samples, 1, gamma, i != 0);
#endif

        if(i != 0) {
            assert(ml->memSize != -1);
            // printf("hhhh %d\n", ml->memSize);
            for(int l = 0; l < ml->memSize; ++l) { vis_swap[l * 2] = 0; vis_swap[l * 2 + 1] = 0; }
            if(multiMode == 0) {
                
                // for(int l = 0; l < ml->memSize; ++l) {
                //     if(ml->memIter[l] < 0) {
                //         int tmp = - (ml->memIter[l]) - 1;
                //         vis_swap[l * 2] += vis[tmp * 2];
                //         vis_swap[l * 2 + 1] += vis[tmp * 2 + 1];
                //         // setSPoint(l, SPoint(vis[tmp * 2], vis[tmp * 2 + 1]));
                //         for(int j = mls[i-1]->head[l]; ~j; j = mls[i-1]->graph[j].next) {
                //             int w = mls[i-1]->graph[j].to;
                //             // if(ml->memIter[w] >= 0) {
                //                 vis_swap[w * 2] += vis[tmp * 2] / mls[i-1]->du[w];
                //                 vis_swap[w * 2 + 1] += vis[tmp * 2 + 1] / mls[i-1]->du[w];
                //             // } 
                //             // else printf("hhhhhhh\n");
                //         }

                //     }
                // }

                vector<int> points;
                for(int l = 0; l < ml->memSize; ++l) {
                    if(ml->memIter[l] < 0) {
                        int tmp = - (ml->memIter[l]) - 1;
                        vis_swap[l * 2] += vis[tmp * 2];
                        vis_swap[l * 2 + 1] += vis[tmp * 2 + 1];
                    } else points.push_back(l);
                }
                for(int l : points) {
                    SPoint tt;
                    int cnt = 0;

                    for(int j = mls[i-1]->head[l]; ~j; j = mls[i-1]->graph[j].next) {
                        int w = mls[i-1]->graph[j].to;
                        if(ml->memIter[w] < 0) {
                            // printf("want point: %d\n", w);
                            int tmp = -(ml->memIter[w]) - 1;
                            tt = tt + SPoint(vis[tmp * 2], vis[tmp * 2 + 1]);
                            cnt ++;
                        }
                    }

                    // printf("get poiint: %.3f %.3f %d\n", tt.x, tt.y, cnt);
                    assert(cnt > 0);

                    tt = tt / cnt;
                    setSPoint(l, tt);
                }
  
            } else {
                makeNewVis(mls[i-1]);
            }
            swap(vis_swap, vis);
        }
    }
}

void visualizemod::makeNewVis(Multilevel* pre) {    // ml: level - 1
    timer t;
    t.start();
    vector<int> pm_nodes;
    vector<int> planetOrMoon;
    for(int i = 0; i < ml->memSize; ++i) {
        if(ml->nodeAttribute[i].nodeType == 1) {
            setSPoint(i, SPoint(vis[ml->nodeAttribute[i].mem * 2], vis[ml->nodeAttribute[i].mem * 2 + 1]));
            ml->nodeAttribute[i].isPlace = true;
        } else if(ml->nodeAttribute[i].nodeType == 3) {
            pm_nodes.push_back(i);
        } else {
            planetOrMoon.push_back(i);
        }
    }

    printf("planetOrMoon %d pm_nodes %d\n", (int)planetOrMoon.size(), (int)pm_nodes.size());

    for(int i = 0, len = planetOrMoon.size(); i < len; ++i) {
        vector<SPoint> SPointList;
        int v = planetOrMoon[i];
        SPoint sun_pos = getSPoint(ml->nodeAttribute[v].sun);
        // printf("choose %d %.3f %.3f lambda: %d\n", v, sun_pos.x, sun_pos.y, (int)(ml->nodeAttribute[v].lamba_list.size()));
        float sun_dist = ml->nodeAttribute[v].distToSun;
        for(int j = pre->head[v]; ~j; j = pre->graph[j].next) {
            int w = pre->graph[j].to;
            // printf("xxx: %d %d %d %d\n", w, ml->n_vertices, ml->num_cluster, ml->memSize);
            if( ml->nodeAttribute[w].nodeType != 1 && ml->nodeAttribute[w].isPlace == true
                && ml->nodeAttribute[v].sun == ml->nodeAttribute[w].sun
            ) {

                // printf("neighbor %d\n", w);
                SPoint adj_pos = getSPoint(w);
                // printf("then %.3f %.3f %.3f %.3f %.3f\n", sun_pos.x, sun_pos.y, adj_pos.x, adj_pos.y, pre->graph[j].weight);
                SPointList.push_back(calculate_position(sun_pos, adj_pos, sun_dist, pre->graph[j].weight));
            }
        }

        if(ml->nodeAttribute[v].lamba_list.empty()) {
            if(SPointList.empty()) {
                SPoint new_pos = create_random_pos(sun_pos, sun_dist, 0, 2 * mathPi);
                SPointList.push_back(new_pos);
            }
        } else {
            for(int j = 0, len = ml->nodeAttribute[v].lamba_list.size(); j < len; ++j) {
                float lambda = ml->nodeAttribute[v].lamba_list[j];
                SPoint adj_sun_pos = getSPoint(ml->nodeAttribute[v].neighborSunNode[j]);
                SPoint newPos = get_waggled_inbetween_position(sun_pos, adj_sun_pos, lambda);
                SPointList.push_back(newPos);
            }
        }
        float tmpX = 0; float tmpY = 0;
        for(int j = 0, len = SPointList.size(); j < len; ++j) {
            tmpX += SPointList[j].x; tmpY += SPointList[j].y;
        }

        // printf("SPointList "); for(int j = 0, len = SPointList.size(); j < len; ++j) printf("%.3f:%.3f ", SPointList[j].x, SPointList[j].y); printf("\n");

        tmpX /= (int)SPointList.size();
        tmpY /= (int)SPointList.size();
        // assert((int)SPointList.size() > 0);
        // printf("%.3f %.3f\n", tmpX, tmpY);
        setSPoint(v, SPoint(tmpX, tmpY));

        ml->nodeAttribute[v].isPlace = true;
    }

    // printf("compelte planet and moon\n");
    for(int i = 0, len = pm_nodes.size(); i < len; ++i) {
        vector<SPoint> SPointList;
        int v = pm_nodes[i];
        SPoint sun_pos = getSPoint(ml->nodeAttribute[v].sun);
        float sun_dist = ml->nodeAttribute[v].distToSun;
        for(int j = pre->head[v]; ~j; j = pre->graph[j].next) {
            int w = pre->graph[j].to;
            if( ml->nodeAttribute[w].nodeType != 1 && ml->nodeAttribute[w].isPlace == true && pre->graph[j].isMoon == true
                && ml->nodeAttribute[v].sun == ml->nodeAttribute[w].sun
            ) {

                SPoint adj_pos = getSPoint(w);
                SPointList.push_back(calculate_position(sun_pos, adj_pos, sun_dist, pre->graph[j].weight));
            }
        }

        for(int j = 0, len = ml->nodeAttribute[v].moon_list.size(); j < len; ++j) {
            int moon_node = ml->nodeAttribute[v].moon_list[j];
            SPoint moon_pos = getSPoint(moon_node);
            float moon_dist = ml->nodeAttribute[moon_node].distToSun;
            SPointList.push_back(get_waggled_inbetween_position(sun_pos, moon_pos, sun_dist / moon_dist));
        }

        if(!ml->nodeAttribute[v].lamba_list.empty()) {
            for(int j = 0, len = ml->nodeAttribute[v].lamba_list.size(); j < len; ++j) {
                float lambda = ml->nodeAttribute[v].lamba_list[j];
                SPoint adj_sun_pos = getSPoint(ml->nodeAttribute[v].neighborSunNode[j]);
                SPoint newPos = get_waggled_inbetween_position(sun_pos, adj_sun_pos, lambda);
                SPointList.push_back(newPos);
            }
        }
        float tmpX = 0; float tmpY = 0;
        for(int j = 0, len = SPointList.size(); j < len; ++j) {
            tmpX += SPointList[j].x; tmpY += SPointList[j].y;
        }
        // assert((int)SPointList.size() > 0);
        tmpX /= (int)SPointList.size();
        tmpY /= (int)SPointList.size();

        setSPoint(v, SPoint(tmpX, tmpY));
        ml->nodeAttribute[v].isPlace = true;
    }
    t.end();
    // printf("[CHANGE] CPU Time: %.3f s Real Time: %.3f s\n", t.cpu_time(), t.real_time());
    // delete[] dedicateSun;

    // for(int i = 0; i < ml->memSize; ++i) printf("%.3f:%.3f ", vis_swap[i * 2], vis_swap[i * 2 + 1]);
}

inline SPoint visualizemod::getSPoint(int x) {
    // assert(vis_swap[x * 2] < 1e5 &&  vis_swap[x * 2 + 1] < 1e5);
    return SPoint(vis_swap[x * 2], vis_swap[x * 2 + 1]);
}

inline void visualizemod::setSPoint(int x, SPoint Y) {
    assert(Y.x < 1e5 && Y.y < 1e5 && Y.x > -1e5 && Y.y > -1e5);
    // printf("set[%d] %.3f %.3f\n", x, Y.x, Y.y);
    vis_swap[x * 2] = Y.x;
    vis_swap[x * 2 + 1] = Y.y;
}

void visualizemod::init_neg_table() {
    long long x, p, i;
    long long need_size = (long long)graphdata->n_vertices * 100;
    neg_size = need_size < 1e8 ? need_size : 1e8;
    float sum_weights = 0, dd, *weights = new float[graphdata->n_vertices];
    for (i = 0; i < graphdata->n_vertices; ++i) weights[i] = 0;
    for (x = 0; x < graphdata->n_vertices; ++x)
    {
        for (p = graphdata->head[x]; p >= 0; p = graphdata->graph[p].next)
        {
            weights[x] += graphdata->graph[p].weight;
        }
        //  weights[x] = 1.0;
        sum_weights += weights[x] = pow(weights[x], 0.75);
    }
    

    // for(i = 0; i < graphdata->n_vertices; ++i) printf("%.3f ", weights[i]); printf("\n");
    // printf("minnn: %.3f maxxx: %.3f\n",*std::min_element(weights, weights + graphdata->n_vertices), *std::max_element(weights, weights + graphdata->n_vertices));


    neg_table = new int[neg_size];
    dd = weights[0];
    for (i = x = 0; i < neg_size; ++i)
    {
        neg_table[i] = x;
        if (i / (float)neg_size > dd / sum_weights && x < graphdata->n_vertices - 1)
        {
            dd += weights[++x];
        }
    }
    // printf("sum weight: %.3f\n", sum_weights);
    delete[] weights;
}

inline SPoint visualizemod::calculate_position(SPoint P, SPoint Q, float dist_P, float dist_Q)
{
	float dist_PQ = (P-Q).norm();
	float lambda = (dist_P + (dist_PQ - dist_P - dist_Q)/2)/dist_PQ;
	return get_waggled_inbetween_position(P,Q,lambda);
}

inline SPoint visualizemod::get_waggled_inbetween_position(SPoint s, SPoint t, float lambda)
{
	const float WAGGLEFACTOR = 0.05;
	// const int BILLION = 1000000000;
	SPoint inbetween_SPoint;
	inbetween_SPoint.x = s.x + lambda*(t.x - s.x);
	inbetween_SPoint.y = s.y + lambda*(t.y - s.y);
	float radius = WAGGLEFACTOR * (t-s).norm();
	float rnd = gsl_rng_uniform(gsl_r);
	float rand_radius =  radius * rnd;
	return create_random_pos(inbetween_SPoint,rand_radius,0,2.0*mathPi);
}

inline SPoint visualizemod::create_random_pos(SPoint center,float radius,float angle_1,
	float angle_2)
{
	// const int BILLION = 1000000000;
	SPoint new_SPoint;
	float rnd = gsl_rng_uniform(gsl_r);
	float rnd_angle = angle_1 +(angle_2-angle_1)*rnd;
	float dx = cos(rnd_angle) * radius;
	float dy = sin(rnd_angle) * radius;
	new_SPoint.x = center.x + dx ;
	new_SPoint.y = center.y + dy;
	return new_SPoint;
}


void visualizemod::random_vis(Multilevel *ml) {
    // printf("hhhh %d %d\n",graphdata->n_vertices, 2);
    int i;
    vis_swap = new float[graphdata->n_vertices * 2];
    vis = new float[graphdata->n_vertices * 2];
    // 按点所在团分配节点位置，一个团的节点在同一位置
    for (i = 0; i < ml->num_cluster; ++i) {
        vis[i*2] = (float)((gsl_rng_uniform(gsl_r) - 0.5) * 0.0001);
        vis[i*2 + 1] = (float)((gsl_rng_uniform(gsl_r) - 0.5) * 0.0001);
    }

}


#ifdef USE_CUDA

__global__ static void Kernel_visualize
    (float* randomList, int* alias_edge, float* prob_edge, int n_edge, int* neg_table, int neg_size/*int* alias_vertice, float* prob_vertice*/, int n_vertice, int* edge_from,
        int* edge_to, int* membership, float* vis, float grad_clip, float gamma,
        float cur_alpha_init, int sample_size, int data_size, bool ml_method, int NEG_NUM, 
        float a, float b)
{
    const size_t tID = size_t(threadIdx.x);// 线程
    const size_t bID = size_t(blockIdx.x);

    int k, p, x, y, mx, i, j, my;//, my_old;
    float cur[2], err[2], samp[2], f, g, gg, r1, r2;
    float n_edge_f = n_edge - 0.1;
    // float n_vertice_f = n_vertice - 0.1;
    float cur_alpha = cur_alpha_init;
    float neg_size_f = neg_size - 0.1;
    int randList[105];

    for ( size_t idx = bID * THREADS_NUM + tID;
        idx < data_size;
        idx += BLOCK_NUM * THREADS_NUM )
    {
        r1 = randomList[idx];
        r2 = randomList[idx + data_size];
        k = (int)(r1 * n_edge_f);
        p = r2 <= prob_edge[k] ? k : alias_edge[k];
        x = edge_from[p]; y = edge_to[p];

        if( (ml_method ? membership[y] : y) == (ml_method ? membership[x] : x) ) {
            continue;
        }

        mx =  ml_method ? membership[x] : x;
        
        #pragma unroll
        for (i = 0; i < 2; ++i) {
            cur[i] = vis[mx * 2 + i];
            err[i] = 0;                                    
        }

        for(i = 0; i < NEG_NUM; ++i) {
            int tmp = neg_table[(int)(randomList[ (idx * NEG_NUM + data_size * 2 + i)] * neg_size_f)];
            if (membership[tmp] == (ml_method ? membership[y] : y) ) { tmp = -1; }
            if (membership[tmp] == (ml_method ? membership[x] : x) ) { tmp = -1; }
            randList[i] = tmp;
        }

        #pragma unroll
        for (i = 0; i < NEG_NUM + 1; ++i)  //negative sampling
        {
            y = i == 0 ? y : randList[i - 1];
            if(y == -1) continue;
            my = ml_method? membership[y] : y;

            #pragma unroll
            for (j = 0, f = 0; j < 2; ++j) {
                samp[j] = (cur[j] - vis[my * 2 + j]);
                f += samp[j] * samp[j];
            }
            
            if(a > 0) {
                if (i == 0) g = -2 * a * b * pow(f, b - 1.0) / (1 + a * pow(f, b));
                else g = 2 * gamma * b / (1 + a * pow(f, b)) / (0.001 + f);
            } else {
                if (i == 0) g = -2 / (1 + f);
                else g = 2 * gamma / (1 + f) / ( (i == 0 ? 0.000001 : 0.1) + f);
            }
            
            #pragma unroll
            for (j = 0; j < 2; ++j)
            {
                gg = max(-grad_clip, min(grad_clip, g * samp[j])) * cur_alpha;
                err[j] += gg;
                atomicAdd(&vis[my * 2 + j], -gg);
            }
        }

        #pragma unroll
        for (j = 0; j < 2; ++j) {
            atomicAdd(&vis[mx * 2 + j], err[j]);
        }
    }
}

void visualizemod::visualize_GPU(Multilevel *m, long long _samples, float _alpha, float _gamma, bool ml_method) {
    if (n_samples < CUDA_MIN_SAMPLE){ //use thread version
        visualize(m, _samples, _alpha,  _gamma, ml_method);
        return;
    }
    // printf("hhhh %d\n", ml_method);

    ml = m;
    n_samples = _samples;
    gamma = _gamma;
    ml_method = ml_method;

    //following are CUDA codes
    int n_iter = n_samples / CUDA_MAX_SAMPLE;
    if (n_iter < 5){
        n_iter = 5;
    }
    per_samples = n_samples / n_iter;
    size_t size1 = graphdata->n_vertices * sizeof(int);
    size_t size2 = graphdata->n_edges * sizeof(int);
    int randomList_num = (per_samples + 5) * (n_negatives + 2);
    size_t size3 = randomList_num * sizeof(int);
    size_t size4 = m->num_cluster * 2 * sizeof(int);
    int *from_host = (int *)malloc(size2);
    int *to_host = (int *)malloc(size2);
    int *membership_device = NULL;
    float *vis_device = NULL;
    int *from_device = NULL;
    int *to_device = NULL;
    int *neg_device = NULL;
    // float *prob_vertice_device = NULL;
    // int *alias_vertice_device = NULL;
    float *prob_edge_device = NULL;
    int *alias_edge_device = NULL;
    float *randomList_device = NULL;
    for(int i = 0; i < graphdata->n_edges; ++i) {
        from_host[i] = graphdata->graph[i].from;
        to_host[i] = graphdata->graph[i].to;
    }
    cudaMalloc((void **)&membership_device, size1);
    cudaMalloc((void **)&vis_device, size4);
    cudaMalloc((void **)&from_device, size2);
    cudaMalloc((void **)&to_device, size2);
    // cudaMalloc((void **)&prob_vertice_device, size1);
    // cudaMalloc((void **)&alias_vertice_device, size1);
    cudaMalloc((void **)&neg_device, neg_size * sizeof(int));
    cudaMalloc((void **)&prob_edge_device, size2);
    cudaMalloc((void **)&alias_edge_device, size2);
    cudaMalloc((void **)&randomList_device, size3);
    cudaMemcpyAsync(membership_device, ml->membership, size1, cudaMemcpyHostToDevice);
    cudaMemcpyAsync(vis_device, vis, size4, cudaMemcpyHostToDevice);
    // cudaMemcpyAsync(prob_vertice_device, prob_vertice, size1, cudaMemcpyHostToDevice);
    // cudaMemcpyAsync(alias_vertice_device, alias_vertice, size1, cudaMemcpyHostToDevice);
    cudaMemcpyAsync(neg_device, neg_table, neg_size * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpyAsync(prob_edge_device, prob_edge, size2, cudaMemcpyHostToDevice);
    cudaMemcpyAsync(alias_edge_device, alias_edge, size2, cudaMemcpyHostToDevice);
    cudaMemcpyAsync(from_device, from_host, size2, cudaMemcpyHostToDevice);
    cudaMemcpyAsync(to_device, to_host, size2, cudaMemcpyHostToDevice);

    curandGenerator_t gen;
    curandCreateGenerator(&gen, CURAND_RNG_PSEUDO_DEFAULT);
    curandSetPseudoRandomGeneratorSeed(gen, time(NULL));

    // thrust::device_vector<float> cur_alpha_(1);
    for (int iter = 0; iter < n_iter; iter++){
        float cur_alpha = initial_alpha * (1 - iter * per_samples / (n_samples + 1.0));
        if (cur_alpha < initial_alpha * 0.0001) cur_alpha = initial_alpha * 0.0001;

        curandGenerateUniform(gen, randomList_device, randomList_num);
        cudaError_t cudaStatus;
        cudaStatus = cudaSetDevice(0);

        //cout << cur_alpha_[0] << endl;

        Kernel_visualize<<<BLOCK_NUM, THREADS_NUM>>>(
            randomList_device,
            alias_edge_device,
            prob_edge_device,
            graphdata->n_edges,
            neg_device,
            neg_size,
            // alias_vertice_device,
            // prob_vertice_device,
            graphdata->n_vertices,
            from_device,
            to_device,
            membership_device,
            vis_device,
            grad_clip,
            gamma,
            cur_alpha,
            n_samples,
            per_samples,
            ml_method,
            n_negatives,
            use_Mode.first,
            use_Mode.second
        );
        cudaStatus = cudaDeviceSynchronize( );
        if (cudaStatus != cudaSuccess){
            std::cout << "Cuda Synch Fail!" << std::endl;
            // exit(0);
        }
    }
    //cout << endl;
    curandDestroyGenerator(gen);
    cudaMemcpyAsync(vis, vis_device, size4, cudaMemcpyDeviceToHost);
    cudaFree(membership_device);
    cudaFree(vis_device);
    cudaFree(from_device);
    cudaFree(to_device);
    cudaFree(neg_device);
    // cudaFree(prob_vertice_device);
    // cudaFree(alias_vertice_device);
    cudaFree(prob_edge_device);
    cudaFree(alias_edge_device);
    cudaFree(randomList_device);
    free(from_host);
    free(to_host);

}
#endif

void visualizemod::visualize(Multilevel *m, long long _samples, float _alpha, float _gamma, bool method) {
    // 初始化visualize所用的布局
    ml = m;
    n_samples = _samples;
    gamma = _gamma;
    ml_method = method;

    edge_count_actual = 0;

    int n_iter = 1;
    pthread_t *pt;
    per_samples = n_samples / n_iter;

    for (int iter = 0; iter < n_iter; iter++) {
        pt = new pthread_t[n_threads];
        edge_count_actual = iter * per_samples;
        for (int j = 0; j < n_threads; ++j) pthread_create(&pt[j], nullptr, visualizemod::visualize_thread_caller, new arg_struct(this, j, iter));
        for (int j = 0; j < n_threads; ++j) pthread_join(pt[j], nullptr);
        delete[] pt;
    }
}

void *visualizemod::visualize_thread_caller(void *arg)
{
    visualizemod *ptr = (visualizemod*)(((arg_struct*)arg)->ptr);
    ptr->visualize_thread(((arg_struct*)arg)->id, ((arg_struct*)arg)->times);
    pthread_exit(NULL);
}

void visualizemod::visualize_thread(int id, int iter)
{
    const float a = use_Mode.first;
    const float b = use_Mode.second;

    long long edge_count = 0, last_edge_count = 0;
    int x, y, p, i, j;
    float f, g, gg, cur_alpha = initial_alpha;
    float *cur = new float[2];
    float *err = new float[2];
    float *samp = new float[2];
    int randList[n_negatives];
    while (1)
    {
        // printf("%lld\n", edge_count);
        if (edge_count > per_samples / n_threads + 2) break;
        if (edge_count - last_edge_count > 100) // adjust alphac and output intermediate progress
        {
            edge_count_actual += edge_count - last_edge_count;
            last_edge_count = edge_count;
            cur_alpha = initial_alpha * (1 - edge_count_actual / (n_samples + 1.0));
            if (cur_alpha < initial_alpha * 0.0001) cur_alpha = initial_alpha * 0.0001;
        }
        p = sample_an_edge(gsl_rng_uniform(gsl_r), gsl_rng_uniform(gsl_r));
        // printf("choose edge %d in %d edges\n", p, graphdata->n_edges); fflush(stdout);
        x = graphdata->graph[p].from;
        y = graphdata->graph[p].to;
        if( (ml_method ? ml->membership[y] : y) == (ml_method ? ml->membership[x] : x) ) {
            ++edge_count;
            continue;
        }

        int mx, my;
        mx =  ml_method ? ml->membership[x] : x;
        for (i = 0; i < 2; ++i) {
            cur[i] = vis[mx * 2 + i];
            err[i] = 0;
        }
        
        for(i = 0; i < n_negatives; ++i) {
            int tmp = neg_table[(unsigned int)floor(gsl_rng_uniform(gsl_r) * (neg_size - 0.1))];
            if (ml->membership[tmp] == (ml_method ? ml->membership[graphdata->graph[p].to] : graphdata->graph[p].to) ){  tmp = -1; }
            if (ml->membership[tmp] == (ml_method ? ml->membership[x] : x) ) { tmp = -1;  }
            randList[i] = tmp;
        }
        for (i = 0; i < n_negatives + 1; ++i)
        {
            y = i == 0 ? y : randList[i - 1];
            if(y == -1) continue;
            my = ml_method ? ml->membership[y] : y;
            for (j = 0, f= 0; j < 2; ++j) {
                samp[j] = (cur[j] - vis[my * 2 + j]);
                f += samp[j] * samp[j];
            }
            
            if(a > 0) {
                if (i == 0) g = -2 * a * b * pow(f, b - 1.0) / (1 + a * pow(f, b));
                else g = 2 * gamma * b / (1 + a * pow(f, b)) / (0.001 + f);
            } else {
                if (i == 0) g = -2 / (1 + f);
                else g = 2 * gamma / (1 + f) / ( (i == 0 ? 0.000001 : 0.1) + f);
            }
            
            for (j = 0; j < 2; ++j)
            {
                gg = max(-grad_clip, min(grad_clip, g * samp[j])) * cur_alpha;
                err[j] += gg;
                vis[my * 2 + j] -= gg;
            }
        }
        for (int j = 0; j < 2; ++j) {
            vis[mx * 2 + j] += err[j];
        }
        ++edge_count;
    }
    delete[] cur;
    delete[] err;
    delete[] samp;
}
// #endif
// https://en.wikipedia.org/wiki/Alias_method
void visualizemod::init_alias_table(int n, float* weight, int* alias, float* prob) {


    auto *norm_prob = new float[n];
    auto *large_block = new int[n];
    auto *small_block = new int[n];

    float sum = 0;
    int cur_small_block, cur_large_block;
    int num_small_block = 0, num_large_block = 0;

    for (int k = 0; k < n; ++k) sum += weight[k];
    for (int k = 0; k < n; ++k) alias[k] = 0, norm_prob[k] = weight[k] * n / sum;

    for (int k = n - 1; k >= 0; --k) {
        if (norm_prob[k] < 1)
            small_block[num_small_block++] = k;
        else
            large_block[num_large_block++] = k;
    }

    while (num_small_block && num_large_block)
    {
        cur_small_block = small_block[--num_small_block];
        cur_large_block = large_block[--num_large_block];
        prob[cur_small_block] = norm_prob[cur_small_block];
        alias[cur_small_block] = cur_large_block;
        norm_prob[cur_large_block] = norm_prob[cur_large_block] + norm_prob[cur_small_block] - 1;
        if (norm_prob[cur_large_block] < 1)
            small_block[num_small_block++] = cur_large_block;
        else
            large_block[num_large_block++] = cur_large_block;
    }

    while (num_large_block) prob[large_block[--num_large_block]] = 1;
    while (num_small_block) prob[small_block[--num_small_block]] = 1;

    delete[] norm_prob;
    delete[] small_block;
    delete[] large_block;
}

void visualizemod::init_edge_table() {
    float *weight = new float[graphdata->n_edges];
    int i;
    for(i = 0; i < graphdata->n_edges; ++i) {
        weight[i] = graphdata->graph[i].weight;
        // weight[i] = 1.0;
    }

    // for(i = 0; i < graphdata->n_edges; ++i) printf("%.3f ", weight[i]); printf("\n");
    // printf("minn: %.3f maxx: %.3f\n",*std::min_element(weight, weight + graphdata->n_edges), *std::max_element(weight, weight + graphdata->n_edges));

    
    // alias_edge = new int[graphdata->n_edges];
    // prob_edge = new float[graphdata->n_edges];
    init_alias_table(graphdata->n_edges, weight, alias_edge, prob_edge);
    delete[] weight;
    // for(int i = 0; i < graphdata->n_edges; ++i) printf("%d %.3f : ", alias_edge[i], prob_edge[i]); printf("\n");
    // printf("edges %d\n\n", graphdata->n_edges);
}

void visualizemod::init_vertice_table() {
    int x, p, i;
    float *weights = new float[graphdata->n_vertices];
    for (i = 0; i < graphdata->n_vertices; ++i) weights[i] = 0;

// #pragma omp parallel for
    for (x = 0; x < graphdata->n_vertices; ++x) {
        for (p = graphdata->head[x]; p >= 0; p = graphdata->graph[p].next)
        {
            weights[x] += graphdata->graph[p].weight;
        }
        weights[x] = pow(weights[x], 0.75);
    }
    // alias_edge = new int[graphdata->n_vertices];
    // prob_edge = new float[graphdata->n_vertices];
    // for(int i = 0; i < graphdata->n_vertices; ++i) printf("%.3f ", weights[i]); printf("\n");
    init_alias_table(graphdata->n_vertices, weights, alias_vertice, prob_vertice);
    delete[] weights;
    // printf("vertices %d\n\n", graphdata->n_vertices);
    // for(int i = 0; i < graphdata->n_vertices; ++i) printf("%d %.3f : ", alias_vertice[i], prob_vertice[i]); printf("\n");
}

int visualizemod::sample_an_edge(float rand_value1, float rand_value2) {
    int k = (int)((graphdata->n_edges - 0.1) * rand_value1);
    return rand_value2 <= prob_edge[k] ? k : alias_edge[k];
}

int visualizemod::sample_an_vertice(float rand_value1, float rand_value2) {
    int k = (int)((graphdata->n_vertices - 0.1) * rand_value1);
    return rand_value2 <= prob_vertice[k] ? k : alias_vertice[k];
}



void visualizemod::save(string& outfile)
{
    FILE *fout = fopen(outfile.c_str(), "wb");
    fprintf(fout, "%d %d\n", graphdata->n_vertices, 2);
    for (int i = 0; i < graphdata->n_vertices; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            if (j>0) fprintf(fout, " ");
            fprintf(fout, "%.6f", vis[i * 2 + j]);
        }
        fprintf(fout, "\n");
    }
    fclose(fout);
}

#endif


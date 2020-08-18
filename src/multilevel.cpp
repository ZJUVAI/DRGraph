
#ifndef MULTILEVEL_CPP
#define MULTILEVEL_CPP

#include "multilevel.h"


Multilevel::Multilevel(){
}
Multilevel::~Multilevel(){
    if(membership) { delete[] membership; membership = nullptr; }
    if(head) { delete[] head; head = nullptr; }
    if(nodeAttribute) { delete[] nodeAttribute; nodeAttribute = nullptr; }
    if(mass) { delete[] mass; mass = nullptr; }
    if(similarity) { delete[] similarity; similarity = nullptr; }
    vector<Link>().swap(graph);
}

Multilevel::Multilevel(knn& gd, int k_level, int mode, float* similarity_weight) {
    if(k_level == -1) k_level = gd.n_vertices;
    n_vertices = gd.n_vertices;
    n_edges = 0;
    num_cluster = gd.n_vertices;
    graph.reserve(gd.n_vertices * 3);

    head = new int[n_vertices];
    mass = new int[n_vertices];
    similarity = new float[n_vertices];
    membership = new int[n_vertices];
    for(int i = 0; i < n_vertices; ++i) {
        head[i] = -1;
        mass[i] = 1;
        similarity[i] = similarity_weight[i];
        membership[i] = i;
    }

    set<pair<int, int> > st;
    
    for (int x = 0; x < n_vertices; ++x) {
        gd.knn_vec[x].resize(min(k_level, (int)gd.knn_vec[x].size()));
        // printf("knn size: %d\n", (int)gd.knn_vec[x].size());
        for (int i = 0, len = gd.knn_vec[x].size(); i < len; ++i) {
            int y = gd.knn_vec[x][i];
            int fr = x; int to = y;
            
            if(fr == to) continue;
            if(fr > to) swap(fr, to);
            if(!st.count(make_pair(fr, to))) {
                st.insert({fr, to});
            } else continue;    

            if(mode == 0) add_edge(fr, to, 0.0);
            else add_edge(fr, to,  gd.CalcDist(fr, to));
            // du[fr] ++; du[to] ++;
            //     if(fr == to) continue;
            //     if(fr > to) swap(fr, to);
            //     if(!st.count(make_pair(fr, to))) {
            //         st.insert({fr, to});
            //     } else continue;   
            //     add_edge(fr, to,  gd.CalcDist(fr, to));
            // }
        }
    }
}

Multilevel::Multilevel(Data* gd) {
    n_vertices = gd->n_vertices;
    n_edges = 0;
    num_cluster = gd->n_vertices;
    graph.reserve(gd->n_vertices * 3);
    head = new int[n_vertices];
    
    mass = new int[n_vertices];
    membership = new int[n_vertices];
    for(int i = 0; i < n_vertices; ++i) {
        head[i] = -1;
        mass[i] = 1;
        membership[i] = i;
    }

    for(int i = 0, len = gd->graph.size(); i < len; i += 2) {
        if(gd->graph[i].weight <= 1) {
            add_edge(gd->graph[i].from, gd->graph[i].to, 1.0);
        }
    }
}


void Multilevel::add_edge(int v1, int v2, float weight) {
    graph.push_back(Link(v1, v2, head[v1], weight)); head[v1] = n_edges ++;
    graph.push_back(Link(v2, v1, head[v2], weight)); head[v2] = n_edges ++;
}

void Multilevel::add_one_edge(int v1, int v2, float weight) {
    graph.push_back(Link(v1, v2, head[v1], weight)); head[v1] = n_edges ++;
}


vector<Multilevel*> Multilevel::gen_multilevel(knn& gd, int min_clusters, int k_level, int multiMode, float* similarity_weight) {
    vector<Multilevel*> results;

    Multilevel *temp = new Multilevel(gd, k_level, multiMode, similarity_weight);
    results.push_back(temp);
    int last_level = 0;
    int last_clusters = temp->num_cluster;
    
    bool first_round = true;
    int bad_edgenr_counter = 0;
	int act_level = 0;
    float ratio = 0;
    //  && edgenumbersum_of_all_levels_is_linear(ratio, last_level, bad_edgenr_counter)
    while ( (last_clusters > min_clusters)) {
        temp = new Multilevel();

        if(multiMode) temp->multilevelFM3(*results[last_level], first_round);
        else temp->multilevelNormal(*results[last_level], first_round);


        first_round = false;
        if (last_level != 0 && ((temp->num_cluster * 1.0 / last_clusters) > (last_level < 3 ? 0.8 : 0.7) || temp->num_cluster < min_clusters * 0.8)) { delete temp; break; }
        results.push_back(temp);
        // break;
        last_level++;
        ratio = (temp->num_cluster * 1.0 / last_clusters);
        std::printf("mulitlevel %d %.3f\n", temp->num_cluster, (temp->num_cluster * 1.0 / last_clusters)); std::fflush(stdout);
        // 如果上一层的团数减少的不明显或者没有改变则break
       
        last_clusters = temp->num_cluster;
        
    }
    
    return results;
}


vector<Multilevel*> Multilevel::gen_multilevel(Data* gd, int min_clusters, int k_level, int mode) {
    vector<Multilevel*> results;

    Multilevel *temp = new Multilevel(gd);
    results.push_back(temp);
    int last_level = 0;
    int last_clusters = temp->num_cluster;
    
    bool first_round = true;
    int bad_edgenr_counter = 0;
	int act_level = 0;
    float ratio = 0;
    //  && edgenumbersum_of_all_levels_is_linear(ratio, last_level, bad_edgenr_counter)
    while ( (last_clusters > min_clusters)) {
        temp = new Multilevel();

        if(mode) temp->multilevelFM3(*results[last_level], first_round);
        else temp->multilevelNormal(*results[last_level], first_round);


        first_round = false;
        if (last_level != 0 && ((temp->num_cluster * 1.0 / last_clusters) > 0.7 || temp->num_cluster < min_clusters * 0.8)) { delete temp; break; }
        results.push_back(temp);
        // break;
        last_level++;
        ratio = (temp->num_cluster * 1.0 / last_clusters);
        std::printf("mulitlevel %d %.3f\n", temp->num_cluster, (temp->num_cluster * 1.0 / last_clusters)); std::fflush(stdout);
        // 如果上一层的团数减少的不明显或者没有改变则break
       
        last_clusters = temp->num_cluster;
        
    }
    
    // if(mode == 0) {
    //     for(int i = 0; i < results.size(); ++i) {
    //         delete[] (*results[i]).head;
    //         vector<Link>().swap((*results[i]).graph);
    //     }
    // }
    return results;
}

bool Multilevel::edgenumbersum_of_all_levels_is_linear(float ratio, int level, int& bad_edgenr_counter) {
    if(level == 0)
		return true;
	else
	{
		if(ratio <= 0.8)
			return true;
		else if(bad_edgenr_counter < 5)
		{
			bad_edgenr_counter++;
			return true;
		}
		return false;
	}
}

/*
 * 从池子（v_pool）中删除一个节点，并更新索引(v_index)
 * i v_pool的下标
 * */
void Multilevel::deleteNode (int i, int& curV, int* v_index, int* v_pool) {
    // i下标的节点与v_pool最后一个值交换
    curV--;
    int temp = v_pool[i];
    v_pool[i] = v_pool[curV];
    v_pool[curV] = temp;

    // 更新节点位置索引
    v_index[temp] = curV;
    v_index[v_pool[i]] = i;
}

void Multilevel::multilevelFM3(Multilevel& ml, bool first_round) { // ml means the level - 1, prelevel
    map<pair<int, int>, pair<int, int> > edgeSet;
    // map<pair<int, int>, int> edgePath;

    num_cluster = 0;
    int mem_v = ml.num_cluster;
    int curV = ml.num_cluster;
    nodeAttribute = new Node[curV];
    memSize = curV; // num_cluster of (level - 1) > num_cluster
    int randNum = 3;

    int* v_index = new int[curV];
    int* v_pool = new int[curV];
    int* hasDelete = new int[curV];

    for (int i = 0; i < curV; i++) {
        v_index[i] = v_pool[i] = i;
        hasDelete[i] = 0;
    }   

    genrandom r;
    r.init_gsl();
    while (curV > 0) {
        int choose;
        {
            int minn = INF;
            for(int i = 0; i < randNum; ++i) {
                int hh = (int)floor(r.gslRandom() * curV);
                int tmp = v_pool[hh];
                if(ml.mass[tmp] < minn) {
                    minn = ml.mass[tmp];
                    choose = hh;
                }
            }
        }
        
        
        int u = v_pool[choose];
        deleteNode(choose, curV, v_index, v_pool);
        hasDelete[u] = 1;
        // 把与抽取团内的节点(ml->clusters[u])及其相关的节点合成一团
        nodeAttribute[u].mem = num_cluster;
        nodeAttribute[u].nodeType = 1; 
        nodeAttribute[u].sun = u;
        nodeAttribute[u].distToSun = 0;
        // sun_nodes.insert(u);
        // 团v是与团u相关联的团
        vector<int> uNeighbor;
        for (int p = ml.head[u]; ~p; p = ml.graph[p].next) {
            int v = ml.graph[p].to;
            // (u,v) 存在一条边 且 团V没有被标记 ,则把该团的点与u团的点归类到同一团

            nodeAttribute[v].mem = num_cluster;
            nodeAttribute[v].nodeType = 2;
            nodeAttribute[v].sun = u;
            nodeAttribute[v].distToSun = ml.graph[p].weight;
            uNeighbor.push_back(v); 
            if(hasDelete[v] == 0) {
                deleteNode(v_index[v], curV, v_index, v_pool);
                hasDelete[v] = 1;
            }
            
        }
        
        for(int i = 0, len = uNeighbor.size(); i < len; ++i) {
            int v = uNeighbor[i];
            for(int p = ml.head[v]; ~p; p = ml.graph[p].next) {
                int w = ml.graph[p].to;
                if(hasDelete[w] == 0) {
                    deleteNode(v_index[w], curV, v_index, v_pool);
                    hasDelete[w] = 1;
                }
            }
        }

        num_cluster++;
    }
    

    for(int i = 0; i < memSize; ++i) {
        if(nodeAttribute[i].nodeType == 0) {
            // printf("find %d ", i);
            float minDistance = std::numeric_limits<float>::max(); int minPos = -1; int Path = -1;
            int u = i;
            for (int p = ml.head[u]; ~p; p = ml.graph[p].next) {
                int v = ml.graph[p].to;
                if(nodeAttribute[v].nodeType != 2 && nodeAttribute[v].nodeType != 3) continue;
                float dist = ml.graph[p].weight;
                if(dist < minDistance) {
                    Path = p;
                    minDistance = dist;
                    minPos = v;
                }
            }

            assert(minPos != -1);
            
            ml.graph[Path].isMoon = true;
            // ml.graph[Path ^ 1].isMoon = true;
            nodeAttribute[minPos].nodeType = 3;
            nodeAttribute[u].nodeType = 4;
            nodeAttribute[u].mem = nodeAttribute[minPos].mem;
            nodeAttribute[u].sun = nodeAttribute[minPos].sun;
            
            nodeAttribute[u].distToSun = minDistance + nodeAttribute[minPos].distToSun;
            nodeAttribute[minPos].moon_list.push_back(u);

        }
    }

    // for(int i = 0; i < memSize; ++i) {
    //     printf("[%d %d %d]", i, nodeAttribute[i].sun, nodeAttribute[i].nodeType);
    //     // if(sun_nodes.find(nodeAttribute[i].sun) == sun_nodes.end()) {
    //     //     assert(0);
    //     // }
    // }
    // printf("\n");

    n_vertices = ml.n_vertices;
    n_edges = 0;
    mass = new int[num_cluster];
    for(int i = 0; i < num_cluster; ++i) {
        mass[i] = 0;
    }
    graph.reserve(num_cluster * 3);
    head = new int[num_cluster];
    membership = new int[n_vertices];
    for(int i = 0; i < num_cluster; ++i) {
        head[i] = -1;
    }

    for(int i = 0; i < memSize; ++i) {
        int tt = nodeAttribute[nodeAttribute[i].sun].mem;
        mass[tt] ++;
    }

   

    for(int i = 0, len = ml.n_edges; i < len; i += 2) {
        
        int from = nodeAttribute[ml.graph[i].from].mem; int to = nodeAttribute[ml.graph[i].to].mem;
        // printf("ying %d %d %d %d %d\n", i, ml.n_edges, num_cluster, from, to);
        if(from > to) swap(from, to);
        if(from == to) continue;
        float length_s_edge = nodeAttribute[ml.graph[i].from].distToSun;
        float length_t_edge = nodeAttribute[ml.graph[i].to].distToSun;
        float newLength = ml.graph[i].weight + length_s_edge + length_t_edge;
        if(edgeSet.find(make_pair(from, to)) == edgeSet.end()) {
            edgeSet[make_pair(from, to)] = make_pair(1, n_edges);
            add_edge(from, to, newLength);
            // add_edge(to, from, newLength);
        } else {
            pair<int, int> tmp = edgeSet[make_pair(from, to)];
            edgeSet[make_pair(from, to)] = make_pair(tmp.first + 1, tmp.second);
            graph[tmp.second].weight += newLength;
            graph[tmp.second ^ 1].weight += newLength;
        }
		float lambdaS = length_s_edge / newLength;
		float lambdaT = length_t_edge / newLength;
        nodeAttribute[ml.graph[i].from].lamba_list.push_back(lambdaS);
        nodeAttribute[ml.graph[i].from].neighborSunNode.push_back(nodeAttribute[ml.graph[i].to].sun);
        nodeAttribute[ml.graph[i].to].lamba_list.push_back(lambdaT);
        nodeAttribute[ml.graph[i].to].neighborSunNode.push_back(nodeAttribute[ml.graph[i].from].sun);
    }

    for(auto it = edgeSet.begin(); it != edgeSet.end(); ++it) {
        int t3 = (it->second).first; int t4 = (it->second).second;
        graph[t4].weight /= t3;
        graph[t4 ^ 1].weight /= t3;
    } 
    
    for(int i = 0; i < n_vertices; ++i) {
        membership[i] = nodeAttribute[ml.membership[i]].mem;
    }

    // delete[] mem;
    // delete[] ml.head;
    // vector<Link>().swap(ml.graph);
    delete[] v_index;
    delete[] v_pool;
    delete[] hasDelete;
}

void Multilevel::multilevelNormal(Multilevel& ml, bool first_round) { // ml means the level - 1, prelevel
    map<pair<int, int>, pair<int, int> > edgeSet;
    num_cluster = 0;
    int mem_v = ml.num_cluster;
    int curV = ml.num_cluster;
    memIter = new int[curV];
    memSize = curV; // num_cluster of (level - 1) > num_cluster

    int* v_index = new int[curV];
    int* v_pool = new int[curV];
    int* hasDelete = new int[curV];

    for (int i = 0; i < curV; i++) {
        v_index[i] = v_pool[i] = i;
        hasDelete[i] = 0;
    }   

    genrandom r;
    auto Abs = [](int x) { return x < 0 ? -x - 1 : x; };
    r.init_gsl();
    while (curV > 0) {
        int choose = -1;
        {
            int target = -1;
            for(int i = 0; i < 10; ++i) {
                int hh = (int)floor(r.gslRandom() * curV);
                int tmp = v_pool[hh];
                if(ml.similarity[tmp] > target) {
                    target = ml.similarity[tmp];
                    choose = hh;
                }
            }
        }
        // Global::multiChoose.push_back(choose);
        int u = v_pool[choose];
        deleteNode(choose, curV, v_index, v_pool);
        
        hasDelete[u] = 1;
        // 把与抽取团内的节点(ml->clusters[u])及其相关的节点合成一团
        memIter[u] = -num_cluster - 1;

        for (int p = ml.head[u]; ~p; p = ml.graph[p].next) {
            int v = ml.graph[p].to; 
            if(hasDelete[v] == 0) {
                memIter[v] = num_cluster;
                deleteNode(v_index[v], curV, v_index, v_pool);
                hasDelete[v] = 1;
            }
        }
        num_cluster++;
    }

    n_vertices = ml.n_vertices;
    n_edges = 0;
    // du = new int[num_cluster];
    similarity = new float[num_cluster];
    for(int i = 0; i < num_cluster; ++i) {
        similarity[i] = 0;
    }
    // mass = new int[num_cluster];
    // for(int i = 0; i < num_cluster; ++i) {
    //     mass[i] = 0;
    // }
    graph.reserve(num_cluster * 3);
    head = new int[num_cluster];
    membership = new int[n_vertices];
    for(int i = 0; i < num_cluster; ++i) {
        head[i] = -1;
    }

    for(int i = 0, len = ml.n_edges; i < len; i += 2) {
        int from = Abs(memIter[ml.graph[i].from]); int to = Abs(memIter[ml.graph[i].to]);
        // printf("ying %d %d %d %d %d\n", i, ml.n_edges, num_cluster, from, to);
        if(from > to) swap(from, to);
        if(from == to) continue;
        
        if(edgeSet.find(make_pair(from, to)) == edgeSet.end()) {
            edgeSet[make_pair(from, to)] = make_pair(1, n_edges);
            add_edge(from, to, 0.0);
            // du[from] ++; du[to] ++;
        }
    }

    for(int i = 0; i < n_vertices; ++i) {
        membership[i] = Abs(memIter[ml.membership[i]]);
    }
    // for(int i = 0; i < memSize; ++i) {
    //     if(memIter[i] < 0) {
    //         similarity[Abs(memIter[i])] += ml.similarity[i];
    //         for (int p = ml.head[i]; ~p; p = ml.graph[p].next) {
    //             int v = ml.graph[p].to;
    //             if(memIter[v] >= 0)
    //                 similarity[memIter[v]] += ml.similarity[i] / ml.du[v];
    //             // else printf("yingyingying\n");
    //         }
    //     }
    // }

    for(int i = 0; i < memSize; ++i) {
       similarity[Abs(memIter[i])] += ml.similarity[i];
    }

    delete[] v_index;
    delete[] v_pool;
    delete[] hasDelete;
}

#endif

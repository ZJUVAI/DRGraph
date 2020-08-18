#ifndef QUADTREE_H
#define QUADTREE_H

#include <cstdlib>
#include <vector>

static inline float min(float x, float y) { return (x <= y ? x : y); }
static inline float max(float x, float y) { return (x <= y ? y : x); }
static inline float abs_d(float x) { return (x <= 0 ? -x : x); }

class Cell {

public:
    float* center;
    float* width;
    int n_dims;
    bool   containsPoint(float point[]);
    ~Cell() {
        delete[] center;
        delete[] width;
    }
};


class QuadTree
{

    // Fixed constants
    int QT_NODE_CAPACITY;

    // Properties of this node in the tree
    int QT_NO_DIMS;
    int QT_N;

    int cum_size;

    // Axis-aligned bounding box stored as a center with half-dimensions to represent the boundaries of this quad tree
    Cell boundary;

    // Indices in this quad tree node, corresponding center-of-mass, and list of all children
    float* data;



public:
    static int num_leaf;
    bool is_leaf;
    int size;
    int *index;
    int num_children;
    float* center_of_mass;
    std::vector<QuadTree*> children;
    QuadTree(float* inp_data, int N, int no_dims);
    QuadTree(QuadTree* inp_parent, float* inp_data, float* mean_Y, float* width_Y, int node_capacity);
    ~QuadTree();
    void construct(Cell boundary);
    bool insert(int new_index);
    void subdivide();
    void computeNonEdgeForces(int point_index, float theta, float* neg_f, float* sum_Q);
private:

    void init(QuadTree* inp_parent, float* inp_data, float* mean_Y, float* width_Y);
    void fill(int N);
};



#endif

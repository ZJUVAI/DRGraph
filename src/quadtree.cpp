#ifndef QUADTREE
#define QUADTREE

#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <cstdio>
#include <stack>
#include <iostream>
#include "quadtree.h"
using namespace std;

int QuadTree::num_leaf = 0;

// Checks whether a point lies in a cell
bool Cell::containsPoint(float point[])
{
    for (int i = 0; i< n_dims; ++i) {
        if (abs_d(center[i] - point[i]) > width[i]) {
            return false;
        }
    }
    return true;
}


// Default constructor for quadtree -- build tree, too!
QuadTree::QuadTree(float* inp_data, int N, int no_dims)
{
    QT_N = N;
    QT_NODE_CAPACITY = floor(50);
    QT_NO_DIMS = no_dims;
    num_children = 1 << no_dims;
//    std::cout << "QT_NODE_CAPACITY : " << QT_NODE_CAPACITY << endl;
    index = new int[QT_NODE_CAPACITY];
    // Compute mean, width, and height of current map (boundaries of QuadTree)
    float* mean_Y = new float[QT_NO_DIMS];
    for (int d = 0; d < QT_NO_DIMS; d++) {
        mean_Y[d] = .0;
    }

    float*  min_Y = new float[QT_NO_DIMS];
    for (int d = 0; d < QT_NO_DIMS; d++) {
        min_Y[d] =  DBL_MAX;
    }
    float*  max_Y = new float[QT_NO_DIMS];
    for (int d = 0; d < QT_NO_DIMS; d++) {
        max_Y[d] = -DBL_MAX;
    }

    for (int n = 0; n < N; n++) {
        for (int d = 0; d < QT_NO_DIMS; d++) {
            mean_Y[d] += inp_data[n * QT_NO_DIMS + d];
            min_Y[d] = min(min_Y[d], inp_data[n * QT_NO_DIMS + d]);
            max_Y[d] = max(max_Y[d], inp_data[n * QT_NO_DIMS + d]);
        }

    }

    float* width_Y = new float[QT_NO_DIMS];
    for (int d = 0; d < QT_NO_DIMS; d++) {
        mean_Y[d] /= (float) N;
        width_Y[d] = max(max_Y[d] - mean_Y[d], mean_Y[d] - min_Y[d]) + 1e-5;
    }

    // Construct QuadTree
    init(NULL, inp_data, mean_Y, width_Y);
    fill(N);
    delete[] max_Y; delete[] min_Y;
}

// Constructor for QuadTree with particular size and parent (do not fill the tree)
QuadTree::QuadTree(QuadTree* inp_parent, float* inp_data, float* mean_Y, float* width_Y, int node_capacity)
{
    QT_NODE_CAPACITY = node_capacity;
    QT_NO_DIMS = inp_parent->QT_NO_DIMS;
    num_children = 1 << QT_NO_DIMS;

    index = new int[QT_NODE_CAPACITY];
    init(inp_parent, inp_data, mean_Y, width_Y);
}


// Main initialization function
void QuadTree::init(QuadTree* inp_parent, float* inp_data, float* mean_Y, float* width_Y)
{
    // parent = inp_parent;
    data = inp_data;
    is_leaf = true;
    num_leaf++;
    size = 0;
    cum_size = 0;

    boundary.center = mean_Y;
    boundary.width  = width_Y;
    boundary.n_dims = QT_NO_DIMS;

    index[0] = 0;

    center_of_mass = new float[QT_NO_DIMS];
    for (int i = 0; i < QT_NO_DIMS; i++) {
        center_of_mass[i] = .0;
    }
}


// Destructor for QuadTree
QuadTree::~QuadTree()
{
    for(unsigned int i = 0; i != children.size(); i++) {
        delete children[i];
    }
    delete[] center_of_mass;
}


// Insert a point into the QuadTree
bool QuadTree::insert(int new_index)
{
    // Ignore objects which do not belong in this quad tree
    float* point = data + new_index * QT_NO_DIMS;
    if (!boundary.containsPoint(point)) {
        return false;
    }

    // Online update of cumulative size and center-of-mass
    cum_size++;
    float mult1 = (float) (cum_size - 1) / (float) cum_size;
    float mult2 = 1.0 / (float) cum_size;
    for (int d = 0; d < QT_NO_DIMS; d++) {
        center_of_mass[d] = center_of_mass[d] * mult1 + mult2 * point[d];
    }

    // If there is space in this quad tree and it is a leaf, add the object here
    if (is_leaf && size < QT_NODE_CAPACITY) {
        index[size] = new_index;
        size++;
        return true;
    }

    // Don't add duplicates for now (this is not very nice)
    bool any_duplicate = false;
    for (int n = 0; n < size; n++) {
        bool duplicate = true;
        for (int d = 0; d < QT_NO_DIMS; d++) {
            if (point[d] != data[index[n] * QT_NO_DIMS + d]) { duplicate = false; break; }
        }
        any_duplicate = any_duplicate | duplicate;
    }
    if (any_duplicate) {
        return true;
    }

    // Otherwise, we need to subdivide the current cell
    if (is_leaf) {
        subdivide();
    }

    // Find out where the point can be inserted
    for (int i = 0; i < num_children; ++i) {
        if (children[i]->insert(new_index)) {
            return true;
        }
    }

    // Otherwise, the point cannot be inserted (this should never happen)
//    printf("%s\n", "No no, this should not happen");
    return false;
}

int *get_bits(int n, int bitswanted){
    int *bits = new int[bitswanted];

    int k;
    for(k=0; k<bitswanted; k++) {
        int mask =  1 << k;
        int masked_n = n & mask;
        int thebit = masked_n >> k;
        bits[k] = thebit;
    }

    return bits;
}

// Create four children which fully divide this cell into four quads of equal area
void QuadTree::subdivide() {

    // Create children
    float* new_centers = new float[2 * QT_NO_DIMS];
    for(int i = 0; i < QT_NO_DIMS; ++i) {
        new_centers[i*2]     = boundary.center[i] - .5 * boundary.width[i];
        new_centers[i*2 + 1] = boundary.center[i] + .5 * boundary.width[i];
    }

    for (int i = 0; i < num_children; ++i) {
        int *bits = get_bits(i, QT_NO_DIMS);

        float* mean_Y = new float[QT_NO_DIMS];
        float* width_Y = new float[QT_NO_DIMS];

        // fill the means and width
        for (int d = 0; d < QT_NO_DIMS; d++) {
            mean_Y[d] = new_centers[d*2 + bits[d]];
            width_Y[d] = .5*boundary.width[d];
        }

        QuadTree* qt = new QuadTree(this, data, mean_Y, width_Y, QT_NODE_CAPACITY);
        children.push_back(qt);
        delete[] bits;
    }
    delete[] new_centers;

    // Move existing points to correct children
    for (int i = 0; i < size; i++) {
        // bool flag = false;
        for (int j = 0; j < num_children; j++) {
            if (children[j]->insert(index[i])) {
                // flag = true;
                break;
            }
        }
        // if (flag == false) {
        index[i] = -1;
        // }
    }

    // This node is not leaf now
    // Empty it
    size = 0;
    is_leaf = false;
    num_leaf--;
}


// Build QuadTree on dataset
void QuadTree::fill(int N)
{
    for (int i = 0; i < N; i++) {
        insert(i);
    }
}


#endif

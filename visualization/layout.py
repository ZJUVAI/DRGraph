#coding=utf-8
import networkx as nx
from matplotlib import pyplot
import numpy as np
import argparse
import warnings
import random


def fxn():
    warnings.warn("deprecated", DeprecationWarning)

def normalize(Y):
    Y_cpy = Y.copy()
    # Translate s.t. smallest values for both x and y are 0.
    for dim in range(Y.shape[1]):
        Y_cpy[:, dim] += -Y_cpy[:, dim].min()
    # Scale s.t. max(max(x, y)) = 1 (while keeping the same aspect ratio!)
    scaling = 1 / (np.absolute(Y_cpy).max())
    Y_cpy *= scaling
    return Y_cpy

def load_graphpos(filename):
    nnode = 0
    dim = 0
    with open(filename) as f:
        header = f.readline()
        nnode,dim = [int(i) for i in header.split()]
    rawdata = np.loadtxt(filename,dtype=float,skiprows=1)
    data = [(n[0],n[1]) for n in rawdata]
    return nnode, dim, np.array(data)

def txtToPng(layout, outpng, graph):
    G=nx.Graph()
    nnode, dim, Y = load_graphpos(layout)
    data = normalize(Y)

    index = 0
    while index < nnode:
        i = data[index][0]
        j = data[index][1]
        G.add_node(index, pos=(i,j))
        index += 1

    f = open(graph)
    line = f.readline()
    l1 = line.split(' ')
    nnode = int(l1[0])
    nedge = int(l1[1])
    index = 0
    while line:
        line = f.readline()
        ll = line.split(' ')
        if len(ll) < 3:
            continue
        i = int(ll[0])
        j = int(ll[1])
        if i == j:
            continue
        if nedge < 600000:
            G.add_edges_from([(i,j)])
        elif nedge < 3000000:
            if random.randint(0, 9) == 0:
                G.add_edges_from([(i,j)])
        else:
            if random.randint(0, 99) == 0:
                G.add_edges_from([(i,j)])

        index += 1
    f.close()
    pos=nx.get_node_attributes(G,'pos')
    edges = G.edges()
    edge_length = []
    edge_length = [np.sqrt((pos[x][0]-pos[y][0])**2 + (pos[x][1]-pos[y][1])**2) for (x,y) in edges]
    edge_length = np.array(edge_length)
    edge_length = edge_length - np.min(edge_length)
    edge_length = edge_length / np.max(edge_length)

    pyplot.axis('off')
    set_width = 0.2
    if nedge < 5000:
        set_width = 0.4
    elif nedge > 400000:
        set_width = 0.02

    nx.draw_networkx(G,pos, node_color='white',node_size=0, alpha=1, width = set_width, with_labels=False, edge_color = edge_length, edge_cmap = pyplot.get_cmap('jet_r'))
    pyplot.savefig(outpng, dpi = 300, pad_inches = 0)
    pyplot.close('all')
    print("visualize complete!")

if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument('-graph', default = '', help = 'Input graphs')
    parser.add_argument('-layout', default = '', help = 'Graph layout result')
    parser.add_argument('-outpng', default = '', help = 'Visualization result')

    args = parser.parse_args()

    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        txtToPng(args.layout, args.outpng, args.graph)
        fxn()


from flask import Flask, request
import json
import random
import numpy as np
import os
import time
from werkzeug.wrappers import Response, Request
from flask_cors import CORS
import subprocess

app = Flask(__name__)
CORS(app)

def updateEdgeList(edge_list, node_list, x, y):
    edge_list.append({"source_x": float(node_list[x][0]), "source_y": float(node_list[x][1]), 
     "target_x": float(node_list[y][0]), "target_y": float(node_list[y][1])})


@app.route('/getLayout', methods=['GET', 'POST'])
def getLayout():
    reponse = json.loads(request.get_data())

    graph_path = "./data/" + reponse["file"] + ".txt"
    output_path = "./data/" + reponse["file"] + "_vec.txt"

    # subprocess.call([
    #     "../../Vis",
    #     "-input", 
    #     graph_path,
    #     "-output",
    #     output_path,
    #     "-neg",
    #     "7",
    #     "-samples",
    #     "1000",
    #     "-gamma",
    #     "0.1",
    #     "-mode",
    #     "1",
    #     "-A",
    #     "2",
    #     "-B",
    #     "2",
    # ])

    edge_list = []
    

    f = open(output_path)
    line = f.readline()
    l1 = line.split(' ')
    nnode = int(l1[0])
    nedge = int(l1[1])
    node_pos = np.zeros((nnode, 2)).astype(np.float)
    cnt = 0
    while line:
        line = f.readline()
        ll = line.split(' ')
        if len(ll) < 2:
            continue
        node_pos[cnt][0] = float(ll[0])
        node_pos[cnt][1] = float(ll[1])
        cnt = cnt + 1
    f.close()

    node_pos -= np.mean(node_pos)
    node_pos /= np.max(node_pos)

    f = open(graph_path)
    line = f.readline()
    l1 = line.split(' ')
    nnode = int(l1[0])
    nedge = int(l1[1])
    print("nodes: ", nnode, "edges: ", nedge)
    while line:
        line = f.readline()
        ll = line.split(' ')
        if len(ll) < 3:
            continue
        ll[0] = int(ll[0])
        ll[1] = int(ll[1])
        if nedge < 300000:
            updateEdgeList(edge_list, node_pos, ll[0], ll[1])
        elif nedge < 500000:
            if random.randint(0, 4) == 0:
                updateEdgeList(edge_list, node_pos, ll[0], ll[1])
        elif nedge < 1000000:
            if random.randint(0, 99) == 0:
                updateEdgeList(edge_list, node_pos, ll[0], ll[1])
        elif nedge < 2000000:
            if random.randint(0, 19) == 0:
                updateEdgeList(edge_list, node_pos, ll[0], ll[1])
        elif nedge < 5000000:
            if random.randint(0, 49) == 0:
                updateEdgeList(edge_list, node_pos, ll[0], ll[1])
        elif nedge < 10000000:
            if random.randint(0, 99) == 0:
                updateEdgeList(edge_list, node_pos, ll[0], ll[1])
        elif nedge < 20000000:
            if random.randint(0, 199) == 0:
                updateEdgeList(edge_list, node_pos, ll[0], ll[1])
        elif nedge < 50000000:
            if random.randint(0, 599) == 0:
                updateEdgeList(edge_list, node_pos, ll[0], ll[1])
        else:
            if random.randint(0, 999) == 0:
                updateEdgeList(edge_list, node_pos, ll[0], ll[1])
    f.close()
    
    result = {"node_num" : nnode, "edge_num" : nedge, "links" : edge_list}

    print("prepare to send data")
    return Response(json.dumps(result), content_type="application/json")

@app.route('/getAll')
def getAll():
    result = {}
    file_list = os.listdir("./data")
    graph_file_list = []
    for file_ele in file_list:
        if(file_ele[-8:] == "_vec.txt" and os.path.exists("./data/" + file_ele[:-8] + ".txt")):
            graph_file_list.append(file_ele[:-8])
    result["graph_list"] = graph_file_list
    result["layout"] = { "links": []}
    return Response(json.dumps(result), content_type="application/json")

if __name__ == '__main__':
    app.run(debug=True)
    # app.run()
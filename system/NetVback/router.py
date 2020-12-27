from flask import Flask, request
import json
import random
import numpy as np
import os
import time
from werkzeug.wrappers import Response, Request
from flask_cors import CORS
import subprocess
import random
import time
import subprocess

app = Flask(__name__)
CORS(app)


def randomString(len):
    array_list = random.sample(['z','y','x','w','v','u','t','s','r','q','p','o','n','m','l','k','j','i','h','g','f','e','d','c','b','a'], len)
    result = ""
    for key in array_list:
        result += key
    return result + ".txt"

@app.route('/getLayout', methods=['GET', 'POST'])
def getLayout():
    parameterList = {
        "neg": 5,
        "samples": 200,
        "gamma" : 0.1,
        "A": 2.0,
        "B": 1.0
    }

    reponse = json.loads(request.get_data())
    for key in reponse:
        if key in parameterList:
            parameterList[key] = reponse[key]
    
    fileNameUnit = "___error.txt"
    
    while(True):
        fileNameUnit = randomString(8)
        if(os.path.exists("./file/" + fileNameUnit)):
            time.sleep(1)
        else:
            break
    
    fileName = "./file/" + fileNameUnit
    outputName = "./file/" + fileNameUnit + ".out"

    result = {"status": "SUCESS", "event": ""}
    
    if("graph" not in reponse):
        result["status"] = "FAILED"
        result["event"] = "request has no `graph` attribute"
        return Response(json.dumps(result), content_type="application/json")

    if("node" not in reponse):
        result["status"] = "FAILED"
        result["event"] = "request has no `node` attribute"
        return Response(json.dumps(result), content_type="application/json")

    if("edge" not in reponse):
        result["status"] = "FAILED"
        result["event"] = "request has no `edge` attribute"
        return Response(json.dumps(result), content_type="application/json")
    
    if(reponse["edge"] != len(reponse["graph"])):
        result["status"] = "FAILED"
        result["event"] = "the length of graph must be equal to the edge"
        return Response(json.dumps(result), content_type="application/json")
    
    with open(fileName, "w") as f:
        f.write(str(reponse["node"]) + " " + str(reponse["edge"]) + "\n")
        for item in reponse["graph"]:
            f.write(str(item[0]) + " " + str(item[1]) + "\n")
        f.close() 
    
    subprocess.call([
        "../../Vis",
        "-input", 
        fileName,
        "-output",
        outputName,
        "-neg",
        str(parameterList["neg"]),
        "-samples",
        str(parameterList["samples"]),
        "-gamma",
        str(parameterList["gamma"]),
        "-mode",
        "1",
        "-A",
        str(parameterList["A"]),
        "-B",
        str(parameterList["B"]),
    ])

    node_pos = []

    with open(outputName, "r") as f:
        line = f.readline()
        l1 = line.split(' ')
        nnode = int(l1[0])
        nedge = int(l1[1])
        cnt = 0
        while line:
            line = f.readline()
            ll = line.split(' ')
            if len(ll) < 2:
                continue
            node_pos.append([str(float(ll[0])), str(float(ll[1]))])
            cnt = cnt + 1
            if(cnt == nnode):
                break
        f.close()

    result["pos"] = node_pos

    if(reponse["node"] != len(result["pos"])):
        result["status"] = "FAILED"
        result["event"] = "some error make our result is not compelete"
        return Response(json.dumps(result), content_type="application/json")

    os.remove(fileName)
    os.remove(outputName)
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
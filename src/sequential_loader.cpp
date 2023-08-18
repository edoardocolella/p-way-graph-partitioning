#include "Graph.h"

#include <iostream>
#include <fstream>
#include <chrono>

using namespace std;
Graph* loadFromFile_s(const string& path){
    ifstream file(path, ios::binary);
    if(!file.is_open())
        throw std::runtime_error("Error opening file");

    cout << "Loading graph..." << endl;
    auto start_time = chrono::high_resolution_clock::now();

    auto g = new Graph();
    int num_nodes;
    int num_edges;

    file.read((char*)&num_nodes, sizeof(int));
    file.read((char*)&num_edges, sizeof(int));

    cout << "num_nodes: " << num_nodes << endl;


    int id, node_weight;
    for(int i= 0; i<num_nodes; i++){
        file.read((char*)&id, sizeof(int));
        file.read((char*)&node_weight, sizeof(int));
        g->add_node(id, node_weight);
    }

    int source, dest, distance;
    for(int i = 0; i<num_edges; i++){
        file.read((char*)&source, sizeof(int));
        file.read((char*)&dest, sizeof(int));
        file.read((char*)&distance, sizeof(int));
        g->add_edge(source, dest, distance);
    }

    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    cout << "Time to load graph: " << duration.count() << "ms" << endl;

    return g;
}
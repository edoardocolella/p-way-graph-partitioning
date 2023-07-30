#ifndef GRAPH_H
#define GRAPH_H

#include <barrier>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

struct Edge {
    int source;
    int dest;
    int weight;
    bool matched;

    bool operator<(const Edge &a) const;

    bool operator>(const Edge &a) const;

    bool operator==(const Edge &c) const;
};

struct Node {
    int id;
    int weight;
    int partition;
};

class Graph {
private:
    set<Edge> edges;
    vector<Node> nodes;
    mutex mNode;
    mutex mEdge;
    set<Edge>::iterator maxIterator;
    void node_reader(const string &path, unsigned long offset_from_start_nodes, int nodes_to_read);
    void edge_reader(const string &path, unsigned long offset_from_start_edges, int edges_to_read);

public:
    Graph();
    Graph(const string &path);
    void add_node(int id, int weight, int partition);
    void add_node(const Node &n_node);
    void add_edge(int source, int dest, int weight);
    void add_edge(Edge n_edge);
    void increase_edge(int source, int dest, int weight_increment);
    Node get_node(int id);
    Edge get_edge(int source, int dest);
    Edge get_next_max_edge(vector<bool> &matched);
    int get_num_nodes();
    int get_num_edges();
    void print();
    void set_node_partition(int id, int partition);
    set<Edge> get_edges();
    void resetMaxIterator();
};

#endif

#include "Graph.h"
#include "partitioning.h"
#include <iostream>
#include <set>
#include <thread>
#include <vector>

void uncoarsen_graph_step(std::shared_ptr<Graph>& g, std::vector<int> &partitions,
                          std::vector<int> &newPartitions, int num_nodes, int start, int step) {
    int i = start;
    while (i < num_nodes) {
        newPartitions[g->nodes[i]->id] = partitions[g->nodes[i]->child->id];
        i += step;
    }
}

std::vector<int> uncoarsen_graph_p(std::shared_ptr<Graph>& g, std::vector<int> &partitions, int num_thread) {
    unsigned int num_nodes = g->V();
    std::vector<int> newPartitions(num_nodes);
    std::vector<std::thread> threads(num_thread);

    for (int i = 0; i < num_thread; i++) {
        threads[i] = std::thread(uncoarsen_graph_step, ref(g), ref(partitions), ref(newPartitions), num_nodes, i, num_thread);
    }
    for (auto &t : threads) {
        t.join();
    }

    return newPartitions;
}

void partitioning_p(std::shared_ptr<Graph>& g, int requestedPartitions, int num_threads) {
    unsigned int actual_num_partitions = g->V();

    std::vector<std::shared_ptr<Graph>> allGraphs;
    allGraphs.push_back(g);

    int iterations = 0;
    while (actual_num_partitions > requestedPartitions * 15 && iterations++ < 50) {
        std::cout << "Iteration " << iterations << std::endl;
        auto start          = std::chrono::high_resolution_clock::now();
        auto coarsedGraph = coarseGraph_p(allGraphs.back(), num_threads);
        auto end            = std::chrono::high_resolution_clock::now();
        std::cout << "Coarsening time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
        // coarsedGraph->print();
        actual_num_partitions = coarsedGraph->V();
        allGraphs.push_back(coarsedGraph);
    }

    std::shared_ptr<Graph> coarsestGraph = allGraphs[allGraphs.size() - 1];
    std::vector<int> partitions(coarsestGraph->V(), -1);
    for (int i = 0; i < coarsestGraph->V(); i++)
        partitions[i] = i % requestedPartitions;

    kernighanLin_p(coarsestGraph, requestedPartitions, partitions, coarsestGraph->num_colours, coarsestGraph->colours, num_threads);

    for (auto i = (int) allGraphs.size() - 2; i >= 0; i--) {
        partitions = uncoarsen_graph_p(allGraphs[i], partitions, num_threads);
        std::cout << "Uncoarsening step " << allGraphs.size() - i - 1 << std::endl;
        allGraphs[i]->partitions_size = allGraphs[i + 1]->partitions_size;
        kernighanLin_p(allGraphs[i], requestedPartitions, partitions, coarsestGraph->num_colours, coarsestGraph->colours, num_threads);
    }

    /*for (int i = 0; i < partitions.size(); i++) {
        cout << "Node " << i << " in partition " << partitions[i] << endl;
    }*/

    save_to_file("OutputPartitions.txt", g, partitions, requestedPartitions);

    //for (int i = 1; i < allGraphs.size(); i++)
    //    delete allGraphs[i];
}

bool Compare_Node(Node *a, Node *b) {
    if (a->weight > b->weight || (a->weight == b->weight && a->id > b->id))
        return true;
    else
        return false;
}

void partitioning_thread(std::shared_ptr<Graph> &graph, std::vector<int> *partitions,
                         std::vector<std::mutex> *nodes_m, std::vector<int> *starting_nodes,
                         int avg_p_weight, std::vector<int> *weight) {
    // vector of sets
    std::vector<std::set<Node *, bool (*)(Node *, Node *)>> queues_of_nodes;
    for (int i = 0; i < starting_nodes->size(); i++) {
        // collecting in a set all neighbours of my starting node that are still unassigned to a partition
        std::set<Node *, bool (*)(Node *, Node *)> queue(Compare_Node);
        for (auto n : graph->nodes[(*starting_nodes)[i]]->get_neighbors())
            queue.insert(n);

        queues_of_nodes.push_back(queue);
    }
    // we assign each neighbour to the partition of the starting node iff
    // the weight of this partition we are filling up is lower than the graph avg partition weight
    // otherwise we leave the neighbour unassigned and will treat it later
    bool next_partition = false;
    int index           = 0;
    // weight of the partition we are forming by adding each neighbour to it
    while (!next_partition) {
        for (auto neighbour = queues_of_nodes[index].begin(); neighbour != queues_of_nodes[index].end();) {
            // lock starting node and its neighbour
            std::scoped_lock((*nodes_m)[(*neighbour)->id], (*nodes_m)[(*starting_nodes)[index]]);
            // we set 1.4 waiting to test if it's ok as a multiplication factor
            // if adding the neighbour to the partition does not exceed the avg and the neighbour hasn't been
            // assigned by another thread to another partition, assign it and remove it from the set
            // of unassigned node
            if ((*weight)[index] + (*neighbour)->weight <= avg_p_weight * 1.4 && (*partitions)[(*neighbour)->id] == -1) {
                (*partitions)[(*neighbour)->id] = (*partitions)[(*starting_nodes)[index]];
                (*weight)[index] += (*neighbour)->weight;

                Node *s_neighbour = *neighbour;

                // add neighbour's neighbours to the queue
                for (auto n : s_neighbour->get_neighbors())
                    queues_of_nodes[index].insert(n);

                // remove neighbour from unassigned
                queues_of_nodes[index].erase(s_neighbour);
                // reset iterator to the start of the set since we have added new neighbours to this set
                neighbour = queues_of_nodes[index].begin();

            } else if ((*partitions)[(*neighbour)->id] != -1) {
                // if another thread assigned it already you can remove it from set of unassigned and release lock on it
                queues_of_nodes[index].erase(neighbour);
            } else {
                neighbour++;
            }
        }
        // move to next node in our starting pool
        index++;
        if (index > starting_nodes->size())
            next_partition = true;
    }
}

void initial_partitioning_p(std::shared_ptr<Graph>&graph, std::vector<int> &partitions,
                            int num_partitions, int num_threads) {
    std::vector<std::vector<int>> starting_nodes(num_threads);
    std::set<Node *, bool (*)(Node *, Node *)> ordered_nodes(graph->nodes.begin(), graph->nodes.end(), Compare_Node);
    // one weight vector for each thread to operate on
    std::vector<std::vector<int>> weights(num_threads);

    auto top_node = ordered_nodes.begin();
    // more partitions than threads available: anyway this is just the initial partitioning
    for (int i = 0; i < num_partitions; i++) {
        partitions[(*top_node)->id] = i;
        starting_nodes[i % num_threads].push_back((*top_node)->id);
        // we save partition weights as we need to know them in the last step of this function: assignment of unassigned nodes
        weights[i % num_threads].push_back((*top_node)->weight);
        top_node++;
        if (top_node == ordered_nodes.end())
            break;
    }

    std::vector<std::mutex> nodes_m(graph->nodes.size());

    std::vector<std::thread> partitioners;

    for (int i = 0; i < num_threads; i++)
        partitioners.emplace_back(partitioning_thread, ref(graph), &partitions, &nodes_m, &starting_nodes[i], graph->node_weight_global / num_partitions, &weights[i]);

    for (auto &t : partitioners)
        t.join();

    // assign leftover neighbouring nodes still without a partition
    // criterion: assign it to the LIGHTEST neighbouring partition
    for (int i = 0; i < partitions.size(); i++) {
        if (partitions[i] == -1) {
            int w_lightest_partition  = -1;
            std::vector<Node *> neighbours = graph->nodes[i]->get_neighbors();
            for (auto s : neighbours) {
                if (partitions[s->id] == -1)
                    continue;
                if (w_lightest_partition == -1) {
                    w_lightest_partition = partitions[s->id];
                    continue;
                }
                if (weights[s->id % num_threads][s->id / num_threads] < weights[w_lightest_partition % num_threads][w_lightest_partition / num_threads]) {
                    w_lightest_partition = s->id;
                }
            }
            partitions[i] = partitions[w_lightest_partition];
            //if it crashes because w_lightest_partition == -1 still, try changing factor 1.4 above
            
        }
    }
}

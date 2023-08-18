#include "Graph.h"
#ifndef CHANGE_H
#define CHANGE_H

struct Change{
    Change(int new_partition, Node *node, int gain);
    Change();
    Node* node;
    int new_partition;
    int gain;
    bool operator< (const Change &other) const;
};

#endif
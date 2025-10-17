#ifndef NODE_H
#define NODE_H

#include<udp.h>
#include<logger.h>
#include<pflx.h>

typedef struct{
    size_t processId;
    size_t CurrentMessageId;

    pflx* socket;

    Logger* logger;
}Node;

// main loop of a node
int nodeLoop(Node* n);

// construct node
Node* init_node(size_t id);

// desctructor node
int destroy_node(Node* n);

#endif
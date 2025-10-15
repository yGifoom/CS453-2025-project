#include<udp.h>
#include<logger.h>
#include<pflx.h>

typedef struct{
    size_t id;
    size_t CurrentMessageId;

    pflx* socket;
    sockaddr_in* phonebook;

    logger* logger;
}node;

int nodeLoop(node* n);
// main loop of a node

int addConnections(node* n, sockaddr_in* phonebook);
// saves all addresses of nodes on the network
// position in the array is equal to the id of node, value is corresponding to the 
// node of that ID

node* init_node(size_t id);
// construct node

int destroy_node(node* n);
// desctructor node

#include<udp.h>

typedef struct{
    UDP* udpSocket;
}pflx;

int sendPflx(pflx* pflx, void* buffer, size_t processId);
// send with perfect link properties
// downcalled from node

int deliver(pflx* pflx, void* buffer, size_t buffSize, size_t processId);
// deliver a message
// called by recv

int recvPflx(pflx* pflx, void* buffer, size_t buffSize); 
// recieve a message following the perfect link properties
// upcalled by udp

pflx* init_pflx(UDP* udpSocket);
// construct perfect link

int destroy_pflx(pflx* udp);
// destroy perfect link
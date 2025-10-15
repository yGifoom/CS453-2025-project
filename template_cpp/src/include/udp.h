
#include <arpa/inet.h>
#include <time.h>
#include <sys/socket.h>

typedef struct{
    int sockfd;
    sockaddr_in addr;
}UDP;

int recvUdp(UDP *udp, void *buffer, size_t bufferSize);
// recieve with udp  
// upcalles pflx.recvPflx

int sendUdp(UDP *udp, sockaddr_in addr, int port, const void *message);
// send with udp
// downcalled by pflx.sendPflx

UDP* init_udp(int port);
// construct udp socket

int destroy_udp(UDP* udp);
// destroy udp socket
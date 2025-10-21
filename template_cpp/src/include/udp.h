#ifndef UDP_H
#define UDP_H

#include <arpa/inet.h>
#include <time.h>
#include <sys/socket.h>

typedef struct{
    int sockfd;
    struct sockaddr_in addr;
}UDP;

// send with udp
// downcalled by pflx.pflx_send
int udp_send(UDP *udp, const char* ip, short unsigned port, const void *message);

// recieve with udp  
// upcalles pflx.pflx_recv
ssize_t udp_recv(UDP *udp, void* buffer, size_t buffer_size);
ssize_t udp_recv_timeout(UDP *udp, void* buffer, size_t buffer_size, int timeout_ms);

// construct udp socket
UDP* udp_init(short unsigned port);

// destroy udp socket
int udp_destroy(UDP* udp);

#endif // UDP_H
#include"udp.h"
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<unistd.h>
#include<sys/select.h>
#include<errno.h>


// ---------- UDP FUNCTIONS ----------

int udp_send(UDP *udp, const char* ip, short unsigned port, const void* message, size_t message_size) {
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &dest_addr.sin_addr);

    ssize_t res = sendto(udp->sockfd, message, message_size, 0,
           (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    return (int)res;
}

ssize_t udp_recv(UDP *udp, void* buffer, size_t buffer_size) {
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    ssize_t len = recvfrom(udp->sockfd, buffer, buffer_size, 0,
                           (struct sockaddr *)&sender_addr, &addr_len);
    return len;
}

ssize_t udp_recv_timeout(UDP *udp, void* buffer, size_t buffer_size, int timeout_ms) {
    fd_set read_fds;
    struct timeval timeout;
    
    FD_ZERO(&read_fds);
    FD_SET(udp->sockfd, &read_fds);
    
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    int ret = select(udp->sockfd + 1, &read_fds, NULL, NULL, &timeout);
    
    if (ret < 0) {
        perror("select");
        return -1;
    } else if (ret == 0) {
        // Timeout occurred
        return 0;
    }
    
    // Data is available, proceed with recv
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    ssize_t len = recvfrom(udp->sockfd, buffer, buffer_size, 0,
                           (struct sockaddr *)&sender_addr, &addr_len);
    return len;
}

UDP *udp_init(short unsigned port) {
    UDP *udp = malloc(sizeof(UDP));
    udp->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp->sockfd < 0) {
        perror("socket");
        free(udp);
        exit(EXIT_FAILURE);
    }

    memset(&udp->addr, 0, sizeof(udp->addr));
    udp->addr.sin_family = AF_INET;
    udp->addr.sin_addr.s_addr = INADDR_ANY;
    udp->addr.sin_port = htons(port);

    if (bind(udp->sockfd, (struct sockaddr *)&udp->addr, sizeof(udp->addr)) < 0) {
        perror("bind");
        close(udp->sockfd);
        free(udp);
        exit(EXIT_FAILURE);
    }

    return udp;
}

int udp_destroy(UDP *udp) {
    close(udp->sockfd);
    free(udp);
    return 0;
}
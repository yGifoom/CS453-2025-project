#include<node.h>

// ---------- NODE FUNCTIONS ----------

void node_send(Node *node, const char *message) {
    udp_send(node->udp, node->peer_ip, node->peer_port, message);
}

void node_listen(Node *node) {
    char buffer[BUFFER_SIZE];
    time_t last_flush = time(NULL);
    time_t last_send = time(NULL);

    printf("[Node %d] Listening on port %d...\n", node->id, ntohs(node->udp->addr.sin_port));

    while (1) {
        // Non-blocking receive with small timeout
        struct timeval tv = {1, 0};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(node->udp->sockfd, &fds);
        int activity = select(node->udp->sockfd + 1, &fds, NULL, NULL, &tv);

        if (activity > 0 && FD_ISSET(node->udp->sockfd, &fds)) {
            ssize_t len = udp_receive(node->udp, buffer, BUFFER_SIZE);
            if (len > 0) {
                printf("[Node %d] Received: %s\n", node->id, buffer);
                logger_add(&node->logger, buffer);
            }
        }

        time_t now = time(NULL);

        // Periodically send message
        if (difftime(now, last_send) >= SEND_INTERVAL) {
            node_broadcast(node);
            last_send = now;
        }

        // Periodically flush log
        if (difftime(now, last_flush) >= FLUSH_INTERVAL) {
            logger_flush(&node->logger);
            last_flush = now;
        }
    }
}

void node_init(size_t id, int port, const char *logfile) {
    Node* node = malloc(sizeof(Node))

    node->id = id;
    node->socket = pflx_create(port);
    logger_init(&node->logger, logfile);
    strncpy(node->peer_ip, peer_ip, INET_ADDRSTRLEN);
    node->peer_port = peer_port;
}

void node_destroy(Node *node) {
    logger_destroy(&node->logger);
    pflx_destroy(node->socket);
}
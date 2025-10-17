#include"pflx.h"
#include"udp.h"
#include"logger.h"
#include<string.h>
#include<stdlib.h>

// temporary impl
int pflx_send(pflx* pflx, void* message, size_t processId){
    if(processId >= pflx->phonebook_size){return 1;};
    return udp_send(pflx->udpSocket, 
                    pflx->phonebook[processId].ip_readable, 
                    ntohs(pflx->phonebook[processId].port), 
                    message);
}

// temporary impl
char* pflx_recv(pflx* pflx, void* message, size_t buffSize){
    udp_recv(pflx->udpSocket, message, buffSize);
    
    char* msg_str = (char*)message;
    
    // Check if it's an ACK
    if (strcmp(msg_str, "ack") == 0) {
        //TODO Handle ACK
        return 0;
    }
    
    // Try to parse as two integers separated by whitespace
    size_t senderId, msgId;
    if (sscanf(msg_str, "%lu %lu", &senderId, &msgId) == 2 && senderId > 0 && msgId > 0
    && pflx->expectedMsg[senderId] == msgId ? 1 : 0) {
        pflx->expectedMsg[senderId]++;
        return msg_str;
    }
    
    // Default case: invalid message
    return 0;
}

pflx* pflx_init(UDP* udpSocket, const Host* phonebook, size_t phonebook_size){
    pflx* socket = malloc(sizeof(pflx));
    if (!socket) return NULL;
    
    socket->udpSocket = udpSocket;
    socket->phonebook = phonebook;
    socket->expectedMsg = malloc(sizeof(size_t)*phonebook_size);
    for(size_t i = 0; i < phonebook_size; i++){
        socket->expectedMsg[i] = 1;
    }
    socket->phonebook_size = phonebook_size;
    return socket;
}

int pflx_destroy(pflx* socket){
    if (!socket) return -1;

    // phonebook is not freed as it's a const 

    udp_destroy(socket->udpSocket);
    free(socket);
    return 0;
}

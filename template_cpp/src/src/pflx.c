#include"pflx.h"
#include"udp.h"
#include"logger.h"
#include<string.h>
#include<stdlib.h>

// temporary impl
int pflx_send(pflx* pflx, void* message, size_t processId){
    // processId is 1-based, convert to 0-based array index
    if(processId < 1 || processId > pflx->phonebook_size){
        return -1;
    }
    size_t index = processId - 1;
    return udp_send(pflx->udpSocket, 
                    pflx->phonebook[index].ip_readable, 
                    ntohs(pflx->phonebook[index].port), 
                    message);
}

// temporary impl
// returns for pflx: ("%lu %lu", senderid msgId)
char* pflx_recv(pflx* pflx, void* message, size_t buffSize){
    ssize_t len = udp_recv_timeout(pflx->udpSocket, message, buffSize, 1000); // 1 second timeout
    
    if (len == 0) {
        // Timeout occurred
        return NULL;
    }
    
    if (len < 0) {
        // Error occurred
        return NULL;
    }
    
    char* msg_str = (char*)message;
    
    // Check if it's an ACK
    if (strcmp(msg_str, "ack") == 0) {
        //TODO Handle ACK
        return NULL;
    }
    
    // Try to parse as two integers separated by whitespace
    size_t senderId, msgId;
    if (sscanf(msg_str, "%lu %lu", &senderId, &msgId) == 2 
        && senderId < pflx->phonebook_size 
        && msgId > 0
        && pflx->expectedMsg[senderId] == msgId) {
        pflx->expectedMsg[senderId]++;
        return msg_str;
    }
    
    // Default case: invalid message
    return NULL;
}

pflx* pflx_init(short unsigned port, const Host* phonebook, size_t phonebook_size){
    pflx* socket = malloc(sizeof(pflx));
    if (!socket) return NULL;
    
    UDP* udpSocket = udp_init(port);

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

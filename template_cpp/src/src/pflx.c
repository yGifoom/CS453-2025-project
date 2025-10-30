#include"pflx.h"
#include"udp.h"
#include"logger.h"
#include<string.h>
#include<stdlib.h>

int pflx_send(pflx* pflx, void* message, size_t messageSize, size_t processId){
    pflx_message* msg = pflx_message_init(message, messageSize, processId, pflx->ownMessageID);
    int res = queue_push(pflx->outQueue, msg, sizeof(msg));
    if (res != 0){
        return res;
    }

    pflx->ownMessageID++;
    return 0;
}

// temporary impl
int _pflx_send_routine(pflx* pflx){
    /*// processId is 1-based, convert to 0-based array index
    if(processId < 1 || processId > pflx->phonebook_size){
        return -1;
    }
    size_t index = processId - 1;
    return udp_send(pflx->udpSocket, 
                    pflx->phonebook[index].ip_readable, 
                    ntohs(pflx->phonebook[index].port), 
                    message);*/
    return 1;
}

// temporary impl
// returns for pflx: ("%lu %lu", senderid msgId)
int pflx_recv(pflx* pflx, void* message, size_t buffSize){
    int res = queue_pop(pflx->outQueue, &message, &buffSize);
    if(res != 0){
        return 1;
    }

    return 0;
}

int _pflx_recv_routine(pflx* pflx){
    /*ssize_t len = udp_recv_timeout(pflx->udpSocket, message, buffSize, 1000); // 1 second timeout
    
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
    return NULL;*/
    return 1;
}

pflx* pflx_init(short unsigned port, const Host* phonebook, size_t phonebook_size){
    pflx* socket = malloc(sizeof(pflx));
    if (!socket) return NULL;
    
    UDP* udpSocket = udp_init(port);

    socket->inQueue = queue_init();
    socket->outQueue = queue_init();


    socket->udpSocket = udpSocket;
    socket->phonebook = phonebook;
    socket->ownMessageID = 0;
    socket->expectedConsequentAck = malloc(sizeof(size_t)*phonebook_size);
    for(size_t i = 0; i < phonebook_size; i++){
        socket->expectedConsequentAck[i] = 1;
    }
    socket->phonebook_size = phonebook_size;
    return socket;
}

int pflx_destroy(pflx* socket){
    if (!socket) return -1;

    // phonebook is not freed as it's a const 

    udp_destroy(socket->udpSocket);
    queue_destroy(socket->inQueue); queue_destroy(socket->outQueue);
    free(socket->expectedConsequentAck);
    free(socket);
    return 0;
}

pflx_message* pflx_message_init(void* message, size_t messageSize, size_t processID, size_t messageID){
    pflx_message* msg = malloc(sizeof(pflx_message));

    if (msg == NULL){
        return NULL;
    }

    void* messageCopy = malloc(messageSize);
    if (messageCopy == NULL){
        return NULL;
    }

    memcpy(messageCopy, message, sizeof(messageSize));
    if (messageCopy == NULL){
        return NULL;
    }

    msg->message = messageCopy;
    msg->messageSize = messageSize;

    msg->messageID = messageID;
    msg->processID - processID;

    return msg;
}

int pflx_message_destroy(pflx_message* message){
    free(message->message);
    free(message);

    return 0;
}
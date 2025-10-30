#include"pflx.h"
#include"pflx_threads_entry.h"
#include"udp.h"
#include"logger.h"
#include<string.h>
#include<stdlib.h>
#include<stdio.h>

const int BUFFERSIZE = 256;

int pflx_start(pflx* pflx){
    _pflx_threads_entry* e = _thr_get(pflx, 1);
    if (!e) return -1;

    // Start receiver first
    if (pthread_create(&e->recv_tid, NULL, _recv_thread_main, pflx) != 0) {
        return -1;
    }
    e->recv_started = 1;

    // Start sender
    if (pthread_create(&e->send_tid, NULL, _send_thread_main, pflx) != 0) {
        // rollback receiver
        pthread_cancel(e->recv_tid);
        pthread_join(e->recv_tid, NULL);
        e->recv_started = 0;
        return -1;
    }
    e->send_started = 1;

    return 0;
}

int pflx_stop(pflx* pflx){
    _pflx_threads_entry* e = _thr_get(pflx, 0);
    if (!e) return -1;

    // Cancel and join both threads
    if (e->send_started) pthread_cancel(e->send_tid);
    if (e->recv_started) pthread_cancel(e->recv_tid);

    if (e->send_started) { pthread_join(e->send_tid, NULL); e->send_started = 0; }
    if (e->recv_started) { pthread_join(e->recv_tid, NULL); e->recv_started = 0; }

    _thr_remove(pflx);
    return 0;
}

int pflx_send(pflx* pflx, void* message, size_t messageSize, size_t originID, size_t targetID){
    pflx_message* msg = pflx_message_init(message, messageSize, originID, pflx->ownMessageID, targetID);
    int res = queue_push(pflx->downQueue, msg, sizeof(msg)); // push pointer value
    if (res != 0){
        return res;
    }

    pflx->ownMessageID++;
    return 0;
}

// main ruotine for sending 
int _pflx_send_routine(pflx* pflx){
    // init
    void* buffer = malloc(BUFFERSIZE);
    size_t dataSize;

    while (1){
        int res = queue_pop(pflx->downQueue, &buffer, &dataSize);
        if (res != 0){
            return res;
        }
        pflx_message* msg_to_send = (pflx_message*)buffer;

        size_t ack_msg_id;
        int lenMessageSent;
        size_t index = msg_to_send->targetID - 1;

        // check if message was ack
        if (msg_to_send->message && 
            sscanf((char*)msg_to_send->message, "ack %zu", &ack_msg_id) == 1) {
            // strictly reciever behaviour
            lenMessageSent = udp_send(pflx->udpSocket, 
                                    pflx->phonebook[index].ip_readable, 
                                    ntohs(pflx->phonebook[index].port), 
                                    msg_to_send);
            if (lenMessageSent < 0){
                // send failed, we might want to handle it
            }

            
        } else{ // I assume that all messages on the queues are well formatted
            // strictly sender behaviour
            int ok = bst_set_lookup(pflx->nonConsequentAcks[index], msg_to_send->messageID);
            if(ok || ack_read(&pflx->expectedConsequentAck[index]) > msg_to_send->messageID){
                // already acked -> drop
                continue;
            } else{
                lenMessageSent = udp_send(pflx->udpSocket, 
                                    pflx->phonebook[index].ip_readable, 
                                    ntohs(pflx->phonebook[index].port), 
                                    msg_to_send);

                // put message back in, we will be waiting for ack
                res = queue_push(pflx->downQueue, msg_to_send, sizeof(msg_to_send));
                if (res != 0){
                    return res;
                }

                if (lenMessageSent < 0){
                    // send failed, or queue push failed we might want to handle it
                }
            }
        }
    }
    return 1;
}

// returns what is deliverable: ("%zu %zu", senderid msgId)
int pflx_recv(pflx* pflx, void* message, size_t buffSize){
    int res = queue_pop(pflx->upQueue, &message, &buffSize);
    if(res != 0){
        return 1;
    }

    return 0;
}

int _pflx_recv_routine(pflx* pflx){
    // init
    void* buffer = malloc(BUFFERSIZE);

    while(1){
        ssize_t len = udp_recv_timeout(pflx->udpSocket, buffer, BUFFERSIZE, 1000); // 1 second timeout
        
        if (len == 0) {
            // Timeout occurred
            continue;
        }
        
        if (len < 0) {
            // Error occurred
            return 1;
        }
        
        pflx_message* msg_recvd = NULL;
        memcpy(msg_recvd, buffer, BUFFERSIZE);
        if(msg_recvd == NULL) {
            return 1;
        }
        
        size_t ack_msg_id;
        size_t origin_index = msg_recvd->originID - 1;
        size_t target_index = msg_recvd->targetID - 1;
        size_t senderId, msgId;

        // Check if it's an ACK
        if (sscanf((char*)msg_recvd->message, "ack %zu", &ack_msg_id) == 1) {
            // strictly sender behaviour
            // check if ack updates our ack statuses
            if(ack_read(&pflx->expectedConsequentAck[target_index]) == msg_recvd->messageID){
                size_t removed = bst_set_compact_consequent(
                    pflx->nonConsequentAcks[target_index],
                    ack_read(&pflx->expectedConsequentAck[target_index]) + 1);
                ack_write(&pflx->expectedConsequentAck[target_index],
                              msg_recvd->messageID + removed + 1);

                // deliver message
                queue_push(pflx->upQueue, msg_recvd->message, msg_recvd->messageSize);
                continue;
            } else{
                int res = bst_set_add(pflx->nonConsequentAcks[target_index], msg_recvd->messageID);
                if(res == -1){
                    return res;
                }
                if(res == 0){
                    queue_push(pflx->upQueue, msg_recvd->message, msg_recvd->messageSize);
                }
            }

        // Try to parse as two integers separated by whitespace
        } else if (sscanf(msg_recvd->message, "%lu %lu", &senderId, &msgId) == 2  
            && senderId < pflx->phonebook_size 
            && msgId > 0){
            // strictly reciever behaviour
            if(ack_read(&pflx->expectedConsequentAck[origin_index]) == msg_recvd->messageID){
                size_t removed = bst_set_compact_consequent(
                    pflx->nonConsequentAcks[origin_index],
                    ack_read(&pflx->expectedConsequentAck[origin_index]) + 1);

                ack_write(&pflx->expectedConsequentAck[origin_index],
                            msg_recvd->messageID + removed + 1);

                // deliver message
                queue_push(pflx->upQueue, msg_recvd->message, msg_recvd->messageSize);
            }
            else{
                // non consequent, let's save it
                int res = bst_set_add(pflx->nonConsequentAcks[origin_index], msg_recvd->messageID);
                if (res == -1){
                    return res;
                }
                if (res == 0){
                    queue_push(pflx->upQueue, msg_recvd->message, msg_recvd->messageSize);
                }
            }
            // sending ack back regardless
            sprintf(buffer, "ack %zu", msg_recvd->messageID);
            pflx_send(pflx, buffer, BUFFERSIZE, msg_recvd->targetID, msg_recvd->originID);
        }
    }
    return 1;
}

pflx* pflx_init(short unsigned port, const Host* phonebook, size_t phonebook_size){
    pflx* socket = malloc(sizeof(pflx));
    if (!socket) return NULL;
    
    UDP* udpSocket = udp_init(port);

    socket->upQueue = queue_init();
    socket->downQueue = queue_init();
    // allocate array of bst_set* of length phonebook_size
    socket->nonConsequentAcks = malloc(sizeof(bst_set*) * phonebook_size);
    for(size_t i = 0; i < phonebook_size; i++){
        socket->nonConsequentAcks[i] = bst_set_init();
    }

    socket->udpSocket = udpSocket;
    socket->phonebook = phonebook;
    socket->ownMessageID = 1; // start at 1 to match expectedConsequentAck default

    // Init expectedConsequentAck as ack_state array
    socket->expectedConsequentAck = malloc(sizeof(ack_state)*phonebook_size);
    for(size_t i = 0; i < phonebook_size; i++){
        pthread_mutex_init(&socket->expectedConsequentAck[i].mutex, NULL);
        socket->expectedConsequentAck[i].value = 1;
    }

    socket->phonebook_size = phonebook_size;
    return socket;
}

int pflx_destroy(pflx* socket){
    if (!socket) return -1;

    // phonebook is not freed as it's a const 

    udp_destroy(socket->udpSocket);
    queue_destroy(socket->upQueue); queue_destroy(socket->downQueue);

    // Destroy ack_state array
    for(size_t i = 0; i < socket->phonebook_size; i++){
        pthread_mutex_destroy(&socket->expectedConsequentAck[i].mutex);
    }
    free(socket->expectedConsequentAck);

    for(size_t i = 0; i < socket->phonebook_size ; i++){
        bst_set_destroy(socket->nonConsequentAcks[i]);
    }
    free(socket->nonConsequentAcks);
    free(socket);
    return 0;
}

pflx_message* pflx_message_init(void* message, size_t messageSize, size_t originID, size_t messageID, size_t targetID){
    pflx_message* msg = malloc(sizeof(pflx_message));

    if (msg == NULL){
        return NULL;
    }

    void* messageCopy = malloc(messageSize);
    if (messageCopy == NULL){
        free(msg);
        return NULL;
    }

    memcpy(messageCopy, message, messageSize);

    msg->message = messageCopy;
    msg->messageSize = messageSize;

    msg->messageID = messageID;
    msg->originID = originID;
    msg->targetID = targetID;

    return msg;
}

int pflx_message_destroy(pflx_message* message){
    free(message->message);
    free(message);

    return 0;
}

// Thread-safe accessors
size_t ack_read(ack_state* s){
    if (!s) return 0;
    pthread_mutex_lock(&s->mutex);
    size_t v = s->value;
    pthread_mutex_unlock(&s->mutex);
    return v;
}
void ack_write(ack_state* s, size_t v){
    if (!s) return;
    pthread_mutex_lock(&s->mutex);
    s->value = v;
    pthread_mutex_unlock(&s->mutex);
}
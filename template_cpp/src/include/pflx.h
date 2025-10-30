#ifndef PFLX_H
#define PFLX_H

#include"udp.h"
#include"queue.h"
#include"bst_set.h"
#include"parser.h"
#include <pthread.h>

// New thread-safe state for expectedConsequentAck
typedef struct {
    size_t value;
    pthread_mutex_t mutex;
} ack_state;

typedef struct {
    UDP* udpSocket;
    queue_t* upQueue;
    queue_t* downQueue;
    

    const Host* phonebook;
    size_t ownMessageID;
    ack_state* expectedConsequentAck; // was size_t*
    bst_set** nonConsequentAcks; // array of maps of size phonebook_size
    
    size_t phonebook_size;
} pflx;

// start up the network routines
int pflx_start(pflx* pflx);

// gently stops all routines in network
int pflx_stop(pflx* pflx);

// send with perfect link properties
// will push in downQueue
// downcalled from nodeloop
// processId is 1-based (matches Host IDs in parser)
int pflx_send(pflx* pflx, void* message, size_t messageSize, size_t originID, size_t targetID);

// recieve a message following the perfect link properties
// pops the buffer the pop of upQueue, puts result in buffer
// upcalled by udp
int pflx_recv(pflx* pflx, void* buffer, size_t buffSize); 

// continuously check in downQueue
// if there should be messages to be sent
// sends them and goes to the next one
// messages are not pushed back into the queue 
// only when their ack is found in the state of pflx
int _pflx_send_routine(pflx* pflx);

// continuously check upd socket
// puts back on the downQueue acks
// pushes in upQueue if the message is to be delivered
int _pflx_recv_routine(pflx* pflx);

// construct perfect link
pflx* pflx_init(short unsigned int port, const Host* phonebook, size_t phonebook_size);

// destroy perfect link
int pflx_destroy(pflx* udp);

typedef struct{
    void* message;
    size_t messageSize;

    size_t originID;
    size_t messageID;

    size_t targetID;
} pflx_message;

// initializes message in the pflx format
pflx_message* pflx_message_init(void* message, size_t messageSize, size_t originID, size_t messageID, size_t targetID);

// destroys message in the pflx format
int pflx_message_destroy(pflx_message* message);

// Thread-safe accessors for ack_state
size_t ack_read(ack_state* s);

void ack_write(ack_state* s, size_t v);

#endif // PFLX_H
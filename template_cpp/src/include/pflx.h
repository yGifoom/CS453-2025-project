#ifndef PFLX_H
#define PFLX_H

#include"udp.h"
#include"queue.h"

#include"parser.h"

typedef struct {
    UDP* udpSocket;
    queue_t* inQueue;
    queue_t* outQueue;
    

    const Host* phonebook;
    size_t ownMessageID;
    size_t* expectedConsequentAck;
    
    size_t phonebook_size;
} pflx;

// send with perfect link properties
// will put in a processing datastructure
// downcalled from nodeloop
// processId is 1-based (matches Host IDs in parser)
int pflx_send(pflx* pflx, void* buffer, size_t messageSize, size_t processId);

// recieve a message following the perfect link properties
// returns pointr of what is to be delivered, 
// if not to be delivered returns null pointer
// upcalled by udp
int pflx_recv(pflx* pflx, void* buffer, size_t buffSize); 

// continuously check in datastructure of unprocessed 
// outgoing messages
// if there should be messages to be sent
// sends them and goes to the next one
int _pflx_send_routine(pflx* pflx);

// continuously check in datastructure of unprocessed
// ingoing messages
// if there's any message in the outgoing datastruct
// that has been accepted
// save in the to be delivered struct the newly accepted 
// message
int _pflx_recv_routine(pflx* pflx);

// construct perfect link
pflx* pflx_init(short unsigned int port, const Host* phonebook, size_t phonebook_size);

// destroy perfect link
int pflx_destroy(pflx* udp);

typedef struct{
    void* message;
    size_t messageSize;

    size_t processID;
    size_t messageID;
} pflx_message;

pflx_message* pflx_message_init(void* message, size_t messageSize, size_t processID, size_t messageID);

int pflx_message_destroy(pflx_message* message);

#endif // PFLX_H
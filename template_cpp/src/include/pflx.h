#ifndef PFLX_H
#define PFLX_H

#include"udp.h"
#include"parser.h"

typedef struct {
    UDP* udpSocket;

    const Host* phonebook;
    size_t* expectedMsg;
    size_t phonebook_size;
} pflx;

// send with perfect link properties
// will return on timeout or recv of ack
// downcalled from nodeloop
// processId is 1-based (matches Host IDs in parser)
int pflx_send(pflx* pflx, void* buffer, size_t processId);

// recieve a message following the perfect link properties
// returns pointr of what is to be delivered, 
// if not to be delivered returns null pointer
// upcalled by udp
char* pflx_recv(pflx* pflx, void* buffer, size_t buffSize); 

// construct perfect link
pflx* pflx_init(short unsigned int port, const Host* phonebook, size_t phonebook_size);

// destroy perfect link
int pflx_destroy(pflx* udp);

#endif // PFLX_H
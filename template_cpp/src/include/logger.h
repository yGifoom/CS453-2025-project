
#include<stddef.h>
#include<stdio.h>

typedef struct{
    char** messages;
    size_t count; // in bytes
    size_t capacity; // in bytes
    FILE* outputFile;
    int debug;
} Logger;

// add an entry of a log to the internal state of logger
// called by pflx.deliver
int logger_add(Logger* l, char* log);

// write internal state of logger onto logger->outputFile, clear internal state
// called periodically
// can be called asynchronously by signal interrupt
int logger_flush();

// constructor of logger
// debug == 1 means debugger active, otherwise inactive
Logger* logger_init(const char* outputfile, int debug);

// destroy logger
int logger_destroy(Logger* l);
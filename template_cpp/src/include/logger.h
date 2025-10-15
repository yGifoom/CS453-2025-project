
#include<stddef.h>
#include<stdio.h>

typedef struct{
    char** messages;
    size_t count;
    size_t capacity;
    FILE* outputFile;
    bool debug;
} logger;

int addLog(char* log);
// add an entry of a log to the internal state of logger
// called by pflx.deliver

int dump();
// write internal state of logger onto logger->outputFile, clear internal state
// called periodically
// can be called asynchronously by signal interrupt

logger* init_logger(FILE* outputfile, bool debug);
// constructor of logger

int destroy_logger(logger* l);
// destroy logger
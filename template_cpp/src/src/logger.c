#include"utils.h"
#include"logger.h"
#include"parser.h"
#include<stdlib.h>

// ---------- LOGGER FUNCTIONS ----------

int logger_add(Logger *logger, char *log) {
    if (logger->count >= logger->capacity) {
        logger->capacity *= 2;
        logger->messages = realloc(logger->messages, sizeof(char *) * logger->capacity);
    }
    logger->messages[logger->count++] = my_strdup(log);

    return 0;
}

int logger_flush(Logger *logger) {
    for (size_t i = 0; i < logger->count; i++) {
        // prints to output file with newlines
        fprintf(logger->outputFile, "%s\n", logger->messages[i]);
        free(logger->messages[i]);
    }
    fflush(logger->outputFile);
    logger->count = 0;

    return 0;
}

Logger* logger_init(const char* outputFile, int debug) {
    Logger* l = malloc(sizeof(Logger));
    size_t cap = 10 * sizeof(char); // standard capacity of logger 

    l->messages = malloc(sizeof(char *) * cap);
    l->count = 0;
    l->capacity = cap;
    l->outputFile = fopen(outputFile, "a");
    if (!l->outputFile) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    return l;
}

int logger_destroy(Logger *logger) {
    logger_flush(logger);
    fclose(logger->outputFile);
    free(logger->messages);
    free(logger);
    return 0;
}
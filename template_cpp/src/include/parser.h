#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include<stddef.h>

typedef struct {
    uint32_t id;
    uint32_t ip;
    uint16_t port;
    char* ip_readable;
    char* port_readable;
} Host;

typedef struct Parser {
    int argc;
    char** argv;
    uint32_t my_id;
    Host* hosts;
    size_t hosts_count;
    char* output_path;
    char* config_path;
} Parser;

// Initialize parser with command line arguments
Parser* parser_create(int argc, char** argv);

// Parse the arguments and config files
int parser_parse(Parser* parser);

// Getters
uint32_t parser_get_id(const Parser* parser);
const Host* parser_get_hosts(const Parser* parser, size_t* count);
const char* parser_get_output_path(const Parser* parser);
const char* parser_get_config_path(const Parser* parser);

// Cleanup
void parser_destroy(Parser* parser);

#ifdef __cplusplus
}
#endif
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "parser.h"
#include "hello.h"

static void stop(int sig) {
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    
    printf("Immediately stopping network packet processing.\n");
    printf("Writing output.\n");
    
    exit(0);
}

int main(int argc, char** argv) {
    signal(SIGTERM, stop);
    signal(SIGINT, stop);
    
    Parser* parser = parser_create(argc, argv);
    if (!parser) {
        fprintf(stderr, "Failed to create parser\n");
        return 1;
    }
    
    if (parser_parse(parser) != 0) {
        fprintf(stderr, "Failed to parse arguments\n");
        parser_destroy(parser);
        return 1;
    }
    
    hello();
    printf("\n");
    
    printf("My PID: %d\n", getpid());
    printf("From a new terminal type `kill -SIGINT %d` or `kill -SIGTERM %d` to stop processing packets\n\n", 
           getpid(), getpid());
    
    printf("My ID: %u\n\n", parser_get_id(parser));
    
    printf("List of resolved hosts is:\n");
    printf("==========================\n");
    
    size_t hosts_count;
    const Host* hosts = parser_get_hosts(parser, &hosts_count);
    for (size_t i = 0; i < hosts_count; i++) {
        printf("%u\n", hosts[i].id);
        printf("Human-readable IP: %s\n", hosts[i].ip_readable);
        printf("Machine-readable IP: %u\n", hosts[i].ip);
        printf("Human-readable Port: %s\n", hosts[i].port_readable);
        printf("Machine-readable Port: %u\n", hosts[i].port);
        printf("\n");
    }
    printf("\n");
    
    printf("Path to output:\n");
    printf("===============\n");
    printf("%s\n\n", parser_get_output_path(parser));
    
    printf("Path to config:\n");
    printf("===============\n");
    printf("%s\n\n", parser_get_config_path(parser));
    
    printf("Doing some initialization...\n\n");
    printf("Broadcasting and delivering messages...\n\n");

    
    parser_destroy(parser);
    return 0;
}
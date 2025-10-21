#include"logger.h"
#include"parser.h"
#include"udp.h"
#include"pflx.h"
#include"tests.h"
#include"node.h"
#include"utils.h"
#include<stdlib.h>
#include<pthread.h>
#include<string.h>

// test for correctness of logger
void testLogger(char* res, Parser* parser){

    // Get output path from parser
    const char* output_path = parser_get_output_path(parser);
    if (!output_path) {
        strcpy(res, "fail");
        return;
    }
    
    // Initialize logger with output file
    FILE* output_file = fopen(output_path, "w");
    if (!output_file) {
        strcpy(res, "fail");
        return;
    }
    fclose(output_file);
    
    Logger* logger = logger_init(output_path, 0);
    if (!logger) {
        strcpy(res, "fail");
        return;
    }
    
    // Add 5 test logs
    logger_add(logger, "b 1");
    logger_add(logger, "b 2");
    logger_add(logger, "d 1 1");
    logger_add(logger, "d 2 2");
    logger_add(logger, "b 3");
    
    // Flush the logger
    logger_flush(logger);
    logger_destroy(logger);
    
    // Read back the output file and verify
    FILE* verify_file = fopen(output_path, "r");
    if (!verify_file) {
        strcpy(res, "fail");
        return;
    }
    
    const char* expected[] = {
        "b 1",
        "b 2",
        "d 1 1",
        "d 2 2",
        "b 3"
    };
    
    char line[256];
    int line_num = 0;
    int passed = 1;
    
    while (fgets(line, sizeof(line), verify_file) && line_num < 5) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, expected[line_num]) != 0) {
            passed = 0;
            break;
        }
        line_num++;
    }
    
    fclose(verify_file);
    
    // Check if we got exactly 5 lines
    if (line_num != 5) {
        passed = 0;
    }
    
    strcpy(res, passed ? "pass" : "fail");

    size_t hosts_count;
    const Host* hosts = parser_get_hosts(parser, &hosts_count);
    // Cleanup
    // cleanup output log files before final test
    char filename[256];
    for(size_t i = 1; i <= hosts_count; i++){
        snprintf(filename, sizeof(filename), "../example/output/%zu.output", i);
        FILE* f = fopen(filename, "w");
        if (f) fclose(f);
    }
    
    return;
}

void testUdp(char* res, Parser* parser){
    // Get host information from parser
    size_t hosts_count;
    const Host* hosts = parser_get_hosts(parser, &hosts_count);
    
    if (hosts_count < 2) {
        strcpy(res, "fail - need at least 2 hosts");
        return;
    }
    
    // Initialize two UDP sockets on different ports
    short unsigned int port_a = ntohs(hosts[0].port);
    short unsigned int port_b = ntohs(hosts[1].port);
    
    UDP* udp_a = udp_init(port_a);
    UDP* udp_b = udp_init(port_b);
    
    if (!udp_a || !udp_b) {
        strcpy(res, "fail - socket init");
        if (udp_a) udp_destroy(udp_a);
        if (udp_b) udp_destroy(udp_b);
        return;
    }
    
    // Test message from A to B
    const char* test_message = "Hello from A";
    char buffer_b[256];
    memset(buffer_b, 0, sizeof(buffer_b));
    
    // A sends to B
    udp_send(udp_a, hosts[1].ip_readable, port_b, test_message);
    
    // B receives from A
    ssize_t received_len = udp_recv(udp_b, buffer_b, sizeof(buffer_b));
    
    if (received_len < 0 || strcmp(buffer_b, test_message) != 0) {
        strcpy(res, "fail - A to B transmission");
        udp_destroy(udp_a);
        udp_destroy(udp_b);
        return;
    }
    
    // Test ACK from B to A
    const char* ack_message = "ACK from B";
    char buffer_a[256];
    memset(buffer_a, 0, sizeof(buffer_a));
    
    // B sends ACK to A
    udp_send(udp_b, hosts[0].ip_readable, port_a, ack_message);
    
    // A receives ACK from B
    received_len = udp_recv(udp_a, buffer_a, sizeof(buffer_a));
    
    if (received_len < 0 || strcmp(buffer_a, ack_message) != 0) {
        strcpy(res, "fail - B to A ACK");
        udp_destroy(udp_a);
        udp_destroy(udp_b);
        return;
    }
    
    // Cleanup
    udp_destroy(udp_a);
    udp_destroy(udp_b);
    
    strcpy(res, "pass");
}

void testPflx(char* res, Parser* parser){
    // Get host information from parser
    Logger* logger = logger_init(parser_get_output_path(parser), 1);

    size_t hosts_count;
    const Host* hosts = parser_get_hosts(parser, &hosts_count);
    
    if (hosts_count < 2) {
        strcpy(res, "fail - need at least 2 hosts");
        return;
    }
    
    // Initialize two UDP sockets and pflx instances
    short unsigned port_a = ntohs(hosts[0].port);
    short unsigned port_b = ntohs(hosts[1].port);
    
    // Create pflx instances (A sends to B)
    pflx* pflx_a = pflx_init(port_a, hosts, hosts_count);
    pflx* pflx_b = pflx_init(port_b, hosts, hosts_count);
    
    if (!pflx_a || !pflx_b) {
        strcpy(res, "fail - pflx init");
        if (pflx_a) pflx_destroy(pflx_a);
        if (pflx_b) pflx_destroy(pflx_b);
        return;
    }
    
    void* buffer = malloc(256);
    void* result;
    int sendSig;
    // Test 1: Send valid messages from A (id=1) to B (id=2) using 1-based IDs
    for (int i = 1; i <= 3; i++) {
        snprintf((char*)buffer, 256, "1 %d", i);
        sendSig = pflx_send(pflx_a, buffer, 2); // Send to process 2 (B) - 1-based
        
        snprintf((char*)buffer, sizeof(buffer), "1 %d", i);
        sendSig = pflx_send(pflx_a, buffer, 1); // Send to process 1 (B)
        
        void* recv_buffer[256];
        memset(recv_buffer, 0, sizeof(recv_buffer));
        result = pflx_recv(pflx_b, recv_buffer, sizeof(recv_buffer));
        
        if (result == NULL) {
            strcpy(res, "fail - valid message not delivered");
            free(buffer);
            pflx_destroy(pflx_a);
            pflx_destroy(pflx_b);
            return;
        }
        if (sendSig < 0){
            logger_add(logger, "failed pflx send");
        }

        logger_add(logger, result);
    }
    logger_flush(logger);
    
    // Test 2: Send garbage message (should be rejected)
    strcpy(buffer, "garbage data");
    pflx_send(pflx_a, buffer, 2);
    
    char garbage_buffer[256];
    memset(garbage_buffer, 0, sizeof(garbage_buffer));
    result = pflx_recv(pflx_b, garbage_buffer, sizeof(garbage_buffer));
    
    if (result != NULL) {
        strcpy(res, "fail - garbage accepted");
        free(buffer);
        pflx_destroy(pflx_a);
        pflx_destroy(pflx_b);
        return;
    }
    
    // Test 3: Send repeated message (should be rejected)
    strcpy(buffer, "1 2"); // Message 2 already delivered
    sendSig = pflx_send(pflx_a, buffer, 2);
    
    char repeat_buffer[256];
    memset(repeat_buffer, 0, sizeof(repeat_buffer));
    result = pflx_recv(pflx_b, repeat_buffer, sizeof(repeat_buffer));
    
    if (result != NULL) {
        strcpy(res, "fail - duplicate message delivered");
        free(buffer);
        pflx_destroy(pflx_a);
        pflx_destroy(pflx_b);
        return;
    }

    // Test 4: Send next expected message (should succeed)
    strcpy(buffer, "1 4"); // Next expected message
    sendSig = pflx_send(pflx_a, buffer, 2);
    
    char next_buffer[256];
    memset(next_buffer, 0, sizeof(next_buffer));
    result = pflx_recv(pflx_b, next_buffer, sizeof(next_buffer));
    
    if (result == NULL) {
        strcpy(res, "fail - next expected message not delivered");
        free(buffer);
        pflx_destroy(pflx_a);
        pflx_destroy(pflx_b);
        return;
    }
    logger_add(logger, result);
    logger_flush(logger);

    // Cleanup
    free(buffer);
    pflx_destroy(pflx_a);
    pflx_destroy(pflx_b);
    
    char filename[256];
    for(size_t i = 1; i <= hosts_count; i++){
        snprintf(filename, sizeof(filename), "../example/output/%zu.output", i);
        FILE* f = fopen(filename, "w");
        if (f) fclose(f);
    }

    strcpy(res, "pass");
}

void testNodeSeq(char* res, Parser* parser) {
    // Get host information from parser
    size_t hosts_count;
    const Host* hosts = parser_get_hosts(parser, &hosts_count);
    
    if (hosts_count != 2) {
        strcpy(res, "fail - need 2 hosts");
        return;
    }
    
    const size_t NUM_MESSAGES = parser_get_num_messages(parser);
    if (NUM_MESSAGES == 0) {
        strcpy(res, "fail - no messages in config");
        return;
    }
    
    const size_t nodeId = parser_get_id(parser);

    // Create temporary log files
    const char* node_log = parser_get_output_path(parser);
    printf("initializing node....\n");
    
    // Initialize nodes using node_init
    Node* node = node_init(nodeId, NUM_MESSAGES, hosts, hosts_count, node_log);
    if (!node) {
        strcpy(res, "fail - node init");
        return;
    }
    printf("node initialized!\nstarting loop\n");
    

    // this will block indefinetly, or until it crashes
    node_loop(node);
    
    printf("loop finished! now checking results....\n");
    
    // Verify sender output
    FILE* node_file = fopen(node_log, "r");
    if (!node_file) {
        strcpy(res, "fail - cannot open node log");
        return;
    }
    
    char line[256];
    if(nodeId < hosts_count){
        for (size_t i = 1; i <= NUM_MESSAGES; i++) {
            if (!fgets(line, sizeof(line), node_file)) {
                strcpy(res, "fail - sender missing lines");
                fclose(node_file);
                return;
            }
            line[strcspn(line, "\n")] = 0;
            
            char expected[256];
            snprintf(expected, sizeof(expected), "b %zu", i);
            if (strcmp(line, expected) != 0) {
                strcpy(res, "fail - sender wrong format");
                fclose(node_file);
                return;
            }
        }
    }
    else{
        for (size_t i = 1; i <= NUM_MESSAGES; i++) {
            if (!fgets(line, sizeof(line), node_file)) {
                strcpy(res, "fail - reciever missing lines");
                fclose(node_file);
                return;
            }
            line[strcspn(line, "\n")] = 0;
            
            char expected[256];
            snprintf(expected, sizeof(expected), "d 1 %zu", i);
            if (strcmp(line, expected) != 0) {
                strcpy(res, "fail - reciever wrong format");
                fclose(node_file);
                return;
            }
        }
    }

    // Cleanup
    fclose(node_file);
    remove(node_log);

    strcpy(res, "pass");
}
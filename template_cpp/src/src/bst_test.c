#include"bst_set.h"
#include"parser.h"
#include "tests.h"
#include<string.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

// Helper for testing data integrity
static int verify_bst_property(bst_node* node, size_t min_key, size_t max_key) {
    if (!node) return 1;
    
    if (node->key <= min_key || node->key >= max_key) return 0;
    
    return verify_bst_property(node->left, min_key, node->key) &&
           verify_bst_property(node->right, node->key, max_key);
}

void* lookup_thread_func(void* arg);
void* add_thread_func(void* arg);

void* lookup_thread_func(void* arg) {
        bst_set* s = (bst_set*)arg;
        for (size_t i = 1; i <= 100; i++) {
            void* data = NULL;
            size_t data_size = 0;
            if (bst_set_lookup(s, i, &data, &data_size) != 1) {
                return (void*)1; // Fail
            }
        }
        return (void*)0; // Success
    }

void* add_thread_func(void* arg){
        bst_set* s = (bst_set*)arg;
        for (size_t i = 101; i <= 200; i++) {
            bst_set_add(s, i, NULL, 0);
        }
        return NULL;
    }

// Condition function for compact_consequent test
static int is_even_value(void* data, size_t data_size) {
    if (!data || data_size != sizeof(int)) return 0;
    int value = *(int*)data;
    return (value % 2 == 0) ? 1 : 0;
}

static int is_even_value_size_t(void* data, size_t data_size) {
    if (!data || data_size != sizeof(size_t)) return 0;
    size_t value = *(size_t*)data;
    return (value % 2 == 0) ? 1 : 0;
}

void testBstSet(char* res, Parser* parser) {
    // Test 1: Initialization
    bst_set* set = bst_set_init();
    if (set == NULL) {
        strcpy(res, "fail - bst_set init");
        return;
    }
    
    if (set->size != 0) {
        strcpy(res, "fail - initial size not 0");
        bst_set_destroy(set);
        return;
    }
    
    // Test 2: Add elements with data
    int val50 = 500;
    if (bst_set_add(set, 50, &val50, sizeof(int)) != 0) {
        strcpy(res, "fail - add first element with data");
        bst_set_destroy(set);
        return;
    }
    
    void* retrieved_data = NULL;
    size_t retrieved_size = 0;
    if (bst_set_lookup(set, 50, &retrieved_data, &retrieved_size) != 1) {
        strcpy(res, "fail - lookup added element with data");
        bst_set_destroy(set);
        return;
    }
    
    if (retrieved_size != sizeof(int) || *(int*)retrieved_data != 500) {
        strcpy(res, "fail - data mismatch after add");
        bst_set_destroy(set);
        return;
    }
    
    // Test 3: Add more elements with data and verify BST property
    size_t keys[] = {30, 70, 20, 40, 60, 80, 10, 25, 35};
    int values[] = {300, 700, 200, 400, 600, 800, 100, 250, 350};
    
    for (size_t i = 0; i < 9; i++) {
        if (bst_set_add(set, keys[i], &values[i], sizeof(int)) != 0) {
            strcpy(res, "fail - add element with data");
            bst_set_destroy(set);
            return;
        }
    }
    
    // Verify BST property
    if (!verify_bst_property(set->root, 0, SIZE_MAX)) {
        strcpy(res, "fail - BST property violated after adds");
        bst_set_destroy(set);
        return;
    }
    
    // Test 4: Verify all data is intact
    for (size_t i = 0; i < 9; i++) {
        retrieved_data = NULL;
        retrieved_size = 0;
        if (bst_set_lookup(set, keys[i], &retrieved_data, &retrieved_size) != 1) {
            strcpy(res, "fail - lookup element after multiple adds");
            bst_set_destroy(set);
            return;
        }
        if (retrieved_size != sizeof(int) || *(int*)retrieved_data != values[i]) {
            strcpy(res, "fail - data corrupted after multiple adds");
            bst_set_destroy(set);
            return;
        }
    }
    
    // Test 5: Delete and verify data integrity
    if (bst_set_delete(set, 20) != 0) {
        strcpy(res, "fail - delete existing element with data");
        bst_set_destroy(set);
        return;
    }
    
    if (!verify_bst_property(set->root, 0, SIZE_MAX)) {
        strcpy(res, "fail - BST property violated after delete");
        bst_set_destroy(set);
        return;
    }
    
    // Verify remaining data is still intact
    for (size_t i = 0; i < 9; i++) {
        if (keys[i] == 20) continue;
        
        retrieved_data = NULL;
        retrieved_size = 0;
        if (bst_set_lookup(set, keys[i], &retrieved_data, &retrieved_size) != 1) {
            strcpy(res, "fail - lookup after delete");
            bst_set_destroy(set);
            return;
        }
        if (*(int*)retrieved_data != values[i]) {
            strcpy(res, "fail - data corrupted after delete");
            bst_set_destroy(set);
            return;
        }
    }
    
    // Test 6: Delete root (node with 2 children) and verify data is moved correctly
    retrieved_data = NULL;
    retrieved_size = 0;
    if (bst_set_lookup(set, 50, &retrieved_data, &retrieved_size) != 1) {
        strcpy(res, "fail - lookup root before delete");
        bst_set_destroy(set);
        return;
    }
    
    if (bst_set_delete(set, 50) != 0) {
        strcpy(res, "fail - delete root with data");
        bst_set_destroy(set);
        return;
    }
    
    if (!verify_bst_property(set->root, 0, SIZE_MAX)) {
        strcpy(res, "fail - BST property violated after root delete");
        bst_set_destroy(set);
        return;
    }
    
    // Test 7: Test compact_consequent without condition
    bst_set_destroy(set);
    set = bst_set_init();
    
    // Add sequential elements 1-10 with data
    for (size_t i = 1; i <= 10; i++) {
        size_t val = i * 10;
        bst_set_add(set, i, &val, sizeof(size_t));
    }
    
    size_t lastCon = 0;
    size_t removed = bst_set_compact_consequent(set, lastCon, NULL);
    
    if (removed != 10) {
        strcpy(res, "fail - compact_consequent removed wrong count");
        bst_set_destroy(set);
        return;
    }
    
    if (set->size != 0) {
        strcpy(res, "fail - compact_consequent final size");
        bst_set_destroy(set);
        return;
    }
    
    // Test 8: Test compact_consequent with condition
    bst_set_destroy(set);
    set = bst_set_init();
    
    // Add sequential elements with even/odd values
    for (size_t i = 1; i <= 10; i++) {
        size_t val = i; // value equals key
        bst_set_add(set, i, &val, sizeof(size_t));
    }
    
    lastCon = 0;
    removed = bst_set_compact_consequent(set, lastCon, is_even_value_size_t);
    
    // Should remove 0 elements because key=1 has value=1 (odd)
    if (removed != 0) {
        strcpy(res, "fail - compact_consequent with condition count");
        bst_set_destroy(set);
        return;
    }
    
    // Try with keys starting at 2 (even values)
    bst_set_destroy(set);
    set = bst_set_init();
    
    for (size_t i = 2; i <= 10; i++) {
        size_t val = i;
        bst_set_add(set, i, &val, sizeof(size_t));
    }
    
    lastCon = 1;
    removed = bst_set_compact_consequent(set, lastCon, is_even_value_size_t);
    
    // Should remove key=2 (value=2, even), then stop at key=3 (value=3, odd)
    if (removed != 1) {
        strcpy(res, "fail - compact_consequent conditional remove count");
        bst_set_destroy(set);
        return;
    }
    
    if (set->size != 8) {
        strcpy(res, "fail - compact_consequent conditional final size");
        bst_set_destroy(set);
        return;
    }
    
    // Test 9: Verify garbage collection - add and delete many nodes with data
    bst_set_destroy(set);
    set = bst_set_init();
    
    // Add 100 nodes with allocated data
    for (size_t i = 1; i <= 100; i++) {
        size_t* large_data = malloc(sizeof(size_t)*256); // Allocate larger data
        for (size_t j = 0; j < 256; j++) {
            large_data[j] = i * 100 + j;
        }
        bst_set_add(set, i, large_data, sizeof(size_t)*256);
        free(large_data);
    }
    
    // Delete half of them
    for (size_t i = 1; i <= 50; i++) {
        bst_set_delete(set, i);
    }
    
    // Verify remaining data
    for (size_t i = 51; i <= 100; i++) {
        retrieved_data = NULL;
        retrieved_size = 0;
        if (bst_set_lookup(set, i, &retrieved_data, &retrieved_size) != 1) {
            strcpy(res, "fail - lookup after bulk delete");
            bst_set_destroy(set);
            return;
        }
        if (retrieved_size != sizeof(size_t)*256) {
            strcpy(res, "fail - data size wrong after bulk delete");
            bst_set_destroy(set);
            return;
        }
        size_t* data_arr = (size_t*)retrieved_data;
        if (data_arr[0] != i * 100) {
            strcpy(res, "fail - data corrupted after bulk delete");
            bst_set_destroy(set);
            return;
        }
    }
    
    // Test 10: Concurrent operations (original test)
    set = bst_set_init();
    if (set == NULL) {
        strcpy(res, "fail - concurrent test init");
        return;
    }
    
    // Add initial elements
    for (size_t i = 1; i <= 100; i++) {
        bst_set_add(set, i, NULL, 0);
    }
    
    // Thread-safe lookup while another thread is modifying
    pthread_t thread1, thread2;
    
    pthread_create(&thread1, NULL, add_thread_func, set);
    pthread_create(&thread2, NULL, lookup_thread_func, set);
    
    void* lookup_result;
    pthread_join(thread1, NULL);
    pthread_join(thread2, &lookup_result);
    
    if (lookup_result != (void*)0) {
        strcpy(res, "fail - concurrent lookup failed");
        bst_set_destroy(set);
        return;
    }
    
    if (set->size != 200) {
        strcpy(res, "fail - concurrent final size");
        bst_set_destroy(set);
        return;
    }
    
    bst_set_destroy(set);
    strcpy(res, "pass");
}
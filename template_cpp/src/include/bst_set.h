#ifndef BST_SET_H
#define BST_SET_H

#include <stddef.h>
#include <pthread.h>

// struct designed with the help of LLM
typedef struct bst_node {
    size_t key;
    int height;
    struct bst_node* left;
    struct bst_node* right;
} bst_node;

typedef struct {
    bst_node* root;
    size_t size;
    pthread_mutex_t mutex;
} bst_set;

// Initialize a new ordered set
bst_set* bst_set_init(void);

// Add an element with the given key
// Returns 0 on success, -1 on error, 1 if key already exists
int bst_set_add(bst_set* set, size_t key);

// Delete an element with the given key
// Returns 0 on success, -1 if key not found
int bst_set_delete(bst_set* set, size_t key);

// Lookup if a key is present in the set
// Returns 1 if present, 0 if not present
int bst_set_lookup(bst_set* set, size_t key);

// Destroy the set and free all memory
void bst_set_destroy(bst_set* set);

#endif // BST_SET_H

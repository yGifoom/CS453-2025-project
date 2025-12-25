#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "version_types.h"

// Node structure for the linked list
typedef struct ll_node {
    void *data;
    struct ll_node *next;
} ll_node_t;

// Linked list structure with head and tail for O(1) append
typedef struct ll {
    ll_node_t *head;
    ll_node_t *tail;
    size_t size;
    version_lock lock;  // Lock for thread-safe operations
} ll_t;

/**
 * Initialize an empty linked list
 * @param list Pointer to the linked list to initialize
 * @return true on success, false on failure
 */
bool ll_init(ll_t *list);

/**
 * Destroy the linked list and free all nodes
 * @param list Pointer to the linked list to destroy
 */
void ll_destroy(ll_t *list);

/**
 * Destroy the linked list, free all nodes, and call destructor on each data element
 * @param list Pointer to the linked list to destroy
 * @param destructor Function to free the data in each node (can be free or custom)
 */
void ll_destroy_nested(ll_t *list, void (*destructor)(void *));

/**
 * Append data to the end of the linked list in O(1) time
 * @param list Pointer to the linked list
 * @param data Data to append
 * @return true on success, false on failure
 */
bool ll_append(ll_t *list, void *data);

/**
 * Thread-safe append data to the end of the linked list in O(1) time
 * @param list Pointer to the linked list
 * @param data Data to append
 * @return true on success, false on failure
 */
bool ll_append_safe(ll_t *list, void *data);

/**
 * Thread-safe concatenate two linked lists in O(1) time
 * Appends all nodes from list2 to list1, leaving list2 empty
 * @param list1 Destination list
 * @param list2 Source list (will be emptied)
 * @return true on success, false on failure
 */
bool ll_concat_safe(ll_t *list1, ll_t *list2);

/**
 * Remove the first occurrence of data from the linked list
 * @param list Pointer to the linked list
 * @param data Data to remove (compared by pointer equality)
 * @return true if removed, false if not found
 */
bool ll_remove(ll_t *list, void *data);

/**
 * Remove a node by pointer comparison with custom comparator
 * @param list Pointer to the linked list
 * @param data Data to remove
 * @param compare Comparison function (returns 0 if equal)
 * @return true if removed, false if not found
 */
bool ll_remove_cmp(ll_t *list, void *data, int (*compare)(void *, void *));

/**
 * Get the size of the linked list
 * @param list Pointer to the linked list
 * @return Number of elements in the list
 */
size_t ll_size(ll_t *list);

/**
 * Check if the linked list is empty
 * @param list Pointer to the linked list
 * @return true if empty, false otherwise
 */
bool ll_is_empty(ll_t *list);

#include "ll.h"
#include <stdlib.h>
#include <string.h>

bool ll_init(ll_t *list) {
    if (list == NULL) {
        return false;
    }
    
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    atomic_store(&list->lock, 0);  // Initialize lock to unlocked state
    
    return true;
}

void ll_destroy(ll_t *list) {
    if (list == NULL) {
        return;
    }
    
    ll_node_t *current = list->head;
    ll_node_t *next;
    
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

void ll_destroy_nested(ll_t *list, void (*destructor)(void *)) {
    if (list == NULL) {
        return;
    }
    
    if (destructor == NULL) {
        ll_destroy(list);
        return;
    }
    
    ll_node_t *current = list->head;
    ll_node_t *next;
    
    while (current != NULL) {
        next = current->next;
        
        // Free the data using the provided destructor
        if (current->data != NULL) {
            destructor(current->data);
        }
        
        free(current);
        current = next;
    }
    
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

bool ll_append(ll_t *list, void *data) {
    if (list == NULL) {
        return false;
    }
    
    // Allocate new node
    ll_node_t *new_node = (ll_node_t *)malloc(sizeof(ll_node_t));
    if (new_node == NULL) {
        return false;
    }
    
    new_node->data = data;
    new_node->next = NULL;
    
    // If list is empty, set both head and tail to new node
    if (list->head == NULL) {
        list->head = new_node;
        list->tail = new_node;
    } else {
        // Append to tail - O(1) operation
        list->tail->next = new_node;
        list->tail = new_node;
    }
    
    list->size++;
    return true;
}

bool ll_append_safe(ll_t *list, void *data) {
    if (list == NULL) {
        return false;
    }
    
    // Acquire lock before modifying the list
    lock_acquire(&list->lock);
    
    // Allocate new node
    ll_node_t *new_node = (ll_node_t *)malloc(sizeof(ll_node_t));
    if (new_node == NULL) {
        lock_release(&list->lock);
        return false;
    }
    
    new_node->data = data;
    new_node->next = NULL;
    
    // If list is empty, set both head and tail to new node
    if (list->head == NULL) {
        list->head = new_node;
        list->tail = new_node;
    } else {
        // Append to tail - O(1) operation
        list->tail->next = new_node;
        list->tail = new_node;
    }
    
    list->size++;
    
    // Release lock after modification is complete
    lock_release(&list->lock);
    
    return true;
}

bool ll_concat_safe(ll_t *list1, ll_t *list2) {
    if (list1 == NULL || list2 == NULL) {
        return false;
    }
    
    // Avoid self-concatenation
    if (list1 == list2) {
        return false;
    }
    
    lock_acquire(&list1->lock);
    
    // If list2 is empty, nothing to do
    if (list2->head == NULL) {
        lock_release(&list1->lock);
        return true;
    }
    
    // If list1 is empty, just transfer everything from list2
    if (list1->head == NULL) {
        list1->head = list2->head;
        list1->tail = list2->tail;
        list1->size = list2->size;
    } else {
        // Connect list1's tail to list2's head
        list1->tail->next = list2->head;
        list1->tail = list2->tail;
        list1->size += list2->size;
    }
    
    // Clear list2
    list2->head = NULL;
    list2->tail = NULL;
    list2->size = 0;
    
    lock_release(&list1->lock);
    
    return true;
}

bool ll_remove(ll_t *list, void *data) {
    if (list == NULL || list->head == NULL) {
        return false;
    }
    
    ll_node_t *current = list->head;
    ll_node_t *prev = NULL;
    
    while (current != NULL) {
        // Compare pointers
        if (current->data == data) {
            // Found the node to remove
            if (prev == NULL) {
                // Removing head
                list->head = current->next;
                
                // If we removed the only node, update tail
                if (list->head == NULL) {
                    list->tail = NULL;
                }
            } else {
                // Removing middle or tail node
                prev->next = current->next;
                
                // If we removed the tail, update tail pointer
                if (current == list->tail) {
                    list->tail = prev;
                }
            }
            
            free(current);
            list->size--;
            return true;
        }
        
        prev = current;
        current = current->next;
    }
    
    return false;
}

bool ll_remove_cmp(ll_t *list, void *data, int (*compare)(void *, void *)) {
    if (list == NULL || list->head == NULL || compare == NULL) {
        return false;
    }
    
    ll_node_t *current = list->head;
    ll_node_t *prev = NULL;
    
    while (current != NULL) {
        // Use custom comparator
        if (compare(current->data, data) == 0) {
            // Found the node to remove
            if (prev == NULL) {
                // Removing head
                list->head = current->next;
                
                // If we removed the only node, update tail
                if (list->head == NULL) {
                    list->tail = NULL;
                }
            } else {
                // Removing middle or tail node
                prev->next = current->next;
                
                // If we removed the tail, update tail pointer
                if (current == list->tail) {
                    list->tail = prev;
                }
            }
            
            free(current);
            list->size--;
            return true;
        }
        
        prev = current;
        current = current->next;
    }
    
    return false;
}

size_t ll_size(ll_t *list) {
    if (list == NULL) {
        return 0;
    }
    return list->size;
}

bool ll_is_empty(ll_t *list) {
    if (list == NULL) {
        return true;
    }
    return list->size == 0;
}

#include "bst_set.h"
#include <stdlib.h>
#include <pthread.h>

// Helper functions
static int max(int a, int b) {
    return (a > b) ? a : b;
}

static int height(bst_node* node) {
    return node ? node->height : 0;
}

static int get_balance(bst_node* node) {
    if (!node) return 0;
    int lh = height(node->left);
    int rh = height(node->right);
    int res;
    if (lh > rh){
        res = -(lh - rh);
    }else{
        res = lh - rh;
    }
    return res;
}

static bst_node* create_node(size_t key) {
    bst_node* node = malloc(sizeof(bst_node));
    if (!node) return NULL;
    
    node->key = key;
    node->height = 1;
    node->left = NULL;
    node->right = NULL;
    return node;
}

static bst_node* rotate_right(bst_node* y) {
    bst_node* x = y->left;
    bst_node* T2 = x->right;
    
    x->right = y;
    y->left = T2;
    
    y->height = max(height(y->left), height(y->right)) + 1;
    x->height = max(height(x->left), height(x->right)) + 1;
    
    return x;
}

static bst_node* rotate_left(bst_node* x) {
    bst_node* y = x->right;
    bst_node* T2 = y->left;
    
    y->left = x;
    x->right = T2;
    
    x->height = max(height(x->left), height(x->right)) + 1;
    y->height = max(height(y->left), height(y->right)) + 1;
    
    return y;
}

static bst_node* find_min(bst_node* node) {
    while (node->left) {
        node = node->left;
    }
    return node;
}

static bst_node* insert_helper(bst_node* node, size_t key, int* status) {
    if (!node) {
        *status = 0;
        return create_node(key);
    }
    
    if (key < node->key) {
        node->left = insert_helper(node->left, key, status);
    } else if (key > node->key) {
        node->right = insert_helper(node->right, key, status);
    } else {
        *status = 1; // Key already exists
        return node;
    }
    
    node->height = 1 + max(height(node->left), height(node->right));
    
    int balance = get_balance(node);
    
    // Left Left Case
    if (balance > 1 && key < node->left->key) {
        return rotate_right(node);
    }
    
    // Right Right Case
    if (balance < -1 && key > node->right->key) {
        return rotate_left(node);
    }
    
    // Left Right Case
    if (balance > 1 && key > node->left->key) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }
    
    // Right Left Case
    if (balance < -1 && key < node->right->key) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }
    
    return node;
}

static bst_node* delete_helper(bst_node* node, size_t key, int* status) {
    if (!node) {
        *status = -1; // Key not found
        return NULL;
    }
    
    if (key < node->key) {
        node->left = delete_helper(node->left, key, status);
    } else if (key > node->key) {
        node->right = delete_helper(node->right, key, status);
    } else {
        *status = 0; // Key found
        
        if (!node->left || !node->right) {
            bst_node* temp = node->left ? node->left : node->right;
            
            if (!temp) {
                temp = node;
                node = NULL;
            } else {
                *node = *temp;
            }
            free(temp);
        } else {
            bst_node* temp = find_min(node->right);
            node->key = temp->key;
            node->right = delete_helper(node->right, temp->key, status);
        }
    }
    
    if (!node) return NULL;
    
    node->height = 1 + max(height(node->left), height(node->right));
    
    int balance = get_balance(node);
    
    // Left Left Case
    if (balance > 1 && get_balance(node->left) >= 0) {
        return rotate_right(node);
    }
    
    // Left Right Case
    if (balance > 1 && get_balance(node->left) < 0) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }
    
    // Right Right Case
    if (balance < -1 && get_balance(node->right) <= 0) {
        return rotate_left(node);
    }
    
    // Right Left Case
    if (balance < -1 && get_balance(node->right) > 0) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }
    
    return node;
}

static int lookup_helper(bst_node* node, size_t key) {
    if (!node) return 0;
    
    if (key == node->key) return 1;
    if (key < node->key) return lookup_helper(node->left, key);
    return lookup_helper(node->right, key);
}

static void destroy_helper(bst_node* node) {
    if (!node) return;
    
    destroy_helper(node->left);
    destroy_helper(node->right);
    free(node);
}

// Public interface
bst_set* bst_set_init(void) {
    bst_set* set = malloc(sizeof(bst_set));
    if (!set) return NULL;
    
    set->root = NULL;
    set->size = 0;
    
    if (pthread_mutex_init(&set->mutex, NULL) != 0) {
        free(set);
        return NULL;
    }
    
    return set;
}

int bst_set_add(bst_set* set, size_t key) {
    if (!set) return -1;
    
    pthread_mutex_lock(&set->mutex);
    
    int status = 0;
    set->root = insert_helper(set->root, key, &status);
    
    if (status == 0) {
        set->size++;
    }
    
    pthread_mutex_unlock(&set->mutex);
    
    return status;
}

int bst_set_delete(bst_set* set, size_t key) {
    if (!set) return -1;
    
    pthread_mutex_lock(&set->mutex);
    
    int status = 0;
    set->root = delete_helper(set->root, key, &status);
    
    if (status == 0) {
        set->size--;
    }
    
    pthread_mutex_unlock(&set->mutex);
    
    return status;
}

int bst_set_lookup(bst_set* set, size_t key) {
    if (!set) return 0;
    
    pthread_mutex_lock(&set->mutex);
    int result = lookup_helper(set->root, key);
    pthread_mutex_unlock(&set->mutex);
    
    return result;
}

void bst_set_destroy(bst_set* set) {
    if (!set) return;
    
    pthread_mutex_lock(&set->mutex);
    destroy_helper(set->root);
    pthread_mutex_unlock(&set->mutex);
    
    pthread_mutex_destroy(&set->mutex);
    free(set);
}

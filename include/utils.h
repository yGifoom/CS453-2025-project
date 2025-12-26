#pragma once

#include <dict.h>
#include <tx_t.h>
#include <shared_t.h>
#include <stdbool.h>

// struct to communicate need for rollback operation
typedef struct{
    shared_rgn* region;
    transaction_t* transaction;
    void* key;
}region_and_index;

int lock_write_set(void *key, int count, void* *value, void *user);
int unlock_write_set_until(void *key, int count, void* *value, void *user);
int validate_reading_set(void *key, int count, void* *value, void *user);
int write_writing_set(void *key, int count, void* *value, void *user);
int update_locks_writing_set(void *key, int count, void* *value, void *user);


int nested_free_value_dict(void *key, int count, void* *value, void *user);
int add_from_dict(void *key, int count, void* *value, void *user);
int rm_from_dict(void *key, int count, void* *value, void *user);
void dic_nested_destroy(struct dictionary*);
void tx_destroy(transaction_t*, bool);

version_lock* lock_get_from_pointer(shared_rgn* shared, void* ptr);


#pragma once

#include <dict.h>
#include <tx_t.h>
#include <stdbool.h>

int nested_free_dict(void *key, int count, void* *value, void *user);
int add_from_dict(void *key, int count, void* *value, void *user);
int rm_from_dict(void *key, int count, void* *value, void *user);
void dic_nested_destroy(struct dictionary*);
void tx_destroy(transaction_t*, bool);

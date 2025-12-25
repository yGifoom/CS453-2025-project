#pragma once

#include <version_types.h>
#include <dict.h>
#include <ll.h>
#include <stdbool.h>

typedef struct {
    int read_version;
    int write_version;
    
    bool read_only; // if transaction will only perform reads

    struct dictionary* read_set;    // is set
    struct dictionary* write_set;   // is dict
    struct ll* alloc_set;   // if alloc_set value is NULL, it has already been freed
}transaction_t; 
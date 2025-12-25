#pragma once

#include "version_types.h"
#include "ll.h"


typedef struct {
    version_lock* lock_region;
    void* data_region;
}segment;

typedef struct {
    global_counter global_version;
    void* start;

    size_t size;
    size_t align;

    version_lock* locks;
    struct ll* segments;
} shared_rgn; // The type of a shared memory region
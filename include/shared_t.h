#pragma once

#include "version_types.h"
#include "dict.h"


typedef void* segment;

typedef struct {
    global_counter global_version;
    void* start;

    size_t size;
    size_t align;

    struct dictionary* segments;
    version_lock segment_guard; // when adding or deleting guard makes thread safe
} shared_rgn; // The type of a shared memory region
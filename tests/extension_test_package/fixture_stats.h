#pragma once
#include <stdint.h>
/* Test instrumentation only. Not an extension API capability. */
typedef struct NectFixtureStats {
    uint32_t struct_size, api_major, api_minor;
    uint32_t entries, creates, destroys, evaluations, live, maximum_active;
} NectFixtureStats;

#pragma once

#include "internal.h"

typedef enum {
    GC_WHITE0 = 0,
    GC_WHITE1 = 1,
    GC_BLACK = 2,
    GC_GRAY = 3
} gc_color;

void gc_init(prolog *pl);
void gc_collect(prolog *pl);
void gc_mark_cell(prolog *pl, cell *c);
void gc_register_object(prolog *pl, void *obj, uint8_t tag);

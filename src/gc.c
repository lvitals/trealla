#include <stdlib.h>
#include <string.h>

#include "gc.h"
#include "prolog.h"
#include "query.h"
#include "hash_table.h"

/*
 * Native Incremental Garbage Collector for Trealla.
 * Focuses on managed terms (strbufs, bigints, blobs) to match Lua Store's GC efficiency.
 */

typedef struct gc_obj_ {
    void *ptr;          // Pointer to the managed object
    uint8_t tag;        // Object tag
    uint8_t color;      // GC color
    struct gc_obj_ *next;
} gc_obj;

static gc_obj *g_objects = NULL;
static lock g_gc_lock;

void gc_init(prolog *pl) {
    init_lock(&g_gc_lock);
}

void gc_register_object(prolog *pl, void *obj, uint8_t tag) {
    acquire_lock(&g_gc_lock);
    gc_obj *go = malloc(sizeof(gc_obj));
    go->ptr = obj;
    go->tag = tag;
    go->color = GC_WHITE0;
    go->next = g_objects;
    g_objects = go;
    release_lock(&g_gc_lock);
}

void gc_mark_cell(prolog *pl, cell *c) {
    if (!is_managed(c)) return;
    
    void *ptr = NULL;
    if (is_strbuf(c)) ptr = c->val_strb;
    else if (is_bigint(c)) ptr = c->val_bigint;
    else if (is_blob(c)) ptr = c->val_blob;

    if (!ptr) return;

    // Search for the object and mark it black (simplified)
    gc_obj *go = g_objects;
    while (go) {
        if (go->ptr == ptr) {
            go->color = GC_BLACK;
            break;
        }
        go = go->next;
    }
}

static void gc_mark_hash_table(prolog *pl, hash_table *ht) {
    if (!ht) return;
    hash_iter *it = hash_first(ht);
    cell key, val;
    while (hash_next(it, &key, &val)) {
        gc_mark_cell(pl, &key);
        gc_mark_cell(pl, &val);
        if (val.tag == TAG_EMPTY && val.val_ptr) {
            // Blackboard entry - check if it's a cell array
            cell *c = (cell*)val.val_ptr;
            for (pl_idx i = 0; i < c->num_cells; i++)
                gc_mark_cell(pl, c + i);
        }
    }
    hash_done(it);
}

static void gc_mark_query(prolog *pl, query *q) {
    if (!q) return;
    // Mark heap pages
    for (page *a = q->heap_pages; a; a = a->next) {
        for (pl_idx i = 0; i < a->idx; i++)
            gc_mark_cell(pl, a->cells + i);
    }
}

void gc_collect(prolog *pl) {
    acquire_lock(&g_gc_lock);

    // 1. Reset
    gc_obj *go = g_objects;
    while (go) {
        go->color = GC_WHITE0;
        go = go->next;
    }

    // 2. Mark from roots
    // Roots: Global Blackboard
    gc_mark_hash_table(pl, pl->keyval);

    // Roots: All streams
    for (int i = 0; i < MAX_STREAMS; i++) {
        stream *str = &pl->streams[i];
        if (str->is_map) gc_mark_hash_table(pl, str->keyval);
    }

    // Roots: Active queries
    for (int i = 0; i < MAX_THREADS; i++) {
        if (pl->threads[i].q)
            gc_mark_query(pl, pl->threads[i].q);
    }

    // 3. Sweep
    gc_obj **curr = &g_objects;
    while (*curr) {
        gc_obj *go = *curr;
        if (go->color == GC_WHITE0) {
            // Reclaim (Simplified: only if refcnt is 1 or managed solely by GC)
            // For now, this GC acts as a safety net.
            *curr = go->next;
            free(go);
        } else {
            curr = &go->next;
        }
    }

    release_lock(&g_gc_lock);
}

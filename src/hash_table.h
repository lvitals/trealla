#pragma once

#include "internal.h"

/* 
 * Hash node for collisions (chaining)
 */
typedef struct node_ {
    cell key;
    cell val;
    struct node_ *next;
} hash_node;

typedef void (*hash_free_val_t)(const cell *key, const cell *val, void *param);

/*
 * Hybrid Hash Table: Array part for dense integer keys, 
 * Hash part for arbitrary/sparse keys.
 */
typedef struct hash_table {
    hash_node **nodes;
    cell *array;
    pl_idx size;    // size of hash part
    pl_idx count;   // number of elements in hash part
    pl_idx asize;   // size of array part
    hash_free_val_t free_val;
    void *param;
} hash_table;

hash_table *hash_create(pl_idx size, hash_free_val_t free_val, void *param);
void hash_destroy(hash_table *ht);

bool hash_set(hash_table *ht, prolog *pl, const cell *key, const cell *val);
bool hash_get(hash_table *ht, prolog *pl, const cell *key, cell *res);
bool hash_del(hash_table *ht, prolog *pl, const cell *key);

pl_idx hash_count(const hash_table *ht);

typedef struct hash_iter {
    hash_table *ht;
    pl_idx idx;
    hash_node *node;
    bool is_array;
} hash_iter;

hash_iter *hash_first(hash_table *ht);
bool hash_next(hash_iter *it, cell *key, cell *val);
void hash_done(hash_iter *it);

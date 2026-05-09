#include <stdlib.h>
#include <string.h>

#include "hash_table.h"
#include "prolog.h"
#include "query.h"

/*
 * Native Hash Table for Trealla.
 * Hybrid: Array part for small dense integers, Hash part for everything else.
 */

static bool is_equal(prolog *pl, const cell *c1, const cell *c2)
{
    if (c1->tag != c2->tag) return false;
    if (c1->arity != c2->arity) return false;

    if (is_smallint(c1)) return get_smallint(c1) == get_smallint(c2);
    if (is_atom(c1)) return c1->val_off == c2->val_off;
    if (is_float(c1)) return get_float(c1) == get_float(c2);

    if (is_cstring(c1)) {
        size_t len1 = _C_STRLEN(pl, (cell*)c1);
        size_t len2 = _C_STRLEN(pl, (cell*)c2);
        if (len1 != len2) return false;
        return memcmp(_C_STR(pl, (cell*)c1), _C_STR(pl, (cell*)c2), len1) == 0;
    }

    if (is_compound(c1)) {
        if (c1->val_off != c2->val_off) return false;
        pl_idx num_cells = c1->num_cells;
        for (pl_idx i = 1; i < num_cells; i++) {
            if (!is_equal(pl, c1 + i, c2 + i)) return false;
        }
        return true;
    }

    return c1->val_int == c2->val_int;
}

static uint64_t hash_cell(prolog *pl, const cell *c)
{
    if (is_smallint(c))
        return (uint64_t)get_smallint(c);
    if (is_atom(c))
        return (uint64_t)c->val_off;
    if (is_cstring(c)) {
        const char *s = _C_STR(pl, (cell*)c);
        size_t len = _C_STRLEN(pl, (cell*)c);
        uint64_t h = 0;
        for (size_t i = 0; i < len; i++)
            h = h * 31 + s[i];
        return h;
    }
    if (is_compound(c)) {
        uint64_t h = (uint64_t)c->val_off * 31 + c->arity;
        pl_idx num_cells = c->num_cells;
        for (pl_idx i = 1; i < num_cells && i < 5; i++) {
            h = h * 31 + hash_cell(pl, c + i);
        }
        return h;
    }
    return (uint64_t)c->tag ^ (uint64_t)c->val_int;
}

hash_table *hash_create(pl_idx size, hash_free_val_t free_val, void *param)
{
    hash_table *ht = calloc(1, sizeof(hash_table));
    if (!ht) return NULL;
    ht->size = size > 0 ? size : 16;
    ht->nodes = calloc(ht->size, sizeof(hash_node*));
    ht->free_val = free_val;
    ht->param = param;
    return ht;
}

void hash_destroy(hash_table *ht)
{
    if (!ht) return;
    for (pl_idx i = 0; i < ht->size; i++) {
        hash_node *n = ht->nodes[i];
        while (n) {
            hash_node *next = n->next;
            if (ht->free_val) ht->free_val(&n->key, &n->val, ht->param);
            free(n);
            n = next;
        }
    }
    if (ht->array) {
        if (ht->free_val) {
            for (pl_idx i = 0; i < ht->asize; i++) {
                if (!is_empty(&ht->array[i])) {
                    cell key;
                    make_int(&key, i);
                    ht->free_val(&key, &ht->array[i], ht->param);
                }
            }
        }
        free(ht->array);
    }
    free(ht->nodes);
    free(ht);
}

bool hash_set(hash_table *ht, prolog *pl, const cell *key, const cell *val)
{
    // Array part for small dense integers (0..1023)
    if (is_smallint(key) && get_smallint(key) >= 0 && get_smallint(key) < 1024) {
        pl_idx idx = (pl_idx)get_smallint(key);
        if (idx >= ht->asize) {
            pl_idx new_asize = idx + 1;
            cell *new_array = realloc(ht->array, sizeof(cell) * new_asize);
            if (new_array) {
                for (pl_idx i = ht->asize; i < new_asize; i++)
                    new_array[i].tag = TAG_EMPTY;
                ht->array = new_array;
                ht->asize = new_asize;
            }
        }
        if (idx < ht->asize) {
            if (ht->free_val && !is_empty(&ht->array[idx]))
                ht->free_val(key, &ht->array[idx], ht->param);
            ht->array[idx] = *val;
            return true;
        }
    }

    uint64_t h = hash_cell(pl, key) % ht->size;
    hash_node *n = ht->nodes[h];
    while (n) {
        if (is_equal(pl, key, &n->key)) {
            if (ht->free_val) ht->free_val(&n->key, &n->val, ht->param);
            n->val = *val;
            return true;
        }
        n = n->next;
    }

    n = malloc(sizeof(hash_node));
    if (!n) return false;
    n->key = *key;
    n->val = *val;
    n->next = ht->nodes[h];
    ht->nodes[h] = n;
    ht->count++;

    // Rehash if too crowded
    if (ht->count > ht->size * 2) {
        pl_idx new_size = ht->size * 2;
        hash_node **new_nodes = calloc(new_size, sizeof(hash_node*));
        if (new_nodes) {
            for (pl_idx i = 0; i < ht->size; i++) {
                hash_node *curr = ht->nodes[i];
                while (curr) {
                    hash_node *next = curr->next;
                    uint64_t h2 = hash_cell(pl, &curr->key) % new_size;
                    curr->next = new_nodes[h2];
                    new_nodes[h2] = curr;
                    curr = next;
                }
            }
            free(ht->nodes);
            ht->nodes = new_nodes;
            ht->size = new_size;
        }
    }

    return true;
}

bool hash_get(hash_table *ht, prolog *pl, const cell *key, cell *res)
{
    if (is_smallint(key) && get_smallint(key) >= 0 && get_smallint(key) < ht->asize) {
        cell *c = &ht->array[get_smallint(key)];
        if (!is_empty(c)) {
            *res = *c;
            return true;
        }
    }

    uint64_t h = hash_cell(pl, key) % ht->size;
    hash_node *n = ht->nodes[h];
    while (n) {
        if (is_equal(pl, key, &n->key)) {
            *res = n->val;
            return true;
        }
        n = n->next;
    }
    return false;
}

bool hash_del(hash_table *ht, prolog *pl, const cell *key)
{
    if (is_smallint(key) && get_smallint(key) >= 0 && get_smallint(key) < ht->asize) {
        cell *c = &ht->array[get_smallint(key)];
        if (!is_empty(c)) {
            if (ht->free_val) ht->free_val(key, c, ht->param);
            c->tag = TAG_EMPTY;
            return true;
        }
    }

    uint64_t h = hash_cell(pl, key) % ht->size;
    hash_node *curr = ht->nodes[h];
    hash_node *prev = NULL;
    while (curr) {
        if (is_equal(pl, key, &curr->key)) {
            if (prev) prev->next = curr->next;
            else ht->nodes[h] = curr->next;
            if (ht->free_val) ht->free_val(&curr->key, &curr->val, ht->param);
            free(curr);
            ht->count--;
            return true;
        }
        prev = curr;
        curr = curr->next;
    }
    return false;
}

pl_idx hash_count(const hash_table *ht)
{
    pl_idx total = ht->count;
    if (ht->array) {
        for (pl_idx i = 0; i < ht->asize; i++) {
            if (!is_empty(&ht->array[i])) total++;
        }
    }
    return total;
}

hash_iter *hash_first(hash_table *ht)
{
    hash_iter *it = calloc(1, sizeof(hash_iter));
    it->ht = ht;
    it->is_array = ht->asize > 0;
    return it;
}

bool hash_next(hash_iter *it, cell *key, cell *val)
{
    if (it->is_array) {
        while (it->idx < it->ht->asize) {
            cell *c = &it->ht->array[it->idx];
            if (!is_empty(c)) {
                make_int(key, it->idx);
                *val = *c;
                it->idx++;
                return true;
            }
            it->idx++;
        }
        it->is_array = false;
        it->idx = 0;
    }

    while (it->idx < it->ht->size) {
        if (!it->node) it->node = it->ht->nodes[it->idx];
        if (it->node) {
            *key = it->node->key;
            *val = it->node->val;
            it->node = it->node->next;
            if (!it->node) it->idx++;
            return true;
        }
        it->idx++;
    }
    return false;
}

void hash_done(hash_iter *it)
{
    free(it);
}

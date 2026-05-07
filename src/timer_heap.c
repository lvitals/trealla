#include <stdlib.h>
#include "internal.h"
#include "query.h"

#if USE_LUA

void timer_heap_push(prolog *pl, query *q)
{
	acquire_lock(&pl->timer_heap_lock);
	if (pl->timer_heap_size == pl->timer_heap_cap) {
		pl->timer_heap_cap = pl->timer_heap_cap ? pl->timer_heap_cap * 2 : 128;
		pl->timer_heap = realloc(pl->timer_heap, sizeof(query*) * pl->timer_heap_cap);
	}

	pl_idx i = pl->timer_heap_size++;
	while (i > 0) {
		pl_idx p = (i - 1) / 2;
		if (pl->timer_heap[p]->tmo_msecs <= q->tmo_msecs)
			break;
		pl->timer_heap[i] = pl->timer_heap[p];
		pl->timer_heap[i]->heap_idx = i;
		i = p;
	}
	pl->timer_heap[i] = q;
	q->heap_idx = i;
	release_lock(&pl->timer_heap_lock);
}

static void timer_heap_bubble_down(prolog *pl, pl_idx i)
{
	query *q = pl->timer_heap[i];
	while (i * 2 + 1 < pl->timer_heap_size) {
		pl_idx child = i * 2 + 1;
		if (child + 1 < pl->timer_heap_size && 
			pl->timer_heap[child+1]->tmo_msecs < pl->timer_heap[child]->tmo_msecs)
			child++;
		if (q->tmo_msecs <= pl->timer_heap[child]->tmo_msecs)
			break;
		pl->timer_heap[i] = pl->timer_heap[child];
		pl->timer_heap[i]->heap_idx = i;
		i = child;
	}
	pl->timer_heap[i] = q;
	q->heap_idx = i;
}

void timer_heap_delete(prolog *pl, query *q)
{
	acquire_lock(&pl->timer_heap_lock);
	if (q->heap_idx == ERR_IDX) {
		release_lock(&pl->timer_heap_lock);
		return;
	}
	pl_idx i = q->heap_idx;
	pl_idx last = --pl->timer_heap_size;
	if (i != last) {
		pl->timer_heap[i] = pl->timer_heap[last];
		pl->timer_heap[i]->heap_idx = i;
		if (pl->timer_heap[i]->tmo_msecs < q->tmo_msecs) {
			query *moved = pl->timer_heap[i];
			while (i > 0) {
				pl_idx p = (i - 1) / 2;
				if (pl->timer_heap[p]->tmo_msecs <= moved->tmo_msecs) break;
				pl->timer_heap[i] = pl->timer_heap[p];
				pl->timer_heap[i]->heap_idx = i;
				i = p;
			}
			pl->timer_heap[i] = moved;
			moved->heap_idx = i;
		} else {
			timer_heap_bubble_down(pl, i);
		}
	}
	q->heap_idx = ERR_IDX;
	release_lock(&pl->timer_heap_lock);
}

query *timer_heap_pop(prolog *pl)
{
	acquire_lock(&pl->timer_heap_lock);
	if (pl->timer_heap_size == 0) {
		release_lock(&pl->timer_heap_lock);
		return NULL;
	}
	query *root = pl->timer_heap[0];
	pl_idx last = --pl->timer_heap_size;
	if (last != 0) {
		pl->timer_heap[0] = pl->timer_heap[last];
		pl->timer_heap[0]->heap_idx = 0;
		timer_heap_bubble_down(pl, 0);
	}
	root->heap_idx = ERR_IDX;
	release_lock(&pl->timer_heap_lock);
	return root;
}

#endif

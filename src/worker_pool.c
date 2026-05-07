#include <stdlib.h>
#include "internal.h"
#include "query.h"

#if USE_THREADS && USE_LUA
typedef struct {
	prolog *pl;
	unsigned id;
} worker_arg;

static void *worker_proc(void *arg)
{
	worker_arg *wa = (worker_arg*)arg;
	prolog *pl = wa->pl;
	unsigned worker_id = wa->id + 1;
	free(wa);

	while (!pl->halt) {
		query *task = NULL;
		acquire_lock(&pl->run_queue_lock);
		while (!(task = list_pop_front(&pl->run_queue)) && !pl->halt) {
			pthread_cond_wait(&pl->run_queue_cond, &pl->run_queue_lock.mutex);
		}
		release_lock(&pl->run_queue_lock);

		if (task) {
			task->worker_id = worker_id;
			start(task);
			
			// Devolve a tarefa para o parent para que o bif_wait_0 possa processá-la
			query *parent = task->parent;
			if (parent) {
				push_task(parent, task);
				acquire_lock(&parent->tasks_lock);
				parent->inflight--;
				release_lock(&parent->tasks_lock);
			} else {
				query_destroy(task);
			}
		}
	}
	return NULL;
}

void init_worker_pool(prolog *pl)
{
	init_lock(&pl->run_queue_lock);
	pthread_cond_init(&pl->run_queue_cond, NULL);
	list_init(&pl->run_queue);
	
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 512 * 1024); // 512KB stack
	
	unsigned cnt = g_cpu_count;
	if (cnt > 16)
		cnt = 16;
	if (cnt > MAX_THREADS - 1)
		cnt = MAX_THREADS - 1;

	for (unsigned i = 0; i < cnt; i++) {
		worker_arg *wa = calloc(1, sizeof(*wa));
		if (!wa) break;
		wa->pl = pl;
		wa->id = i;
		if (pthread_create(&pl->worker_threads[pl->worker_count], &attr, worker_proc, wa) != 0) {
			free(wa);
			break;
		}
		pl->worker_count++;
	}
	pthread_attr_destroy(&attr);
}

void destroy_worker_pool(prolog *pl)
{
	acquire_lock(&pl->run_queue_lock);
	pl->halt = true;
	pthread_cond_broadcast(&pl->run_queue_cond);
	release_lock(&pl->run_queue_lock);

	for (unsigned i = 0; i < pl->worker_count; i++)
		pthread_join(pl->worker_threads[i], NULL);

	pl->worker_count = 0;
}
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "module.h"
#include "parser.h"
#include "prolog.h"
#include "query.h"
#include "kpoll.h"

#ifdef _WIN32
#include <windows.h>
#define msleep Sleep
#define localtime_r(p1,p2) localtime(p1)
#else
#endif

bool do_yield(query *q, int msecs)
{
#ifdef __wasi__
	if (!q->is_task && !q->pl->is_query)
#else
	if (!q->is_task)
#endif
		return true;

	q->yield_at = 0;
	q->yielded = true;
	q->tmo_msecs = get_time_in_usec() / 1000;
	q->tmo_msecs += msecs > 0 ? msecs : 1;

	#if USE_LUA
	if (q->is_task)
		timer_heap_push(q->pl, q);
	#endif

	CHECKED(push_choice(q));
	return false;
}

bool do_yield_then(query *q, bool status)
{
#ifdef __wasi__
	if (!q->is_task && !q->pl->is_query)
#else
	if (!q->is_task)
#endif
		return true;

	q->yield_at = 0;
	q->yielded = true;
	q->tmo_msecs = get_time_in_usec() / 1000 + 1;
	// Push a choice point with the same result as the goal we hijacked
	// With that we can continue as if the yield didn't happen
	CHECKED(push_choice(q));
	choice *ch = GET_CURR_CHOICE();

	if (status)
		ch->succeed_on_retry = true;
	else
		ch->fail_on_retry = true;

	return false;
}

void do_yield_at(query *q, unsigned int time_in_ms)
{
	q->yield_at = get_time_in_usec() / 1000;
	q->yield_at += time_in_ms > 0 ? time_in_ms : 1;
}

static cell *pop_queue(query *q)
{
	if (!q->qp[0])
		return NULL;

	cell *c = q->queue[0] + q->popp;
	q->popp += c->num_cells;

	if (q->popp == q->qp[0])
		q->popp = q->qp[0] = 0;

	return c;
}

void push_task(query *q, query *task)
{
	acquire_lock(&q->tasks_lock);
	task->next = q->tasks;
	task->prev = NULL;

	if (q->tasks)
		q->tasks->prev = task;

	q->tasks = task;
	release_lock(&q->tasks_lock);
}

query *pop_task(query *q, query *task)
{
	acquire_lock(&q->tasks_lock);
	if (task->prev)
		task->prev->next = task->next;

	if (task->next)
		task->next->prev = task->prev;

	if (task == q->tasks)
		q->tasks = task->next;
	
	query *next = task->next;
	task->next = task->prev = NULL;
	release_lock(&q->tasks_lock);
	return next;
}

static query *detach_task_locked(query *q, query *task)
{
	if (task->prev)
		task->prev->next = task->next;

	if (task->next)
		task->next->prev = task->prev;

	if (task == q->tasks)
		q->tasks = task->next;

	query *next = task->next;
	task->next = task->prev = NULL;
	return next;
}

static bool bif_end_wait_0(query *q)
{
	if (q->parent)
		q->parent->end_wait = true;

	return true;
}

static bool bif_wait_0(query *q)
{
	while (true) {
		acquire_lock(&q->tasks_lock);
		bool work_done = (q->tasks == NULL && q->inflight == 0);
		release_lock(&q->tasks_lock);
		
		if (work_done || q->end_wait) break;

		CHECK_INTERRUPT();
		uint64_t now = get_time_in_usec() / 1000;
		uint64_t min_tmo = 0;

		#if USE_LUA
		while (true) {
			query *task = timer_heap_pop(q->pl);
			if (!task) break;
			
			if (task->tmo_msecs > now) {
				timer_heap_push(q->pl, task);
				min_tmo = task->tmo_msecs - now;
				break;
			}

			task->tmo_msecs = 0;
			acquire_lock(&q->tasks_lock);
			if (task->parent == q)
				detach_task_locked(q, task);
			if (!task->yielded || !task->st.instr || task->error) {
				release_lock(&q->tasks_lock);
				query_destroy(task);
				continue;
			}

			q->inflight++;
			release_lock(&q->tasks_lock);
			
			acquire_lock(&q->pl->run_queue_lock);
			list_push_back(&q->pl->run_queue, task);
			pthread_cond_signal(&q->pl->run_queue_cond);
			release_lock(&q->pl->run_queue_lock);
		}
		#endif

		acquire_lock(&q->tasks_lock);
		query *task = q->tasks;
		query *ready_head = NULL, *ready_tail = NULL;
		unsigned spawn_cnt = 0;

		while (task) {
			query *next = task->next;
			bool is_ready = false;

			if (task->spawned) {
				spawn_cnt++;
				if (spawn_cnt >= (g_cpu_count * 1024)) break;
			}

			if (task->tmo_msecs && !task->error) {
#if USE_LUA
				task = next;
				continue;
#else
				if (now <= task->tmo_msecs) {
					task = next;
					continue;
				}
				task->tmo_msecs = 0;
#endif
			}

#if USE_LUA
			if (task->wait_fd != -1 && !task->error) {
				struct kpollfd *kfd = NULL;
				prolog_lock(q->pl);
				sl_get(q->pl->fds, (void*)(ptrdiff_t)task->wait_fd, (const void**)&kfd);
				prolog_unlock(q->pl);
				if (kfd && !kfd->revents) {
					task = next;
					continue;
				}
			}
			is_ready = true;
#endif

			if (!task->yielded || !task->st.instr || task->error) {
				detach_task_locked(q, task);
				
				#if USE_LUA
				timer_heap_delete(q->pl, task);
				#endif
				query_destroy(task);
				task = next;
				continue;
			}

#if USE_LUA
			if (is_ready) {
				detach_task_locked(q, task);
				if (!ready_head) ready_head = task;
				else ready_tail->next = task;
				ready_tail = task;
				q->inflight++;
			}
#else
			start(task);
#endif
			task = next;
		}
		release_lock(&q->tasks_lock);

#if USE_LUA
		if (ready_head) {
			acquire_lock(&q->pl->run_queue_lock);
			while (ready_head) {
				query *next = ready_head->next;
				list_push_back(&q->pl->run_queue, ready_head);
				ready_head = next;
			}
			pthread_cond_broadcast(&q->pl->run_queue_cond);
			release_lock(&q->pl->run_queue_lock);
		} else {
			int timeout = min_tmo > 1000 ? 1000 : (int)min_tmo;
			if (min_tmo == 0) {
				acquire_lock(&q->tasks_lock);
				timeout = q->inflight ? 1 : 1000;
				release_lock(&q->tasks_lock);
			}
			kpoll_wait(&q->pl->kpoll_ctx, timeout);
		}
#else
		{
			msleep(1);
		}
#endif
	}

	q->end_wait = false;
	return true;
}

static bool bif_await_0(query *q)
{
	while (q->tasks) {
		CHECK_INTERRUPT();
		uint64_t now = get_time_in_usec() / 1000;
		query *task = q->tasks;
		unsigned spawn_cnt = 0;
		bool did_something = false;
		uint64_t min_tmo = 0;

		while (task) {
			CHECK_INTERRUPT();

			if (task->spawned) {
				spawn_cnt++;

				#if USE_LUA
				if (spawn_cnt >= (g_cpu_count * 1024))
#else
				if (spawn_cnt >= g_cpu_count)
#endif
					break;
			}

			if (task->tmo_msecs && !task->error) {
				if (now <= task->tmo_msecs) {
					uint64_t tmo = task->tmo_msecs - now;
					if (!min_tmo || tmo < min_tmo) min_tmo = tmo;
					task = task->next;
					continue;
				}

				task->tmo_msecs = 0;
			}

#if USE_LUA
			if (task->wait_fd != -1 && !task->error) {
				struct kpollfd *kfd = NULL;
				if (sl_get(q->pl->fds, (void*)(ptrdiff_t)task->wait_fd, (const void**)&kfd)) {
					if (!kfd->revents) {
						task = task->next;
						continue;
					}
				}
			}
#endif

			if (!task->yielded || !task->st.instr || task->error) {
				query *save = task;
				task = pop_task(q, task);
				query_destroy(save);
				continue;
			}

			start(task);

			if (!task->tmo_msecs && task->yielded) {
				did_something = true;
				break;
			}
		}

		if (!did_something) {
#if USE_LUA
			int timeout = min_tmo > 1000 ? 1000 : (int)min_tmo;
			if (min_tmo == 0) timeout = 1000; // Default sleep if no timeouts
			kpoll_wait(&q->pl->kpoll_ctx, timeout);
#else
			msleep(1);
#endif
		} else
			break;
	}

	if (!q->tasks)
		return false;

	CHECKED(push_choice(q));
	return true;
}

static bool bif_yield_0(query *q)
{
	if (q->retry)
		return true;

	return do_yield(q, 0);
}

static bool bif_call_task_n(query *q)
{
	pl_idx save_hp = q->st.hp;
	cell *p0 = clone_term_to_heap(q, q->st.instr, q->st.curr_fp);
	GET_FIRST_RAW_ARG0(p1,callable,p0);
	CHECKED(init_tmp_heap(q));
	CHECKED(clone_term_to_tmp(q, p1, p1_ctx));
	unsigned arity = p1->arity;
	unsigned args = 1;

	while (args++ < q->st.instr->arity) {
		GET_NEXT_RAW_ARG(p2,any);
		CHECKED(append_to_tmp(q, p2, p2_ctx));
		arity++;
	}

	cell *tmp2 = get_tmp_heap(q, 0);
	tmp2->num_cells = tmp_heap_used(q);
	tmp2->arity = arity;
	bool found = false;

	if ((tmp2->match = search_predicate(q->st.m, tmp2)) != NULL) {
		tmp2->flags &= ~FLAG_INTERNED_BUILTIN;
	} else if ((tmp2->bif_ptr = get_builtin_term(q->st.m, tmp2, &found, NULL)), found) {
		tmp2->flags |= FLAG_INTERNED_BUILTIN;
	}

	q->st.hp = save_hp;
	cell *tmp = prepare_call(q, CALL_SKIP, tmp2, q->st.curr_fp, 0);
	query *task = query_create_task(q, tmp);
	task->yielded = task->spawned = true;

	push_task(q, task);
	return true;
}

static bool bif_fork_0(query *q)
{
	cell *instr = q->st.instr + q->st.instr->num_cells;
	query *task = query_create_task(q, instr);
	task->yielded = true;
	push_task(q, task);
	return false;
}

static bool bif_sys_cancel_future_1(query *q)
{
	GET_FIRST_ARG(p1,integer);
	uint64_t future = get_smalluint(p1);

	for (query *task = q->tasks; task; task = task->next) {
		if (task->future == future) {
			task->error = true;
			break;
		}
	}

	return true;
}

static bool bif_sys_set_future_1(query *q)
{
	GET_FIRST_ARG(p1,integer);
	q->future = get_smalluint(p1);
	return true;
}

static bool bif_task_add_fd_2(query *q)
{
	GET_FIRST_ARG(p1,integer);
	GET_NEXT_ARG(p2,integer);
	int fd = (int)get_smallint(p1);
	short events = (short)get_smallint(p2);

#if USE_LUA
	struct kpollfd *kfd = NULL;
	if (sl_get(q->pl->fds, (void*)(ptrdiff_t)fd, (const void**)&kfd)) {

		kpoll_ctl(&q->pl->kpoll_ctx, kfd, events);
	} else {
		kfd = calloc(1, sizeof(struct kpollfd));
		kpoll_add(&q->pl->kpoll_ctx, kfd, fd);
		kpoll_ctl(&q->pl->kpoll_ctx, kfd, events);
		sl_set(q->pl->fds, (void*)(ptrdiff_t)fd, kfd);
	}
	return true;
#else
	return false;
#endif
}

static bool bif_task_del_fd_1(query *q)
{
	GET_FIRST_ARG(p1,integer);
	int fd = (int)get_smallint(p1);

#if USE_LUA
	struct kpollfd *kfd = NULL;
	if (sl_get(q->pl->fds, (void*)(ptrdiff_t)fd, (const void**)&kfd)) {

		kpoll_del(&q->pl->kpoll_ctx, kfd);
		sl_del(q->pl->fds, (void*)(ptrdiff_t)fd);
	}
	return true;
#else
	return false;
#endif
}

static bool bif_task_wait_fd_3(query *q)
{
	GET_FIRST_ARG(p1,integer);
	GET_NEXT_ARG(p2,integer);
	GET_NEXT_ARG(p3,any);
	int fd = (int)get_smallint(p1);
	short events = (short)get_smallint(p2);

#if USE_LUA
	struct kpollfd *kfd = NULL;
	if (!sl_get(q->pl->fds, (void*)(ptrdiff_t)fd, (const void**)&kfd)) {
		kfd = calloc(1, sizeof(struct kpollfd));
		kpoll_add(&q->pl->kpoll_ctx, kfd, fd);
		sl_set(q->pl->fds, (void*)(ptrdiff_t)fd, kfd);
	}

	if (kfd->revents & events) {
		cell res;
		make_int(&res, kfd->revents & events);
		kfd->revents &= ~events;
		q->wait_fd = -1;
		return unify(q, p3, p3_ctx, &res, q->st.curr_fp);
	}

	kpoll_ctl(&q->pl->kpoll_ctx, kfd, events);
	q->wait_fd = fd;
	return do_yield(q, 0);
#else
	return false;
#endif
}

static bool bif_send_1(query *q)
{
	GET_FIRST_ARG(p1,nonvar);
	query *dstq = q->parent && !q->parent->done ? q->parent : q;
	CHECKED(init_tmp_heap(q));
	cell *c = clone_term_to_tmp(q, p1, p1_ctx);
	CHECKED(c);

	for (pl_idx i = 0; i < c->num_cells; i++) {
		cell *c2 = c + i;
		share_cell(c2);
	}

	CHECKED(alloc_queuen(dstq, 0, c));
	q->yielded = true;
	return true;
}

static bool bif_recv_1(query *q)
{
	GET_FIRST_ARG(p1,any);

	while (true) {
		CHECK_INTERRUPT();
		cell *c = pop_queue(q);
		if (!c) break;

		if (unify(q, p1, p1_ctx, c, q->st.curr_fp))
			return true;

		CHECKED(alloc_queuen(q, 0, c));
	}

	return false;
}

builtins g_tasks_bifs[] =
{
	{"call_task", 1, bif_call_task_n, ":callable", false, false, BLAH},
	{"call_task", 2, bif_call_task_n, ":callable,?term", false, false, BLAH},
	{"call_task", 3, bif_call_task_n, ":callable,?term,?term", false, false, BLAH},
	{"call_task", 4, bif_call_task_n, ":callable,?term,?term,?term", false, false, BLAH},
	{"call_task", 5, bif_call_task_n, ":callable,?term,?term,?term,?term", false, false, BLAH},
	{"call_task", 6, bif_call_task_n, ":callable,?term,?term,?term,?term,?term", false, false, BLAH},
	{"call_task", 7, bif_call_task_n, ":callable,?term,?term,?term,?term,?term,?term", false, false, BLAH},
	{"call_task", 8, bif_call_task_n, ":callable,?term,?term,?term,?term,?term,?term,?term", false, false, BLAH},

	{"end_wait", 0, bif_end_wait_0, NULL, false, false, BLAH},
	{"wait", 0, bif_wait_0, NULL, false, false, BLAH},
	{"await", 0, bif_await_0, NULL, false, false, BLAH},
	{"yield", 0, bif_yield_0, NULL, false, false, BLAH},
	{"fork", 0, bif_fork_0, NULL, false, false, BLAH},
	{"send", 1, bif_send_1, "+term", false, false, BLAH},
	{"recv", 1, bif_recv_1, "?term", false, false, BLAH},

	{"task_add_fd", 2, bif_task_add_fd_2, "+integer,+integer", false, false, BLAH},
	{"task_del_fd", 1, bif_task_del_fd_1, "+integer", false, false, BLAH},
	{"task_wait_fd", 3, bif_task_wait_fd_3, "+integer,+integer,-integer", false, false, BLAH},

	{"$cancel_future", 1, bif_sys_cancel_future_1, "+integer", false, false, BLAH},
	{"$set_future", 1, bif_sys_set_future_1, "+integer", false, false, BLAH},

	{0}
};

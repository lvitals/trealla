#pragma once

#include <poll.h>

#if defined(__has_include)
#if __has_include(<sys/queue.h>)
#include <sys/queue.h>
#endif
#endif

/* Standard LIST macros if not defined (minimal implementation) */
#ifndef LIST_HEAD
#define LIST_HEAD(name, type)                                           \
struct name {                                                           \
        struct type *lh_first;  /* first element */                     \
}

#define LIST_ENTRY(type)                                                \
struct {                                                                \
        struct type *le_next;   /* next element */                      \
        struct type **le_prev;  /* address of previous next element */  \
}

#define LIST_FIRST(head)        ((head)->lh_first)
#define LIST_NEXT(elm, field)   ((elm)->field.le_next)
#define LIST_EMPTY(head)        ((head)->lh_first == NULL)

#define LIST_INIT(head) do {                                            \
        (head)->lh_first = NULL;                                        \
} while (0)

#define LIST_INSERT_HEAD(head, elm, field) do {                         \
        if (((elm)->field.le_next = (head)->lh_first) != NULL)          \
                (head)->lh_first->field.le_prev = &(elm)->field.le_next;\
        (head)->lh_first = (elm);                                       \
        (elm)->field.le_prev = &(head)->lh_first;                       \
} while (0)

#define LIST_REMOVE(elm, field) do {                                    \
        if ((elm)->field.le_next != NULL)                               \
                (elm)->field.le_next->field.le_prev =                   \
                    (elm)->field.le_prev;                               \
        *(elm)->field.le_prev = (elm)->field.le_next;                   \
} while (0)

#define LIST_FOREACH(var, head, field)                                  \
        for ((var) = LIST_FIRST((head));                                \
            (var);                                                      \
            (var) = LIST_NEXT((var), field))
#endif


struct kpollfd {
	int fd;
	short events;
	short revents;
	LIST_ENTRY(kpollfd) le;
};

struct kpoll {
	int fd;

	LIST_HEAD(, kpollfd) pending;
	LIST_HEAD(, kpollfd) polling;
	LIST_HEAD(, kpollfd) dormant;

	struct {
		struct kpollfd event;
		int fd[2];
	} alert;
};

int kpoll_init(struct kpoll *kp);
void kpoll_destroy(struct kpoll *kp);
void kpoll_add(struct kpoll *kp, struct kpollfd *pfd, int fd);
void kpoll_del(struct kpoll *kp, struct kpollfd *fd);
int kpoll_ctl(struct kpoll *kp, struct kpollfd *fd, short events);
int kpoll_wait(struct kpoll *kp, int timeout);
struct kpollfd *kpoll_next(struct kpoll *kp);
int kpoll_alert(struct kpoll *kp);
int kpoll_calm(struct kpoll *kp);

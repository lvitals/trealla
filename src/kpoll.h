#pragma once

#include <poll.h>
#include <sys/queue.h>

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

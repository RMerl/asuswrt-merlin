#ifndef __NAMESPACE_H__
#define __NAMESPACE_H__ 1

#include <sched.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <errno.h>

#define NETNS_RUN_DIR "/var/run/netns"
#define NETNS_ETC_DIR "/etc/netns"

#ifndef CLONE_NEWNET
#define CLONE_NEWNET 0x40000000	/* New network namespace (lo, device, names sockets, etc) */
#endif

#ifndef MNT_DETACH
#define MNT_DETACH	0x00000002	/* Just detach from the tree */
#endif /* MNT_DETACH */

/* sys/mount.h may be out too old to have these */
#ifndef MS_REC
#define MS_REC		16384
#endif

#ifndef MS_SLAVE
#define MS_SLAVE	(1 << 19)
#endif

#ifndef MS_SHARED
#define MS_SHARED	(1 << 20)
#endif

#ifndef HAVE_SETNS
static inline int setns(int fd, int nstype)
{
#ifdef __NR_setns
	return syscall(__NR_setns, fd, nstype);
#else
	errno = ENOSYS;
	return -1;
#endif
}
#endif /* HAVE_SETNS */

extern int netns_switch(char *netns);
extern int netns_get_fd(const char *netns);
extern int netns_foreach(int (*func)(char *nsname, void *arg), void *arg);

struct netns_func {
	int (*func)(char *nsname, void *arg);
	void *arg;
};

#endif /* __NAMESPACE_H__ */

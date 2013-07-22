#ifndef UTIL_LINUX_PROCUTILS
#define UTIL_LINUX_PROCUTILS

#include <dirent.h>

struct proc_tasks {
	DIR *dir;
};

extern struct proc_tasks *proc_open_tasks(pid_t pid);
extern void proc_close_tasks(struct proc_tasks *tasks);
extern int proc_next_tid(struct proc_tasks *tasks, pid_t *tid);

#endif /* UTIL_LINUX_PROCUTILS */

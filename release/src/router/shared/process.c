/*

	Tomato Firmware
	Copyright (C) 2006-2009 Jonathan Zarate

*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>

#include "shared.h"


//# cat /proc/1/stat
//1 (init) S 0 0 0 0 -1 256 287 10043 109 21377 7 110 473 1270 9 0 0 0 27 1810432 126 2147483647 4194304 4369680 2147450688 2147449688 717374852 0 0 0 514751 2147536844 0 0 0 0

char *psname(int pid, char *buffer, int maxlen)
{
	char buf[512];
	char path[64];
	char *p;

	if (maxlen <= 0) return NULL;
	*buffer = 0;
	sprintf(path, "/proc/%d/stat", pid);
	if ((f_read_string(path, buf, sizeof(buf)) > 4) && ((p = strrchr(buf, ')')) != NULL)) {
		*p = 0;
		if (((p = strchr(buf, '(')) != NULL) && (atoi(buf) == pid)) {
			strlcpy(buffer, p + 1, maxlen);
		}
	}
	return buffer;
}

/* There is a race condition when a brand new daemon starts up using the double-fork method.
 *   Example: dnsmasq
 * There are 2 windows of vulnerability.
 * 1) At the beginning of process startup, the new process has the wrong name, such as "init" because
 * init forks a child which execve()'s dnsmasq, but the execve() hasn't happened yet.
 * 2) At the end of process startup, the timing can be such that we don't see the long-lived process,
 * only the pid(s) of the short-lived process(es), but the psname fails because they've exited by then.
 *
 * The 1st can be covered by a retry after a slight delay.
 * The 2nd can be covered by a retry immediately.
 */
static int _pidof(const char *name, pid_t **pids)
{
	const char *p;
	char *e;
	DIR *dir;
	struct dirent *de;
	pid_t i;
	int count;
	char buf[256];

	count = 0;
	if (pids != NULL)
		*pids = NULL;
	if ((p = strrchr(name, '/')) != NULL) name = p + 1;
	if ((dir = opendir("/proc")) != NULL) {
		while ((de = readdir(dir)) != NULL) {
			i = strtol(de->d_name, &e, 10);
			if (*e != 0) continue;
			if (strcmp(name, psname(i, buf, sizeof(buf))) == 0) {
				if (pids == NULL) {
					count = i;
					break;
				}
				if ((*pids = realloc(*pids, sizeof(pid_t) * (count + 1))) == NULL) {
					return -1;
				}
				(*pids)[count++] = i;
			}
		}
	}
	closedir(dir);
	return count;
}

/* If we hit both windows, it will take three tries to discover the pid. */
int pidof(const char *name)
{
	pid_t p;

	p = _pidof(name, NULL);
	if (p < 1) {
		usleep(10 * 1000);
		p = _pidof(name, NULL);
		if (p < 1)
			p = _pidof(name, NULL);
	}
	if (p < 1)
		return -1;
	return p;
}

int ppid(int pid) {
	char buf[512];
	char path[64];
	int ppid = 0;

	buf[0] = 0;
	sprintf(path, "/proc/%d/stat", pid);
	if ((f_read_string(path, buf, sizeof(buf)) > 4))
		sscanf(buf, "%*d %*s %*c %d", &ppid);

	return ppid;
}

int killall(const char *name, int sig)
{
	pid_t *pids;
	int i;
	int r;

	if ((i = _pidof(name, &pids)) > 0) {
		r = 0;
		do {
			r |= kill(pids[--i], sig);
		} while (i > 0);
		free(pids);
		return r;
	}
	return -2;
}


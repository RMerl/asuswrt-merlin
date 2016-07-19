#define _GNU_SOURCE

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/klog.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <dirent.h>
//#include <wlutils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <httpd.h>

#define nvram_match(name, match) ({ \
	const char *value = nvram_get(name); \
	(value && !strcmp(value, match)); \
})

#define FW_CREATE	0
#define FW_APPEND	1
#define FW_NEWLINE	2

#define _dprintf(args...)	do { } while(0)


/*
typedef union {
	int i;
	long l;
	const char *s;
} nvset_varg_t;


typedef struct {
	const char *name;
	enum {
		VT_LENGTH,		// check length of string
		VT_RANGE,		// expect an integer, check range
	} vtype;
	nvset_varg_t va;
	nvset_varg_t vb;
} nvset_t;


#define V_01				VT_RANGE,	{ .l = 0 },		{ .l = 1 }
#define V_LENGTH(min, max)	VT_LENGTH,	{ .i = min },	{ .i = max }
#define V_RANGE(min, max)	VT_RANGE,	{ .l = min },	{ .l = max }
#define _dprintf(args...)	do { } while(0)

static const nvset_t nvset_list[] = {
// admin-bwm
	{ "rstats_enable",		V_01				},
	{ "rstats_path",		V_LENGTH(0, 48)		},
	{ "rstats_stime",		V_RANGE(1, 168)		},
	{ "rstats_offset",		V_RANGE(1, 31)		},
	{ "rstats_exclude",		V_LENGTH(0, 64)		},
	{ "rstats_sshut",		V_01				},
	{ "rstats_bak",			V_01				},
	{ NULL }
};
*/

FILE *connfp;

// for backup =========================================================

int web_eat(int max)
{
	char buf[512];
	int n;
	while (max > 0) {
		 if ((n = web_read(buf, (max < sizeof(buf)) ? max : sizeof(buf))) <= 0) return 0;
		 max -= n;
	}
	return 1;
}

int f_write(const char *path, const void *buffer, int len, unsigned flags, unsigned cmode)
{
	static const char nl = '\n';
	int f;
	int r = -1;
	mode_t m;

	m = umask(0);
	if (cmode == 0) cmode = 0666;
	if ((f = open(path, (flags & FW_APPEND) ? (O_WRONLY|O_CREAT|O_APPEND) : (O_WRONLY|O_CREAT|O_TRUNC), cmode)) >= 0) {
		if ((buffer == NULL) || ((r = write(f, buffer, len)) == len)) {
			if (flags & FW_NEWLINE) {
				if (write(f, &nl, 1) == 1) ++r;
			}
		}
		close(f);
	}
	umask(m);
	return r;
}

const char *resmsg_get(void)
{
	return get_cgi("resmsg");
}

void resmsg_set(const char *msg)
{
	set_cgi("resmsg", strdup(msg));	// m ok
}

// for bandwidth =========================================================

int f_exists(const char *path)	// note: anything but a directory
{
	struct stat st;
	return (stat(path, &st) == 0) && (!S_ISDIR(st.st_mode));
}

static int _f_wait_exists(const char *name, int max, int invert)
{
	while (max-- > 0) {
		if (f_exists(name) ^ invert) return 1;
		sleep(1);
	}
	return 0;
}

int f_wait_exists(const char *name, int max)
{
	return _f_wait_exists(name, max, 0);
}

int f_read(const char *path, void *buffer, int max)
{
	int f;
	int n;

	if ((f = open(path, O_RDONLY)) < 0) return -1;
	n = read(f, buffer, max);
	close(f);
	return n;
}

int f_read_string(const char *path, char *buffer, int max)
{
	if (max <= 0) return -1;
	int n = f_read(path, buffer, max - 1);
	buffer[(n > 0) ? n : 0] = 0;
	return n;
}

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

static int _pidof(const char *name, pid_t** pids)
{
	const char *p;
	char *e;
	DIR *dir;
	struct dirent *de;
	pid_t i;
	int count;
	char buf[256];

	count = 0;
	*pids = NULL;
	if ((p = strchr(name, '/')) != NULL) name = p + 1;
	if ((dir = opendir("/proc")) != NULL) {
		while ((de = readdir(dir)) != NULL) {
			i = strtol(de->d_name, &e, 10);
			if (*e != 0) continue;
			if (strcmp(name, psname(i, buf, sizeof(buf))) == 0) {
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

void do_f(char *path, webs_t wp)
{
	FILE *f;
	char buf[1024];
	int ret;

//printf("do_f: %s\n", path);
	if ((f = fopen(path, "r")) != NULL) {
//		while ((nr = fread(buf, 1, sizeof(buf), f)) > 0){
		while (fgets(buf, sizeof(buf), f) != NULL){
//printf("%s\n", buf);
			ret += websWrite(wp, buf);
		}
		fclose(f);	
	}
	return;
}

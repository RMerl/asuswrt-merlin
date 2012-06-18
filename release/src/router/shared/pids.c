/*
 * find_pid_by_name from busybox
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <ctype.h>

enum {
        PSSCAN_PID      = 1 << 0,
        PSSCAN_PPID     = 1 << 1,
        PSSCAN_PGID     = 1 << 2,
        PSSCAN_SID      = 1 << 3,
        PSSCAN_UIDGID   = 1 << 4,
        PSSCAN_COMM     = 1 << 5,
        /* PSSCAN_CMD      = 1 << 6, - use read_cmdline instead */
        PSSCAN_ARGV0    = 1 << 7,
        /* PSSCAN_EXE      = 1 << 8, - not implemented */
        PSSCAN_STATE    = 1 << 9,
        PSSCAN_VSZ      = 1 << 10,
        PSSCAN_RSS      = 1 << 11,
        PSSCAN_STIME    = 1 << 12,
        PSSCAN_UTIME    = 1 << 13,
        PSSCAN_TTY      = 1 << 14,
        PSSCAN_SMAPS    = (1 << 15) * 0,
        PSSCAN_ARGVN    = (1 << 16) * 1,
        PSSCAN_START_TIME = 1 << 18,
        /* These are all retrieved from proc/NN/stat in one go: */
        PSSCAN_STAT     = PSSCAN_PPID | PSSCAN_PGID | PSSCAN_SID
                        | PSSCAN_COMM | PSSCAN_STATE
                        | PSSCAN_VSZ | PSSCAN_RSS
                        | PSSCAN_STIME | PSSCAN_UTIME | PSSCAN_START_TIME
                        | PSSCAN_TTY,
};

/*
In Linux we have three ways to determine "process name":
1. /proc/PID/stat has "...(name)...", among other things. It's so-called "comm" field.
2. /proc/PID/cmdline's first NUL-terminated string. It's argv[0] from exec syscall.
3. /proc/PID/exe symlink. Points to the running executable file.

kernel threads:
 comm: thread name
 cmdline: empty
 exe: <readlink fails>

executable
 comm: first 15 chars of base name
 (if executable is a symlink, then first 15 chars of symlink name are used)
 cmdline: argv[0] from exec syscall
 exe: points to executable (resolves symlink, unlike comm)

script (an executable with #!/path/to/interpreter):
 comm: first 15 chars of script's base name (symlinks are not resolved)
 cmdline: /path/to/interpreter (symlinks are not resolved)
 (script name is in argv[1], args are pushed into argv[2] etc)
 exe: points to interpreter's executable (symlinks are resolved)

If FEATURE_PREFER_APPLETS=y (and more so if FEATURE_SH_STANDALONE=y),
some commands started from busybox shell, xargs or find are started by
execXXX("/proc/self/exe", applet_name, params....)
and therefore comm field contains "exe".
*/

#define PROCPS_BUFSIZE 1024

static int read_to_buf(const char *filename, void *buf)
{
	int fd;
	/* open_read_close() would do two reads, checking for EOF.
	 * When you have 10000 /proc/$NUM/stat to read, it isn't desirable */
	int ret = -1;
	fd = open(filename, O_RDONLY);
	if (fd >= 0) {
		ret = read(fd, buf, PROCPS_BUFSIZE-1);
		close(fd);
	}
	((char *)buf)[ret > 0 ? ret : 0] = '\0';
	return ret;
}

void* xzalloc(size_t size)
{
        void *ptr = malloc(size);
        memset(ptr, 0, size);
        return ptr;
}

typedef struct procps_status_t {
        DIR *dir;
        unsigned char shift_pages_to_bytes;
        unsigned char shift_pages_to_kb;
/* Fields are set to 0/NULL if failed to determine (or not requested) */
        unsigned int argv_len;
        char *argv0;
        /* Everything below must contain no ptrs to malloc'ed data:
         * it is memset(0) for each process in procps_scan() */
        unsigned long vsz, rss; /* we round it to kbytes */
        unsigned long stime, utime;
        unsigned long start_time;
        unsigned pid;
        unsigned ppid;
        unsigned pgid;
        unsigned sid;
        unsigned uid;
        unsigned gid;
        unsigned tty_major,tty_minor;
        char state[4];
        /* basename of executable in exec(2), read from /proc/N/stat
         * (if executable is symlink or script, it is NOT replaced
         * by link target or interpreter name) */
        char comm[16];
        /* user/group? - use passwd/group parsing functions */
} procps_status_t;

static procps_status_t* alloc_procps_scan(void)
{
	unsigned n = getpagesize();
	procps_status_t* sp = xzalloc(sizeof(procps_status_t));
	sp->dir = opendir("/proc");
	while (1) {
		n >>= 1;
		if (!n) break;
		sp->shift_pages_to_bytes++;
	}
	sp->shift_pages_to_kb = sp->shift_pages_to_bytes - 10;
	return sp;
}

void BUG_comm_size(void)
{
}

#define ULLONG_MAX     (~0ULL)
#define UINT_MAX       (~0U)

static unsigned long long ret_ERANGE(void)
{
        errno = ERANGE; /* this ain't as small as it looks (on glibc) */
        return ULLONG_MAX;
}

static unsigned long long handle_errors(unsigned long long v, char **endp, char *endptr)
{
        if (endp) *endp = endptr;

        /* errno is already set to ERANGE by strtoXXX if value overflowed */
        if (endptr[0]) {
                /* "1234abcg" or out-of-range? */
                if (isalnum(endptr[0]) || errno)
                        return ret_ERANGE();
                /* good number, just suspicious terminator */
                errno = EINVAL;
        }
        return v;
}

unsigned bb_strtou(const char *arg, char **endp, int base)
{
        unsigned long v;
        char *endptr;

        if (!isalnum(arg[0])) return ret_ERANGE();
        errno = 0;
        v = strtoul(arg, &endptr, base);
        if (v > UINT_MAX) return ret_ERANGE();
        return handle_errors(v, endp, endptr);
}

unsigned long long bb_strtoull(const char *arg, char **endp, int base)
{
        unsigned long long v;
        char *endptr;

        /* strtoul("  -4200000000") returns 94967296, errno 0 (!) */
        /* I don't think that this is right. Preventing this... */
        if (!isalnum(arg[0])) return ret_ERANGE();

        /* not 100% correct for lib func, but convenient for the caller */
        errno = 0;
        v = strtoull(arg, &endptr, base);
        return handle_errors(v, endp, endptr);
}

void* xrealloc(void *ptr, size_t size)
{
        ptr = realloc(ptr, size);
        if (ptr == NULL && size != 0)
                perror("no memory");
        return ptr;
}

void* xrealloc_vector_helper(void *vector, unsigned sizeof_and_shift, int idx)
{
        int mask = 1 << (unsigned char)sizeof_and_shift;

        if (!(idx & (mask - 1))) {
                sizeof_and_shift >>= 8; /* sizeof(vector[0]) */
                vector = xrealloc(vector, sizeof_and_shift * (idx + mask + 1));
                memset((char*)vector + (sizeof_and_shift * idx), 0, sizeof_and_shift * (mask + 1));
        }
        return vector;
}

#define xrealloc_vector(vector, shift, idx) \
        xrealloc_vector_helper((vector), (sizeof((vector)[0]) << 8) + (shift), (idx))

void free_procps_scan(procps_status_t* sp)
{
	closedir(sp->dir);
	free(sp->argv0);
	free(sp);
}

procps_status_t* procps_scan(procps_status_t* sp, int flags)
{
	struct dirent *entry;
	char buf[PROCPS_BUFSIZE];
	char filename[sizeof("/proc//cmdline") + sizeof(int)*3];
	char *filename_tail;
	long tasknice;
	unsigned pid;
	int n;
	struct stat sb;

	if (!sp)
		sp = alloc_procps_scan();

	for (;;) {
		entry = readdir(sp->dir);
		if (entry == NULL) {
			free_procps_scan(sp);
			return NULL;
		}
		pid = bb_strtou(entry->d_name, NULL, 10);
		if (errno)
			continue;

		/* After this point we have to break, not continue
		 * ("continue" would mean that current /proc/NNN
		 * is not a valid process info) */

		memset(&sp->vsz, 0, sizeof(*sp) - offsetof(procps_status_t, vsz));

		sp->pid = pid;
		if (!(flags & ~PSSCAN_PID)) break;

		filename_tail = filename + sprintf(filename, "/proc/%d", pid);

		if (flags & PSSCAN_UIDGID) {
			if (stat(filename, &sb))
				break;
			/* Need comment - is this effective or real UID/GID? */
			sp->uid = sb.st_uid;
			sp->gid = sb.st_gid;
		}

		if (flags & PSSCAN_STAT) {
			char *cp, *comm1;
			int tty;
			unsigned long vsz, rss;

			/* see proc(5) for some details on this */
			strcpy(filename_tail, "/stat");
			n = read_to_buf(filename, buf);
			if (n < 0)
				break;
			cp = strrchr(buf, ')'); /* split into "PID (cmd" and "<rest>" */
			/*if (!cp || cp[1] != ' ')
				break;*/
			cp[0] = '\0';
			if (sizeof(sp->comm) < 16)
				BUG_comm_size();
			comm1 = strchr(buf, '(');
			/*if (comm1)*/
				strncpy(sp->comm, comm1 + 1, sizeof(sp->comm));

			n = sscanf(cp+2,
				"%c %u "               /* state, ppid */
				"%u %u %d %*s "        /* pgid, sid, tty, tpgid */
				"%*s %*s %*s %*s %*s " /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
				"%lu %lu "             /* utime, stime */
				"%*s %*s %*s "         /* cutime, cstime, priority */
				"%ld "                 /* nice */
				"%*s %*s "             /* timeout, it_real_value */
				"%lu "                 /* start_time */
				"%lu "                 /* vsize */
				"%lu "                 /* rss */
			/*	"%lu %lu %lu %lu %lu %lu " rss_rlim, start_code, end_code, start_stack, kstk_esp, kstk_eip */
			/*	"%u %u %u %u "         signal, blocked, sigignore, sigcatch */
			/*	"%lu %lu %lu"          wchan, nswap, cnswap */
				,
				sp->state, &sp->ppid,
				&sp->pgid, &sp->sid, &tty,
				&sp->utime, &sp->stime,
				&tasknice,
				&sp->start_time,
				&vsz,
				&rss);
			if (n != 11)
				break;
			/* vsz is in bytes and we want kb */
			sp->vsz = vsz >> 10;
			/* vsz is in bytes but rss is in *PAGES*! Can you believe that? */
			sp->rss = rss << sp->shift_pages_to_kb;
			sp->tty_major = (tty >> 8) & 0xfff;
			sp->tty_minor = (tty & 0xff) | ((tty >> 12) & 0xfff00);

			if (sp->vsz == 0 && sp->state[0] != 'Z')
				sp->state[1] = 'W';
			else
				sp->state[1] = ' ';
			if (tasknice < 0)
				sp->state[2] = '<';
			else if (tasknice) /* > 0 */
				sp->state[2] = 'N';
			else
				sp->state[2] = ' ';

		}

		if (flags & (PSSCAN_ARGV0|PSSCAN_ARGVN)) {
			free(sp->argv0);
			sp->argv0 = NULL;
			strcpy(filename_tail, "/cmdline");
			n = read_to_buf(filename, buf);
			if (n <= 0)
				break;
			if (flags & PSSCAN_ARGVN) {
				sp->argv_len = n;
				sp->argv0 = malloc(n + 1);
				memcpy(sp->argv0, buf, n + 1);
				/* sp->argv0[n] = '\0'; - buf has it */
			} else {
				sp->argv_len = 0;
				sp->argv0 = strdup(buf);
			}
		}
		break;
	}
	return sp;
}

const char* bb_basename(const char *name)
{
        const char *cp = strrchr(name, '/');
        if (cp)
                return cp + 1;
        return name;
}

static int comm_match(procps_status_t *p, const char *procName)
{
	int argv1idx;

	/* comm does not match */
	if (strncmp(p->comm, procName, 15) != 0)
		return 0;

	/* in Linux, if comm is 15 chars, it may be a truncated */
	if (p->comm[14] == '\0') /* comm is not truncated - match */
		return 1;

	/* comm is truncated, but first 15 chars match.
	 * This can be crazily_long_script_name.sh!
	 * The telltale sign is basename(argv[1]) == procName. */

	if (!p->argv0)
		return 0;

	argv1idx = strlen(p->argv0) + 1;
	if (argv1idx >= p->argv_len)
		return 0;

	if (strcmp(bb_basename(p->argv0 + argv1idx), procName) != 0)
		return 0;

	return 1;
}

/* find_pid_by_name()
 *
 *  Modified by Vladimir Oleynik for use with libbb/procps.c
 *  This finds the pid of the specified process.
 *  Currently, it's implemented by rummaging through
 *  the proc filesystem.
 *
 *  Returns a list of all matching PIDs
 *  It is the caller's duty to free the returned pidlist.
 */
pid_t* find_pid_by_name(const char *procName)
{
	pid_t* pidList;
	int i = 0;
	procps_status_t* p = NULL;

	pidList = xzalloc(sizeof(*pidList));
	while ((p = procps_scan(p, PSSCAN_PID|PSSCAN_COMM|PSSCAN_ARGVN))) {
		if (comm_match(p, procName)
		/* or we require argv0 to match (essential for matching reexeced /proc/self/exe)*/
		 || (p->argv0 && strcmp(bb_basename(p->argv0), procName) == 0)
		/* TOOD: we can also try /proc/NUM/exe link, do we want that? */
		) {
			if (p->state[0] != 'Z')
			{
				pidList = xrealloc_vector(pidList, 2, i);
				pidList[i++] = p->pid;
			}
		}
	}

	pidList[i] = 0;
	return pidList;
}

int pids_main(char *appname)
{
	pid_t *pidList;
	pid_t *pl;
	int count = 0;

	pidList = find_pid_by_name(appname);
	for (pl = pidList; *pl; pl++) {
		count++;
		fprintf(stderr, "%u ", (unsigned)*pl);
	}
	if (count) fprintf(stderr, "\n");
	free(pidList);

	fprintf(stderr, "pid count: %d\n", count);

	return count;
}

int pids(char *appname)
{
	pid_t *pidList;
	pid_t *pl;
	int count = 0;

	pidList = find_pid_by_name(appname);
	for (pl = pidList; *pl; pl++) {
		count++;
	}
	free(pidList);

	if (count)
		return 1;
	else
		return 0;
}

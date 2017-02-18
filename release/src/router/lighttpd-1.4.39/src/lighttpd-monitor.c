#ifdef APP_IPKG
#include "log.h"
#endif
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#ifdef EMBEDDED_EANBLE 
#ifndef APP_IPKG
#include <utils.h>
#include <shutils.h>
#include <shared.h>
#endif
#include "nvram_control.h"
#endif
#ifdef APP_IPKG
/*pids()*/
#include<sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <stddef.h>
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
/*end pids()*/

/*??*/
enum {
        ACT_IDLE,
        ACT_TFTP_UPGRADE_UNUSED,
        ACT_WEB_UPGRADE,
        ACT_WEBS_UPGRADE_UNUSED,
        ACT_SW_RESTORE,
        ACT_HW_RESTORE,
        ACT_ERASE_NVRAM,
        ACT_NVRAM_COMMIT,
        ACT_REBOOT,
        ACT_UNKNOWN
};
/*??*/
/*check_action*/
#include <fcntl.h>
#ifdef DEBUG_NOISY
#define _dprintf		cprintf
#define csprintf		cprintf
#else
#define _dprintf(args...)	do { } while(0)
#define csprintf(args...)	do { } while(0)
#endif

int check_action(void)
{
        int a;
        int r = 3;

        while (f_read("/var/lock/action", &a, sizeof(a)) != sizeof(a)) {
                sleep(1);
                if (--r == 0) return ACT_UNKNOWN;
        }
        _dprintf("check_action %d\n", a);

        return a;
}
/**/
#endif
#define DBE 0
#define LIGHTTPD_PID_FILE_PATH	"/tmp/lighttpd/lighttpd.pid"
#define LIGHTTPD_MONITOR_PID_FILE_PATH	"/tmp/lighttpd/lighttpd-monitor.pid"
#define LIGHTTPD_ARPPING_PID_FILE_PATH	"/tmp/lighttpd/lighttpd-arpping.pid"

static siginfo_t last_sigterm_info;
static siginfo_t last_sighup_info;

static volatile sig_atomic_t start_process    = 1;
static volatile sig_atomic_t graceful_restart = 0;
static volatile pid_t pid = -1;

#define BINPATH SBIN_DIR"/lighttpd-monitor"
#define UNUSED(x) ( (void)(x) )

int is_shutdown = 0;

static void sigaction_handler(int sig, siginfo_t *si, void *context) {
	static siginfo_t empty_siginfo;
	UNUSED(context);
	
	if (!si) si = &empty_siginfo;

	switch (sig) {
	case SIGTERM:		
		is_shutdown = 1;
		break;
	case SIGINT:
		break;
	case SIGALRM: 
		break;
	case SIGHUP:
		break;
	case SIGCHLD:
		break;
	}
}

void stop_arpping_process()
{
	FILE *fp;
    char buf[256];
    pid_t pid = 0;
    int n;

	if ((fp = fopen(LIGHTTPD_ARPPING_PID_FILE_PATH, "r")) != NULL) {
		if (fgets(buf, sizeof(buf), fp) != NULL)
	    	pid = strtoul(buf, NULL, 0);
		fclose(fp);
	}
	
	if (pid > 1 && kill(pid, SIGTERM) == 0) {
		n = 10;	       
		while ((kill(pid, SIGTERM) == 0) && (n-- > 0)) {
			//Cdbg(DBE,"Mod_smbdav: %s: waiting pid=%d n=%d\n", __FUNCTION__, pid, n);
			usleep(100 * 1000);
		}
	}

	unlink(LIGHTTPD_ARPPING_PID_FILE_PATH);
}

int main(int argc, char **argv) {
	
	UNUSED(argc);

	//- Check if same process is running.
	FILE *fp = fopen(LIGHTTPD_MONITOR_PID_FILE_PATH, "r");
	if (fp) {
		fclose(fp);
		return 0;
	}
	
	//- Write PID file
	pid_t pid = getpid();
	fp = fopen(LIGHTTPD_MONITOR_PID_FILE_PATH, "w");
	if (!fp) {
		exit(EXIT_FAILURE);
	}
	fprintf(fp, "%d\n", pid);
	fclose(fp);
	
#if EMBEDDED_EANBLE
	sigset_t sigs_to_catch;

	/* set the signal handler */
    sigemptyset(&sigs_to_catch);
    sigaddset(&sigs_to_catch, SIGTERM);
    sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

   	signal(SIGTERM, sigaction_handler);  
#else
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGUSR1, &act, NULL);

	act.sa_sigaction = sigaction_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;

	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGHUP,  &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGCHLD, &act, NULL);	
#endif

	time_t prv_ts = time(NULL);

	int stop_arp_count = 0;
	int commit_count = 0;
	
	while (!is_shutdown) {
		
		sleep(10);

		int start_lighttpd = 0;
		int start_lighttpd_arp = 0;
	
		time_t cur_ts = time(NULL);
		
	#if EMBEDDED_EANBLE
  		if (!pids("lighttpd")){
			start_lighttpd = 1;
  		}

		if (!pids("lighttpd-arpping")){
			start_lighttpd_arp = 1;
  		}
	#else
		if (!system("pidof lighttpd")){
			start_lighttpd = 1;
		}

		if (!system("pidof lighttpd-arpping")){
			start_lighttpd_arp = 1;
		}
	#endif
		
		//-every 30 sec 
		if(cur_ts - prv_ts >= 30){
	
			if(start_lighttpd){
			#if EMBEDDED_EANBLE
			#ifndef APP_IPKG
				system("/usr/sbin/lighttpd -f /tmp/lighttpd.conf -D &");
			#else
				system("/opt/bin/lighttpd -f /tmp/lighttpd.conf -D &");
			#endif
			#else
				system("./_inst/sbin/lighttpd -f lighttpd.conf &");
			#endif
			}

			if(start_lighttpd_arp){
			#if EMBEDDED_EANBLE
			#ifndef APP_IPKG
				system("/usr/sbin/lighttpd-arpping -f br0 &");
			#else
			#if defined I686
				system("/opt/bin/lighttpd-arpping -f br2 &");
			#else
				system("/opt/bin/lighttpd-arpping -f br0 &");
			#endif
			#endif
			#else
				system("./_inst/sbin/lighttpd-arpping -f eth0 &");
			#endif
			}

			#if 0
			//-every 2 hour
			if(stop_arp_count>=240){
				stop_arpping_process();
				stop_arp_count=0;
			}
			#endif
			
			//-every 12 hour
			if(commit_count>=1440){				

				#if EMBEDDED_EANBLE
				int i, act;
				for (i = 30; i > 0; --i) {
			    	if (((act = check_action()) == ACT_IDLE) || (act == ACT_REBOOT)) break;
			        fprintf(stderr, "Busy with %d. Waiting before shutdown... %d", act, i);
			        sleep(1);
			    }
					
				nvram_do_commit();
				#endif
				
				commit_count=0;
			}
			
			prv_ts = cur_ts;
			stop_arp_count++;
			commit_count++;
		}
	}

	//Cdbg(DBE, "Success to terminate lighttpd-monitor.....");
	
	exit(EXIT_SUCCESS);
	
	return 0;
}


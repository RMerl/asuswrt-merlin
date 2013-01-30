/* vi: set sw=4 ts=4: */
/*
 * tiny fuser implementation
 *
 * Copyright 2004 Tony J. White
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#define MAX_LINE 255

#define OPTION_STRING "mks64"
enum {
	OPT_MOUNT  = (1 << 0),
	OPT_KILL   = (1 << 1),
	OPT_SILENT = (1 << 2),
	OPT_IP6    = (1 << 3),
	OPT_IP4    = (1 << 4),
};

typedef struct inode_list {
	struct inode_list *next;
	ino_t inode;
	dev_t dev;
} inode_list;

typedef struct pid_list {
	struct pid_list *next;
	pid_t pid;
} pid_list;


struct globals {
	pid_list *pid_list_head;
	inode_list *inode_list_head;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define INIT_G() do { } while (0)


static void add_pid(const pid_t pid)
{
	pid_list **curr = &G.pid_list_head;

	while (*curr) {
		if ((*curr)->pid == pid)
			return;
		curr = &(*curr)->next;
	}

	*curr = xzalloc(sizeof(pid_list));
	(*curr)->pid = pid;
}

static void add_inode(const struct stat *st)
{
	inode_list **curr = &G.inode_list_head;

	while (*curr) {
		if ((*curr)->dev == st->st_dev
		 && (*curr)->inode == st->st_ino
		) {
			return;
		}
		curr = &(*curr)->next;
	}

	*curr = xzalloc(sizeof(inode_list));
	(*curr)->dev = st->st_dev;
	(*curr)->inode = st->st_ino;
}

static void scan_proc_net(const char *path, unsigned port)
{
	char line[MAX_LINE + 1];
	long long uint64_inode;
	unsigned tmp_port;
	FILE *f;
	struct stat st;
	int fd;

	/* find socket dev */
	st.st_dev = 0;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd >= 0) {
		fstat(fd, &st);
		close(fd);
	}

	f = fopen_for_read(path);
	if (!f)
		return;

	while (fgets(line, MAX_LINE, f)) {
		char addr[68];
		if (sscanf(line, "%*d: %64[0-9A-Fa-f]:%x %*x:%*x %*x %*x:%*x "
				"%*x:%*x %*x %*d %*d %llu",
				addr, &tmp_port, &uint64_inode) == 3
		) {
			int len = strlen(addr);
			if (len == 8 && (option_mask32 & OPT_IP6))
				continue;
			if (len > 8 && (option_mask32 & OPT_IP4))
				continue;
			if (tmp_port == port) {
				st.st_ino = uint64_inode;
				add_inode(&st);
			}
		}
	}
	fclose(f);
}

static int search_dev_inode(const struct stat *st)
{
	inode_list *ilist = G.inode_list_head;

	while (ilist) {
		if (ilist->dev == st->st_dev) {
			if (option_mask32 & OPT_MOUNT)
				return 1;
			if (ilist->inode == st->st_ino)
				return 1;
		}
		ilist = ilist->next;
	}
	return 0;
}

static void scan_pid_maps(const char *fname, pid_t pid)
{
	FILE *file;
	char line[MAX_LINE + 1];
	int major, minor;
	long long uint64_inode;
	struct stat st;

	file = fopen_for_read(fname);
	if (!file)
		return;

	while (fgets(line, MAX_LINE, file)) {
		if (sscanf(line, "%*s %*s %*s %x:%x %llu", &major, &minor, &uint64_inode) != 3)
			continue;
		st.st_ino = uint64_inode;
		if (major == 0 && minor == 0 && st.st_ino == 0)
			continue;
		st.st_dev = makedev(major, minor);
		if (search_dev_inode(&st))
			add_pid(pid);
	}
	fclose(file);
}

static void scan_link(const char *lname, pid_t pid)
{
	struct stat st;

	if (stat(lname, &st) >= 0) {
		if (search_dev_inode(&st))
			add_pid(pid);
	}
}

static void scan_dir_links(const char *dname, pid_t pid)
{
	DIR *d;
	struct dirent *de;
	char *lname;

	d = opendir(dname);
	if (!d)
		return;

	while ((de = readdir(d)) != NULL) {
		lname = concat_subpath_file(dname, de->d_name);
		if (lname == NULL)
			continue;
		scan_link(lname, pid);
		free(lname);
	}
	closedir(d);
}

/* NB: does chdir internally */
static void scan_proc_pids(void)
{
	DIR *d;
	struct dirent *de;
	pid_t pid;

	xchdir("/proc");
	d = opendir("/proc");
	if (!d)
		return;

	while ((de = readdir(d)) != NULL) {
		pid = (pid_t)bb_strtou(de->d_name, NULL, 10);
		if (errno)
			continue;
		if (chdir(de->d_name) < 0)
			continue;
		scan_link("cwd", pid);
		scan_link("exe", pid);
		scan_link("root", pid);

		scan_dir_links("fd", pid);
		scan_dir_links("lib", pid);
		scan_dir_links("mmap", pid);

		scan_pid_maps("maps", pid);
		xchdir("/proc");
	}
	closedir(d);
}

int fuser_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int fuser_main(int argc UNUSED_PARAM, char **argv)
{
	pid_list *plist;
	pid_t mypid;
	char **pp;
	struct stat st;
	unsigned port;
	int opt;
	int exitcode;
	int killsig;
/*
fuser [OPTIONS] FILE or PORT/PROTO
Find processes which use FILEs or PORTs
        -m      Find processes which use same fs as FILEs
        -4      Search only IPv4 space
        -6      Search only IPv6 space
        -s      Don't display PIDs
        -k      Kill found processes
        -SIGNAL Signal to send (default: KILL)
*/
	/* Handle -SIGNAL. Oh my... */
	killsig = SIGKILL; /* yes, the default is not SIGTERM */
	pp = argv;
	while (*++pp) {
		char *arg = *pp;
		if (arg[0] != '-')
			continue;
		if (arg[1] == '-' && arg[2] == '\0') /* "--" */
			break;
		if ((arg[1] == '4' || arg[1] == '6') && arg[2] == '\0')
			continue; /* it's "-4" or "-6" */
		opt = get_signum(&arg[1]);
		if (opt < 0)
			continue;
		/* "-SIGNAL" option found. Remove it and bail out */
		killsig = opt;
		do {
			pp[0] = arg = pp[1];
			pp++;
		} while (arg);
		break;
	}

	opt_complementary = "-1"; /* at least one param */
	opt = getopt32(argv, OPTION_STRING);
	argv += optind;

	pp = argv;
	while (*pp) {
		/* parse net arg */
		char path[20], tproto[5];
		if (sscanf(*pp, "%u/%4s", &port, tproto) != 2)
			goto file;
		sprintf(path, "/proc/net/%s", tproto);
		if (access(path, R_OK) != 0) { /* PORT/PROTO */
			scan_proc_net(path, port);
		} else { /* FILE */
 file:
			xstat(*pp, &st);
			add_inode(&st);
		}
		pp++;
	}

	scan_proc_pids(); /* changes dir to "/proc" */

	mypid = getpid();
	plist = G.pid_list_head;
	while (1) {
		if (!plist)
			return EXIT_FAILURE;
		if (plist->pid != mypid)
			break;
		plist = plist->next;
	}

	exitcode = EXIT_SUCCESS;
	do {
		if (plist->pid != mypid) {
			if (opt & OPT_KILL) {
				if (kill(plist->pid, killsig) != 0) {
					bb_perror_msg("kill pid %u", (unsigned)plist->pid);
					exitcode = EXIT_FAILURE;
				}
			}
			if (!(opt & OPT_SILENT)) {
				printf("%u ", (unsigned)plist->pid);
			}
		}
		plist = plist->next;
	} while (plist);

	if (!(opt & (OPT_SILENT))) {
		bb_putchar('\n');
	}

	return exitcode;
}

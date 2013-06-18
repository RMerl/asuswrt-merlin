/*
 * pfsck --- A generic, parallelizing front-end for the fsck program.
 * It will automatically try to run fsck programs in parallel if the
 * devices are on separate spindles.  It is based on the same ideas as
 * the generic front end for fsck by David Engel and Fred van Kempen,
 * but it has been completely rewritten from scratch to support
 * parallel execution.
 *
 * Written by Theodore Ts'o, <tytso@mit.edu>
 *
 * Miquel van Smoorenburg (miquels@drinkel.ow.org) 20-Oct-1994:
 *   o Changed -t fstype to behave like with mount when -A (all file
 *     systems) or -M (like mount) is specified.
 *   o fsck looks if it can find the fsck.type program to decide
 *     if it should ignore the fs type. This way more fsck programs
 *     can be added without changing this front-end.
 *   o -R flag skip root file system.
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 * 	2001, 2002, 2003, 2004, 2005 by  Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#define _XOPEN_SOURCE 600 /* for inclusion of sa_handler in Solaris */

#include "config.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_PATHS_H
#include <paths.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include "../version.h"
#include "nls-enable.h"
#include "fsck.h"
#include "blkid/blkid.h"

#ifndef _PATH_MNTTAB
#define	_PATH_MNTTAB	"/etc/fstab"
#endif

static const char *ignored_types[] = {
	"ignore",
	"iso9660",
	"nfs",
	"proc",
	"sw",
	"swap",
	"tmpfs",
	"devpts",
	NULL
};

static const char *really_wanted[] = {
	"minix",
	"ext2",
	"ext3",
	"ext4",
	"ext4dev",
	"jfs",
	"reiserfs",
	"xiafs",
	"xfs",
	NULL
};

#define BASE_MD "/dev/md"

/*
 * Global variables for options
 */
char *devices[MAX_DEVICES];
char *args[MAX_ARGS];
int num_devices, num_args;

int verbose = 0;
int doall = 0;
int noexecute = 0;
int serialize = 0;
int skip_root = 0;
int ignore_mounted = 0;
int notitle = 0;
int parallel_root = 0;
int progress = 0;
int progress_fd = 0;
int force_all_parallel = 0;
int num_running = 0;
int max_running = 0;
volatile int cancel_requested = 0;
int kill_sent = 0;
char *progname;
char *fstype = NULL;
struct fs_info *filesys_info = NULL, *filesys_last = NULL;
struct fsck_instance *instance_list;
const char *fsck_prefix_path = "/sbin:/sbin/fs.d:/sbin/fs:/etc/fs:/etc";
char *fsck_path = 0;
blkid_cache cache = NULL;

static char *string_copy(const char *s)
{
	char	*ret;

	if (!s)
		return 0;
	ret = malloc(strlen(s)+1);
	if (ret)
		strcpy(ret, s);
	return ret;
}

static int string_to_int(const char *s)
{
	long l;
	char *p;

	l = strtol(s, &p, 0);
	if (*p || l == LONG_MIN || l == LONG_MAX || l < 0 || l > INT_MAX)
		return -1;
	else
		return (int) l;
}

static int ignore(struct fs_info *);

static char *skip_over_blank(char *cp)
{
	while (*cp && isspace(*cp))
		cp++;
	return cp;
}

static char *skip_over_word(char *cp)
{
	while (*cp && !isspace(*cp))
		cp++;
	return cp;
}

static void strip_line(char *line)
{
	char	*p;

	while (*line) {
		p = line + strlen(line) - 1;
		if ((*p == '\n') || (*p == '\r'))
			*p = 0;
		else
			break;
	}
}

static char *parse_word(char **buf)
{
	char *word, *next;

	word = *buf;
	if (*word == 0)
		return 0;

	word = skip_over_blank(word);
	next = skip_over_word(word);
	if (*next)
		*next++ = 0;
	*buf = next;
	return word;
}

static void parse_escape(char *word)
{
	char	*p, *q;
	int	ac, i;

	if (!word)
		return;

	for (p = word, q = word; *p; p++, q++) {
		*q = *p;
		if (*p != '\\')
			continue;
		if (*++p == 0)
			break;
		if (*p == 't') {
			*q = '\t';
			continue;
		}
		if (*p == 'n') {
			*q = '\n';
			continue;
		}
		if (!isdigit(*p)) {
			*q = *p;
			continue;
		}
		ac = 0;
		for (i = 0; i < 3; i++, p++) {
			if (!isdigit(*p))
				break;
			ac = (ac * 8) + (*p - '0');
		}
		*q = ac;
		p--;
	}
	*q = 0;
}

static void free_instance(struct fsck_instance *i)
{
	free(i->prog);
	free(i->device);
	free(i->base_device);
	free(i);
	return;
}

static struct fs_info *create_fs_device(const char *device, const char *mntpnt,
					const char *type, const char *opts,
					int freq, int passno)
{
	struct fs_info *fs;

	if (!(fs = malloc(sizeof(struct fs_info))))
		return NULL;

	fs->device = string_copy(device);
	fs->mountpt = string_copy(mntpnt);
	fs->type = string_copy(type);
	fs->opts = string_copy(opts ? opts : "");
	fs->freq = freq;
	fs->passno = passno;
	fs->flags = 0;
	fs->next = NULL;

	if (!filesys_info)
		filesys_info = fs;
	else
		filesys_last->next = fs;
	filesys_last = fs;

	return fs;
}



static int parse_fstab_line(char *line, struct fs_info **ret_fs)
{
	char	*dev, *device, *mntpnt, *type, *opts, *freq, *passno, *cp;
	struct fs_info *fs;

	*ret_fs = 0;
	strip_line(line);
	cp = line;

	device = parse_word(&cp);
	if (!device || *device == '#')
		return 0;	/* Ignore blank lines and comments */
	mntpnt = parse_word(&cp);
	type = parse_word(&cp);
	opts = parse_word(&cp);
	freq = parse_word(&cp);
	passno = parse_word(&cp);

	if (!mntpnt || !type)
		return -1;

	parse_escape(device);
	parse_escape(mntpnt);
	parse_escape(type);
	parse_escape(opts);
	parse_escape(freq);
	parse_escape(passno);

	dev = blkid_get_devname(cache, device, NULL);
	if (dev)
		device = dev;

	if (strchr(type, ','))
		type = 0;

	fs = create_fs_device(device, mntpnt, type ? type : "auto", opts,
			      freq ? atoi(freq) : -1,
			      passno ? atoi(passno) : -1);
	free(dev);

	if (!fs)
		return -1;
	*ret_fs = fs;
	return 0;
}

static void interpret_type(struct fs_info *fs)
{
	char	*t;

	if (strcmp(fs->type, "auto") != 0)
		return;
	t = blkid_get_tag_value(cache, "TYPE", fs->device);
	if (t) {
		free(fs->type);
		fs->type = t;
	}
}

/*
 * Load the filesystem database from /etc/fstab
 */
static void load_fs_info(const char *filename)
{
	FILE	*f;
	char	buf[1024];
	int	lineno = 0;
	int	old_fstab = 1;
	struct fs_info *fs;

	if ((f = fopen(filename, "r")) == NULL) {
		fprintf(stderr, _("WARNING: couldn't open %s: %s\n"),
			filename, strerror(errno));
		return;
	}
	while (!feof(f)) {
		lineno++;
		if (!fgets(buf, sizeof(buf), f))
			break;
		buf[sizeof(buf)-1] = 0;
		if (parse_fstab_line(buf, &fs) < 0) {
			fprintf(stderr, _("WARNING: bad format "
				"on line %d of %s\n"), lineno, filename);
			continue;
		}
		if (!fs)
			continue;
		if (fs->passno < 0)
			fs->passno = 0;
		else
			old_fstab = 0;
	}

	fclose(f);

	if (old_fstab && filesys_info) {
		fputs("\007\007\007", stderr);
		fputs(_(
		"WARNING: Your /etc/fstab does not contain the fsck passno\n"
		"	field.  I will kludge around things for you, but you\n"
		"	should fix your /etc/fstab file as soon as you can.\n\n"), stderr);

		for (fs = filesys_info; fs; fs = fs->next) {
			fs->passno = 1;
		}
	}
}

/* Lookup filesys in /etc/fstab and return the corresponding entry. */
static struct fs_info *lookup(char *filesys)
{
	struct fs_info *fs;

	/* No filesys name given. */
	if (filesys == NULL)
		return NULL;

	for (fs = filesys_info; fs; fs = fs->next) {
		if (!strcmp(filesys, fs->device) ||
		    (fs->mountpt && !strcmp(filesys, fs->mountpt)))
			break;
	}

	return fs;
}

/* Find fsck program for a given fs type. */
static char *find_fsck(char *type)
{
  char *s;
  const char *tpl;
  static char prog[256];
  char *p = string_copy(fsck_path);
  struct stat st;

  /* Are we looking for a program or just a type? */
  tpl = (strncmp(type, "fsck.", 5) ? "%s/fsck.%s" : "%s/%s");

  for(s = strtok(p, ":"); s; s = strtok(NULL, ":")) {
	sprintf(prog, tpl, s, type);
	if (stat(prog, &st) == 0) break;
  }
  free(p);
  return(s ? prog : NULL);
}

static int progress_active(NOARGS)
{
	struct fsck_instance *inst;

	for (inst = instance_list; inst; inst = inst->next) {
		if (inst->flags & FLAG_DONE)
			continue;
		if (inst->flags & FLAG_PROGRESS)
			return 1;
	}
	return 0;
}

/*
 * Execute a particular fsck program, and link it into the list of
 * child processes we are waiting for.
 */
static int execute(const char *type, const char *device, const char *mntpt,
		   int interactive)
{
	char *s, *argv[80], prog[80];
	int  argc, i;
	struct fsck_instance *inst, *p;
	pid_t	pid;

	inst = malloc(sizeof(struct fsck_instance));
	if (!inst)
		return ENOMEM;
	memset(inst, 0, sizeof(struct fsck_instance));

	sprintf(prog, "fsck.%s", type);
	argv[0] = string_copy(prog);
	argc = 1;

	for (i=0; i <num_args; i++)
		argv[argc++] = string_copy(args[i]);

	if (progress) {
		if ((strcmp(type, "ext2") == 0) ||
		    (strcmp(type, "ext3") == 0) ||
		    (strcmp(type, "ext4") == 0) ||
		    (strcmp(type, "ext4dev") == 0)) {
			char tmp[80];

			tmp[0] = 0;
			if (!progress_active()) {
				snprintf(tmp, 80, "-C%d", progress_fd);
				inst->flags |= FLAG_PROGRESS;
			} else if (progress_fd)
				snprintf(tmp, 80, "-C%d", progress_fd * -1);
			if (tmp[0])
				argv[argc++] = string_copy(tmp);
		}
	}

	argv[argc++] = string_copy(device);
	argv[argc] = 0;

	s = find_fsck(prog);
	if (s == NULL) {
		fprintf(stderr, _("fsck: %s: not found\n"), prog);
		free(inst);
		return ENOENT;
	}

	if (verbose || noexecute) {
		printf("[%s (%d) -- %s] ", s, num_running,
		       mntpt ? mntpt : device);
		for (i=0; i < argc; i++)
			printf("%s ", argv[i]);
		printf("\n");
	}

	/* Fork and execute the correct program. */
	if (noexecute)
		pid = -1;
	else if ((pid = fork()) < 0) {
		perror("fork");
		free(inst);
		return errno;
	} else if (pid == 0) {
		if (!interactive)
			close(0);
		(void) execv(s, argv);
		perror(argv[0]);
		free(inst);
		exit(EXIT_ERROR);
	}

	for (i=0; i < argc; i++)
		free(argv[i]);

	inst->pid = pid;
	inst->prog = string_copy(prog);
	inst->type = string_copy(type);
	inst->device = string_copy(device);
	inst->base_device = base_device(device);
	inst->start_time = time(0);
	inst->next = NULL;

	/*
	 * Find the end of the list, so we add the instance on at the end.
	 */
	for (p = instance_list; p && p->next; p = p->next);

	if (p)
		p->next = inst;
	else
		instance_list = inst;

	return 0;
}

/*
 * Send a signal to all outstanding fsck child processes
 */
static int kill_all(int signum)
{
	struct fsck_instance *inst;
	int	n = 0;

	for (inst = instance_list; inst; inst = inst->next) {
		if (inst->flags & FLAG_DONE)
			continue;
		kill(inst->pid, signum);
		n++;
	}
	return n;
}

/*
 * Wait for one child process to exit; when it does, unlink it from
 * the list of executing child processes, and return it.
 */
static struct fsck_instance *wait_one(int flags)
{
	int	status;
	int	sig;
	struct fsck_instance *inst, *inst2, *prev;
	pid_t	pid;

	if (!instance_list)
		return NULL;

	if (noexecute) {
		inst = instance_list;
		prev = 0;
#ifdef RANDOM_DEBUG
		while (inst->next && (random() & 1)) {
			prev = inst;
			inst = inst->next;
		}
#endif
		inst->exit_status = 0;
		goto ret_inst;
	}

	/*
	 * gcc -Wall fails saving throw against stupidity
	 * (inst and prev are thought to be uninitialized variables)
	 */
	inst = prev = NULL;

	do {
		pid = waitpid(-1, &status, flags);
		if (cancel_requested && !kill_sent) {
			kill_all(SIGTERM);
			kill_sent++;
		}
		if ((pid == 0) && (flags & WNOHANG))
			return NULL;
		if (pid < 0) {
			if ((errno == EINTR) || (errno == EAGAIN))
				continue;
			if (errno == ECHILD) {
				fprintf(stderr,
					_("%s: wait: No more child process?!?\n"),
					progname);
				return NULL;
			}
			perror("wait");
			continue;
		}
		for (prev = 0, inst = instance_list;
		     inst;
		     prev = inst, inst = inst->next) {
			if (inst->pid == pid)
				break;
		}
	} while (!inst);

	if (WIFEXITED(status))
		status = WEXITSTATUS(status);
	else if (WIFSIGNALED(status)) {
		sig = WTERMSIG(status);
		if (sig == SIGINT) {
			status = EXIT_UNCORRECTED;
		} else {
			printf(_("Warning... %s for device %s exited "
			       "with signal %d.\n"),
			       inst->prog, inst->device, sig);
			status = EXIT_ERROR;
		}
	} else {
		printf(_("%s %s: status is %x, should never happen.\n"),
		       inst->prog, inst->device, status);
		status = EXIT_ERROR;
	}
	inst->exit_status = status;
	inst->flags |= FLAG_DONE;
	if (progress && (inst->flags & FLAG_PROGRESS) &&
	    !progress_active()) {
		for (inst2 = instance_list; inst2; inst2 = inst2->next) {
			if (inst2->flags & FLAG_DONE)
				continue;
			if (strcmp(inst2->type, "ext2") &&
			    strcmp(inst2->type, "ext3") &&
			    strcmp(inst2->type, "ext4") &&
			    strcmp(inst2->type, "ext4dev"))
				continue;
			/*
			 * If we've just started the fsck, wait a tiny
			 * bit before sending the kill, to give it
			 * time to set up the signal handler
			 */
			if (inst2->start_time < time(0)+2) {
				if (fork() == 0) {
					sleep(1);
					kill(inst2->pid, SIGUSR1);
					exit(0);
				}
			} else
				kill(inst2->pid, SIGUSR1);
			inst2->flags |= FLAG_PROGRESS;
			break;
		}
	}
ret_inst:
	if (prev)
		prev->next = inst->next;
	else
		instance_list = inst->next;
	if (verbose > 1)
		printf(_("Finished with %s (exit status %d)\n"),
		       inst->device, inst->exit_status);
	num_running--;
	return inst;
}

#define FLAG_WAIT_ALL		0
#define FLAG_WAIT_ATLEAST_ONE	1
/*
 * Wait until all executing child processes have exited; return the
 * logical OR of all of their exit code values.
 */
static int wait_many(int flags)
{
	struct fsck_instance *inst;
	int	global_status = 0;
	int	wait_flags = 0;

	while ((inst = wait_one(wait_flags))) {
		global_status |= inst->exit_status;
		free_instance(inst);
#ifdef RANDOM_DEBUG
		if (noexecute && (flags & WNOHANG) && !(random() % 3))
			break;
#endif
		if (flags & FLAG_WAIT_ATLEAST_ONE)
			wait_flags = WNOHANG;
	}
	return global_status;
}

/*
 * Run the fsck program on a particular device
 *
 * If the type is specified using -t, and it isn't prefixed with "no"
 * (as in "noext2") and only one filesystem type is specified, then
 * use that type regardless of what is specified in /etc/fstab.
 *
 * If the type isn't specified by the user, then use either the type
 * specified in /etc/fstab, or DEFAULT_FSTYPE.
 */
static void fsck_device(struct fs_info *fs, int interactive)
{
	const char *type;
	int retval;

	interpret_type(fs);

	if (strcmp(fs->type, "auto") != 0)
		type = fs->type;
	else if (fstype && strncmp(fstype, "no", 2) &&
	    strncmp(fstype, "opts=", 5) && strncmp(fstype, "loop", 4) &&
	    !strchr(fstype, ','))
		type = fstype;
	else
		type = DEFAULT_FSTYPE;

	num_running++;
	retval = execute(type, fs->device, fs->mountpt, interactive);
	if (retval) {
		fprintf(stderr, _("%s: Error %d while executing fsck.%s "
			"for %s\n"), progname, retval, type, fs->device);
		num_running--;
	}
}


/*
 * Deal with the fsck -t argument.
 */
struct fs_type_compile {
	char **list;
	int *type;
	int  negate;
} fs_type_compiled;

#define FS_TYPE_NORMAL	0
#define FS_TYPE_OPT	1
#define FS_TYPE_NEGOPT	2

static const char *fs_type_syntax_error =
N_("Either all or none of the filesystem types passed to -t must be prefixed\n"
   "with 'no' or '!'.\n");

static void compile_fs_type(char *fs_type, struct fs_type_compile *cmp)
{
	char 	*cp, *list, *s;
	int	num = 2;
	int	negate, first_negate = 1;

	if (fs_type) {
		for (cp=fs_type; *cp; cp++) {
			if (*cp == ',')
				num++;
		}
	}

	cmp->list = malloc(num * sizeof(char *));
	cmp->type = malloc(num * sizeof(int));
	if (!cmp->list || !cmp->type) {
		fputs(_("Couldn't allocate memory for filesystem types\n"),
		      stderr);
		exit(EXIT_ERROR);
	}
	memset(cmp->list, 0, num * sizeof(char *));
	memset(cmp->type, 0, num * sizeof(int));
	cmp->negate = 0;

	if (!fs_type)
		return;

	list = string_copy(fs_type);
	num = 0;
	s = strtok(list, ",");
	while(s) {
		negate = 0;
		if (strncmp(s, "no", 2) == 0) {
			s += 2;
			negate = 1;
		} else if (*s == '!') {
			s++;
			negate = 1;
		}
		if (strcmp(s, "loop") == 0)
			/* loop is really short-hand for opts=loop */
			goto loop_special_case;
		else if (strncmp(s, "opts=", 5) == 0) {
			s += 5;
		loop_special_case:
			cmp->type[num] = negate ? FS_TYPE_NEGOPT : FS_TYPE_OPT;
		} else {
			if (first_negate) {
				cmp->negate = negate;
				first_negate = 0;
			}
			if ((negate && !cmp->negate) ||
			    (!negate && cmp->negate)) {
				fputs(_(fs_type_syntax_error), stderr);
				exit(EXIT_USAGE);
			}
		}
#if 0
		printf("Adding %s to list (type %d).\n", s, cmp->type[num]);
#endif
	        cmp->list[num++] = string_copy(s);
		s = strtok(NULL, ",");
	}
	free(list);
}

/*
 * This function returns true if a particular option appears in a
 * comma-delimited options list
 */
static int opt_in_list(const char *opt, char *optlist)
{
	char	*list, *s;

	if (!optlist)
		return 0;
	list = string_copy(optlist);

	s = strtok(list, ",");
	while(s) {
		if (strcmp(s, opt) == 0) {
			free(list);
			return 1;
		}
		s = strtok(NULL, ",");
	}
        free(list);
	return 0;
}

/* See if the filesystem matches the criteria given by the -t option */
static int fs_match(struct fs_info *fs, struct fs_type_compile *cmp)
{
	int n, ret = 0, checked_type = 0;
	char *cp;

	if (cmp->list == 0 || cmp->list[0] == 0)
		return 1;

	for (n=0; (cp = cmp->list[n]); n++) {
		switch (cmp->type[n]) {
		case FS_TYPE_NORMAL:
			checked_type++;
			if (strcmp(cp, fs->type) == 0) {
				ret = 1;
			}
			break;
		case FS_TYPE_NEGOPT:
			if (opt_in_list(cp, fs->opts))
				return 0;
			break;
		case FS_TYPE_OPT:
			if (!opt_in_list(cp, fs->opts))
				return 0;
			break;
		}
	}
	if (checked_type == 0)
		return 1;
	return (cmp->negate ? !ret : ret);
}

/* Check if we should ignore this filesystem. */
static int ignore(struct fs_info *fs)
{
	const char **ip;
	int wanted = 0;

	/*
	 * If the pass number is 0, ignore it.
	 */
	if (fs->passno == 0)
		return 1;

	/*
	 * If this is a bind mount, ignore it.
	 */
	if (opt_in_list("bind", fs->opts)) {
		fprintf(stderr,
			_("%s: skipping bad line in /etc/fstab: bind mount with nonzero fsck pass number\n"),
			fs->mountpt);
		return 1;
	}

	interpret_type(fs);

	/*
	 * If a specific fstype is specified, and it doesn't match,
	 * ignore it.
	 */
	if (!fs_match(fs, &fs_type_compiled)) return 1;

	/* Are we ignoring this type? */
	for(ip = ignored_types; *ip; ip++)
		if (strcmp(fs->type, *ip) == 0) return 1;

	/* Do we really really want to check this fs? */
	for(ip = really_wanted; *ip; ip++)
		if (strcmp(fs->type, *ip) == 0) {
			wanted = 1;
			break;
		}

	/* See if the <fsck.fs> program is available. */
	if (find_fsck(fs->type) == NULL) {
		if (wanted)
			fprintf(stderr, _("fsck: cannot check %s: fsck.%s not found\n"),
				fs->device, fs->type);
		return 1;
	}

	/* We can and want to check this file system type. */
	return 0;
}

/*
 * Returns TRUE if a partition on the same disk is already being
 * checked.
 */
static int device_already_active(char *device)
{
	struct fsck_instance *inst;
	char *base;

	if (force_all_parallel)
		return 0;

#ifdef BASE_MD
	/* Don't check a soft raid disk with any other disk */
	if (instance_list &&
	    (!strncmp(instance_list->device, BASE_MD, sizeof(BASE_MD)-1) ||
	     !strncmp(device, BASE_MD, sizeof(BASE_MD)-1)))
		return 1;
#endif

	base = base_device(device);
	/*
	 * If we don't know the base device, assume that the device is
	 * already active if there are any fsck instances running.
	 */
	if (!base)
		return (instance_list != 0);
	for (inst = instance_list; inst; inst = inst->next) {
		if (!inst->base_device || !strcmp(base, inst->base_device)) {
			free(base);
			return 1;
		}
	}
	free(base);
	return 0;
}

/* Check all file systems, using the /etc/fstab table. */
static int check_all(NOARGS)
{
	struct fs_info *fs = NULL;
	int status = EXIT_OK;
	int not_done_yet = 1;
	int passno = 1;
	int pass_done;

	if (verbose)
		fputs(_("Checking all file systems.\n"), stdout);

	/*
	 * Do an initial scan over the filesystem; mark filesystems
	 * which should be ignored as done, and resolve any "auto"
	 * filesystem types (done as a side-effect of calling ignore()).
	 */
	for (fs = filesys_info; fs; fs = fs->next) {
		if (ignore(fs))
			fs->flags |= FLAG_DONE;
	}

	/*
	 * Find and check the root filesystem.
	 */
	if (!parallel_root) {
		for (fs = filesys_info; fs; fs = fs->next) {
			if (!strcmp(fs->mountpt, "/"))
				break;
		}
		if (fs) {
			if (!skip_root && !ignore(fs) &&
			    !(ignore_mounted && is_mounted(fs->device))) {
				fsck_device(fs, 1);
				status |= wait_many(FLAG_WAIT_ALL);
				if (status > EXIT_NONDESTRUCT)
					return status;
			}
			fs->flags |= FLAG_DONE;
		}
	}
	/*
	 * This is for the bone-headed user who enters the root
	 * filesystem twice.  Skip root will skep all root entries.
	 */
	if (skip_root)
		for (fs = filesys_info; fs; fs = fs->next)
			if (!strcmp(fs->mountpt, "/"))
				fs->flags |= FLAG_DONE;

	while (not_done_yet) {
		not_done_yet = 0;
		pass_done = 1;

		for (fs = filesys_info; fs; fs = fs->next) {
			if (cancel_requested)
				break;
			if (fs->flags & FLAG_DONE)
				continue;
			/*
			 * If the filesystem's pass number is higher
			 * than the current pass number, then we don't
			 * do it yet.
			 */
			if (fs->passno > passno) {
				not_done_yet++;
				continue;
			}
			if (ignore_mounted && is_mounted(fs->device)) {
				fs->flags |= FLAG_DONE;
				continue;
			}
			/*
			 * If a filesystem on a particular device has
			 * already been spawned, then we need to defer
			 * this to another pass.
			 */
			if (device_already_active(fs->device)) {
				pass_done = 0;
				continue;
			}
			/*
			 * Spawn off the fsck process
			 */
			fsck_device(fs, serialize);
			fs->flags |= FLAG_DONE;

			/*
			 * Only do one filesystem at a time, or if we
			 * have a limit on the number of fsck's extant
			 * at one time, apply that limit.
			 */
			if (serialize ||
			    (max_running && (num_running >= max_running))) {
				pass_done = 0;
				break;
			}
		}
		if (cancel_requested)
			break;
		if (verbose > 1)
			printf(_("--waiting-- (pass %d)\n"), passno);
		status |= wait_many(pass_done ? FLAG_WAIT_ALL :
				    FLAG_WAIT_ATLEAST_ONE);
		if (pass_done) {
			if (verbose > 1)
				printf("----------------------------------\n");
			passno++;
		} else
			not_done_yet++;
	}
	if (cancel_requested && !kill_sent) {
		kill_all(SIGTERM);
		kill_sent++;
	}
	status |= wait_many(FLAG_WAIT_ATLEAST_ONE);
	return status;
}

static void usage(NOARGS)
{
	fputs(_("Usage: fsck [-AMNPRTV] [ -C [ fd ] ] [-t fstype] [fs-options] [filesys ...]\n"), stderr);
	exit(EXIT_USAGE);
}

#ifdef HAVE_SIGNAL_H
static void signal_cancel(int sig FSCK_ATTR((unused)))
{
	cancel_requested++;
}
#endif

static void PRS(int argc, char *argv[])
{
	int	i, j;
	char	*arg, *dev, *tmp = 0;
	char	options[128];
	int	opt = 0;
	int     opts_for_fsck = 0;
#ifdef HAVE_SIGNAL_H
	struct sigaction	sa;

	/*
	 * Set up signal action
	 */
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = signal_cancel;
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGTERM, &sa, 0);
#endif

	num_devices = 0;
	num_args = 0;
	instance_list = 0;

	progname = argv[0];

	for (i=1; i < argc; i++) {
		arg = argv[i];
		if (!arg)
			continue;
		if ((arg[0] == '/' && !opts_for_fsck) || strchr(arg, '=')) {
			if (num_devices >= MAX_DEVICES) {
				fprintf(stderr, _("%s: too many devices\n"),
					progname);
				exit(EXIT_ERROR);
			}
			dev = blkid_get_devname(cache, arg, NULL);
			if (!dev && strchr(arg, '=')) {
				/*
				 * Check to see if we failed because
				 * /proc/partitions isn't found.
				 */
				if (access("/proc/partitions", R_OK) < 0) {
					fprintf(stderr, "Couldn't open /proc/partitions: %s\n",
						strerror(errno));
					fprintf(stderr, "Is /proc mounted?\n");
					exit(EXIT_ERROR);
				}
				/*
				 * Check to see if this is because
				 * we're not running as root
				 */
				if (geteuid())
					fprintf(stderr,
		"Must be root to scan for matching filesystems: %s\n", arg);
				else
					fprintf(stderr,
		"Couldn't find matching filesystem: %s\n", arg);
				exit(EXIT_ERROR);
			}
			devices[num_devices++] = dev ? dev : string_copy(arg);
			continue;
		}
		if (arg[0] != '-' || opts_for_fsck) {
			if (num_args >= MAX_ARGS) {
				fprintf(stderr, _("%s: too many arguments\n"),
					progname);
				exit(EXIT_ERROR);
			}
			args[num_args++] = string_copy(arg);
			continue;
		}
		for (j=1; arg[j]; j++) {
			if (opts_for_fsck) {
				options[++opt] = arg[j];
				continue;
			}
			switch (arg[j]) {
			case 'A':
				doall++;
				break;
			case 'C':
				progress++;
				if (arg[j+1]) {
					progress_fd = string_to_int(arg+j+1);
					if (progress_fd < 0)
						progress_fd = 0;
					else
						goto next_arg;
				} else if ((i+1) < argc &&
					   !strncmp(argv[i+1], "-", 1) == 0) {
					progress_fd = string_to_int(argv[i]);
					if (progress_fd < 0)
						progress_fd = 0;
					else {
						++i;
						goto next_arg;
					}
				}
				break;
			case 'V':
				verbose++;
				break;
			case 'N':
				noexecute++;
				break;
			case 'R':
				skip_root++;
				break;
			case 'T':
				notitle++;
				break;
			case 'M':
				ignore_mounted++;
				break;
			case 'P':
				parallel_root++;
				break;
			case 's':
				serialize++;
				break;
			case 't':
				tmp = 0;
				if (fstype)
					usage();
				if (arg[j+1])
					tmp = arg+j+1;
				else if ((i+1) < argc)
					tmp = argv[++i];
				else
					usage();
				fstype = string_copy(tmp);
				compile_fs_type(fstype, &fs_type_compiled);
				goto next_arg;
			case '-':
				opts_for_fsck++;
				break;
			case '?':
				usage();
				break;
			default:
				options[++opt] = arg[j];
				break;
			}
		}
	next_arg:
		if (opt) {
			options[0] = '-';
			options[++opt] = '\0';
			if (num_args >= MAX_ARGS) {
				fprintf(stderr,
					_("%s: too many arguments\n"),
					progname);
				exit(EXIT_ERROR);
			}
			args[num_args++] = string_copy(options);
			opt = 0;
		}
	}
	if (getenv("FSCK_FORCE_ALL_PARALLEL"))
		force_all_parallel++;
	if ((tmp = getenv("FSCK_MAX_INST")))
	    max_running = atoi(tmp);
}

int main(int argc, char *argv[])
{
	int i, status = 0;
	int interactive = 0;
	char *oldpath = getenv("PATH");
	const char *fstab;
	struct fs_info *fs;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	setvbuf(stderr, NULL, _IONBF, BUFSIZ);

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif
	blkid_get_cache(&cache, NULL);
	PRS(argc, argv);

	if (!notitle)
		printf("fsck %s (%s)\n", E2FSPROGS_VERSION, E2FSPROGS_DATE);

	fstab = getenv("FSTAB_FILE");
	if (!fstab)
		fstab = _PATH_MNTTAB;
	load_fs_info(fstab);

	/* Update our search path to include uncommon directories. */
	if (oldpath) {
		fsck_path = malloc (strlen (fsck_prefix_path) + 1 +
				    strlen (oldpath) + 1);
		if (!fsck_path) {
			fprintf(stderr, "%s: Unable to allocate memory for fsck_path\n", progname);
			exit(EXIT_ERROR);
		}
		strcpy (fsck_path, fsck_prefix_path);
		strcat (fsck_path, ":");
		strcat (fsck_path, oldpath);
	} else {
		fsck_path = string_copy(fsck_prefix_path);
	}

	if ((num_devices == 1) || (serialize))
		interactive = 1;

	/* If -A was specified ("check all"), do that! */
	if (doall)
		return check_all();

	if (num_devices == 0) {
		serialize++;
		interactive++;
		return check_all();
	}
	for (i = 0 ; i < num_devices; i++) {
		if (cancel_requested) {
			if (!kill_sent) {
				kill_all(SIGTERM);
				kill_sent++;
			}
			break;
		}
		fs = lookup(devices[i]);
		if (!fs) {
			fs = create_fs_device(devices[i], 0, "auto",
					      0, -1, -1);
			if (!fs)
				continue;
		}
		if (ignore_mounted && is_mounted(fs->device))
			continue;
		fsck_device(fs, interactive);
		if (serialize ||
		    (max_running && (num_running >= max_running))) {
			struct fsck_instance *inst;

			inst = wait_one(0);
			if (inst) {
				status |= inst->exit_status;
				free_instance(inst);
			}
			if (verbose > 1)
				printf("----------------------------------\n");
		}
	}
	status |= wait_many(FLAG_WAIT_ALL);
	free(fsck_path);
	blkid_put_cache(cache);
	return status;
}

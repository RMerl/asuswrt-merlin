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
 *	         2001, 2002, 2003, 2004, 2005 by  Theodore Ts'o.
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the GNU Public
 * License.
 */

#define _XOPEN_SOURCE 600 /* for inclusion of sa_handler in Solaris */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <paths.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <blkid.h>

#include "fsprobe.h"

#include "nls.h"
#include "pathnames.h"
#include "ismounted.h"
#include "c.h"
#include "fsck.h"

#define XALLOC_EXIT_CODE	EXIT_ERROR
#include "xalloc.h"

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

int lockdisk = 0;
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
char *fstype = NULL;
struct fs_info *filesys_info = NULL, *filesys_last = NULL;
struct fsck_instance *instance_list;
const char fsck_prefix_path[] = FS_SEARCH_PATH;
char *fsck_path = 0;

static char *string_copy(const char *s)
{
	char	*ret;

	if (!s)
		return 0;
	ret = xmalloc(strlen(s)+1);
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

static dev_t get_disk(const char *device)
{
	struct stat st;
	dev_t disk;

	if (!stat(device, &st) &&
	    !blkid_devno_to_wholedisk(st.st_rdev, NULL, 0, &disk))
		return disk;

	return 0;
}

static int is_irrotational_disk(dev_t disk)
{
	char path[PATH_MAX];
	FILE *f;
	int rc, x;

	rc = snprintf(path, sizeof(path),
			"/sys/dev/block/%d:%d/queue/rotational",
			major(disk), minor(disk));

	if (rc < 0 || (unsigned int) (rc + 1) > sizeof(path))
		return 0;

	f = fopen(path, "r");
	if (!f)
		return 0;

	rc = fscanf(f, "%d", &x);
	if (rc != 1) {
		if (ferror(f))
			warn(_("failed to read: %s"), path);
		else
			warnx(_("parse error: %s"), path);
	}
	fclose(f);

	return rc == 1 ? !x : 0;
}

static void lock_disk(struct fsck_instance *inst)
{
	dev_t disk = inst->fs->disk ? inst->fs->disk : get_disk(inst->fs->device);
	char *diskname;

	if (!disk || is_irrotational_disk(disk))
		return;

	diskname = blkid_devno_to_devname(disk);
	if (!diskname)
		return;

	if (verbose)
		printf(_("Locking disk %s ... "), diskname);

	inst->lock = open(diskname, O_CLOEXEC | O_RDONLY);
	if (inst->lock >= 0) {
		int rc = -1;

		/* inform users that we're waiting on the lock */
		if (verbose &&
		    (rc = flock(inst->lock, LOCK_EX | LOCK_NB)) != 0 &&
		    errno == EWOULDBLOCK)
			printf(_("(waiting) "));

		if (rc != 0 && flock(inst->lock, LOCK_EX) != 0) {
			close(inst->lock);			/* failed */
			inst->lock = -1;
		}
	}

	if (verbose)
		/* TRANSLATORS: These are followups to "Locking disk...". */
		printf("%s.\n", inst->lock >= 0 ? _("succeeded") : _("failed"));

	free(diskname);
	return;
}

static void unlock_disk(struct fsck_instance *inst)
{
	if (inst->lock >= 0) {
		/* explicitly unlock, don't rely on close(), maybe some library
		 * (e.g. liblkid) has still open the device.
		 */
		flock(inst->lock, LOCK_UN);
		close(inst->lock);
	}
}



static void free_instance(struct fsck_instance *i)
{
	if (lockdisk)
		unlock_disk(i);
	free(i->prog);
	free(i);
	return;
}

static struct fs_info *create_fs_device(const char *device, const char *mntpnt,
					const char *type, const char *opts,
					int freq, int passno)
{
	struct fs_info *fs;

	fs = xmalloc(sizeof(struct fs_info));

	fs->device = string_copy(device);
	fs->mountpt = string_copy(mntpnt);
	fs->type = string_copy(type);
	fs->opts = string_copy(opts ? opts : "");
	fs->freq = freq;
	fs->passno = passno;
	fs->flags = 0;
	fs->next = NULL;
	fs->disk = 0;
	fs->stacked = 0;

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

	dev = fsprobe_get_devname_by_spec(device);
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
	t = fsprobe_get_fstype_by_devname(fs->device);
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
		warn(_("WARNING: couldn't open %s"), filename);
		return;
	}
	while (!feof(f)) {
		lineno++;
		if (!fgets(buf, sizeof(buf), f))
			break;
		buf[sizeof(buf)-1] = 0;
		if (parse_fstab_line(buf, &fs) < 0) {
			warnx(_("WARNING: bad format "
				"on line %d of %s"), lineno, filename);
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
		warnx(_(
		"WARNING: Your /etc/fstab does not contain the fsck passno\n"
		"	field.  I will kludge around things for you, but you\n"
		"	should fix your /etc/fstab file as soon as you can.\n"));

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
static int execute(const char *type, struct fs_info *fs, int interactive)
{
	char *s, *argv[80], prog[80];
	int  argc, i;
	struct fsck_instance *inst, *p;
	pid_t	pid;

	inst = xmalloc(sizeof(struct fsck_instance));
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

	argv[argc++] = string_copy(fs->device);
	argv[argc] = 0;

	s = find_fsck(prog);
	if (s == NULL) {
		warnx(_("%s: not found"), prog);
		free(inst);
		return ENOENT;
	}

	if (verbose || noexecute) {
		printf("[%s (%d) -- %s] ", s, num_running,
		       fs->mountpt ? fs->mountpt : fs->device);
		for (i=0; i < argc; i++)
			printf("%s ", argv[i]);
		printf("\n");
	}


	inst->fs = fs;
	inst->lock = -1;

	if (lockdisk)
		lock_disk(inst);

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
				warnx(_("wait: no more child process?!?"));
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
			warnx(_("Warning... %s for device %s exited "
			       "with signal %d."),
			       inst->prog, inst->fs->device, sig);
			status = EXIT_ERROR;
		}
	} else {
		warnx(_("%s %s: status is %x, should never happen."),
		       inst->prog, inst->fs->device, status);
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
					exit(EXIT_OK);
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
		       inst->fs->device, inst->exit_status);
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
static int fsck_device(struct fs_info *fs, int interactive)
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
	retval = execute(type, fs, interactive);
	if (retval) {
		warnx(_("error %d while executing fsck.%s for %s"),
			retval, type, fs->device);
		num_running--;
		return EXIT_ERROR;
	}
	return 0;
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

	cmp->list = xmalloc(num * sizeof(char *));
	cmp->type = xmalloc(num * sizeof(int));
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
				errx(EXIT_USAGE,
					_("Either all or none of the filesystem types passed to -t must be prefixed\n"
					  "with 'no' or '!'."));
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

/*
 * Check if a device exists
 */
static int device_exists(const char *device)
{
	struct stat st;

	if (stat(device, &st) == -1)
		return 0;

	if (!S_ISBLK(st.st_mode))
		return 0;

	return 1;
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
		warnx(_("%s: skipping bad line in /etc/fstab: "
			"bind mount with nonzero fsck pass number"),
			fs->mountpt);
		return 1;
	}

	/*
	 * ignore devices that don't exist and have the "nofail" mount option
	 */
	if (!device_exists(fs->device)) {
		if (opt_in_list("nofail", fs->opts)) {
			if (verbose)
				printf(_("%s: skipping nonexistent device\n"),
								fs->device);
			return 1;
		}
		if (verbose)
			printf(_("%s: nonexistent device (\"nofail\" fstab "
				 "option may be used to skip this device)\n"),
				 fs->device);
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
			warnx(_("cannot check %s: fsck.%s not found"),
				fs->device, fs->type);
		return 1;
	}

	/* We can and want to check this file system type. */
	return 0;
}

static int count_slaves(dev_t disk)
{
	DIR *dir;
	struct dirent *dp;
	char dirname[PATH_MAX];
	int count = 0;

	snprintf(dirname, sizeof(dirname),
			"/sys/dev/block/%u:%u/slaves/",
			major(disk), minor(disk));

	if (!(dir = opendir(dirname)))
		return -1;

	while ((dp = readdir(dir)) != 0) {
#ifdef _DIRENT_HAVE_D_TYPE
		if (dp->d_type != DT_UNKNOWN && dp->d_type != DT_LNK)
			continue;
#endif
		if (dp->d_name[0] == '.' &&
		    ((dp->d_name[1] == 0) ||
		     ((dp->d_name[1] == '.') && (dp->d_name[2] == 0))))
			continue;

		count++;
	}
	closedir(dir);
	return count;
}

/*
 * Returns TRUE if a partition on the same disk is already being
 * checked.
 */
static int disk_already_active(struct fs_info *fs)
{
	struct fsck_instance *inst;

	if (force_all_parallel)
		return 0;

	if (instance_list && instance_list->fs->stacked)
		/* any instance for a stacked device is already running */
		return 1;

	if (!fs->disk) {
		fs->disk = get_disk(fs->device);
		if (fs->disk)
			fs->stacked = count_slaves(fs->disk);
	}

	/*
	 * If we don't know the base device, assume that the device is
	 * already active if there are any fsck instances running.
	 *
	 * Don't check a stacked device with any other disk too.
	 */
	if (!fs->disk || fs->stacked)
		return (instance_list != 0);

	for (inst = instance_list; inst; inst = inst->next) {
		if (!inst->fs->disk || fs->disk == inst->fs->disk)
			return 1;
	}
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
				status |= fsck_device(fs, 1);
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
			if (disk_already_active(fs)) {
				pass_done = 0;
				continue;
			}
			/*
			 * Spawn off the fsck process
			 */
			status |= fsck_device(fs, serialize);
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

static void __attribute__((__noreturn__)) usage(void)
{
	printf(_("\nUsage:\n"
		 " %s [fsck-options] [fs-options] [filesys ...]\n"),
		program_invocation_short_name);

	puts(_(	"\nOptions:\n"
		" -A         check all filesystems\n"
		" -R         skip root filesystem; useful only with `-A'\n"
		" -M         do not check mounted filesystems\n"
		" -t <type>  specify filesystem types to be checked;\n"
		"              type is allowed to be comma-separated list\n"
		" -P         check filesystems in parallel, including root\n"
		" -s         serialize fsck operations\n"
		" -l         lock the device using flock()\n"
		" -N         do not execute, just show what would be done\n"
		" -T         do not show the title on startup\n"
		" -C <fd>    display progress bar; file descriptor is for GUIs\n"
		" -V         explain what is being done\n"
		" -?         display this help and exit\n\n"
		"See fsck.* commands for fs-options."));

	exit(EXIT_USAGE);
}

static void signal_cancel(int sig FSCK_ATTR((unused)))
{
	cancel_requested++;
}

static void PRS(int argc, char *argv[])
{
	int	i, j;
	char	*arg, *dev, *tmp = 0;
	char	options[128];
	int	opt = 0;
	int     opts_for_fsck = 0;
	struct sigaction	sa;

	/*
	 * Set up signal action
	 */
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = signal_cancel;
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGTERM, &sa, 0);

	num_devices = 0;
	num_args = 0;
	instance_list = 0;

	for (i=1; i < argc; i++) {
		arg = argv[i];
		if (!arg)
			continue;
		if ((arg[0] == '/' && !opts_for_fsck) || strchr(arg, '=')) {
			if (num_devices >= MAX_DEVICES)
				errx(EXIT_ERROR, _("too many devices"));
			dev = fsprobe_get_devname_by_spec(arg);
			if (!dev && strchr(arg, '=')) {
				/*
				 * Check to see if we failed because
				 * /proc/partitions isn't found.
				 */
				if (access(_PATH_PROC_PARTITIONS, R_OK) < 0) {
					warn(_("couldn't open %s"),
						_PATH_PROC_PARTITIONS);
					errx(EXIT_ERROR, _("Is /proc mounted?"));
				}
				/*
				 * Check to see if this is because
				 * we're not running as root
				 */
				if (geteuid())
					errx(EXIT_ERROR,
						_("must be root to scan for matching filesystems: %s"),
						arg);
				else
					errx(EXIT_ERROR,
						_("couldn't find matching filesystem: %s"),
						arg);
			}
			devices[num_devices++] = dev ? dev : string_copy(arg);
			continue;
		}
		if (arg[0] != '-' || opts_for_fsck) {
			if (num_args >= MAX_ARGS)
				errx(EXIT_ERROR, _("too many arguments"));
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
				doall = 1;
				break;
			case 'C':
				progress = 1;
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
			case 'l':
				lockdisk = 1;
				break;
			case 'V':
				verbose++;
				break;
			case 'N':
				noexecute = 1;
				break;
			case 'R':
				skip_root = 1;
				break;
			case 'T':
				notitle = 1;
				break;
			case 'M':
				ignore_mounted = 1;
				break;
			case 'P':
				parallel_root = 1;
				break;
			case 's':
				serialize = 1;
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
			if (num_args >= MAX_ARGS)
				errx(EXIT_ERROR, _("too many arguments"));
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

	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	fsprobe_init();
	PRS(argc, argv);

	if (!notitle)
		printf(_("%s from %s\n"), program_invocation_short_name, PACKAGE_STRING);

	fstab = getenv("FSTAB_FILE");
	if (!fstab)
		fstab = _PATH_MNTTAB;
	load_fs_info(fstab);

	/* Update our search path to include uncommon directories. */
	if (oldpath) {
		fsck_path = xmalloc (strlen (fsck_prefix_path) + 1 +
				    strlen (oldpath) + 1);
		strcpy (fsck_path, fsck_prefix_path);
		strcat (fsck_path, ":");
		strcat (fsck_path, oldpath);
	} else {
		fsck_path = string_copy(fsck_prefix_path);
	}

	if ((num_devices == 1) || (serialize))
		interactive = 1;

	if (lockdisk && (doall || num_devices > 1)) {
		warnx(_("the -l option can be used with one "
				  "device only -- ignore"));
		lockdisk = 0;
	}

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
		status |= fsck_device(fs, interactive);
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
	fsprobe_exit();
	return status;
}


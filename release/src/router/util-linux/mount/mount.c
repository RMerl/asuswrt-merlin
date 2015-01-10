/*
 * A mount(8) for Linux.
 *
 * Modifications by many people. Distributed under GPL.
 */

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>

#include <pwd.h>
#include <grp.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>

#include <mntent.h>

#ifdef HAVE_LIBSELINUX
#include <selinux/selinux.h>
#include <selinux/context.h>
#endif

#include "pathnames.h"
#include "fsprobe.h"
#include "devname.h"
#include "mount_constants.h"
#include "sundries.h"
#include "mount_mntent.h"
#include "fstab.h"
#include "loopdev.h"
#include "linux_version.h"
#include "getusername.h"
#include "env.h"
#include "nls.h"
#include "blkdev.h"
#include "strutils.h"

#define DO_PS_FIDDLING

#ifdef DO_PS_FIDDLING
#include "setproctitle.h"
#endif

/* True for fake mount (-f).  */
static int fake = 0;

/* True if we are allowed to call /sbin/mount.${FSTYPE} */
static int external_allowed = 1;

/* Don't write an entry in /etc/mtab (-n).  */
static int nomtab = 0;

/* True for explicit readonly (-r).  */
static int readonly = 0;

/* Nonzero for sloppy (-s).  */
static int sloppy = 0;

/* True for explicit read/write (-w).  */
static int readwrite = 0;

/* True for all mount (-a).  */
static int mount_all = 0;

/* True for fork() during all mount (-F).  */
static int optfork = 0;

/* Add volumelabel in a listing of mounted devices (-l). */
static int list_with_volumelabel = 0;

/* Nonzero for mount {bind|move|make-shared|make-private|
 *				make-unbindable|make-slave}
 */
static int mounttype = 0;

/* True if (ruid != euid) or (0 != ruid), i.e. only "user" mounts permitted.  */
static int restricted = 1;

/* Contains the fd to read the passphrase from, if any. */
static int pfd = -1;

#ifdef HAVE_LIBMOUNT_MOUNT
static struct libmnt_update *mtab_update;
static char *mtab_opts;
static unsigned long mtab_flags;

static void prepare_mtab_entry(const char *spec, const char *node,
			const char *type, const char *opts, unsigned long flags);
#endif

/* mount(2) options */
struct mountargs {
       const char *spec;
       const char *node;
       const char *type;
       int flags;
       void *data;
};

/* Map from -o and fstab option strings to the flag argument to mount(2).  */
struct opt_map {
  const char *opt;		/* option name */
  int  skip;			/* skip in mtab option string */
  int  inv;			/* true if flag value should be inverted */
  int  mask;			/* flag mask value */
  int  cmask;			/* comments mask */
};

/* Custom mount options for our own purposes.  */
/* Maybe these should now be freed for kernel use again */
#define MS_NOAUTO	0x80000000
#define MS_USERS	0x40000000
#define MS_USER		0x20000000
#define MS_OWNER	0x10000000
#define MS_GROUP	0x08000000
#define MS_COMMENT	0x02000000
#define MS_LOOP		0x00010000

#define MS_COMMENT_NOFAIL	(1 << 1)
#define MS_COMMENT_NETDEV	(1 << 2)

/* Options that we keep the mount system call from seeing.  */
#define MS_NOSYS	(MS_NOAUTO|MS_USERS|MS_USER|MS_COMMENT|MS_LOOP)

/* Options that we keep from appearing in the options field in the mtab.  */
#define MS_NOMTAB	(MS_REMOUNT|MS_NOAUTO|MS_USERS|MS_USER)

#define MS_PROPAGATION  (MS_SHARED|MS_SLAVE|MS_UNBINDABLE|MS_PRIVATE)

/* Options that we make ordinary users have by default.  */
#define MS_SECURE	(MS_NOEXEC|MS_NOSUID|MS_NODEV)

/* Options that we make owner-mounted devices have by default */
#define MS_OWNERSECURE	(MS_NOSUID|MS_NODEV)

static const struct opt_map opt_map[] = {
  { "defaults",	0, 0, 0		},	/* default options */
  { "ro",	1, 0, MS_RDONLY	},	/* read-only */
  { "rw",	1, 1, MS_RDONLY	},	/* read-write */
  { "exec",	0, 1, MS_NOEXEC	},	/* permit execution of binaries */
  { "noexec",	0, 0, MS_NOEXEC	},	/* don't execute binaries */
  { "suid",	0, 1, MS_NOSUID	},	/* honor suid executables */
  { "nosuid",	0, 0, MS_NOSUID	},	/* don't honor suid executables */
  { "dev",	0, 1, MS_NODEV	},	/* interpret device files  */
  { "nodev",	0, 0, MS_NODEV	},	/* don't interpret devices */
  { "sync",	0, 0, MS_SYNCHRONOUS},	/* synchronous I/O */
  { "async",	0, 1, MS_SYNCHRONOUS},	/* asynchronous I/O */
  { "dirsync",	0, 0, MS_DIRSYNC},	/* synchronous directory modifications */
  { "remount",  0, 0, MS_REMOUNT},      /* Alter flags of mounted FS */
  { "bind",	0, 0, MS_BIND   },	/* Remount part of tree elsewhere */
  { "rbind",	0, 0, MS_BIND|MS_REC }, /* Idem, plus mounted subtrees */
  { "auto",	0, 1, MS_NOAUTO	},	/* Can be mounted using -a */
  { "noauto",	0, 0, MS_NOAUTO	},	/* Can  only be mounted explicitly */
  { "users",	0, 0, MS_USERS	},	/* Allow ordinary user to mount */
  { "nousers",	0, 1, MS_USERS	},	/* Forbid ordinary user to mount */
  { "user",	0, 0, MS_USER	},	/* Allow ordinary user to mount */
  { "nouser",	0, 1, MS_USER	},	/* Forbid ordinary user to mount */
  { "owner",	0, 0, MS_OWNER  },	/* Let the owner of the device mount */
  { "noowner",	0, 1, MS_OWNER  },	/* Device owner has no special privs */
  { "group",	0, 0, MS_GROUP  },	/* Let the group of the device mount */
  { "nogroup",	0, 1, MS_GROUP  },	/* Device group has no special privs */
  { "_netdev",	0, 0, MS_COMMENT, MS_COMMENT_NETDEV },	/* Device requires network */
  { "comment",	0, 0, MS_COMMENT},	/* fstab comment only (kudzu,_netdev)*/

  /* add new options here */
#ifdef MS_NOSUB
  { "sub",	0, 1, MS_NOSUB	},	/* allow submounts */
  { "nosub",	0, 0, MS_NOSUB	},	/* don't allow submounts */
#endif
#ifdef MS_SILENT
  { "silent",	0, 0, MS_SILENT    },	/* be quiet  */
  { "loud",	0, 1, MS_SILENT    },	/* print out messages. */
#endif
#ifdef MS_MANDLOCK
  { "mand",	0, 0, MS_MANDLOCK },	/* Allow mandatory locks on this FS */
  { "nomand",	0, 1, MS_MANDLOCK },	/* Forbid mandatory locks on this FS */
#endif
  { "loop",	1, 0, MS_LOOP	},	/* use a loop device */
#ifdef MS_NOATIME
  { "atime",	0, 1, MS_NOATIME },	/* Update access time */
  { "noatime",	0, 0, MS_NOATIME },	/* Do not update access time */
#endif
#ifdef MS_I_VERSION
  { "iversion",	0, 0, MS_I_VERSION },	/* Update inode I_version time */
  { "noiversion", 0, 1, MS_I_VERSION },	/* Don't update inode I_version time */
#endif
#ifdef MS_NODIRATIME
  { "diratime",	0, 1, MS_NODIRATIME },	/* Update dir access times */
  { "nodiratime", 0, 0, MS_NODIRATIME },/* Do not update dir access times */
#endif
#ifdef MS_RELATIME
  { "relatime",	0, 0, MS_RELATIME },   /* Update access times relative to
					  mtime/ctime */
  { "norelatime", 0, 1, MS_RELATIME }, /* Update access time without regard
					  to mtime/ctime */
#endif
#ifdef MS_STRICTATIME
  { "strictatime", 0, 0, MS_STRICTATIME }, /* Strict atime semantics */
  { "nostrictatime", 0, 1, MS_STRICTATIME }, /* kernel default atime */
#endif
  { "nofail",	0, 0, MS_COMMENT, MS_COMMENT_NOFAIL },	/* Do not fail if ENOENT on dev */
  { NULL,	0, 0, 0		}
};

static int opt_nofail;
static int invuser_flags;
static int comment_flags;

static const char *opt_loopdev, *opt_vfstype, *opt_offset, *opt_sizelimit,
        *opt_encryption, *opt_speed, *opt_comment, *opt_uhelper, *opt_helper;

static int is_readonly(const char *node);
static int mounted (const char *spec0, const char *node0, struct mntentchn *fstab_mc);
static int check_special_mountprog(const char *spec, const char *node,
		const char *type, int flags, char *extra_opts, int *status);

static struct string_opt_map {
  char *tag;
  int skip;
  const char **valptr;
} string_opt_map[] = {
  { "loop=",	0, &opt_loopdev },
  { "vfs=",	1, &opt_vfstype },
  { "offset=",	0, &opt_offset },
  { "sizelimit=",  0, &opt_sizelimit },
  { "encryption=", 0, &opt_encryption },
  { "speed=", 0, &opt_speed },
  { "comment=", 1, &opt_comment },
  { "uhelper=", 0, &opt_uhelper },
  { "helper=", 0, &opt_helper },
  { NULL, 0, NULL }
};

static void
clear_string_opts(void) {
	struct string_opt_map *m;

	for (m = &string_opt_map[0]; m->tag; m++)
		*(m->valptr) = NULL;
}

static void
clear_flags_opts(void) {
	invuser_flags = 0;
	comment_flags = 0;
	opt_nofail = 0;
}

static int
parse_string_opt(char *s) {
	struct string_opt_map *m;
	int lth;

	for (m = &string_opt_map[0]; m->tag; m++) {
		lth = strlen(m->tag);
		if (!strncmp(s, m->tag, lth)) {
			*(m->valptr) = xstrdup(s + lth);
			return 1;
		}
	}
	return 0;
}

/* Report on a single mount.  */
static void
print_one (const struct my_mntent *me) {

	char *fsname = NULL;

	if (mount_quiet)
		return;

	/* users assume backing file name rather than /dev/loopN in
	 * mount(8) output if the device has been initialized by mount(8).
	 */
	if (strncmp(me->mnt_fsname, "/dev/loop", 9) == 0 &&
	    loopdev_is_autoclear(me->mnt_fsname))
		fsname = loopdev_get_backing_file(me->mnt_fsname);

	if (!fsname)
		fsname = (char *) me->mnt_fsname;

	printf ("%s on %s", fsname, me->mnt_dir);
	if (me->mnt_type != NULL && *(me->mnt_type) != '\0')
		printf (" type %s", me->mnt_type);
	if (me->mnt_opts != NULL)
		printf (" (%s)", me->mnt_opts);
	if (list_with_volumelabel && is_pseudo_fs(me->mnt_type) == 0) {
		const char *devname = spec_to_devname(me->mnt_fsname);

		if (devname) {
			const char *label;

			label = fsprobe_get_label_by_devname(devname);
			my_free(devname);

			if (label) {
				printf (" [%s]", label);
				my_free(label);
			}
		}
	}
	printf ("\n");
}

/* Report on everything in mtab (of the specified types if any).  */
static int
print_all (char *types) {
     struct mntentchn *mc, *mc0;

     mc0 = mtab_head();
     for (mc = mc0->nxt; mc && mc != mc0; mc = mc->nxt) {
	  if (matching_type (mc->m.mnt_type, types))
	       print_one (&(mc->m));
     }

     if (!mtab_does_not_exist() && !mtab_is_a_symlink() && is_readonly(_PATH_MOUNTED))
          printf(_("\n"
	"mount: warning: /etc/mtab is not writable (e.g. read-only filesystem).\n"
	"       It's possible that information reported by mount(8) is not\n"
	"       up to date. For actual information about system mount points\n"
	"       check the /proc/mounts file.\n\n"));

     exit (0);
}

/* reallocates its first arg */
static char *
append_opt(char *s, const char *opt, const char *val)
{
	if (!opt)
		return s;
	if (!s) {
		if (!val)
		       return xstrdup(opt);		/* opt */

		return xstrconcat3(NULL, opt, val);	/* opt=val */
	}
	if (!val)
		return xstrconcat3(s, ",", opt);	/* s,opt */

	return xstrconcat4(s, ",", opt, val);		/* s,opt=val */
}

static char *
append_numopt(char *s, const char *opt, unsigned int num)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%u", num);
	return append_opt(s, opt, buf);
}

#ifdef HAVE_LIBSELINUX
/* strip quotes from a "string"
 * Warning: This function modify the "str" argument.
 */
static char *
strip_quotes(char *str)
{
	char *end = NULL;

	if (*str != '"')
		return str;

	end = strrchr(str, '"');
	if (end == NULL || end == str)
		die (EX_USAGE, _("mount: improperly quoted option string '%s'"), str);

	*end = '\0';
	return str+1;
}

/* translates SELinux context from human to raw format and
 * appends it to the mount extra options.
 *
 * returns -1 on error and 0 on success
 */
static int
append_context(const char *optname, char *optdata, char **extra_opts)
{
	security_context_t raw = NULL;
	char *data = NULL;

	if (is_selinux_enabled() != 1)
		/* ignore the option if we running without selinux */
		return 0;

	if (optdata==NULL || *optdata=='\0' || optname==NULL)
		return -1;

	/* TODO: use strip_quotes() for all mount options? */
	data = *optdata =='"' ? strip_quotes(optdata) : optdata;

	if (selinux_trans_to_raw_context(
			(security_context_t) data, &raw) == -1 ||
			raw == NULL)
		return -1;

	if (verbose)
		printf(_("mount: translated %s '%s' to '%s'\n"),
				optname, data, (char *) raw);

	*extra_opts = append_opt(*extra_opts, optname, NULL);
	*extra_opts = xstrconcat4(*extra_opts, "\"", (char *) raw, "\"");

	freecon(raw);
	return 0;
}

/* returns newly allocated string without *context= options */
static char *remove_context_options(char *opts)
{
	char *begin = NULL, *end = NULL, *p;
	int open_quote = 0, changed = 0;

	if (!opts)
		return NULL;

	opts = xstrdup(opts);

	for (p = opts; p && *p; p++) {
		if (!begin)
			begin = p;		/* begin of the option item */
		if (*p == '"')
			open_quote ^= 1;	/* reverse the status */
		if (open_quote)
			continue;		/* still in quoted block */
		if (*p == ',')
			end = p;		/* terminate the option item */
		else if (*(p + 1) == '\0')
			end = p + 1;		/* end of optstr */
		if (!begin || !end)
			continue;

		if (strncmp(begin, "context=", 8) == 0 ||
		    strncmp(begin, "fscontext=", 10) == 0 ||
		    strncmp(begin, "defcontext=", 11) == 0 ||
	            strncmp(begin, "rootcontext=", 12) == 0 ||
		    strncmp(begin, "seclabel", 8) == 0) {
			size_t sz;

			if ((begin == opts || *(begin - 1) == ',') && *end == ',')
				end++;
			sz = strlen(end);

			memmove(begin, end, sz + 1);
			if (!*begin && *(begin - 1) == ',')
				*(begin - 1) = '\0';

			p = begin;
			changed = 1;
		}
		begin = end = NULL;
	}

	if (changed && verbose)
		printf (_("mount: SELinux *context= options are ignore on remount.\n"));

	return opts;
}

static int has_context_option(char *opts)
{
	if (get_option("context=", opts, NULL) ||
	    get_option("fscontext=", opts, NULL) ||
	    get_option("defcontext=", opts, NULL) ||
	    get_option("rootcontext=", opts, NULL))
		return 1;

	return 0;
}

#endif

/*
 * Look for OPT in opt_map table and return mask value.
 * If OPT isn't found, tack it onto extra_opts (which is non-NULL).
 * For the options uid= and gid= replace user or group name by its value.
 */
static inline void
parse_opt(char *opt, int *mask, int *inv_user, char **extra_opts) {
	const struct opt_map *om;

	for (om = opt_map; om->opt != NULL; om++)
		if (streq (opt, om->opt)) {
			if (om->inv)
				*mask &= ~om->mask;
			else
				*mask |= om->mask;
			if (om->inv && ((*mask & MS_USER) || (*mask & MS_USERS))
			    && (om->mask & MS_SECURE))
				*inv_user |= om->mask;
			if ((om->mask == MS_USER || om->mask == MS_USERS)
			    && !om->inv)
				*mask |= MS_SECURE;
			if ((om->mask == MS_OWNER || om->mask == MS_GROUP)
			    && !om->inv)
				*mask |= MS_OWNERSECURE;
#ifdef MS_SILENT
			if (om->mask == MS_SILENT && om->inv)  {
				mount_quiet = 1;
				verbose = 0;
			}
#endif
			if (om->mask == MS_COMMENT) {
				comment_flags |= om->cmask;
				if (om->cmask == MS_COMMENT_NOFAIL)
					opt_nofail = 1;
			}
			return;
		}

	/* convert nonnumeric ids to numeric */
	if (!strncmp(opt, "uid=", 4) && !isdigit(opt[4])) {
		struct passwd *pw = getpwnam(opt+4);

		if (pw) {
			*extra_opts = append_numopt(*extra_opts,
						"uid=", pw->pw_uid);
			return;
		}
	}
	if (!strncmp(opt, "gid=", 4) && !isdigit(opt[4])) {
		struct group *gr = getgrnam(opt+4);

		if (gr) {
			*extra_opts = append_numopt(*extra_opts,
						"gid=", gr->gr_gid);
			return;
		}
	}

#ifdef HAVE_LIBSELINUX
	if (strncmp(opt, "context=", 8) == 0 && *(opt+8)) {
		if (append_context("context=", opt+8, extra_opts) == 0)
			return;
	}
	if (strncmp(opt, "fscontext=", 10) == 0 && *(opt+10)) {
		if (append_context("fscontext=", opt+10, extra_opts) == 0)
			return;
	}
	if (strncmp(opt, "defcontext=", 11) == 0 && *(opt+11)) {
		if (append_context("defcontext=", opt+11, extra_opts) == 0)
			return;
	}
	if (strncmp(opt, "rootcontext=", 12) == 0 && *(opt+12)) {
		if (append_context("rootcontext=", opt+12, extra_opts) == 0)
			return;
	}
#endif
	*extra_opts = append_opt(*extra_opts, opt, NULL);
}


/* Take -o options list and compute 4th and 5th args to mount(2).  flags
   gets the standard options (indicated by bits) and extra_opts all the rest */
static void
parse_opts (const char *options, int *flags, char **extra_opts) {
	*flags = 0;
	*extra_opts = NULL;

	clear_string_opts();
	clear_flags_opts();

	if (options != NULL) {
		char *opts = xstrdup(options);
		int open_quote = 0;
		char *opt, *p;

		for (p=opts, opt=NULL; p && *p; p++) {
			if (!opt)
				opt = p;		/* begin of the option item */
			if (*p == '"')
				open_quote ^= 1;	/* reverse the status */
			if (open_quote)
				continue;		/* still in quoted block */
			if (*p == ',')
				*p = '\0';		/* terminate the option item */
			/* end of option item or last item */
			if (*p == '\0' || *(p+1) == '\0') {
				if (!parse_string_opt(opt))
					parse_opt(opt, flags, &invuser_flags, extra_opts);
				opt = NULL;
			}
		}
		free(opts);
	}

	if (readonly)
		*flags |= MS_RDONLY;
	if (readwrite)
		*flags &= ~MS_RDONLY;

	*flags |= mounttype;

	/* The propagation flags should not be used together with any other flags */
	if (*flags & MS_PROPAGATION)
		*flags &= MS_PROPAGATION;
}

/* Try to build a canonical options string.  */
static char *
fix_opts_string (int flags, const char *extra_opts,
		 const char *user, int inv_user)
{
	const struct opt_map *om;
	const struct string_opt_map *m;
	char *new_opts;

	new_opts = append_opt(NULL, (flags & MS_RDONLY) ? "ro" : "rw", NULL);
	for (om = opt_map; om->opt != NULL; om++) {
		if (om->skip)
			continue;
		if (om->inv || !om->mask || (flags & om->mask) != om->mask)
			continue;
		if (om->mask == MS_COMMENT && !(comment_flags & om->cmask))
			continue;
		new_opts = append_opt(new_opts, om->opt, NULL);
		flags &= ~om->mask;
	}
	for (m = &string_opt_map[0]; m->tag; m++) {
		if (!m->skip && *(m->valptr))
			new_opts = append_opt(new_opts, m->tag, *(m->valptr));
	}
	if (extra_opts && *extra_opts)
		new_opts = append_opt(new_opts, extra_opts, NULL);

	if (user)
		new_opts = append_opt(new_opts, "user=", user);

	if (inv_user) {
		for (om = opt_map; om->opt != NULL; om++) {
			if (om->mask && om->inv
			    && (inv_user & om->mask) == om->mask) {
				new_opts = append_opt(new_opts, om->opt, NULL);
				inv_user &= ~om->mask;
			}
		}
	}

	return new_opts;
}

static int
already (const char *spec0, const char *node0) {
	struct mntentchn *mc;
	int ret = 1;
	char *spec = canonicalize_spec(spec0);
	char *node = canonicalize(node0);

	if ((mc = getmntfile(node)) != NULL)
		error (_("mount: according to mtab, "
			 "%s is already mounted on %s"),
		       mc->m.mnt_fsname, node);
	else if (spec && strcmp (spec, "none") &&
		 (mc = getmntfile(spec)) != NULL)
		error (_("mount: according to mtab, %s is mounted on %s"),
		       spec, mc->m.mnt_dir);
	else
		ret = 0;

	free(spec);
	free(node);

	return ret;
}

/* Create mtab with a root entry.  */
static void
create_mtab (void) {
	struct mntentchn *fstab;
	struct my_mntent mnt;
	int flags;
	mntFILE *mfp;

	lock_mtab();

	mfp = my_setmntent (_PATH_MOUNTED, "a+");
	if (mfp == NULL || mfp->mntent_fp == NULL) {
		int errsv = errno;
		die (EX_FILEIO, _("mount: can't open %s for writing: %s"),
		     _PATH_MOUNTED, strerror (errsv));
	}

	/* Find the root entry by looking it up in fstab */
	if ((fstab = getfs_by_dir ("/")) || (fstab = getfs_by_dir ("root"))) {
		char *extra_opts;
		parse_opts (fstab->m.mnt_opts, &flags, &extra_opts);
		mnt.mnt_dir = "/";
		mnt.mnt_fsname = spec_to_devname(fstab->m.mnt_fsname);
		mnt.mnt_type = fstab->m.mnt_type;
		mnt.mnt_opts = fix_opts_string (flags, extra_opts, NULL, 0);
		mnt.mnt_freq = mnt.mnt_passno = 0;
		free(extra_opts);

		if (my_addmntent (mfp, &mnt) == 1) {
			int errsv = errno;
			die (EX_FILEIO, _("mount: error writing %s: %s"),
			     _PATH_MOUNTED, strerror (errsv));
		}
	}
	if (fchmod (fileno (mfp->mntent_fp), 0644) < 0)
		if (errno != EROFS) {
			int errsv = errno;
			die (EX_FILEIO,
			     _("mount: error changing mode of %s: %s"),
			     _PATH_MOUNTED, strerror (errsv));
		}
	my_endmntent (mfp);

	unlock_mtab();

	reset_mtab_info();
}

/* count successful mount system calls */
static int mountcount = 0;

/*
 * do_mount_syscall()
 *	Mount a single file system. Keep track of successes.
 * returns: 0: OK, -1: error in errno
 */
static int
do_mount_syscall (struct mountargs *args) {
	int flags = args->flags;

	if ((flags & MS_MGC_MSK) == 0)
		flags |= MS_MGC_VAL;

	if (verbose > 2)
		printf("mount: mount(2) syscall: source: \"%s\", target: \"%s\", "
			"filesystemtype: \"%s\", mountflags: %d, data: %s\n",
			args->spec, args->node, args->type, flags, (char *) args->data);

	return mount (args->spec, args->node, args->type, flags, args->data);
}

/*
 * do_mount()
 *	Mount a single file system, possibly invoking an external handler to
 *      do so. Keep track of successes.
 * returns: 0: OK, -1: error in errno
 */
static int
do_mount (struct mountargs *args, int *special, int *status) {
	int ret;
	if (check_special_mountprog(args->spec, args->node, args->type,
	                            args->flags, args->data, status)) {
		*special = 1;
		ret = 0;
	} else {
#ifdef HAVE_LIBMOUNT_MOUNT
		prepare_mtab_entry(args->spec, args->node, args->type,
				mtab_opts, mtab_flags);
#endif
		ret = do_mount_syscall(args);
	}
	if (ret == 0)
		mountcount++;
	return ret;
}

/*
 * check_special_mountprog()
 *	If there is a special mount program for this type, exec it.
 * returns: 0: no exec was done, 1: exec was done, status has result
 */
static int
check_special_mountprog(const char *spec, const char *node, const char *type, int flags,
			char *extra_opts, int *status) {
	char search_path[] = FS_SEARCH_PATH;
	char *path, mountprog[150];
	struct stat statbuf;
	int res;

	if (!external_allowed)
		return 0;

	if (type == NULL || strcmp(type, "none") == 0)
		return 0;

	path = strtok(search_path, ":");
	while (path) {
		int type_opt = 0;

		res = snprintf(mountprog, sizeof(mountprog), "%s/mount.%s",
			       path, type);
		path = strtok(NULL, ":");
		if (res < 0 || (size_t) res >= sizeof(mountprog))
			continue;

		res = stat(mountprog, &statbuf);
		if (res == -1 && errno == ENOENT && strchr(type, '.')) {
			/* If type ends with ".subtype" try without it */
			*strrchr(mountprog, '.') = '\0';
			type_opt = 1;
			res = stat(mountprog, &statbuf);
		}
		if (res)
			continue;

		if (verbose)
			fflush(stdout);

		switch (fork()) {
		case 0: { /* child */
			char *oo, *mountargs[12];
			int i = 0;

			if (setgid(getgid()) < 0)
				die(EX_FAIL, _("mount: cannot set group id: %m"));

			if (setuid(getuid()) < 0)
				die(EX_FAIL, _("mount: cannot set user id: %m"));

			oo = fix_opts_string(flags, extra_opts, NULL, invuser_flags);
			mountargs[i++] = mountprog;			/* 1 */
			mountargs[i++] = (char *) spec;			/* 2 */
			mountargs[i++] = (char *) node;			/* 3 */
			if (sloppy && strncmp(type, "nfs", 3) == 0)
				mountargs[i++] = "-s";			/* 4 */
			if (fake)
				mountargs[i++] = "-f";			/* 5 */
			if (nomtab)
				mountargs[i++] = "-n";			/* 6 */
			if (verbose)
				mountargs[i++] = "-v";			/* 7 */
			if (oo && *oo) {
				mountargs[i++] = "-o";			/* 8 */
				mountargs[i++] = oo;			/* 9 */
			}
			if (type_opt) {
				mountargs[i++] = "-t";			/* 10 */
				mountargs[i++] = (char *) type;		/* 11 */
			}
			mountargs[i] = NULL;				/* 12 */

			if (verbose > 2) {
				i = 0;
				while (mountargs[i]) {
					printf("mount: external mount: argv[%d] = \"%s\"\n",
						i, mountargs[i]);
					i++;
				}
				fflush(stdout);
			}

			execv(mountprog, mountargs);
			exit(1);	/* exec failed */
		}

		default: { /* parent */
			int st;
			wait(&st);
			*status = (WIFEXITED(st) ? WEXITSTATUS(st) : EX_SYSERR);
			return 1;
		}

		case -1: { /* error */
			int errsv = errno;
			error(_("mount: cannot fork: %s"), strerror(errsv));
		}
		}
	}

	return 0;
}


/* list of already tested filesystems by procfsloop_mount() */
static struct tried {
	struct tried *next;
	char *type;
} *tried = NULL;

static int
was_tested(const char *fstype) {
	struct tried *t;

	for (t = tried; t; t = t->next) {
		if (!strcmp(t->type, fstype))
			return 1;
	}
	return 0;
}

static void
set_tested(const char *fstype) {
	struct tried *t = xmalloc(sizeof(struct tried));

	t->next = tried;
	t->type = xstrdup(fstype);
	tried = t;
}

static void
free_tested(void) {
	struct tried *t, *tt;

	t = tried;
	while(t) {
		free(t->type);
		tt = t->next;
		free(t);
		t = tt;
	}
	tried = NULL;
}

static char *
procfsnext(FILE *procfs) {
   char line[100];
   char fsname[100];

   while (fgets(line, sizeof(line), procfs)) {
      if (sscanf (line, "nodev %[^#\n]\n", fsname) == 1) continue;
      if (sscanf (line, " %[^# \n]\n", fsname) != 1) continue;
      return xstrdup(fsname);
   }
   return 0;
}

/* Only use /proc/filesystems here, this is meant to test what
   the kernel knows about, so /etc/filesystems is irrelevant.
   Return: 1: yes, 0: no, -1: cannot open procfs */
static int
known_fstype_in_procfs(const char *type)
{
    FILE *procfs;
    char *fsname;
    int ret = -1;

    procfs = fopen(_PATH_PROC_FILESYSTEMS, "r");
    if (procfs) {
	ret = 0;
	while ((fsname = procfsnext(procfs)) != NULL)
	    if (!strcmp(fsname, type)) {
		ret = 1;
		break;
	    }
	fclose(procfs);
	procfs = NULL;
    }
    return ret;
}

/* Try all types in FILESYSTEMS, except those in *types,
   in case *types starts with "no" */
/* return: 0: OK, -1: error in errno, 1: type not found */
/* when 0 or -1 is returned, *types contains the type used */
/* when 1 is returned, *types is NULL */
static int
procfsloop_mount(int (*mount_fn)(struct mountargs *, int *, int *),
			 struct mountargs *args,
			 const char **types,
			 int *special, int *status)
{
	char *files[2] = { _PATH_FILESYSTEMS, _PATH_PROC_FILESYSTEMS };
	FILE *procfs;
	char *fsname;
	const char *notypes = NULL;
	int no = 0;
	int ret = 1;
	int errsv = 0;
	int i;

	if (*types && !strncmp(*types, "no", 2)) {
		no = 1;
		notypes = (*types) + 2;
	}
	*types = NULL;

	/* Use _PATH_PROC_FILESYSTEMS only when _PATH_FILESYSTEMS
	 * (/etc/filesystems) does not exist.  In some cases trying a
	 * filesystem that the kernel knows about on the wrong data will crash
	 * the kernel; in such cases _PATH_FILESYSTEMS can be used to list the
	 * filesystems that we are allowed to try, and in the order they should
	 * be tried.  End _PATH_FILESYSTEMS with a line containing a single '*'
	 * only, if _PATH_PROC_FILESYSTEMS should be tried afterwards.
	 */
	for (i=0; i<2; i++) {
		procfs = fopen(files[i], "r");
		if (!procfs)
			continue;
		while ((fsname = procfsnext(procfs)) != NULL) {
			if (!strcmp(fsname, "*")) {
				fclose(procfs);
				goto nexti;
			}
			if (was_tested (fsname))
				continue;
			if (no && matching_type(fsname, notypes))
				continue;
			set_tested (fsname);
			args->type = fsname;
			if (verbose)
				printf(_("Trying %s\n"), fsname);
			if ((*mount_fn) (args, special, status) == 0) {
				*types = fsname;
				ret = 0;
				break;
			} else if (errno != EINVAL &&
				   known_fstype_in_procfs(fsname) == 1) {
				*types = "guess";
				ret = -1;
				errsv = errno;
				break;
			}
		}
		free_tested();
		fclose(procfs);
		errno = errsv;
		return ret;
	nexti:;
	}
	return 1;
}

static const char *
guess_fstype_by_devname(const char *devname, int *ambivalent)
{
   const char *type = fsprobe_get_fstype_by_devname_ambi(devname, ambivalent);

   if (verbose) {
      printf (_("mount: you didn't specify a filesystem type for %s\n"), devname);

      if (!type)
         printf (_("       I will try all types mentioned in %s or %s\n"),
		      _PATH_FILESYSTEMS, _PATH_PROC_FILESYSTEMS);
      else if (!strcmp(type, MNTTYPE_SWAP))
         printf (_("       and it looks like this is swapspace\n"));
      else
         printf (_("       I will try type %s\n"), type);
   }
   return type;
}

/*
 * guess_fstype_and_mount()
 *	Mount a single file system. Guess the type when unknown.
 * returns: 0: OK, -1: error in errno, 1: other error
 *	don't exit on non-fatal errors.
 *	on return types is filled with the type used.
 */
static int
guess_fstype_and_mount(const char *spec, const char *node, const char **types,
		       int flags, char *mount_opts, int *special, int *status) {
   struct mountargs args = { spec, node, NULL, flags & ~MS_NOSYS, mount_opts };
   int ambivalent = 0;

   if (*types && strcasecmp (*types, "auto") == 0)
      *types = NULL;

   if (!*types && !(flags & MS_REMOUNT)) {
      *types = guess_fstype_by_devname(spec, &ambivalent);
      if (*types) {
	  if (!strcmp(*types, MNTTYPE_SWAP)) {
	      error(_("%s looks like swapspace - not mounted"), spec);
	      *types = NULL;
	      return 1;
	  } else {
	      args.type = *types;
	      return do_mount (&args, special, status);
          }
      } else if (ambivalent) {
          error(_("mount: %s: more filesystems detected. This should not happen,\n"
		  "       use -t <type> to explicitly specify the filesystem type or\n"
		  "       use wipefs(8) to clean up the device.\n"), spec);
	  return 1;
      }
   }

   /* Accept a comma-separated list of types, and try them one by one */
   /* A list like "nonfs,.." indicates types not to use */
   if (*types && strncmp(*types, "no", 2) && strchr(*types,',')) {
      char *t = strdup(*types);
      char *p;

      while((p = strchr(t,',')) != NULL) {
	 *p = 0;
	 args.type = *types = t;
	 if (do_mount (&args, special, status) == 0)
	    return 0;
	 t = p+1;
      }
      /* do last type below */
      *types = t;
   }

   if (*types || (flags & MS_REMOUNT)) {
      args.type = *types;
      return do_mount (&args, special, status);
   }

   return procfsloop_mount(do_mount, &args, types, special, status);
}

/*
 * restricted_check()
 *	Die if the user is not allowed to do this.
 */
static void
restricted_check(const char *spec, const char *node, int *flags, char **user) {
  if (restricted) {
      /*
       * MS_OWNER: Allow owners to mount when fstab contains
       * the owner option.  Note that this should never be used
       * in a high security environment, but may be useful to give
       * people at the console the possibility of mounting a floppy.
       * MS_GROUP: Allow members of device group to mount. (Martin Dickopp)
       */
      if (*flags & (MS_OWNER | MS_GROUP)) {
	  struct stat sb;

	  if (!strncmp(spec, "/dev/", 5) && stat(spec, &sb) == 0) {

	      if (*flags & MS_OWNER) {
		  if (getuid() == sb.st_uid)
		      *flags |= MS_USER;
	      }

	      if (*flags & MS_GROUP) {
		  if (getgid() == sb.st_gid)
		      *flags |= MS_USER;
		  else {
		      int n = getgroups(0, NULL);

		      if (n > 0) {
			      gid_t *groups = xmalloc(n * sizeof(*groups));
			      if (getgroups(n, groups) == n) {
				      int i;
				      for (i = 0; i < n; i++) {
					      if (groups[i] == sb.st_gid) {
						      *flags |= MS_USER;
						      break;
					      }
				      }
			      }
			      free(groups);
		      }
		  }
	      }
	  }
      }

      /* James Kehl <mkehl@gil.com.au> came with a similar patch:
	 allow an arbitrary user to mount when he is the owner of
	 the mount-point and has write-access to the device.
	 This is even less secure. Let me skip it for the time being;
	 there should be an explicit fstab line allowing such things. */

      if (!(*flags & (MS_USER | MS_USERS))) {
	  if (already (spec, node))
	    die (EX_USAGE, _("mount failed"));
	  else
	    die (EX_USAGE, _("mount: only root can mount %s on %s"), spec, node);
      }
      if (*flags & MS_USER)
	  *user = getusername();
  }

  *flags &= ~(MS_OWNER | MS_GROUP);
}

/* Check, if there already exists a mounted loop device on the mountpoint node
 * with the same parameters.
 */
static int
is_mounted_same_loopfile(const char *node0, const char *loopfile, unsigned long long offset)
{
	struct mntentchn *mnt = NULL;
	char *node;
	int res = 0;

	node = canonicalize(node0);

	/* Search for mountpoint node in mtab,
	 * procceed if any of these has the loop option set or
	 * the device is a loop device
	 */
	mnt = getmntdirbackward(node, mnt);
	if (!mnt) {
		free(node);
		return 0;
	}
	for(; mnt && res == 0; mnt = getmntdirbackward(node, mnt)) {
		char *p;

		if (strncmp(mnt->m.mnt_fsname, "/dev/loop", 9) == 0)
			res = loopdev_is_used((char *) mnt->m.mnt_fsname,
					loopfile, offset, LOOPDEV_FL_OFFSET);

		else if (mnt->m.mnt_opts &&
			 (p = strstr(mnt->m.mnt_opts, "loop=")))
		{
			char *dev = xstrdup(p+5);
			if ((p = strchr(dev, ',')))
				*p = '\0';
			res =  loopdev_is_used(dev,
					loopfile, offset, LOOPDEV_FL_OFFSET);
			free(dev);
		}
	}

	free(node);
	return res;
}

static int
parse_offset(const char **opt, uintmax_t *val)
{
	char *tmp;

	if (strtosize(*opt, val))
		return -1;

	tmp = xmalloc(32);
	snprintf(tmp, 32, "%jd", *val);
	my_free(*opt);
	*opt = tmp;
	return 0;
}

static int
loop_check(const char **spec, const char **type, int *flags,
	   int *loop, const char **loopdev, const char **loopfile,
	   const char *node) {
  int looptype;
  uintmax_t offset = 0, sizelimit = 0;
  struct loopdev_cxt lc;

  /*
   * In the case of a loop mount, either type is of the form lo@/dev/loop5
   * or the option "-o loop=/dev/loop5" or just "-o loop" is given, or
   * mount just has to figure things out for itself from the fact that
   * spec is not a block device. We do not test for a block device
   * immediately: maybe later other types of mountable objects will occur.
   */

  *loopdev = opt_loopdev;

  looptype = (*type && strncmp("lo@", *type, 3) == 0);
  if (looptype) {
    if (*loopdev)
      error(_("mount: loop device specified twice"));
    *loopdev = *type + 3;
    *type = opt_vfstype;
  } else if (opt_vfstype) {
    if (*type)
      error(_("mount: type specified twice"));
    else
      *type = opt_vfstype;
  }

  *loop = ((*flags & MS_LOOP) || *loopdev || opt_offset || opt_sizelimit || opt_encryption);
  *loopfile = *spec;

  /* Automatically create a loop device from a regular file if a filesystem
   * is not specified or the filesystem is known for libblkid (these
   * filesystems work with block devices only).
   *
   * Note that there is not a restriction (on kernel side) that prevents regular
   * file as a mount(2) source argument. A filesystem that is able to mount
   * regular files could be implemented.
   */
  if (!*loop && !(*flags & (MS_BIND | MS_MOVE | MS_PROPAGATION)) &&
      (!*type || strcmp(*type, "auto") == 0 || fsprobe_known_fstype(*type))) {

    struct stat st;
    if (stat(*loopfile, &st) == 0)
      *loop = S_ISREG(st.st_mode);
  }

  if (*loop) {
    *flags |= MS_LOOP;
    if (fake) {
      if (verbose)
	printf(_("mount: skipping the setup of a loop device\n"));
    } else {
      int loop_opts = 0;

      /* since 2.6.37 we don't have to store backing filename to mtab
       * because kernel provides the name in /sys
       */
      if (get_linux_version() >= KERNEL_VERSION(2, 6, 37) ||
	  mtab_is_writable() == 0) {

	if (verbose)
	  printf(_("mount: enabling autoclear loopdev flag\n"));
	loop_opts = LO_FLAGS_AUTOCLEAR;
      }

      if (*flags & MS_RDONLY)
        loop_opts |= LO_FLAGS_READ_ONLY;

      if (opt_offset && parse_offset(&opt_offset, &offset)) {
        error(_("mount: invalid offset '%s' specified"), opt_offset);
        return EX_FAIL;
      }
      if (opt_sizelimit && parse_offset(&opt_sizelimit, &sizelimit)) {
        error(_("mount: invalid sizelimit '%s' specified"), opt_sizelimit);
        return EX_FAIL;
      }

      if (is_mounted_same_loopfile(node, *loopfile, offset)) {
        error(_("mount: according to mtab %s is already mounted on %s as loop"), *loopfile, node);
        return EX_FAIL;
      }

      loopcxt_init(&lc, 0);
      /* loopcxt_enable_debug(&lc, 1); */

      if (*loopdev && **loopdev)
	loopcxt_set_device(&lc, *loopdev);	/* use loop=<devname> */

      do {
	int rc;

        if ((!*loopdev || !**loopdev) && loopcxt_find_unused(&lc) == 0)
	    *loopdev = loopcxt_strdup_device(&lc);

	if (!*loopdev) {
	  error(_("mount: failed to found free loop device"));
	  loopcxt_deinit(&lc);
	  goto err;	/* no more loop devices */
	}
	if (verbose)
	  printf(_("mount: going to use the loop device %s\n"), *loopdev);

	rc = loopcxt_set_backing_file(&lc, *loopfile);

	if (!rc && offset)
	  rc = loopcxt_set_offset(&lc, offset);
	if (!rc && sizelimit)
	  rc = loopcxt_set_sizelimit(&lc, sizelimit);
	if (!rc)
	  loopcxt_set_flags(&lc, loop_opts);

	if (rc) {
	   error(_("mount: %s: failed to set loopdev attributes"), *loopdev);
	   loopcxt_deinit(&lc);
	   goto err;
	}

	/* setup the device */
	rc = loopcxt_setup_device(&lc);
	if (!rc)
	  break;	/* success */

	if (rc != -EBUSY) {
	  if (verbose)
	    printf(_("mount: failed setting up loop device\n"));
	  if (!opt_loopdev) {
	    my_free(*loopdev);
	    *loopdev = NULL;
	  }
	  loopcxt_deinit(&lc);
	  goto err;
	}

	if (!opt_loopdev) {
	  if (verbose)
	    printf(_("mount: stolen loop=%s ...trying again\n"), *loopdev);
	    my_free(*loopdev);
	    *loopdev = NULL;
	    continue;
	}
	error(_("mount: stolen loop=%s"), *loopdev);
	loopcxt_deinit(&lc);
	goto err;

      } while (!*loopdev);

      if (verbose > 1)
	printf(_("mount: setup loop device successfully\n"));
      *spec = *loopdev;

      if (loopcxt_is_readonly(&lc))
        *flags |= MS_RDONLY;

      if (loopcxt_is_autoclear(&lc))
        /* Prevent recording loop dev in mtab for cleanup on umount */
        *loop = 0;

      /* We have to keep the device open until mount(2), otherwise it will
       * be auto-cleared by kernel (because LO_FLAGS_AUTOCLEAR) */
      loopcxt_set_fd(&lc, -1, 0);
      loopcxt_deinit(&lc);
    }
  }

  return 0;
err:
  return EX_FAIL;
}


#ifdef HAVE_LIBMOUNT_MOUNT
static void
verbose_mount_info(const char *spec, const char *node, const char *type,
		  const char *opts)
{
	struct my_mntent mnt;

	mnt.mnt_fsname = is_pseudo_fs(type) ? spec : canonicalize(spec);
	mnt.mnt_dir = canonicalize (node);
	mnt.mnt_type = type;
	mnt.mnt_opts = opts;

	print_one (&mnt);

	if (spec != mnt.mnt_fsname)
		my_free(mnt.mnt_fsname);
	my_free(mnt.mnt_dir);
}

static void
prepare_mtab_entry(const char *spec, const char *node, const char *type,
					  const char *opts, unsigned long flags)
{
	struct libmnt_fs *fs = mnt_new_fs();
	int rc = -1;

	if (!mtab_update)
		mtab_update = mnt_new_update();

	if (mtab_update && fs) {
		const char *cn_spec = is_pseudo_fs(type) ? spec : canonicalize(spec);
		const char *cn_node = canonicalize(node);

		mnt_fs_set_source(fs, cn_spec);
		mnt_fs_set_target(fs, cn_node);
		mnt_fs_set_fstype(fs, type);
		mnt_fs_set_options(fs, opts);

		rc = mnt_update_set_fs(mtab_update, flags, NULL, fs);

		if (spec != cn_spec)
			my_free(cn_spec);
		my_free(cn_node);
	}

	mnt_free_fs(fs);

	if (rc) {
		mnt_free_update(mtab_update);
		mtab_update = NULL;
	}
}

static void update_mtab_entry(int flags)
{
	unsigned long fl;

	if (!mtab_update)
		return;

	fl = mnt_update_get_mflags(mtab_update);

	if ((flags & MS_RDONLY) != (fl & MS_RDONLY))
		mnt_update_force_rdonly(mtab_update, flags & MS_RDONLY);

	if (!nomtab) {
		if (mtab_does_not_exist()) {
			if (verbose > 1)
				printf(_("mount: no %s found - creating it..\n"),
				       _PATH_MOUNTED);
			create_mtab ();
		}

		mnt_update_table(mtab_update, NULL);
	}

	mnt_free_update(mtab_update);
	mtab_update = NULL;
}

#else /*!HAVE_LIBMOUNT_MOUNT */
static void
update_mtab_entry(const char *spec, const char *node, const char *type,
		  const char *opts, int flags, int freq, int pass) {
	struct my_mntent mnt;

	mnt.mnt_fsname = is_pseudo_fs(type) ? xstrdup(spec) : canonicalize(spec);
	mnt.mnt_dir = canonicalize (node);
	mnt.mnt_type = type;
	mnt.mnt_opts = opts;
	mnt.mnt_freq = freq;
	mnt.mnt_passno = pass;

	/* We get chatty now rather than after the update to mtab since the
	   mount succeeded, even if the write to /etc/mtab should fail.  */
	if (verbose)
		print_one (&mnt);

	if (!nomtab && mtab_does_not_exist()) {
		if (verbose > 1)
			printf(_("mount: no %s found - creating it..\n"),
			       _PATH_MOUNTED);
		create_mtab ();

	}

	if (!nomtab && mtab_is_writable()) {
		if (flags & MS_REMOUNT)
			update_mtab (mnt.mnt_dir, &mnt);
		else if (flags & MS_MOVE)
			update_mtab(mnt.mnt_fsname, &mnt);
		else
			update_mtab(NULL, &mnt);
	}
	my_free(mnt.mnt_fsname);
	my_free(mnt.mnt_dir);
}
#endif /* !HAVE_LIBMOUNT_MOUNT */

static void
set_pfd(char *s) {
	if (!isdigit(*s))
		die(EX_USAGE,
		    _("mount: argument to -p or --pass-fd must be a number"));
	pfd = atoi(optarg);
}

static void
cdrom_setspeed(const char *spec) {
#define CDROM_SELECT_SPEED      0x5322  /* Set the CD-ROM speed */
	if (opt_speed) {
		int cdrom;
		int speed = atoi(opt_speed);

		if ((cdrom = open(spec, O_RDONLY | O_NONBLOCK)) < 0)
			die(EX_FAIL,
			    _("mount: cannot open %s for setting speed"),
			    spec);
		if (ioctl(cdrom, CDROM_SELECT_SPEED, speed) < 0)
			die(EX_FAIL, _("mount: cannot set speed: %m"));
		close(cdrom);
	}
}

/*
 * Check if @path is on read-only filesystem independently on file permissions.
 */
static int
is_readonly(const char *path)
{
	if (access(path, W_OK) == 0)
		return 0;
	if (errno == EROFS)
		return 1;
	if (errno != EACCES)
		return 0;

#ifdef HAVE_FUTIMENS
	/*
	 * access(2) returns EACCES on read-only FS:
	 *
	 * - for set-uid application if one component of the path is not
	 *   accessible for the current rUID. (Note that euidaccess(2) does not
	 *   check for EROFS at all).
	 *
	 * - for read-write filesystem with read-only VFS node (aka -o remount,ro,bind)
	 */
	{
		struct timespec times[2];

		times[0].tv_nsec = UTIME_NOW;	/* atime */
		times[1].tv_nsec = UTIME_OMIT;	/* mtime */

		if (utimensat(AT_FDCWD, path, times, 0) == -1)
			return errno == EROFS;
	}
#endif
	return 0;
}

/*
 * try_mount_one()
 *	Try to mount one file system.
 *
 * returns: 0: OK, EX_SYSERR, EX_FAIL, return code from nfsmount,
 *      return status from wait
 */
static int
try_mount_one (const char *spec0, const char *node0, const char *types0,
	       const char *opts0, int freq, int pass, int ro) {
  int res = 0, status = 0, special = 0;
  int mnt5_res = 0;		/* only for gcc */
  int mnt_err;
  int flags;
  char *extra_opts;		/* written in mtab */
  char *mount_opts;		/* actually used on system call */
  const char *opts, *spec, *node, *types;
  char *user = 0;
  int loop = 0;
  const char *loopdev = 0, *loopfile = 0;
  struct stat statbuf;

  /* copies for freeing on exit */
  const char *opts1, *spec1, *node1, *types1;

  if (verbose > 2) {
	  printf("mount: spec:  \"%s\"\n", spec0);
	  printf("mount: node:  \"%s\"\n", node0);
	  printf("mount: types: \"%s\"\n", types0);
	  printf("mount: opts:  \"%s\"\n", opts0);
  }

  spec = spec1 = xstrdup(spec0);
  node = node1 = xstrdup(node0);
  types = types1 = xstrdup(types0);
  opts = opts1 = xstrdup(opts0);

  parse_opts (opts, &flags, &extra_opts);
  mount_opts = xstrdup(extra_opts);

  /* quietly succeed for fstab entries that don't get mounted automatically */
  if (mount_all && (flags & MS_NOAUTO))
      goto out;

  restricted_check(spec, node, &flags, &user);

  /* The "mount -f" checks for for existing record in /etc/mtab (with
   * regular non-fake mount this is usually done by kernel)
   */
  if (!(flags & MS_REMOUNT) && fake && mounted (spec, node, NULL))
      die(EX_USAGE, _("mount: according to mtab, "
                      "%s is already mounted on %s\n"),
		      spec, node);

  if (opt_speed)
      cdrom_setspeed(spec);

  if (!(flags & MS_REMOUNT)) {
      /*
       * Don't set up a (new) loop device if we only remount - this left
       * stale assignments of files to loop devices. Nasty when used for
       * encryption.
       */
      res = loop_check(&spec, &types, &flags, &loop, &loopdev, &loopfile, node);
      if (res)
	  goto out;
  }

  if (loop)
      opt_loopdev = loopdev;

  if (flags & (MS_BIND | MS_MOVE | MS_PROPAGATION))
      types = "none";

#ifdef HAVE_LIBSELINUX
  if (flags & MS_REMOUNT) {
      /*
       * Linux kernel does not accept any selinux context option on remount
       */
      if (mount_opts) {
          char *tmp = mount_opts;
          mount_opts = remove_context_options(mount_opts);
          my_free(tmp);
      }

  } else if (types && strcmp(types, "tmpfs") == 0 && is_selinux_enabled() > 0 &&
	   !has_context_option(mount_opts)) {
      /*
       * Add rootcontext= mount option for tmpfs
       * https://bugzilla.redhat.com/show_bug.cgi?id=476964
       */
      security_context_t sc = NULL;

      if (getfilecon(node, &sc) > 0 && strcmp("unlabeled", sc))
	      append_context("rootcontext=", (char *) sc, &mount_opts);
      freecon(sc);
  }
#endif

  /*
   * Call mount.TYPE for types that require a separate mount program.
   * For the moment these types are ncpfs and smbfs. Maybe also vxfs.
   * All such special things must occur isolated in the types string.
   */
  if (check_special_mountprog(spec, node, types, flags, mount_opts, &status)) {
      res = status;
      goto out;
  }

#ifdef HAVE_LIBMOUNT_MOUNT
  mtab_opts = fix_opts_string(flags & ~MS_NOMTAB, extra_opts, user, 0);
  mtab_flags = flags;

  if (fake)
    prepare_mtab_entry(spec, node, types, mtab_opts, mtab_flags);
#endif

  block_signals (SIG_BLOCK);

  if (!fake) {
    mnt5_res = guess_fstype_and_mount (spec, node, &types, flags & ~MS_NOSYS,
				       mount_opts, &special, &status);

    if (special) {
      block_signals (SIG_UNBLOCK);
      res = status;
      goto out;
    }
  }

  /* Kernel allows to use MS_RDONLY for bind mounts, but the read-only request
   * could be silently ignored. Check it to avoid 'ro' in mtab and 'rw' in
   * /proc/mounts.
   */
  if (!fake && mnt5_res == 0 &&
      (flags & MS_BIND) && (flags & MS_RDONLY) && !is_readonly(node)) {

      printf(_("mount: warning: %s seems to be mounted read-write.\n"), node);
      flags &= ~MS_RDONLY;
  }

  /* Kernel can silently add MS_RDONLY flag when mounting file system that
   * does not have write support. Check this to avoid 'ro' in /proc/mounts
   * and 'rw' in mtab.
   */
  if (!fake && mnt5_res == 0 &&
      !(flags & (MS_RDONLY | MS_PROPAGATION | MS_MOVE)) &&
      is_readonly(node)) {

      printf(_("mount: warning: %s seems to be mounted read-only.\n"), node);
      flags |= MS_RDONLY;
  }

  if (fake || mnt5_res == 0) {
      char *mo = fix_opts_string (flags & ~MS_NOMTAB, extra_opts, user, 0);
      const char *tp = types ? types : "unknown";

      /* Mount succeeded, report this (if verbose) and write mtab entry.  */
#ifdef HAVE_LIBMOUNT_MOUNT
      update_mtab_entry(flags);
      if (verbose)
	      verbose_mount_info(loop ? loopfile : spec, node, tp, mo);
#else
      if (!(mounttype & MS_PROPAGATION))
	      update_mtab_entry(loop ? loopfile : spec,
			node,
			tp,
			mo,
			flags,
			freq,
			pass);
#endif
      block_signals (SIG_UNBLOCK);
      free(mo);

      res = 0;
      goto out;
  }

  mnt_err = errno;

  if (loop)
	loopdev_delete(spec);

  block_signals (SIG_UNBLOCK);

  /* Mount failed, complain, but don't die.  */

  if (types == 0) {
    if (restricted)
      error (_("mount: I could not determine the filesystem type, "
	       "and none was specified"));
    else
      error (_("mount: you must specify the filesystem type"));
  } else if (mnt5_res != -1) {
      /* should not happen */
      error (_("mount: mount failed"));
  } else {
   switch (mnt_err) {
    case EPERM:
      if (geteuid() == 0) {
	   if (stat (node, &statbuf) || !S_ISDIR(statbuf.st_mode))
		error (_("mount: mount point %s is not a directory"), node);
	   else
		error (_("mount: permission denied"));
      } else
	error (_("mount: must be superuser to use mount"));
      break;
    case EBUSY:
      if (flags & MS_REMOUNT) {
	error (_("mount: %s is busy"), node);
      } else if (!strcmp(types, "proc") && !strcmp(node, "/proc")) {
	/* heuristic: if /proc/version exists, then probably proc is mounted */
	if (stat ("/proc/version", &statbuf))   /* proc mounted? */
	   error (_("mount: %s is busy"), node);   /* no */
	else if (!mount_all || verbose)            /* yes, don't mention it */
	   error (_("mount: proc already mounted"));
      } else {
	error (_("mount: %s already mounted or %s busy"), spec, node);
	already (spec, node);
      }
      break;
    case ENOENT:
      if (lstat (node, &statbuf))
	   error (_("mount: mount point %s does not exist"), node);
      else if (stat (node, &statbuf))
	   error (_("mount: mount point %s is a symbolic link to nowhere"),
		  node);
      else if (stat (spec, &statbuf)) {
	   if (opt_nofail)
		goto out;
	   error (_("mount: special device %s does not exist"), spec);
      } else {
	   errno = mnt_err;
	   perror("mount");
      }
      break;
    case ENOTDIR:
      if (stat (node, &statbuf) || ! S_ISDIR(statbuf.st_mode))
	   error (_("mount: mount point %s is not a directory"), node);
      else if (stat (spec, &statbuf) && errno == ENOTDIR) {
	   if (opt_nofail)
              goto out;
	   error (_("mount: special device %s does not exist\n"
		    "       (a path prefix is not a directory)\n"), spec);
      } else {
	   errno = mnt_err;
	   perror("mount");
      }
      break;
    case EINVAL:
    { int fd;
      unsigned long long size = 0;

      if (flags & MS_REMOUNT) {
	error (_("mount: %s not mounted or bad option"), node);
      } else {
	error (_("mount: wrong fs type, bad option, bad superblock on %s,\n"
	       "       missing codepage or helper program, or other error"),
	       spec);

	if (stat(spec, &statbuf) < 0) {
	  if (errno == ENOENT)         /* network FS? */
	    error(_(
	       "       (for several filesystems (e.g. nfs, cifs) you might\n"
	       "       need a /sbin/mount.<type> helper program)"));

	} else if (S_ISBLK(statbuf.st_mode)
	                 && (fd = open(spec, O_RDONLY | O_NONBLOCK)) >= 0) {

	  if (blkdev_get_size(fd, &size) == 0) {
	    if (size == 0 && !loop)
	      error(_(
		 "       (could this be the IDE device where you in fact use\n"
		 "       ide-scsi so that sr0 or sda or so is needed?)"));

	    if (size && size <= 2)
	      error(_(
		  "       (aren't you trying to mount an extended partition,\n"
		  "       instead of some logical partition inside?)"));

	    close(fd);
	  }
	}
	error(_(
		"       In some cases useful info is found in syslog - try\n"
		"       dmesg | tail  or so\n"));
      }
      break;
    }
    case EMFILE:
      error (_("mount table full")); break;
    case EIO:
      error (_("mount: %s: can't read superblock"), spec); break;
    case ENODEV:
    {
      int pfs = known_fstype_in_procfs(types);

      if (pfs == 1 || !strcmp(types, "guess"))
        error(_("mount: %s: unknown device"), spec);
      else if (pfs == 0) {
	char *lowtype, *p;
	int u;

	error (_("mount: unknown filesystem type '%s'"), types);

	/* maybe this loser asked for FAT or ISO9660 or isofs */
	lowtype = xstrdup(types);
	u = 0;
	for(p=lowtype; *p; p++) {
	  if(tolower(*p) != *p) {
	    *p = tolower(*p);
	    u++;
	  }
	}
	if (u && known_fstype_in_procfs(lowtype) == 1)
	  error (_("mount: probably you meant %s"), lowtype);
	else if (!strncmp(lowtype, "iso", 3) &&
			known_fstype_in_procfs("iso9660") == 1)
	  error (_("mount: maybe you meant 'iso9660'?"));
	else if (!strncmp(lowtype, "fat", 3) &&
			known_fstype_in_procfs("vfat") == 1)
	  error (_("mount: maybe you meant 'vfat'?"));
	free(lowtype);
      } else
	error (_("mount: %s has wrong device number or fs type %s not supported"),
	       spec, types);
      break;
    }
    case ENOTBLK:
      if (opt_nofail)
        goto out;
      if (stat (spec, &statbuf)) /* strange ... */
	error (_("mount: %s is not a block device, and stat fails?"), spec);
      else if (S_ISBLK(statbuf.st_mode))
        error (_("mount: the kernel does not recognize %s as a block device\n"
	       "       (maybe `modprobe driver'?)"), spec);
      else if (S_ISREG(statbuf.st_mode))
	error (_("mount: %s is not a block device (maybe try `-o loop'?)"),
		 spec);
      else
	error (_("mount: %s is not a block device"), spec);
      break;
    case ENXIO:
      if (opt_nofail)
        goto out;
      error (_("mount: %s is not a valid block device"), spec); break;
    case EACCES:  /* pre-linux 1.1.38, 1.1.41 and later */
    case EROFS:   /* linux 1.1.38 and later */
    { char *bd = (loop ? "" : _("block device "));
      if (ro || (flags & MS_RDONLY)) {
          error (_("mount: cannot mount %s%s read-only"),
		 bd, spec);
          break;
      } else if (readwrite) {
	  error (_("mount: %s%s is write-protected but explicit `-w' flag given"),
		 bd, spec);
	  break;
      } else if (flags & MS_REMOUNT) {
	  error (_("mount: cannot remount %s%s read-write, is write-protected"),
		 bd, spec);
	  break;
      } else {
	 opts = opts0;
	 types = types0;

         if (opts) {
	     char *opts2 = append_opt(xstrdup(opts), "ro", NULL);
	     my_free(opts1);
	     opts = opts1 = opts2;
         } else
             opts = "ro";
	 if (types && !strcmp(types, "guess"))
	     types = 0;
         error (_("mount: %s%s is write-protected, mounting read-only"),
		bd, spec0);
	 res = try_mount_one (spec0, node0, types, opts, freq, pass, 1);
	 goto out;
      }
      break;
    }
    case ENOMEDIUM:
      error(_("mount: no medium found on %s"), spec);
      break;
    default:
      error ("mount: %s", strerror (mnt_err)); break;
    }
  }
  res = EX_FAIL;

 out:

#if defined(HAVE_LIBSELINUX) && defined(HAVE_SECURITY_GET_INITIAL_CONTEXT)
  if (res != EX_FAIL && verbose && is_selinux_enabled() > 0) {
      security_context_t raw = NULL, def = NULL;

      if (getfilecon(node, &raw) > 0 &&
		     security_get_initial_context("file", &def) == 0) {

	  if (!selinux_file_context_cmp(raw, def))
	      printf(_("mount: %s does not contain SELinux labels.\n"
                   "       You just mounted an file system that supports labels which does not\n"
                   "       contain labels, onto an SELinux box. It is likely that confined\n"
                   "       applications will generate AVC messages and not be allowed access to\n"
                   "       this file system.  For more details see restorecon(8) and mount(8).\n"),
                   node);
      }
      freecon(raw);
      freecon(def);
  }
#endif

  my_free(mount_opts);
  my_free(extra_opts);
  my_free(spec1);
  my_free(node1);
  my_free(opts1);
  my_free(types1);

  return res;
}

static char *
subst_string(const char *s, const char *sub, int sublen, const char *repl) {
	char *n;

	n = (char *) xmalloc(strlen(s)-sublen+strlen(repl)+1);
	strncpy (n, s, sub-s);
	strcpy (n + (sub-s), repl);
	strcat (n, sub+sublen);
	return n;
}

static char *
usersubst(const char *opts) {
	char *s, *w;
	char id[40];

	if (!opts)
		return NULL;

	s = "uid=useruid";
	if (opts && (w = strstr(opts, s)) != NULL) {
		sprintf(id, "uid=%u", getuid());
		opts = subst_string(opts, w, strlen(s), id);
	}
	s = "gid=usergid";
	if (opts && (w = strstr(opts, s)) != NULL) {
		sprintf(id, "gid=%u", getgid());
		opts = subst_string(opts, w, strlen(s), id);
	}
	return xstrdup(opts);
}

static int
is_existing_file (const char *s) {
	struct stat statbuf;

	return (stat(s, &statbuf) == 0);
}

/*
 * Return 0 for success (either mounted sth or -a and NOAUTO was given)
 */
static int
mount_one (const char *spec, const char *node, const char *types,
	   const char *fstabopts, char *cmdlineopts, int freq, int pass) {
	const char *nspec = NULL;
	char *opts;

	/* Substitute values in opts, if required */
	opts = usersubst(fstabopts);

	/* Merge the fstab and command line options.  */
	opts = append_opt(opts, cmdlineopts, NULL);

	if (types == NULL && !mounttype && !is_existing_file(spec)) {
		if (strchr (spec, ':') != NULL) {
			types = "nfs";
			if (verbose)
				printf(_("mount: no type was given - "
					 "I'll assume nfs because of "
					 "the colon\n"));
		} else if(!strncmp(spec, "//", 2)) {
			types = "cifs";
			if (verbose)
				printf(_("mount: no type was given - "
					 "I'll assume cifs because of "
					 "the // prefix\n"));
		}
	}

	/* Handle possible LABEL= and UUID= forms of spec */
	if (types == NULL || (strncmp(types, "9p", 2) &&
			      strncmp(types, "nfs", 3) &&
			      strncmp(types, "cifs", 4) &&
			      strncmp(types, "smbfs", 5))) {
		if (!is_pseudo_fs(types))
			nspec = spec_to_devname(spec);
		if (nspec)
			spec = nspec;
	}

	return try_mount_one (spec, node, types, opts, freq, pass, 0);
}

#ifdef HAVE_LIBMOUNT_MOUNT
static struct libmnt_table *minfo;	/* parsed mountinfo file */

/* Check if an fsname/dir pair was already in the old mtab.  */
static int
mounted (const char *spec0, const char *node0, struct mntentchn *fstab_mc) {
#else
static int
mounted (const char *spec0, const char *node0,
	 struct mntentchn *fstab_mc __attribute__((__unused__))) {
#endif
	struct mntentchn *mc, *mc0;
	const char *spec, *node;
	int ret = 0;

#ifdef HAVE_LIBMOUNT_MOUNT
	/*
	 * Use libmount to check for already mounted bind mounts on systems
	 * without mtab.
	 */
	if (fstab_mc && fstab_mc->m.mnt_opts &&
	    mtab_is_a_symlink() && strstr(fstab_mc->m.mnt_opts, "bind")) {

		struct libmnt_fs *fs = mnt_new_fs();
		int rc = fs ? 0 : -1;

		if (!rc)
			rc = mnt_fs_set_fstype(fs, fstab_mc->m.mnt_type);
		if (!rc)
			rc = mnt_fs_set_source(fs, fstab_mc->m.mnt_fsname);
		if (!rc)
			rc = mnt_fs_set_target(fs, fstab_mc->m.mnt_dir);
		if (!rc)
			rc =  mnt_fs_set_options(fs, fstab_mc->m.mnt_opts);
		if (!rc && !minfo)
			 minfo = mnt_new_table_from_file("/proc/self/mountinfo");
		if (!rc && minfo)
			rc = mnt_table_is_fs_mounted(minfo, fs);

		mnt_free_fs(fs);
		if (rc == 1)
			return 1;
	}
#endif
	/* Handle possible UUID= and LABEL= in spec */
	spec = spec_to_devname(spec0);
	if (!spec)
		return ret;

	node = canonicalize(node0);


	mc0 = mtab_head();
	for (mc = mc0->nxt; mc && mc != mc0; mc = mc->nxt)
		if (streq (spec, mc->m.mnt_fsname) &&
		    streq (node, mc->m.mnt_dir)) {
			ret = 1;
			break;
		}

	my_free(spec);
	my_free(node);

	return ret;
}

/* returns 0 if not mounted, 1 if mounted and -1 in case of error */
static int
is_fstab_entry_mounted(struct mntentchn *mc, int verbose)
{
	struct stat st;

	if (mounted(mc->m.mnt_fsname, mc->m.mnt_dir, mc))
		goto yes;

	/* extra care for loop devices */
	if ((mc->m.mnt_opts && strstr(mc->m.mnt_opts, "loop=")) ||
	    (stat(mc->m.mnt_fsname, &st) == 0 && S_ISREG(st.st_mode))) {

		char *p = get_option_value(mc->m.mnt_opts, "offset=");
		uintmax_t offset = 0;

		if (p && strtosize(p, &offset) != 0) {
			if (verbose)
				printf(_("mount: ignore %s "
					"(unparsable offset= option)\n"),
					mc->m.mnt_fsname);
			return -1;
		}
		free(p);
		if (is_mounted_same_loopfile(mc->m.mnt_dir, mc->m.mnt_fsname, offset))
			goto yes;
	}

	return 0;
yes:
	if (verbose)
		printf(_("mount: %s already mounted on %s\n"),
			       mc->m.mnt_fsname, mc->m.mnt_dir);
	return 1;
}

/* avoid using stat() on things we are not going to mount anyway.. */
static int
has_noauto (const char *opts) {
	char *s;

	if (!opts)
		return 0;
	s = strstr(opts, "noauto");
	if (!s)
		return 0;
	return (s == opts || s[-1] == ',') && (s[6] == 0 || s[6] == ',');
}

/* Mount all filesystems of the specified types except swap and root.  */
/* With the --fork option: fork and let different incarnations of
   mount handle different filesystems.  However, try to avoid several
   simultaneous mounts on the same physical disk, since that is very slow. */
#define DISKMAJOR(m)	(((int) m) & ~0xf)

static int
do_mount_all (char *types, char *options, char *test_opts) {
	struct mntentchn *mc, *mc0, *mtmp;
	int status = 0;
	struct stat statbuf;
	struct child {
		pid_t pid;
		char *group;
		struct mntentchn *mec;
		struct mntentchn *meclast;
		struct child *nxt;
	} childhead, *childtail, *cp;
	char major[22];
	char *g, *colon;

	/* build a chain of what we have to do, or maybe
	   several chains, one for each major or NFS host */
	childhead.nxt = 0;
	childtail = &childhead;
	mc0 = fstab_head();
	for (mc = mc0->nxt; mc && mc != mc0; mc = mc->nxt) {
		if (has_noauto (mc->m.mnt_opts))
			continue;
		if (matching_type (mc->m.mnt_type, types)
		    && matching_opts (mc->m.mnt_opts, test_opts)
		    && !streq (mc->m.mnt_dir, "/")
		    && !streq (mc->m.mnt_dir, "root")
		    && !is_fstab_entry_mounted(mc, verbose)) {

			mtmp = (struct mntentchn *) xmalloc(sizeof(*mtmp));
			*mtmp = *mc;
			mtmp->nxt = 0;
			g = NULL;
			if (optfork) {
				if (stat(mc->m.mnt_fsname, &statbuf) == 0 &&
				    S_ISBLK(statbuf.st_mode)) {
					sprintf(major, "#%x",
						DISKMAJOR(statbuf.st_rdev));
					g = major;
				}
				if (strcmp(mc->m.mnt_type, "nfs") == 0) {
					g = xstrdup(mc->m.mnt_fsname);
					colon = strchr(g, ':');
					if (colon)
						*colon = '\0';
				}
			}
			if (g) {
				for (cp = childhead.nxt; cp; cp = cp->nxt)
					if (cp->group &&
					    strcmp(cp->group, g) == 0) {
						cp->meclast->nxt = mtmp;
						cp->meclast = mtmp;
						goto fnd;
					}
			}
			cp = (struct child *) xmalloc(sizeof *cp);
			cp->nxt = 0;
			cp->mec = cp->meclast = mtmp;
			cp->group = xstrdup(g);
			cp->pid = 0;
			childtail->nxt = cp;
			childtail = cp;
		fnd:;

		}
	}
			      
	/* now do everything */
	for (cp = childhead.nxt; cp; cp = cp->nxt) {
		pid_t p = -1;
		if (optfork) {
			p = fork();
			if (p == -1) {
				int errsv = errno;
				error(_("mount: cannot fork: %s"),
				      strerror (errsv));
			}
			else if (p != 0)
				cp->pid = p;
		}

		/* if child, or not forked, do the mounting */
		if (p == 0 || p == -1) {
			for (mc = cp->mec; mc; mc = mc->nxt) {
				status |= mount_one (mc->m.mnt_fsname,
						     mc->m.mnt_dir,
						     mc->m.mnt_type,
						     mc->m.mnt_opts,
						     options, 0, 0);
			}
			if (mountcount)
				status |= EX_SOMEOK;
			if (p == 0)
				exit(status);
		}
	}

	/* wait for children, if any */
	while ((cp = childhead.nxt) != NULL) {
		childhead.nxt = cp->nxt;
		if (cp->pid) {
			int ret;
		keep_waiting:
			if(waitpid(cp->pid, &ret, 0) == -1) {
				if (errno == EINTR)
					goto keep_waiting;
				perror("waitpid");
			} else if (WIFEXITED(ret))
				status |= WEXITSTATUS(ret);
			else
				status |= EX_SYSERR;
		}
	}
	if (mountcount)
		status |= EX_SOMEOK;
	return status;
}

static struct option longopts[] = {
	{ "all", 0, 0, 'a' },
	{ "fake", 0, 0, 'f' },
	{ "fork", 0, 0, 'F' },
	{ "help", 0, 0, 'h' },
	{ "no-mtab", 0, 0, 'n' },
	{ "read-only", 0, 0, 'r' },
	{ "ro", 0, 0, 'r' },
	{ "verbose", 0, 0, 'v' },
	{ "version", 0, 0, 'V' },
	{ "read-write", 0, 0, 'w' },
	{ "rw", 0, 0, 'w' },
	{ "options", 1, 0, 'o' },
	{ "test-opts", 1, 0, 'O' },
	{ "pass-fd", 1, 0, 'p' },
	{ "types", 1, 0, 't' },
	{ "bind", 0, 0, 'B' },
	{ "move", 0, 0, 'M' },
	{ "guess-fstype", 1, 0, 134 },
	{ "rbind", 0, 0, 'R' },
	{ "make-shared", 0, 0, 136 },
	{ "make-slave", 0, 0, 137 },
	{ "make-private", 0, 0, 138 },
	{ "make-unbindable", 0, 0, 139 },
	{ "make-rshared", 0, 0, 140 },
	{ "make-rslave", 0, 0, 141 },
	{ "make-rprivate", 0, 0, 142 },
	{ "make-runbindable", 0, 0, 143 },
	{ "no-canonicalize", 0, 0, 144 },
	{ "internal-only", 0, 0, 'i' },
	{ NULL, 0, 0, 0 }
};

/* Keep the usage message at max 22 lines, each at most 70 chars long.
   The user should not need a pager to read it. */
static void
usage (FILE *fp, int n) {
	fprintf(fp, _(
	  "Usage: mount -V                 : print version\n"
	  "       mount -h                 : print this help\n"
	  "       mount                    : list mounted filesystems\n"
	  "       mount -l                 : idem, including volume labels\n"
	  "So far the informational part. Next the mounting.\n"
	  "The command is `mount [-t fstype] something somewhere'.\n"
	  "Details found in /etc/fstab may be omitted.\n"
	  "       mount -a [-t|-O] ...     : mount all stuff from /etc/fstab\n"
	  "       mount device             : mount device at the known place\n"
	  "       mount directory          : mount known device here\n"
	  "       mount -t type dev dir    : ordinary mount command\n"
	  "Note that one does not really mount a device, one mounts\n"
	  "a filesystem (of the given type) found on the device.\n"
	  "One can also mount an already visible directory tree elsewhere:\n"
	  "       mount --bind olddir newdir\n"
	  "or move a subtree:\n"
	  "       mount --move olddir newdir\n"
	  "One can change the type of mount containing the directory dir:\n"
	  "       mount --make-shared dir\n"
	  "       mount --make-slave dir\n"
	  "       mount --make-private dir\n"
	  "       mount --make-unbindable dir\n"
	  "One can change the type of all the mounts in a mount subtree\n"
	  "containing the directory dir:\n"
	  "       mount --make-rshared dir\n"
	  "       mount --make-rslave dir\n"
	  "       mount --make-rprivate dir\n"
	  "       mount --make-runbindable dir\n"
	  "A device can be given by name, say /dev/hda1 or /dev/cdrom,\n"
	  "or by label, using  -L label  or by uuid, using  -U uuid .\n"
	  "Other options: [-nfFrsvw] [-o options] [-p passwdfd].\n"
	  "For many more details, say  man 8 mount .\n"
	));

	unlock_mtab();
	exit (n);
}

/* returns mount entry from fstab */
static struct mntentchn *
getfs(const char *spec, const char *uuid, const char *label)
{
	struct mntentchn *mc = NULL;
	const char *devname = NULL;

	if (!spec && !uuid && !label)
		return NULL;

	/*
	 * A) 99% of all cases, the spec on cmdline matches
	 *    with spec in fstab
	 */
	if (uuid)
		mc = getfs_by_uuid(uuid);
	else if (label)
		mc = getfs_by_label(label);
	else {
		mc = getfs_by_dir(spec);

		if (!mc)
			mc = getfs_by_spec(spec);
	}
	if (mc)
		return mc;

	/*
	 * B) UUID or LABEL on cmdline, but devname in fstab
	 */
	if (uuid)
		devname = fsprobe_get_devname_by_uuid(uuid);
	else if (label)
		devname = fsprobe_get_devname_by_label(label);
	else
		devname = spec_to_devname(spec);

	if (devname)
		mc = getfs_by_devname(devname);

	/*
	 * C) mixed
	 */
	if (!mc && devname) {
		const char *id = NULL;

		if (!label && (!spec || strncmp(spec, "LABEL=", 6))) {
			id = fsprobe_get_label_by_devname(devname);
			if (id)
				mc = getfs_by_label(id);
		}
		if (!mc && !uuid && (!spec || strncmp(spec, "UUID=", 5))) {
			id = fsprobe_get_uuid_by_devname(devname);
			if (id)
				mc = getfs_by_uuid(id);
		}
		my_free(id);

		if (mc) {
			/* use real device name to avoid repetitional
			 * conversion from LABEL/UUID to devname
			 */
			my_free(mc->m.mnt_fsname);
			mc->m.mnt_fsname = xstrdup(devname);
		}
	}

	/*
	 * D) remount -- try /etc/mtab
	 *    Earlier mtab was tried first, but this would sometimes try the
	 *    wrong mount in case mtab had the root device entry wrong.  Try
	 *    the last occurrence first, since that is the visible mount.
	 */
	if (!mc && (devname || spec))
		mc = getmntfilebackward (devname ? devname : spec, NULL);

	my_free(devname);
	return mc;
}


static void
print_version(int rc) {
	printf(	"mount from %s (with "
#ifdef HAVE_LIBBLKID
		"libblkid"
#else
		"libvolume_id"
#endif
#ifdef HAVE_LIBSELINUX
		" and selinux"
#endif
		" support)\n", PACKAGE_STRING);
	exit(rc);
}

int
main(int argc, char *argv[]) {
	int c, result = 0, specseen;
	char *options = NULL, *test_opts = NULL, *node;
	const char *spec = NULL;
	char *label = NULL;
	char *uuid = NULL;
	char *types = NULL;
	char *p;
	struct mntentchn *mc;
	int fd;

	sanitize_env();
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	progname = argv[0];
	if ((p = strrchr(progname, '/')) != NULL)
		progname = p+1;

	umask(022);

	/* People report that a mount called from init without console
	   writes error messages to /etc/mtab
	   Let us try to avoid getting fd's 0,1,2 */
	while((fd = open("/dev/null", O_RDWR)) == 0 || fd == 1 || fd == 2) ;
	if (fd > 2)
		close(fd);

	fsprobe_init();

#ifdef DO_PS_FIDDLING
	initproctitle(argc, argv);
#endif

	while ((c = getopt_long (argc, argv, "aBfFhilL:Mno:O:p:rRsU:vVwt:",
				 longopts, NULL)) != -1) {
		switch (c) {
		case 'a':	       /* mount everything in fstab */
			++mount_all;
			break;
		case 'B': /* bind */
			mounttype = MS_BIND;
			break;
		case 'f':	       /* fake: don't actually call mount(2) */
			++fake;
			break;
		case 'F':
			++optfork;
			break;
		case 'h':		/* help */
			usage (stdout, 0);
			break;
		case 'i':
			external_allowed = 0;
			break;
		case 'l':
			list_with_volumelabel = 1;
			break;
		case 'L':
			label = optarg;
			break;
		case 'M': /* move */
			mounttype = MS_MOVE;
			break;
		case 'n':		/* do not write /etc/mtab */
			++nomtab;
			break;
		case 'o':		/* specify mount options */
			options = append_opt(options, optarg, NULL);
			break;
		case 'O':		/* with -t: mount only if (not) opt */
			test_opts = append_opt(test_opts, optarg, NULL);
			break;
		case 'p':		/* fd on which to read passwd */
			set_pfd(optarg);
			break;
		case 'r':		/* mount readonly */
			readonly = 1;
			readwrite = 0;
			break;
		case 'R': /* rbind */
			mounttype = (MS_BIND | MS_REC);
			break;
		case 's':		/* allow sloppy mount options */
			sloppy = 1;
			break;
		case 't':		/* specify file system types */
			types = optarg;
			break;
		case 'U':
			uuid = optarg;
			break;
		case 'v':		/* be chatty - more so if repeated */
			++verbose;
			break;
		case 'V':		/* version */
			print_version(EXIT_SUCCESS);
			break;
		case 'w':		/* mount read/write */
			readwrite = 1;
			readonly = 0;
			break;
		case 0:
			break;

		case 134:
			/* undocumented, may go away again:
			   call: mount --guess-fstype device
			   use only for testing purposes -
			   the guessing is not reliable at all */
		    {
			const char *fstype;
			fstype = fsprobe_get_fstype_by_devname(optarg);
			printf("%s\n", fstype ? fstype : "unknown");
			exit(fstype ? 0 : EX_FAIL);
		    }

		case 136:
			mounttype = MS_SHARED;
			break;

		case 137:
			mounttype = MS_SLAVE;
			break;

		case 138:
			mounttype = MS_PRIVATE;
			break;

		case 139:
			mounttype = MS_UNBINDABLE;
			break;

		case 140:
			mounttype = (MS_SHARED | MS_REC);
			break;

		case 141:
			mounttype = (MS_SLAVE | MS_REC);
			break;

		case 142:
			mounttype = (MS_PRIVATE | MS_REC);
			break;

		case 143:
			mounttype = (MS_UNBINDABLE | MS_REC);
			break;
		case 144:
			nocanonicalize = 1;
			break;
		case '?':
		default:
			usage (stderr, EX_USAGE);
		}
	}

	if (verbose > 2) {
		printf("mount: fstab path: \"%s\"\n", _PATH_MNTTAB);
		printf("mount: mtab path:  \"%s\"\n", _PATH_MOUNTED);
		printf("mount: lock path:  \"%s\"\n", _PATH_MOUNTED_LOCK);
		printf("mount: temp path:  \"%s\"\n", _PATH_MOUNTED_TMP);
		printf("mount: UID:        %u\n", getuid());
		printf("mount: eUID:       %u\n", geteuid());
	}

#ifdef HAVE_LIBMOUNT_MOUNT
	mnt_init_debug(0);
#endif
	argc -= optind;
	argv += optind;

	specseen = (uuid || label) ? 1 : 0;	/* yes, .. i know */

	if (argc+specseen == 0 && !mount_all) {
		if (options || mounttype)
			usage (stderr, EX_USAGE);
		return print_all (types);
	}

	{
		const uid_t ruid = getuid();
		const uid_t euid = geteuid();

		/* if we're really root and aren't running setuid */
		if (((uid_t)0 == ruid) && (ruid == euid)) {
			restricted = 0;
		}

		if (restricted &&
		    (types || options || readwrite || nomtab || mount_all ||
		     nocanonicalize || fake || mounttype ||
		     (argc + specseen) != 1)) {

			if (ruid == 0 && euid != 0)
				/* user is root, but setuid to non-root */
				die (EX_USAGE, _("mount: only root can do that "
					"(effective UID is %u)"), euid);

			die (EX_USAGE, _("mount: only root can do that"));
		}
	}

	atexit(unlock_mtab);

	switch (argc+specseen) {
	case 0:
		/* mount -a */
		result = do_mount_all (types, options, test_opts);
		if (result == 0 && verbose && !fake)
			error(_("nothing was mounted"));
		break;

	case 1:
		/* mount [-nfrvw] [-o options] special | node
		 * mount -L label  (or -U uuid)
		 * (/etc/fstab is necessary)
		 */
		if (types != NULL)
			usage (stderr, EX_USAGE);

		if (uuid || label)
			mc = getfs(NULL, uuid, label);
		else
			mc = getfs(*argv, NULL, NULL);

		if (!mc) {
			if (uuid || label)
				die (EX_USAGE, _("mount: no such partition found"));

			die (EX_USAGE,
			     _("mount: can't find %s in %s or %s"),
			     *argv, _PATH_MNTTAB, _PATH_MOUNTED);
		}

		result = mount_one (xstrdup (mc->m.mnt_fsname),
				    xstrdup (mc->m.mnt_dir),
				    xstrdup (mc->m.mnt_type),
				    mc->m.mnt_opts, options, 0, 0);
		break;

	case 2:
		/* mount special node  (/etc/fstab is not necessary) */
		if (specseen) {
			/* mount -L label node   (or -U uuid) */
			spec = uuid ?	fsprobe_get_devname_by_uuid(uuid) :
					fsprobe_get_devname_by_label(label);
			node = argv[0];
		} else {
			/* mount special node */
			spec = argv[0];
			node = argv[1];
		}
		if (!spec)
			die (EX_USAGE, _("mount: no such partition found"));

		result = mount_one (spec, node, types, NULL, options, 0, 0);
		break;

	default:
		usage (stderr, EX_USAGE);
	}

	if (result == EX_SOMEOK)
		result = 0;

	fsprobe_exit();

	exit (result);
}

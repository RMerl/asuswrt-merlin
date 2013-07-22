/*
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: optmap
 * @title: Option maps
 * @short_description: description for mount options
 *
 * The mount(2) linux syscall uses two arguments for mount options:
 *
 *	@mountflags: (see MS_* macros in linux/fs.h)
 *
 *	@mountdata: (usully a comma separated string of options)
 *
 * The libmount uses options-map(s) to describe mount options.
 *
 * The option description (map entry) includes:
 *
 *	@name: and argument name
 *
 *	@id: (in the map unique identifier or a mountflags, e.g MS_RDONLY)
 *
 *	@mask: (MNT_INVERT, MNT_NOMTAB)
 *
 * The option argument value is defined by:
 *
 *	"="   -- required argument, e.g "comment="
 *
 *	"[=]" -- optional argument, e.g. "loop[=]"
 *
 * Example:
 *
 * <informalexample>
 *   <programlisting>
 *     #define MY_MS_FOO   (1 << 1)
 *     #define MY_MS_BAR   (1 << 2)
 *
 *     libmnt_optmap myoptions[] = {
 *       { "foo",   MY_MS_FOO },
 *       { "nofoo", MY_MS_FOO | MNT_INVERT },
 *       { "bar=",  MY_MS_BAR },
 *       { NULL }
 *     };
 *   </programlisting>
 * </informalexample>
 *
 * The libmount defines two basic built-in options maps:
 *
 *	@MNT_LINUX_MAP: fs-independent kernel mount options (usually MS_* flags)
 *
 *	@MNT_USERSPACE_MAP: userspace specific mount options (e.g. "user", "loop")
 *
 * For more details about option map struct see "struct mnt_optmap" in
 * mount/mount.h.
 */
#include "mountP.h"

/*
 * fs-independent mount flags (built-in MNT_LINUX_MAP)
 */
static const struct libmnt_optmap linux_flags_map[] =
{
   { "ro",       MS_RDONLY },                 /* read-only */
   { "rw",       MS_RDONLY, MNT_INVERT },     /* read-write */
   { "exec",     MS_NOEXEC, MNT_INVERT },     /* permit execution of binaries */
   { "noexec",   MS_NOEXEC },                 /* don't execute binaries */
   { "suid",     MS_NOSUID, MNT_INVERT },     /* honor suid executables */
   { "nosuid",   MS_NOSUID },                 /* don't honor suid executables */
   { "dev",      MS_NODEV, MNT_INVERT },      /* interpret device files  */
   { "nodev",    MS_NODEV },                  /* don't interpret devices */

   { "sync",     MS_SYNCHRONOUS },            /* synchronous I/O */
   { "async",    MS_SYNCHRONOUS, MNT_INVERT },/* asynchronous I/O */

   { "dirsync",  MS_DIRSYNC },                /* synchronous directory modifications */
   { "remount",  MS_REMOUNT, MNT_NOMTAB },    /* alter flags of mounted FS */
   { "bind",     MS_BIND },                   /* Remount part of tree elsewhere */
   { "rbind",    MS_BIND | MS_REC },          /* Idem, plus mounted subtrees */
#ifdef MS_NOSUB
   { "sub",      MS_NOSUB, MNT_INVERT },      /* allow submounts */
   { "nosub",    MS_NOSUB },                  /* don't allow submounts */
#endif
#ifdef MS_SILENT
   { "silent",	 MS_SILENT },                 /* be quiet  */
   { "loud",     MS_SILENT, MNT_INVERT },     /* print out messages. */
#endif
#ifdef MS_MANDLOCK
   { "mand",     MS_MANDLOCK },               /* Allow mandatory locks on this FS */
   { "nomand",   MS_MANDLOCK, MNT_INVERT },   /* Forbid mandatory locks on this FS */
#endif
#ifdef MS_NOATIME
   { "atime",    MS_NOATIME, MNT_INVERT },    /* Update access time */
   { "noatime",	 MS_NOATIME },                /* Do not update access time */
#endif
#ifdef MS_I_VERSION
   { "iversion", MS_I_VERSION },              /* Update inode I_version time */
   { "noiversion", MS_I_VERSION,  MNT_INVERT},/* Don't update inode I_version time */
#endif
#ifdef MS_NODIRATIME
   { "diratime", MS_NODIRATIME, MNT_INVERT }, /* Update dir access times */
   { "nodiratime", MS_NODIRATIME },           /* Do not update dir access times */
#endif
#ifdef MS_RELATIME
   { "relatime", MS_RELATIME },               /* Update access times relative to mtime/ctime */
   { "norelatime", MS_RELATIME, MNT_INVERT }, /* Update access time without regard to mtime/ctime */
#endif
#ifdef MS_STRICTATIME
   { "strictatime", MS_STRICTATIME },         /* Strict atime semantics */
   { "nostrictatime", MS_STRICTATIME, MNT_INVERT }, /* kernel default atime */
#endif
   { NULL, 0, 0 }
};

/*
 * userspace mount option (built-in MNT_USERSPACE_MAP)
 *
 * TODO: offset=, sizelimit=, encryption=, vfs=
 */
static const struct libmnt_optmap userspace_opts_map[] =
{
   { "defaults", 0, 0 },               /* default options */

   { "auto",    MNT_MS_NOAUTO, MNT_INVERT | MNT_NOMTAB },  /* Can be mounted using -a */
   { "noauto",  MNT_MS_NOAUTO, MNT_NOMTAB },               /* Can  only be mounted explicitly */

   { "user[=]", MNT_MS_USER },                             /* Allow ordinary user to mount (mtab) */
   { "nouser",  MNT_MS_USER, MNT_INVERT | MNT_NOMTAB },    /* Forbid ordinary user to mount */

   { "users",   MNT_MS_USERS, MNT_NOMTAB },                /* Allow ordinary users to mount */
   { "nousers", MNT_MS_USERS, MNT_INVERT | MNT_NOMTAB },   /* Forbid ordinary users to mount */

   { "owner",   MNT_MS_OWNER, MNT_NOMTAB },                /* Let the owner of the device mount */
   { "noowner", MNT_MS_OWNER, MNT_INVERT | MNT_NOMTAB },   /* Device owner has no special privs */

   { "group",   MNT_MS_GROUP, MNT_NOMTAB },                /* Let the group of the device mount */
   { "nogroup", MNT_MS_GROUP, MNT_INVERT | MNT_NOMTAB },   /* Device group has no special privs */

   { "_netdev", MNT_MS_NETDEV },                           /* Device requires network */

   { "comment=", MNT_MS_COMMENT, MNT_NOMTAB },             /* fstab comment only */
   { "x-",      MNT_MS_XCOMMENT, MNT_NOMTAB | MNT_PREFIX }, /* x- options */

   { "loop[=]", MNT_MS_LOOP },                             /* use the loop device */
   { "offset=", MNT_MS_OFFSET, MNT_NOMTAB },		   /* loop device offset */
   { "sizelimit=", MNT_MS_SIZELIMIT, MNT_NOMTAB },	   /* loop device size limit */

   { "nofail",  MNT_MS_NOFAIL, MNT_NOMTAB },               /* Do not fail if ENOENT on dev */

   { "uhelper=", MNT_MS_UHELPER },			   /* /sbin/umount.<helper> */

   { "helper=", MNT_MS_HELPER },			   /* /sbin/umount.<helper> */

   { NULL, 0, 0 }
};

/**
 * mnt_get_builtin_map:
 * @id: map id -- MNT_LINUX_MAP or MNT_USERSPACE_MAP
 *
 * MNT_LINUX_MAP - Linux kernel fs-independent mount options
 *                 (usually MS_* flags, see linux/fs.h)
 *
 * MNT_USERSPACE_MAP - userpace mount(8) specific mount options
 *                     (e.g user=, _netdev, ...)
 *
 * Returns: static built-in libmount map.
 */
const struct libmnt_optmap *mnt_get_builtin_optmap(int id)
{
	assert(id);

	if (id == MNT_LINUX_MAP)
		return linux_flags_map;
	else if (id == MNT_USERSPACE_MAP)
		return userspace_opts_map;
	return NULL;
}

/*
 * Lookups for the @name in @maps and returns a map and in @mapent
 * returns the map entry
 */
const struct libmnt_optmap *mnt_optmap_get_entry(
				struct libmnt_optmap const **maps,
				int nmaps,
				const char *name,
				size_t namelen,
				const struct libmnt_optmap **mapent)
{
	int i;

	assert(maps);
	assert(nmaps);
	assert(name);
	assert(namelen);

	if (mapent)
		*mapent = NULL;

	for (i = 0; i < nmaps; i++) {
		const struct libmnt_optmap *map = maps[i];
		const struct libmnt_optmap *ent;
		const char *p;

		for (ent = map; ent && ent->name; ent++) {
			if (ent->mask & MNT_PREFIX) {
				if (startswith(name, ent->name)) {
					if (mapent)
						*mapent = ent;
					return map;
				}
				continue;
			}
			if (strncmp(ent->name, name, namelen))
				continue;
			p = ent->name + namelen;
			if (*p == '\0' || *p == '=' || *p == '[') {
				if (mapent)
					*mapent = ent;
				return map;
			}
		}
	}
	return NULL;
}


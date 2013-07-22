/*
 * lsblk(8) - list block devices
 *
 * Copyright (C) 2010,2011 Red Hat, Inc. All rights reserved.
 * Written by Milan Broz <mbroz@redhat.com>
 *            Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <locale.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>

#include <blkid.h>

#include <assert.h>

#include "pathnames.h"
#include "blkdev.h"
#include "canonicalize.h"
#include "ismounted.h"
#include "nls.h"
#include "tt.h"
#include "xalloc.h"
#include "strutils.h"
#include "c.h"
#include "sysfs.h"

/* column IDs */
enum {
	COL_NAME = 0,
	COL_KNAME,
	COL_MAJMIN,
	COL_FSTYPE,
	COL_TARGET,
	COL_LABEL,
	COL_UUID,
	COL_RO,
	COL_RM,
	COL_MODEL,
	COL_SIZE,
	COL_STATE,
	COL_OWNER,
	COL_GROUP,
	COL_MODE,
	COL_ALIOFF,
	COL_MINIO,
	COL_OPTIO,
	COL_PHYSEC,
	COL_LOGSEC,
	COL_ROTA,
	COL_SCHED,
	COL_RQ_SIZE,
	COL_TYPE,
	COL_DALIGN,
	COL_DGRAN,
	COL_DMAX,
	COL_DZERO,

	__NCOLUMNS
};

/* column names */
struct colinfo {
	const char	*name;		/* header */
	double		whint;		/* width hint (N < 1 is in percent of termwidth) */
	int		flags;		/* TT_FL_* */
	const char      *help;
};

/* columns descriptions */
static struct colinfo infos[__NCOLUMNS] = {
	[COL_NAME]   = { "NAME",    0.25, TT_FL_TREE, N_("device name") },
	[COL_KNAME]  = { "KNAME",   0.3, 0, N_("internal kernel device name") },
	[COL_MAJMIN] = { "MAJ:MIN", 6, 0, N_("major:minor device number") },
	[COL_FSTYPE] = { "FSTYPE",  0.1, TT_FL_TRUNC, N_("filesystem type") },
	[COL_TARGET] = { "MOUNTPOINT", 0.10, TT_FL_TRUNC, N_("where the device is mounted") },
	[COL_LABEL]  = { "LABEL",   0.1, 0, N_("filesystem LABEL") },
	[COL_UUID]   = { "UUID",    36,  0, N_("filesystem UUID") },
	[COL_RO]     = { "RO",      1, TT_FL_RIGHT, N_("read-only device") },
	[COL_RM]     = { "RM",      1, TT_FL_RIGHT, N_("removable device") },
	[COL_ROTA]   = { "ROTA",    1, TT_FL_RIGHT, N_("rotational device") },
	[COL_MODEL]  = { "MODEL",   0.1, TT_FL_TRUNC, N_("device identifier") },
	[COL_SIZE]   = { "SIZE",    6, TT_FL_RIGHT, N_("size of the device") },
	[COL_STATE]  = { "STATE",   7, TT_FL_TRUNC, N_("state of the device") },
	[COL_OWNER]  = { "OWNER",   0.1, TT_FL_TRUNC, N_("user name"), },
	[COL_GROUP]  = { "GROUP",   0.1, TT_FL_TRUNC, N_("group name") },
	[COL_MODE]   = { "MODE",    10,   0, N_("device node permissions") },
	[COL_ALIOFF] = { "ALIGNMENT", 6, TT_FL_RIGHT, N_("alignment offset") },
	[COL_MINIO]  = { "MIN-IO",  6, TT_FL_RIGHT, N_("minimum I/O size") },
	[COL_OPTIO]  = { "OPT-IO",  6, TT_FL_RIGHT, N_("optimal I/O size") },
	[COL_PHYSEC] = { "PHY-SEC", 7, TT_FL_RIGHT, N_("physical sector size") },
	[COL_LOGSEC] = { "LOG-SEC", 7, TT_FL_RIGHT, N_("logical sector size") },
	[COL_SCHED]  = { "SCHED",   0.1, 0, N_("I/O scheduler name") },
	[COL_RQ_SIZE]= { "RQ-SIZE", 5, TT_FL_RIGHT, N_("request queue size") },
	[COL_TYPE]   = { "TYPE",    4, 0, N_("device type") },
	[COL_DALIGN] = { "DISC-ALN", 6, TT_FL_RIGHT, N_("discard alignment offset") },
	[COL_DGRAN]  = { "DISC-GRAN", 6, TT_FL_RIGHT, N_("discard granularity") },
	[COL_DMAX]   = { "DISC-MAX", 6, TT_FL_RIGHT, N_("discard max bytes") },
	[COL_DZERO]  = { "DISC-ZERO", 1, TT_FL_RIGHT, N_("discard zeroes data") },
};

struct lsblk {
	struct tt *tt;			/* output table */
	unsigned int all_devices:1;	/* print all devices */
	unsigned int bytes:1;		/* print SIZE in bytes */
	unsigned int nodeps:1;		/* don't print slaves/holders */
};

struct lsblk *lsblk;	/* global handler */
int columns[__NCOLUMNS];/* enabled columns */
int ncolumns;		/* number of enabled columns */

int excludes[256];
size_t nexcludes;

struct blkdev_cxt {
	struct blkdev_cxt *parent;

	struct tt_line *tt_line;
	struct stat	st;

	char *name;		/* kernel name in /sys/block */
	char *dm_name;		/* DM name (dm/block) */

	char *filename;		/* path to device node */

	struct sysfs_cxt  sysfs;

	int partition;		/* is partition? TRUE/FALSE */

	int probed;		/* already probed */
	char *fstype;		/* detected fs, NULL or "?" if cannot detect */
	char *uuid;		/* UUID of device / filesystem */
	char *label;		/* FS label */

	int nholders;		/* # of devices mapped directly to this device
				 * /sys/block/.../holders + number of partition */
	int nslaves;		/* # of devices this device maps to */
	int maj, min;		/* devno */
	int discard;		/* supports discard */

	uint64_t size;		/* device size */
};

static int is_maj_excluded(int maj)
{
	size_t i;

	assert(ARRAY_SIZE(excludes) > nexcludes);

	for (i = 0; i < nexcludes; i++)
		if (excludes[i] == maj)
			return 1;
	return 0;
}

/* array with IDs of enabled columns */
static int get_column_id(int num)
{
	assert(ARRAY_SIZE(columns) == __NCOLUMNS);
	assert(num < ncolumns);
	assert(columns[num] < __NCOLUMNS);
	return columns[num];
}

static struct colinfo *get_column_info(int num)
{
	return &infos[ get_column_id(num) ];
}

static int column_name_to_id(const char *name, size_t namesz)
{
	int i;

	for (i = 0; i < __NCOLUMNS; i++) {
		const char *cn = infos[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static void reset_blkdev_cxt(struct blkdev_cxt *cxt)
{
	if (!cxt)
		return;
	free(cxt->name);
	free(cxt->dm_name);
	free(cxt->filename);
	free(cxt->fstype);
	free(cxt->uuid);
	free(cxt->label);

	sysfs_deinit(&cxt->sysfs);

	memset(cxt, 0, sizeof(*cxt));
}

static int is_dm(const char *name)
{
	return strncmp(name, "dm-", 3) ? 0 : 1;
}

static struct dirent *xreaddir(DIR *dp)
{
	struct dirent *d;

	assert(dp);

	while ((d = readdir(dp))) {
		if (!strcmp(d->d_name, ".") ||
		    !strcmp(d->d_name, ".."))
			continue;

		/* blacklist here? */
		break;
	}
	return d;
}

static char *get_device_path(struct blkdev_cxt *cxt)
{
	char path[PATH_MAX];

	assert(cxt);
	assert(cxt->name);

	if (is_dm(cxt->name))
		return canonicalize_dm_name(cxt->name);

	snprintf(path, sizeof(path), "/dev/%s", cxt->name);
	return xstrdup(path);
}

static char *get_device_mountpoint(struct blkdev_cxt *cxt)
{
	int fl = 0;
	char mnt[PATH_MAX];

	assert(cxt);
	assert(cxt->filename);

	*mnt = '\0';

	/*
	 * TODO: use libmount and parse /proc/mountinfo only once
	 */
	if (check_mount_point(cxt->filename, &fl, mnt, sizeof(mnt)) == 0 &&
	    (fl & MF_MOUNTED)) {
		if (fl & MF_SWAP)
			strcpy(mnt, "[SWAP]");
	}
	return strlen(mnt) ? xstrdup(mnt) : NULL;
}

/* TODO: read info from udev db (if possible) for non-root users
 */
static void probe_device(struct blkdev_cxt *cxt)
{
	char *path = NULL;
	blkid_probe pr = NULL;

	if (cxt->probed)
		return;
	cxt->probed = 1;

	if (!cxt->size)
		return;

	pr = blkid_new_probe_from_filename(cxt->filename);
	if (!pr)
		return;

	/* TODO: we have to enable partitions probing to avoid conflicts
	 *       between raids and PT -- see blkid(8) code for more details
	 */
	blkid_probe_enable_superblocks(pr, 1);
	blkid_probe_set_superblocks_flags(pr, BLKID_SUBLKS_LABEL |
					      BLKID_SUBLKS_UUID |
					      BLKID_SUBLKS_TYPE);
	if (!blkid_do_safeprobe(pr)) {
		const char *data = NULL;

		if (!blkid_probe_lookup_value(pr, "TYPE", &data, NULL))
			cxt->fstype = xstrdup(data);
		if (!blkid_probe_lookup_value(pr, "UUID", &data, NULL))
			cxt->uuid = xstrdup(data);
		if (!blkid_probe_lookup_value(pr, "LABEL", &data, NULL))
			cxt->label = xstrdup(data);
	}

	free(path);
	blkid_free_probe(pr);
	return;
}

static int is_readonly_device(struct blkdev_cxt *cxt)
{
	int fd, ro = 0;

	if (sysfs_scanf(&cxt->sysfs, "ro", "%d", &ro) == 0)
		return ro;

	/* fallback if "ro" attribute does not exist */
	fd = open(cxt->filename, O_RDONLY);
	if (fd != -1) {
		ioctl(fd, BLKROGET, &ro);
		close(fd);
	}
	return ro;
}

static char *get_scheduler(struct blkdev_cxt *cxt)
{
	char *str = sysfs_strdup(&cxt->sysfs, "queue/scheduler");
	char *p, *res = NULL;

	if (!str)
		return NULL;
	p = strchr(str, '[');
	if (p) {
		res = p + 1;
		p = strchr(res, ']');
		if (p) {
			*p = '\0';
			res = xstrdup(res);
		} else
			res = NULL;
	}
	free(str);
	return res;
}

static char *get_type(struct blkdev_cxt *cxt)
{
	char *res = NULL, *p;

	if (is_dm(cxt->name)) {
		char *dm_uuid = sysfs_strdup(&cxt->sysfs, "dm/uuid");

		/* The DM_UUID prefix should be set to subsystem owning
		 * the device - LVM, CRYPT, DMRAID, MPATH, PART */
		if (dm_uuid) {
			char *tmp = dm_uuid;
			char *dm_uuid_prefix = strsep(&tmp, "-");

			if (dm_uuid_prefix) {
				/* kpartx hack to remove partition number */
				if (strncasecmp(dm_uuid_prefix, "part", 4) == 0)
					dm_uuid_prefix[4] = '\0';

				res = xstrdup(dm_uuid_prefix);
			}
		}

		free(dm_uuid);
		if (!res)
			/* No UUID or no prefix - just mark it as DM device */
			res = xstrdup("dm");

	} else if (!strncmp(cxt->name, "loop", 4)) {
		res = xstrdup("loop");

	} else if (!strncmp(cxt->name, "md", 2)) {
		char *md_level = sysfs_strdup(&cxt->sysfs, "md/level");
		res = md_level ? md_level : xstrdup("md");

	} else {
		const char *type = cxt->partition ? "part" : "disk";
		int x = 0;

		sysfs_read_int(&cxt->sysfs, "device/type", &x);

		switch (x) {
			case 0x0c: /* TYPE_RAID */
				type = "raid"; break;
			case 0x01: /* TYPE_TAPE */
				type = "raid"; break;
			case 0x04: /* TYPE_WORM */
			case 0x05: /* TYPE_ROM */
				type = "rom"; break;
			case 0x07: /* TYPE_MOD */
				type = "mo-disk"; break;
			case 0x0e: /* TYPE_RBC */
				type = "rbc"; break;
		}

		res = xstrdup(type);
	}

	for (p = res; p && *p; p++)
		*p = tolower((unsigned char) *p);
	return res;
}

static void set_tt_data(struct blkdev_cxt *cxt, int col, int id, struct tt_line *ln)
{
	char buf[1024];
	char *p = NULL;

	if (!cxt->st.st_rdev && (id == COL_OWNER || id == COL_GROUP ||
				 id == COL_MODE))
		stat(cxt->filename, &cxt->st);

	switch(id) {
	case COL_NAME:
		if (cxt->dm_name) {
			snprintf(buf, sizeof(buf), "%s (%s)",
					cxt->dm_name, cxt->name);
			tt_line_set_data(ln, col, xstrdup(buf));
			break;
		}
	case COL_KNAME:
		tt_line_set_data(ln, col, xstrdup(cxt->name));
		break;
	case COL_OWNER:
	{
		struct passwd *pw = getpwuid(cxt->st.st_uid);
		if (pw)
			tt_line_set_data(ln, col, xstrdup(pw->pw_name));
		break;
	}
	case COL_GROUP:
	{
		struct group *gr = getgrgid(cxt->st.st_gid);
		if (gr)
			tt_line_set_data(ln, col, xstrdup(gr->gr_name));
		break;
	}
	case COL_MODE:
	{
		char md[11];
		strmode(cxt->st.st_mode, md);
		tt_line_set_data(ln, col, xstrdup(md));
		break;
	}
	case COL_MAJMIN:
		if ((lsblk->tt->flags & TT_FL_RAW) ||
		    (lsblk->tt->flags & TT_FL_EXPORT))
			snprintf(buf, sizeof(buf), "%u:%u", cxt->maj, cxt->min);
		else
			snprintf(buf, sizeof(buf), "%3u:%-3u", cxt->maj, cxt->min);
		tt_line_set_data(ln, col, xstrdup(buf));
		break;
	case COL_FSTYPE:
		probe_device(cxt);
		if (cxt->fstype)
			tt_line_set_data(ln, col, xstrdup(cxt->fstype));
		break;
	case COL_TARGET:
		if (!cxt->nholders) {
			p = get_device_mountpoint(cxt);
			if (p)
				tt_line_set_data(ln, col, p);
		}
		break;
	case COL_LABEL:
		probe_device(cxt);
		if (cxt->label)
			tt_line_set_data(ln, col, xstrdup(cxt->label));
		break;
	case COL_UUID:
		probe_device(cxt);
		if (cxt->uuid)
			tt_line_set_data(ln, col, xstrdup(cxt->uuid));
		break;
	case COL_RO:
		tt_line_set_data(ln, col, is_readonly_device(cxt) ?
					xstrdup("1") : xstrdup("0"));
		break;
	case COL_RM:
		p = sysfs_strdup(&cxt->sysfs, "removable");
		if (!p && cxt->parent)
			p = sysfs_strdup(&cxt->parent->sysfs, "removable");
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_ROTA:
		p = sysfs_strdup(&cxt->sysfs, "queue/rotational");
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_MODEL:
		if (!cxt->partition && cxt->nslaves == 0) {
			p = sysfs_strdup(&cxt->sysfs, "device/model");
			if (p)
				tt_line_set_data(ln, col, p);
		}
		break;
	case COL_SIZE:
		if (cxt->size) {
			if (lsblk->bytes) {
				if (asprintf(&p, "%jd", cxt->size) < 0)
					p = NULL;
			} else
				p = size_to_human_string(SIZE_SUFFIX_1LETTER, cxt->size);
			if (p)
				tt_line_set_data(ln, col, p);
		}
		break;
	case COL_STATE:
		if (!cxt->partition && !cxt->dm_name) {
			p = sysfs_strdup(&cxt->sysfs, "device/state");
		} else if (cxt->dm_name) {
			int x = 0;
			if (sysfs_read_int(&cxt->sysfs, "dm/suspended", &x) == 0)
				p = x ? xstrdup("suspended") : xstrdup("running");
		}
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_ALIOFF:
		p = sysfs_strdup(&cxt->sysfs, "alignment_offset");
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_MINIO:
		p = sysfs_strdup(&cxt->sysfs, "queue/minimum_io_size");
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_OPTIO:
		p = sysfs_strdup(&cxt->sysfs, "queue/optimal_io_size");
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_PHYSEC:
		p = sysfs_strdup(&cxt->sysfs, "queue/physical_block_size");
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_LOGSEC:
		p = sysfs_strdup(&cxt->sysfs, "queue/logical_block_size");
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_SCHED:
		p = get_scheduler(cxt);
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_RQ_SIZE:
		p = sysfs_strdup(&cxt->sysfs, "queue/nr_requests");
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_TYPE:
		p = get_type(cxt);
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_DALIGN:
		p = sysfs_strdup(&cxt->sysfs, "discard_alignment");
		if (cxt->discard && p)
			tt_line_set_data(ln, col, p);
		else
			tt_line_set_data(ln, col, "0");
		break;
	case COL_DGRAN:
		if (lsblk->bytes)
			p = sysfs_strdup(&cxt->sysfs, "queue/discard_granularity");
		else {
			uint64_t x;

			if (sysfs_read_u64(&cxt->sysfs,
					   "queue/discard_granularity", &x) == 0)
				p = size_to_human_string(SIZE_SUFFIX_1LETTER, x);
		}
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_DMAX:
		if (lsblk->bytes)
			p = sysfs_strdup(&cxt->sysfs, "queue/discard_max_bytes");
		else {
			uint64_t x;

			if (sysfs_read_u64(&cxt->sysfs,
					   "queue/discard_max_bytes", &x) == 0)
				p = size_to_human_string(SIZE_SUFFIX_1LETTER, x);
		}
		if (p)
			tt_line_set_data(ln, col, p);
		break;
	case COL_DZERO:
		p = sysfs_strdup(&cxt->sysfs, "queue/discard_zeroes_data");
		if (cxt->discard && p)
			tt_line_set_data(ln, col, p);
		else
			tt_line_set_data(ln, col, "0");
		break;
	};
}

static void print_device(struct blkdev_cxt *cxt, struct tt_line *tt_parent)
{
	int i;

	cxt->tt_line = tt_add_line(lsblk->tt, tt_parent);

	for (i = 0; i < ncolumns; i++)
		set_tt_data(cxt, i, get_column_id(i), cxt->tt_line);
}

static int set_cxt(struct blkdev_cxt *cxt,
		    struct blkdev_cxt *parent,
		    const char *name,
		    int partition)
{
	dev_t devno;

	cxt->parent = parent;
	cxt->name = xstrdup(name);
	cxt->partition = partition;

	cxt->filename = get_device_path(cxt);
	if (!cxt->filename) {
		warnx(_("%s: failed to get device path"), name);
		return -1;
	}

	devno = sysfs_devname_to_devno(name,
			partition && parent ? parent->name : NULL);
	if (!devno) {
		warnx(_("%s: unknown device name"), name);
		return -1;
	}

	if (sysfs_init(&cxt->sysfs, devno, parent ? &parent->sysfs : NULL)) {
		warnx(_("%s: failed to initialize sysfs handler"), name);
		return -1;
	}

	cxt->maj = major(devno);
	cxt->min = minor(devno);

	sysfs_read_u64(&cxt->sysfs, "size", &cxt->size);	/* in sectors */
	cxt->size <<= 9;					/* in bytes */

	sysfs_read_int(&cxt->sysfs, "queue/discard_granularity", &cxt->discard);

	/* Ignore devices of zero size */
	if (!lsblk->all_devices && cxt->size == 0)
		return -1;

	if (is_dm(name)) {
		cxt->dm_name = sysfs_strdup(&cxt->sysfs, "dm/name");
		if (!cxt->dm_name) {
			warnx(_("%s: failed to get dm name"), name);
			return -1;
		}
	}
	cxt->nholders = sysfs_count_dirents(&cxt->sysfs, "holders") +
			sysfs_count_partitions(&cxt->sysfs, name);

	cxt->nslaves = sysfs_count_dirents(&cxt->sysfs, "slaves");

	return 0;
}

/*
 * List devices (holders) mapped to device
 */
static int list_holders(struct blkdev_cxt *cxt)
{
	DIR *dir;
	struct dirent *d;
	struct blkdev_cxt holder = {};

	assert(cxt);

	if (lsblk->nodeps)
		return 0;

	if (!cxt->nholders)
		return 0;

	/* Partitions */
	dir = sysfs_opendir(&cxt->sysfs, NULL);
	if (!dir)
		err(EXIT_FAILURE, _("failed to open device directory in sysfs"));

	while ((d = xreaddir(dir))) {
		if (!sysfs_is_partition_dirent(dir, d, cxt->name))
			continue;

		if (set_cxt(&holder, cxt, d->d_name, 1)) {
			reset_blkdev_cxt(&holder);
			continue;
		}
		print_device(&holder, cxt->tt_line);
		list_holders(&holder);
		reset_blkdev_cxt(&holder);
	}
	closedir(dir);

	/* Holders */
	dir = sysfs_opendir(&cxt->sysfs, "holders");
	if (!dir)
		return 0;

	while ((d = xreaddir(dir))) {
		if (set_cxt(&holder, cxt, d->d_name, 0)) {
			reset_blkdev_cxt(&holder);
			continue;
		}
		print_device(&holder, cxt->tt_line);
		list_holders(&holder);
		reset_blkdev_cxt(&holder);
	}
	closedir(dir);

	return 0;
}

/* Iterate top-level devices in sysfs */
static int iterate_block_devices(void)
{
	DIR *dir;
	struct dirent *d;
	struct blkdev_cxt cxt = {};

	if (!(dir = opendir(_PATH_SYS_BLOCK)))
		return EXIT_FAILURE;

	while ((d = xreaddir(dir))) {
		if (set_cxt(&cxt, NULL, d->d_name, 0))
			goto next;

		/* Skip devices in the middle of dependence tree */
		if (cxt.nslaves > 0)
			goto next;

		if (!lsblk->all_devices && is_maj_excluded(cxt.maj))
			goto next;

		print_device(&cxt, NULL);
		list_holders(&cxt);
	next:
		reset_blkdev_cxt(&cxt);
	}

	closedir(dir);

	return EXIT_SUCCESS;
}

static int process_one_device(char *devname)
{
	struct blkdev_cxt parent = {}, cxt = {};
	struct stat st;
	char buf[PATH_MAX + 1], *diskname = NULL;
	dev_t disk = 0;
	int status = EXIT_FAILURE;

	if (stat(devname, &st) || !S_ISBLK(st.st_mode)) {
		warnx(_("%s: not a block device"), devname);
		return EXIT_FAILURE;
	}
	if (blkid_devno_to_wholedisk(st.st_rdev, buf, sizeof(buf), &disk)) {
		warn(_("%s: failed to get whole-disk device number"), devname);
		return EXIT_FAILURE;
	}
	if (st.st_rdev == disk) {
		/*
		 * unpartitioned device
		 */
		if (set_cxt(&cxt, NULL, buf, 0))
			goto leave;
	} else {
		/*
		 * Parititioned, read sysfs name of the device
		 */
		ssize_t len;
		char path[PATH_MAX], *name;

		if (!sysfs_devno_path(st.st_rdev, path, sizeof(path))) {
			warn(_("failed to compose sysfs path for %s"), devname);
			goto leave;
		}

		diskname = xstrdup(buf);
		len = readlink(path, buf, PATH_MAX);
		if (len < 0) {
			warn(_("%s: failed to read link"), path);
			goto leave;
		}
		buf[len] = '\0';

		/* sysfs device name */
		name = strrchr(buf, '/') + 1;

		if (set_cxt(&parent, NULL, diskname, 0))
			goto leave;
		if (set_cxt(&cxt, &parent, name, 1))
			goto leave;
	}

	print_device(&cxt, NULL);
	list_holders(&cxt);
	status = EXIT_SUCCESS;
leave:
	free(diskname);
	reset_blkdev_cxt(&cxt);

	if (st.st_rdev != disk)
		reset_blkdev_cxt(&parent);

	return status;
}

static void parse_excludes(const char *str)
{
	nexcludes = 0;

	while (str && *str) {
		char *end = NULL;
		unsigned long n;

		errno = 0;
		n = strtoul(str, &end, 10);

		if (end == str || (errno != 0 && (n == ULONG_MAX || n == 0)))
			err(EXIT_FAILURE, _("failed to parse list '%s'"), str);
		excludes[nexcludes++] = n;

		if (nexcludes == ARRAY_SIZE(excludes))
			/* TRANSLATORS: The standard value for %d is 256. */
			errx(EXIT_FAILURE, _("the list of excluded devices is "
					"too large (limit is %d devices)"),
					(int)ARRAY_SIZE(excludes));
		str = end && *end ? end + 1 : NULL;
	}
}

static void __attribute__((__noreturn__)) help(FILE *out)
{
	int i;

	fprintf(out, _(
		"\nUsage:\n"
		" %s [options] [<device> ...]\n"), program_invocation_short_name);

	fprintf(out, _(
		"\nOptions:\n"
		" -a, --all            print all devices\n"
		" -b, --bytes          print SIZE in bytes rather than in human readable format\n"
		" -d, --nodeps         don't print slaves or holders\n"
		" -D, --discard        print discard capabilities\n"
		" -e, --exclude <list> exclude devices by major number (default: RAM disks)\n"
		" -f, --fs             output info about filesystems\n"
		" -h, --help           usage information (this)\n"
		" -i, --ascii          use ascii characters only\n"
		" -m, --perms          output info about permissions\n"
		" -l, --list           use list format ouput\n"
		" -n, --noheadings     don't print headings\n"
		" -o, --output <list>  output columns\n"
		" -P, --pairs          use key=\"value\" output format\n"
		" -r, --raw            use raw output format\n"
		" -t, --topology       output info about topology\n"));

	fprintf(out, _("\nAvailable columns:\n"));

	for (i = 0; i < __NCOLUMNS; i++)
		fprintf(out, " %10s  %s\n", infos[i].name, _(infos[i].help));

	fprintf(out, _("\nFor more information see lsblk(8).\n"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void __attribute__((__noreturn__))
errx_mutually_exclusive(const char *opts)
{
	errx(EXIT_FAILURE, "%s %s", opts, _("options are mutually exclusive"));
}

int main(int argc, char *argv[])
{
	struct lsblk _ls;
	int tt_flags = TT_FL_TREE;
	int i, c, status = EXIT_FAILURE;

	static const struct option longopts[] = {
		{ "all",	0, 0, 'a' },
		{ "bytes",      0, 0, 'b' },
		{ "nodeps",     0, 0, 'd' },
		{ "discard",    0, 0, 'D' },
		{ "help",	0, 0, 'h' },
		{ "output",     1, 0, 'o' },
		{ "perms",      0, 0, 'm' },
		{ "noheadings",	0, 0, 'n' },
		{ "list",       0, 0, 'l' },
		{ "ascii",	0, 0, 'i' },
		{ "raw",        0, 0, 'r' },
		{ "fs",         0, 0, 'f' },
		{ "exclude",    1, 0, 'e' },
		{ "topology",   0, 0, 't' },
		{ "pairs",      0, 0, 'P' },
		{ NULL, 0, 0, 0 },
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	lsblk = &_ls;
	memset(lsblk, 0, sizeof(*lsblk));

	while((c = getopt_long(argc, argv, "abdDe:fhlnmo:Pirt", longopts, NULL)) != -1) {
		switch(c) {
		case 'a':
			lsblk->all_devices = 1;
			break;
		case 'b':
			lsblk->bytes = 1;
			break;
		case 'd':
			lsblk->nodeps = 1;
			break;
		case 'D':
			columns[ncolumns++] = COL_NAME;
			columns[ncolumns++] = COL_DALIGN;
			columns[ncolumns++] = COL_DGRAN;
			columns[ncolumns++] = COL_DMAX;
			columns[ncolumns++] = COL_DZERO;
			break;
		case 'e':
			parse_excludes(optarg);
			break;
		case 'h':
			help(stdout);
			break;
		case 'l':
			if ((tt_flags & TT_FL_RAW)|| (tt_flags & TT_FL_EXPORT))
				errx_mutually_exclusive("--{raw,list,export}");

			tt_flags &= ~TT_FL_TREE; /* disable the default */
			break;
		case 'n':
			tt_flags |= TT_FL_NOHEADINGS;
			break;
		case 'o':
			ncolumns = string_to_idarray(optarg,
						columns, ARRAY_SIZE(columns),
						column_name_to_id);
			if (ncolumns < 0)
				return EXIT_FAILURE;
			break;
		case 'P':
			tt_flags |= TT_FL_EXPORT;
			tt_flags &= ~TT_FL_TREE;	/* disable the default */
			break;
		case 'i':
			tt_flags |= TT_FL_ASCII;
			break;
		case 'r':
			tt_flags &= ~TT_FL_TREE;	/* disable the default */
			tt_flags |= TT_FL_RAW;		/* enable raw */
			break;
		case 'f':
			columns[ncolumns++] = COL_NAME;
			columns[ncolumns++] = COL_FSTYPE;
			columns[ncolumns++] = COL_LABEL;
			columns[ncolumns++] = COL_TARGET;
			break;
		case 'm':
			columns[ncolumns++] = COL_NAME;
			columns[ncolumns++] = COL_SIZE;
			columns[ncolumns++] = COL_OWNER;
			columns[ncolumns++] = COL_GROUP;
			columns[ncolumns++] = COL_MODE;
			break;
		case 't':
			columns[ncolumns++] = COL_NAME;
			columns[ncolumns++] = COL_ALIOFF;
			columns[ncolumns++] = COL_MINIO;
			columns[ncolumns++] = COL_OPTIO;
			columns[ncolumns++] = COL_PHYSEC;
			columns[ncolumns++] = COL_LOGSEC;
			columns[ncolumns++] = COL_ROTA;
			columns[ncolumns++] = COL_SCHED;
			columns[ncolumns++] = COL_RQ_SIZE;
			break;
		default:
			help(stderr);
		}
	}

	if (!ncolumns) {
		columns[ncolumns++] = COL_NAME;
		columns[ncolumns++] = COL_MAJMIN;
		columns[ncolumns++] = COL_RM;
		columns[ncolumns++] = COL_SIZE;
		columns[ncolumns++] = COL_RO;
		columns[ncolumns++] = COL_TYPE;
		columns[ncolumns++] = COL_TARGET;
	}

	if (nexcludes && lsblk->all_devices)
		errx_mutually_exclusive("--{all,exclude}");
	else if (!nexcludes)
		excludes[nexcludes++] = 1;	/* default: ignore RAM disks */
	/*
	 * initialize output columns
	 */
	if (!(lsblk->tt = tt_new_table(tt_flags)))
		errx(EXIT_FAILURE, _("failed to initialize output table"));

	for (i = 0; i < ncolumns; i++) {
		struct colinfo *ci = get_column_info(i);
		int fl = ci->flags;

		if (!(tt_flags & TT_FL_TREE) && get_column_id(i) == COL_NAME)
			fl &= ~TT_FL_TREE;

		if (!tt_define_column(lsblk->tt, ci->name, ci->whint, fl)) {
			warn(_("failed to initialize output column"));
			goto leave;
		}
	}

	if (optind == argc)
		status = iterate_block_devices();
	else while (optind < argc)
		status = process_one_device(argv[optind++]);

	tt_print_table(lsblk->tt);

leave:
	tt_free_table(lsblk->tt);
	return status;
}

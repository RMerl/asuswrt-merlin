/* vi: set sw=4 ts=4: */
/*
 * Support functions for mounting devices by label/uuid
 *
 * Copyright (C) 2006 by Jason Schoon <floydpink@gmail.com>
 * Some portions cribbed from e2fsprogs, util-linux, dosfstools
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include <sys/mount.h> /* BLKGETSIZE64 */
#if !defined(BLKGETSIZE64)
# define BLKGETSIZE64 _IOR(0x12,114,size_t)
#endif
#include "volume_id_internal.h"

static struct uuidCache_s {
	struct uuidCache_s *next;
	dev_t devno;
	char *device;
	char *label;
	char *uc_uuid; /* prefix makes it easier to grep for */
} *uuidCache;

/* Returns !0 on error.
 * Otherwise, returns malloc'ed strings for label and uuid
 * (and they can't be NULL, although they can be "").
 * NB: closes fd. */
static int
get_label_uuid(int fd, char **label, char **uuid)
{
	int rv = 1;
	uint64_t size;
	struct volume_id *vid;

	/* fd is owned by vid now */
	vid = volume_id_open_node(fd);

	if (ioctl(/*vid->*/fd, BLKGETSIZE64, &size) != 0)
		size = 0;

	if (volume_id_probe_all(vid, /*0,*/ size) != 0)
		goto ret;

	if (vid->label[0] != '\0' || vid->uuid[0] != '\0') {
		*label = xstrndup(vid->label, sizeof(vid->label));
		*uuid  = xstrndup(vid->uuid, sizeof(vid->uuid));
		dbg("found label '%s', uuid '%s'", *label, *uuid);
		rv = 0;
	}
 ret:
	free_volume_id(vid); /* also closes fd */
	return rv;
}

/* NB: we take ownership of (malloc'ed) label and uuid */
static void
uuidcache_addentry(char *device, dev_t devno, char *label, char *uuid)
{
	struct uuidCache_s *last;

	if (!uuidCache) {
		last = uuidCache = xzalloc(sizeof(*uuidCache));
	} else {
		for (last = uuidCache; last->next; last = last->next)
			continue;
		last->next = xzalloc(sizeof(*uuidCache));
		last = last->next;
	}
	/*last->next = NULL; - xzalloc did it*/
	last->devno = devno;
	last->device = device;
	last->label = label;
	last->uc_uuid = uuid;
}

/* If get_label_uuid() on device_name returns success,
 * add a cache entry for this device.
 * If device node does not exist, it will be temporarily created. */
static int FAST_FUNC
uuidcache_check_device(const char *device,
		struct stat *statbuf,
		void *userData UNUSED_PARAM,
		int depth UNUSED_PARAM)
{
	char *uuid = uuid; /* for compiler */
	char *label = label;
	int fd;

	/* note: this check rejects links to devices, among other nodes */
	if (!S_ISBLK(statbuf->st_mode))
		return TRUE;

	/* Users report that mucking with floppies (especially non-present
	 * ones) is significant PITA. This is a horribly dirty hack,
	 * but it is very useful in real world.
	 * If this will ever need to be enabled, consider using O_NONBLOCK.
	 */
	if (major(statbuf->st_rdev) == 2)
		return TRUE;

	fd = open(device, O_RDONLY);
	if (fd < 0)
		return TRUE;

	/* get_label_uuid() closes fd in all cases (success & failure) */
	if (get_label_uuid(fd, &label, &uuid) == 0) {
		/* uuidcache_addentry() takes ownership of all three params */
		uuidcache_addentry(xstrdup(device), statbuf->st_rdev, label, uuid);
	}
	return TRUE;
}

static void
uuidcache_init(void)
{
	if (uuidCache)
		return;

	/* We were scanning /proc/partitions
	 * and /proc/sys/dev/cdrom/info here.
	 * Missed volume managers. I see that "standard" blkid uses these:
	 * /dev/mapper/control
	 * /proc/devices
	 * /proc/evms/volumes
	 * /proc/lvm/VGs
	 * This is unacceptably complex. Let's just scan /dev.
	 * (Maybe add scanning of /sys/block/XXX/dev for devices
	 * somehow not having their /dev/XXX entries created?) */

	recursive_action("/dev", ACTION_RECURSE,
		uuidcache_check_device, /* file_action */
		NULL, /* dir_action */
		NULL, /* userData */
		0 /* depth */);
}

#define UUID   1
#define VOL    2
#define DEVNO  3

static char *
get_spec_by_x(int n, const char *t, dev_t *devnoPtr)
{
	struct uuidCache_s *uc;

	uuidcache_init();
	uc = uuidCache;

	while (uc) {
		switch (n) {
		case UUID:
			/* case of hex numbers doesn't matter */
			if (strcasecmp(t, uc->uc_uuid) == 0)
				goto found;
			break;
		case VOL:
			if (uc->label[0] && strcmp(t, uc->label) == 0)
				goto found;
			break;
		case DEVNO:
			if (uc->devno == (*devnoPtr))
				goto found;
			break;
		}
		uc = uc->next;
	}
	return NULL;

found:
	if (devnoPtr)
		*devnoPtr = uc->devno;
	return xstrdup(uc->device);
}

/* Used by blkid */
void display_uuid_cache(void)
{
	struct uuidCache_s *u;

	uuidcache_init();
	u = uuidCache;
	while (u) {
		printf("%s:", u->device);
		if (u->label[0])
			printf(" LABEL=\"%s\"", u->label);
		if (u->uc_uuid[0])
			printf(" UUID=\"%s\"", u->uc_uuid);
		bb_putchar('\n');
		u = u->next;
	}
}

/* Used by mount and findfs & old_e2fsprogs */

char *get_devname_from_label(const char *spec)
{
	return get_spec_by_x(VOL, spec, NULL);
}

char *get_devname_from_uuid(const char *spec)
{
	return get_spec_by_x(UUID, spec, NULL);
}

char *get_devname_from_device(dev_t dev)
{
	return get_spec_by_x(DEVNO, NULL, &dev);
}

int resolve_mount_spec(char **fsname)
{
	char *tmp = *fsname;

	if (strncmp(*fsname, "UUID=", 5) == 0)
		tmp = get_devname_from_uuid(*fsname + 5);
	else if (strncmp(*fsname, "LABEL=", 6) == 0)
		tmp = get_devname_from_label(*fsname + 6);
	else
		return 0; /* no UUID= or LABEL= prefix found */

	if (!tmp)
		return -2; /* device defined by UUID= or LABEL= wasn't found */

	*fsname = tmp;
	return 1;
}

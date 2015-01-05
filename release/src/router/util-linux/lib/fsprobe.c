#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>

#include <blkid.h>

#include "blkdev.h"
#include "canonicalize.h"
#include "pathnames.h"
#include "fsprobe.h"

/*
 * THIS IS DEPRECATED -- use blkid_evaluate_* API rather than this extra layer
 */


static blkid_cache blcache;
static blkid_probe blprobe;

void
fsprobe_init(void)
{
	blcache = NULL;
	blprobe = NULL;
}

void
fsprobe_exit(void)
{
	if (blprobe)
		blkid_free_probe(blprobe);
	if (blcache)
		blkid_put_cache(blcache);
}

/*
 * Parses NAME=value, returns -1 on parse error, 0 success. The success is also
 * when the 'spec' doesn't contain name=value pair (because the spec could be
 * a devname too). In particular case the pointer 'name' is set to NULL.
 */
int
fsprobe_parse_spec(const char *spec, char **name, char **value)
{
	*name = NULL;
	*value = NULL;

	if (strchr(spec, '='))
		return blkid_parse_tag_string(spec, name, value);

	return 0;
}

char *
fsprobe_get_devname_by_spec(const char *spec)
{
	return blkid_evaluate_spec(spec, &blcache);
}

int
fsprobe_known_fstype(const char *fstype)
{
	return blkid_known_fstype(fstype);
}


/* returns device LABEL, UUID, FSTYPE, ... by low-level
 * probing interface
 */
static char *
fsprobe_get_value(const char *name, const char *devname, int *ambi)
{
	int fd, rc;
	const char *data = NULL;

	if (!devname || !name)
		return NULL;
	fd = open(devname, O_RDONLY);
	if (fd < 0)
		return NULL;
	if (!blprobe)
		blprobe = blkid_new_probe();
	if (!blprobe)
		goto done;
	if (blkid_probe_set_device(blprobe, fd, 0, 0))
		goto done;

	blkid_probe_enable_superblocks(blprobe, 1);

	blkid_probe_set_superblocks_flags(blprobe,
		BLKID_SUBLKS_LABEL | BLKID_SUBLKS_UUID | BLKID_SUBLKS_TYPE);

	rc = blkid_do_safeprobe(blprobe);
	if (ambi)
		*ambi = rc == -2 ? 1 : 0;	/* ambivalent probing result */
	if (!rc)
		blkid_probe_lookup_value(blprobe, name, &data, NULL);
done:
	close(fd);
	return data ? strdup((char *) data) : NULL;
}

char *
fsprobe_get_label_by_devname(const char *devname)
{
	return fsprobe_get_value("LABEL", devname, NULL);
}

char *
fsprobe_get_uuid_by_devname(const char *devname)
{
	return fsprobe_get_value("UUID", devname, NULL);
}

char *
fsprobe_get_fstype_by_devname(const char *devname)
{
	return fsprobe_get_value("TYPE", devname, NULL);
}

char *
fsprobe_get_fstype_by_devname_ambi(const char *devname, int *ambi)
{
	return fsprobe_get_value("TYPE", devname, ambi);
}

char *
fsprobe_get_devname_by_uuid(const char *uuid)
{
	return blkid_evaluate_tag("UUID", uuid, &blcache);
}

char *
fsprobe_get_devname_by_label(const char *label)
{
	return blkid_evaluate_tag("LABEL", label, &blcache);
}


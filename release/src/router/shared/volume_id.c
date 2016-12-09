#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <errno.h>

/****************************************************/
/* Use busybox routines to get labels for fat & ext */
/* Probe for label the same way that mount does.    */
/****************************************************/
#include "autoconf.h"
#include "volume_id_internal.h"

#ifdef __GLIBC__
#undef errno
#define errno (*__errno_location ())
#endif

#pragma GCC visibility push(hidden)

#ifndef DMALLOC
void* FAST_FUNC xmalloc(size_t size)
{
	return malloc(size);
}

void* FAST_FUNC xrealloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}
#endif /* DMALLOC */

ssize_t FAST_FUNC safe_read(int fd, void *buf, size_t count)
{
	ssize_t n;

	do {
		n = read(fd, buf, count);
	} while (n < 0 && errno == EINTR);

	return n;
}

/*
 * Read all of the supplied buffer from a file.
 * This does multiple reads as necessary.
 * Returns the amount read, or -1 on an error.
 * A short read is returned on an end of file.
 */
ssize_t FAST_FUNC full_read(int fd, void *buf, size_t len)
{
	ssize_t cc;
	ssize_t total;

	total = 0;

	while (len) {
		cc = safe_read(fd, buf, len);

		if (cc < 0) {
			if (total) {
				/* we already have some! */
				/* user can do another read to know the error code */
				return total;
			}
			return cc; /* read() returns -1 on failure. */
		}
		if (cc == 0)
			break;
		buf = ((char *)buf) + cc;
		total += cc;
		len -= cc;
	}

	return total;
}

#pragma GCC visibility pop

/* Put the label in *label and uuid in *uuid.
 * Return 0 if no label/uuid found, NZ if there is a label or uuid.
 * Return -1 if no device found.
 */
int find_label_or_uuid(char *dev_name, char *label, char *uuid)
{
	struct volume_id id;

	memset(&id, 0, sizeof(id));
	if (label) *label = 0;
	if (uuid) *uuid = 0;

	if ((id.fd = open(dev_name, O_RDONLY)) < 0)
		return -1;

	volume_id_get_buffer(&id, 0, SB_BUFFER_SIZE);

	if (volume_id_probe_linux_swap(&id) == 0 || id.error)
		goto ret;
	if (volume_id_probe_vfat(&id) == 0 || id.error)
		goto ret;
	if (volume_id_probe_ext(&id) == 0 || id.error)
		goto ret;
	if (volume_id_probe_ntfs(&id) == 0 || id.error)
		goto ret;
#if defined(RTCONFIG_HFS)
	if (volume_id_probe_hfs_hfsplus(&id) == 0 || id.error)
		goto ret;
#endif
ret:
	volume_id_free_buffer(&id);

	close(id.fd);

	if (label && *id.label != 0)
		strcpy(label, id.label);
	if (uuid && *id.uuid != 0)
		strcpy(uuid, id.uuid);

	return (label && *label) || (uuid && *uuid);
}

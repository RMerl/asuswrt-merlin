/*
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
 *
 * -- based on mount/losetup.c
 *
 * Simple library for work with loop devices.
 *
 *  - requires kernel 2.6.x
 *  - reads info from /sys/block/loop<N>/loop/<attr> (new kernels)
 *  - reads info by ioctl
 *  - supports *unlimited* number of loop devices
 *  - supports /dev/loop<N> as well as /dev/loop/<N>
 *  - minimize overhead (fd, loopinfo, ... are shared for all operations)
 *  - setup (associate device and backing file)
 *  - delete (dis-associate file)
 *  - old LOOP_{SET,GET}_STATUS (32bit) ioctls are unsupported
 *  - extendible
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <inttypes.h>
#include <dirent.h>
#include <linux/posix_types.h>

#include "linux_version.h"
#include "c.h"
#include "sysfs.h"
#include "pathnames.h"
#include "loopdev.h"
#include "canonicalize.h"

#define CONFIG_LOOPDEV_DEBUG

#ifdef CONFIG_LOOPDEV_DEBUG
# include <stdarg.h>

# define DBG(l,x)	do { \
				if ((l)->debug) {\
					fprintf(stderr, "loopdev:  [%p]: ", (l)); \
					x; \
				} \
			} while(0)

static inline void __attribute__ ((__format__ (__printf__, 1, 2)))
loopdev_debug(const char *mesg, ...)
{
	va_list ap;
	va_start(ap, mesg);
	vfprintf(stderr, mesg, ap);
	va_end(ap);
	fputc('\n', stderr);
}

#else /* !CONFIG_LOOPDEV_DEBUG */
# define DBG(m,x) do { ; } while(0)
#endif


#define loopcxt_ioctl_enabled(_lc)	(!((_lc)->flags & LOOPDEV_FL_NOIOCTL))

/*
 * @lc: context
 * @device: device name, absolute device path or NULL to reset the current setting
 *
 * Sets device, absolute paths (e.g. "/dev/loop<N>") are unchanged, device
 * names ("loop<N>") are converted to the path (/dev/loop<N> or to
 * /dev/loop/<N>)
 *
 * Returns: <0 on error, 0 on success
 */
int loopcxt_set_device(struct loopdev_cxt *lc, const char *device)
{
	if (!lc)
		return -EINVAL;

	if (lc->fd >= 0)
		close(lc->fd);
	lc->fd = -1;
	lc->mode = 0;
	lc->has_info = 0;
	lc->info_failed = 0;
	*lc->device = '\0';
	memset(&lc->info, 0, sizeof(lc->info));

	/* set new */
	if (device) {
		if (*device != '/') {
			const char *dir = _PATH_DEV;

			/* compose device name for /dev/loop<n> or /dev/loop/<n> */
			if (lc->flags & LOOPDEV_FL_DEVSUBDIR) {
				if (strlen(device) < 5)
					return -1;
				device += 4;
				dir = _PATH_DEV_LOOP "/";	/* _PATH_DEV uses tailing slash */
			}
			snprintf(lc->device, sizeof(lc->device), "%s%s",
				dir, device);
		} else {
			strncpy(lc->device, device, sizeof(lc->device));
			lc->device[sizeof(lc->device) - 1] = '\0';
		}
		DBG(lc, loopdev_debug("%s successfully assigned", device));
	}

	sysfs_deinit(&lc->sysfs);
	return 0;
}

int loopcxt_has_device(struct loopdev_cxt *lc)
{
	return lc && *lc->device;
}

/*
 * @lc: context
 * @flags: LOOPDEV_FL_* flags
 *
 * Initilize loop handler.
 *
 * We have two sets of the flags:
 *
 *	* LOOPDEV_FL_* flags control loopcxt_* API behavior
 *
 *	* LO_FLAGS_* are kernel flags used for LOOP_{SET,GET}_STAT64 ioctls
 *
 * Note about LOOPDEV_FL_{RDONLY,RDWR} flags. These flags are used for open(2)
 * syscall to open loop device. By default is the device open read-only.
 *
 * The expection is loopcxt_setup_device(), where the device is open read-write
 * if LO_FLAGS_READ_ONLY flags is not set (see loopcxt_set_flags()).
 *
 * Returns: <0 on error, 0 on success.
 */
int loopcxt_init(struct loopdev_cxt *lc, int flags)
{
	struct stat st;

	if (!lc)
		return -EINVAL;

	memset(lc, 0, sizeof(*lc));
	lc->flags = flags;
	loopcxt_set_device(lc, NULL);

	if (!(lc->flags & LOOPDEV_FL_NOSYSFS) &&
	    get_linux_version() >= KERNEL_VERSION(2,6,37))
		/*
		 * Use only sysfs for basic information about loop devices
		 */
		lc->flags |= LOOPDEV_FL_NOIOCTL;

	if (!(lc->flags & LOOPDEV_FL_CONTROL) && !stat(_PATH_DEV_LOOPCTL, &st))
		lc->flags |= LOOPDEV_FL_CONTROL;

	return 0;
}

/*
 * @lc: context
 *
 * Deinitialize loop context
 */
void loopcxt_deinit(struct loopdev_cxt *lc)
{
	if (!lc)
		return;

	DBG(lc, loopdev_debug("de-initialize"));

	free(lc->filename);
	lc->filename = NULL;

	loopcxt_set_device(lc, NULL);
	loopcxt_deinit_iterator(lc);
}

/*
 * @lc: context
 * @enable: TRUE/FALSE
 *
 * Enabled/disables debug messages
 */
void loopcxt_enable_debug(struct loopdev_cxt *lc, int enable)
{
	if (lc)
		lc->debug = enable ? 1 : 0;
}

/*
 * @lc: context
 *
 * Returns newly allocated device path.
 */
char *loopcxt_strdup_device(struct loopdev_cxt *lc)
{
	if (!lc || !*lc->device)
		return NULL;
	return strdup(lc->device);
}

/*
 * @lc: context
 *
 * Returns pointer device name in the @lc struct.
 */
const char *loopcxt_get_device(struct loopdev_cxt *lc)
{
	return lc ? lc->device : NULL;
}

/*
 * @lc: context
 *
 * Returns pointer to the sysfs context (see lib/sysfs.c)
 */
struct sysfs_cxt *loopcxt_get_sysfs(struct loopdev_cxt *lc)
{
	if (!lc || !*lc->device || (lc->flags & LOOPDEV_FL_NOSYSFS))
		return NULL;

	if (!lc->sysfs.devno) {
		dev_t devno = sysfs_devname_to_devno(lc->device, NULL);
		if (!devno) {
			DBG(lc, loopdev_debug("sysfs: failed devname to devno"));
			return NULL;
		}
		if (sysfs_init(&lc->sysfs, devno, NULL)) {
			DBG(lc, loopdev_debug("sysfs: init failed"));
			return NULL;
		}
	}

	return &lc->sysfs;
}

/*
 * @lc: context
 *
 * Returns: file descriptor to the open loop device or <0 on error. The mode
 *          depends on LOOPDEV_FL_{RDWR,RDONLY} context flags. Default is
 *          read-only.
 */
int loopcxt_get_fd(struct loopdev_cxt *lc)
{
	if (!lc || !*lc->device)
		return -EINVAL;

	if (lc->fd < 0) {
		lc->mode = lc->flags & LOOPDEV_FL_RDWR ? O_RDWR : O_RDONLY;
		lc->fd = open(lc->device, lc->mode);
		DBG(lc, loopdev_debug("open %s", lc->fd < 0 ? "failed" : "ok"));
	}
	return lc->fd;
}

int loopcxt_set_fd(struct loopdev_cxt *lc, int fd, int mode)
{
	if (!lc)
		return -EINVAL;

	lc->fd = fd;
	lc->mode = mode;
	return 0;
}

/*
 * @lc: context
 * @flags: LOOPITER_FL_* flags
 *
 * Iterator allows to scan list of the free or used loop devices.
 *
 * Returns: <0 on error, 0 on success
 */
int loopcxt_init_iterator(struct loopdev_cxt *lc, int flags)
{
	struct loopdev_iter *iter;
	struct stat st;

	if (!lc)
		return -EINVAL;

	DBG(lc, loopdev_debug("iter: initialize"));

	iter = &lc->iter;

	/* always zeroize
	 */
	memset(iter, 0, sizeof(*iter));
	iter->ncur = -1;
	iter->flags = flags;
	iter->default_check = 1;

	if (!lc->extra_check) {
		/*
		 * Check for /dev/loop/<N> subdirectory
		 */
		if (!(lc->flags & LOOPDEV_FL_DEVSUBDIR) &&
		    stat(_PATH_DEV_LOOP, &st) == 0 && S_ISDIR(st.st_mode))
			lc->flags |= LOOPDEV_FL_DEVSUBDIR;

		lc->extra_check = 1;
	}
	return 0;
}

/*
 * @lc: context
 *
 * Returns: <0 on error, 0 on success
 */
int loopcxt_deinit_iterator(struct loopdev_cxt *lc)
{
	struct loopdev_iter *iter;

	if (!lc)
		return -EINVAL;

	DBG(lc, loopdev_debug("iter: de-initialize"));

	iter = &lc->iter;

	free(iter->minors);
	if (iter->proc)
		fclose(iter->proc);
	iter->minors = NULL;
	iter->proc = NULL;
	iter->done = 1;
	return 0;
}

/*
 * Same as loopcxt_set_device, but also checks if the device is
 * associeted with any file.
 *
 * Returns: <0 on error, 0 on success, 1 device does not match with
 *         LOOPITER_FL_{USED,FREE} flags.
 */
static int loopiter_set_device(struct loopdev_cxt *lc, const char *device)
{
	int rc = loopcxt_set_device(lc, device);
	int used;

	if (rc)
		return rc;

	if (!(lc->iter.flags & LOOPITER_FL_USED) &&
	    !(lc->iter.flags & LOOPITER_FL_FREE))
		return 0;	/* caller does not care about device status */

	used = loopcxt_get_offset(lc, NULL) == 0;

	if ((lc->iter.flags & LOOPITER_FL_USED) && used)
		return 0;

	if ((lc->iter.flags & LOOPITER_FL_FREE) && !used)
		return 0;

	DBG(lc, loopdev_debug("iter: setting device"));
	loopcxt_set_device(lc, NULL);
	return 1;
}

static int cmpnum(const void *p1, const void *p2)
{
	return (((* (int *) p1) > (* (int *) p2)) -
			((* (int *) p1) < (* (int *) p2)));
}

/*
 * The classic scandir() is more expensive and less portable.
 * We needn't full loop device names -- loop numbers (loop<N>)
 * are enough.
 */
static int loop_scandir(const char *dirname, int **ary, int hasprefix)
{
	DIR *dir;
	struct dirent *d;
	unsigned int n, count = 0, arylen = 0;

	if (!dirname || !ary)
		return 0;
	dir = opendir(dirname);
	if (!dir)
		return 0;
	free(*ary);
	*ary = NULL;

	while((d = readdir(dir))) {
#ifdef _DIRENT_HAVE_D_TYPE
		if (d->d_type != DT_BLK && d->d_type != DT_UNKNOWN &&
		    d->d_type != DT_LNK)
			continue;
#endif
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;

		if (hasprefix) {
			/* /dev/loop<N> */
			if (sscanf(d->d_name, "loop%u", &n) != 1)
				continue;
		} else {
			/* /dev/loop/<N> */
			char *end = NULL;

			n = strtol(d->d_name, &end, 10);
			if (d->d_name == end || (end && *end) || errno)
				continue;
		}
		if (n < LOOPDEV_DEFAULT_NNODES)
			continue;			/* ignore loop<0..7> */

		if (count + 1 > arylen) {
			int *tmp;

			arylen += 1;

			tmp = realloc(*ary, arylen * sizeof(int));
			if (!tmp) {
				free(*ary);
				closedir(dir);
				return -1;
			}
			*ary = tmp;
		}
		if (*ary)
			(*ary)[count++] = n;
	}
	if (count && *ary)
		qsort(*ary, count, sizeof(int), cmpnum);

	closedir(dir);
	return count;
}

/*
 * @lc: context, has to initialized by loopcxt_init_iterator()
 *
 * Returns: 0 on success, -1 on error, 1 at the end of scanning. The details
 *          about the current loop device are available by
 *          loopcxt_get_{fd,backing_file,device,offset, ...} functions.
 */
int loopcxt_next(struct loopdev_cxt *lc)
{
	struct loopdev_iter *iter;

	if (!lc)
		return -EINVAL;

	DBG(lc, loopdev_debug("iter: next"));

	iter = &lc->iter;
	if (iter->done)
		return 1;

	/* A) Look for used loop devices in /proc/partitions ("losetup -a" only)
	 */
	if (iter->flags & LOOPITER_FL_USED) {
		char buf[BUFSIZ];

		if (!iter->proc)
			iter->proc = fopen(_PATH_PROC_PARTITIONS, "r");

		while (iter->proc && fgets(buf, sizeof(buf), iter->proc)) {
			unsigned int m;
			char name[128];

			if (sscanf(buf, " %u %*s %*s %128[^\n ]",
				   &m, name) != 2 || m != LOOPDEV_MAJOR)
				continue;

			if (loopiter_set_device(lc, name) == 0)
				return 0;
		}

		goto done;
	}

	/* B) Classic way, try first eight loop devices (default number
	 *    of loop devices). This is enough for 99% of all cases.
	 */
	if (iter->default_check) {
		for (++iter->ncur; iter->ncur < LOOPDEV_DEFAULT_NNODES;
							iter->ncur++) {
			char name[16];
			snprintf(name, sizeof(name), "loop%d", iter->ncur);

			if (loopiter_set_device(lc, name) == 0)
				return 0;
		}
		iter->default_check = 0;
	}

	/* C) the worst possibility, scan whole /dev or /dev/loop/<N>
	 */
	if (!iter->minors) {
		iter->nminors = (lc->flags & LOOPDEV_FL_DEVSUBDIR) ?
			loop_scandir(_PATH_DEV_LOOP, &iter->minors, 0) :
			loop_scandir(_PATH_DEV, &iter->minors, 1);
		iter->ncur = -1;
	}
	for (++iter->ncur; iter->ncur < iter->nminors; iter->ncur++) {
		char name[16];
		snprintf(name, sizeof(name), "loop%d", iter->minors[iter->ncur]);

		if (loopiter_set_device(lc, name) == 0)
			return 0;
	}
done:
	loopcxt_deinit_iterator(lc);
	return 1;
}

/*
 * @device: path to device
 */
int is_loopdev(const char *device)
{
	struct stat st;

	if (!device)
		return 0;

	return (stat(device, &st) == 0 &&
		S_ISBLK(st.st_mode) &&
		major(st.st_rdev) == LOOPDEV_MAJOR);
}

/*
 * @lc: context
 *
 * Returns result from LOOP_GET_STAT64 ioctl or NULL on error.
 */
struct loop_info64 *loopcxt_get_info(struct loopdev_cxt *lc)
{
	int fd;

	if (!lc || lc->info_failed)
		return NULL;
	if (lc->has_info)
		return &lc->info;

	fd = loopcxt_get_fd(lc);
	if (fd < 0)
		return NULL;

	if (ioctl(fd, LOOP_GET_STATUS64, &lc->info) == 0) {
		lc->has_info = 1;
		lc->info_failed = 0;
		DBG(lc, loopdev_debug("reading loop_info64 OK"));
		return &lc->info;
	} else {
		lc->info_failed = 1;
		DBG(lc, loopdev_debug("reading loop_info64 FAILED"));
	}

	return NULL;
}

/*
 * @lc: context
 *
 * Returns (allocated) string with path to the file assicieted
 * with the current loop device.
 */
char *loopcxt_get_backing_file(struct loopdev_cxt *lc)
{
	struct sysfs_cxt *sysfs = loopcxt_get_sysfs(lc);
	char *res = NULL;

	if (sysfs)
		/*
		 * This is always preffered, the loop_info64
		 * has too small buffer for the filename.
		 */
		res = sysfs_strdup(sysfs, "loop/backing_file");

	if (!res && loopcxt_ioctl_enabled(lc)) {
		struct loop_info64 *lo = loopcxt_get_info(lc);

		if (lo) {
			lo->lo_file_name[LO_NAME_SIZE - 2] = '*';
			lo->lo_file_name[LO_NAME_SIZE - 1] = '\0';
			res = strdup((char *) lo->lo_file_name);
		}
	}

	DBG(lc, loopdev_debug("get_backing_file [%s]", res));
	return res;
}

/*
 * @lc: context
 * @offset: returns offset number for the given device
 *
 * Returns: <0 on error, 0 on success
 */
int loopcxt_get_offset(struct loopdev_cxt *lc, uint64_t *offset)
{
	struct sysfs_cxt *sysfs = loopcxt_get_sysfs(lc);
	int rc = -EINVAL;

	if (sysfs)
		rc = sysfs_read_u64(sysfs, "loop/offset", offset);

	if (rc && loopcxt_ioctl_enabled(lc)) {
		struct loop_info64 *lo = loopcxt_get_info(lc);
		if (lo) {
			if (offset)
				*offset = lo->lo_offset;
			rc = 0;
		}
	}

	DBG(lc, loopdev_debug("get_offset [rc=%d]", rc));
	return rc;
}

/*
 * @lc: context
 * @sizelimit: returns size limit for the given device
 *
 * Returns: <0 on error, 0 on success
 */
int loopcxt_get_sizelimit(struct loopdev_cxt *lc, uint64_t *size)
{
	struct sysfs_cxt *sysfs = loopcxt_get_sysfs(lc);
	int rc = -EINVAL;

	if (sysfs)
		rc = sysfs_read_u64(sysfs, "loop/sizelimit", size);

	if (rc && loopcxt_ioctl_enabled(lc)) {
		struct loop_info64 *lo = loopcxt_get_info(lc);
		if (lo) {
			if (size)
				*size = lo->lo_sizelimit;
			rc = 0;
		}
	}

	DBG(lc, loopdev_debug("get_sizelimit [rc=%d]", rc));
	return rc;
}

/*
 * @lc: context
 * @devno: returns encryption type
 *
 * Cryptoloop is DEPRECATED!
 *
 * Returns: <0 on error, 0 on success
 */
int loopcxt_get_encrypt_type(struct loopdev_cxt *lc, uint32_t *type)
{
	struct loop_info64 *lo = loopcxt_get_info(lc);
	int rc = -EINVAL;

	if (lo) {
		if (type)
			*type = lo->lo_encrypt_type;
		rc = 0;
	}
	DBG(lc, loopdev_debug("get_encrypt_type [rc=%d]", rc));
	return rc;
}

/*
 * @lc: context
 * @devno: returns crypt name
 *
 * Cryptoloop is DEPRECATED!
 *
 * Returns: <0 on error, 0 on success
 */
const char *loopcxt_get_crypt_name(struct loopdev_cxt *lc)
{
	struct loop_info64 *lo = loopcxt_get_info(lc);

	if (lo)
		return (char *) lo->lo_crypt_name;

	DBG(lc, loopdev_debug("get_crypt_name failed"));
	return NULL;
}

/*
 * @lc: context
 * @devno: returns backing file devno
 *
 * Returns: <0 on error, 0 on success
 */
int loopcxt_get_backing_devno(struct loopdev_cxt *lc, dev_t *devno)
{
	struct loop_info64 *lo = loopcxt_get_info(lc);
	int rc = -EINVAL;

	if (lo) {
		if (devno)
			*devno = lo->lo_device;
		rc = 0;
	}
	DBG(lc, loopdev_debug("get_backing_devno [rc=%d]", rc));
	return rc;
}

/*
 * @lc: context
 * @ino: returns backing file inode
 *
 * Returns: <0 on error, 0 on success
 */
int loopcxt_get_backing_inode(struct loopdev_cxt *lc, ino_t *ino)
{
	struct loop_info64 *lo = loopcxt_get_info(lc);
	int rc = -EINVAL;

	if (lo) {
		if (ino)
			*ino = lo->lo_inode;
		rc = 0;
	}
	DBG(lc, loopdev_debug("get_backing_inode [rc=%d]", rc));
	return rc;
}

/*
 * Check if the kernel supports partitioned loop devices.
 *
 * Notes:
 *   - kernels < 3.2 support partitioned loop devices and PT scanning
 *     only if max_part= module paremeter is non-zero
 *
 *   - kernels >= 3.2 always support partitioned loop devices
 *
 *   - kernels >= 3.2 always support BLKPG_{ADD,DEL}_PARTITION ioctls
 *
 *   - kernels >= 3.2 enable PT scanner only if max_part= is non-zero or if the
 *     LO_FLAGS_PARTSCAN flag is set for the device. The PT scanner is disabled
 *     by default.
 *
 *  See kernel commit e03c8dd14915fabc101aa495828d58598dc5af98.
 */
int loopmod_supports_partscan(void)
{
	int rc, ret = 0;
	FILE *f;

	if (get_linux_version() >= KERNEL_VERSION(3,2,0))
		return 1;

	f = fopen("/sys/module/loop/parameters/max_part", "r");
	if (!f)
		return 0;
	rc = fscanf(f, "%d", &ret);
	fclose(f);
	return rc == 1 ? ret : 0;
}

/*
 * @lc: context
 *
 * Returns: 1 if the partscan flags is set *or* (for old kernels) partitions
 * scannig is enabled for all loop devices.
 */
int loopcxt_is_partscan(struct loopdev_cxt *lc)
{
	struct sysfs_cxt *sysfs = loopcxt_get_sysfs(lc);

	if (sysfs) {
		/* kernel >= 3.2 */
		int fl;
		if (sysfs_read_int(sysfs, "loop/partscan", &fl) == 0)
			return fl;
	}

	/* old kernels (including kernels without loopN/loop/<flags> directory */
	return loopmod_supports_partscan();
}

/*
 * @lc: context
 *
 * Returns: 1 if the autoclear flags is set.
 */
int loopcxt_is_autoclear(struct loopdev_cxt *lc)
{
	struct sysfs_cxt *sysfs = loopcxt_get_sysfs(lc);

	if (sysfs) {
		int fl;
		if (sysfs_read_int(sysfs, "loop/autoclear", &fl) == 0)
			return fl;
	}

	if (loopcxt_ioctl_enabled(lc)) {
		struct loop_info64 *lo = loopcxt_get_info(lc);
		if (lo)
			return lo->lo_flags & LO_FLAGS_AUTOCLEAR;
	}
	return 0;
}

/*
 * @lc: context
 *
 * Returns: 1 if the readonly flags is set.
 */
int loopcxt_is_readonly(struct loopdev_cxt *lc)
{
	struct sysfs_cxt *sysfs = loopcxt_get_sysfs(lc);

	if (sysfs) {
		int fl;
		if (sysfs_read_int(sysfs, "ro", &fl) == 0)
			return fl;
	}

	if (loopcxt_ioctl_enabled(lc)) {
		struct loop_info64 *lo = loopcxt_get_info(lc);
		if (lo)
			return lo->lo_flags & LO_FLAGS_READ_ONLY;
	}
	return 0;
}

/*
 * @lc: context
 * @st: backing file stat or NULL
 * @backing_file: filename
 * @offset: offset
 * @flags: LOOPDEV_FL_OFFSET if @offset should not be ignored
 *
 * Returns 1 if the current @lc loopdev is associated with the given backing
 * file. Note that the preferred way is to use devno and inode number rather
 * than filename. The @backing_file filename is poor solution usable in case
 * that you don't have rights to call stat().
 *
 * Don't forget that old kernels provide very restricted (in size) backing
 * filename by LOOP_GET_STAT64 ioctl only.
 */
int loopcxt_is_used(struct loopdev_cxt *lc,
		    struct stat *st,
		    const char *backing_file,
		    uint64_t offset,
		    int flags)
{
	ino_t ino;
	dev_t dev;

	if (!lc)
		return 0;

	DBG(lc, loopdev_debug("checking %s vs. %s",
				loopcxt_get_device(lc),
				backing_file));

	if (st && loopcxt_get_backing_inode(lc, &ino) == 0 &&
		  loopcxt_get_backing_devno(lc, &dev) == 0) {

		if (ino == st->st_ino && dev == st->st_dev)
			goto found;

		/* don't use filename if we have devno and inode */
		return 0;
	}

	/* poor man's solution */
	if (backing_file) {
		char *name = loopcxt_get_backing_file(lc);
		int rc = name && strcmp(name, backing_file) == 0;

		free(name);
		if (rc)
			goto found;
	}

	return 0;
found:
	if (flags & LOOPDEV_FL_OFFSET) {
		uint64_t off;

		return loopcxt_get_offset(lc, &off) == 0 && off == offset;
	}
	return 1;
}

/*
 * The setting is removed by loopcxt_set_device() loopcxt_next()!
 */
int loopcxt_set_offset(struct loopdev_cxt *lc, uint64_t offset)
{
	if (!lc)
		return -EINVAL;
	lc->info.lo_offset = offset;

	DBG(lc, loopdev_debug("set offset=%jd", offset));
	return 0;
}

/*
 * The setting is removed by loopcxt_set_device() loopcxt_next()!
 */
int loopcxt_set_sizelimit(struct loopdev_cxt *lc, uint64_t sizelimit)
{
	if (!lc)
		return -EINVAL;
	lc->info.lo_sizelimit = sizelimit;

	DBG(lc, loopdev_debug("set sizelimit=%jd", sizelimit));
	return 0;
}

/*
 * @lc: context
 * @flags: kernel LO_FLAGS_{READ_ONLY,USE_AOPS,AUTOCLEAR} flags
 *
 * The setting is removed by loopcxt_set_device() loopcxt_next()!
 *
 * Returns: 0 on success, <0 on error.
 */
int loopcxt_set_flags(struct loopdev_cxt *lc, uint32_t flags)
{
	if (!lc)
		return -EINVAL;
	lc->info.lo_flags = flags;

	DBG(lc, loopdev_debug("set flags=%u", (unsigned) flags));
	return 0;
}

/*
 * @lc: context
 * @filename: backing file path (the path will be canonicalized)
 *
 * The setting is removed by loopcxt_set_device() loopcxt_next()!
 *
 * Returns: 0 on success, <0 on error.
 */
int loopcxt_set_backing_file(struct loopdev_cxt *lc, const char *filename)
{
	if (!lc)
		return -EINVAL;

	lc->filename = canonicalize_path(filename);
	if (!lc->filename)
		return -errno;

	strncpy((char *)lc->info.lo_file_name, lc->filename, LO_NAME_SIZE);
	lc->info.lo_file_name[LO_NAME_SIZE- 1] = '\0';

	DBG(lc, loopdev_debug("set backing file=%s", lc->info.lo_file_name));
	return 0;
}

static int digits_only(const char *s)
{
	while (*s)
		if (!isdigit(*s++))
			return 0;
	return 1;
}

/*
 * @lc: context
 * @encryption: encryption name / type (see lopsetup man page)
 * @password
 *
 * Note that the encryption functionality is deprecated an unmaintained. Use
 * cryptsetup (it also supports AES-loops).
 *
 * The setting is removed by loopcxt_set_device() loopcxt_next()!
 *
 * Returns: 0 on success, <0 on error.
 */
int loopcxt_set_encryption(struct loopdev_cxt *lc,
			   const char *encryption,
			   const char *password)
{
	if (!lc)
		return -EINVAL;

	DBG(lc, loopdev_debug("setting encryption '%s'", encryption));

	if (encryption && *encryption) {
		if (digits_only(encryption)) {
			lc->info.lo_encrypt_type = atoi(encryption);
		} else {
			lc->info.lo_encrypt_type = LO_CRYPT_CRYPTOAPI;
			snprintf((char *)lc->info.lo_crypt_name, LO_NAME_SIZE,
				 "%s", encryption);
		}
	}

	switch (lc->info.lo_encrypt_type) {
	case LO_CRYPT_NONE:
		lc->info.lo_encrypt_key_size = 0;
		break;
	default:
		DBG(lc, loopdev_debug("setting encryption key"));
		memset(lc->info.lo_encrypt_key, 0, LO_KEY_SIZE);
		strncpy((char *)lc->info.lo_encrypt_key, password, LO_KEY_SIZE);
		lc->info.lo_encrypt_key[LO_KEY_SIZE - 1] = '\0';
		lc->info.lo_encrypt_key_size = LO_KEY_SIZE;
		break;
	}

	DBG(lc, loopdev_debug("encryption successfully set"));
	return 0;
}

/*
 * @cl: context
 *
 * Associate the current device (see loopcxt_{set,get}_device()) with
 * a file (see loopcxt_set_backing_file()).
 *
 * The device is initialized read-write by default. If you want read-only
 * device then set LO_FLAGS_READ_ONLY by loopcxt_set_flags(). The LOOPDEV_FL_*
 * flags are ignored and modified according to LO_FLAGS_*.
 *
 * If the device is already open by loopcxt_get_fd() then this setup device
 * function will re-open the device to fix read/write mode.
 *
 * The device is also initialized read-only if the backing file is not
 * possible to open read-write (e.g. read-only FS).
 *
 * Returns: <0 on error, 0 on success.
 */
int loopcxt_setup_device(struct loopdev_cxt *lc)
{
	int file_fd, dev_fd, mode = O_RDWR, rc = -1;

	if (!lc || !*lc->device || !lc->filename)
		return -EINVAL;

	DBG(lc, loopdev_debug("device setup requested"));

	/*
	 * Open backing file and device
	 */
	if (lc->info.lo_flags & LO_FLAGS_READ_ONLY)
		mode = O_RDONLY;

	if ((file_fd = open(lc->filename, mode)) < 0) {
		if (mode != O_RDONLY && (errno == EROFS || errno == EACCES))
			file_fd = open(lc->filename, mode = O_RDONLY);

		if (file_fd < 0) {
			DBG(lc, loopdev_debug("open backing file failed: %m"));
			return -errno;
		}
	}
	DBG(lc, loopdev_debug("setup: backing file open: OK"));

	if (lc->fd != -1 && lc->mode != mode) {
		close(lc->fd);
		lc->fd = -1;
		lc->mode = 0;
	}

	if (mode == O_RDONLY) {
		lc->flags |= LOOPDEV_FL_RDONLY;			/* open() mode */
		lc->info.lo_flags |= LO_FLAGS_READ_ONLY;	/* kernel loopdev mode */
	} else {
		lc->flags |= LOOPDEV_FL_RDWR;			/* open() mode */
		lc->info.lo_flags &= ~LO_FLAGS_READ_ONLY;
		lc->flags &= ~LOOPDEV_FL_RDONLY;
	}

	dev_fd = loopcxt_get_fd(lc);
	if (dev_fd < 0) {
		rc = -errno;
		goto err;
	}

	DBG(lc, loopdev_debug("setup: device open: OK"));

	/*
	 * Set FD
	 */
	if (ioctl(dev_fd, LOOP_SET_FD, file_fd) < 0) {
		rc = -errno;
		DBG(lc, loopdev_debug("LOOP_SET_FD failed: %m"));
		goto err;
	}

	DBG(lc, loopdev_debug("setup: LOOP_SET_FD: OK"));

	close(file_fd);
	file_fd = -1;

	if (ioctl(dev_fd, LOOP_SET_STATUS64, &lc->info)) {
		DBG(lc, loopdev_debug("LOOP_SET_STATUS64 failed: %m"));
		goto err;
	}

	DBG(lc, loopdev_debug("setup: LOOP_SET_STATUS64: OK"));

	memset(&lc->info, 0, sizeof(lc->info));
	lc->has_info = 0;
	lc->info_failed = 0;

	DBG(lc, loopdev_debug("setup success [rc=0]"));
	return 0;
err:
	if (file_fd >= 0)
		close(file_fd);
	if (dev_fd >= 0)
		ioctl(dev_fd, LOOP_CLR_FD, 0);

	DBG(lc, loopdev_debug("setup failed [rc=%d]", rc));
	return rc;
}

int loopcxt_delete_device(struct loopdev_cxt *lc)
{
	int fd = loopcxt_get_fd(lc);

	if (fd < 0)
		return -EINVAL;

	if (ioctl(fd, LOOP_CLR_FD, 0) < 0) {
		DBG(lc, loopdev_debug("LOOP_CLR_FD failed: %m"));
		return -errno;
	}

	DBG(lc, loopdev_debug("device removed"));
	return 0;
}

/*
 * Note that LOOP_CTL_GET_FREE ioctl is supported since kernel 3.1. In older
 * kernels we have to check all loop devices to found unused one.
 *
 * See kernel commit 770fe30a46a12b6fb6b63fbe1737654d28e8484.
 */
int loopcxt_find_unused(struct loopdev_cxt *lc)
{
	int rc = -1;

	DBG(lc, loopdev_debug("find_unused requested"));

	if (lc->flags & LOOPDEV_FL_CONTROL) {
		int ctl = open(_PATH_DEV_LOOPCTL, O_RDWR);

		if (ctl >= 0)
			rc = ioctl(ctl, LOOP_CTL_GET_FREE);
		if (rc >= 0) {
			char name[16];
			snprintf(name, sizeof(name), "loop%d", rc);

			rc = loopiter_set_device(lc, name);
		}
		if (ctl >= 0)
			close(ctl);
		DBG(lc, loopdev_debug("find_unused by loop-control [rc=%d]", rc));
	}

	if (rc < 0) {
		rc = loopcxt_init_iterator(lc, LOOPITER_FL_FREE);
		if (rc)
			return rc;

		rc = loopcxt_next(lc);
		loopcxt_deinit_iterator(lc);
		DBG(lc, loopdev_debug("find_unused by scan [rc=%d]", rc));
	}
	return rc;
}



/*
 * Return: TRUE/FALSE
 */
int loopdev_is_autoclear(const char *device)
{
	struct loopdev_cxt lc;
	int rc;

	if (!device)
		return 0;

	loopcxt_init(&lc, 0);
	loopcxt_set_device(&lc, device);
	rc = loopcxt_is_autoclear(&lc);
	loopcxt_deinit(&lc);

	return rc;
}

char *loopdev_get_backing_file(const char *device)
{
	struct loopdev_cxt lc;
	char *res;

	if (!device)
		return NULL;

	loopcxt_init(&lc, 0);
	loopcxt_set_device(&lc, device);
	res = loopcxt_get_backing_file(&lc);
	loopcxt_deinit(&lc);

	return res;
}

/*
 * Returns: TRUE/FALSE
 */
int loopdev_is_used(const char *device, const char *filename,
		    uint64_t offset, int flags)
{
	struct loopdev_cxt lc;
	struct stat st;
	int rc = 0;

	if (!device || !filename)
		return 0;

	loopcxt_init(&lc, 0);
	loopcxt_set_device(&lc, device);

	rc = !stat(filename, &st);
	rc = loopcxt_is_used(&lc, rc ? &st : NULL, filename, offset, flags);

	loopcxt_deinit(&lc);
	return rc;
}

int loopdev_delete(const char *device)
{
	struct loopdev_cxt lc;
	int rc;

	if (!device)
		return -EINVAL;

	loopcxt_init(&lc, 0);
	rc = loopcxt_set_device(&lc, device);
	if (!rc)
		rc = loopcxt_delete_device(&lc);
	loopcxt_deinit(&lc);
	return rc;
}

/*
 * Returns: 0 = success, < 0 error, 1 not found
 */
int loopcxt_find_by_backing_file(struct loopdev_cxt *lc, const char *filename,
				 uint64_t offset, int flags)
{
	int rc, hasst;
	struct stat st;

	if (!filename)
		return -EINVAL;

	hasst = !stat(filename, &st);

	rc = loopcxt_init_iterator(lc, LOOPITER_FL_USED);
	if (rc)
		return rc;

	while ((rc = loopcxt_next(lc)) == 0) {

		if (loopcxt_is_used(lc, hasst ? &st : NULL,
					filename, offset, flags))
			break;
	}

	loopcxt_deinit_iterator(lc);
	return rc;
}

/*
 * Returns allocated string with device name
 */
char *loopdev_find_by_backing_file(const char *filename, uint64_t offset, int flags)
{
	struct loopdev_cxt lc;
	char *res = NULL;

	if (!filename)
		return NULL;

	loopcxt_init(&lc, 0);
	if (loopcxt_find_by_backing_file(&lc, filename, offset, flags))
		res = loopcxt_strdup_device(&lc);
	loopcxt_deinit(&lc);

	return res;
}

/*
 * Returns number of loop devices associated with @file, if only one loop
 * device is associeted with the given @filename and @loopdev is not NULL then
 * @loopdev returns name of the device.
 */
int loopdev_count_by_backing_file(const char *filename, char **loopdev)
{
	struct loopdev_cxt lc;
	int count = 0;

	if (!filename)
		return -1;

	loopcxt_init(&lc, 0);
	if (loopcxt_init_iterator(&lc, LOOPITER_FL_USED))
		return -1;

	while(loopcxt_next(&lc) == 0) {
		char *backing = loopcxt_get_backing_file(&lc);

		if (!backing || strcmp(backing, filename)) {
			free(backing);
			continue;
		}

		free(backing);
		if (loopdev && count == 0)
			*loopdev = loopcxt_strdup_device(&lc);
		count++;
	}

	loopcxt_deinit(&lc);

	if (loopdev && count > 1) {
		free(*loopdev);
		*loopdev = NULL;
	}
	return count;
}


#ifdef TEST_PROGRAM_LOOPDEV
#include <errno.h>
#include <err.h>

static void test_loop_info(const char *device, int flags, int debug)
{
	struct loopdev_cxt lc;
	char *p;
	uint64_t u64;

	loopcxt_init(&lc, flags);
	loopcxt_enable_debug(&lc, debug);

	if (loopcxt_set_device(&lc, device))
		err(EXIT_FAILURE, "failed to set device");

	p = loopcxt_get_backing_file(&lc);
	printf("\tBACKING FILE: %s\n", p);
	free(p);

	if (loopcxt_get_offset(&lc, &u64) == 0)
		printf("\tOFFSET: %jd\n", u64);

	if (loopcxt_get_sizelimit(&lc, &u64) == 0)
		printf("\tSIZE LIMIT: %jd\n", u64);

	printf("\tAUTOCLEAR: %s\n", loopcxt_is_autoclear(&lc) ? "YES" : "NOT");

	loopcxt_deinit(&lc);
}

static void test_loop_scan(int flags, int debug)
{
	struct loopdev_cxt lc;
	int rc;

	loopcxt_init(&lc, 0);
	loopcxt_enable_debug(&lc, debug);

	if (loopcxt_init_iterator(&lc, flags))
		err(EXIT_FAILURE, "iterator initlization failed");

	while((rc = loopcxt_next(&lc)) == 0) {
		const char *device = loopcxt_get_device(&lc);

		if (flags & LOOPITER_FL_USED) {
			char *backing = loopcxt_get_backing_file(&lc);
			printf("\t%s: %s\n", device, backing);
			free(backing);
		} else
			printf("\t%s\n", device);
	}

	if (rc < 0)
		err(EXIT_FAILURE, "loopdevs scanning failed");

	loopcxt_deinit(&lc);
}

static int test_loop_setup(const char *filename, const char *device, int debug)
{
	struct loopdev_cxt lc;
	int rc = 0;

	loopcxt_init(&lc, 0);
	loopcxt_enable_debug(&lc, debug);

	if (device) {
		rc = loopcxt_set_device(&lc, device);
		if (rc)
			err(EXIT_FAILURE, "failed to set device: %s", device);
	}

	do {
		if (!device) {
			rc = loopcxt_find_unused(&lc);
			if (rc)
				err(EXIT_FAILURE, "failed to find unused device");
			printf("Trying to use '%s'\n", loopcxt_get_device(&lc));
		}

		if (loopcxt_set_backing_file(&lc, filename))
			err(EXIT_FAILURE, "failed to set backing file");

		rc = loopcxt_setup_device(&lc);
		if (rc == 0)
			break;		/* success */

		if (device || rc != -EBUSY)
			err(EXIT_FAILURE, "failed to setup device for %s",
					lc.filename);

		printf("device stolen...trying again\n");
	} while (1);

	loopcxt_deinit(&lc);

	return 0;
}

int main(int argc, char *argv[])
{
	int dbg;

	if (argc < 2)
		goto usage;

	dbg = getenv("LOOPDEV_DEBUG") == NULL ? 0 : 1;

	if (argc == 3 && strcmp(argv[1], "--info") == 0) {
		printf("---sysfs & ioctl:---\n");
		test_loop_info(argv[2], 0, dbg);
		printf("---sysfs only:---\n");
		test_loop_info(argv[2], LOOPDEV_FL_NOIOCTL, dbg);
		printf("---ioctl only:---\n");
		test_loop_info(argv[2], LOOPDEV_FL_NOSYSFS, dbg);

	} else if (argc == 2 && strcmp(argv[1], "--used") == 0) {
		printf("---all used devices---\n");
		test_loop_scan(LOOPITER_FL_USED, dbg);

	} else if (argc == 2 && strcmp(argv[1], "--free") == 0) {
		printf("---all free devices---\n");
		test_loop_scan(LOOPITER_FL_FREE, dbg);

	} else if (argc >= 3 && strcmp(argv[1], "--setup") == 0) {
		test_loop_setup(argv[2], argv[3], dbg);

	} else if (argc == 3 && strcmp(argv[1], "--delete") == 0) {
		if (loopdev_delete(argv[2]))
			errx(EXIT_FAILURE, "failed to deinitialize device %s", argv[2]);
	} else
		goto usage;

	return EXIT_SUCCESS;

usage:
	errx(EXIT_FAILURE, "usage: \n"
			   "  %1$s --info <device>\n"
			   "  %1$s --free\n"
			   "  %1$s --used\n"
			   "  %1$s --setup <filename> [<device>]\n"
			   "  %1$s --delete\n",
			   argv[0]);
}

#endif /* TEST_PROGRAM */

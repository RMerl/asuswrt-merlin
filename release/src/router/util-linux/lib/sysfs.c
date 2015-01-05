/*
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
 */

#include "c.h"
#include "at.h"
#include "pathnames.h"
#include "sysfs.h"

char *sysfs_devno_attribute_path(dev_t devno, char *buf,
				 size_t bufsiz, const char *attr)
{
	int len;

	if (attr)
		len = snprintf(buf, bufsiz, _PATH_SYS_DEVBLOCK "/%d:%d/%s",
			major(devno), minor(devno), attr);
	else
		len = snprintf(buf, bufsiz, _PATH_SYS_DEVBLOCK "/%d:%d",
			major(devno), minor(devno));

	return (len < 0 || (size_t) len + 1 > bufsiz) ? NULL : buf;
}

int sysfs_devno_has_attribute(dev_t devno, const char *attr)
{
	char path[PATH_MAX];
	struct stat info;

	if (!sysfs_devno_attribute_path(devno, path, sizeof(path), attr))
		return 0;
	if (stat(path, &info) == 0)
		return 1;
	return 0;
}

char *sysfs_devno_path(dev_t devno, char *buf, size_t bufsiz)
{
	return sysfs_devno_attribute_path(devno, buf, bufsiz, NULL);
}

dev_t sysfs_devname_to_devno(const char *name, const char *parent)
{
	char buf[PATH_MAX], *path = NULL;
	dev_t dev = 0;

	if (strncmp("/dev/", name, 5) == 0) {
		/*
		 * Read from /dev
		 */
		struct stat st;

		if (stat(name, &st) == 0)
			dev = st.st_rdev;
		else
			name += 5;	/* unaccesible, or not node in /dev */
	}

	if (!dev && parent) {
		/*
		 * Create path to /sys/block/<parent>/<name>/dev
		 */
		int len = snprintf(buf, sizeof(buf),
				_PATH_SYS_BLOCK "/%s/%s/dev", parent, name);
		if (len < 0 || (size_t) len + 1 > sizeof(buf))
			return 0;
		path = buf;

	} else if (!dev) {
		/*
		 * Create path to /sys/block/<name>/dev
		 */
		int len = snprintf(buf, sizeof(buf),
				_PATH_SYS_BLOCK "/%s/dev", name);
		if (len < 0 || (size_t) len + 1 > sizeof(buf))
			return 0;
		path = buf;
	}

	if (path) {
		/*
		 * read devno from sysfs
		 */
		FILE *f;
		int maj = 0, min = 0;

		f = fopen(path, "r");
		if (!f)
			return 0;

		if (fscanf(f, "%u:%u", &maj, &min) == 2)
			dev = makedev(maj, min);
		fclose(f);
	}
	return dev;
}

/*
 * Returns devname (e.g. "/dev/sda1") for the given devno.
 *
 * Note that the @buf has to be large enough to store /sys/dev/block/<maj:min>
 * symlinks.
 *
 * Please, use more robust blkid_devno_to_devname() in your applications.
 */
char *sysfs_devno_to_devpath(dev_t devno, char *buf, size_t bufsiz)
{
	struct sysfs_cxt cxt;
	char *name;
	size_t sz;
	struct stat st;

	if (sysfs_init(&cxt, devno, NULL))
		return NULL;

	name = sysfs_get_devname(&cxt, buf, bufsiz);
	sysfs_deinit(&cxt);

	if (!name)
		return NULL;

	sz = strlen(name);

	if (sz + sizeof("/dev/") > bufsiz)
		return NULL;

	/* create the final "/dev/<name>" string */
	memmove(buf + 5, name, sz + 1);
	memcpy(buf, "/dev/", 5);

	if (!stat(buf, &st) && S_ISBLK(st.st_mode) && st.st_rdev == devno)
		return buf;

	return NULL;
}

int sysfs_init(struct sysfs_cxt *cxt, dev_t devno, struct sysfs_cxt *parent)
{
	char path[PATH_MAX];
	int fd, rc = 0;

	memset(cxt, 0, sizeof(*cxt));
	cxt->dir_fd = -1;

	if (!sysfs_devno_path(devno, path, sizeof(path)))
		goto err;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		goto err;
	cxt->dir_fd = fd;

	cxt->dir_path = strdup(path);
	if (!cxt->dir_path)
		goto err;
	cxt->devno = devno;
	cxt->parent = parent;
	return 0;
err:
	rc = -errno;
	sysfs_deinit(cxt);
	return rc;
}

void sysfs_deinit(struct sysfs_cxt *cxt)
{
	if (!cxt)
		return;

	if (cxt->dir_fd >= 0)
	       close(cxt->dir_fd);
	free(cxt->dir_path);

	cxt->devno = 0;
	cxt->dir_fd = -1;
	cxt->parent = NULL;
	cxt->dir_path = NULL;
}

int sysfs_stat(struct sysfs_cxt *cxt, const char *attr, struct stat *st)
{
	int rc = fstat_at(cxt->dir_fd, cxt->dir_path, attr, st, 0);

	if (rc != 0 && errno == ENOENT &&
	    strncmp(attr, "queue/", 6) == 0 && cxt->parent) {

		/* Exception for "queue/<attr>". These attributes are available
		 * for parental devices only
		 */
		return fstat_at(cxt->parent->dir_fd,
				cxt->parent->dir_path, attr, st, 0);
	}
	return rc;
}

int sysfs_has_attribute(struct sysfs_cxt *cxt, const char *attr)
{
	struct stat st;

	return sysfs_stat(cxt, attr, &st) == 0;
}

static int sysfs_open(struct sysfs_cxt *cxt, const char *attr)
{
	int fd = open_at(cxt->dir_fd, cxt->dir_path, attr, O_RDONLY);

	if (fd == -1 && errno == ENOENT &&
	    strncmp(attr, "queue/", 6) == 0 && cxt->parent) {

		/* Exception for "queue/<attr>". These attributes are available
		 * for parental devices only
		 */
		fd = open_at(cxt->parent->dir_fd, cxt->dir_path, attr, O_RDONLY);
	}
	return fd;
}

ssize_t sysfs_readlink(struct sysfs_cxt *cxt, const char *attr,
		   char *buf, size_t bufsiz)
{
	if (attr)
		return readlink_at(cxt->dir_fd, cxt->dir_path, attr, buf, bufsiz);

	/* read /sys/dev/block/<maj:min> link */
	return readlink(cxt->dir_path, buf, bufsiz);
}

DIR *sysfs_opendir(struct sysfs_cxt *cxt, const char *attr)
{
	DIR *dir;
	int fd;

	if (attr)
		fd = sysfs_open(cxt, attr);
	else
		/* request to open root of device in sysfs (/sys/block/<dev>)
		 * -- we cannot use cxt->sysfs_fd directly, because closedir()
		 * will close this our persistent file descriptor.
		 */
		fd = dup(cxt->dir_fd);

	if (fd < 0)
		return NULL;

	dir = fdopendir(fd);
	if (!dir) {
		close(fd);
		return NULL;
	}
	if (!attr)
		 rewinddir(dir);
	return dir;
}


static FILE *sysfs_fopen(struct sysfs_cxt *cxt, const char *attr)
{
	int fd = sysfs_open(cxt, attr);

	return fd < 0 ? NULL : fdopen(fd, "r");
}


static struct dirent *xreaddir(DIR *dp)
{
	struct dirent *d;

	while ((d = readdir(dp))) {
		if (!strcmp(d->d_name, ".") ||
		    !strcmp(d->d_name, ".."))
			continue;

		/* blacklist here? */
		break;
	}
	return d;
}

int sysfs_is_partition_dirent(DIR *dir, struct dirent *d, const char *parent_name)
{
	char path[256];

#ifdef _DIRENT_HAVE_D_TYPE
	if (d->d_type != DT_DIR)
		return 0;
#endif
	if (strncmp(parent_name, d->d_name, strlen(parent_name)))
		return 0;

	/* Cannot use /partition file, not supported on old sysfs */
	snprintf(path, sizeof(path), "%s/start", d->d_name);

	return faccessat(dirfd(dir), path, R_OK, 0) == 0;
}

int sysfs_scanf(struct sysfs_cxt *cxt,  const char *attr, const char *fmt, ...)
{
	FILE *f = sysfs_fopen(cxt, attr);
	va_list ap;
	int rc;

	if (!f)
		return -EINVAL;
	va_start(ap, fmt);
	rc = vfscanf(f, fmt, ap);
	va_end(ap);

	fclose(f);
	return rc;
}


int sysfs_read_s64(struct sysfs_cxt *cxt, const char *attr, int64_t *res)
{
	int64_t x = 0;

	if (sysfs_scanf(cxt, attr, "%"SCNd64, &x) == 1) {
		if (res)
			*res = x;
		return 0;
	}
	return -1;
}

int sysfs_read_u64(struct sysfs_cxt *cxt, const char *attr, uint64_t *res)
{
	uint64_t x = 0;

	if (sysfs_scanf(cxt, attr, "%"SCNu64, &x) == 1) {
		if (res)
			*res = x;
		return 0;
	}
	return -1;
}

int sysfs_read_int(struct sysfs_cxt *cxt, const char *attr, int *res)
{
	int x = 0;

	if (sysfs_scanf(cxt, attr, "%d", &x) == 1) {
		if (res)
			*res = x;
		return 0;
	}
	return -1;
}

char *sysfs_strdup(struct sysfs_cxt *cxt, const char *attr)
{
	char buf[1024];
	return sysfs_scanf(cxt, attr, "%1024[^\n]", buf) == 1 ?
						strdup(buf) : NULL;
}

int sysfs_count_dirents(struct sysfs_cxt *cxt, const char *attr)
{
	DIR *dir;
	int r = 0;

	if (!(dir = sysfs_opendir(cxt, attr)))
		return 0;

	while (xreaddir(dir)) r++;

	closedir(dir);
	return r;
}

int sysfs_count_partitions(struct sysfs_cxt *cxt, const char *devname)
{
	DIR *dir;
	struct dirent *d;
	int r = 0;

	if (!(dir = sysfs_opendir(cxt, NULL)))
		return 0;

	while ((d = xreaddir(dir))) {
		if (sysfs_is_partition_dirent(dir, d, devname))
			r++;
	}

	closedir(dir);
	return r;
}

/*
 * Returns slave name if there is only one slave, otherwise returns NULL.
 * The result should be deallocated by free().
 */
char *sysfs_get_slave(struct sysfs_cxt *cxt)
{
	DIR *dir;
	struct dirent *d;
	char *name = NULL;

	if (!(dir = sysfs_opendir(cxt, "slaves")))
		return NULL;

	while ((d = xreaddir(dir))) {
		if (name)
			goto err;	/* more slaves */

		name = strdup(d->d_name);
	}

	closedir(dir);
	return name;
err:
	free(name);
	closedir(dir);
	return NULL;
}

/*
 * Note that the @buf has to be large enough to store /sys/dev/block/<maj:min>
 * symlinks.
 */
char *sysfs_get_devname(struct sysfs_cxt *cxt, char *buf, size_t bufsiz)
{
	char *name = NULL;
	ssize_t sz;

	sz = sysfs_readlink(cxt, NULL, buf, bufsiz - 1);
	if (sz < 0)
		return NULL;

	buf[sz] = '\0';
	name = strrchr(buf, '/');
	if (!name)
		return NULL;

	name++;
	sz = strlen(name);

	memmove(buf, name, sz + 1);
	return buf;
}

#ifdef TEST_PROGRAM_SYSFS
#include <errno.h>
#include <err.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	struct sysfs_cxt cxt;
	char *devname;
	dev_t devno;
	char path[PATH_MAX];
	int i;
	uint64_t u64;
	ssize_t len;

	if (argc != 2)
		errx(EXIT_FAILURE, "usage: %s <devname>", argv[0]);

	devname = argv[1];
	devno = sysfs_devname_to_devno(devname, NULL);

	if (!devno)
		err(EXIT_FAILURE, "failed to read devno");

	printf("NAME: %s\n", devname);
	printf("DEVNO: %u\n", (unsigned int) devno);
	printf("DEVNOPATH: %s\n", sysfs_devno_path(devno, path, sizeof(path)));
	printf("DEVPATH: %s\n", sysfs_devno_to_devpath(devno, path, sizeof(path)));
	printf("PARTITION: %s\n",
		sysfs_devno_has_attribute(devno, "partition") ? "YES" : "NOT");

	sysfs_init(&cxt, devno, NULL);

	len = sysfs_readlink(&cxt, NULL, path, sizeof(path) - 1);
	if (len > 0) {
		path[len] = '\0';
		printf("DEVNOLINK: %s\n", path);
	}

	printf("SLAVES: %d\n", sysfs_count_dirents(&cxt, "slaves"));

	if (sysfs_read_u64(&cxt, "size", &u64))
		printf("read SIZE failed\n");
	else
		printf("SIZE: %jd\n", u64);

	if (sysfs_read_int(&cxt, "queue/hw_sector_size", &i))
		printf("read SECTOR failed\n");
	else
		printf("SECTOR: %d\n", i);

	printf("DEVNAME: %s\n", sysfs_get_devname(&cxt, path, sizeof(path)));

	sysfs_deinit(&cxt);
	return EXIT_SUCCESS;
}
#endif

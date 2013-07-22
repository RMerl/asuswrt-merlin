#ifndef UTIL_LINUX_PARTX_H
#define UTIL_LINUX_PARTX_H

#include <sys/ioctl.h>
#include <linux/blkpg.h>

static inline int partx_del_partition(int fd, int partno)
{
	struct blkpg_ioctl_arg a;
	struct blkpg_partition p;

	p.pno = partno;
	p.start = 0;
	p.length = 0;
	p.devname[0] = 0;
	p.volname[0] = 0;
	a.op = BLKPG_DEL_PARTITION;
	a.flags = 0;
	a.datalen = sizeof(p);
	a.data = &p;

	return ioctl(fd, BLKPG, &a);
}

static inline int partx_add_partition(int fd, int partno,
			unsigned long start, unsigned long size)
{
	struct blkpg_ioctl_arg a;
	struct blkpg_partition p;

	p.pno = partno;
	p.start = start << 9;
	p.length = size << 9;
	p.devname[0] = 0;
	p.volname[0] = 0;
	a.op = BLKPG_ADD_PARTITION;
	a.flags = 0;
	a.datalen = sizeof(p);
	a.data = &p;

	return ioctl(fd, BLKPG, &a);
}

#endif /*  UTIL_LINUX_PARTX_H */

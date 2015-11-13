#include <stdio.h>	     
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include <rtconfig.h>
#include <bcmnvram.h>
#if defined(RTCONFIG_QCA)
#include <mtd/mtd-user.h>
#elif defined(LINUX26)
#include <linux/compiler.h>
#include <mtd/mtd-user.h>
#else
#include <linux/mtd/mtd.h>
#endif

#include "mtd-abi.h"
#include "flash_mtd.h"

#if defined(RTCONFIG_RALINK)
#include "ralink.h"
#elif defined(RTCONFIG_QCA)
#include "qca.h"
#else
#error unknown platform
#endif

#ifdef DEBUG
#define debug(fmt, args...)	printf(fmt, ##args)
#else
#define debug(fmt, args...)	do {} while (0)
#endif

#define MAX_DUMP_LEN	512

#ifndef ROUNDUP
#define	ROUNDUP(x, y)		((((x)+((y)-1))/(y))*(y))
#endif

#ifndef ROUNDDOWN
#define	ROUNDDOWN(x, y)		((x) - (x%y))
#endif


static struct mtd_info info[NUM_INFO];

/**
 * Given MTD partition name, return struct mtd_info.
 * @mtd_name:	MTD partition name, such as "Bootloader", "linux", "Factory", etc
 * @mi:		Pointer to struct mtd_info.
 * 		If return value is greater than or equal to zero,
 * 		mi would be filled with necessary information.
 * @return:
 * 	>= 0:	Success. Return value equal to index of MTD partition.
 *  otherwise:	Fail.
 */
static int get_mtd_info(const char *mtd_name, struct mtd_info *mi)
{
	FILE *fp;
	char line[128], mtd_dev[]="/dev/mtd1XXXXXX";
	int i, ret = -3, sz, esz, r;
	char nm[12], *p;
	mtd_info_t m;

	if (!mtd_name || *mtd_name == '\0' || !mi)
		return -1;

	if (!(fp = fopen("/proc/mtd", "r")))
		return -2;

	memset(mi, 0, sizeof(struct mtd_info));
	fgets(line, sizeof(line), fp); //skip the 1st line
	while (ret < 0 && fgets(line, sizeof(line), fp)) {
		if (sscanf(line, "mtd%d: %x %x \"%s\"", &i, &sz, &esz, nm) != 4)
			continue;

		/* strip tailed " character, if present. */
		if ((p = strchr(nm, '"')) != NULL)
			*p = '\0';
		if (strcmp(mtd_name, nm))
			continue;

		snprintf(mtd_dev, sizeof(mtd_dev), "/dev/mtd%d", i);
		if ((r = open(mtd_dev, O_RDWR|O_SYNC)) < 0)
			continue;

		if (ioctl(r, MEMGETINFO, &m) < 0)
			continue;

		sprintf(mi->dev, "mtd%d", i);
		mi->size = sz;
		mi->erasesize = esz;
		snprintf(mi->name, sizeof(mi->name), "%s", mtd_name);
		mi->type = m.type;
		mi->writesize = m.writesize;
		ret = i;
	}
	fclose(fp);

	return ret;
}

int flash_mtd_init_info(void)
{
	FILE *fp;
	char line[128];
	int i, sz, esz;
	char nm[12];
	int total_sz;

	memset(info, 0, sizeof(info));
	if ((fp = fopen("/proc/mtd", "r"))) {
		fgets(line, sizeof(line), fp); //skip the 1st line
		while (fgets(line, sizeof(line), fp)) {
			if (sscanf(line, "mtd%d: %x %x \"%s\"", &i, &sz, &esz, nm)) {
				if (i >= NUM_INFO)
					printf("please enlarge 'NUM_INFO'\n");
				else {
					sprintf(info[i].dev, "mtd%d", i);
					info[i].size = sz;
					info[i].erasesize = esz;
					nm[strlen((char *)nm)-1] = '\0'; //FIXME: sscanf
					sprintf(info[i].name, "%s", nm);
				}
			}
		}
		fclose(fp);
	}
	else {
		fprintf(stderr, "failed to open /proc/mtd\n");
		return -1;
	}

	total_sz = 0;
	for (i = 0; i < NUM_INFO+1; i++) {
		total_sz += info[i].size;
	}

#ifdef DEBUG
	printf("dev  size     erasesize name\n"); 
	for (i = 0; i < NUM_INFO; i++) {
		if (info[i].dev[0] != 0)
			printf("%s %08x %08x  %s\n", info[i].dev, info[i].size,
					info[i].erasesize, info[i].name);
	}
	printf("total size: %x\n", total_sz);
#endif
	return 0;
}

int flash_mtd_open(int num, int flags)
{
	char dev[10];
	snprintf(dev, sizeof(dev), "/dev/mtd%d", num);
	return open(dev, flags);
}

int flash_mtd_open_name(const char *dev_name, int flags)
{
	char dev[10];

	if (!dev_name || *dev_name == '\0')
		return -1;

	snprintf(dev, sizeof(dev), "/dev/%s", dev_name);
	return open(dev, flags);
}

int flash_mtd_read(int offset, int count)
{
	int i, o, off, cnt, addr, fd, len;
	unsigned char *buf, *p;

	debug("%s: offset %x, count %d\n", __func__, offset, count);
	buf = (unsigned char *)malloc(count);
	if (buf == NULL) {
		fprintf(stderr, "fail to alloc memory for %d bytes\n", count);
		return -1;
	}
	p = buf;
	cnt = count;
	off = offset;

	for (i = 0, addr = 0; i < NUM_INFO; i++) {
		if (addr <= off && off < addr + info[i].size) {
			o = off - addr;
			fd = flash_mtd_open(i, O_RDONLY | O_SYNC);
			if (fd < 0) {
				fprintf(stderr, "failed to open mtd%d\n", i);
				free(buf);
				return -1;
			}
			lseek(fd, o, SEEK_SET);
			len = ((o + cnt) < info[i].size)? cnt : (info[i].size - o);
			debug("  read from mtd%d: o %x, len %d\n", i, o, len);
			read(fd, p, len);
			close(fd);
			cnt -= len;
			if (cnt == 0)
				break;
			off += len;
			p += len;
		}
		addr += info[i].size;
	}
	for (i = 0, p = buf; i < count; i++, p++) {
#if 1 //backward compatibility
		printf("%X: %X\n", offset + i, *p);
#else
		printf("%02x", *p);
		if (i % 2 == 1) printf(" ");
#endif
	}
	free(buf);
	return 0;
}

/**
 * Read data from specificed partition.
 * @mtd_name:	MTD partition name. such as "Bootloader", "Factory", etc
 * @offset:	offset in MTD partition. (start from 0)
 * @buf:
 * @count:
 * @return:
 * 	0:	Successful
 * 	-1:	Invalid parameter or error occurs.
 * 	-2:	Out of Factory partition.
 * 	-3:	Open MTD partition fail.
 */
int MTDPartitionRead(const char *mtd_name, const unsigned char *buf, int offset, int count)
{
	int cnt, fd, ret;
	unsigned char *p;
	struct mtd_info info, *mi = &info;

	if (!mtd_name || *mtd_name == '\0' || !buf || offset < 0 || count <= 0 || get_mtd_info(mtd_name, mi) < 0)
		return -1;
	if ((offset + count) > mi->size) {
		fprintf(stderr, "%s: Out of Factory partition. (offset 0x%x count 0x%x)\n",
			__func__, offset, count);
		return -2;
	}

	if ((fd = flash_mtd_open_name(mi->dev, O_RDONLY | O_SYNC)) < 0) {
		fprintf(stderr, "failed to open %s. (errno %d (%s))\n",
			mi->dev, errno, strerror(errno));
		return -3;
	}

	debug("%s: %s offset %x, count %d\n", __func__, mtd_name, offset, count);
	lseek(fd, offset, SEEK_SET);
	p = (unsigned char*) buf;
	cnt = count;
	while (cnt > 0) {
		ret = read(fd, p, cnt);
		if (ret > 0) {
			cnt -= ret;
			p += ret;
			continue;
		}
		fprintf(stderr, "%s: Read fail! (offset 0x%x, count 0x%x, cnt 0x%x, errno %d (%s))\n",
			__func__, offset, count, cnt, errno, strerror(errno));
		break;
	}
	close(fd);
	return 0;
}

#if defined(RTCONFIG_QCA) && ( defined(RTCONFIG_WIFI_QCA9557_QCA9882) || defined(RTCONFIG_QCA953X) || defined(RTCONFIG_QCA956X))
int VVPartitionRead(const char *mtd_name, const unsigned char *buf, int offset, int count)
{
	int cnt, fd, ret;
	unsigned char *p;
	struct mtd_info info, *mi = &info;
	char cmd[1024];
	char *tmpfile="/tmp/cal";

	if (!mtd_name || *mtd_name == '\0' || !buf || offset < 0 || count <= 0 || get_mtd_info(mtd_name, mi) < 0)
		return -1;
	if ((offset + count) > mi->size) {
		fprintf(stderr, "%s: Out of Factory partition. (offset 0x%x count 0x%x)\n",
			__func__, offset, count);
		return -2;
	}

	sprintf(cmd,"cat /dev/%s > %s", mi->dev, tmpfile);
	system(cmd);
	if ((fd = open(tmpfile, O_RDONLY)) < 0) {
		fprintf(stderr, "failed to open %s. (errno %d (%s))\n",
			tmpfile, errno, strerror(errno));
		return -3;
	}

	debug("%s: %s offset %x, count %d\n", __func__, mtd_name, offset, count);
	lseek(fd, offset, SEEK_SET);
	p = (unsigned char*) buf;
	cnt = count;
	while (cnt > 0) {
		ret = read(fd, p, cnt);
		if (ret > 0) {
			cnt -= ret;
			p += ret;
			continue;
		}
		fprintf(stderr, "%s: Read fail! (offset 0x%x, count 0x%x, cnt 0x%x, errno %d (%s))\n",
			__func__, offset, count, cnt, errno, strerror(errno));
		break;
	}
	close(fd);
	unlink(tmpfile);
	return 0;
}

int CalRead(const unsigned char *buf, int offset, int count)
{
	/// TBD. MTDPartitionRead will fail...
	return VVPartitionRead(CALDATA_MTD_NAME, buf, offset, count);
}
#endif	/* RTCONFIG_QCA && RTCONFIG_WIFI_QCA9557_QCA9882 */

/**
 * Read data from Factory partition.
 * @offset:	offset in Factory partition. (start from 0; NOT 0x40000)
 * @return:
 * 	0:	Successful
 */
int FactoryRead(const unsigned char *buf, int offset, int count)
{
	return MTDPartitionRead(FACTORY_MTD_NAME, buf, offset, count);
}

/**
 * Read data from linux partition.
 * @offset:	offset in Factory partition. (start from 0; NOT 0x50000)
 * @return:
 * 	0:	Successful
 */
int linuxRead(const unsigned char *buf, int offset, int count)
{
	return MTDPartitionRead(LINUX_MTD_NAME, buf, offset, count);
}

/**
 * Equal to original FRead().
 * Don't use this function to read data from Factory partition directly.
 * Call FRead() with old absolute address of flash or call FactoryRead()
 * with relative address regards to Factory partition instead.
 */
int FlashRead(const unsigned char *dst, int src, int count)
{
	int i, o, off, cnt, addr, fd, len;
	unsigned char *buf, *p;

	if (flash_mtd_init_info())
		return -1;

	debug("%s: src %x, count %d\n", __func__, src, count);
	buf = malloc(count);
	if (buf == NULL) {
		fprintf(stderr, "fail to alloc memory for %d bytes\n", count);
		return -1;
	}
	p = buf;
	cnt = count;
	off = src;

	for (i = 0, addr = 0; i < NUM_INFO; i++) {
		if (addr <= off && off < addr + info[i].size) {
			o = off - addr;
			fd = flash_mtd_open(i, O_RDONLY | O_SYNC);
			if (fd < 0) {
				fprintf(stderr, "failed to open mtd%d\n", i);
				free(buf);
				return -1;
			}
			lseek(fd, o, SEEK_SET);
			len = ((o + cnt) < info[i].size)? cnt : (info[i].size - o);
			debug("  read from mtd%d: o %x, len %d\n", i, o, len);
			read(fd, p, len);
			close(fd);
			cnt -= len;
			if (cnt == 0)
				break;
			off += len;
			p += len;
		}
		addr += info[i].size;
	}

	memcpy((void*) dst, buf, count);
	free(buf);
	return 0;
}

/**
 * Read data from Factory partition of Flash.
 * Backward compatibility to origin caller and convention.
 * 1. If you want to read data from Factory, converts absolute address of flash to
 *    relative address regards to Factory partition and call FactoryRead() instead.
 * 2. If you want to read raw data from anywhere in flash, call FlashRead() instead.
 * 3. If you read data not in 0x40000~0x4FFFF, this function print warning message
 *    and call FlashRead() instead.
 */
int FRead(const unsigned char *buf, int addr, int count)
{
	if (!buf || addr < 0 || count <=0)
		return -1;

	/* If address fall in old Factory partition, call FactoryRead() instead. */
	if (addr >= OFFSET_MTD_FACTORY &&
	    (addr + count) <= (OFFSET_MTD_FACTORY + SPI_PARALLEL_NOR_FLASH_FACTORY_LENGTH)) {
		return FactoryRead(buf, addr - OFFSET_MTD_FACTORY, count);
	}

	fprintf(stderr, "%s: Read data out of factory region or cross old factory boundary. (addr 0x%x count 0x%x)\n",
		__func__, addr, count);
	return FlashRead(buf, addr, count);
}

int flash_mtd_write(int offset, int value)
{
	int i, o, fd, off, addr, sz;
	unsigned char *buf;
	struct erase_info_user ei;

	debug("%s: offset %x, value %x\n", __func__, offset, (unsigned char)value);
	off = offset;

	for (i = 0, addr = 0; i < NUM_INFO; i++) {
		if (addr <= off && off < addr + info[i].size) {
			sz = info[i].erasesize;
			buf = (unsigned char *)malloc(sz);
			if (buf == NULL) {
				fprintf(stderr, "failed to alloc memory for %d bytes\n",
						sz);
				return -1;
			}
			fd = flash_mtd_open(i, O_RDWR | O_SYNC);
			if (fd < 0) {
				fprintf(stderr, "failed to open mtd%d\n", i);
				free(buf);
				return -1;
			}
			off -= addr;
			o = (off / sz) * sz;
			lseek(fd, o, SEEK_SET);
			debug("  backup mtd%d, o %x(off %x), len %x\n", i, o, off, sz);
			//backup
			if (read(fd, buf, sz) != sz) {
				fprintf(stderr, "failed to read %d bytes from mtd%d\n",
						sz, i);
				free(buf);
				close(fd);
				return -1;
			}
			//erase
			ei.start = o;
			ei.length = sz;
			if (ioctl(fd, MEMERASE, &ei) < 0) {
				fprintf(stderr, "failed to erase mtd%d\n", i);
				free(buf);
				close(fd);
				return -1;
			}
			//write
			lseek(fd, o, SEEK_SET);
			debug("  buf[%x] = %x\n", off - o, (unsigned char)value);
			*(buf + (off - o)) = (unsigned char)value;
			if (write(fd, buf, sz) == -1) {
				fprintf(stderr, "failed to write mtd%d\n", i);
				free(buf);
				close(fd);
				return -1;
			}
			free(buf);
			close(fd);
			break;
		}
		addr += info[i].size;
	}
	printf("Write %0X to %0X\n", (unsigned char)value, offset);
	return 0;
}

/**
 * Write data to Factory partition.
 * @mtd_name:	MTD partition name. such as "Bootloader", "Factory", etc
 * @buf:
 * @offset:	offset in Factory partition. (start from 0; NOT 0x40000)
 * @count:
 * @return:
 * 	0:	Successful
 * 	-1:	Invalid parameter or error occurs.
 * 	-2:	Out of Factory partition.
 * 	-3:	Open MTD partition fail.
 * 	-4:	Malloc buffer for erase procedure fail.
 * 	-5:	Aligned offset and length exceeds MTD partition.
 * 	-6:	Malloc buffer for alignment offset and/or length to write size fail.
 * 	-7:	Read data for alignment data fail.
 */
int MTDPartitionWrite(const char *mtd_name, const unsigned char *buf, int offset, int count)
{
	int fd, o, off, cnt, ret = 0, len;
	unsigned char *tmp;
	const unsigned char *p = buf;
	unsigned char *tmp_buf = NULL;
	struct erase_info_user ei;
	struct mtd_info info, *mi = &info;
	int old_offset, old_count;

	if (!mtd_name || *mtd_name == '\0' || !buf || offset < 0 || count <= 0 || get_mtd_info(mtd_name, mi) < 0)
		return -1;

	if ((offset + count) > mi->size) {
		fprintf(stderr, "%s: Out of %s partition. (offset 0x%x count 0x%x)\n",
			__func__, mi->name, offset, count);
		return -2;
	}

	/* Align offset and length to write size of UBI volume backed MTD device. */
	old_offset = offset;
	old_count = count;
	if (mi->type == MTD_UBIVOLUME) {
		int e = ROUNDUP(offset + count, mi->writesize), o;

		offset = ROUNDDOWN(offset, mi->writesize);
		count = e - offset;
		debug("%s: offset %x -> %x, count %x -> %x, mtd->writesize %x\n",
			__func__, old_offset, offset, old_count, count, mi->writesize);
		if ((offset + count) > mi->size) {
			fprintf(stderr, "%s: Out of %s partition. (aligned offset 0x%x count 0x%x)\n",
				__func__, mi->name, offset, count);
			return -5;
		}

		if (old_offset != offset || old_count != count) {
			tmp_buf = malloc(count);
			if (!tmp_buf) {
				fprintf(stderr, "%s: allocate buffer fail!\n", __func__);
				return -6;
			}
			if (MTDPartitionRead(mtd_name, tmp_buf, offset, count)) {
				fprintf(stderr, "%s: read data fail!\n", __func__);
				return -7;
			}

			o = old_offset - offset;
			memcpy(tmp_buf + o, buf, old_count);
			p = tmp_buf;
		}
	}

#ifdef DEBUG
	debug("%s: %s offset %x, buf string", __func__, mtd_name, offset);
	{
		int i, len = (count > MAX_DUMP_LEN)? MAX_DUMP_LEN:count;

		for (i = 0; i < len; i++)
			debug(" %x", (unsigned char) *(buf + i));
		debug("%s\n", (len == count)? "":"...");
	}
#endif

	fd = flash_mtd_open_name(mi->dev, O_RDWR | O_SYNC);
	if (fd < 0) {
		fprintf(stderr, "%s: failed to open %s. (errno %d (%s))\n",
			__func__, mi->dev, errno, strerror(errno));
		if (tmp_buf)
			free(tmp_buf);
		return -3;
	}

	tmp = malloc(mi->erasesize);
	if (!tmp) {
		fprintf(stderr, "%s: failed to alloc memory for %d bytes\n",
			__func__, mi->erasesize);
		if (tmp_buf)
			free(tmp_buf);
		return -4;
	}
	off = offset;
	cnt = count;
	while (cnt > 0) {
		o = ROUNDDOWN(off, mi->erasesize);	/* aligned to erase boundary */
		len = mi->erasesize - (off - o);
		if (cnt < len)
			len = cnt;
		lseek(fd, o, SEEK_SET);
		debug("  backup %s, o %x(off %x), len %x\n",
			mi->dev, o, off, mi->erasesize);
		//backup
		if (read(fd, tmp, mi->erasesize) != mi->erasesize) {
			fprintf(stderr, "%s: failed to read %d bytes from %s\n",
				__func__, mi->erasesize, mi->dev);
			ret = -5;
			break;
		}
		//erase
		ei.start = o;
		ei.length = mi->erasesize;
		if (ioctl(fd, MEMERASE, &ei) < 0) {
			fprintf(stderr, "%s: failed to erase %s start 0x%x length 0x%x. errno %d (%s)\n",
				__func__, mi->dev, ei.start, ei.length, errno, strerror(errno));
			ret = -6;
			break;
		}
		//write
		lseek(fd, o, SEEK_SET);
#ifdef DEBUG
		{
			int i, len = (count > MAX_DUMP_LEN)? MAX_DUMP_LEN:count;

			for (i = 0; i < len; i++)
				debug("  tmp[%x] = %x\n", off - o + i, (unsigned char)*(buf + i));
			debug("%s\n", (len == count)? "":"...");
		}
#endif
		memcpy(tmp + (off - o), p, len);
		if (write(fd, tmp, mi->erasesize) != mi->erasesize) {
			fprintf(stderr, "%s: failed to write %s\n",
				__func__, mi->dev);
			ret = -7;
			break;
		}

		p += len;
		off += len;
		cnt -= len;
	}
	free(tmp);
	free(tmp_buf);

	if (!cnt) {
		tmp = (unsigned char *)malloc(count);
		MTDPartitionRead(mtd_name, tmp, offset, count);
		free(tmp);
	}

	return ret;
}

/**
 * Write data to Factory partition.
 * @offset:	offset in Factory partition. (start from 0; NOT 0x40000)
 * @return:
 * 	0:	Successful
 */
int FactoryWrite(const unsigned char *buf, int offset, int count)
{
	return MTDPartitionWrite(FACTORY_MTD_NAME, buf, offset, count);
}

/**
 * Equal to original FWrite().
 * Don't use this function to write data to Factory partition directly.
 * Call FWrite() with old absolute address of flash or call FactoryRead()
 * with relative address regards to Factory partition instead.
 */
int FlashWrite(const unsigned char *src, int offset, int count)
{
	int i, o, fd, off, addr, sz;
	unsigned char *buf;
	struct erase_info_user ei;

	if (flash_mtd_init_info())
		return -1;

#ifdef DEBUG
	debug("%s: offset %x, src string", __func__, offset);
	for (i = 0; i < count; i++)
		debug(" %x", (unsigned char) *(src + i));
	debug("\n");
#endif
	off = offset;

	for (i = 0, addr = 0; i < NUM_INFO; i++) {
		if (addr <= off && off < addr + info[i].size) {
			sz = info[i].erasesize;
			buf = (unsigned char *)malloc(sz);
			if (buf == NULL) {
				fprintf(stderr, "failed to alloc memory for %d bytes\n",
						sz);
				return -1;
			}
			fd = flash_mtd_open(i, O_RDWR | O_SYNC);
			if (fd < 0) {
				fprintf(stderr, "failed to open mtd%d\n", i);
				free(buf);
				return -1;
			}
			off -= addr;
			o = (off / sz) * sz;
			lseek(fd, o, SEEK_SET);
			debug("  backup mtd%d, o %x(off %x), len %x\n", i, o, off, sz);
			//backup
			if (read(fd, buf, sz) != sz) {
				fprintf(stderr, "failed to read %d bytes from mtd%d\n",
						sz, i);
				free(buf);
				close(fd);
				return -1;
			}
			//erase
			ei.start = o;
			ei.length = sz;
			if (ioctl(fd, MEMERASE, &ei) < 0) {
				fprintf(stderr, "failed to erase mtd%d\n", i);
				free(buf);
				close(fd);
				return -1;
			}
			//write
			lseek(fd, o, SEEK_SET);
#ifdef DEBUG
			for (i = 0; i < count; i++)
				debug("  buf[%x] = %x\n", off - o + i, (unsigned char)*(src + i));
			debug("\n");
#endif
			memcpy(buf + (off - o), src, count);
			if (write(fd, buf, sz) == -1) {
				fprintf(stderr, "failed to write mtd%d\n", i);
				free(buf);
				close(fd);
				return -1;
			}
			free(buf);
			close(fd);
			break;
		}
		addr += info[i].size;
	}
	buf = (unsigned char *)malloc(count);
	FRead(buf, offset, count);
	free(buf);
	return 0;
}

/**
 * Write data to Factory partition of Flash.
 * Backward compatibility to origin caller and convention.
 * 1. If you want to read data from Factory, converts absolute address of flash to
 *    relative address regards to Factory partition and call FactoryWrite() instead.
 * 2. If you want to read raw data from anywhere in flash, call FlashWrite() instead.
 * 3. If you read data not in 0x40000~0x4FFFF, this function print warning message
 *    and call FlashWrite() instead.
 */
int FWrite(const unsigned char *buf, int addr, int count)
{
	if (!buf || addr <= 0 || count <=0)
		return -1;

	/* If address fall in old Factory partition, call FactoryRead() instead. */
	if (addr >= OFFSET_MTD_FACTORY &&
	    (addr + count) <= (OFFSET_MTD_FACTORY + SPI_PARALLEL_NOR_FLASH_FACTORY_LENGTH)) {
		return FactoryWrite(buf, addr - OFFSET_MTD_FACTORY, count);
	}

	fprintf(stderr, "%s: Write data out of factory region or cross old factory boundary. (addr 0x%x count 0x%x)\n",
		__func__, addr, count);
	return FlashWrite(buf, addr, count);
}

int flash_mtd_erase(int start, int end)
{
	int i, addr, fd;
	struct erase_info_user ei;

	debug("%s: start %x, end %x\n", __func__, start, end);
	for (i = 0, addr = 0; i < NUM_INFO; i++) {
		if (addr <= start && start < addr + info[i].size) {
			if (end < start || end >= addr + info[i].size) {
				fprintf(stderr, "not support\n");
				return -1;
			}
			fd = flash_mtd_open(i, O_RDWR | O_SYNC);
			if (fd < 0) {
				fprintf(stderr, "failed to open mtd%d\n", i);
				return -1;
			}
			ei.start = start - addr;
			ei.length = end - start + 1;
			debug("  erase mtd%d, start %x, len %x\n", i, ei.start, ei.length);
			if (ioctl(fd, MEMERASE, &ei) < 0) {
				fprintf(stderr, "failed to erase mtd%d\n", i);
				close(fd);
				return -1;
			}
			close(fd);
			break;
		}
		addr += info[i].size;
	}
	printf("Erase Addr From %0X To %0X\n", start, end);
	return 0;
}

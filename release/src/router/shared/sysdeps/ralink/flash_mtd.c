#include <stdio.h>	     
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <bcmnvram.h>

#include "mtd-abi.h"
#include "flash_mtd.h"

#if 0
#define DEBUG 1
#endif

struct mtd_info info[NUM_INFO];

#if 0
void flash_mtd_usage(char *cmd)
{
	printf("Usage:\n");
	printf("  %s -r <offset> -c <count> - read <count> bytes from <offset>\n", cmd);
	printf("  %s -w <offset> -o <value> - write <offset> with <value>\n", cmd);
	printf("  %s -f <start> -l <end>    - erase from <start> to <end>\n", cmd);
	printf("Note:\n");
	printf("  <count> is decimal, the other parameters are hexadecimal\n");
	exit(EXIT_SUCCESS);
}
#endif

int flash_mtd_init_info(void)
{
	FILE *fp;
	char line[128];
	int i, sz, esz, nm[12];
	int total_sz;

	memset(info, 0, sizeof(info));
	if ((fp = fopen("/proc/mtd", "r"))) {
		fgets(line, sizeof(line), fp); //skip the 1st line
		while (fgets(line, sizeof(line), fp)) {
			if (sscanf(line, "mtd%d: %x %x \"%s\"", &i, &sz, &esz,
						nm)) {
				if (i >= NUM_INFO)
					printf("please enlarge 'NUM_INFO'\n");
				else {
					sprintf(info[i].dev, "mtd%d", i);
					info[i].size = sz;
					info[i].erasesize = esz;
					nm[strlen((char *)nm)-2] = '\0'; //FIXME: sscanf
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

int flash_mtd_read(int offset, int count)
{
	int i, o, off, cnt, addr, fd, len;
	unsigned char *buf, *p;

#ifdef DEBUG
	printf("%s: offset %x, count %d\n", __func__, offset, count);
#endif
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
#ifdef DEBUG
			printf("  read from mtd%d: o %x, len %d\n", i, o, len);
#endif
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

int FRead(char *dst, int src, int count)
{
	int i, o, off, cnt, addr, fd, len;
	unsigned char *buf, *p;

	if (flash_mtd_init_info())
		return -1;

#ifdef DEBUG
	printf("%s: src %x, count %d\n", __func__, src, count);
#endif
	buf = (unsigned char *)malloc(count);
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
#ifdef DEBUG
			printf("  read from mtd%d: o %x, len %d\n", i, o, len);
#endif
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

	memcpy(dst, buf, count);

#ifdef DEBUG
	for (i = 0, p = buf; i < count; i++, p++) {
#if 1 //backward compatibility
		printf("%X: %X\n", src + i, *p);
#else
		printf("%02x", *p);
		if (i % 2 == 1) printf(" ");
#endif
	}
#endif
	free(buf);
	return 0;
}

int flash_mtd_write(int offset, int value)
{
	int i, o, fd, off, addr, sz;
	unsigned char *buf;
	struct erase_info_user ei;

#ifdef DEBUG
	printf("%s: offset %x, value %x\n", __func__, offset, (unsigned char)value);
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
#ifdef DEBUG
			printf("  backup mtd%d, o %x(off %x), len %x\n", i, o, off, sz);
#endif
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
			printf("  buf[%x] = %x\n", off - o, (unsigned char)value);
#endif
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

int FWrite(char *src, int offset, int count)
{
	int i, o, fd, off, addr, sz;
	unsigned char *buf;
	struct erase_info_user ei;

	if (flash_mtd_init_info())
		return -1;

#ifdef DEBUG
	printf("%s: offset %x, src string", __func__, offset);
	for (i = 0; i < count; i++)
		printf(" %x", (unsigned char) *(src + i));
	printf("\n");
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
#ifdef DEBUG
			printf("  backup mtd%d, o %x(off %x), len %x\n", i, o, off, sz);
#endif
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
				printf("  buf[%x] = %x\n", off - o + i, (unsigned char)*(src + i));
			printf("\n");
#endif
//			*(buf + (off - o)) = (unsigned char)value;
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

#if 0
int FWrite(char *src, int dst, int count)
{
	int i, ret;

	if (flash_mtd_init_info())
		return -1;

	for (i = 0; i < count ; i++)
	{
		fprintf(stderr, "%d: %x\n", i, *(src + i));
		ret = flash_mtd_write(dst + i, *(src + i));

		if (ret == -1)
			return -1;
	}

	return 0;
}
#endif

int flash_mtd_erase(int start, int end)
{
	int i, addr, fd;
	struct erase_info_user ei;
#ifdef DEBUG
	printf("%s: start %x, end %x\n", __func__, start, end);
#endif

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
#ifdef DEBUG
			printf("  erase mtd%d, start %x, len %x\n", i, ei.start,
					ei.length);
#endif
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

#if 0
int flash_mtd_main(int argc, char *argv[])
{
	int opt;
	char method = '0';
	int offset = 0, count = 0, value = -1, end = 0;

	if (argc < 3)
		flash_mtd_usage(argv[0]);
	if (flash_mtd_init_info())
		exit(EXIT_FAILURE);

	while ((opt = getopt (argc, argv, "r:w:f:l:o:c:")) != -1) {
		switch (opt) {
			case 'r':
				offset = strtol(optarg, NULL, 16);
				method = 'r';
				break;
			case 'w':
				offset = strtol(optarg, NULL, 16);
				method = 'w';
				break;
			case 'f':
				offset = strtol(optarg, NULL, 16);
				method = 'e';
				break;
			case 'l':
				end = strtol(optarg, NULL, 16);
				break;
			case 'c':
				count = strtol(optarg, NULL, 10);
				if (count > MAX_READ_CNT) {
					fprintf(stderr, "MAX %d bytes\n",
							MAX_READ_CNT);
					return 0;
				}
				break;
			case 'o':
				value = strtol(optarg, NULL, 16);
				break;
			default:
				flash_mtd_usage(argv[0]);
		}
	}

	switch (method) {
		case 'r':
			if (count == 0)
				break;
			flash_mtd_read(offset, count);
			return 0;
		case 'w':
			if (value == -1)
				break;
			flash_mtd_write(offset, value);
			return 0;
		case 'e':
			if (end == 0)
				break;
			flash_mtd_erase(offset, end);
			return 0;
	}
	flash_mtd_usage(argv[0]);
}
#endif

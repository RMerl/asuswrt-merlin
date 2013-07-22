/*
 * flash_otp_write.c -- write One-Time-Program data
 */

#define PROGRAM_NAME "flash_otp_write"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <mtd/mtd-user.h>

int main(int argc,char *argv[])
{
	int fd, val, ret, size, wrote, len;
	mtd_info_t mtdInfo;
	off_t offset;
	char *p, buf[2048];

	if (argc != 4 || strcmp(argv[1], "-u")) {
		fprintf(stderr, "Usage: %s -u <device> <offset>\n", PROGRAM_NAME);
		fprintf(stderr, "the raw data to write should be provided on stdin\n");
		fprintf(stderr, "CAUTION! ONCE SET TO 0, OTP DATA BITS CAN'T BE ERASED!\n");
		return EINVAL;
	}

	fd = open(argv[2], O_WRONLY);
	if (fd < 0) {
		perror(argv[2]);
		return errno;
	}

	val = MTD_OTP_USER;
	ret = ioctl(fd, OTPSELECT, &val);
	if (ret < 0) {
		perror("OTPSELECT");
		return errno;
	}

	if (ioctl(fd, MEMGETINFO, &mtdInfo)) {
		perror("MEMGETINFO");
		return errno;
	}

	offset = strtoul(argv[3], &p, 0);
	if (argv[3][0] == 0 || *p != 0) {
		fprintf(stderr, "%s: bad offset value\n", PROGRAM_NAME);
		return ERANGE;
	}

	if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
		perror("lseek()");
		return errno;
	}

	printf("Writing OTP user data on %s at offset 0x%lx\n", argv[2], offset);

	if (mtdInfo.type == MTD_NANDFLASH)
		len = mtdInfo.writesize;
	else
		len = 256;

	wrote = 0;
	while ((size = read(0, buf, len))) {
		if (size < 0) {
			perror("read()");
			return errno;
		}
		p = buf;
		while (size > 0) {
			if (mtdInfo.type == MTD_NANDFLASH) {
				/* Fill remain buffers with 0xff */
				memset(buf + size, 0xff, mtdInfo.writesize - size);
				size = mtdInfo.writesize;
			}
			ret = write(fd, p, size);
			if (ret < 0) {
				perror("write()");
				return errno;
			}
			if (ret == 0) {
				printf("write() returned 0 after writing %d bytes\n", wrote);
				return 0;
			}
			p += ret;
			wrote += ret;
			size -= ret;
		}
	}

	printf("Wrote %d bytes of OTP user data\n", wrote);
	return 0;
}

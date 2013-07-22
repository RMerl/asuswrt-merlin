/*
 * flash_otp_lock.c -- lock area of One-Time-Program data
 */

#define PROGRAM_NAME "flash_otp_lock"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <mtd/mtd-user.h>

int main(int argc,char *argv[])
{
	int fd, val, ret, offset, size;
	char *p, buf[8];

	if (argc != 5 || strcmp(argv[1], "-u")) {
		fprintf(stderr, "Usage: %s -u <device> <offset> <size>\n", PROGRAM_NAME);
		fprintf(stderr, "offset and size must match on OTP region boundaries\n");
		fprintf(stderr, "CAUTION! ONCE LOCKED, OTP REGIONS CAN'T BE UNLOCKED!\n");
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

	offset = strtoul(argv[3], &p, 0);
	if (argv[3][0] == 0 || *p != 0) {
		fprintf(stderr, "%s: bad offset value\n", PROGRAM_NAME);
		return ERANGE;
	}

	size = strtoul(argv[4], &p, 0);
	if (argv[4][0] == 0 || *p != 0) {
		fprintf(stderr, "%s: bad size value\n", PROGRAM_NAME);
		return ERANGE;
	}

	printf("About to lock OTP user data on %s from 0x%x to 0x%x\n",
			argv[2], offset, offset + size);
	printf("Are you sure (yes|no)? ");
	if (fgets(buf, sizeof(buf), stdin) && strcmp(buf, "yes\n") == 0) {
		struct otp_info info;
		info.start = offset;
		info.length = size;
		ret = ioctl(fd, OTPLOCK, &info);
		if (ret	< 0) {
			perror("OTPLOCK");
			return errno;
		}
		printf("Done.\n");
	} else {
		printf("Aborted\n");
	}

	return 0;
}

/*
 * flash_otp_info.c -- print info about One-Time-Programm data
 */

#define PROGRAM_NAME "flash_otp_info"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <mtd/mtd-user.h>

int main(int argc,char *argv[])
{
	int fd, val, i, ret;

	if (argc != 3 || (strcmp(argv[1], "-f") && strcmp(argv[1], "-u"))) {
		fprintf(stderr,"Usage: %s [ -f | -u ] <device>\n", PROGRAM_NAME);
		return EINVAL;
	}

	fd = open(argv[2], O_RDONLY);
	if (fd < 0) {
		perror(argv[2]);
		return errno;
	}

	val = argv[1][1] == 'f' ? MTD_OTP_FACTORY : MTD_OTP_USER;
	ret = ioctl(fd, OTPSELECT, &val);
	if (ret < 0) {
		perror("OTPSELECT");
		return errno;
	}

	ret = ioctl(fd, OTPGETREGIONCOUNT, &val);
	if (ret < 0) {
		perror("OTPGETREGIONCOUNT");
		return errno;
	}

	printf("Number of OTP %s blocks on %s: %d\n",
			argv[1][1] == 'f' ? "factory" : "user", argv[2], val);

	if (val > 0) {
		struct otp_info info[val];

		ret = ioctl(fd, OTPGETREGIONINFO, &info);
		if (ret	< 0) {
			perror("OTPGETREGIONCOUNT");
			return errno;
		}

		for (i = 0; i < val; i++)
			printf("block %2d:  offset = 0x%04x  "
					"size = %2d bytes  %s\n",
					i, info[i].start, info[i].length,
					info[i].locked ? "[locked]" : "[unlocked]");
	}

	close(fd);
	return 0;
}

/* Ported to busybox from mtd-utils.
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

//applet:IF_UBIATTACH(APPLET_ODDNAME(ubiattach, ubi_attach_detach, _BB_DIR_USR_SBIN, _BB_SUID_DROP, ubiattach))
//applet:IF_UBIDETACH(APPLET_ODDNAME(ubidetach, ubi_attach_detach, _BB_DIR_USR_SBIN, _BB_SUID_DROP, ubidetach))

//kbuild:lib-$(CONFIG_UBIATTACH)   += ubi_attach_detach.o
//kbuild:lib-$(CONFIG_UBIDETACH)   += ubi_attach_detach.o

//config:config UBIATTACH
//config:	bool "ubiattach"
//config:	default n
//config:	help
//config:	  Attach MTD device to an UBI device.
//config:
//config:config UBIDETACH
//config:	bool "ubidetach"
//config:	default n
//config:	help
//config:	  Detach MTD device from an UBI device.

#include "libbb.h"
#include <mtd/ubi-user.h>

#define OPTION_M	(1 << 0)
#define OPTION_D	(1 << 1)

#define do_attach (ENABLE_UBIATTACH && \
		(!ENABLE_UBIDETACH || (applet_name[3] == 'a')))

//usage:#define ubiattach_trivial_usage
//usage:       "-m MTD_NUM [-d UBI_NUM] UBI_CTRL_DEV"
//usage:#define ubiattach_full_usage "\n\n"
//usage:       "Attach MTD device to UBI\n"
//usage:     "\nOptions:"
//usage:     "\n	-m MTD_NUM	MTD device number to attach"
//usage:     "\n	-d UBI_NUM	UBI device number to assign"
//usage:
//usage:#define ubidetach_trivial_usage
//usage:       "-d UBI_NUM UBI_CTRL_DEV"
//usage:#define ubidetach_full_usage "\n\n"
//usage:       "Detach MTD device from UBI\n"
//usage:     "\nOptions:"
//usage:     "\n	-d UBI_NUM	UBI device number"

int ubi_attach_detach_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ubi_attach_detach_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opts;
	char *ubi_ctrl;
	//struct stat st;
	struct ubi_attach_req req;
	int fd;
	int mtd_num;
	int dev_num = UBI_DEV_NUM_AUTO;

	opt_complementary = "=1:m+:d+";
	opts = getopt32(argv, "m:d:", &mtd_num, &dev_num);
	ubi_ctrl = argv[optind];

	fd = xopen(ubi_ctrl, O_RDWR);
	//fstat(fd, &st);
	//if (!S_ISCHR(st.st_mode))
	//	bb_error_msg_and_die("'%s' is not a char device", ubi_ctrl);

	if (do_attach) {
		if (!(opts & OPTION_M))
			bb_error_msg_and_die("%s device not specified", "MTD");

		memset(&req, 0, sizeof(req));
		req.mtd_num = mtd_num;
		req.ubi_num = dev_num;

		xioctl(fd, UBI_IOCATT, &req);
	} else { /* detach */
		if (!(opts & OPTION_D))
			bb_error_msg_and_die("%s device not specified", "UBI");

		xioctl(fd, UBI_IOCDET, &dev_num);
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);

	return EXIT_SUCCESS;
}

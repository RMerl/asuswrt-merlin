/* Ported to busybox from mtd-utils.
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//config:config UBIATTACH
//config:	bool "ubiattach"
//config:	default y
//config:	select PLATFORM_LINUX
//config:	help
//config:	  Attach MTD device to an UBI device.
//config:
//config:config UBIDETACH
//config:	bool "ubidetach"
//config:	default y
//config:	select PLATFORM_LINUX
//config:	help
//config:	  Detach MTD device from an UBI device.
//config:
//config:config UBIMKVOL
//config:	bool "ubimkvol"
//config:	default y
//config:	select PLATFORM_LINUX
//config:	help
//config:	  Create a UBI volume.
//config:
//config:config UBIRMVOL
//config:	bool "ubirmvol"
//config:	default y
//config:	select PLATFORM_LINUX
//config:	help
//config:	  Delete a UBI volume.
//config:
//config:config UBIRSVOL
//config:	bool "ubirsvol"
//config:	default y
//config:	select PLATFORM_LINUX
//config:	help
//config:	  Resize a UBI volume.
//config:
//config:config UBIUPDATEVOL
//config:	bool "ubiupdatevol"
//config:	default y
//config:	select PLATFORM_LINUX
//config:	help
//config:	  Update a UBI volume.

//applet:IF_UBIATTACH(APPLET_ODDNAME(ubiattach, ubi_tools, BB_DIR_USR_SBIN, BB_SUID_DROP, ubiattach))
//applet:IF_UBIDETACH(APPLET_ODDNAME(ubidetach, ubi_tools, BB_DIR_USR_SBIN, BB_SUID_DROP, ubidetach))
//applet:IF_UBIMKVOL(APPLET_ODDNAME(ubimkvol, ubi_tools, BB_DIR_USR_SBIN, BB_SUID_DROP, ubimkvol))
//applet:IF_UBIRMVOL(APPLET_ODDNAME(ubirmvol, ubi_tools, BB_DIR_USR_SBIN, BB_SUID_DROP, ubirmvol))
//applet:IF_UBIRSVOL(APPLET_ODDNAME(ubirsvol, ubi_tools, BB_DIR_USR_SBIN, BB_SUID_DROP, ubirsvol))
//applet:IF_UBIUPDATEVOL(APPLET_ODDNAME(ubiupdatevol, ubi_tools, BB_DIR_USR_SBIN, BB_SUID_DROP, ubiupdatevol))

//kbuild:lib-$(CONFIG_UBIATTACH) += ubi_tools.o
//kbuild:lib-$(CONFIG_UBIDETACH) += ubi_tools.o
//kbuild:lib-$(CONFIG_UBIMKVOL)  += ubi_tools.o
//kbuild:lib-$(CONFIG_UBIRMVOL)  += ubi_tools.o
//kbuild:lib-$(CONFIG_UBIRSVOL)  += ubi_tools.o
//kbuild:lib-$(CONFIG_UBIUPDATEVOL) += ubi_tools.o

#include "libbb.h"
/* Some versions of kernel have broken headers, need this hack */
#ifndef __packed
# define __packed __attribute__((packed))
#endif
#include <mtd/ubi-user.h>

#define OPTION_M  (1 << 0)
#define OPTION_D  (1 << 1)
#define OPTION_n  (1 << 2)
#define OPTION_N  (1 << 3)
#define OPTION_s  (1 << 4)
#define OPTION_a  (1 << 5)
#define OPTION_t  (1 << 6)

#define do_attach (ENABLE_UBIATTACH && applet_name[3] == 'a')
#define do_detach (ENABLE_UBIDETACH && applet_name[3] == 'd')
#define do_mkvol  (ENABLE_UBIMKVOL  && applet_name[3] == 'm')
#define do_rmvol  (ENABLE_UBIRMVOL  && applet_name[4] == 'm')
#define do_rsvol  (ENABLE_UBIRSVOL  && applet_name[4] == 's')
#define do_update (ENABLE_UBIUPDATEVOL && applet_name[3] == 'u')

//usage:#define ubiattach_trivial_usage
//usage:       "-m MTD_NUM [-d UBI_NUM] UBI_CTRL_DEV"
//usage:#define ubiattach_full_usage "\n\n"
//usage:       "Attach MTD device to UBI\n"
//usage:     "\n	-m MTD_NUM	MTD device number to attach"
//usage:     "\n	-d UBI_NUM	UBI device number to assign"
//usage:
//usage:#define ubidetach_trivial_usage
//usage:       "-d UBI_NUM UBI_CTRL_DEV"
//usage:#define ubidetach_full_usage "\n\n"
//usage:       "Detach MTD device from UBI\n"
//usage:     "\n	-d UBI_NUM	UBI device number"
//usage:
//usage:#define ubimkvol_trivial_usage
//usage:       "UBI_DEVICE -N NAME -s SIZE"
//usage:#define ubimkvol_full_usage "\n\n"
//usage:       "Create UBI volume\n"
//usage:     "\n	-a ALIGNMENT	Volume alignment (default 1)"
//usage:     "\n	-n VOLID	Volume ID, if not specified, it"
//usage:     "\n			will be assigned automatically"
//usage:     "\n	-N NAME		Volume name"
//usage:     "\n	-s SIZE		Size in bytes"
//usage:     "\n	-t TYPE		Volume type (static|dynamic)"
//usage:
//usage:#define ubirmvol_trivial_usage
//usage:       "UBI_DEVICE -n VOLID"
//usage:#define ubirmvol_full_usage "\n\n"
//usage:       "Remove UBI volume\n"
//usage:     "\n	-n VOLID	Volume ID"
//usage:
//usage:#define ubirsvol_trivial_usage
//usage:       "UBI_DEVICE -n VOLID -s SIZE"
//usage:#define ubirsvol_full_usage "\n\n"
//usage:       "Resize UBI volume\n"
//usage:     "\n	-n VOLID	Volume ID to resize"
//usage:     "\n	-s SIZE		Size in bytes"
//usage:
//usage:#define ubiupdatevol_trivial_usage
//usage:       "UBI_DEVICE [IMG_FILE]"
//usage:#define ubiupdatevol_full_usage "\n\n"
//usage:       "Update UBI volume\n"
//usage:     "\n	-t	Truncate UBI volume"
//usage:     "\n	-s SIZE	Bytes in input (if reading stdin)"


int ubi_tools_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ubi_tools_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opts;
	char *ubi_ctrl;
	//struct stat st;
	int fd;
	int mtd_num;
	int dev_num = UBI_DEV_NUM_AUTO;
	int vol_id = UBI_VOL_NUM_AUTO;
	char *vol_name = NULL;
	int size_bytes;
	int alignment = 1;
	char *type = NULL;

	opt_complementary = "-1:m+:d+:n+:s+:a+";
	opts = getopt32(argv, "m:d:n:N:s:a:t::",
			&mtd_num, &dev_num, &vol_id,
			&vol_name, &size_bytes, &alignment, &type
	);
	ubi_ctrl = argv[optind];

	fd = xopen(ubi_ctrl, O_RDWR);
	//xfstat(fd, &st, ubi_ctrl);
	//if (!S_ISCHR(st.st_mode))
	//	bb_error_msg_and_die("%s: not a char device", ubi_ctrl);

	if (do_attach) {
		struct ubi_attach_req req;
		if (!(opts & OPTION_M))
			bb_error_msg_and_die("%s device not specified", "MTD");

		memset(&req, 0, sizeof(req));
		req.mtd_num = mtd_num;
		req.ubi_num = dev_num;

		xioctl(fd, UBI_IOCATT, &req);
	} else
	if (do_detach) {
		if (!(opts & OPTION_D))
			bb_error_msg_and_die("%s device not specified", "UBI");

		xioctl(fd, UBI_IOCDET, &dev_num);
	} else
	if (do_mkvol) {
		struct ubi_mkvol_req req;
		int vol_name_len;
		if (!(opts & OPTION_s))
			bb_error_msg_and_die("%s size not specified", "UBI");
		if (!(opts & OPTION_N))
			bb_error_msg_and_die("%s name not specified", "UBI");
		vol_name_len = strlen(vol_name);
		if (vol_name_len > UBI_MAX_VOLUME_NAME)
			bb_error_msg_and_die("%s volume name too long", "UBI");

		memset(&req, 0, sizeof(req));
		req.vol_id = vol_id;
		if ((opts & OPTION_t) && type) {
			if (type[0] == 's')
				req.vol_type = UBI_STATIC_VOLUME;
			else
				req.vol_type = UBI_DYNAMIC_VOLUME;
		} else {
			req.vol_type = UBI_DYNAMIC_VOLUME;
		}
		req.alignment = alignment;
		req.bytes = size_bytes;
		strncpy(req.name, vol_name, UBI_MAX_VOLUME_NAME);
		req.name_len = vol_name_len;

		xioctl(fd, UBI_IOCMKVOL, &req);
	} else
	if (do_rmvol) {
		if (!(opts & OPTION_n))
			bb_error_msg_and_die("%s volume id not specified", "UBI");

		xioctl(fd, UBI_IOCRMVOL, &vol_id);
	} else
	if (do_rsvol) {
		struct ubi_rsvol_req req;
		if (!(opts & OPTION_s))
			bb_error_msg_and_die("%s size not specified", "UBI");
		if (!(opts & OPTION_n))
			bb_error_msg_and_die("%s volume id not specified", "UBI");

		memset(&req, 0, sizeof(req));
		req.bytes = size_bytes;
		req.vol_id = vol_id;

		xioctl(fd, UBI_IOCRSVOL, &req);
	} else
	if (do_update) {
		long long bytes;

		if (opts & OPTION_t) {
			// truncate the volume by starting an update for size 0
			bytes = 0;
			xioctl(fd, UBI_IOCVOLUP, &bytes);
		}
		else {
			struct stat st;
			char buf[sizeof("/sys/class/ubi/ubi%d_%d/usable_eb_size") + 2 * sizeof(int)*3];
			int input_fd;
			unsigned ubinum, volnum;
			unsigned leb_size;
			ssize_t len;
			char *input_data;

			// Make assumption that device not is in normal format.
			// Removes need for scanning sysfs tree as full libubi does
			if (sscanf(ubi_ctrl, "/dev/ubi%u_%u", &ubinum, &volnum) != 2)
				bb_error_msg_and_die("%s volume node not in correct format", "UBI");

			sprintf(buf, "/sys/class/ubi/ubi%u_%u/usable_eb_size", ubinum, volnum);
			if (open_read_close(buf, buf, sizeof(buf)) <= 0)
				bb_error_msg_and_die("%s could not get LEB size", "UBI");
			if (sscanf(buf, "%u", &leb_size) != 1)
				bb_error_msg_and_die("%s could not get LEB size", "UBI");

			if (opts & OPTION_s) {
				input_fd = 0;
			} else {
				if (!argv[optind+1])
					bb_show_usage();
				xstat(argv[optind+1], &st);
				size_bytes = st.st_size;
				input_fd = xopen(argv[optind+1], O_RDONLY);
			}

			bytes = size_bytes;
			xioctl(fd, UBI_IOCVOLUP, &bytes);

			input_data = xmalloc(leb_size);
			while ((len = full_read(input_fd, input_data, leb_size)) > 0) {
				xwrite(fd, input_data, len);
			}
			if (len < 0)
				bb_error_msg_and_die("%s volume update failed", "UBI");
			if (ENABLE_FEATURE_CLEAN_UP)
				close(input_fd);
		}
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);

	return EXIT_SUCCESS;
}

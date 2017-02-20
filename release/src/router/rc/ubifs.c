/*
	Tomato Firmware
	Copyright (C) 2006-2009 Jonathan Zarate

*/

#include "rc.h"

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <mtd/ubi-user.h>
#include <errno.h>
#ifndef MNT_DETACH
#define MNT_DETACH	0x00000002
#endif

#define UBI_SYSFS_DIR	"/sys/class/ubi"

#define UBIFS_VOL_NAME	"jffs2"
#define UBIFS_MNT_DIR	"/jffs"
#define UBIFS_FS_TYPE	"ubifs"

static void error(const char *message)
{
	char s[512];

	snprintf(s, sizeof(s),
		 "Error %s JFFS. Check the logs to see if they contain more details about this error.",
		 message);
	notice_set("ubifs", s);
}

/* erase and format specified UBI volume that is used to provide container of UBIFS
 * @return:
 * 	0:	success
 *     -1:	invalid parameter
 *     -2:	get total number of erase blocks fail
 *     -3:	invalid reserved ebs of the ubi volume
 *     -4:	open /dev/ubiX_Y failed.
 *     -5:	erase LEB fail
 *     -6:	got upd_marker flag fail
 */
static int ubifs_unlock(int dev, int part)
{
	int i, r, fd, nr_ebs = 0, upd_marker = 0;
	int32_t lnum;
	char *s, path[PATH_MAX];

	if (dev < 0 || part < 0)
		return -1;

	snprintf(path, sizeof(path), "%s/ubi%d_%d/upd_marker", UBI_SYSFS_DIR, dev, part);
	if (!(s = file2str(path)))
		return -6;
	if (atoi(s) == 1)
		upd_marker = 1;
	free(s);

	snprintf(path, sizeof(path), "%s/ubi%d_%d/reserved_ebs", UBI_SYSFS_DIR, dev, part);
	if (!(s = file2str(path)))
		return -2;
	nr_ebs = atoi(s);
	free(s);
	if (nr_ebs <= 0)
		return -3;

	snprintf(path, sizeof(path), "/dev/ubi%d_%d", dev, part);
	if (!(fd = open(path, O_RDWR)))
		return -4;

	if (upd_marker) {
		int64_t bytes = 0;

		if ((r = ioctl(fd, UBI_IOCVOLUP, &bytes)) != 0)
			_dprintf("%s: clean upd_marker fail!. (ret %d errno %d %s)\n",
				__func__, r, errno, strerror(errno));
	}

	for (i = 0; i < nr_ebs; ++i) {
		lnum = i;
		if (!(r = ioctl(fd, UBI_IOCEBER, &lnum)))
			continue;
		_dprintf("%s: erase leb %d of ubi%d_%d fail. (ret %d errno %d %s)\n",
			__func__, lnum, dev, part, r, errno, strerror(errno));
	}

	close(fd);

	snprintf(path, sizeof(path), "/dev/ubi%d_%d", dev, part);
	eval("mkfs.ubifs", "-x", "favor_lzo", path);
	return 0;
}

/* format specified UBI volume that is used to provide container of UBIFS
 * @return:
 */
static inline int ubifs_erase(int dev, int part)
{
	return ubifs_unlock(dev, part);
}

void start_ubifs(void)
{
	int format = 0;
	char s[256];
	int dev, part, size;
	const char *p;
	struct statfs sf;
#if defined(RTCONFIG_TEST_BOARDDATA_FILE)
	int r;
#endif

	if (!nvram_match("ubifs_on", "1")) {
		notice_set("ubifs", "");
		return;
	}

	if (!wait_action_idle(10))
		return;

	if (ubi_getinfo(UBIFS_VOL_NAME, &dev, &part, &size) < 0)
		return;

	_dprintf("*** ubifs: %d, %d, %d\n", dev, part, size);
	if (nvram_match("ubifs_format", "1")) {
		nvram_set("ubifs_format", "0");

		if (ubifs_erase(dev, part)) {
			error("formatting");
			return;
		}

		format = 1;
	}

	sprintf(s, "%d", size);
	p = nvram_get("ubifs_size");
	if ((p == NULL) || (strcmp(p, s) != 0)) {
		if (format) {
			nvram_set("ubifs_size", s);
			nvram_commit_x();
		} else if ((p != NULL) && (*p != 0)) {
			error("verifying known size of");
			return;
		}
	}

	if ((statfs(UBIFS_MNT_DIR, &sf) == 0)
	    && (sf.f_type != 0x73717368 /* squashfs */ )) {
		// already mounted
		notice_set("ubifs", format ? "Formatted" : "Loaded");
		return;
	}
	if (nvram_get_int("ubifs_clean_fs")) {
		if (ubifs_unlock(dev, part)) {
			error("unlocking");
			return;
		}
	}
	sprintf(s, "/dev/ubi%d_%d", dev, part);

	if (mount(s, UBIFS_MNT_DIR, UBIFS_FS_TYPE, MS_NOATIME, "") != 0) {
		_dprintf("*** ubifs mount error\n");
		if (ubifs_erase(dev, part)) {
			error("formatting");
			return;
		}

		format = 1;
		if (mount(s, UBIFS_MNT_DIR, UBIFS_FS_TYPE, MS_NOATIME, "") != 0) {
			_dprintf("*** ubifs 2-nd mount error\n");
			error("mounting");
			return;
		}
	}

	if (nvram_get_int("ubifs_clean_fs")) {
		_dprintf("Clean /jffs/*\n");
		system("rm -fr /jffs/*");
		nvram_unset("ubifs_clean_fs");
		nvram_commit_x();
	}

	notice_set("ubifs", format ? "Formatted" : "Loaded");

	if (((p = nvram_get("ubifs_exec")) != NULL) && (*p != 0)) {
		chdir(UBIFS_MNT_DIR);
		system(p);
		chdir("/");
	}
	run_userfile(UBIFS_MNT_DIR, ".asusrouter", UBIFS_MNT_DIR, 3);

#if defined(RTCONFIG_TEST_BOARDDATA_FILE)
	/* Copy /lib/firmware to /tmp/firmware, and
	 * bind mount /tmp/firmware to /lib/firmware.
	 */
	if (!d_exists(UBIFS_MNT_DIR "/firmware")) {
		_dprintf("Rebuild /lib/firmware on " UBIFS_MNT_DIR "/firmware!\n");
		eval("cp", "-a", "/lib/firmware", UBIFS_MNT_DIR "/firmware");
		sync();
	}
	if ((r = mount(UBIFS_MNT_DIR "/firmware", "/lib/firmware", NULL, MS_BIND, NULL)) != 0)
		_dprintf("%s: bind mount " UBIFS_MNT_DIR "/firmware fail! (r = %d)\n", __func__, r);
#endif

}

void stop_ubifs(int stop)
{
	struct statfs sf;
#if defined(RTCONFIG_PSISTLOG)
	int restart_syslogd = 0;
#endif
#if defined(RTCONFIG_TEST_BOARDDATA_FILE)
	struct mntent *mnt;
#endif

	if (!wait_action_idle(10))
		return;

	if ((statfs(UBIFS_MNT_DIR, &sf) == 0) && (sf.f_type != 0x73717368)) {
		// is mounted
		run_userfile(UBIFS_MNT_DIR, ".autostop", UBIFS_MNT_DIR, 5);
		run_nvscript("script_autostop", UBIFS_MNT_DIR, 5);
	}
#if defined(RTCONFIG_PSISTLOG)
	if (!stop && !strncmp(get_syslog_fname(0), UBIFS_MNT_DIR "/", sizeof(UBIFS_MNT_DIR) + 1)) {
		restart_syslogd = 1;
		stop_syslogd();
		eval("cp", UBIFS_MNT_DIR "/syslog.log", UBIFS_MNT_DIR "/syslog.log-1", "/tmp");
	}
#endif

#if defined(RTCONFIG_TEST_BOARDDATA_FILE)
	if ((mnt = findmntents("/lib/firmware", 0, NULL, 0)) != NULL) {
		_dprintf("Unmount /lib/firmware\n");
		sync();
		if (umount("/lib/firmware"))
			umount2("/lib/firmware", MNT_DETACH);
	}
#endif

	notice_set("ubifs", "Stopped");
	if (umount(UBIFS_MNT_DIR))
		umount2(UBIFS_MNT_DIR, MNT_DETACH);

#if defined(RTCONFIG_PSISTLOG)
	if (restart_syslogd)
		start_syslogd();
#endif
}

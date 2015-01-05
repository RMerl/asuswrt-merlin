/*

	Tomato Firmware
	USB Support Module

*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <dirent.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <linux/version.h>

#include <bcmnvram.h>
#include <bcmdevs.h>
#include <wlutils.h>

#include "shutils.h"
#include "shared.h"

#include <linux/version.h>
#ifndef LINUX_KERNEL_VERSION
#define LINUX_KERNEL_VERSION LINUX_VERSION_CODE
#endif

/* Serialize using fcntl() calls 
 */

int check_magic(char *buf, char *magic){
	if(!strncmp(magic, "ext3_chk", 8)){
		if(!((*buf)&4))
			return 0;
		if(*(buf+4) >= 0x40)
			return 0;
		if(*(buf+8) >= 8)
			return 0;
		return 1;
	}
 
	if(!strncmp(magic, "ext4_chk", 8)){
		if(!((*buf)&4))
			return 0;
		if(*(buf+4) > 0x3F)
			return 1;
		if(*(buf+4) >= 0x40)
			return 0;
		if(*(buf+8) <= 7)
			return 0;
		return 1;
	}

	return 0;
}

char *detect_fs_type(char *device)
{
	int fd;
	unsigned char buf[4096];

	if ((fd = open(device, O_RDONLY)) < 0)
		return NULL;

	if (read(fd, buf, sizeof(buf)) != sizeof(buf))
	{
		close(fd);
		return NULL;
	}

	close(fd);

	/* first check for mbr */
	if (*device && device[strlen(device) - 1] > '9' &&
		buf[510] == 0x55 && buf[511] == 0xAA && /* signature */
		((buf[0x1be] | buf[0x1ce] | buf[0x1de] | buf[0x1ee]) & 0x7f) == 0) /* boot flags */ 
	{
		return "mbr";
	} 
	/* detect swap */
	else if (memcmp(buf + 4086, "SWAPSPACE2", 10) == 0 ||
		 memcmp(buf + 4086, "SWAP-SPACE", 10) == 0 ||
		 memcmp(buf + 4086, "S1SUSPEND", 9) == 0 ||
		 memcmp(buf + 4086, "S2SUSPEND", 9) == 0 ||
		 memcmp(buf + 4086, "ULSUSPEND", 9) == 0)
	{
		return "swap";
	}
	/* detect ext2/3/4 */
	else if (buf[0x438] == 0x53 && buf[0x439] == 0xEF)
	{
		if(check_magic((char *) &buf[0x45c], "ext3_chk"))
			return "ext3";
		else if(check_magic((char *) &buf[0x45c], "ext4_chk"))
			return "ext4";
		else
			return "ext2";
	}
	/* detect hfs */
	else if(buf[1024] == 0x48){
		if(!memcmp(buf+1032, "HFSJ", 4)){
			if(buf[1025] == 0x58) // with case-sensitive
				return "hfs+jx";
			else
				return "hfs+j";
		}
		else
			return "hfs";
	}
	/* detect ntfs */
	else if (buf[510] == 0x55 && buf[511] == 0xAA && /* signature */
		memcmp(buf + 3, "NTFS    ", 8) == 0)
	{
		return "ntfs";
	}
	/* detect vfat */
	else if (buf[510] == 0x55 && buf[511] == 0xAA && /* signature */
		buf[11] == 0 && buf[12] >= 1 && buf[12] <= 8 /* sector size 512 - 4096 */ &&
		buf[13] != 0 && (buf[13] & (buf[13] - 1)) == 0) /* sectors per cluster */
	{
		if(buf[6] == 0x20 && buf[7] == 0x20 && !memcmp(buf+71, "EFI        ", 11))
			return "apple_efi";
		else
			return "vfat";
	}

	return "unknown";
}


/* Execute a function for each disc partition on the specified controller.
 *
 * Directory /dev/discs/ looks like this:
 * disc0 -> ../scsi/host0/bus0/target0/lun0/
 * disc1 -> ../scsi/host1/bus0/target0/lun0/
 * disc2 -> ../scsi/host2/bus0/target0/lun0/
 * disc3 -> ../scsi/host2/bus0/target0/lun1/
 *
 * Scsi host 2 supports multiple drives.
 * Scsi host 0 & 1 support one drive.
 *
 * For attached drives, like this.  If not attached, there is no "part#" item.
 * Here, only one drive, with 2 partitions, is plugged in.
 * /dev/discs/disc0/disc
 * /dev/discs/disc0/part1
 * /dev/discs/disc0/part2
 * /dev/discs/disc1/disc
 * /dev/discs/disc2/disc
 *
 * Which is the same as:
 * /dev/scsi/host0/bus0/target0/lun0/disc
 * /dev/scsi/host0/bus0/target0/lun0/part1
 * /dev/scsi/host0/bus0/target0/lun0/part2
 * /dev/scsi/host1/bus0/target0/lun0/disc
 * /dev/scsi/host2/bus0/target0/lun0/disc
 * /dev/scsi/host2/bus0/target0/lun1/disc
 *
 * Implementation notes:
 * Various mucking about with a disc that just got plugged in or unplugged
 * will make the scsi subsystem try a re-validate, and read the partition table of the disc.
 * This will make sure the partitions show up.
 *
 * It appears to try to do the revalidate and re-read & update the partition
 * information when this code does the "readdir of /dev/discs/disc0/?".  If the
 * disc has any mounted partitions the revalidate will be rejected.  So the
 * current partition info will remain.  On an unplug event, when it is doing the
 * readdir's, it will try to do the revalidate as we are doing the readdir's.
 * But luckily they'll be rejected, otherwise the later partitions will disappear as
 * soon as we get the first one.
 * But be very careful!  If something goes not exactly right, the partition entries
 * will disappear before we've had a chance to unmount from them.
 *
 * To avoid this automatic revalidation, we go through /proc/partitions looking for the partitions
 * that /dev/discs point to.  That will avoid the implicit revalidate attempt.
 * 
 * If host < 0, do all hosts.   If >= 0, it is the host number to do.
 *
 */

/* check if the block device has no partition */
int is_no_partition(const char *discname)
{
	FILE *procpt;
	char line[128], ptname[32];
	int ma, mi, sz;
	int count = 0;

	if ((procpt = fopen("/proc/partitions", "r"))) {
		while (fgets(line, sizeof(line), procpt)) {
			if (sscanf(line, " %d %d %d %[^\n ]", &ma, &mi, &sz, ptname) != 4)
				continue;
			if (strstr(ptname, discname))
				count++;
		}
	}

	return (count == 1);
}

int exec_for_host(int host, int obsolete, uint flags, host_exec func)
{
	DIR *usb_dev_disc;
	char ptname[32];/* Will be: discDN_PN	 					*/
	char dsname[16];/* Will be: discDN	 					*/
	int host_no;	/* SCSI controller/host # */
	struct dirent *dp;
	FILE *prt_fp;
	int siz;
	char line[256];
	int result = 0;
#ifdef LINUX26
	int ret;
	char hostbuf[16], device_path[PATH_MAX], linkbuf[PATH_MAX], *h;
#else
	char link[256];	/* Will be: ../scsi/host#/bus0/target0/lun#  that bfr links to. */
			/* When calling the func, will be: /dev/discs/disc#/part#	*/
	char bfr[256];	/* Will be: /dev/discs/disc#					*/
	char bfr2[128];	/* Will be: /dev/discs/disc#/disc     for the BLKRRPART.	*/
	char *cp;
	int len;
	int disc_num;	/* Disc # */
	int part_num;	/* Parition # */
	char *mp;	/* Ptr to after any leading ../ path */
#endif

	_dprintf("exec_for_host(%d, %d, %d, %d)\n", host, obsolete, flags, func);
	if (!func)
		return 0;

	flags |= EFH_1ST_HOST;

#ifdef LINUX26
	/* /sys/bus/scsi/devices/X:X:X:X/block:sdX doesn't exist in kernel 3.0
	 * 1. Enumerate sub-directory, DIR, of /sys/block.
	 * 2. Skip ., .., loop*, mtdblock*, ram*, etc.
	 * 3. read DIR/device link. Check whether X:X:X:X exist. e.g.
	 *    56U: ../../devices/platform/rt3xxx-ehci/usb1/1-1/1-1:1.0/host1/target1:0:0/1:0:0:0
	 *    65U: ../../devices/pci0000:00/0000:00:00.0/0000:01:00.0/usb1/1-2/1-2:1.0/host1/target1:0:0/1:0:0:0
	 * 4. If yes, DIR would be sda, sdb, etc.
	 * 5. Search DIR in /proc/partitions.
	 */
	sprintf(hostbuf, "%d:", host);
	if (!(usb_dev_disc = opendir("/sys/block")))
		return 0;
	while ((dp = readdir(usb_dev_disc))) {
		if (!strncmp(dp->d_name, "loop", 4) ||
		    !strncmp(dp->d_name, "mtdblock", 8) ||
		    !strncmp(dp->d_name, "ram", 3) ||
		    !strcmp(dp->d_name, ".") ||
		    !strcmp(dp->d_name, "..")
		   )
			continue;
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(3,3,0)
		snprintf(device_path, sizeof(device_path), "/sys/block/%s", dp->d_name);
#else
		snprintf(device_path, sizeof(device_path), "/sys/block/%s/device", dp->d_name);
#endif
		if (readlink(device_path, linkbuf, sizeof(linkbuf)) == -1)
			continue;
		h = strstr(linkbuf, "/host");
		if (!h)	continue;
		if ((ret = sscanf(h, "/host%*d/target%*d:%*d:%*d/%d:%*d:%*d:%*d", &host_no)) != 1) {
			_dprintf("%s(): sscanf can't distinguish host_no from [%s]. ret %d\n", __func__, linkbuf, ret);
			continue;
		}
		if (host >= 0 && host != host_no)
			continue;
		snprintf(dsname, sizeof(dsname), dp->d_name);
		siz = strlen(dsname);
		flags |= EFH_1ST_DISC;
		if (!(prt_fp = fopen("/proc/partitions", "r")))
			continue;
		while (fgets(line, sizeof(line) - 2, prt_fp)) {
			if (sscanf(line, " %*s %*s %*s %s", ptname) != 1)
				continue;

			if (!strncmp(ptname, dsname, siz)) {
				if (!strcmp(ptname, dsname) && !is_no_partition(dsname))
					continue;
				sprintf(line, "/dev/%s", ptname);
				result = (*func)(line, host_no, dsname, ptname, flags) || result;
				flags &= ~(EFH_1ST_HOST | EFH_1ST_DISC);
			}
		}
		fclose(prt_fp);
	}
	closedir(usb_dev_disc);

#else	/* !LINUX26 */

	if ((usb_dev_disc = opendir(DEV_DISCS_ROOT))) {
		while ((dp = readdir(usb_dev_disc))) {
			sprintf(bfr, "%s/%s", DEV_DISCS_ROOT, dp->d_name);
			if (strncmp(dp->d_name, "disc", 4) != 0)
				continue;

			disc_num = atoi(dp->d_name + 4);
			len = readlink(bfr, link, sizeof(link) - 1);
			if (len < 0)
				continue;

			link[len] = 0;
			cp = strstr(link, "/scsi/host");
			if (!cp)
				continue;

			host_no = atoi(cp + 10);
			if (host >= 0 && host_no != host)
				continue;

			/* We have found a disc that is on this controller.
			 * Loop thru all the partitions on this disc.
			 * The new way, reading thru /proc/partitions.
			 */
			mp = link;
			if ((cp = strstr(link, "../")) != NULL)
				mp = cp + 3;
			siz = strlen(mp);

			flags |= EFH_1ST_DISC;
			if (func && (prt_fp = fopen("/proc/partitions", "r"))) {
				while (fgets(line, sizeof(line) - 2, prt_fp)) {
					if (sscanf(line, " %*s %*s %*s %s", bfr2) == 1 &&
					    strncmp(bfr2, mp, siz) == 0)
					{
						if ((cp = strstr(bfr2, "/part"))) {
							part_num = atoi(cp + 5);
							sprintf(line, "%s/part%d", bfr, part_num);
							sprintf(dsname, "disc%d", disc_num);
							sprintf(ptname, "disc%d_%d", disc_num, part_num);
						}
						else if ((cp = strstr(bfr2, "/disc"))) {
							*(++cp) = 0;
							if (!is_no_partition(bfr2))
								continue;
							sprintf(line, "%s/disc", bfr);
							sprintf(dsname, "disc%d", disc_num);
							strcpy(ptname, dsname);
						}
						else {
							continue;
						}
						result = (*func)(line, host_no, dsname, ptname, flags) || result;
						flags &= ~(EFH_1ST_HOST | EFH_1ST_DISC);
					}
				}
				fclose(prt_fp);
			}
		}
		closedir(usb_dev_disc);
	}

#endif	/* LINUX26 */

	return result;
}

/* Concept taken from the e2fsprogs/ismounted.c.
 * Find wherever 'file' (actually: device) is mounted.
 * Either the exact same device-name, or another device-name.
 * The latter is detected by comparing the rdev or dev&inode.
 * So aliasing won't fool us---we'll still find if it's mounted.
 * Return its mnt entry.
 * In particular, the caller would look at the mnt->mountpoint.
 *
 * Find the matching devname(s) in mounts or swaps.
 * If func is supplied, call it for each match.  If not, return mnt on the first match.
 */

static inline int is_same_device(char *fsname, dev_t file_rdev, dev_t file_dev, ino_t file_ino)
{
	struct stat st_buf;

	if (stat(fsname, &st_buf) == 0) {
		if (S_ISBLK(st_buf.st_mode)) {
			if (file_rdev && (file_rdev == st_buf.st_rdev))
				return 1;
		}
		else {
			if (file_dev && ((file_dev == st_buf.st_dev) &&
				(file_ino == st_buf.st_ino)))
				return 1;
			/* Check for [swap]file being on the device. */
			if (file_dev == 0 && file_ino == 0 && file_rdev == st_buf.st_dev)
				return 1;
		}
	}
	return 0;
}


struct mntent *findmntents(char *file, int swp, int (*func)(struct mntent *mnt, uint flags), uint flags)
{
	struct mntent	*mnt;
	struct stat	st_buf;
	dev_t		file_dev=0, file_rdev=0;
	ino_t		file_ino=0;
	FILE		*f;

	if ((f = setmntent(swp ? "/proc/swaps": "/proc/mounts", "r")) == NULL)
		return NULL;

	if (stat(file, &st_buf) == 0) {
		if (S_ISBLK(st_buf.st_mode)) {
			file_rdev = st_buf.st_rdev;
		}
		else {
			file_dev = st_buf.st_dev;
			file_ino = st_buf.st_ino;
		}
	}
	while ((mnt = getmntent(f)) != NULL) {
		/* Always ignore rootfs mount */
		if (strcmp(mnt->mnt_fsname, "rootfs") == 0)
			continue;

		if (strcmp(file, mnt->mnt_fsname) == 0 ||
		    strcmp(file, mnt->mnt_dir) == 0 ||
		    is_same_device(mnt->mnt_fsname, file_rdev , file_dev, file_ino)) {
			if (func == NULL)
				break;
			(*func)(mnt, flags);
		}
	}

	endmntent(f);
	return mnt;
}


//#define SAME_AS_KERNEL
/* Simulate a hotplug event, as if a USB storage device
 * got plugged or unplugged.
 * Either use a hardcoded program name, or the same
 * hotplug program that the kernel uses for a real event.
 */
void add_remove_usbhost(char *host, int add)
{
	setenv("ACTION", add ? "add" : "remove", 1);
	setenv("SCSI_HOST", host, 1);
	setenv("PRODUCT", host, 1);
	setenv("INTERFACE", "TOMATO/0", 1);
#ifdef SAME_AS_KERNEL
	char pgm[256] = "/sbin/hotplug usb";
	char *p;
	int fd = open("/proc/sys/kernel/hotplug", O_RDONLY);
	if (fd) {
		if (read(fd, pgm, sizeof(pgm) - 5) >= 0) {
			if ((p = strchr(pgm, '\n')) != NULL)
				*p = 0;
			strcat(pgm, " usb");
		}
		close(fd);
	}
	system(pgm);
#else
	// don't use value from /proc/sys/kernel/hotplug 
	// since it may be overriden by a user.
	system("/sbin/hotplug usb");
#endif
	unsetenv("INTERFACE");
	unsetenv("PRODUCT");
	unsetenv("SCSI_HOST");
	unsetenv("ACTION");
}


/****************************************************/
/* Use busybox routines to get labels for fat & ext */
/* Probe for label the same way that mount does.    */
/****************************************************/

#define VOLUME_ID_LABEL_SIZE		64
#define VOLUME_ID_UUID_SIZE		36
#define SB_BUFFER_SIZE			0x11000

struct volume_id {
	int		fd;
	int		error;
	size_t		sbbuf_len;
	size_t		seekbuf_len;
	uint8_t		*sbbuf;
	uint8_t		*seekbuf;
	uint64_t	seekbuf_off;
	char		label[VOLUME_ID_LABEL_SIZE+1];
	char		uuid[VOLUME_ID_UUID_SIZE+1];
};

extern void volume_id_set_uuid();
extern void *volume_id_get_buffer();
extern void volume_id_free_buffer();
extern int volume_id_probe_ext();
extern int volume_id_probe_vfat();
extern int volume_id_probe_ntfs();
extern int volume_id_probe_linux_swap();
extern int volume_id_probe_hfs_hfsplus(struct volume_id *id);

/* Put the label in *label and uuid in *uuid.
 * Return 0 if no label/uuid found, NZ if there is a label or uuid.
 */
int find_label_or_uuid(char *dev_name, char *label, char *uuid)
{
	struct volume_id id;

	memset(&id, 0x00, sizeof(id));
	if (label) *label = 0;
	if (uuid) *uuid = 0;
	if ((id.fd = open(dev_name, O_RDONLY)) < 0)
		return 0;

	volume_id_get_buffer(&id, 0, SB_BUFFER_SIZE);

	if (volume_id_probe_linux_swap(&id) == 0 || id.error)
		goto ret;
	if (volume_id_probe_vfat(&id) == 0 || id.error)
		goto ret;
	if (volume_id_probe_ext(&id) == 0 || id.error)
		goto ret;
	if (volume_id_probe_ntfs(&id) == 0 || id.error)
		goto ret;
#if defined(RTCONFIG_HFS)
	if(volume_id_probe_hfs_hfsplus(&id) == 0 || id.error)
		goto ret;
#endif
ret:
	volume_id_free_buffer(&id);
	if (label && (*id.label != 0))
		strcpy(label, id.label);
	if (uuid && (*id.uuid != 0))
		strcpy(uuid, id.uuid);
	close(id.fd);
	return (label && *label != 0) || (uuid && *uuid != 0);
}

void *xmalloc(size_t siz)
{
	return (malloc(siz));
}
#if 0
static void *xrealloc(void *old, size_t size)
{
	return realloc(old, size);
}
#endif
ssize_t full_read(int fd, void *buf, size_t len)
{
	return read(fd, buf, len);
}

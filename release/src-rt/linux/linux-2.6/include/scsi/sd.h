#ifndef _SCSI_DISK_H
#define _SCSI_DISK_H

/*
 * More than enough for everybody ;)  The huge number of majors
 * is a leftover from 16bit dev_t days, we don't really need that
 * much numberspace.
 */
#define SD_MAJORS	16

/*
 * This is limited by the naming scheme enforced in sd_probe,
 * add another character to it if you really need more disks.
 */
#define SD_MAX_DISKS	(((26 * 26) + 26 + 1) * 26)

/*
 * Time out in seconds for disks and Magneto-opticals (which are slower).
 */
#define SD_TIMEOUT		(30 * HZ)
#define SD_MOD_TIMEOUT		(75 * HZ)

/*
 * Number of allowed retries
 */
#define SD_MAX_RETRIES		5
#define SD_PASSTHROUGH_RETRIES	1

/*
 * Size of the initial data buffer for mode and read capacity data
 */
#define SD_BUF_SIZE		512

/*
 * Number of sectors at the end of the device to avoid multi-sector
 * accesses to in the case of last_sector_bug
 */
#define SD_LAST_BUGGY_SECTORS	8

struct scsi_disk {
	struct scsi_driver *driver;	/* always &sd_template */
	struct scsi_device *device;
	struct class_device cdev;
	struct gendisk	*disk;
	unsigned int	openers;	/* protected by BKL for now, yuck */
	sector_t	capacity;	/* size in 512-byte sectors */
	u32		index;
	u8		media_present;
	u8		write_prot;
	unsigned	WCE : 1;	/* state of disk WCE bit */
	unsigned	RCD : 1;	/* state of disk RCD bit, unused */
	unsigned	DPOFUA : 1;	/* state of disk DPOFUA bit */
	unsigned	first_scan : 1;
};
#define to_scsi_disk(obj) container_of(obj,struct scsi_disk,cdev)

static inline struct scsi_disk *scsi_disk(struct gendisk *disk)
{
	return container_of(disk->private_data, struct scsi_disk, driver);
}

static int  sd_revalidate_disk(struct gendisk *disk);
static void sd_rw_intr(struct scsi_cmnd * SCpnt);
static int  sd_probe(struct device *);
static int  sd_remove(struct device *);
static void sd_shutdown(struct device *dev);
static int sd_suspend(struct device *dev, pm_message_t state);
static int sd_resume(struct device *dev);
static void sd_rescan(struct device *);
static int  sd_init_command(struct scsi_cmnd *);
static int  sd_issue_flush(struct device *, sector_t *);
static void sd_prepare_flush(request_queue_t *, struct request *);
static void sd_read_capacity(struct scsi_disk *sdkp, unsigned char *buffer);
static void scsi_disk_release(struct class_device *cdev);
static void sd_print_sense_hdr(struct scsi_disk *, struct scsi_sense_hdr *);
static void sd_print_result(struct scsi_disk *, int);

#define sd_printk(prefix, sdsk, fmt, a...)				\
        (sdsk)->disk ?							\
	sdev_printk(prefix, (sdsk)->device, "[%s] " fmt,		\
		    (sdsk)->disk->disk_name, ##a) :			\
	sdev_printk(prefix, (sdsk)->device, fmt, ##a)

#endif /* _SCSI_DISK_H */

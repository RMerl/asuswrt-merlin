#ifndef FDISK_SUN_LABEL_H
#define FDISK_SUN_LABEL_H

#include <stdint.h>

struct sun_partition {
	uint32_t	start_cylinder;
	uint32_t	num_sectors;
};

struct sun_tag_flag {
	uint16_t	tag;
#define SUN_TAG_UNASSIGNED	0x00	/* Unassigned partition */
#define SUN_TAG_BOOT		0x01	/* Boot partition	*/
#define SUN_TAG_ROOT		0x02	/* Root filesystem	*/
#define SUN_TAG_SWAP		0x03	/* Swap partition	*/
#define SUN_TAG_USR		0x04	/* /usr filesystem	*/
#define SUN_TAG_BACKUP		0x05	/* Full-disk slice	*/
#define SUN_TAG_STAND		0x06	/* Stand partition	*/
#define SUN_TAG_VAR		0x07	/* /var filesystem	*/
#define SUN_TAG_HOME		0x08	/* /home filesystem	*/
#define SUN_TAG_ALTSCTR		0x09	/* Alt sector partition	*/
#define SUN_TAG_CACHE		0x0a	/* Cachefs partition	*/
#define SUN_TAG_RESERVED	0x0b	/* SMI reserved data	*/
#define SUN_TAG_LINUX_SWAP	0x82	/* Linux SWAP		*/
#define SUN_TAG_LINUX_NATIVE	0x83	/* Linux filesystem	*/
#define SUN_TAG_LINUX_LVM	0x8e	/* Linux LVM		*/
#define SUN_TAG_LINUX_RAID	0xfd	/* LInux RAID		*/

	uint16_t	flag;
#define SUN_FLAG_UNMNT		0x01	/* Unmountable partition*/
#define SUN_FLAG_RONLY		0x10	/* Read only		*/
};

#define SUN_LABEL_SIZE		512

#define SUN_LABEL_ID_SIZE	128
#define SUN_VOLUME_ID_SIZE	8

#define SUN_LABEL_VERSION	0x00000001
#define SUN_LABEL_SANE		0x600ddeee
#define SUN_NUM_PARTITIONS	8

struct sun_disk_label {
	char			label_id[SUN_LABEL_ID_SIZE];
	uint32_t			version;
	char			volume_id[SUN_VOLUME_ID_SIZE];
	uint16_t			num_partitions;
	struct sun_tag_flag	part_tags[SUN_NUM_PARTITIONS];
	uint32_t			bootinfo[3];
	uint32_t			sanity;
	uint32_t			resv[10];
	uint32_t			part_timestamps[SUN_NUM_PARTITIONS];
	uint32_t			write_reinstruct;
	uint32_t			read_reinstruct;
	uint8_t			pad[148];
	uint16_t			rpm;
	uint16_t			pcyl;
	uint16_t			apc;
	uint16_t			resv1;
	uint16_t			resv2;
	uint16_t			intrlv;
	uint16_t			ncyl;
	uint16_t			acyl;
	uint16_t			nhead;
	uint16_t			nsect;
	uint16_t			resv3;
	uint16_t			resv4;
	struct sun_partition	partitions[SUN_NUM_PARTITIONS];
	uint16_t			magic;
	uint16_t			cksum;
};

#define SUN_LABEL_MAGIC		0xDABE
#define SUN_LABEL_MAGIC_SWAPPED	0xBEDA
#define sunlabel ((struct sun_disk_label *)MBRbuffer)

/* fdisksunlabel.c */
extern struct systypes sun_sys_types[];
extern int check_sun_label(void);
extern void sun_nolabel(void);
extern void create_sunlabel(void);
extern void sun_delete_partition(int i);
extern int sun_change_sysid(int i, uint16_t sys);
extern void sun_list_table(int xtra);
extern void verify_sun(void);
extern void add_sun_partition(int n, int sys);
extern void sun_write_table(void);
extern void sun_set_alt_cyl(void);
extern void sun_set_ncyl(int cyl);
extern void sun_set_xcyl(void);
extern void sun_set_ilfact(void);
extern void sun_set_rspeed(void);
extern void sun_set_pcylcount(void);
extern void toggle_sunflags(int i, uint16_t mask);
extern int sun_get_sysid(int i);

#endif /* FDISK_SUN_LABEL_H */

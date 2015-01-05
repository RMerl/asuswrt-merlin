/*
   fdisk.h
*/

#include "c.h"

#define DEFAULT_SECTOR_SIZE	512
#define MAX_SECTOR_SIZE	2048
#define SECTOR_SIZE	512	/* still used in BSD code */
#define MAXIMUM_PARTS	60

#define ACTIVE_FLAG     0x80

#define EXTENDED        0x05
#define WIN98_EXTENDED  0x0f
#define LINUX_PARTITION 0x81
#define LINUX_SWAP      0x82
#define LINUX_NATIVE    0x83
#define LINUX_EXTENDED  0x85
#define LINUX_LVM       0x8e
#define LINUX_RAID      0xfd

#define IS_EXTENDED(i) \
	((i) == EXTENDED || (i) == WIN98_EXTENDED || (i) == LINUX_EXTENDED)

#define cround(n)	(display_in_cyl_units ? ((n)/units_per_sector)+1 : (n))
#define scround(x)	(((x)+units_per_sector-1)/units_per_sector)

struct partition {
	unsigned char boot_ind;         /* 0x80 - active */
	unsigned char head;             /* starting head */
	unsigned char sector;           /* starting sector */
	unsigned char cyl;              /* starting cylinder */
	unsigned char sys_ind;          /* What partition type */
	unsigned char end_head;         /* end head */
	unsigned char end_sector;       /* end sector */
	unsigned char end_cyl;          /* end cylinder */
	unsigned char start4[4];        /* starting sector counting from 0 */
	unsigned char size4[4];         /* nr of sectors in partition */
} __attribute__ ((packed));

enum menutype {
	MAIN_MENU,
	EXPERT_MENU,
};

enum failure {ioctl_error,
	unable_to_open, unable_to_read, unable_to_seek,
	unable_to_write};

enum action {fdisk, try_only};

struct geom {
	unsigned int heads;
	unsigned int sectors;
	unsigned int cylinders;
};

/* prototypes for fdisk.c */
extern char *disk_device, *line_ptr;
extern int fd, partitions;
extern unsigned int display_in_cyl_units, units_per_sector;
extern void change_units(void);
extern void fatal(enum failure why);
extern void get_geometry(int fd, struct geom *);
extern int  get_partition(int warn, int max);
extern void list_types(struct systypes *sys);
extern int read_line (int *asked);
extern char read_char(char *mesg);
extern int read_hex(struct systypes *sys);
extern void reread_partition_table(int leave);
extern struct partition *get_part_table(int);
extern int valid_part_table_flag(unsigned char *b);
extern unsigned int read_int(unsigned int low, unsigned int dflt,
			     unsigned int high, unsigned int base, char *mesg);
extern void print_menu(enum menutype);
extern void print_partition_size(int num, unsigned long long start, unsigned long long stop, int sysid);

extern unsigned char *MBRbuffer;
extern void zeroize_mbr_buffer(void);

extern unsigned int heads, cylinders, sector_size;
extern unsigned long long sectors;
extern char *partition_type(unsigned char type);
extern void update_units(void);
extern char read_chars(char *mesg);
extern void set_changed(int);
extern void set_all_unchanged(void);

#define PLURAL	0
#define SINGULAR 1
extern const char * str_units(int);

extern unsigned long long get_start_sect(struct partition *p);
extern unsigned long long get_nr_sects(struct partition *p);

enum labeltype {
	DOS_LABEL = 1,
	SUN_LABEL = 2,
	SGI_LABEL = 4,
	AIX_LABEL = 8,
	OSF_LABEL = 16,
	MAC_LABEL = 32,
	ANY_LABEL = -1
};

extern enum labeltype disklabel;

/* prototypes for fdiskbsdlabel.c */
extern void bsd_command_prompt(void);
extern int check_osf_label(void);
extern int btrydev(char * dev);
extern void xbsd_print_disklabel(int);

#ifndef FDISK_COMMON_H
#define FDISK_COMMON_H

/* common stuff for fdisk, cfdisk, sfdisk */

struct systypes {
	unsigned char type;
	char *name;
};

extern struct systypes i386_sys_types[];

extern char *partname(char *dev, int pno, int lth);

#endif /* FDISK_COMMON_H */

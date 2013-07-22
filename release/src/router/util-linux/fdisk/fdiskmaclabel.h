#ifndef FDISK_MAC_LABEL_H
#define FDISK_MAC_LABEL_H

#include <sys/types.h>
/*
 * Copyright (C) Andreas Neuper, Sep 1998.
 *	This file may be redistributed under
 *	the terms of the GNU Public License.
 */

typedef struct {
	unsigned int  magic;        /* expect MAC_LABEL_MAGIC */
	unsigned int  fillbytes1[124];
	unsigned int  physical_volume_id;
	unsigned int  fillbytes2[124];
} mac_partition;

/* MAC magic number only 16bits, do I always know that there are 0200
 * following? Problem, after magic the uint16_t res1; follows, I donnno know
 * about the 200k */
#define	MAC_LABEL_MAGIC		0x45520000
#define	MAC_LABEL_MAGIC_2	0x50530000
#define	MAC_LABEL_MAGIC_3	0x504d0000

#define	MAC_LABEL_MAGIC_SWAPPED		0x00002554

#define	MAC_LABEL_MAGIC_2_SWAPPED	0x00003505
#define	MAC_LABEL_MAGIC_3_SWAPPED	0x0000d405

/* fdisk.c */
#define maclabel ((mac_partition *)MBRbuffer)

/* fdiskmaclabel.c */
extern struct	systypes mac_sys_types[];
extern void	mac_nolabel( void );
extern int	check_mac_label( void );

#endif /* FDISK_MAC_LABEL_H */


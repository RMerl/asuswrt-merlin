/*
 * mediabay.h: definitions for using the media bay
 * on PowerBook 3400 and similar computers.
 *
 * Copyright (C) 1997 Paul Mackerras.
 */
#ifndef _PPC_MEDIABAY_H
#define _PPC_MEDIABAY_H

#ifdef __KERNEL__

#define MB_FD		0	/* media bay contains floppy drive (automatic eject ?) */
#define MB_FD1		1	/* media bay contains floppy drive (manual eject ?) */
#define MB_SOUND	2	/* sound device ? */
#define MB_CD		3	/* media bay contains ATA drive such as CD or ZIP */
#define MB_PCI		5	/* media bay contains a PCI device */
#define MB_POWER	6	/* media bay contains a Power device (???) */
#define MB_NO		7	/* media bay contains nothing */

int check_media_bay(struct device_node *which_bay, int what);
int check_media_bay_by_base(unsigned long base, int what);

/* Number of bays in the machine or 0 */
extern int media_bay_count;

/* called by pmac-ide.c to register IDE controller for media bay */
extern int media_bay_set_ide_infos(struct device_node* which_bay,
			unsigned long base, int irq, int index);

#endif /* __KERNEL__ */
#endif /* _PPC_MEDIABAY_H */

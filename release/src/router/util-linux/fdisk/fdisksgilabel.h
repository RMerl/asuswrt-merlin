#ifndef FDISK_SGI_LABEL_H
#define FDISK_SGI_LABEL_H

#include <stdint.h>

#include "bitops.h"

/*
 * Copyright (C) Andreas Neuper, Sep 1998.
 *	This file may be modified and redistributed under
 *	the terms of the GNU Public License.
 */

struct device_parameter { /* 48 bytes */
	unsigned char  skew;
	unsigned char  gap1;
	unsigned char  gap2;
	unsigned char  sparecyl;
	unsigned short pcylcount;
	unsigned short head_vol0;
	unsigned short ntrks;	/* tracks in cyl 0 or vol 0 */
	unsigned char  cmd_tag_queue_depth;
	unsigned char  unused0;
	unsigned short unused1;
	unsigned short nsect;	/* sectors/tracks in cyl 0 or vol 0 */
	unsigned short bytes;
	unsigned short ilfact;
	unsigned int   flags;		/* controller flags */
	unsigned int   datarate;
	unsigned int   retries_on_error;
	unsigned int   ms_per_word;
	unsigned short xylogics_gap1;
	unsigned short xylogics_syncdelay;
	unsigned short xylogics_readdelay;
	unsigned short xylogics_gap2;
	unsigned short xylogics_readgate;
	unsigned short xylogics_writecont;
};

#define	SGI_VOLHDR	0x00
/* 1 and 2 were used for drive types no longer supported by SGI */
#define	SGI_SWAP	0x03
/* 4 and 5 were for filesystem types SGI haven't ever supported on MIPS CPUs */
#define	SGI_VOLUME	0x06
#define	SGI_EFS		0x07
#define	SGI_LVOL	0x08
#define	SGI_RLVOL	0x09
#define	SGI_XFS		0x0a
#define	SGI_XFSLOG	0x0b
#define	SGI_XLV		0x0c
#define	SGI_XVM		0x0d
#define	ENTIRE_DISK	SGI_VOLUME
/*
 * controller flags
 */
#define	SECTOR_SLIP	0x01
#define	SECTOR_FWD	0x02
#define	TRACK_FWD	0x04
#define	TRACK_MULTIVOL	0x08
#define	IGNORE_ERRORS	0x10
#define	RESEEK		0x20
#define	CMDTAGQ_ENABLE	0x40

typedef struct {
	unsigned int   magic;		 /* expect SGI_LABEL_MAGIC */
	unsigned short boot_part;        /* active boot partition */
	unsigned short swap_part;        /* active swap partition */
	unsigned char  boot_file[16];    /* name of the bootfile */
	struct device_parameter devparam;	/*  1 * 48 bytes */
	struct volume_directory {		/* 15 * 16 bytes */
		unsigned char vol_file_name[8];	/* a character array */
		unsigned int  vol_file_start;	/* number of logical block */
		unsigned int  vol_file_size;	/* number of bytes */
	} directory[15];
	struct sgi_partition {			/* 16 * 12 bytes */
		unsigned int num_sectors;	/* number of blocks */
		unsigned int start_sector;	/* must be cylinder aligned */
		unsigned int id;
	} partitions[16];
	unsigned int   csum;
	unsigned int   fillbytes;
} sgi_partition;

typedef struct {
	unsigned int   magic;		/* looks like a magic number */
	unsigned int   a2;
	unsigned int   a3;
	unsigned int   a4;
	unsigned int   b1;
	unsigned short b2;
	unsigned short b3;
	unsigned int   c[16];
	unsigned short d[3];
	unsigned char  scsi_string[50];
	unsigned char  serial[137];
	unsigned short check1816;
	unsigned char  installer[225];
} sgiinfo;

#define	SGI_LABEL_MAGIC		0x0be5a941
#define	SGI_LABEL_MAGIC_SWAPPED	0x41a9e50b
#define	SGI_INFO_MAGIC		0x00072959
#define	SGI_INFO_MAGIC_SWAPPED	0x59290700

#define SSWAP16(x) (other_endian ? swab16(x) : (uint16_t)(x))
#define SSWAP32(x) (other_endian ? swab32(x) : (uint32_t)(x))

/* fdisk.c */
#define sgilabel ((sgi_partition *)MBRbuffer)
#define sgiparam (sgilabel->devparam)

/* fdisksgilabel.c */
extern struct	systypes sgi_sys_types[];
extern void	sgi_nolabel( void );
extern int	check_sgi_label( void );
extern void	sgi_list_table( int xtra );
extern int  sgi_change_sysid( int i, int sys );
extern unsigned int	sgi_get_start_sector( int i );
extern unsigned int	sgi_get_num_sectors( int i );
extern int	sgi_get_sysid( int i );
extern void	sgi_delete_partition( int i );
extern void	sgi_add_partition( int n, int sys );
extern void	create_sgilabel( void );
extern void	create_sgiinfo( void );
extern int	verify_sgi( int verbose );
extern void	sgi_write_table( void );
extern void	sgi_set_ilfact( void );
extern void	sgi_set_rspeed( void );
extern void	sgi_set_pcylcount( void );
extern void	sgi_set_xcyl( void );
extern void	sgi_set_ncyl( void );
extern void	sgi_set_bootpartition( int i );
extern void	sgi_set_swappartition( int i );
extern int	sgi_get_bootpartition( void );
extern int	sgi_get_swappartition( void );
extern void	sgi_set_bootfile( const char* aFile );
extern const char *sgi_get_bootfile( void );

#endif /* FDISK_SGI_LABEL_H */

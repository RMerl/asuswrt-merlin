/************************************************************************/
/*                                                                      */
/*  AMD CFI Enabled Flash Memory Drivers                                */
/*  File name: CFIFLASH.H                                               */
/*  Revision:  1.0  5/07/98                                             */
/*                                                                      */
/* Copyright (c) 1998 ADVANCED MICRO DEVICES, INC. All Rights Reserved. */
/* This software is unpublished and contains the trade secrets and      */
/* confidential proprietary information of AMD. Unless otherwise        */
/* provided in the Software Agreement associated herewith, it is        */
/* licensed in confidence "AS IS" and is not to be reproduced in whole  */
/* or part by any means except for backup. Use, duplication, or         */
/* disclosure by the Government is subject to the restrictions in       */
/* paragraph (b) (3) (B) of the Rights in Technical Data and Computer   */
/* Software clause in DFAR 52.227-7013 (a) (Oct 1988).                  */
/* Software owned by                                                    */
/* Advanced Micro Devices, Inc.,                                        */
/* One AMD Place,                                                       */
/* P.O. Box 3453                                                        */
/* Sunnyvale, CA 94088-3453.                                            */
/************************************************************************/
/*  This software constitutes a basic shell of source code for          */
/*  programming all AMD Flash components. AMD                           */
/*  will not be responsible for misuse or illegal use of this           */
/*  software for devices not supported herein. AMD is providing         */
/*  this source code "AS IS" and will not be responsible for            */
/*  issues arising from incorrect user implementation of the            */
/*  source code herein. It is the user's responsibility to              */
/*  properly design-in this source code.                                */
/*                                                                      */ 
/************************************************************************/
#ifndef _CFIFLASH_H
#define _CFIFLASH_H

#if defined __cplusplus
extern "C" {
#endif

/* include board/CPU specific definitions */
#include "bcmtypes.h"
#include "board.h"

#define FLASH_BASE_ADDR_REG FLASH_BASE

#define FAR

#ifndef NULL
#define NULL 0
#endif

#define MAXSECTORS  128      /* maximum number of sectors supported */

/* A structure for identifying a flash part.  There is one for each
 * of the flash part definitions.  We need to keep track of the
 * sector organization, the address register used, and the size
 * of the sectors.
 */
struct flashinfo {
	 char *name;         /* "Am29DL800T", etc. */
	 unsigned long addr; /* physical address, once translated */
	 int areg;           /* Can be set to zero for all parts */
	 int nsect;          /* # of sectors -- 19 in LV, 22 in DL */
	 int bank1start;     /* first sector # in bank 1 */
	 int bank2start;     /* first sector # in bank 2, if DL part */
 struct {
	long size;           /* # of bytes in this sector */
	long base;           /* offset from beginning of device */
	int bank;            /* 1 or 2 for DL; 1 for LV */
	 } sec[MAXSECTORS];  /* per-sector info */
};

/*
 * This structure holds all CFI query information as defined
 * in the JEDEC standard. All information up to 
 * primary_extended_query is standard among all amnufactures
 * with CFI enabled devices.
 */

struct cfi_query {
	char query_string[4];		/* Should be 'QRY' */
	WORD oem_command_set;           /* Command set */
	WORD primary_table_address;     /* Addy of entended table */
	WORD alt_command_set;           /* Alt table */
	WORD alt_table_address;         /* Alt table addy */
	int vcc_min;                  /* Vcc minimum */
	int vcc_max;			/* Vcc maximum */
	int vpp_min;			/* Vpp minimum, if supported */
	int vpp_max;			/* Vpp maximum, if supported */
	int timeout_single_write;       /* Time of single write */
	int timeout_buffer_write;	/* Time of buffer write */
	int timeout_block_erase;	/* Time of sector erase */
	int timeout_chip_erase;		/* Time of chip erase */
	int max_timeout_single_write;   /* Max time of single write */
	int max_timeout_buffer_write; 	/* Max time of buffer write */
	int max_timeout_block_erase;	/* Max time of sector erase */
	int max_timeout_chip_erase; 	/* Max time of chip erase */
	long device_size;		/* Device size in bytes */
	WORD interface_description;	/* Interface description */
	int max_multi_byte_write;	/* Time of multi-byte write */
	int num_erase_blocks;		/* Number of sector defs. */
	struct {
	  unsigned long sector_size;	/* byte size of sector */
	  int num_sectors;		/* Num sectors of this size */
	} erase_block[8];		/* Max of 256, but 8 is good */
	char primary_extended_query[4];	/* Vendor specific info here */
	WORD major_version;		/* Major code version */
	WORD minor_version;		/* Minor code version */
	BYTE sensitive_unlock;		/* Is byte sensitive unlock? */
	BYTE erase_suspend;		/* Capable of erase suspend? */
	BYTE sector_protect;		/* Can Sector protect? */
	BYTE sector_temp_unprotect;	/* Can we temporarily unprotect? */
	BYTE protect_scheme;		/* Scheme of unprotection */
	BYTE is_simultaneous;		/* Is a smulataneous part? */
	BYTE is_burst;			/* Is a burst mode part? */
	BYTE is_page;			/* Is a page mode part? */
};

/* Standard Boolean declarations */
#define TRUE 				1
#define FALSE 				0

/* Command codes for the flash_command routine */
#define FLASH_SELECT    0       /* no command; just perform the mapping */
#define FLASH_RESET     1       /* reset to read mode */
#define FLASH_READ      1       /* reset to read mode, by any other name */
#define FLASH_AUTOSEL   2       /* autoselect (fake Vid on pin 9) */
#define FLASH_PROG      3       /* program a word */
#define FLASH_CERASE    4       /* chip erase */
#define FLASH_SERASE    5       /* sector erase */
#define FLASH_ESUSPEND  6       /* suspend sector erase */
#define FLASH_ERESUME   7       /* resume sector erase */
#define FLASH_UB        8       /* go into unlock bypass mode */
#define FLASH_UBPROG    9       /* program a word using unlock bypass */
#define FLASH_UBRESET   10      /* reset to read mode from unlock bypass mode */
#define FLASH_CFIQUERY  11      /* CFI query */
#define FLASH_LASTCMD   11      /* used for parameter checking */

/* Return codes from flash_status */
#define STATUS_READY    0       /* ready for action */
#define STATUS_BUSY     1       /* operation in progress */
#define STATUS_ERSUSP   2       /* erase suspended */
#define STATUS_TIMEOUT  3       /* operation timed out */
#define STATUS_ERROR    4       /* unclassified but unhappy status */

/* Used to mask of bytes from word data */
#define HIGH_BYTE(a) (a >> 8)
#define LOW_BYTE(a)  (a & 0xFF)

/* AMD's manufacturer ID */
#define AMDPART   	0x01

/* A list of 4 AMD device ID's - add others as needed */
#define ID_AM29DL800T   0x224A
#define ID_AM29DL800B   0x22CB
#define ID_AM29LV800T   0x22DA
#define ID_AM29LV800B   0x225B
#define ID_AM29LV400B   0x22BA

#define ID_AM29LV160B   0x2249      // 2MG R board BottomBoot
#define ID_AM29LV160T   0x22C4      // 2MG R board topboot
#define ID_AM29LV320T   0x22F6      // 4MG SV board TB
#define ID_AM29LV320B   0x22F9      // 4MG SV baord BB 

/* An index into the memdesc organization array.  Is set by init_flash */
/* so the proper sector tables are used in the correct device          */
/* Add others as needed, and as added to the device ID section         */

#define AM29DL800T 	0
#define AM29DL800B 	1
#define AM29LV800T 	2
#define AM29LV800B 	3
#define AM29LV160B 4
#define AM29LV400B 5


extern BYTE flash_init(void);
extern BYTE flash_reset(void);

extern BYTE flash_ub(BYTE sector);
extern int flash_write_buf_ub(BYTE sector, int offset,
                           BYTE *buffer, int numbytes);
extern BYTE flash_reset_ub(void);

extern BYTE flash_write_word(BYTE sector, int offset, WORD data);
extern int flash_write_buf(BYTE sector, int offset, 
                        BYTE *buffer, int numbytes);
extern BYTE flash_sector_erase(BYTE sector);
extern BYTE flash_erase_suspend(BYTE sector);
extern BYTE flash_erase_resume(BYTE sector);

extern BYTE flash_sector_erase_int(BYTE sector);

extern int flash_get_numsectors(void);
extern int flash_get_sector_size(BYTE sector);
extern int flash_get_total_size(void);
extern BYTE flash_sector_protect_verify(BYTE sector);
extern int flash_read_buf(byte sector, int offset, byte *buffer, int numbytes);
extern unsigned char *flash_get_memptr(byte sector);
extern int flash_get_blk(int addr);

#if defined __cplusplus
}
#endif

#endif

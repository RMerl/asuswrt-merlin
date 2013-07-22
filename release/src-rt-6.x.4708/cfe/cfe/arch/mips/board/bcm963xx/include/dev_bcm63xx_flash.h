/***************************************************************************
 * Broadcom Corp. Confidential
 * Copyright 2001 Broadcom Corp. All Rights Reserved.
 *
 * THIS SOFTWARE MAY ONLY BE USED SUBJECT TO AN EXECUTED 
 * SOFTWARE LICENSE AGREEMENT BETWEEN THE USER AND BROADCOM. 
 * YOU HAVE NO RIGHT TO USE OR EXPLOIT THIS MATERIAL EXCEPT 
 * SUBJECT TO THE TERMS OF SUCH AN AGREEMENT.
 *
 ***************************************************************************
 * File Name  : dev_bcm63xx_flash.h 
 *
 * Created on :  04/18/2002  seanl
 ***************************************************************************/

#if !defined(_DEV_BCM63XX_FLASH_)
#define _DEV_BCM63XX_FLASH_

#include "bcmtypes.h"
#include "board.h"

#define ONEK                            1024
#define BLK64K                          (64*ONEK)
#define FLASH63XX_ADDR_BOOT_ROM         ((char *) FLASH_BASE)

// nvram and psi flash definitions for 45
#define FLASH45_BLKS_BOOT_ROM           3
#define FLASH45_LENGTH_BOOT_ROM         (FLASH45_BLKS_BOOT_ROM * BLK64K)

// Used for RTEMS.
#define FLASH45_IMAGE_START_ADDR        (FLASH_BASE + FLASH45_LENGTH_BOOT_ROM)

    
// nvram and psi flash definitions for 5x
#define FLASH5X_BLKS_BOOT_ROM           2
#define FLASH5X_LENGTH_BOOT_ROM         (FLASH5X_BLKS_BOOT_ROM * BLK128K)
    

/*****************************************************************************/
/*       NVRAM definition                                                    */
/*****************************************************************************/
#define NVRAM_LENGTH                    ONEK            // 1k nvram 
#define NVRAM_VERSION_NUMBER            1
#define NVRAM_VERSION_NUMBER_ADDRESS    0

#define NVRAM_BOOTLINE_LEN              256
#define NVRAM_BOARD_ID_STRING_LEN       16
#define NVRAM_MAC_ADDRESS_LEN           6
#define NVRAM_MAC_COUNT_MAX             32

#define NVRAM_PSI_MIN_SIZE              1       // min 1k
#define NVRAM_PSI_MAX_SIZE              128     // max 128k
#define NVRAM_PSI_DEFAULT_6345          15      // default 15k for 6345
#define NVRAM_PSI_DEFAULT_635X          127     // default 127k for 635X
#define NVRAM_MAC_COUNT_DEFAULT_6345    3
#define NVRAM_MAC_COUNT_DEFAULT_635X    5

typedef struct
{
    unsigned long ulVersion;
    char szBootline[NVRAM_BOOTLINE_LEN];
    char szBoardId[NVRAM_BOARD_ID_STRING_LEN];
    unsigned long ulPsiSize;
    unsigned long ulEnetModeFlag; 
    unsigned long ulNumMacAddrs;
    unsigned char ucaBaseMacAddr[NVRAM_MAC_ADDRESS_LEN];
    char chReserved[2];
    unsigned long ulCheckSum;
} NVRAM_DATA, *PNVRAM_DATA;


typedef struct flashaddrinfo
{
    int flash_persistent_start_blk;
    int flash_persistent_number_blk;
    int flash_persistent_length;
    unsigned long flash_persistent_blk_offset;

    int flash_nvram_start_blk;
    int flash_nvram_number_blk;
    int flash_nvram_length;
    unsigned long flash_nvram_blk_offset;
} FLASH_ADDR_INFO, *PFLASH_ADDR_INFO;

void kerSysFlashInit(void);
void kerSysFlashAddrInfoGet(PFLASH_ADDR_INFO pflash_addr_info);
int kerSysNvRamSet(char *string,int strLen,int offset);
int kerSysNvRamGet(char *string,int strLen,int offset);
int kerSysBcmImageSet( int flash_start_addr, char *string, int size, int fWholeImage);
void kerSysMipsSoftReset(void);
int kerSysErasePsi(void);
int kerSysEraseNvRam(void);
int kerSysPsiSet(char *string,int strLen,int offset);
int kerSysPsiGet(char *string,int strLen,int offset);
int kerSysMemoryTypeSet(int flash_start_addr, char *string, int size);
int kerSysMemoryTypeGet(void);
void dumpHex(unsigned char *start, int len);


#ifndef _CFIFLASH_H
extern BYTE flash_reset(void);
extern int flash_get_numsectors(void);
extern BYTE flash_sector_erase_int(BYTE sector);
extern unsigned char *flash_get_memptr(byte sector);
extern int flash_get_blk(int addr);
#endif

#endif /* _DEV_BCM63XX_FLASH_ */

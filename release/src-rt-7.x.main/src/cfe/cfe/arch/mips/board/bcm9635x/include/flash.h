/*
<:copyright-gpl 

 Copyright 2002 Broadcom Corp. All Rights Reserved. 
 
 This program is free software; you can distribute it and/or modify it 
 under the terms of the GNU General Public License (Version 2) as 
 published by the Free Software Foundation. 
 
 This program is distributed in the hope it will be useful, but WITHOUT 
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
 for more details. 
 
 You should have received a copy of the GNU General Public License along 
 with this program; if not, write to the Free Software Foundation, Inc., 
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA. 

:>
*/
//**************************************************************************
//    Filename: flash.h
//    Author:   Dannie Gay
//    Creation Date: 2-mar-97
//
//**************************************************************************
//    Description:
//
//      CM MAC FLASH Memory definitions for Intel 28F016SV Flash.
//      
//**************************************************************************
//    Revision History:
//      0.1 add support for AMD flash. 10-15-99 ytl
//
//**************************************************************************
#ifndef flash_h
#define flash_h


#if defined __cplusplus
extern "C" {
#endif

/* include board/CPU specific definitions */
#include "bcmtypes.h"
#include "board.h"

/* macro to convert logical data addresses to physical */
/* DMA hardware must see physical address */
#define LtoP( x )       ( (uint32)x & 0x1fffffff )
#define PtoL( x )       ( LtoP(x) | 0xa0000000 )

#define FLASH_BASE_ADDR_REG FLASH_BASE

/* Flash block lock request */
#define UNLOCK      0
#define LOCK        1


// this information is returned by the function DetectFlashInfo. DetectFlashInfo should only be
// called after the flash has been detected.
typedef struct _FLASH_INFO
{
    unsigned long  flash_base;                 // the base of the flash
    unsigned long  flash_block_size;           // size of each block
    unsigned long  flash_block_mask;           // mask of the block
    unsigned short flash_block_total;          // total number of blocks
    unsigned short flashId;                    //  yt flash id
} FLASH_INFO;
    

//------------------------------------------------------------------
// Flash Block (word) Size
//------------------------------------------------------------------
#define  FLASH64K_BLOCK_SIZE           0x10000      //64K bytes
#define  FLASH64K_BLOCK_SIZE_MASK      0xffff0000L
#define  TOTAL64K_FLASH_BLOCKS         64 // 32

#define  STRATA_FLASH_BLOCK_SIZE           0x20000      //128K bytes
#define  STRATA_FLASH_BLOCK_SIZE_MASK      0xfffe0000L
#define  STRATA_FLASH64_TOTAL_FLASH_BLOCKS         32 // onlly 32 can be used 64 
#define  STRATA_FLASH32_TOTAL_FLASH_BLOCKS         32 

/* Strata Flash Specific Defines */
#define STRATA32_PHYS_FLASH_BASE             0x1FC00000
#define STRATA64_PHYS_FLASH_BASE             0x1FC00000

#define STRATA32_FLASH_BASE                  PHYS_TO_K1(STRATA32_PHYS_FLASH_BASE)
#define STRATA64_FLASH_BASE                  PHYS_TO_K1(STRATA64_PHYS_FLASH_BASE)

//------------------------------------------------------------------
// manufacturer/device ID codes
//------------------------------------------------------------------
#define  STRATA_FLASH_MAN_CODE      0x0089
#define  STRATA_FLASH32_DEV_CODE    0x0016
#define  STRATA_FLASH64_DEV_CODE    0x0017

#define  FLASH_ID   ((FLASH_AMD29LV160B_MAN_CODE<<8)|FLASH_AMD29LV160BB_DEV_CODE)

#define  FLASH_016_MAN_CODE         0x0089
#define  FLASH_016_DEV_CODE         0x00a0

#define  FLASH_160_MAN_CODE         0x00b0
#define  FLASH_160_DEV_CODE         0x00d0

#define  FLASH_320_MAN_CODE         0x00b0
#define  FLASH_320_DEV_CODE         0x00d4

#define  FLASH_28F320C3_MAN_CODE    0x0089
#define  FLASH_28F320C3_DEV_CODE    0x00C5

#define  FLASH_AMD29LV160B_MAN_CODE     0x0001
#define  FLASH_AMD29LV160BB_DEV_CODE    0x0049
#define  FLASH_AMD29LV160BT_DEV_CODE    0x00c4

#define  FLASH_AMD29LV160BB_ID  ((FLASH_AMD29LV160B_MAN_CODE<<8)|FLASH_AMD29LV160BB_DEV_CODE)

//------------------------------------------------------------------
// error codes
//------------------------------------------------------------------
#define  NO_ERROR                   0
#define  VPP_LOW                    1
#define  OP_ABORTED                 2
#define  BLOCK_LOCKED               3
#define  COMMAND_SEQ_ERROR          4
#define  WP_LOW                     5
#define  OPERATION_STATUS_ERR       0x2020

//------------------------------------------------------------------
// Flash (28F00SA) Compatible Commands
//------------------------------------------------------------------
#define  ALT_WORD_WRITE_CMD         0x1010
#define  SINGLE_BLOCK_ERASE_CMD     0x2020
#define  WORD_WRITE_CMD             0x4040
#define  CLR_STATUS_REGS_CMD        0x5050
#define  READ_CSR_CMD               0x7070
#define  READ_ID_CODES_CMD          0x9090
#define  ERASE_SUSPEND_CMD          0xb0b0
#define  CONFIRM_RESUME_CMD         0xd0d0
#define  READ_ARRAY_CMD             0xffff
#define  CFG_SETUP_CMD              0x6060
#define  BLOCK_LOCK_CMD             0x0101
#define  BLOCK_UNLOCK_CMD           0xd0d0


//------------------------------------------------------------------
// Flash (28F016SA) Performance-Enhancement Commands
//------------------------------------------------------------------
#define  PAGE_BUF_WRITE_CMD         0x0c0c   
#define  ESR_READ_CMD               0x7171
#define  PAGE_BUF_SWAP_CMD          0x7272
#define  SINGLE_LD_PAGE_BUF_CMD     0x7474
#define  READ_PAGE_BUF_CMD          0x7575
#define  LOCK_BLOCK_CMD             0x7777
#define  ABORT_CMD                  0x8080
#define  UPL0AD_BSRS_WLCKBIT_CMD    0x9797
#define  UPLOAD_DEV_INFO_CMD        0x9999
#define  ERASE_ALL_UNLCK_BLKS_CMD   0xa7a7
#define  SEQ_LOAD_PAGE_BUF_CMD      0xe0e0
#define  SLEEP_CMD                  0xf0f0

//------------------------------------------------------------------
// Flash (28F160S5) Commands and error codes
//------------------------------------------------------------------
#define READ_SR_CMD                 0x7070
#define WRITE_BUFFER_CMD            0xe8e8
#define OPERATION_STATUS_ERR_2      0x0030

//------------------------------------------------------------------
// Various Bit Masks
//------------------------------------------------------------------
#define  BIT_0                      0x0001
#define  BIT_1                      0x0002
#define  BIT_2                      0x0004
#define  BIT_3                      0x0008
#define  BIT_4                      0x0010
#define  BIT_5                      0x0020
#define  BIT_6                      0x0040
#define  BIT_7                      0x0080

#define  LOW_BYTE                   0x00ff
#define  HIGH_BYTE                  0xff00


//------------------------------------------------------------------
// Flash (AMD29LV160B) Commands Sequence
//------------------------------------------------------------------

//macros for reading and writing to AMD flash directly using flashBasePtr as base pointer
#define AMDFLASH_READ_RAW(_addr)            ( *( flashBasePtr + _addr ) )
#define AMDFLASH_WRITE_RAW(_sword, _addr)   ( *( flashBasePtr + _addr ) = _sword )

#define  AMD_MAN_ID_ADDR        0x00
#define  AMD_DEV_ID_ADDR        0x01
#define  AMD_SEC_PROTECT_ADDR   0x02

#define  AMD_AUTOSELECT_CMD     0x9090
#define  AMD_PROGRAM_CMD        0xa0a0
#define  AMD_ULBYPASS_MOD_CMD   0x2020
#define  AMD_ULBYPASS_PGM_CMD   0xa0a0
#define  AMD_ULBYPASS_RST_CMD   0x9090


#define  AMD_ERASE_CMD          0x8080
#define  AMD_CHIP_ERASE_CMD     0x1010
#define  AMD_SECTOR_ERASE_CMD   0x3030

#define  AMD_ERASE_SUSPEND_CMD  0xb0b0
#define  AMD_ERASE_RESUME_CMD   0x3030

#define  AMD_RESET_CMD          0xf0f0


#define  DQ2    0x04
#define  DQ5    0x20
#define  DQ6    0x40
#define  DQ7    0x80

#define  B6B2TOGGLE (DQ6+DQ2)

//------------------------------------------------------------------
// Functions
//------------------------------------------------------------------
// Debug macro: magical dissapearing BDGPrint() funcion for release built.
// Place BDGPrint() function anywhere during development for console debug.
// For production build, just undefine DEBUG_BUILD, the BDGPrint() will disappear.
// No need to comment out BDGPrint() statements in the code.
// Comment the following for release build, Uncomment the following for debug build

//#define DEBUG_BUILD

// usage
#ifdef DEBUG_BUILD
#define BDGPrint(x) Print x
#else
#define BDGPrint(x)
#endif


// flash read array macro
//#define READ_ARRAY  *((unsigned short *)FLASH_BASE_ADDR_REG) = READ_ARRAY_CMD
#define READ_ARRAY  Set_Read_Array()

extern void Delayms(uint16 n);

void sDelay(unsigned long cnt);
unsigned short * Base(unsigned short * dataPtr);
int BlkWriteProtect(unsigned short blk, int on);
int AddrWriteProtect(unsigned short * dest, int on);
int FlashBlockLock(volatile unsigned short * blockBase, int on);
int IsBlockProtected(unsigned short blk);
int EraseFlashAddressBlock(unsigned short * dest, unsigned short * esr);
int EraseFlashBlock(unsigned short blk);
int FlashWordWrite(unsigned short * dest, unsigned short * src, unsigned long count, unsigned short * esr);
unsigned short SuspendEraseToReadArray(unsigned short * dataPtr, unsigned short * result);
unsigned char ReadFlashCsrStatus(void);
unsigned short ReadDeviceInformation(void);
unsigned short ReadFlashIdCodes(void);
//dg-mod, added debugMsg parm, don't need AutoDetectFlash2() anymore
void AutoDetectFlash(unsigned char debugMsg);
void DetectFlashInfo(FLASH_INFO *); 
void Set_Read_Array(void);

unsigned char flash_init(void);
unsigned char flash_reset(void);
unsigned char flash_sector_erase_int(unsigned char sector);
int flash_get_numsectors(void);
int flash_get_sector_size( unsigned char sector );
unsigned char *flash_get_memptr( unsigned char sector );
int flash_get_blk( int addr );
int flash_get_total_size(void);
#if defined __cplusplus
}
#endif

#endif

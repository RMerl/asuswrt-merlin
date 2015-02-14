/*
<:copyright-broadcom 
 
 Copyright (c) 2002 Broadcom Corporation 
 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          16215 Alton Parkway 
          Irvine, California 92619 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
//**************************************************************************
//    Filename: flash.c
//    Author:   Dannie Gay
//    Creation Date: 2-mar-97
//
//**************************************************************************
//    Description:
//
//              CM MAC FLASH Memory access routines.
//              
//**************************************************************************
//    Revision History:
//              0.1 initial revision
//              0.2 support for 28016 Flash
//              0.3 BCM3300I MIPS port
//              0.4 add support for AMD flash. 10-15-99 ytl
//
//**************************************************************************

/* Includes. */
#include "lib_types.h"
#include "lib_string.h"
#include "lib_printf.h"
#include "cfe_timer.h"
#include "sbmips.h"
#include "flash.h"

extern void udelay(long us);

/* Globals. */
// Pointer to base (word) of Flash Memory
unsigned short * flashBasePtr = (unsigned short *)FLASH_BASE_ADDR_REG;

static unsigned char s3s5Device=FALSE;  // flag indicating new flash device
static unsigned short flashId=FALSE;                    // flashId read from device
static unsigned short TRead_Array_Cmd=FALSE;    // runtime cmd for function Set_Read_Array 

int nTOTAL_FLASH_BLOCKS;

// storage to hold flash information
static FLASH_INFO flashinfo;

//local function pointer that assigned dynamically to flash routines during startup
static int (*TEraseFlashAddressBlock)(unsigned short *, unsigned short * ) = 0;
static unsigned short (*TEraseAllUnlockFlashBlocks)(unsigned short * , long * ) = 0;
static unsigned short (*TSuspendEraseToReadArray)(unsigned short *, unsigned short *) = 0;
static int (*TFlashWordWrite)(unsigned short *, unsigned short *, unsigned long, unsigned short *) = 0;
static unsigned short (*TReadFlashIdCodes)(void) = 0;

int IntelEraseFlashAddressBlock(unsigned short * dest, unsigned short * esr);
int AMDEraseFlashAddressBlock(unsigned short * dest, unsigned short * esr);
unsigned short EraseAllUnlockFlashBlocks(unsigned short * basePtr, long * failureList);
unsigned short IntelEraseAllUnlockFlashBlocks( unsigned short * basePtr, long * failureList);
unsigned short AMDEraseAllUnlockFlashBlocks(unsigned short * basePtr, long * failureList);
int IntelFlashWordWrite(unsigned short * dest, unsigned short * src, unsigned long count, unsigned short * esr);
int AMDFlashWordWrite(unsigned short * dest, unsigned short * src, unsigned long count, unsigned short * esr);
unsigned short IntelSuspendEraseToReadArray(unsigned short * dataPtr, unsigned short * result);
unsigned short AMDSuspendEraseToReadArray(unsigned short * dataPtr, unsigned short * result);
unsigned short ReadIntelFlashIdCodes(void);
unsigned short ReadAMDFlashIdCodes(void);
void AutoDetectFlash(unsigned char debugMsg);
void DetectFlashInfo(FLASH_INFO * autoflashinfo);



/************************************************************************
   Base

        Description:   Given a word address in the Flash - return a
                  word pointer to the base of the corresponding
                  32K (word) block.
                  (note: Base of flash resides on a 1Meg boundary)
        
        Paramters:

         unsigned short * dataPtr  - pointer to a word within a 32K block

        Return:
         unsigned short * - blkBasePtr - pointer to block base

************************************************************************/
unsigned short *
Base(unsigned short * dataPtr)
{
    if( flashId == FLASH_28F320C3_DEV_CODE )
    {
        if (((unsigned long)dataPtr - FLASH_BASE) < 0x10000)
            return (unsigned short *)((unsigned long)dataPtr & 0xffffe000);
        else
            return (unsigned short *)((unsigned long)dataPtr & flashinfo.flash_block_mask);
    }
    else
        return (unsigned short *)((unsigned long)dataPtr & flashinfo.flash_block_mask);
}

/************************************************************************

   WriteProtect

        Description:   Turn on/off write protection to Flash
        
        Paramters:

         int       1 turns write protect on,
                   0 turns write protect off.    

        Return:

         int - NULL = success, otherwise error.

************************************************************************/
int 
BlkWriteProtect(unsigned short blk, int on)
{
    unsigned long blkBasePtr;
    volatile unsigned short * blockBase;
    unsigned short * dest;

    if (s3s5Device)
    {
        if( flashId == FLASH_28F320C3_DEV_CODE )
        {
            const unsigned short FLASH_64K_BLKS_TOTAL = 63;
            const unsigned short FLASH_8K_BLKS_TOTAL = 8;
            /* max block = FLASH_64K_BLKS_TOTAL * 64K + 8 blks * 8K */
            if (blk >= (FLASH_64K_BLKS_TOTAL + FLASH_8K_BLKS_TOTAL))
                return -1;

            if (blk < 8)
                blkBasePtr = flashinfo.flash_base + (blk * 0x2000);
            else
                blkBasePtr = flashinfo.flash_base + ((blk - 7) * 0x10000);
        }
        else
        {
            if (blk >= flashinfo.flash_block_total)
                return -1;

            blkBasePtr = (unsigned long)flashinfo.flash_base +  
                (unsigned long)(blk * flashinfo.flash_block_size);
        }

        //------------------------------------------------------------------
        // Convert block number to block base address and issue erase
        //------------------------------------------------------------------
        dest = (unsigned short *)blkBasePtr;
        blockBase = Base(dest);

        return (FlashBlockLock(blockBase, on));
    }

    return 0;
}

int 
AddrWriteProtect(unsigned short * dest, int on)
{
    volatile unsigned short * blockBase;

    if (s3s5Device)
    {
        if( flashId == FLASH_28F320C3_DEV_CODE )
        {
            if (dest >= (unsigned short *)(FLASH_BASE + 
                (flashinfo.flash_block_total * flashinfo.flash_block_size)))
            {
                return -1;
            }
        }
        else
        {
            if (dest >= (unsigned short *)(FLASH_BASE + 
                (flashinfo.flash_block_total * flashinfo.flash_block_size)))
            {
                return -1;
            }
        }

        blockBase = Base(dest);

        return (FlashBlockLock(blockBase, on));
    }
    return 0;
}

int FlashBlockLock(volatile unsigned short * blockBase, int on)
{
    unsigned short cmd;
    volatile unsigned short esr;
    int j;

    if (on)
    { // set block lock bit
        cmd = BLOCK_LOCK_CMD;
    }
    else
    { // clear block lock bit
        cmd = BLOCK_UNLOCK_CMD;
    }
    //------------------------------------------------------------------
    // Issue Single Block lock at block address
    //------------------------------------------------------------------
    *blockBase = CFG_SETUP_CMD;
    *blockBase = cmd;
                
    //------------------------------------------------------------------
    // Poll SR.7 until block ready
    // Clear Status Register, reset device to read array mode
    //------------------------------------------------------------------

    /* according to the spec, this should take max of 0.7 second;
       typically, it takes about 0.5 sec.  So, let's poll this
       every 0.5 sec; allow max of 3 times.  */
    for (j = 0; j < 3; j++)
    {
        cfe_sleep(CFE_HZ/2);
        if (BIT_7 & *(blockBase+1))
            break;
    }

    if (j == 3)
    {
        printf("FlashBlockLock(): SR.7 is not set after about 1.5 second.  Timeout!\n");
        return -1;
    }

    esr = *(blockBase+1);
    
    *blockBase = CLR_STATUS_REGS_CMD;
    *blockBase = READ_ARRAY_CMD;
        
    //------------------------------------------------------------------
    // Check for any errors
    //------------------------------------------------------------------
    if (esr & OPERATION_STATUS_ERR_2)
    {
            *blockBase = READ_ARRAY_CMD;
            return -1;
    }      
    else
    {
            *blockBase = READ_ARRAY_CMD;
            return 0;
    }      
}

/************************************************************************

   IsBlockProtected

        Description:   Read block lock status
        
        Paramters:

        unsigned short blk - block number

        Return:

        int        1 block protect on,
                   0 block protect off.    

************************************************************************/
int IsBlockProtected(unsigned short blk)
{
    unsigned long blkBasePtr;
    volatile unsigned short * blockBase;
    unsigned short * dest;
    volatile unsigned short LockStatus = 0;

    if (s3s5Device)
    {

        if( flashId == FLASH_28F320C3_DEV_CODE )
        {
            if (blk < 8)
                blkBasePtr = flashinfo.flash_base + (blk * 0x2000);
            else
                blkBasePtr = flashinfo.flash_base + ((blk - 7) * 0x10000);
        }
        else
        {
            blkBasePtr = (unsigned long) flashinfo.flash_base + (unsigned long)
                (blk * flashinfo.flash_block_size);
        }

        //------------------------------------------------------------------
        // Convert block number to block base address and issue erase
        //------------------------------------------------------------------
        dest = (unsigned short *)blkBasePtr;
        blockBase = Base(dest);

        //------------------------------------------------------------------
        *blockBase = READ_ID_CODES_CMD;
        LockStatus = *(blockBase+2);

        *blockBase = READ_ARRAY_CMD;

    }

    return LockStatus;
}

/************************************************************************

   EraseFlashAddressBlock

        Description:   Erase Flash Block. The Flash is partitioned into
                  32 Blocks of 32K words each
        
        Paramters:

         unsigned short * dest - pointer to flash destination address of
                         block to erase
         unsigned short * esr - pointer for returned esr status
                  (Upper byte=GSR, Lower byte=BSR)
        Return:

         int - NULL = success, otherwise error.

************************************************************************/
int EraseFlashAddressBlock(unsigned short * dest, unsigned short * esr)
{
    BDGPrint(("EraseFlashAddressBlock %08x\n", (unsigned long)dest));

    return TEraseFlashAddressBlock(dest, esr);
}

//------------------------------------------------------------------
// Intel flash specific
//------------------------------------------------------------------
int IntelEraseFlashAddressBlock(unsigned short * dest, unsigned short * esr)
{
    int j;
    volatile unsigned short * blockBase = Base(dest);
    volatile unsigned short *tempdest = dest;
    volatile unsigned short *volesr = esr;

    //------------------------------------------------------------------
    // support both 28016SV/SA and 28160S3/S5 flash
    //------------------------------------------------------------------
    if (s3s5Device)
    {
        //------------------------------------------------------------------
        // Issue Single Block Erase at block address
        //------------------------------------------------------------------
        *tempdest = SINGLE_BLOCK_ERASE_CMD;
        *tempdest = CONFIRM_RESUME_CMD;
        
        //------------------------------------------------------------------
        // Poll SR.7 until block ready
        // Clear Status Register, reset device to read array mode
        //------------------------------------------------------------------

        /* according to the spec, it would take max of 5 sec,
           typically 1 second.   So we are giving polling once every second
           for 7 sec */

        for (j = 0; j < 7; j++)
        {
            cfe_sleep(CFE_HZ);
            if (BIT_7 & *(blockBase+1))
                break;
        }

        if (j == 7)
        {
            printf("IntelEraseFlashAddressBlock(): SR.7 is not set after about 7 seconds.  Timeout!\n");
            return -1;
        }

        *volesr = *(blockBase+1);
        
        *tempdest = CLR_STATUS_REGS_CMD;
        *tempdest = READ_ARRAY_CMD;
        
        //------------------------------------------------------------------
        // Check for any errors
        //------------------------------------------------------------------
        if (*volesr & OPERATION_STATUS_ERR_2)
        {
            *tempdest = READ_ARRAY_CMD;
            return -1;
        }      
        else
        {
            *tempdest = READ_ARRAY_CMD;
            return ((int)NULL);
        }      
    }
    else
    {
        //------------------------------------------------------------------
        // Read Extended Status Registers - Poll BSR until BSR.3 of dest
        // address=0 (queue available).
        // (note: BSR is 1 word above base of target block in status reg space)
        //------------------------------------------------------------------
        *tempdest = ESR_READ_CMD;

        for (j = 0; j < 500; j++)
        {
            udelay(10);
            if (!(BIT_3 & *(blockBase+1)))
                break;
        }

        if (j == 500)
        {
            printf("IntelEraseFlashAddressBlock(): SR.3 is clear after about 5 msec.  Timeout!\n");
            return -1;
        }

        *tempdest = SINGLE_BLOCK_ERASE_CMD;
        *tempdest = CONFIRM_RESUME_CMD;
        // issue Confirm/Resume again per latest spec errata update
        *tempdest = CONFIRM_RESUME_CMD;
        *tempdest = ESR_READ_CMD;
        
        //------------------------------------------------------------------
        // Poll BSR.7 until block ready
        // Clear Status Register, reset device to read array mode
        //------------------------------------------------------------------
        for (j = 0; j < 7; j++)
        {
            cfe_sleep(CFE_HZ);
            if  ((BIT_7 & *(blockBase+1)))
                break;
        }

        if (j == 7)
        {
            printf("IntelEraseFlashAddressBlock(): SR.7 is not set after about 7 seconds.  Timeout!\n");
            return -1;
        }

        *volesr = (*(blockBase+2) << 8) + (*(blockBase+1) & LOW_BYTE);
        
        *tempdest = CLR_STATUS_REGS_CMD;
        *tempdest = READ_ARRAY_CMD;
        
        //------------------------------------------------------------------
        // Check for any errors
        //------------------------------------------------------------------
        if (*volesr & OPERATION_STATUS_ERR)
        {
            *tempdest = READ_ARRAY_CMD;
            return -1;
        }      
        else
        {
            *tempdest = READ_ARRAY_CMD;
            return ((int)NULL);
        }      
   }
}

//------------------------------------------------------------------
// AMD flash specific
//------------------------------------------------------------------
int AMDEraseFlashAddressBlock(unsigned short * dest, unsigned short * esr)
{
    unsigned long timeout;    
    volatile unsigned short * blockBase;

    //esr not applicable to AMD flash, just set ot all F's for now
    *esr = -1;

    /* AMD erase sector command sequence ... */
    AMDFLASH_WRITE_RAW(0xaaaa, 0x555 );
    AMDFLASH_WRITE_RAW(0x5555, 0x2aa );
    AMDFLASH_WRITE_RAW(AMD_ERASE_CMD, 0x555 );
    AMDFLASH_WRITE_RAW(0xaaaa, 0x555 );
    AMDFLASH_WRITE_RAW(0x5555, 0x2aa );
    /* now the sector to erase */
    blockBase = dest ;
    *blockBase = AMD_SECTOR_ERASE_CMD;

    /* loop and wait for erase to complete. Set a timeout of 5 seconds  */
    /* so it doesn't get stuck here ... device spec says typical 0.7s   */
    for (timeout=0; timeout<50; timeout++)
    {
        unsigned short data = *blockBase ;

        /* device hasn't done yet, wait a while */
        cfe_sleep(1); /* Delayms(100); */

        /* if done bit (D7) is 1, erase is complete */
        if ((data & DQ7) == DQ7 )
        {
            BDGPrint(("AMD sector (0x%08x) erase success (Ntime=%d)!\n", (unsigned long)blockBase, timeout)); 
            return 0;
        }

        /* check for timeout condition */
        else if ( (data & DQ5) == DQ5 )      //DQ5 == 1
        {
            data = *blockBase;     
            if ( (data & DQ7) == DQ7 )
            {
                BDGPrint(("AMD sector (0x%08x) erase success (Stime=%d)!\n", (unsigned long)blockBase, timeout));                     
                return 0;
            }
            else
            {
                BDGPrint(("AMD sector (0x%08x) erase bit5 timeout!\n", (unsigned long)blockBase));
                    *blockBase = AMD_RESET_CMD ;            //reset to return to read mode
                return -1;
            }
        }

    } /* for */

    *blockBase = AMD_RESET_CMD ;            //reset to return to read mode
    BDGPrint(("Loop timeout! AMD sector (0x%08x) erase fail!\n", (unsigned long)blockBase)); 

    return -1;
}

/************************************************************************

   EraseFlashBlock

        Description:   Erase Flash Block. The Flash is partitioned into
                  32 Blocks of 32K words each
        
        Paramters:

         unsigned short blk - block number

        Return:

         int - NULL = success, otherwise error (esr status).

************************************************************************/
int EraseFlashBlock(unsigned short blk)
{
    unsigned short esr = -1;
    unsigned long blkBasePtr;

    BDGPrint(("EraseFlashBlock # %d\n", blk));

    if( (flashId==FLASH_AMD29LV160BB_ID) && !blk )  //AMD flash blk 0 needs to be handled differently
    {
        //For transperancy to the users, the first 64K byte block of the AMD flash
        //needs to be handled differently inorder to looks the same as the Intel flash
        //There are 4 sectors in the first 64K byte; 16K, 8K, 8K, 32K
        unsigned short  i, amdsa[4] = {0,16,24,32};
        for( i=0; i<4; i++ )
        {
            blkBasePtr = (unsigned long)(flashinfo.flash_base)+(unsigned long)(amdsa[i] * 1024);    //offset to the beginning of a block
            if (EraseFlashAddressBlock((unsigned short *)blkBasePtr, &esr) == -1)
                return esr;
        }       //end for
    }       //end if
    else   
    {
        //------------------------------------------------------------------
        // Convert block number to block base address and issue erase
        //------------------------------------------------------------------
        if( flashId == FLASH_28F320C3_DEV_CODE )
        {
            if (blk < 8)
                blkBasePtr = flashinfo.flash_base + (blk * 0x2000);
            else
                blkBasePtr = flashinfo.flash_base + ((blk - 7) * 0x10000);
        }
        else
            blkBasePtr = (unsigned long)flashinfo.flash_base +(unsigned long)(blk * flashinfo.flash_block_size);

        if (EraseFlashAddressBlock((unsigned short *)blkBasePtr, &esr) == -1)
        
{
printf("failed on erase on blkBasePtr 0x%x, with esr %d\n", blkBasePtr, esr);
            return esr;
}
    }       //end else

    return ((int)NULL);             //return success
}

/************************************************************************

   EraseAllUnlockFlashBlocks

        Description:   Erase All Unlocked Flash Blocks. The Flash is
                  partitioned into 32 Blocks of 32K words each.
        
        Paramters:

         unsigned short * basePtr - pointer to base of flash.
         long * failureList - map of block failures (bit position=1 for
                              block failure)
        Return:

         unsigned short - GSR, Global Status Register

************************************************************************/
unsigned short EraseAllUnlockFlashBlocks(unsigned short * basePtr, long * failureList)
{
    BDGPrint(("EraseAllUnlockFlashBlocks(chip erase)\n"));

    return TEraseAllUnlockFlashBlocks(basePtr, failureList);
}

//------------------------------------------------------------------
// Intel flash specific
//------------------------------------------------------------------
unsigned short IntelEraseAllUnlockFlashBlocks( unsigned short * basePtr, long * failureList)
{
    int j;
    unsigned short gsr, block;   
    long power=1;
    volatile unsigned short *volbasePtr = basePtr;

    //------------------------------------------------------------------
    // command not supported on 28160S3/S5 flash
    //------------------------------------------------------------------
    if (s3s5Device)
        return 0;

    //------------------------------------------------------------------
    // Return value will contain GSR in both upper and lower bytes.
    // 32 bit long word pointed to by failureList is used to return
    // map of block failures, each bit representing one block's status.
    // basePtr points to base address of Flash.
    //------------------------------------------------------------------
    *failureList = 0;
    *volbasePtr = ESR_READ_CMD;

    //------------------------------------------------------------------
    // Poll BSR.3 until queue available, then issue full chip erase cmd
    //------------------------------------------------------------------
    for (j = 0; j < 500; j++)
    {
        udelay(10);
        if (!(BIT_3 & *(volbasePtr+1)))
            break;
    }

    if (j == 500)
    {
        printf("(IntelEraseAllUnlockFlashBlocks): BSR.3 is not clear after about 5 msec.  Timeout!\n");
        return -1;
    }

    *volbasePtr = ERASE_ALL_UNLCK_BLKS_CMD;
    *volbasePtr = CONFIRM_RESUME_CMD;
    *volbasePtr = ESR_READ_CMD;

    //------------------------------------------------------------------
    // Poll GSR.7 until WSM ready, then go through all blocks and test
    // BSR.5 for operation failure - set failureList accordingly.
    //------------------------------------------------------------------
    for (j = 0; j < 7; j++)
    {
        cfe_sleep(CFE_HZ);
        if (BIT_7 & *(volbasePtr+2))
            break;
    }

    if (j == 7)
    {
        printf("(IntelEraseAllUnlockFlashBlocks): SR.7 is not clear after about 7 sec.  Timeout!\n");
        return -1;
    }

    for (block=0; block < 0x0020; block++)
    {
        if (BIT_5 & *(volbasePtr + block * flashinfo.flash_block_size + 1))
            *failureList += power;
        power = power << 1;
    }
   
    gsr = *(volbasePtr+2);
    *basePtr = CLR_STATUS_REGS_CMD;
    *basePtr = READ_ARRAY_CMD;
   
    return gsr;
}

//------------------------------------------------------------------
// AMD flash specific
//------------------------------------------------------------------
unsigned short AMDEraseAllUnlockFlashBlocks(unsigned short * basePtr, long * failureList)
{
    unsigned long timeout;    
    unsigned short data1, data2 ;
    volatile unsigned short *volbasePtr = basePtr;

    *failureList = -1;

    /* AMD erase chip command sequence ... */
    AMDFLASH_WRITE_RAW(0xaaaa, 0x555 );
    AMDFLASH_WRITE_RAW(0x5555, 0x2aa );
    AMDFLASH_WRITE_RAW(AMD_ERASE_CMD, 0x555 );
    AMDFLASH_WRITE_RAW(0xaaaa, 0x555 );
    AMDFLASH_WRITE_RAW(0x5555, 0x2aa );
    AMDFLASH_WRITE_RAW(AMD_CHIP_ERASE_CMD, 0x555 );

    /* loop and wait for erase to complete. Set a timeout of 40 seconds  */
    /* so it doesn't get stuck here ... device spec says up to 30s ???  */
    for (timeout=0; timeout<400; timeout++)
    {
        data1 = *volbasePtr; 
        data2 = *volbasePtr;

        //check if DQ6 & DQ2 toggling
        if( (data1 & B6B2TOGGLE) == (data2 & B6B2TOGGLE) )              //toggling?
        {
            data2 = *volbasePtr; 
            if( data2 == 0xFFFF )
            {
                BDGPrint(("AMD chip erase success (Ntime=%d)!\n", timeout));
                *failureList = 0;
                return 0;
            }
        }

        /* check for timeout condition */
        else if ((data2 & DQ5) == DQ5 )      //check if DQ5 == 1
        {
            data1 = *volbasePtr;      
            if ((data1 & DQ7) == DQ7 )
            {
                BDGPrint(("AMD chip erase success (Stime=%d)!\n", timeout));                     
                *failureList = 0;
                return 0;
            }
            else
            {
                BDGPrint("AMD chip erase bit5 timeout!\n");                     
                AMDFLASH_WRITE_RAW(AMD_RESET_CMD, 0x000);       //reset command            
                return -1;
            }
        }

        /* device hasn't done yet, wait a while */
        cfe_sleep(1); /* Delayms(100); */
    } /* end for */

    AMDFLASH_WRITE_RAW(AMD_RESET_CMD, 0x000);       //reset command
    BDGPrint(("AMD chip erase fail timeout, data1=%04x data2=%04x!\n", data1, data2)); 
    return -1;

}

/************************************************************************

   FlashWordWrite

        Description:   Writes words to the Flash
        
        Paramters:

         unsigned short * dest - word pointer to Flash write destination
         unsigned short * src - word pointer to source data
         unsigned short   count - number of words to write
         unsigned short * esr - pointer for returned esr status
                  (Upper byte=GSR, Lower byte=BSR)
        Return:

         int - NULL = success, otherwise error.

************************************************************************/
int FlashWordWrite(unsigned short * dest, unsigned short * src, unsigned long count, unsigned short * esr)
{
    BDGPrint(("Block FlashWordWrite %d swords from %08x to %08x\n", count, src, dest));

    return TFlashWordWrite(dest, src, count, esr);
}

//------------------------------------------------------------------
// Intel flash specific
//------------------------------------------------------------------
int IntelFlashWordWrite(unsigned short * dest, unsigned short * src, unsigned long count, unsigned short * esr)
{
    int i;
    volatile unsigned short *blockBase = Base(dest);
    int j;

    //------------------------------------------------------------------
    // support both 28016SV/SA and 28160S3/S5 flash
    //------------------------------------------------------------------
    if (s3s5Device) 
    {
        //------------------------------------------------------------------
        // Write actual data to flash, Read SR, polling until SR.7=1
        // (note: BSR is 1 word above base of target block in status reg space)
        //------------------------------------------------------------------
        for (i=0; i < count; i++)
        {
            *dest = ALT_WORD_WRITE_CMD;
            *dest++ = *src++;

            /* this shouldn't take too long */
            for (j = 0; j < 2000; j++)
            {
                if (BIT_7 & *(blockBase+1))
                    break;
            }

            if (j == 2000)
            {
                /* one last chance; give it another tick */
                cfe_sleep(1);
                if (!(BIT_7 & *(blockBase+1)))
                {
                    printf("IntelFlashWordWrite(): SR.7 is not set after more than 13 usec.  Timeout!\n");
                    return -1;
                }
            }

            if( (i & 0x3fff) == 0 )
                printf( "-" );
        }

        //------------------------------------------------------------------
        // Check for any errors
        //------------------------------------------------------------------
        *esr = *(blockBase+1);
        if (*esr & OPERATION_STATUS_ERR_2)
        {
            *blockBase = CLR_STATUS_REGS_CMD;
            *blockBase = READ_ARRAY_CMD;
            return -1;
        }      

        *blockBase = CLR_STATUS_REGS_CMD;
        *blockBase = READ_ARRAY_CMD;
      
        return ((int)NULL);
    }
    else 
    {
        //------------------------------------------------------------------
        // Write actual data to flash, Read ESR, polling until BSR.3=0
        // (queue available) then issue write word.
        // (note: BSR is 1 word above base of target block in status reg space)
        //------------------------------------------------------------------
        for (i=0; i < count; i++)
        {
            *dest = ESR_READ_CMD;

            for (j = 0; j < 500; j++)
            {
                udelay(10);
                if (BIT_3 & *(blockBase+1))
                    break;
            }

            if (j == 500)
            {
                printf("IntelFlashWordWrite(): BSR.3 is not set after about 5msec.  Timeout!\n");
                return -1;
            }

            *dest = ALT_WORD_WRITE_CMD;
            *dest++ = *src++;

            if( (i & 0x3fff) == 0 )
                printf( "-" );
        }
        
        //------------------------------------------------------------------
        // Get status of operation and return in esr
        // Poll BSR until BSR.7 of target address=1 (block ready)
        // Put GSR in upper byte and BSR in lower byte of esr.
        //------------------------------------------------------------------

        *dest = ESR_READ_CMD;
        /* according to the spec, this should typically take 210 usec and max
            of 630 usec.  So, let's poll this every 210 usec and 8 times */
        for (j = 0; j < 170; j++)
        {
            udelay(10);
            if (BIT_7 & *(blockBase+1))
                break;
        }

        if (j == 170)
        {
            printf("IntelFlashWordWrite(): SR.7 is not set after about 1.7 msec.  Timeout!\n");
            return -1;
        }

        *esr = (*(blockBase+2) << 8) + (*(blockBase+1) & LOW_BYTE);
        *dest = CLR_STATUS_REGS_CMD;
        *dest = READ_ARRAY_CMD;
     
        //------------------------------------------------------------------
        // Check for any errors
        //------------------------------------------------------------------
        if (*esr & OPERATION_STATUS_ERR)
            return -1;
        else
            return ((int)NULL); 
    }
}

//------------------------------------------------------------------
// AMD flash specific
//------------------------------------------------------------------
#define USE_BLOCK_WRITE
int AMDFlashWordWrite(unsigned short * dest, unsigned short * src, unsigned long count, unsigned short * esr)
{
    unsigned short timeout, value;    
    int ret_val = (int)NULL;        //assume no error

    *esr = (int)NULL;       //esr not supported in AMD flash, set it to 0 for now

#ifdef USE_BLOCK_WRITE
    /* AMD unlock bypass mode command sequence ...(faster for block write) */
    AMDFLASH_WRITE_RAW(0xaaaa, 0x555 );
    AMDFLASH_WRITE_RAW(0x5555, 0x2aa );
    AMDFLASH_WRITE_RAW(AMD_ULBYPASS_MOD_CMD, 0x555 );
#endif

    while( count-- )
    {
#ifdef USE_BLOCK_WRITE
        /* AMD unlock bypass program command... */
        AMDFLASH_WRITE_RAW(AMD_ULBYPASS_PGM_CMD, 0x000 );
#else
        /* AMD single write command sequence ... */
        AMDFLASH_WRITE_RAW(0xaaaa, 0x555 );
        AMDFLASH_WRITE_RAW(0x5555, 0x2aa );
        AMDFLASH_WRITE_RAW(AMD_PROGRAM_CMD, 0x555 );
#endif

        /* write the sword */
        value = *src;
        *dest = value;

        /* loop round waiting for program to complete or time out ... */
        for (timeout=200; timeout; timeout--)
        {
            if ( *dest == value )     //wait for complete
                break;
        }

        if( !timeout )          //timeout?
        {
            ret_val = -1;
            *esr = -1;
            BDGPrint(("AMD program sword @(0x%08x) timeout!\n", dest)); 
        }

        //next location
        src++;
        dest++;

    }       //end while

#ifdef USE_BLOCK_WRITE
    /* AMD unlock bypass reset command... */
    AMDFLASH_WRITE_RAW(AMD_ULBYPASS_RST_CMD, 0x000 );
    AMDFLASH_WRITE_RAW(0x0000, 0x000 );
#endif

    AMDFLASH_WRITE_RAW(AMD_RESET_CMD, 0x000);       //reset command
    return ret_val;
}

/************************************************************************

   SuspendEraseToReadArray

        Description:   Suspend an Erase Cycle.
        
        Paramters:

         unsigned short * dataPtr - pointer value to be read
         unsigned short * result  - holds value read until procedure complete

        Return:

         unsigned short  ESR - GSR in upper byte, BSR in lower byte

************************************************************************/
unsigned short SuspendEraseToReadArray(unsigned short * dataPtr, unsigned short * result)
{
    BDGPrint(("SuspendEraseToReadArray @ address %08x\n", dataPtr));

    return TSuspendEraseToReadArray(dataPtr, result);
}

//------------------------------------------------------------------
// Intel flash specific
//------------------------------------------------------------------
unsigned short IntelSuspendEraseToReadArray(unsigned short * dataPtr, unsigned short * result)
{
    unsigned short esr;
    volatile unsigned short * blockBase=Base(dataPtr);
    int j;

    //------------------------------------------------------------------
    // support both 28016SV/SA and 28160S3/S5 flash
    //------------------------------------------------------------------
    if (s3s5Device)
    {
        //------------------------------------------------------------------
        // issue suspend
        //------------------------------------------------------------------
        *dataPtr = ERASE_SUSPEND_CMD;
        *dataPtr = ESR_READ_CMD;
        // wait until operation complete

        /* according to the spec, this should typically take 36 us and max about
            40 us; so we are polling once every 36 us for 3 times */
        for (j = 0; j < 10; j++)
        {
            udelay(10);
            if (BIT_7 & *(blockBase+1))
                break;
        }

        if (j == 10)
        {
            printf("IntelSuspendEraseToReadArray(): SR.7 is not set after 108 usec.  Timeout!\n");
            return -1;
        }

        // attempt to suspend erase 
        if (BIT_6 & *(blockBase+1))
        {
            *dataPtr = READ_ARRAY_CMD;
            *result = *dataPtr;
            *dataPtr = CONFIRM_RESUME_CMD;
            *dataPtr = CLR_STATUS_REGS_CMD;
        }
        else
        {   // erase already complete
            *dataPtr = CLR_STATUS_REGS_CMD;
            *dataPtr = READ_ARRAY_CMD;
            return 0;
        }
        esr = *(blockBase+2);
    }
    else
    {
        //------------------------------------------------------------------
        // Get status then wait until Block Ready, issue suspend
        //------------------------------------------------------------------
        *dataPtr = ESR_READ_CMD;
        for (j = 0; j < 10; j++)
        {
            udelay(10);
            if (BIT_7 & *(blockBase+1))
                break;
        }

        if (j == 10)
        {
            printf("IntelSuspendEraseToReadArray(): SR.7 is not set after 108 usec.  Timeout!\n");
            return -1;
        }

        *dataPtr = ERASE_SUSPEND_CMD;
        *dataPtr = ESR_READ_CMD;
        
        //------------------------------------------------------------------
        // Wait until WSM ready, check result, read data
        //------------------------------------------------------------------

        /* according to the spec, this should typically take 36 us and max about
            40 us; so we are polling once every 36 us for 3 times */
        for (j = 0; j < 10; j++)
        {
            udelay(10);
            if (BIT_7 & *(blockBase+2))
                break;
        }

        if (j == 10)
        {
            printf("IntelSuspendEraseToReadArray(): SR.7 is not set after 108 usec.  Timeout!\n");
            return -1;
        }

        if (BIT_6 & *(blockBase+2))
        {
            *dataPtr = READ_ARRAY_CMD;
            *result = *dataPtr;
            *dataPtr = CONFIRM_RESUME_CMD;
        }
        else
        {
            *dataPtr = READ_ARRAY_CMD;
            *result = *dataPtr;
        }
        
        *dataPtr = ESR_READ_CMD;
        esr = (*(blockBase+2) << 8) + (*(blockBase+1) & LOW_BYTE);
        *dataPtr = CLR_STATUS_REGS_CMD;
    }
    return esr;
}

//------------------------------------------------------------------
// AMD flash specific
//------------------------------------------------------------------
unsigned short AMDSuspendEraseToReadArray(unsigned short * dataPtr, unsigned short * result)
{
    //For AMD, Suspend erase is only valid during sector erase which typically took 0.7 sec
    //This function is supposed to be called IN the AMDEraseFlashAddressBlock() routine, otherwise
    //it would be just the same as reading routine

    /* AMD Enter erase suspend ... */
    AMDFLASH_WRITE_RAW(AMD_ERASE_SUSPEND_CMD, 0x000 );              //valid only during a sector erase operation

    // read the data
    *result = *dataPtr;

    /* AMD resume erase ... */
    AMDFLASH_WRITE_RAW(AMD_ERASE_RESUME_CMD, 0x000 );               //valid only during a sector erase operation

    return 0;
}

/************************************************************************

   ReadFlashCsrStatus

        Description:   Returns the contents of the Flash CSR status register
        
        Paramters:

         none

        Return:

         unsigned char  - CSR register (SR for 28160S3/S5)

************************************************************************/
unsigned char
ReadFlashCsrStatus(void)
{
    unsigned short csr;

    BDGPrint(("In ReadFlashCsrStatus\n"));

    //------------------------------------------------------------------
    // not supported in AMD flash
    //------------------------------------------------------------------
    if( flashId == FLASH_AMD29LV160BB_ID )
        return 0;

    //------------------------------------------------------------------
    // support both 28016SV/SA and 28160S3/S5 flash
    //------------------------------------------------------------------
    if (s3s5Device)
    {
        *((unsigned short *)flashinfo.flash_base) = READ_SR_CMD;
        csr = *((unsigned short *)flashinfo.flash_base);
        *((unsigned short *)flashinfo.flash_base) = READ_ARRAY_CMD;
    }
    else
    {
        *((unsigned short *)flashinfo.flash_base) = READ_CSR_CMD;
        csr = *((unsigned short *)flashinfo.flash_base);
        *((unsigned short *)flashinfo.flash_base) = READ_ARRAY_CMD;
    }

    return (unsigned char)csr;
}

/************************************************************************

   ReadDeviceInformation

        Description:   Reads the Flash Device Information
        
        Paramters:

         none

        Return:

         unsigned short  - device revision status

************************************************************************/
unsigned short 
ReadDeviceInformation(void)
{
    unsigned short drc;
    volatile unsigned short * blockBase = (unsigned short *) (flashinfo.flash_base);

    BDGPrint(("In ReadDeviceInformation\n"));

    //------------------------------------------------------------------
    // not supported in AMD flash
    //------------------------------------------------------------------
    if( flashId == FLASH_AMD29LV160BB_ID )
        return 0;

    //------------------------------------------------------------------
    // not supported in 28160S3/S5 flash
    //------------------------------------------------------------------
    if (s3s5Device)
        return 0;

    *blockBase = ESR_READ_CMD;

    while((BIT_3 & *(blockBase+2)) && (!(BIT_7 & *(blockBase+2))))
        ;

    *blockBase = UPLOAD_DEV_INFO_CMD;
    *blockBase = CONFIRM_RESUME_CMD;
    *blockBase = ESR_READ_CMD;

    while(!(BIT_7 & *(blockBase+2)))
        ;

    if (BIT_5 & *(blockBase+2))
        return ((*(blockBase+2) & HIGH_BYTE) << 8);

    *blockBase = PAGE_BUF_SWAP_CMD;
    *blockBase = READ_PAGE_BUF_CMD;

    drc = (*(blockBase+2) & 0xff00 << 8) + (*(blockBase+3) & LOW_BYTE);

    *blockBase = CLR_STATUS_REGS_CMD;
    *blockBase = READ_ARRAY_CMD;

    return drc;
}

/************************************************************************

   ReadFlashIdCodes

        Description:   Reads the Flash manufacturer and device IDs
        
        Paramters:

         none

        Return:

         unsigned short  - device Manufacturer (HI-byte) | Device ID code (LO-byte)

************************************************************************/
unsigned short ReadFlashIdCodes(void)
{
    return TReadFlashIdCodes();
}

//------------------------------------------------------------------
// Intel flash specific
//------------------------------------------------------------------
unsigned short ReadIntelFlashIdCodes(void)
{
    volatile unsigned short *blockBase = flashBasePtr;
    unsigned short manCode;
    unsigned short devCode;

    *blockBase = READ_ID_CODES_CMD;
    manCode = *blockBase; 
    devCode = *(blockBase+1);

    *blockBase = CLR_STATUS_REGS_CMD;
    *blockBase = READ_ARRAY_CMD;

    return ((manCode << 8) | devCode);
}

//------------------------------------------------------------------
// AMD flash specific
//------------------------------------------------------------------
unsigned short ReadAMDFlashIdCodes(void)
{
    unsigned short manCode;
    unsigned short devCode;

    /* AMD write command sequence ... */
    AMDFLASH_WRITE_RAW(0xaaaa, 0x555 );
    AMDFLASH_WRITE_RAW(0x5555, 0x2aa );
    AMDFLASH_WRITE_RAW(AMD_AUTOSELECT_CMD, 0x555 );

    manCode = AMDFLASH_READ_RAW(AMD_MAN_ID_ADDR) & 0x00ff;       //only interested in the lower byte
    devCode = AMDFLASH_READ_RAW(AMD_DEV_ID_ADDR) & 0x00ff;               //only interested in the lower byte

    AMDFLASH_WRITE_RAW(AMD_RESET_CMD, 0x000);       //reset to return to read mode

    return ((manCode << 8) | devCode);
}


/************************************************************************

   AutoDectectFlash

        Description:   
        
        Paramters:

         debugMsg   - whether to show debug message or not
         auto_info - a pointer to flash information

        Return:

                  

************************************************************************/
void
AutoDetectFlash(unsigned char debugMsg)
{
    static int firsttime = FALSE;

    BDGPrint(("\nIn AutoDetectFlash\n" ));

    if( !firsttime )
    {
        BDGPrint(("\n[[***OneTime assigning flash routines pointer..." ));
        flashId = ReadAMDFlashIdCodes();
        if( flashId == FLASH_AMD29LV160BB_ID )
        {
            // fill in AMD flash information
            flashinfo.flash_base          = FLASH_BASE_ADDR_REG;       // flash base 
            flashinfo.flash_block_total   = TOTAL64K_FLASH_BLOCKS;     // 64 blocks
            flashinfo.flash_block_size    = FLASH64K_BLOCK_SIZE;       // 64kb
            flashinfo.flash_block_mask    = FLASH64K_BLOCK_SIZE_MASK;  // mask used 

            BDGPrint(("AMD flash detected***]]\n\n" ));
            firsttime = TRUE;
            TSuspendEraseToReadArray = AMDSuspendEraseToReadArray ;
            TFlashWordWrite = AMDFlashWordWrite ;
            TReadFlashIdCodes = ReadAMDFlashIdCodes ;
            TEraseFlashAddressBlock = AMDEraseFlashAddressBlock;
            TEraseAllUnlockFlashBlocks = AMDEraseAllUnlockFlashBlocks;
            TRead_Array_Cmd = AMD_RESET_CMD ;

        }
        else if( (ReadIntelFlashIdCodes() & 0xff) == STRATA_FLASH32_DEV_CODE)
        {
            // fill in Intel flash information
            flashinfo.flash_base          = STRATA32_FLASH_BASE;                         
            flashinfo.flash_block_total   = STRATA_FLASH32_TOTAL_FLASH_BLOCKS;  
            flashinfo.flash_block_size    = STRATA_FLASH_BLOCK_SIZE;                     
            flashinfo.flash_block_mask    = STRATA_FLASH_BLOCK_SIZE_MASK;                

            BDGPrint(("Intel Strata flash detected***]]\n\n" ));
            firsttime = TRUE;
            TSuspendEraseToReadArray = IntelSuspendEraseToReadArray ;
            TFlashWordWrite = IntelFlashWordWrite ;
            TReadFlashIdCodes = ReadIntelFlashIdCodes ;
            TEraseFlashAddressBlock = IntelEraseFlashAddressBlock;
            TEraseAllUnlockFlashBlocks = IntelEraseAllUnlockFlashBlocks;
            TRead_Array_Cmd = READ_ARRAY_CMD;
        }
        else if( (ReadIntelFlashIdCodes() & 0xff) == STRATA_FLASH64_DEV_CODE)
        {
            // fill in Intel flash information
            flashinfo.flash_base          = STRATA64_FLASH_BASE;                         
            flashinfo.flash_block_total   = STRATA_FLASH32_TOTAL_FLASH_BLOCKS;  
            flashinfo.flash_block_size    = STRATA_FLASH_BLOCK_SIZE;                     
            flashinfo.flash_block_mask    = STRATA_FLASH_BLOCK_SIZE_MASK;                

            BDGPrint(("Intel Strata flash detected***]]\n\n" ));
            firsttime = TRUE;
            TSuspendEraseToReadArray = IntelSuspendEraseToReadArray ;
            TFlashWordWrite = IntelFlashWordWrite ;
            TReadFlashIdCodes = ReadIntelFlashIdCodes ;
            TEraseFlashAddressBlock = IntelEraseFlashAddressBlock;
            TEraseAllUnlockFlashBlocks = IntelEraseAllUnlockFlashBlocks;
            TRead_Array_Cmd = READ_ARRAY_CMD;
        }
        else    //assume Intel flash
        {
            // fill in Intel flash information
            flashinfo.flash_base          = FLASH_BASE_ADDR_REG;       // flash base 
            flashinfo.flash_block_total   = TOTAL64K_FLASH_BLOCKS;     // 64 blocks
            flashinfo.flash_block_size    = FLASH64K_BLOCK_SIZE;       // 64kb
            flashinfo.flash_block_mask    = FLASH64K_BLOCK_SIZE_MASK;  // mask used 

            BDGPrint(("Intel flash detected***]]\n\n" ));
            firsttime = TRUE;
            TSuspendEraseToReadArray = IntelSuspendEraseToReadArray ;
            TFlashWordWrite = IntelFlashWordWrite ;
            TReadFlashIdCodes = ReadIntelFlashIdCodes ;
            TEraseFlashAddressBlock = IntelEraseFlashAddressBlock;
            TEraseAllUnlockFlashBlocks = IntelEraseAllUnlockFlashBlocks;
            TRead_Array_Cmd = READ_ARRAY_CMD;
        }
    }       //end if firsttime

    flashId = ReadFlashIdCodes() & 0x00ff; // mask off man code
    flashinfo.flashId = flashId;

    if (flashId == STRATA_FLASH32_DEV_CODE)
    {
        s3s5Device = TRUE;
        nTOTAL_FLASH_BLOCKS = STRATA_FLASH32_TOTAL_FLASH_BLOCKS;
        if (debugMsg)
            BDGPrint("28F320J3A Strata Flash Detected\n");
    }
    else if (flashId == STRATA_FLASH64_DEV_CODE)
    {
        s3s5Device = TRUE;
        nTOTAL_FLASH_BLOCKS = STRATA_FLASH64_TOTAL_FLASH_BLOCKS;
        if (debugMsg)
            BDGPrint("28F640J3A Strata Flash Detected\n");
    }
    else if (flashId == FLASH_160_DEV_CODE)
    {
        s3s5Device = TRUE;
        nTOTAL_FLASH_BLOCKS = 32;
        if (debugMsg)
            BDGPrint("28F160 flash detected\n");
    }
    else if (flashId == FLASH_320_DEV_CODE)
    {
        s3s5Device = TRUE;
        nTOTAL_FLASH_BLOCKS = 64;
        if (debugMsg)
            BDGPrint("28F320 flash detected\n");
    }
    else if (flashId == FLASH_016_DEV_CODE)
    {
        s3s5Device = FALSE;
        nTOTAL_FLASH_BLOCKS = 32;
        if (debugMsg)
            BDGPrint("28F016 flash detected\n");
    }
    else if( flashId == FLASH_28F320C3_DEV_CODE )
    {
        s3s5Device = TRUE;
        nTOTAL_FLASH_BLOCKS = 64;
        if (debugMsg)
            BDGPrint("Intel 28F320C3 flash detected\n");
    }
    else if( flashId == FLASH_AMD29LV160BB_DEV_CODE )
    {
        if (debugMsg)
            BDGPrint("AM29LV160 flash detected\n");
    }
}

/************************************************************************

   Set_Read_Array

        Description: Put flash back to reading mode  
        
        Paramters:

         none

        Return:
                none
                  

************************************************************************/
void Set_Read_Array(void)
{
    *((unsigned short *)FLASH_BASE_ADDR_REG) = TRead_Array_Cmd;
}

/************************************************************************

   DetectFlashInfo

        Description: Copy flash information into passed parameter  
        
        Paramters:

         autoflashinfo - where the flash data is to be stored

        Return:
                none
                  

************************************************************************/
void DetectFlashInfo(FLASH_INFO * autoflashinfo)
{
    AutoDetectFlash(0);

    // copy the flash information
    memcpy(autoflashinfo, &flashinfo, sizeof(flashinfo));
}


/***********************************************************************
 * Function Name: flash_init
 * Description  : Initialize flash driver.
 * Returns      : 0.
 ***********************************************************************/
unsigned char flash_init(void)
{
    AutoDetectFlash(0);
    return( 0 );
} /* flash_sector_erase_int */


/***********************************************************************
 * Function Name: flash_reset
 * Description  : Called after a flash operation.
 * Returns      : 1
 ***********************************************************************/
unsigned char flash_reset(void)
{
    cfe_sleep(1);
    Set_Read_Array();
    return( 1 );
} /* flash_reset */


/***********************************************************************
 * Function Name: flash_sector_erase_int
 * Description  : Erase a flash block.
 * Returns      : 1
 ***********************************************************************/
unsigned char flash_sector_erase_int(unsigned char sector)
{
    EraseFlashBlock(sector);
    return( 1 );
} /* flash_sector_erase_int */


/***********************************************************************
 * Function Name: flash_get_numsectors
 * Description  : Returns the total number of flash blocks.
 * Returns      : 1
 ***********************************************************************/
int flash_get_numsectors(void)
{
    return( (int) flashinfo.flash_block_total );
} /* flash_get_numsectors */


/***********************************************************************
 * Function Name: flash_get_sector_size
 * Description  : Returns the size of a flash block.
 * Returns      : 1
 ***********************************************************************/
int flash_get_sector_size( unsigned char sector )
{
    return( (int) flashinfo.flash_block_size );
} /* flash_get_sector_size */


/***********************************************************************
 * Function Name: flash_get_memptr
 * Description  : Returns the starting address for a flash block.
 * Returns      : 1
 ***********************************************************************/
unsigned char *flash_get_memptr( unsigned char sector )
{
    return( (unsigned char *) (FLASH_BASE_ADDR_REG + 
        flashinfo.flash_block_size * (unsigned long) sector) );
} /* flash_get_memptr */


/***********************************************************************
 * Function Name: flash_get_blk
 * Description  : Returns the starting address for a flash block.
 * Returns      : 1
 ***********************************************************************/
int flash_get_blk( int addr )
{
    int blk_start, i;
    int last_blk = flash_get_numsectors();
    int relative_addr = addr - (int) FLASH_BASE_ADDR_REG;

    for(blk_start=0, i=0; i < relative_addr && blk_start < last_blk; blk_start++)
        i += flash_get_sector_size(blk_start);

    if( i > relative_addr )
    {
         blk_start--;        // last blk, dec by 1
    }
    else
        if( blk_start == last_blk )
        {
            printf("Address is too big.\n");
            blk_start = -1;
        }

    return( blk_start );
} /* flash_get_blk */


/************************************************************************/
/* The purpose of flash_get_total_size() is to return the total size of */
/* the flash                                                            */
/************************************************************************/

int flash_get_total_size()
{
    return flashinfo.flash_block_size * flashinfo.flash_block_total;
}

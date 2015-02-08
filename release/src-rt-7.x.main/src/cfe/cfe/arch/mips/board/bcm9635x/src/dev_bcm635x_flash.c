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
/***************************************************************************
 * File Name  : dev_bcm6352_flash.c
 *
 * Description: This file contains flash entry point functions.
 *
 * Created on :  05/20/2002  lat.  Lifted from BCM96352 board.c.
 *
 ***************************************************************************/


/* Includes. */
#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_string.h"
#include "lib_printf.h"
#include "cfe_timer.h"

#include "board.h"
#include "6352_map.h"
#include "dev_bcm63xx_flash.h"
#include "flash.h"

static FLASH_ADDR_INFO fInfo;
extern NVRAM_DATA nvramData;            
extern int readNvramData(void);


/***********************************************************************
 * Function Name: flashAddrReadBlock
 * Description  : Reads an array of bytes from flash memory.
 * Returns      : None.
 ***********************************************************************/
static void flashAddrReadBlock(char *string, char *addr, int strLen, int offset)
{
    int i;

    if( (offset & 0x01) != ((unsigned long) string & 0x01) )
    {
        /* Source and destination addresses are on different odd/even
         * boundaries.  Do a byte copy.
         */
        unsigned char *dest = (unsigned char *) string;
        unsigned char *src = (unsigned char *) (addr + offset);

        for( i = 0; i < strLen; i++ )
            *dest++ = *src++;
    }
    else
    {
        /* Source and destination addresses are on the same odd/even
         * boundaries.  Do a word (16 bit) copy.
         */
        unsigned short *dest, *src;
      
        if( (offset & 0x01) == 0x01 )
        {
            *string++ = (char) *(addr + offset);
            strLen--;
            offset++;
        }
      
        dest = (unsigned short*) string;
        src = (unsigned short*) (addr + offset);

        for( i = 0; i < strLen / 2; i++ )
            *dest++ = *src++;

        if( (strLen & 0x01) == 0x01 )
            string[strLen - 1] = *(addr + offset + strLen - 1);
    }

} /* flashAddrReadBlock */


//**************************************************************************************
// Flash read/write and image downloading..
//**************************************************************************************

void kerSysFlashInit( void )
{
    int i = 0;
    int totalBlks = 0;
    int totalSize = 0;
    int startAddr = 0;
    int usedBlkSize = 0;

    flash_init();

    totalBlks = flash_get_numsectors();
    totalSize = flash_get_total_size();

#if defined(DEBUG_FLASH)
    printf("totalBlks=%d\n", totalBlks);
    printf("totalSize=%dK\n", totalSize/1024);
#endif

    
    /* nvram is always at the end of flash */
    fInfo.flash_nvram_length = NVRAM_LENGTH;
    startAddr = totalSize - NVRAM_LENGTH;
    fInfo.flash_nvram_start_blk = flash_get_blk(startAddr+FLASH_BASE_ADDR_REG);
    fInfo.flash_nvram_number_blk = totalBlks - fInfo.flash_nvram_start_blk;

printf("nvram startaddr = 0x%x, start_blk = %d\n", startAddr, fInfo.flash_nvram_start_blk);

    // find out the offset in the start_blk
    for (i = fInfo.flash_nvram_start_blk; 
        i < (fInfo.flash_nvram_start_blk + fInfo.flash_nvram_number_blk); i++)
        usedBlkSize += flash_get_sector_size((byte) i);

#if defined(DEBUG_FLASH)
    printf("i=%d, usedBlkSize=%d\n", i, usedBlkSize);
#endif

    fInfo.flash_nvram_blk_offset = usedBlkSize - NVRAM_LENGTH;
    
    if (readNvramData() != 0)
    {
        printf("Board is not initialized: Using the default PSI size: %d\n", NVRAM_PSI_DEFAULT_635X);
        fInfo.flash_persistent_length = NVRAM_PSI_DEFAULT_635X;
    }
    else
        fInfo.flash_persistent_length = nvramData.ulPsiSize;

    /* PSI is right below NVRAM */
    fInfo.flash_persistent_length *= ONEK;
    startAddr -= fInfo.flash_persistent_length;          // PSI start address
    fInfo.flash_persistent_start_blk = flash_get_blk(startAddr+FLASH_BASE_ADDR_REG);
    if (fInfo.flash_nvram_start_blk == fInfo.flash_persistent_start_blk)  // share blk
    {
        fInfo.flash_persistent_number_blk = 1;
        fInfo.flash_persistent_blk_offset = fInfo.flash_nvram_blk_offset - fInfo.flash_persistent_length;
    }
    else // on different blk
    {
        fInfo.flash_persistent_number_blk = totalBlks - fInfo.flash_persistent_start_blk;
        // find out the offset in the start_blk
        usedBlkSize = 0;
        for (i = fInfo.flash_persistent_start_blk; 
            i < (fInfo.flash_persistent_start_blk + fInfo.flash_persistent_number_blk); i++)
            usedBlkSize += flash_get_sector_size((byte) i);

printf("usesize=%d\n", usedBlkSize);

        fInfo.flash_persistent_blk_offset =  usedBlkSize - fInfo.flash_persistent_length - fInfo.flash_nvram_length;
    }

#if defined(DEBUG_FLASH)
    printf("fInfo.flash_nvram_start_blk = %d\n", fInfo.flash_nvram_start_blk);
    printf("fInfo.flash_nvram_blk_offset = 0x%x\n", fInfo.flash_nvram_blk_offset);
    printf("fInfo.flash_nvram_number_blk = %d\n", fInfo.flash_nvram_number_blk);

    printf("psi startAddr = %x\n", startAddr+FLASH_BASE_ADDR_REG);
    printf("fInfo.flash_persistent_start_blk = %d\n", fInfo.flash_persistent_start_blk);
    printf("fInfo.flash_persistent_blk_offset = 0x%x\n", fInfo.flash_persistent_blk_offset);
    printf("fInfo.flash_persistent_number_blk = %d\n", fInfo.flash_persistent_number_blk);
#endif

}


/***********************************************************************
 * Function Name: kerSysFlashAddrInfoGet
 * Description  : Fills in a structure with information about the NVRAM
 *                and persistent storage sections of flash memory.
 * Returns      : None.
 ***********************************************************************/
void kerSysFlashAddrInfoGet(PFLASH_ADDR_INFO pflash_addr_info)
{
    pflash_addr_info->flash_nvram_blk_offset = fInfo.flash_nvram_blk_offset;
    pflash_addr_info->flash_nvram_length = fInfo.flash_nvram_length;
    pflash_addr_info->flash_nvram_number_blk = fInfo.flash_nvram_number_blk;
    pflash_addr_info->flash_nvram_start_blk = fInfo.flash_nvram_start_blk;
    pflash_addr_info->flash_persistent_blk_offset = fInfo.flash_persistent_blk_offset;
    pflash_addr_info->flash_persistent_length = fInfo.flash_persistent_length;
    pflash_addr_info->flash_persistent_number_blk = fInfo.flash_persistent_number_blk;
    pflash_addr_info->flash_persistent_start_blk = fInfo.flash_persistent_start_blk;
}


// get shared blks into *** pTempBuf *** which has to be released bye the caller!
// return: if pTempBuf != NULL, poits to the data with the dataSize of the buffer
// !NULL -- ok
// NULL  -- fail
static char *getSharedBlks(int start_blk, int end_blk)
{
    int i = 0;
    int usedBlkSize = 0;
    int sect_size = 0;
    char *pTempBuf = NULL;
    char *pBuf = NULL;

    for (i = start_blk; i < end_blk; i++)
        usedBlkSize += flash_get_sector_size((byte) i);

//printf("usedBlkSize = %d\n", usedBlkSize);

    if ((pTempBuf = (char *) KMALLOC(usedBlkSize, sizeof(long))) == NULL)
    {
        printf("failed to allocate memory with size: %d\n", usedBlkSize);
        return pTempBuf;
    }
    
    pBuf = pTempBuf;
    for (i = start_blk; i < end_blk; i++)
    {
        sect_size = flash_get_sector_size((byte) i);
#if defined(DEBUG_FLASH)
        printf("getShareBlks: i=%d, sect_size=%d, end_blk=%d\n", i, sect_size, end_blk);
#endif
        flashAddrReadBlock(pBuf, (char *) flash_get_memptr(i), sect_size, 0);
        pBuf += sect_size;
    }
    
    return pTempBuf;
}



// Set the pTempBuf to flash from start_blk to end_blk
// return:
// 0 -- ok
// -1 -- fail
static int setSharedBlks(int start_blk, int end_blk, char *pTempBuf)
{
    int i = 0;
    int sect_size = 0;
    int sts = 0;
    unsigned short esr;
    unsigned char *src = (unsigned char *) pTempBuf;
    unsigned char *dest =  flash_get_memptr(start_blk);

    for (i = start_blk; i < end_blk; i++)
    {
        sect_size = flash_get_sector_size((byte) i);
        if (AddrWriteProtect((unsigned short *) dest, UNLOCK) != 0)
        {
            printf("Failed on unlock\n");
            sts = -1;
            break;
        }
        if (EraseFlashBlock(i) != 0)
        {
            printf("Failed to erase flash sector: %d\n", i);
            sts  = -1;
            break;
        }
        if( FlashWordWrite((unsigned short *) dest, (unsigned short *) src,
            sect_size / sizeof(short), &esr) != 0 )
        {
            sts = -1;
            break;
        }
        AddrWriteProtect((unsigned short *) dest, LOCK);

        src += sect_size;
        dest += sect_size;
        printf(".");
    }

    cfe_sleep(1); /* mdelay(100); */
    Set_Read_Array();

    return sts;
}



/*******************************************************************************
 * NVRAM functions
 *******************************************************************************/

// get nvram data
// return:
//  0 - ok
//  -1 - fail
int kerSysNvRamGet(char *string, int strLen, int offset)
{
    char *pBuf = NULL;

    if (strLen > NVRAM_LENGTH)
        return -1;

    if ((pBuf = getSharedBlks(fInfo.flash_nvram_start_blk,
        (fInfo.flash_nvram_start_blk + fInfo.flash_nvram_number_blk))) == NULL)
        return -1;

    // get string off the memory buffer
    memcpy(string, (pBuf + fInfo.flash_nvram_blk_offset + offset), strLen);

    KFREE(pBuf);

    return 0;
}


// set nvram 
// return:
//  0 - ok
//  -1 - fail
int kerSysNvRamSet(char *string, int strLen, int offset)
{
    int sts = 0;
    char *pBuf = NULL;

    if (strLen > NVRAM_LENGTH)
        return -1;

    if ((pBuf = getSharedBlks(fInfo.flash_nvram_start_blk,
        (fInfo.flash_nvram_start_blk + fInfo.flash_nvram_number_blk))) == NULL)
        return -1;

    // set string to the memory buffer
    memcpy((pBuf + fInfo.flash_nvram_blk_offset + offset), string, strLen);

    if (setSharedBlks(fInfo.flash_nvram_start_blk, 
        (fInfo.flash_nvram_number_blk + fInfo.flash_nvram_start_blk), pBuf) != 0)
        sts = -1;
    
    KFREE(pBuf);

    return sts;
}


/***********************************************************************
 * Function Name: kerSysEraseNvRam
 * Description  : Erase the NVRAM storage section of flash memory.
 * Returns      : 1 -- ok, 0 -- fail
 ***********************************************************************/
int kerSysEraseNvRam(void)
{
    int sts = 1;
    char *tempStorage = KMALLOC(NVRAM_LENGTH, sizeof(long));
    
    // just write the whole buf with '0xff' to the flash
    if (!tempStorage)
        sts = 0;
    else
    {
        memset(tempStorage, 0xff, NVRAM_LENGTH);
        if (kerSysNvRamSet(tempStorage, NVRAM_LENGTH, 0) != 0)
            sts = 0;
        KFREE(tempStorage);
    }

    return sts;
}


/*******************************************************************************
 * PSI functions
 *******************************************************************************/
// get psi data
// return:
//  0 - ok
//  -1 - fail
int kerSysPsiGet(char *string, int strLen, int offset)
{
    char *pBuf = NULL;

    if (strLen > fInfo.flash_persistent_length)
        return -1;

    if ((pBuf = getSharedBlks(fInfo.flash_persistent_start_blk,
        (fInfo.flash_persistent_start_blk + fInfo.flash_persistent_number_blk))) == NULL)
        return -1;

    // get string off the memory buffer
    memcpy(string, (pBuf + fInfo.flash_persistent_blk_offset + offset), strLen);

    KFREE(pBuf);

    return 0;
}


// set psi 
// return:
//  0 - ok
//  -1 - fail
int kerSysPsiSet(char *string, int strLen, int offset)
{
    int sts = 0;
    char *pBuf = NULL;

    if (strLen > fInfo.flash_persistent_length)
        return -1;

    if ((pBuf = getSharedBlks(fInfo.flash_persistent_start_blk,
        (fInfo.flash_persistent_start_blk + fInfo.flash_persistent_number_blk))) == NULL)
        return -1;

    // set string to the memory buffer
    memcpy((pBuf + fInfo.flash_persistent_blk_offset + offset), string, strLen);

    if (setSharedBlks(fInfo.flash_persistent_start_blk, 
        (fInfo.flash_persistent_number_blk + fInfo.flash_persistent_start_blk), pBuf) != 0)
        sts = -1;

    KFREE(pBuf);

    return sts;
}


/***********************************************************************
 * Function Name: kerSysErasePsi
 * Description  : Erase the Psi storage section of flash memory.
 * Returns      : 1 -- ok, 0 -- fail
 ***********************************************************************/
int kerSysErasePsi(void)
{
    int sts = 1;
    char *tempStorage = KMALLOC(fInfo.flash_persistent_length, sizeof(long));
    
    // just write the whole buf with '0xff' to the flash
    if (!tempStorage)
        sts = 0;
    else
    {
        memset(tempStorage, 0xff, fInfo.flash_persistent_length);
        if (kerSysPsiSet(tempStorage, fInfo.flash_persistent_length, 0) != 0)
            sts = 0;
        KFREE(tempStorage);
    }

    return sts;
}


/***********************************************************************
 * Function Name: kerSysMemoryTypeSet
 * Description  : set the memory type 4 bytes
 * Returns      : 1 -- ok, 0 -- fail
 ***********************************************************************/
int kerSysMemoryTypeSet(int flash_address, char *string, int size)
{
    int sect_size;
    int blk_start;
    char *pTempBuf;
    unsigned short esr;

    blk_start = flash_get_blk(flash_address);
    sect_size = flash_get_sector_size(blk_start);

    if ((pTempBuf = (char *) KMALLOC(sect_size, sizeof(long))) == NULL)
    {
        printf("Failed to allocate memory with size: %d\n", sect_size);
        return 0;
    }
            
    flashAddrReadBlock(pTempBuf, (char *) flash_address, sect_size, 0);
    memcpy(pTempBuf+SDRAM_TYPE_ADDRESS_OFFSET, string, size);

    if (EraseFlashBlock(blk_start) != 0)
    {
        printf("Failed to erase flash sector: %d\n", blk_start);
        return 0;
    }
    
    if(FlashWordWrite((unsigned short *)flash_address,(unsigned short *)pTempBuf,sect_size/sizeof(short), &esr) != 0) // failed
    {
        printf("Failed to flash the memory type\n");
        return 0;
    }

    cfe_sleep(1);
    Set_Read_Array();

    return 1;
}


/***********************************************************************
 * Function Name: kerSysMemoryTypeGet
 * Description  : get the memory type 4 bytes
 * Returns      : memory type
 ***********************************************************************/
int kerSysMemoryTypeGet(void)
{
    int sdramType = BOARD_SDRAM_TYPE;

    return sdramType;
}


// flash bcm image 
// return: 
// 0 - ok
// !0 - the sector number fail to be flashed (should not be 0)
int kerSysBcmImageSet( int flash_start_addr, char *string, int size, int fWholeImage)
{
    int sts = -1;
    int sect_size;
    int blk_start;
    char *pTempBuf = NULL;
    unsigned char *flash_address = NULL;
    unsigned short esr;

//printf("flash_start_addr = 0x%x\n", flash_start_addr);

    blk_start = flash_get_blk(flash_start_addr);
    if( blk_start < 0 )
        return sts;

   /* write image to flash memory */
    do 
    {
        sect_size = flash_get_sector_size(blk_start);
        flash_address = flash_get_memptr(blk_start);

#if defined(DEBUG_FLASH)
        printf("Image flasing on block: %d\n", blk_start);
#endif

        // share the blk with nvram only when fWholeImage == 0
        if (!fWholeImage && blk_start == fInfo.flash_nvram_start_blk)  
        {
            if (size > (sect_size - NVRAM_LENGTH))
            {
                printf("Image is too big\n");
                return sts;          // image is too big. Can not overwrite to nvram
            }
            if ((pTempBuf = (char *) KMALLOC(sect_size, sizeof(long))) == NULL)
            {
                printf("Failed to allocate memory with size: %d\n", sect_size);
                return sts;
            }
            flashAddrReadBlock(pTempBuf, (char *) flash_address, sect_size, 0);
            memcpy(pTempBuf, string, size);

            if (AddrWriteProtect((unsigned short *) flash_address, UNLOCK) != 0)
            {
                printf("Failed on unlock\n");
                return sts;
            }
            if (EraseFlashBlock(blk_start) != 0)
            {
                printf("Failed to erase flash sector: %d\n", blk_start);
                return sts;
            }
            if( FlashWordWrite((unsigned short *)flash_address, (unsigned short *) pTempBuf,
                sect_size / sizeof(short), &esr) == 0 ) // write ok
                size = 0;
            AddrWriteProtect((unsigned short *)flash_address, LOCK);
            // break out and say all is ok
            break;     
        }
        
        if (sect_size > size) 
        {
            if (size & 1) 
                size++;
            sect_size = size;
        }

        EraseFlashBlock(blk_start);
        if( FlashWordWrite((unsigned short *)flash_address, (unsigned short *) string,
            sect_size / sizeof(short), &esr) != 0 )
            break;
        printf(".");
        blk_start++;
        string += sect_size;
        size -= sect_size; 
    } while (size > 0);

    cfe_sleep(1);
    Set_Read_Array();

    if( size == 0 ) 
        sts = 0;  // ok
    else  
        sts = blk_start;    // failed to flash this sector

    return sts;
}

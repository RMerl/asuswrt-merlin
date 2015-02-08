/************************************************************************/
/*                                                                      */
/*  AMD CFI Enabled Flash Memory Drivers                                */
/*  File name: CFIFLASH.C                                               */
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
/************************************************************************/                        #ifdef _CFE_                                                
#include "lib_printf.h"
#else       // linux
#include <linux/param.h>
#include <linux/sched.h>
#include <linux/timer.h>
#endif

#include "cfiflash.h"

static int flash_status(volatile WORD *fp);
static byte flash_get_manuf_code(byte sector);
static WORD flash_get_device_id(byte sector);
static byte flash_get_cfi(struct cfi_query *query);
static unsigned char *get_flash_memptr(byte sector);
static int flash_write(byte sector, int offset, byte *buf, int nbytes, int ub);
static void flash_command(int command, byte sector, int offset, unsigned int data);
byte flash_chip_erase(byte sector);
byte flash_write_word_ub(byte sector, int offset, WORD data);


/*********************************************************************/
/* 'meminfo' should be a pointer, but most C compilers will not      */
/* allocate static storage for a pointer without calling             */
/* non-portable functions such as 'new'.  We also want to avoid      */
/* the overhead of passing this pointer for every driver call.       */
/* Systems with limited heap space will need to do this.             */
/*********************************************************************/
struct flashinfo meminfo; /* Flash information structure */
static int topBoot = TRUE;
static int totalSize = 0;

/*********************************************************************/
/* Init_flash is used to build a sector table from the information   */
/* provided through the CFI query.  This information is translated   */
/* from erase_block information to base:offset information for each  */
/* individual sector. This information is then stored in the meminfo */
/* structure, and used throughout the driver to access sector        */
/* information.                                                      */
/*                                                                   */
/* This is more efficient than deriving the sector base:offset       */
/* information every time the memory map switches (since on the      */
/* development platform can only map 64k at a time).  If the entire  */
/* flash memory array can be mapped in, then the addition static     */
/* allocation for the meminfo structure can be eliminated, but the   */
/* drivers will have to be re-written.                               */
/*                                                                   */
/* The meminfo struct occupies 653 bytes of heap space, depending    */
/* on the value of the define MAXSECTORS.  Adjust to suit            */
/* application                                                       */ 
/*********************************************************************/

byte flash_init(void)
{
    int i=0, j=0, count=0;
    int basecount=0L;
	struct cfi_query query;
    byte manuf_id;
    WORD device_id;

    /* First, assume
    * a single 8k sector for sector 0.  This is to allow
    * the system to perform memory mapping to the device,
    * even though the actual physical layout is unknown.
    * Once mapped in, the CFI query will produce all
    * relevant information.
    */
    meminfo.addr = 0L;
    meminfo.areg = 0;
    meminfo.nsect = 1;
    meminfo.bank1start = 0;
    meminfo.bank2start = 0;
    
    meminfo.sec[0].size = 8192;
    meminfo.sec[0].base = 0x00000;
    meminfo.sec[0].bank = 1;
        
    manuf_id = flash_get_manuf_code(0);
    device_id = flash_get_device_id(0);

  	flash_get_cfi(&query);

    count=0;basecount=0L;

    for (i=0; i<query.num_erase_blocks; i++) {
        count += query.erase_block[i].num_sectors;
    }
    
    meminfo.nsect = count;
    count=0;

// need to determine if it top or bottom boot here

#if defined(DEBUG_FLASH)
    printf("device_id = %0x\n", device_id);
#endif

    switch (device_id)
    {
        case ID_AM29DL800B:
        case ID_AM29LV800B:
        case ID_AM29LV400B:   
        case ID_AM29LV160B:
        case ID_AM29LV320B:
            topBoot = FALSE;
            break;
        case ID_AM29DL800T:
        case ID_AM29LV800T:
        case ID_AM29LV160T:
        case ID_AM29LV320T:
            topBoot = TRUE;
            break;
        default:
            printf("Flash memory not supported!  Device id = %x\n", device_id);
            return -1;           
    }


#if defined(DEBUG_FLASH)
    printf("meminfo: topBoot=%d\n", topBoot);    
#endif

    if (!topBoot)
    {
       for (i=0; i<query.num_erase_blocks; i++) {
            for(j=0; j<query.erase_block[i].num_sectors; j++) {
                meminfo.sec[count].size = (int) query.erase_block[i].sector_size;
                meminfo.sec[count].base = (int) basecount;
                basecount += (int) query.erase_block[i].sector_size;

#if defined(DEBUG_FLASH)
printf(" meminfo.sec[%d]: .size[0x%08X], base[0x%08X]\n", count, meminfo.sec[count].size, meminfo.sec[count].base+PHYS_FLASH_BASE);
#endif

                count++;
            }
        }

#if defined(DEBUG_FLASH)
        printf("BottomBoot\n");
#endif

    }
    else /* TOP BOOT */
    {
        for (i = (query.num_erase_blocks - 1); i >= 0; i--) {
            for(j=0; j<query.erase_block[i].num_sectors; j++) {
                meminfo.sec[count].size = (int) query.erase_block[i].sector_size;
                meminfo.sec[count].base = (int) basecount;
                basecount += (int) query.erase_block[i].sector_size;

#if defined(DEBUG_FLASH)
printf("meminfo.sec[%d]: .size[0x%08X], base[0x%08X]\n", count, meminfo.sec[count].size, meminfo.sec[count].base+PHYS_FLASH_BASE);
#endif
                count++;
            }
        }

#if defined(DEBUG_FLASH)
        printf("TopBoot\n");
#endif

    }

    totalSize = meminfo.sec[count-1].base + meminfo.sec[count-1].size;

    // HAVE TO be a printf to reset flash ??? cfe_sleep -- delay will not help!!!
	flash_reset();


	return (0);
}


/*********************************************************************/
/* BEGIN API WRAPPER FUNCTIONS                                       */
/*********************************************************************/
/* Flash_sector_erase() will erase a single sector dictated by the   */
/* sector parameter.                                                 */
/* Note: this function will merely BEGIN the erase program.  Code    */
/* execution will immediately return to the calling function         */
/*********************************************************************/

byte flash_sector_erase(byte sector)
{
    flash_command(FLASH_SERASE,sector,0,0);
    return(1);
}

/*********************************************************************/
/* Flash_sector_erase_int() is identical to flash_sector_erase(),    */
/* except it will wait until the erase is completed before returning */
/* control to the calling function.  This can be used in cases which */
/* require the program to hold until a sector is erased, without     */
/* adding the wait check external to this function.                  */
/*********************************************************************/

byte flash_sector_erase_int(byte sector)
{
    flash_command(FLASH_SERASE,sector,0,0);
    while (flash_status((WORD*)get_flash_memptr(sector))
        == STATUS_BUSY) { }
    return(1);
}

/*********************************************************************/
/* flash_reset() will reset the flash device to reading array data.  */
/* It is good practice to call this function after autoselect        */
/* sequences had been performed.                                     */
/*********************************************************************/

byte flash_reset(void)
{
    flash_command(FLASH_RESET,1,0,0);
    return(1);
}

/*********************************************************************/
/* flash_sector_protect_verify() performs an autoselect command      */
/* sequence which checks the status of the sector protect CAM        */
/* to check if the particular sector is protected.  Function will    */
/* return a '0' is the sector is unprotected, and a '1' if it is     */
/* protected.                                                        */
/*********************************************************************/

byte flash_sector_protect_verify(byte sector)
{
    volatile WORD *fwp; /* flash window */
    byte answer;
    
    fwp = (WORD *)get_flash_memptr(sector);
    
    flash_command(FLASH_AUTOSEL,sector,0,0);
    
    fwp += ((meminfo.sec[sector].base)/2);
    fwp += 2;
    
    answer = (byte) (*fwp & 0x0001); /* Only need DQ0 to check */
    
    flash_command(FLASH_RESET,sector,0,0);
    
    return( (byte) answer );
}

/*********************************************************************/
/* flash_chip_erase() will perform a complete erasure of the flash   */
/* device.  							     */
/*********************************************************************/

byte flash_chip_erase(byte sector)
{
    flash_command(FLASH_CERASE,sector,0,0);
    return(1);
}

/*********************************************************************/
/* flash_write_word() will program a single word of data at the      */
/* specified offset from the beginning of the sector parameter.      */
/* Note: this offset must be word aligned, or else errors are        */
/* possible (this can happen when dealing with odd offsets due to    */
/* only partial programming.                                         */
/* Note: It is good practice to check the desired offset by first    */
/* reading the data, and checking to see if it contains 0xFFFF       */
/*********************************************************************/

byte flash_write_word(byte sector, int offset, WORD data)
{
    flash_command(FLASH_PROG, sector, offset, data);
    return (1);
}

/*********************************************************************/
/* flash_write_buf() functions like flash_write_word(), except       */
/* that it accepts a pointer to a buffer to be programmed.  This     */
/* function will align the data to a word offset, then bulk program  */
/* the flash device with the provided data.                          */
/* The maximum buffer size is currently only limited to the data     */
/* size of the numbytes parameter (which in the test system is       */
/* 16 bits = 65535 words                                             */
/* Since the current maximum flash sector size is 64kbits, this      */
/* should not present a problem.                                     */
/*********************************************************************/

int flash_write_buf(byte sector, int offset, 
                        byte *buffer, int numbytes)
{
    
    return flash_write(sector, offset, buffer, numbytes, FALSE);
}

/*********************************************************************/
/* flash_read_buf() reads buffer of data from the specified          */
/* offset from the sector parameter.                                 */
/*********************************************************************/

int flash_read_buf(byte sector, int offset, 
                        byte *buffer, int numbytes)
{
    byte *fwp;

    flash_command(FLASH_SELECT, sector, 0, 0);
    fwp = (byte *)get_flash_memptr(sector);

	while (numbytes) {
		*buffer++ = *(fwp + offset);
		numbytes--;
		fwp++;
    }

    return (1);
}

/*********************************************************************/
/* flash_erase_suspend() will suspend an erase process in the        */
/* specified sector.  Array data can then be read from other sectors */
/* (or any other sectors in other banks), and the erase can be       */
/* resumed using the flash_erase_resume function.                    */
/* Note: multiple sectors can be queued for erasure, so int as the  */
/* 80 uS erase suspend window has not terminated (see AMD data sheet */
/* concerning erase_suspend restrictions).                           */
/*********************************************************************/

byte flash_erase_suspend(byte sector)
{
    flash_command(FLASH_ESUSPEND, sector, 0, 0);
    return (1);
}

/*********************************************************************/
/* flash_erase_resume() will resume all pending erases in the bank   */
/* in which the sector parameter is located.                         */
/*********************************************************************/

byte flash_erase_resume(byte sector)
{
    flash_command(FLASH_ERESUME, sector, 0, 0);
    return (1);
}

/*********************************************************************/
/* UNLOCK BYPASS FUNCTIONS                                           */
/*********************************************************************/
/* Unlock bypass mode is useful whenever the calling application     */
/* wished to program large amounts of data in minimal time.  Unlock  */
/* bypass mode remove half of the bus overhead required to program   */
/* a single word, from 4 cycles down to 2 cycles.  Programming of    */
/* individual bytes does not gain measurable benefit from unlock     */
/* bypass mode, but programming large buffers can see a significant  */
/* decrease in required programming time.                            */
/*********************************************************************/

/*********************************************************************/
/* flash_ub() places the flash into unlock bypass mode.  This        */
/* is REQUIRED to be called before any of the other unlock bypass    */
/* commands will become valid (most will be ignored without first    */
/* calling this function.                                            */
/*********************************************************************/

byte flash_ub(byte sector)
{
    flash_command(FLASH_UB, sector, 0, 0);
    return(1);
}

/*********************************************************************/
/* flash_write_word_ub() programs a single word using unlock bypass  */
/* mode.  Note that the calling application will see little benefit  */
/* from programming single words using this mode (outlined above)    */
/*********************************************************************/

byte flash_write_word_ub(byte sector, int offset, WORD data)
{
    flash_command(FLASH_UBPROG, sector, offset, data);
    return (1);
}

/*********************************************************************/
/* flash_write_buf_ub() behaves in the exact same manner as          */
/* flash_write_buf() (outlined above), expect that it utilizes       */
/* the unlock bypass mode of the flash device.  This can remove      */
/* significant overhead from the bulk programming operation, and     */
/* when programming bulk data a sizeable performance increase can be */
/* observed.                                                         */
/*********************************************************************/

int flash_write_buf_ub(byte sector, int offset,
                           byte *buffer, int numbytes)
{
    return flash_write(sector, offset, buffer, numbytes, TRUE);
}

/*********************************************************************/
/* flash_reset_ub() is required to remove the flash from unlock      */
/* bypass mode.  This is important, as other flash commands will be  */
/* ignored while the flash is in unlock bypass mode.                 */
/*********************************************************************/

byte flash_reset_ub(void)
{
    flash_command(FLASH_UBRESET,1,0,0);
    return(1);
}

/*********************************************************************/
/* Usefull funtion to return the number of sectors in the device.    */
/* Can be used for functions which need to loop among all the        */
/* sectors, or wish to know the number of the last sector.           */
/*********************************************************************/

int flash_get_numsectors(void)
{
    return meminfo.nsect;
}

/*********************************************************************/
/* flash_get_sector_size() is provided for cases in which the size   */
/* of a sector is required by a host application.  The sector size   */
/* (in bytes) is returned in the data location pointed to by the     */
/* 'size' parameter.                                                 */
/*********************************************************************/

int flash_get_sector_size(byte sector)
{
    return meminfo.sec[sector].size;
}

/*********************************************************************/
/* flash_get_cfi() is the main CFI workhorse function.  Due to it's  */
/* complexity and size it need only be called once upon              */
/* initializing the flash system.  Once it is called, all operations */
/* are performed by looking at the meminfo structure.                */
/* All possible care was made to make this algorithm as efficient as */
/* possible.  90% of all operations are memory reads, and all        */
/* calculations are done using bit-shifts when possible              */
/*********************************************************************/

byte flash_get_cfi(struct cfi_query *query)
{
    volatile WORD *fwp; /* flash window */
    int volts=0, milli=0, temp=0, i=0;
    int offset=0;
    
    flash_command(FLASH_RESET,0,0,0); /* Use sector 0 for all commands */
    flash_command(FLASH_CFIQUERY,0,0,0);
    
    fwp = (WORD *)get_flash_memptr(0);
    
    /* Initial house-cleaning */
    
    for(i=0; i < 8; i++) {
        query->erase_block[i].sector_size = 0;
        query->erase_block[i].num_sectors = 0;
    }
    
    query->query_string[0] = fwp[0x10];
    query->query_string[1] = fwp[0x11];
    query->query_string[2] = fwp[0x12];
    query->query_string[3] = '\0';
    
    /* If not 'QRY', then we dont have a CFI enabled device in the
    socket */
    
    if( query->query_string[0] != 'Q' &&
        query->query_string[1] != 'R' &&
        query->query_string[2] != 'Y') {
        return(-1);
    }
    
    query->oem_command_set       = fwp[0x13];
    query->primary_table_address = fwp[0x15]; /* Important one! */
    query->alt_command_set       = fwp[0x17];
    query->alt_table_address     = fwp[0x19];
    
    /* We will do some bit translation to give the following values
    numerical meaning in terms of C 'float' numbers */
    
    volts = ((fwp[0x1B] & 0xF0) >> 4);
    milli = (fwp[0x1B] & 0x0F);
    query->vcc_min = volts * 10 + milli;
    
    volts = ((fwp[0x1C] & 0xF0) >> 4);
    milli = (fwp[0x1C] & 0x0F);
    query->vcc_max = volts * 10 + milli;
    
    volts = ((fwp[0x1D] & 0xF0) >> 4);
    milli = (fwp[0x1D] & 0x0F);
    query->vpp_min = volts * 10 + milli;
    
    volts = ((fwp[0x1E] & 0xF0) >> 4);
    milli = (fwp[0x1E] & 0x0F);
    query->vpp_max = volts * 10 + milli;
    
    /* Let's not drag in the libm library to calculate powers
    for something as simple as 2^(power)
    Use a bit shift instead - it's faster */
    
    temp = fwp[0x1F];
    query->timeout_single_write = (1 << temp);
    
    temp = fwp[0x20];
    if (temp != 0x00)
        query->timeout_buffer_write = (1 << temp);
    else
        query->timeout_buffer_write = 0x00;
    
    temp = 0;
    temp = fwp[0x21];
    query->timeout_block_erase = (1 << temp);
    
    temp = fwp[0x22];
    if (temp != 0x00)
        query->timeout_chip_erase = (1 << temp);
    else
        query->timeout_chip_erase = 0x00;
    
    temp = fwp[0x23];
    query->max_timeout_single_write = (1 << temp) *
        query->timeout_single_write;
    
    temp = fwp[0x24];
    if (temp != 0x00)
        query->max_timeout_buffer_write = (1 << temp) *
        query->timeout_buffer_write;
    else
        query->max_timeout_buffer_write = 0x00;
    
    temp = fwp[0x25];
    query->max_timeout_block_erase = (1 << temp) *
        query->timeout_block_erase;
    
    temp = fwp[0x26];
    if (temp != 0x00)
        query->max_timeout_chip_erase = (1 << temp) *
        query->timeout_chip_erase;
    else
        query->max_timeout_chip_erase = 0x00;
    
    temp = fwp[0x27];
    query->device_size = (int) (((int)1) << temp);
    
    query->interface_description = fwp[0x28];
    
    temp = fwp[0x2A];
    if (temp != 0x00)
        query->max_multi_byte_write = (1 << temp);
    else
        query->max_multi_byte_write = 0;
    
    query->num_erase_blocks = fwp[0x2C];
    
    for(i=0; i < query->num_erase_blocks; i++) {
        query->erase_block[i].num_sectors = fwp[(0x2D+(4*i))];
        query->erase_block[i].num_sectors++;
        
        query->erase_block[i].sector_size = (int) 256 * 
		( (int)256 * fwp[(0x30+(4*i))] + fwp[(0x2F+(4*i))] );
    }
    
    /* Store primary table offset in variable for clarity */
    offset = query->primary_table_address;
    
    query->primary_extended_query[0] = fwp[(offset)];
    query->primary_extended_query[1] = fwp[(offset + 1)];
    query->primary_extended_query[2] = fwp[(offset + 2)];
    query->primary_extended_query[3] = '\0';
    
    if( query->primary_extended_query[0] != 'P' &&
        query->primary_extended_query[1] != 'R' &&
        query->primary_extended_query[2] != 'I') {
        return(2);
    }
    
    query->major_version = fwp[(offset + 3)];
    query->minor_version = fwp[(offset + 4)];
    
    query->sensitive_unlock      = (byte) (fwp[(offset+5)] & 0x0F);
    query->erase_suspend         = (byte) (fwp[(offset+6)] & 0x0F);
    query->sector_protect        = (byte) (fwp[(offset+7)] & 0x0F);
    query->sector_temp_unprotect = (byte) (fwp[(offset+8)] & 0x0F);
    query->protect_scheme        = (byte) (fwp[(offset+9)] & 0x0F);
    query->is_simultaneous       = (byte) (fwp[(offset+10)] & 0x0F);
    query->is_burst              = (byte) (fwp[(offset+11)] & 0x0F);
    query->is_page               = (byte) (fwp[(offset+12)] & 0x0F);
    
    return(1);
}

/*********************************************************************/
/* The purpose of get_flash_memptr() is to return a memory pointer   */
/* which points to the beginning of memory space allocated for the   */
/* flash.  All function pointers are then referenced from this       */
/* pointer. 							     */
/*                                                                   */
/* Different systems will implement this in different ways:          */
/* possibilities include:                                            */
/*  - A direct memory pointer                                        */
/*  - A pointer to a memory map                                      */
/*  - A pointer to a hardware port from which the linear             */
/*    address is translated                                          */
/*  - Output of an MMU function / service                            */
/*                                                                   */
/* Also note that this function expects the pointer to a specific    */
/* sector of the device.  This can be provided by dereferencing      */
/* the pointer from a translated offset of the sector from a         */
/* global base pointer (e.g. flashptr = base_pointer + sector_offset)*/
/*                                                                   */
/* Important: Many AMD flash devices need both bank and or sector    */
/* address bits to be correctly set (bank address bits are A18-A16,  */
/* and sector address bits are A18-A12, or A12-A15).  Flash parts    */
/* which do not need these bits will ignore them, so it is safe to   */
/* assume that every part will require these bits to be set.         */
/*********************************************************************/

static unsigned char *get_flash_memptr(byte sector)
{
	unsigned char *memptr = (unsigned char*)(FLASH_BASE_ADDR_REG + meminfo.sec[sector].base);

	return (memptr);
}

unsigned char *flash_get_memptr(byte sector)
{
    return( get_flash_memptr(sector) );
}

/*********************************************************************/
/* Flash_command() is the main driver function.  It performs         */
/* every possible command available to AMD B revision                */
/* flash parts. Note that this command is not used directly, but     */
/* rather called through the API wrapper functions provided below.   */
/*********************************************************************/

static void flash_command(int command, byte sector, int offset,
                   unsigned int data)
{
    volatile WORD *flashptr;  /* flash window (64K bytes) */
    int retry;
    
    /**************************************************************/
    /* IMPORTANT: Note that flashptr is defined as a WORD pointer */
    /* If BYTE pointers are used, the command tables will have to */
    /* be remapped						       */
    /* Note 1: flashptr is declared far - if system does not      */
    /*         support far pointers, this will have to be changed */
    /* Note 2: flashptr is declared static to avoid calling       */
    /*         get_flash_memptr() on successive sector accesses   */
    /**************************************************************/
    
    /* Some commands may not work the first time, but will work   */
    /* on successive attempts.  Vary the number of retries to suit*/
    /* requirements.                                              */
    
    static int retrycount[] = {0,0,0,0,15,15,0,15,0,0,0};
    
    retry = retrycount[command];
    
    flashptr = (WORD *) get_flash_memptr(sector);
    
again:
    
    if (command == FLASH_SELECT) {
        return;
    } else if (command == FLASH_RESET || command > FLASH_LASTCMD) {
        flashptr[0] = 0xF0;   /* assume reset device to read mode */
    } else if (command == FLASH_ESUSPEND) {
        flashptr[0] = 0xB0;   /* suspend sector erase */
    } else if (command == FLASH_ERESUME) {
        flashptr[0] = 0x30;   /* resume suspended sector erase */
    } else if (command == FLASH_UBPROG) {
        flashptr[0] = 0xA0;
        flashptr[offset/2] = data;
    } else if (command == FLASH_UBRESET) {
        flashptr[0] = 0x90;
        flashptr[0] = 0x00;
    } else if (command == FLASH_CFIQUERY) {
        flashptr[0x55] = 0x98;
	}
    else {
        flashptr[0x555] = 0xAA;       /* unlock 1 */
        flashptr[0x2AA] = 0x55;       /* unlock 2 */
        switch (command) {
        case FLASH_AUTOSEL:
            flashptr[0x555] = 0x90;
            break;
        case FLASH_PROG:
            flashptr[0x555] = 0xA0;
            flashptr[offset/2] = data;
            break;
        case FLASH_CERASE:
            flashptr[0x555] = 0x80;
            flashptr[0x555] = 0xAA;
            flashptr[0x2AA] = 0x55;
            flashptr[0x555] = 0x10;
            break;
        case FLASH_SERASE:
            flashptr[0x555]  = 0x80;
            flashptr[0x555]  = 0xAA;
            flashptr[0x2AA]  = 0x55;
            flashptr[0] = 0x30;
            break;
        case FLASH_UB:
            flashptr[0x555] = 0x20;
            break;
        }
    }
    
    if (retry-- > 0 && flash_status(flashptr) == STATUS_READY) {
        goto again;
    }
}

/*********************************************************************/
/* Flash_write extends the functionality of flash_program() by       */
/* providing an faster way to program multiple data words, without   */
/* needing the function overhead of looping algorithms which         */
/* program word by word.  This function utilizes fast pointers       */
/* to quickly loop through bulk data.                                */
/*********************************************************************/
static int flash_write(byte sector, int offset, byte *buf, 
                int nbytes, int ub)
{
    volatile WORD *flashptr; /* flash window */
    volatile WORD *dst;
    WORD *src;
    int stat;
    int retry = 0, retried = 0;

    flashptr = (WORD *)get_flash_memptr(sector);

    dst = &flashptr[offset/2];   /* (byte offset) */
    src = (WORD *)buf;

    if ((nbytes | offset) & 1) {
        return -1;
    }

again:
    
    /* Check to see if we're in unlock bypass mode */
    if (ub == FALSE)
        flashptr[0] = 0xF0;  /* reset device to read mode */
    
    while ((stat = flash_status(flashptr)) == STATUS_BUSY) {}
    if (stat != STATUS_READY) {
        return (byte*)src - buf;
    }
    
    while (nbytes > 0) {
        if (ub == FALSE){
            flashptr[0x555] = 0xAA;      /* unlock 1 */
            flashptr[0x2AA] = 0x55;      /* unlock 2 */
        }
        flashptr[0x555] = 0xA0;
        *dst++ = *src++;

        while ((stat = flash_status(flashptr)) == STATUS_BUSY) {}
        if (stat != STATUS_READY) break;
        nbytes -= 2;
    }
    
    if (stat != STATUS_READY || nbytes != 0) {
        if (retry-- > 0) {
            ++retried;
            --dst, --src;   /* back up */
            goto again;     /* and retry the last word */
        }
        if (ub == FALSE)
            flash_command(FLASH_RESET,sector,0,0);
    }
    return (byte*)src - buf;
}


/*********************************************************************/
/* Flash_status utilizes the DQ6, DQ5, and DQ3 polling algorithms    */
/* described in the flash data book.  It can quickly ascertain the   */
/* operational status of the flash device, and return an             */
/* appropriate status code (defined in flash.h)                      */
/*********************************************************************/

static int flash_status(volatile WORD *fp)
{
    WORD d, t;
    int retry = 1;
    
again:
    
    d = *fp;        /* read data */
    t = d ^ *fp;    /* read it again and see what toggled */
    
    if (t == 0) {           /* no toggles, nothing's happening */
        return STATUS_READY;
    }
    else if (t == 0x04) { /* erase-suspend */
        if (retry--) goto again;    /* may have been write completion */
        	return STATUS_ERSUSP;
    }
    else if (t & 0x40) {
        if (d & 0x20) {     /* timeout */
	        if (retry--) goto again;    /* may have been write completion */
    	        return STATUS_TIMEOUT;
        }
        else {
            return STATUS_BUSY;
        }
    }
    
    if (retry--) goto again;    /* may have been write completion */
    
    return STATUS_ERROR;
}

/*********************************************************************/
/* flash_get_device_id() will perform an autoselect sequence on the  */
/* flash device, and return the device id of the component.          */
/* This function automatically resets to read mode.                  */
/*********************************************************************/

static WORD flash_get_device_id(byte sector)
{
    volatile WORD *fwp; /* flash window */
    WORD answer;
    
    fwp = (WORD *)get_flash_memptr(sector);
    
    flash_command(FLASH_AUTOSEL,sector,0,0);
    answer = *(++fwp);
    
    flash_command(FLASH_RESET,sector,0,0);   /* just to be safe */
    return( (WORD) answer );
}

/*********************************************************************/
/* flash_get_manuf_code() will perform an autoselect sequence on the */
/* flash device, and return the manufacturer code of the component.  */
/* This function automatically resets to read mode.                  */
/*********************************************************************/

static byte flash_get_manuf_code(byte sector)
{
    volatile WORD *fwp; /* flash window */
    WORD answer;
    
    fwp = (WORD *)get_flash_memptr(sector);
    
    flash_command(FLASH_AUTOSEL,sector,0,0);
    answer = *fwp;
    
    flash_command(FLASH_RESET,sector,0,0);   /* just to be safe */	
    return( (byte) (answer & 0x00FF) );
}


/*********************************************************************/
/* The purpose of flash_get_blk() is to return a the block number */
/* for a given memory address.                                       */
/*********************************************************************/

int flash_get_blk(int addr)
{
    int blk_start, i;
    int last_blk = flash_get_numsectors();
    int relative_addr = addr - (int) FLASH_BASE_ADDR_REG;

    for(blk_start=0, i=0; i < relative_addr && blk_start < last_blk; blk_start++)
        i += flash_get_sector_size(blk_start);

    if( i > relative_addr )
    {

#if defined(DEBUG_FLASH)
        printf("Address does not start on flash block boundary. Use the current blk_start\n");
#endif

        blk_start--;        // last blk, dec by 1
    }
    else
        if( blk_start == last_blk )
        {
            printf("Address is too big.\n");
            blk_start = -1;
        }

    return( blk_start );
}


/************************************************************************/
/* The purpose of flash_get_total_size() is to return the total size of */
/* the flash                                                            */
/************************************************************************/
int flash_get_total_size()
{
    return totalSize;
}

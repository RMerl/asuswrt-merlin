/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  
    *  bcm63xx board specific routines and commands.
    *  
    *  by:  seanl
    *
    *       April 1, 2002
    *
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */


#include "bcm63xx_util.h"

#define ENTRY_POINT                 0x80010470
#define CODE_ADDRESS                0x80010000
#define FLASH_STAGING_BUFFER	    (1024*1024)	                  /* 1MB line */
#define FLASH_STAGING_BUFFER_SIZE	(4 * 1024 *1024 + TAG_LEN)    // 4 MB for now

extern PFILE_TAG kerSysImageTagGet(void);
extern NVRAM_DATA nvramData;   

static int ui_cmd_set_board_param(ui_cmdline_t *cmd,int argc,char *argv[])
{
    return(setBoardParam());
}



static int ui_cmd_reset(ui_cmdline_t *cmd,int argc,char *argv[])
{
    kerSysMipsSoftReset();

    return 0;
}


// return 0 if 'y'
int yesno(void)
{
    char ans[5];

    printf(" (y/n):");
    console_readline ("", ans, sizeof (ans));
    if (ans[0] != 'y')
        return -1;

    return 0;
}


// erase Persistent sector
static int ui_cmd_erase_psi(ui_cmdline_t *cmd,int argc,char *argv[])
{
    printf("Erase persisten storage data?");
    if (yesno())
        return -1;

    kerSysErasePsi();

    return 0;
}



// erase some sectors
static int ui_cmd_erase(ui_cmdline_t *cmd,int argc,char *argv[])
{

    //FILE_TAG cfeTag;
    PFILE_TAG pTag;
    char *flag;
    int i, blk_start, blk_end;

    flag = cmd_getarg(cmd,0);

    if (!flag)
    {
        printf("Erase [n]vram or [a]ll flash except bootrom; usage: e [n/a]\n");
        return 0;
    }

    switch (*flag)
    {
    case 'b':
        printf("Erase boot loader?");
        if (yesno())
            return 0;
        printf("\nNow think carefully.  Do you really,\n"
            "really want to erase the boot loader?");
        if (yesno())
            return 0;
        flash_sector_erase_int(0);        
        break;
    case 'n':
        printf("Erase nvram?");
        if (yesno())
            return 0;
        kerSysEraseNvRam();
        break;
    case 'a':
        printf("Erase all flash (except bootrom)?");
        if (yesno())
            return 0;

        if ((pTag = kerSysImageTagGet()) != NULL)
            blk_start = flash_get_blk( atoi(pTag->rootfsAddress) );
        else  // just erase all after cfe
        {
            blk_start = FLASH45_BLKS_BOOT_ROM;  
            printf("No image tag found.  Erase the blocks start at [%d]\n", blk_start);
        }
        blk_end = flash_get_numsectors();
        if( blk_start > 0 )
        {
            for (i = blk_start; i < blk_end; i++)
            {
                printf(".");
                flash_sector_erase_int(i);
            }
        }
        printf( "\nResetting board...\n" );
        kerSysMipsSoftReset();
        break;
    default:
        printf("Erase [n]vram or [a]ll flash except bootrom; usage: e [n/a]\n");
        return 0;
    }

    flash_reset();

    return 0;
}



// flash the image 
static int ui_cmd_flash_image(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char hostImageName[BOOT_FILENAME_LEN + BOOT_IP_LEN];
    char *imageName;
    uint8_t *ptr;
    cfe_loadargs_t la;
    int res;

    imageName = cmd_getarg(cmd, 0);

    if (imageName)
    {
        if (strchr(imageName, ':'))
            strcpy(hostImageName, imageName);
        else
        {
            strcpy(hostImageName, bootInfo.hostIp);
            strcat(hostImageName, ":");
            strcat(hostImageName, imageName);
        }
    }
    else  // use default flash file name
    {
        strcpy(hostImageName, bootInfo.hostIp);
        strcat(hostImageName, ":");
        strcat(hostImageName, bootInfo.flashFileName);
    }

    printf("Loading %s ...\n", hostImageName);
    
    ptr = (uint8_t *) PHYS_TO_K0(FLASH_STAGING_BUFFER);
    
    // tftp only
    la.la_filesys = "tftp";
    la.la_filename = hostImageName;
	la.la_device = NULL;
	la.la_address = (intptr_t) ptr;
	la.la_options = 0;
	la.la_maxsize = FLASH_STAGING_BUFFER_SIZE;
	la.la_flags =  LOADFLG_SPECADDR;

	res = cfe_load_program("raw", &la);
	if (res < 0)
    {
	    ui_showerror(res, "Loading failed.");
		return res;
    }
    printf("Finished loading %d bytes\n", res);

    // check and flash image
    res = flashImage(ptr);

    if( res == 0 )
    {
        char *p;

        for( p = nvramData.szBootline; p[2] != '\0'; p++ )
            if( p[0] == 'r' && p[1] == '=' && p[2] == 'h' )
            {
                /* Change boot source to "boot from flash". */
                p[2] = 'f';
                writeNvramData();
                break;
            }
        printf( "Resetting board...\n" );
        kerSysMipsSoftReset();
    }

    return( res );
}


// write the whole image
static int ui_cmd_write_whole_image(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char hostImageName[BOOT_FILENAME_LEN + BOOT_IP_LEN];
    char *imageName;
    uint8_t *ptr;
    cfe_loadargs_t la;
    int res;

    imageName = cmd_getarg(cmd, 0);
    if (!imageName) 
        return ui_showusage(cmd);

    if (strchr(imageName, ':'))
        strcpy(hostImageName, imageName);
    else
    {
        strcpy(hostImageName, bootInfo.hostIp);
        strcat(hostImageName, ":");
        strcat(hostImageName, imageName);
    }

    printf("Loading %s ...\n", hostImageName);
    
    ptr = (uint8_t *) PHYS_TO_K0(FLASH_STAGING_BUFFER);
    
    // tftp only
    la.la_filesys = "tftp";
    la.la_filename = hostImageName;
	la.la_device = NULL;
	la.la_address = (intptr_t) ptr;
	la.la_options = 0;
	la.la_maxsize = FLASH_STAGING_BUFFER_SIZE;
	la.la_flags =  LOADFLG_SPECADDR;

	res = cfe_load_program("raw", &la);
	if (res < 0)
    {
	    ui_showerror(res, "Loading failed.");
		return res;
    }
    printf("Finished loading %d bytes\n", res);

    // check and flash image
    res = writeWholeImage(ptr, res);

    printf("Finished flashing image.  return = %d\n", res);

    if (res == 0)
    {
        printf( "Resetting board...\n" );
        kerSysMipsSoftReset();
    }

    return( res );
}

// Used to flash an RTEMS image. Only works on BCM96345 with top boot flash part.
static int ui_cmd_flash_router_image(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char hostImageName[BOOT_FILENAME_LEN + BOOT_IP_LEN];
    char *imageName;
    uint8_t *ptr;
    cfe_loadargs_t la;
    int res;

    imageName = cmd_getarg(cmd, 0);
    if (!imageName) 
        return ui_showusage(cmd);

    if (strchr(imageName, ':'))
        strcpy(hostImageName, imageName);
    else
    {
        strcpy(hostImageName, bootInfo.hostIp);
        strcat(hostImageName, ":");
        strcat(hostImageName, imageName);
    }

    printf("Loading %s ...\n", hostImageName);
    
    ptr = (uint8_t *) PHYS_TO_K0(FLASH_STAGING_BUFFER);
    
    // tftp only
    la.la_filesys = "tftp";
    la.la_filename = hostImageName;
	la.la_device = NULL;
	la.la_address = (intptr_t) ptr;
	la.la_options = 0;
	la.la_maxsize = FLASH_STAGING_BUFFER_SIZE;
	la.la_flags =  LOADFLG_SPECADDR;

	res = cfe_load_program("raw", &la);
	if (res < 0)
    {
	    ui_showerror(res, "Loading failed.");
		return res;
    }
    printf("Finished loading %d bytes\n", res);

    // check and flash image
    if ((res = kerSysBcmImageSet(FLASH45_IMAGE_START_ADDR, ptr, res, 1)) != 0)
        printf("Failed to flash image. Error: %d\n", res);
    else
        printf("Finished flashing image.\n");

    if (res == 0)
    {
        char *p;

        for( p = nvramData.szBootline; p[2] != '\0'; p++ )
            if( p[0] == 'r' && p[1] == '=' && p[2] == 'h' )
            {
                /* Change boot source to "boot from flash". */
                p[2] = 'f';
                writeNvramData();
                break;
            }
        printf( "Resetting board...\n" );
        kerSysMipsSoftReset();
    }

    return( res );
}

static int autoRun(char *imageName)
{
    char runCmd[BOOT_FILENAME_LEN + BOOT_IP_LEN + 20], ipImageName[BOOT_FILENAME_LEN + BOOT_IP_LEN];
    int ret;


    if (bootInfo.runFrom == 'f' && !imageName)
    {
        strcpy(runCmd, "boot -elf -z -rawfs flash1:");
        ret = ui_docommand(runCmd);
    }
    else // loading from host
    {
        if (imageName)
        {
            if (strchr(imageName, ':'))
                strcpy(ipImageName, imageName);
            else
            {
                strcpy(ipImageName, bootInfo.hostIp);
                strcat(ipImageName, ":");
                strcat(ipImageName, imageName);
            }
        }
        else  // use default host file name
        {
            strcpy(ipImageName, bootInfo.hostIp);
            strcat(ipImageName, ":");
            strcat(ipImageName, bootInfo.hostFileName);
        }

        // try uncompressed image first
        strcpy(runCmd, "boot -elf ");
        strcat(runCmd, ipImageName);

        ret = ui_docommand(runCmd);

        if( ret == CFE_ERR_NOTELF )
        {
            // next try as a compressed image
            printf("Retry loading it as a compressed image.\n");
            strcpy(runCmd, "boot -elf -z ");
            strcat(runCmd, ipImageName);
            ret = ui_docommand(runCmd);
        }
    }

    return ret;
}

                     
// run program from compressed image in flash or from tftped program from host
static int ui_cmd_run_program(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *imageName;

    imageName = cmd_getarg(cmd, 0);

    return autoRun(imageName);
}


static int ui_cmd_print_system_info(ui_cmdline_t *cmd,int argc,char *argv[])
{
    return printSysInfo();
}


static int ui_cmd_change_bootline(ui_cmdline_t *cmd,int argc,char *argv[])
{
    return changeBootLine();
}


static int ui_init_bcm6345_cmds(void)
{

    // Used to flash an RTEMS image.
    cmd_addcmd("flashimage",
	       ui_cmd_flash_router_image,
	       NULL,
	       "Flashes a compressed image after the bootloader.",
	       "eg. flashimage [hostip:]compressed_image_file_name",
	       "");

    cmd_addcmd("reset",
	       ui_cmd_reset,
	       NULL,
	       "Reset the board",
	       "",
	       "");

    cmd_addcmd("b",
	       ui_cmd_set_board_param,
	       NULL,
	       "Change board parameters",
	       "",
	       "");

    cmd_addcmd("i",
	       ui_cmd_erase_psi,
	       NULL,
	       "Erase persistent storage data",
	       "",
	       "");

    cmd_addcmd("f",
	       ui_cmd_flash_image,
	       NULL,
	       "Write image to the flash ",
           "eg. f [[hostip:]filename]<cr> -- if no filename, tftped from host with file name in 'Default host flash file name'",
           "");

    cmd_addcmd("c",
	       ui_cmd_change_bootline,
	       NULL,
	       "Change booline parameters",
	       "",
	       "");

    cmd_addcmd("p",
	       ui_cmd_print_system_info,
	       NULL,
	       "Print boot line and board parameter info",
	       "",
	       "");

    cmd_addcmd("r",
	       ui_cmd_run_program,
	       NULL,
	       "Run program from flash image or from host depend on [f/h] flag",
	       "eg. r [[hostip:]filenaem]<cr> if no filename, use the file name in 'Default host run file name'",
	       "");

    cmd_addcmd("e",
	       ui_cmd_erase,
	       NULL,
	       "Erase [n]vram or [a]ll flash except bootrom",
	       "e [n/a]",
	       "");

    cmd_addcmd("w",
	       ui_cmd_write_whole_image,
	       NULL,
	       "Write the whole image start from beginning of the flash",
	       "eg. w [hostip:]whole_image_file_name",
	       "");

    return 0;
}


static int runDelay(int delayCount)
{
    int goAuot = 0;
    
    if (delayCount == 0)
        return goAuot;

    printf("*** Press any key to stop auto run (%d seconds) ***\n", delayCount);
    printf("Auto run second count down: %d", delayCount);

    cfe_sleep(CFE_HZ/8);        // about 1/4 second

    while (1)
    {
	    printf("\b%d", delayCount);
	    cfe_sleep(CFE_HZ/2);        // about 1 second
        if (console_status())
            break;; 
        if (--delayCount == 0)
        {
            goAuot = 1;
            break;
        }
	}
    printf("\b%d\n", delayCount);

    return goAuot;
}


void bcm63xx_run(void)
{
    ui_init_bcm6345_cmds();
    printSysInfo();
    enet_init();
    if (runDelay(bootInfo.bootDelay))
        autoRun(NULL);        // never returns
}

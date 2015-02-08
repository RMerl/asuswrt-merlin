/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  SWARM-specific commands			File: ui_lausanne.c
    *  
    *  A temporary sandbox for misc test routines and commands.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
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


#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_timer.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_devfuncs.h"
#include "cfe_error.h"
#include "cfe_console.h"

#include "ui_command.h"
#include "cfe.h"

#include "bsp_config.h"

#include "sbmips.h"
#include "sb1250_regs.h"
#include "sb1250_scd.h"

#include "sb1250_genbus.h"

#include "sb1250_pass2.h"
#include "dev_flash.h"
#include "lausanne.h"

/*  *********************************************************************
    *  Configuration
    ********************************************************************* */

/*  *********************************************************************
    *  prototypes
    ********************************************************************* */

int ui_init_swarmcmds(void);

int ui_init_cpldcmds(void);
static int ui_cmd_writecpld(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_readcpld(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_burstwcpld(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_burstrcpld(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_config_flash(ui_cmdline_t *cmd,int argc,char *argv[]);


#if CFG_VGACONSOLE
static int ui_cmd_vgadump(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_vgainit(ui_cmdline_t *cmd,int argc,char *argv[]);
extern int vga_biosinit(void);
extern void vgaraw_dump(char *tail);
#endif



/*  *********************************************************************
    *  Data
    ********************************************************************* */


/*  *********************************************************************
    *  ui_init_swarmcmds()
    *  
    *  Add SWARM-specific commands to the command table
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */


int ui_init_swarmcmds(void)
{
#if CFG_VGACONSOLE
    cmd_addcmd("vga init",
	       ui_cmd_vgainit,
	       NULL,
	       "Initialize the VGA adapter.",
	       "vgainit",
	       "");

    cmd_addcmd("vga dumpbios",
	       ui_cmd_vgadump,
	       NULL,
	       "Dump the VGA BIOS to the console",
	       "vga dumpbios",
	       "");
#endif


    return 0;
}







#if CFG_VGACONSOLE
static int ui_cmd_vgainit(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int res;

    res = vga_biosinit();

    xprintf("vgaraw_init returns %d\n",res);
    
    return 0;
}

static int ui_cmd_vgadump(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *x;

    x = cmd_getarg(cmd,0);
    if (!x) x = "";

    vgaraw_dump(x);
    
    return 0;
}
#endif


int ui_init_cpldcmds(void)
{

    cmd_addcmd("write cpld",
	       ui_cmd_writecpld,
	       NULL,
	       "Write bytes to the cpld",
	       "write cpld",
	       "");

    cmd_addcmd("read cpld",
	       ui_cmd_readcpld,
	       NULL,
	       "Read bytes from the cpld",
	       "read cpld",
	       "");

    cmd_addcmd("burstw cpld",
	       ui_cmd_burstwcpld,
	       NULL,
	       "Write to the cpld in 8-bit BURST MODE",
	       "burstw cpld",
	       "");

    cmd_addcmd("burstr cpld",
	       ui_cmd_burstrcpld,
	       NULL,
	       "Read from the cpld in 8-bit BURST MODE",
	       "burstr cpld",
	       "");

    cmd_addcmd("config flash",
	       ui_cmd_config_flash,
	       NULL,
	       "Configure the flash (flash1) while NOT booting from flash.",
	       "config flash [-n|-w] [-b|-o]",
	       "-n;switch flash1 to 8-bit mode|"
	       "-w;switch flash1 to 16-bit mode|"
	       "-b;burst on|"	
	       "-o;burst off");
    return 0;
}


/* NORMAL WRITE to the cpld
 * Write 8'b01010101 to address 8'b00000001 of the cpld
 */
static int ui_cmd_writecpld(ui_cmdline_t *cmd,int argc,char *argv[])
{
// make sure burst mode is DISABLED
  *((volatile uint64_t *)PHYS_TO_K1(A_IO_EXT_CS_BASE(CPLD_CS))) = CPLD_CONFIG;

  xprintf ("writing 0x55 to cpld address 0x01\n");
  *((volatile uint8_t *) PHYS_TO_K1(0x100B0001)) = (uint8_t) (0x55);
  xprintf("Done.\n\n");
  return 0;
}


/* NORMAL READ to the cpld
 * The cpld is programmed to output the current cpld SUM if address 0x00 is
 * read.  However, if any other address is read, then the cpld will output
 * 5'b00000 concatenated with the lowest three address bits.
 * ie.
 * reading address 0xFF will return 0x07.
 * reading address 0x4A will return 0x02.
 * reading address 0x09 will return 0x01.
 */
static int ui_cmd_readcpld(ui_cmdline_t *cmd,int argc,char *argv[])
{

  uint8_t data;

// make sure burst mode is DISABLED
  *((volatile uint64_t *)PHYS_TO_K1(A_IO_EXT_CS_BASE(CPLD_CS))) = CPLD_CONFIG;

  data = *((volatile uint8_t *) PHYS_TO_K1(0x100B00FC));
  xprintf ("CPLD address 0xFC contains 0x%2X\n", data);
  xprintf ("This value should be 0x04\n\n");
  data = *((volatile uint8_t *) PHYS_TO_K1(0x100B0000));
  xprintf ("CPLD address 0x00 contains 0x%2X\n\n", data);

  return 0;
}


/* BURST WRITE to the cpld
 * Maximum burst size (without doing a UAC store) to cpld is 8 bytes.
 * byte1: 0x01
 * byte2: 0x02
 * byte3: 0x04
 * byte4: 0x08
 * byte5: 0x10
 * byte6: 0x20
 * byte7: 0x40
 * byte8: 0x80
 * To do the burst, write the 64-bit value 0x8040201008040201 to the cpld.
 * At the end of the burst, the cpld SUM register should contain 0xFF.
 */
static int ui_cmd_burstwcpld(ui_cmdline_t *cmd,int argc,char *argv[])
{
// enable burst mode
  *((volatile uint64_t *)PHYS_TO_K1(A_IO_EXT_CS_BASE(CPLD_CS))) = CPLD_CONFIG | M_IO_BURST_EN;

  xprintf("burst writing 8 bytes (0x8040201008040201) to cpld address 0x00\n");
  *((volatile uint64_t *) PHYS_TO_K1(0x100B0000)) = (uint64_t) (0x8040201008040201);
  xprintf("Done.\n\n");

  return 0;
}


/* BURST READ to the cpld
 * Burst reading the cpld at address 0x00 should return
 * 0x07060504030201 concatenated with the cpld SUM[7:0]
 */
static int ui_cmd_burstrcpld(ui_cmdline_t *cmd,int argc,char *argv[])
{
  uint64_t data;

// enable burst mode
  *((volatile uint64_t *)PHYS_TO_K1(A_IO_EXT_CS_BASE(CPLD_CS))) = CPLD_CONFIG | M_IO_BURST_EN;

  data = *((volatile uint64_t *) PHYS_TO_K1(0x100B0000));
  xprintf ("Address 0x00 of cpld contains (by burst reading) 0x%16X\n", data);
  xprintf ("This value should be 0x07060504030201FF if a burst write was just exectued\n\n");

  return 0;
}

static int ui_cmd_config_flash(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int fh;
    int flash_mode = 0;
    int burst_on = 0;

    fh = cfe_open("flash1");
    if (fh < 0) {
	xprintf("Could not open device flash1\n");
	return CFE_ERR_DEVNOTFOUND;
	}

    if (cmd_sw_isset(cmd,"-w")) {

	/*switch to 16-bit mode*/
	flash_mode = INTEL_FLASH_16BIT;
	}
    else if (cmd_sw_isset(cmd,"-n")) {
	/*switch to 8-bit mode*/
	flash_mode = INTEL_FLASH_8BIT;
	}	
    cfe_ioctl(fh,IOCTL_FLASH_DATA_WIDTH_MODE,(uint8_t *) &flash_mode, sizeof(flash_mode),NULL,0);

    if (cmd_sw_isset(cmd,"-b")) {
	burst_on = 1;
	}
    else if (cmd_sw_isset(cmd,"-o")) {
	burst_on = 0;
	}    
	cfe_ioctl(fh,IOCTL_FLASH_BURST_MODE,(uint8_t *) &burst_on, sizeof(burst_on),NULL,0);

    cfe_close(fh);
    return 0;

}

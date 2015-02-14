/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  P5064-specific commands			File: P5064_COMMANDS.C
    *  
    *  This module contains special command extensions for the
    *  Algorithmics P5064 port of CFE.
    *
    *  NOTE: Some of the routines in this module were borrowed
    *  from PMON.
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


#include "sbmips.h"
#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "cfe_devfuncs.h"
#include "cfe_timer.h"
#include "cfe_ioctl.h"

#include "cfe_error.h"
#include "env_subr.h"
#include "ui_command.h"
#include "cfe.h"

#include "sbd.h"
#include "rtc.h"

#include "pcivar.h"

#include "dev_flash.h"


#if !defined(__MIPSEB) && !defined(__MIPSEL)
#error "You must define either __MIPSEB or __MIPSEL"
#endif


int ui_init_p5064cmds(void);
static int ui_cmd_pcmcia(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_poweroff(ui_cmdline_t *cmd,int argc,char *argv[]);

#if CFG_VGACONSOLE
static int ui_cmd_vgadump(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_vgainit(ui_cmdline_t *cmd,int argc,char *argv[]);
extern int vga_biosinit(void);
extern void vgaraw_dump(char *tail);
#endif
static int ui_cmd_flashtest(ui_cmdline_t *cmd,int argc,char *argv[]);

#define INB(x) inb(x)
#define INW(x) inw(x)
#define INL(x) inl(x)

#define OUTB(x,y) outb(x,y)
#define OUTW(x,y) outw(x,y)
#define OUTL(x,y) outl(x,y)




int ui_init_p5064cmds(void)
{

    cmd_addcmd("pcmcia",
	       ui_cmd_pcmcia,
	       NULL,
	       "pcmcia stuff",
	       "pcmcia",
	       "");

    cmd_addcmd("power off",
	       ui_cmd_poweroff,
	       NULL,
	       "Power off the system.",
	       "power off\n\n"
	       "This command turns off the power for systems that support it.",
	       "");

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

    cmd_addcmd("test flash",
	       ui_cmd_flashtest,
	       NULL,
	       "Read manufacturer ID from flash",
	       "test flash",
	       "");

    return 0;
}


unsigned int
apc_bis (int reg, unsigned int val)
{
    unsigned int rtcsa, o, n;

    if (BOARD_REVISION < 'C')
	return ~0;

    OUTB(RTC_ADDR_PORT, RTC_STATUSA);
    rtcsa = INB(RTC_DATA_PORT);
    if ((rtcsa & RTCSA_DVMASK) != RTC_DV2_OSC_ON)
	OUTB(RTC_DATA_PORT, (rtcsa & ~RTCSA_DVMASK) | RTC_DV2_OSC_ON);

    OUTB(RTC_ADDR_PORT, reg);
    o = INB(RTC_DATA_PORT);
    n = o | val;
    if (o != n) {
	OUTB(RTC_DATA_PORT, n);
	}

    /* paranoia - switch back to bank 0 */
    OUTB(RTC_ADDR_PORT, RTC_STATUSA);
    OUTB(RTC_DATA_PORT, (rtcsa & ~RTCSA_DVMASK) | RTC_DV0_OSC_ON);

    return (o);
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


static void pcicdump(int slot)
{
    uint8_t b;
    int idx,idx2;

    for (idx = 0; idx < 63; idx+=16) {
	xprintf("%02X: ",idx);
	for (idx2 = 0; idx2 < 16; idx2++) {
	    OUTB(0x3E0,idx+idx2+slot*64);
	    b = INB(0x3E1);
	    xprintf("%02X ",b);
	    }
	xprintf("\n");
	}
}

#define PCICSET(reg,val) OUTB(0x3E0,(reg)); OUTB(0x3E1,(val))
#define PCICGET(reg,val) OUTB(0x3E0,(reg)); val = INB(0x3E1)

static int ui_cmd_pcmcia(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *tok;
    char *reg,*val;
    uint32_t start,stop;
    uint32_t offset;
    uint8_t *ptr;

    tok = cmd_getarg(cmd,0);
    if (!tok) tok = "dump";

    if (strcmp(tok,"dump") == 0) {
	xprintf("Slot 0:\n"); pcicdump(0);
	xprintf("\n");
	xprintf("Slot 1:\n"); pcicdump(1);
	}
    if (strcmp(tok,"set") == 0) {
	reg = cmd_getarg(cmd,1);
	val = cmd_getarg(cmd,2);
	if (!reg || !val) {
	    xprintf("pcmcia set reg val\n");
	    return -1;
	    }
	PCICSET(xtoi(reg),xtoi(val));
	}
    if (strcmp(tok,"on") == 0) {
	PCICSET(0x2F,0x70);
	PCICSET(0x02,0x30);
	cfe_sleep(CFE_HZ/4);
	PCICSET(0x02,0xB0);
	}
    if (strcmp(tok,"map") == 0) {
	reg = cmd_getarg(cmd,1);
	val = cmd_getarg(cmd,2);
	if (!reg) {
	    xprintf("pcmcia map start stop [a]\n");
	    return -1;
	    }
	start = xtoi(reg);
	stop = xtoi(val);
	PCICSET(0x06,0x00);
	PCICSET(0x10,(start >> 12) & 0xFF);
	PCICSET(0x11,((start >> 20) & 0x0F) | 0x00);
	PCICSET(0x12,(stop >> 12) & 0xFF);
	PCICSET(0x13,((stop >> 20) & 0x0F) | 0x00);

	offset = (uint32_t) ((int32_t) 0 - (int32_t) start);
	xprintf("Offset = %08X\n",offset);

	PCICSET(0x14,(offset >> 12) & 0xFF);
	PCICSET(0x15,(((offset >> 20) & 0x3F) | (cmd_getarg(cmd,3) ? 0x40 : 0)));

	PCICSET(0x06,0x01);

	}
    if (strcmp(tok,"detect") == 0) {
	uint8_t det,newdet;
	PCICGET(0x01,det);
	xprintf("%02X ",det);
	while (!console_status()) {
	    PCICGET(0x01,newdet);
	    if (det != newdet) {
		det = newdet;
		xprintf("%02X ",det);
		}
	    }
	}
    if (strcmp(tok,"tuples") == 0) {
	reg = cmd_getarg(cmd,1);
	if (!reg) {
	    xprintf("pcmcia tuples isa-addr\n");
	    return -1;
	    }
	start = xtoi(reg);
	ptr = (uint8_t *) (intptr_t) (0xB0000000 | start);
	while (*ptr != 0xFF) {
	    uint8_t tpl,len;

	    tpl = *ptr;
	    ptr+=2;
	    len = *ptr;
	    ptr+=2;
	    xprintf("Tuple @ %04X type %02X len %d Data ",
		    (((uint32_t) (intptr_t) ptr) & 0xFFFF) >> 1,tpl,len);
	    while (len) {
		xprintf("%02X ",*ptr);
		ptr += 2;
		len--;
		}
	    xprintf("\n");
	    }
	}
    
    return 0;
}


static int ui_cmd_poweroff(ui_cmdline_t *cmd,int argc,char *argv[])
{
    xprintf("Power off.\n");
    apc_bis(RTC_BANK2_APCR1, APCR1_SOC);
    for (;;) ;
    return 0;
}


static char *flashtypes[] = {
    "Unknown","SRAM","ROM","Flash"
};

static int ui_cmd_flashtest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    flash_info_t info;
    int fd;
    int retlen;
    int res = 0;
    int idx;
    flash_sector_t sector;
    nvram_info_t nvraminfo;
    char *devname;

    devname = cmd_getarg(cmd,0);
    if (!devname) return ui_showusage(cmd);

    fd = cfe_open(devname);
    if (fd < 0) {
	ui_showerror(fd,"Could not open flash device %s",devname);
	return fd;
	}

    res = cfe_ioctl(fd,IOCTL_FLASH_GETINFO,(uint8_t *) &info,sizeof(flash_info_t),&retlen,0);
    if (res == 0) {
	printf("FLASH: Base %016llX size %08X type %02X(%s) flags %08X\n",
	   info.flash_base,info.flash_size,info.flash_type,flashtypes[info.flash_type],
	       info.flash_flags);
	}
    else {
	printf("FLASH: Could not determine flash information\n");
	}

    res = cfe_ioctl(fd,IOCTL_NVRAM_GETINFO,(uint8_t *) &nvraminfo,sizeof(nvram_info_t),&retlen,0);
    if (res == 0) {
	printf("NVRAM: Offset %08X Size %08X EraseFlg %d\n",
	       nvraminfo.nvram_offset,nvraminfo.nvram_size,nvraminfo.nvram_eraseflg);
	}
    else {
	printf("NVRAM: Not supported by this flash\n");
	}

    if (info.flash_type == FLASH_TYPE_FLASH) {
	printf("Flash sector information:\n");

	idx = 0;
	for (;;) {
	    sector.flash_sector_idx = idx;
	    res = cfe_ioctl(fd,IOCTL_FLASH_GETSECTORS,(uint8_t *) &sector,sizeof(flash_sector_t),&retlen,0);
	    if (res != 0) {
		printf("ioctl error\n");
		break;
		}
	    if (sector.flash_sector_status == FLASH_SECTOR_INVALID) break;
	    printf("  Sector %d offset %08X size %d\n",
		   sector.flash_sector_idx,
		   sector.flash_sector_offset,
		   sector.flash_sector_size);     
	    idx++;
	    }
	}

    cfe_close(fd);
    return 0;

}

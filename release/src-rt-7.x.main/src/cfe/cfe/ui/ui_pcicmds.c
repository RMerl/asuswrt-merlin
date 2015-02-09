/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  PCI Commands				File: ui_pcicmds.c
    *  
    *  PCI user interface routines
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

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "env_subr.h"
#include "ui_command.h"
#include "cfe.h"

#include "pcivar.h"
#include "pcireg.h"

#include "bsp_config.h"

int ui_init_pcicmds(void);

#if CFG_PCI
static int pci_print_summary(pcitag_t tag)
{
    pcireg_t id, class;
    char devinfo[256];

    class = pci_conf_read(tag, PCI_CLASS_REG);
    id = pci_conf_read(tag, PCI_ID_REG);

    pci_devinfo(id, class, 1, devinfo);
    pci_tagprintf (tag, "%s\n", devinfo);

    return 0;	   
}

static int pci_print_concise(pcitag_t tag)
{
    pci_tagprintf (tag, "\n");
    pci_conf_print(tag);	

    return 0;
}

static int ui_cmd_pci(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *argp;

    if (cmd_sw_isset(cmd,"-init")) {
	pci_configure(PCI_FLG_LDT_PREFETCH | PCI_FLG_NORMAL);
	return 0;
	}

    argp = cmd_getarg(cmd,0);

    if (argp == NULL) {
	if (cmd_sw_isset(cmd,"-v")) {
	    pci_foreachdev(pci_print_concise);
	    }
	else {
	    pci_foreachdev(pci_print_summary);
	    }
	}
    else {
	/* parse the tuple */
	int n, port, bus, dev, func;
	pcitag_t tag;
	char *p;

	port = bus = dev = func = 0;
	p = argp;  n = 0;

	while (*p >= '0' && *p <= '9') {
	    n = n*10 + (*p - '0');
	    p++;
	    }
	if (*p == '/')
	    bus = n;
	else  if (*p == ':') {
	    port = n;
	    p++;
	    while (*p >= '0' && *p <= '9') {
		bus = bus*10 + (*p - '0');
		p++;
		}
	    }
	else
	    goto fail;
	p++;
	while (*p >= '0' && *p <= '9') {
	    dev = dev*10 + (*p - '0');
	    p++;
	    }
	if (*p != '/')
	    goto fail;
	p++;
	while (*p >= '0' && *p <= '9') {
	    func = func*10 + (*p - '0');
	    p++;
	    }
	if (*p != '\000')
	    goto fail;

	tag = pci_make_tag(port,bus,dev,func);	

	pci_print_concise(tag);
	}

    return 0;

fail:
    printf("invalid PCI tuple %s\n", argp);
    return -1;
}


static uint64_t parse_hex(const char *num)
{
    uint64_t x = 0;
    unsigned int digit;

    if ((*num == '0') && (*(num+1) == 'x')) num += 2;

    while (*num) {
        if ((*num >= '0') && (*num <= '9')) {
            digit = *num - '0';
            }
        else if ((*num >= 'A') && (*num <= 'F')) {
            digit = 10 + *num - 'A';
            }
        else if ((*num >= 'a') && (*num <= 'f')) {
            digit = 10 + *num - 'a';
            }
        else {
            break;
            }
        x *= 16;
        x += digit;
        num++;
        }

    return x;
}

static int ui_cmd_map_pci(ui_cmdline_t *cmd,int argc,char *argv[])
{
    unsigned long offset, size;
    uint64_t paddr;
    int l2ca, endian;
    int enable;
    int result;

    enable = !cmd_sw_isset(cmd, "-off");
    if (enable) {
	offset = parse_hex(cmd_getarg(cmd, 0));
	size = parse_hex(cmd_getarg(cmd, 1));
	paddr = parse_hex(cmd_getarg(cmd, 2));
	l2ca = cmd_sw_isset(cmd,"-l2ca");
	endian = cmd_sw_isset(cmd, "-matchbits");
	result = pci_map_window(paddr, offset, size, l2ca, endian);
	}
    else {
	offset = parse_hex(cmd_getarg(cmd, 0));
	size = parse_hex(cmd_getarg(cmd, 1));
	result = pci_unmap_window(offset, size);
	}

    return result;
}
#endif

#ifdef CFG_VGACONSOLE

extern int vga_biosinit(void);
extern int vga_probe(void);
extern void vgaraw_dump(char *tail);

static int ui_cmd_vgainit(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int res;

    if (vga_probe() == 0) {
	res = vga_biosinit();
	xprintf("vgabios_init returns %d\n",res);
	}
    else
	xprintf("vga_probe found no suitable adapter\n");

    return 0;
}

static int ui_cmd_vgadump(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *x;

    x = cmd_getarg(cmd,0);
    if (x == NULL) x = "";
    vgaraw_dump(x);

    return 0;
}
#endif /* CFG_VGACONSOLE */


int ui_init_pcicmds(void)
{

#if CFG_PCI
    cmd_addcmd("show pci",
	       ui_cmd_pci,
	       NULL,
	       "Display information about PCI buses and devices",
	       "show pci [-v] [bus/dev/func]\n\n"
	       "Displays information about PCI and LDT buses and devices in the\n"
	       "system.  If you specify a bus/dev/func triplet, only that device\n"
	       " will be displayed.",
	       "-v;Display verbose information|"
	       "-init;Reinitialize and rescan the PCI bus");
    cmd_addcmd("map pci",
	       ui_cmd_map_pci,
	       NULL,
	       "Define a BAR0 window available to PCI devices",
	       "map pci offset size paddr [-off] [-l2ca] [-matchbits]\n\n"
	       "Map the region of size bytes starting at paddr to appear\n"
	       "at offset relative to BAR0\n",
	       "-off;Remove the region|"
	       "-l2ca;Make L2 cachable|"
	       "-matchbits;Use match bits policy");

#ifdef CFG_VGACONSOLE
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
#endif /* CFG_VGACONSOLE */
#endif
    return 0;
}

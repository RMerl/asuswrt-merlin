/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Ethernet test commands			File: ui_test_ether.c
    *  
    *  User interface commands to test Ethernet devices
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
#include "cfe_error.h"
#include "cfe_ioctl.h"
#include "cfe_devfuncs.h"
#include "ui_command.h"
#include "cfe.h"

typedef struct netparam_s {
    const char *str;
    int num;
} netparam_t;

const static netparam_t speedtypes[] = {
    {"auto",ETHER_SPEED_AUTO},
    {"10hdx",ETHER_SPEED_10HDX},
    {"10fdx",ETHER_SPEED_10FDX},
    {"100hdx",ETHER_SPEED_100HDX},
    {"100fdx",ETHER_SPEED_100FDX},
    {"1000hdx",ETHER_SPEED_1000HDX},
    {"1000fdx",ETHER_SPEED_1000FDX},
    {0,NULL}};


int ui_init_ethertestcmds(void);

static int ui_cmd_ethertest(ui_cmdline_t *cmd,int argc,char *argv[]);

int ui_init_ethertestcmds(void)
{
    cmd_addcmd("test ether",
	       ui_cmd_ethertest,
	       NULL,
	       "Do an ethernet test, reading packets from the net",
	       "test ether device-name",
	       "-speed=*;Specify speed|"
               "-q;Be quiet|"
	       "-send=*;Transmit packets"
	);

    return 0;
}


static int ui_ifconfig_lookup(char *name,char *val,const netparam_t *list) 
{
    const netparam_t *p = list;
    
    while (p->str) {
	if (strcmp(p->str,val) == 0) return p->num;
	p++;
	}

    xprintf("Invalid parameter for %s: Valid options are: ");
    
    p = list;
    while (p->str) {
	xprintf("%s ",p->str);
	p++;
	}

    xprintf("\n");
    return -1;
}

static int ui_cmd_ethertest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *tok;
    int fh;
    uint8_t packet[2048];
    int res;
    int idx;
    int speed = ETHER_SPEED_AUTO;
    char *x;
    int count = 0;
    int quiet;

    tok = cmd_getarg(cmd,0);
    if (!tok) return -1;

    if (cmd_sw_value(cmd,"-speed",&x)) {
	speed = ui_ifconfig_lookup("-speed",x,speedtypes);
	if (speed < 0) return CFE_ERR_INV_PARAM;
	}

    quiet = cmd_sw_isset(cmd,"-q");


    fh = cfe_open(tok);
    if (fh < 0) {
	xprintf("Could not open device: %s\n",cfe_errortext(fh));
	return fh;
	}

    if (speed != ETHER_SPEED_AUTO) {
	xprintf("Setting speed to %d...\n",speed);
	cfe_ioctl(fh,IOCTL_ETHER_SETSPEED,(uint8_t *) &speed,sizeof(speed),&idx,0);
	}


    if (cmd_sw_value(cmd,"-send",&x)) {
	count = atoi(x);
	memset(packet,0xEE,sizeof(packet));
	memcpy(packet,"\xFF\xFF\xFF\xFF\xFF\xFF\x40\x00\x00\x10\x00\x00\x12\x34",16);
	res = 0;
	for (idx = 0; idx < count; idx++) {
	    res = cfe_write(fh,packet,128);
	    if (res < 0) break;
	    }
	if (res) {
	    ui_showerror(res,"Could not transmit packet");
	    }
	cfe_close(fh);
	return 0;
	}

    xprintf("Receiving... press enter to stop\n");
    while (!console_status()) {
	res = cfe_read(fh,packet,sizeof(packet));
	if (res == 0) continue;
	if (res < 0) {
	    xprintf("Read error: %s\n",cfe_errortext(res));
	    break;
	    }

	if (!quiet) {
	    xprintf("%4d ",res);
	    if (res > 32) res = 32;

	    for (idx = 0; idx < res; idx++) {
		xprintf("%02X",packet[idx]);
		if ((idx == 5) || (idx == 11) || (idx == 13)) xprintf(" ");
		}

	    xprintf("\n");
	    }

	count++;
	if (quiet && !(count % 1000)) printf(".");
	}

    printf("Total packets received: %d\n",count);

    cfe_close(fh);

    return 0;
}

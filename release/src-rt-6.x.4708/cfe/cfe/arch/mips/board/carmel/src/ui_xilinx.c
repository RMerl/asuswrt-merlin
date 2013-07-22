/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Xilinx programming commands		File: ui_xilinx.c
    *  
    *  Some routines to program on-board xilinxes.
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
#include "lib_printf.h"
#include "lib_string.h"
#include "ui_command.h"
#include "cfe.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "cfe_devfuncs.h"

#include "sbmips.h"
#include <sb1250-include/sb1250_regs.h>
#include "carmel.h"
#include "xilinx.h"
#include "cfe_flashimage.h"
#include "carmel_env.h"

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

xpgminfo_t xilinxinfo = {
    M_GPIO_FPGA_INIT,
    M_GPIO_FPGA_DONE,
    M_GPIO_FPGA_PGM,
    M_GPIO_FPGA_DIN,
    M_GPIO_FPGA_DOUT,
    M_GPIO_FPGA_CCLK,
    0
};

xpgminfo_t xilinxinfo_convoluted = {
    M_GPIO_FPGACONV_INIT,
    M_GPIO_FPGACONV_DONE,
    M_GPIO_FPGACONV_PGM,
    M_GPIO_FPGACONV_DIN,
    0,
    M_GPIO_FPGACONV_CCLK,
    0
};

#define XILINX_STAGING_BUFFER	(256*1024)
#define XILINX_DEVICE_SIZE	130012

#define FPGA_IMAGE_SEAL	"FPGA"

/*  *********************************************************************
    *  prototypes
    ********************************************************************* */

static int ui_cmd_xload(ui_cmdline_t *cmd,int argc,char *argv[]) ;
static int ui_cmd_xlist(ui_cmdline_t *cmd,int argc,char *argv[]) ;
int ui_init_xpgmcmds(void);

static char *xload_devname = "flash0.fpga";

/*  *********************************************************************
    *  ui_init_xpgmcmds()
    *  
    *  Add Xilinx commands to the command table
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */


int ui_init_xpgmcmds(void)
{
    cmd_addcmd("xload",
	       ui_cmd_xload,
	       NULL,
	       "Load a bitstream stored in a flash device into the Xilinx FPGA",
	       "xload [key]\n\n"
	       "By default, xload loads the appropriate bitstream according to data\n"
	       "in the ID EEPROM.  xload first looks at M_FPGA, and then looks\n"
	       "at M_BOARDTYPE to determine the key.\n",
	       "");

    cmd_addcmd("xlist",
	       ui_cmd_xlist,
	       NULL,
	       "List Xilinx bitstreams stored in flash.",
	       "xlist",
	       "");
    return 0;
}

static int ui_cmd_xlist(ui_cmdline_t *cmd,int argc,char *argv[]) 
{
    int fh;
    cfe_flashimage_t header;
    cfe_offset_t offset = 0;
    int res;
    uint32_t len;
    int cnt = 0;
    char vbuf[32];

    fh = cfe_open(xload_devname);
    if (fh < 0) return ui_showerror(fh,"Could not open device %s",xload_devname);

    for (;;) {
	res = cfe_readblk(fh,offset,(unsigned char *)&header,sizeof(header));
	if (res != sizeof(header)) break;
	
	if (memcmp(header.seal,FPGA_IMAGE_SEAL,4) != 0) break;

	len = ((uint32_t) header.size[0] << 24) |
	    ((uint32_t) header.size[1] << 16) |
	    ((uint32_t) header.size[2] << 8) |
	    ((uint32_t) header.size[3] << 0);

	offset += sizeof(header);
	    
	if (len == 0) break;

	if (cnt == 0) {
	    printf("FPGA Key         Version  Size\n");
	    printf("---------------- -------- ------\n");
	    }

	sprintf(vbuf,"%d.%d.%d",header.majver,header.minver,header.ecover);
	printf("%16s %8s %d\n",header.boardname,vbuf,len);
	cnt++;
	offset += len;
	}

    if (cnt == 0) printf("No FPGA bitstreams found on device %s\n",xload_devname);

    cfe_close(fh);
    return 0;
}


/*  *********************************************************************
    *  fpga_crc32(buf,len)
    *  
    *  Yes, this is an Ethernet CRC.  I'm lazy.
    *  
    *  Input parameters: 
    *  	   buf - buffer to CRC
    *  	   len - length of data
    *  	   
    *  Return value:
    *  	   CRC-32
    ********************************************************************* */

#define     CRC32_POLY        0xEDB88320UL    /* CRC-32 Poly */
static unsigned int
fpga_crc32(const unsigned char *databuf, unsigned int  datalen) 
{       
    unsigned int idx, bit, data, crc = 0xFFFFFFFFUL;
 
    for (idx = 0; idx < datalen; idx++) {
	for (data = *databuf++, bit = 0; bit < 8; bit++, data >>= 1) {
	    crc = (crc >> 1) ^ (((crc ^ data) & 1) ? CRC32_POLY : 0);
	    }
	}

    return crc;
}

static int ui_cmd_xload(ui_cmdline_t *cmd,int argc,char *argv[]) 
{
    uint8_t *xbuffer = (uint8_t *) PHYS_TO_K0(XILINX_STAGING_BUFFER);
    int res;
    uint32_t hdrcrc,calccrc;
    int found = 0;
    int fh;
    cfe_flashimage_t header;
    cfe_offset_t offset = 0;
    uint32_t len;
    char *key;
    char *conv;
    xpgminfo_t *xilinx;

    key = cmd_getarg(cmd,0);
    if (!key) {
	key = carmel_getenv("M_FPGA");
	if (!key) key = carmel_getenv("M_BOARDTYPE");
	if (!key) {
	    printf("Cannot determine which FPGA to load.  Use 'xlist' for a list of available FPGAs.\n\n");
	    return ui_showusage(cmd);
	    }
	}

    fh = cfe_open(xload_devname);
    if (fh < 0) return ui_showerror(fh,"Could not open device %s",xload_devname);

    len = 0;
    hdrcrc = 0;

    for (;;) {
	res = cfe_readblk(fh,offset,(unsigned char *)&header,sizeof(header));
	if (res != sizeof(header)) break;
	
	if (memcmp(header.seal,FPGA_IMAGE_SEAL,4) != 0) break;

	len = ((uint32_t) header.size[0] << 24) |
	    ((uint32_t) header.size[1] << 16) |
	    ((uint32_t) header.size[2] << 8) |
	    ((uint32_t) header.size[3] << 0);

	hdrcrc = ((uint32_t) header.crc[0] << 24) |
	    ((uint32_t) header.crc[1] << 16) |
	    ((uint32_t) header.crc[2] << 8) |
	    ((uint32_t) header.crc[3] << 0);

	offset += sizeof(header);
	    
	if (len == 0) break;

	if (strcmpi(header.boardname,key) == 0) {
	    found = 1;
	    break;
	    }
	offset += len;
	}


    if (!found) {
	printf("Cannot find FPGA key '%s' in device '%s'\n",key,xload_devname);
	cfe_close(fh);
	return -1;
	}

    res = cfe_readblk(fh,offset,xbuffer,len);
    if (res != len) {
	printf("Could not read data from device %s\n",xload_devname);
	cfe_close(fh);
	return -1;
	}

    cfe_close(fh);

    calccrc = fpga_crc32(xbuffer,len);
    if (calccrc != hdrcrc) {
	printf("FPGA CRC does not match header.  FPGA image may be invalid.\n");
	}

    xilinx = &xilinxinfo;
    conv = carmel_getenv("M_FPGACONVOLUTED");
    if (conv && (strcmp(conv,"YES") == 0)) xilinx = &xilinxinfo_convoluted;

    res = xilinx_program(xilinx,xbuffer,len*8);

    if (res < 0) {
	printf("Xilinx programming failure: ");

	switch (res) {
	    case XPGM_ERR_INIT_NEVER_LOW:
		printf("INIT never went low\n");
		break;
	    case XPGM_ERR_INIT_NEVER_HIGH:
		printf("INIT never went high\n");
		break;
	    case XPGM_ERR_INIT_NOT_HIGH:
		printf("INIT not high\n");
		break;
	    case XPGM_ERR_DONE_NOT_HIGH:
		printf("DONE not high\n");
		break;
	    case XPGM_ERR_OPEN_FAILED: 
		printf("OPEN failed\n");
		break;
	    case XPGM_ERR_CLOSE_FAILED:
		printf("CLOSE failed\n");
		break;
	    }
	}

    return (res < 0) ? -1 : 0;
}

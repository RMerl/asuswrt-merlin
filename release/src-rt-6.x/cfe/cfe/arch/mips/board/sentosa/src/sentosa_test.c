/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Test commands for c3 board		      File: c3_tests.c
    *  
    *  
    *  Author:  Binh Vo (binh@broadcom.com)
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
#include "sb1250_regs.h"

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "ui_command.h"
#include "cfe_console.h"
#include "cfe_error.h"
#include "cfe_ioctl.h"
#include "cfe.h"

int ui_init_sentosa_testcmds(void);
static int ui_cmd_fifotest(ui_cmdline_t *cmd, int argc, char *argv[]);
static int ui_cmd_download(ui_cmdline_t *cmd, int argc, char *argv[]);

int ui_init_sentosa_testcmds(void)
{
    cmd_addcmd("dl",
	       ui_cmd_download,
	       NULL,
	       "Wait for a PCI download from the host",
	       "dl",
	       ""
	       );
    cmd_addcmd("test fifo",
	       ui_cmd_fifotest,
	       NULL,
	       "Do a packet fifo mode test in 8 or 16-bit mode and various other options.",
	       "test fifo {eth0|eth1|eth2} [-8|-16] [-encoded|-gmii|-sop|-eop] [options..]\n\n"
	       "This command converts the MAC(s) into packet FIFO mode.  In 8-bit mode, f0, f1,\n"
	       "and f2 correspond to eth0, eth1, and eth2.  In 16-bit mode, f0 and f1 correspond\n"
	       "to eth0 and eth2.",
	       "-8;8-bit mode (default)|"
	       "-16;16-bit mode|"
	       "-encoded;Encoded signal mode (default)|"
	       "-gmii;GMII style signal mode|"
	       "-sop;SOP flagged signal mode (8-bit mode only)|"
	       "-eop;EOP flagged signal mode (8-bit mode only)|"
	       "-loopback;Internal loopback (8-bit mode only)|"
	       "-pktsize=*;Packet size (default=32 bytes)|"
	       "-pktnum=*;Number of packets to send (default=1)|"
	       "-i;Interactive mode for user-defined data"
	       );

    return 0;
}

extern int cfe_device_download(int boot, char *options);

static int ui_cmd_download(ui_cmdline_t *cmd, int argc, char *argv[])
{
    cfe_device_download(1, "");

    return 0;
}

static int ui_cmd_fifotest(ui_cmdline_t *cmd, int argc, char *argv[])
{
    char *dev = "eth0";
    int fh;
    int fifo_mode = ETHER_FIFO_8; 
    int strobe = ETHER_STROBE_ENCODED;
    int loopback = ETHER_LOOPBACK_OFF;	/* only in 8-bit mode */

    int idx,idx2,idx3,idx4;
    int res=0;
    uint8_t tx_packet[1024];
    uint8_t rx_packet[1024];
    int pktsize=32;
    int pktnum=1;
    int interactive = FALSE;
    char *tmp;
    int pktsize_disp;

    char prompt[12];
    char line[256];
    int num_read=0;

    int rx_count=0;
    int tx_count=0;

    char tmp_char;
    int hi;
    int lo;

    /* obtain and open device(mac) */
    dev = cmd_getarg(cmd,0);
    if (!dev) return ui_showusage(cmd);    
    fh = cfe_open(dev);        
    if (fh < 0) {
	xprintf("Could not open device: %s\n",cfe_errortext(fh));
	return fh;
	}
    
    if (cmd_sw_isset(cmd,"-i")) interactive = TRUE;

    /* 8(default) or 16 bit fifo mode */
    if (cmd_sw_isset(cmd,"-16")) {
	if( (strcmp(dev,"eth1")==0) ) {
	    xprintf("16-bit mode not available on eth1.\n");
	    return -1;
	}
	fifo_mode = ETHER_FIFO_16;
	}
     cfe_ioctl(fh,IOCTL_ETHER_SETPACKETFIFO, (uint8_t *) &fifo_mode, sizeof(fifo_mode),NULL,0);

     /* signal type: encoded(default),gmii,sop(16-bit only),and eop(16-bit only) */
    if (cmd_sw_isset(cmd,"-gmii")) {  
	strobe = ETHER_STROBE_GMII;
	}
    else if (cmd_sw_isset(cmd,"-sop")) {
	strobe = ETHER_STROBE_SOP;
	}
    else if (cmd_sw_isset(cmd,"-eop")) {
	strobe = ETHER_STROBE_EOP;
	}
    cfe_ioctl(fh,IOCTL_ETHER_SETSTROBESIG, (uint8_t *)&strobe,sizeof(strobe),NULL,0);

    /* internal loopback (only for 8-bit fifo) */
    if ( (cmd_sw_isset(cmd,"-loopback")) && (fifo_mode == ETHER_FIFO_8) ) {
	loopback = ETHER_LOOPBACK_INT;
	}
    cfe_ioctl(fh,IOCTL_ETHER_SETLOOPBACK, (uint8_t *) &loopback, sizeof(loopback),NULL,0);

    /* packet size and number of packets to send */
    tmp = NULL;
    cmd_sw_value(cmd,"-pktsize",&tmp);
    if (tmp != NULL) {
	pktsize = atoi(tmp);
	}
    tmp = NULL;
    cmd_sw_value(cmd,"-pktnum",&tmp);
    if (tmp != NULL) {
	pktnum = atoi(tmp);
	}
               
    memset(tx_packet,0xEE,sizeof(tx_packet));
    memset(rx_packet,0xFF,sizeof(rx_packet));
    memcpy(tx_packet,"\xAA\xBB\xCC\xDD\x00\x01\x02\x03\x04\x05\x06\x07\x08\x11",14);
      
    xprintf("\n");
    
    xprintf("Transmit %d packet(s):\n",pktnum);
    xprintf("%d  ",pktsize);
    pktsize_disp = pktsize;
    if(pktsize_disp > 32) {
	pktsize_disp = 32;
	}
    for (idx2 = 0,idx = 1; idx2 < pktsize_disp; idx2++,idx++) {
        xprintf("%02X",tx_packet[idx2]);
        if ( (idx % 4) == 0 ) xprintf(" ");
        }
    
    if(pktsize > pktsize_disp) {
	xprintf("...");
	}
    xprintf("\n\n");

    for (idx=0; idx < pktnum; idx++) {
	res = cfe_write(fh,tx_packet,pktsize);

	if (res < 0) {
	    /* If transmit fails, descriptor ring probably full, try to read sent packets*/
	    for(idx2=0; idx2 < idx; idx2++) {
		res = cfe_read(fh,rx_packet,pktsize);
		if (res == 0) continue;
		    
		xprintf("RX[%d] %d  ",rx_count,res);
		rx_count++;
		if (res > 32) res = 32;
		for (idx3 = 0,idx4 = 1; idx3 < res; idx3++,idx4++) {
		    xprintf("%02X",rx_packet[idx3]);
		    if ( (idx4 % 4) == 0 ) xprintf(" ");
		    }
		if(pktsize > pktsize_disp) xprintf("...");
		xprintf("\n");
		}

	    /*Retransmit the packet that failed*/
	    res = cfe_write(fh,tx_packet,pktsize);
	    if (res < 0) ui_showerror(res,"ERROR Could not transmit packet");
	    }
	tx_count++;
	}

    /*Receive pre-defined test data*/
    while (!console_status()) {
	res = cfe_read(fh,rx_packet,pktsize);
	if (res == 0) continue;
	if (res < 0) {
	    xprintf("Read error: %s\n",cfe_errortext(res));
	    break;
	    }
	xprintf("RX[%d] %d  ",rx_count,res);
	rx_count++;
	if (res > 32) res = 32;
	    
	for (idx = 0,idx2 = 1; idx < res; idx++,idx2++) {
	    xprintf("%02X",rx_packet[idx]);
            if ( (idx2 % 4) == 0 ) xprintf(" ");
	    }

	if(pktsize > pktsize_disp) xprintf("...");
	
	xprintf("\n");

	if(rx_count >= tx_count) break;
	}

    if (interactive) {

	tx_count=0;
	rx_count=0;
	memset(tx_packet,0xEE,sizeof(tx_packet));
	
	xprintf("\nEnter hex value without '0x'. 't' to transmit. 'r' to receive. 'e' to end\n");
	for(;;) {

	    xsprintf(prompt,"TX[%d] :",tx_count);
	    num_read = console_readline(prompt,line,sizeof(line));

	    if (line[0] == 'e') break;
	    
	    if (line[0] == 'r') {

		/*Receive data*/
		for(idx = 0; idx < tx_count+5; idx++) {
		    res = cfe_read(fh,rx_packet,sizeof(rx_packet));	
		    if (res < 0) {
			xprintf("Read error: %s\n",cfe_errortext(res));
			break;
			}
		    if (res == 0) continue;
		    xprintf("RX[%d] %d  ",rx_count,res);
		    rx_count++;
		    if (res > 32) res = 32;
	    
		    for (idx3 = 0,idx4 = 1; idx3 < res; idx3++,idx4++) {
			xprintf("%02X",rx_packet[idx3]);
			if ( (idx4 % 4) == 0 ) xprintf(" ");
			}

		    if(pktsize > pktsize_disp) xprintf("...");
	
		    xprintf("\n");
		    }
		xprintf("\n");
	      } 
	    else if (line[0] == 't') {

		/*Transmit data*/
		xprintf("Transmit %d packet(s):\n",pktnum);
		xprintf("%d  ",pktsize);
		pktsize_disp = pktsize;
		if(pktsize_disp > 32) {
		    pktsize_disp = 32;
		    }
		for (idx2 = 0,idx = 1; idx2 < pktsize_disp; idx2++,idx++) {
		    xprintf("%02X",tx_packet[idx2]);
		    if ( (idx % 4) == 0 ) xprintf(" ");
		    }
    
		if(pktsize > pktsize_disp) {
		    xprintf("...");
		    }
		xprintf("\n\n");
       
		for(idx = 0; idx < pktnum; idx++) {
		    res = cfe_write(fh,tx_packet,pktsize);
		    if (res < 0) {

			/* If transmit fails, descriptor ring probably full, try to read sent packets*/
			for(idx2=0; idx2 < idx; idx2++) {
			    res = cfe_read(fh,rx_packet,pktsize);
			    if (res == 0) continue;
		    
			    xprintf("RX[%d] %d  ",rx_count,res);
			    rx_count++;
			    if (res > 32) res = 32;
			    for (idx3 = 0,idx4 = 1; idx3 < res; idx3++,idx4++) {
				xprintf("%02X",rx_packet[idx3]);
				if ( (idx4 % 4) == 0 ) xprintf(" ");
				}
			    if(pktsize > pktsize_disp) xprintf("...");
			    xprintf("\n");
			    }

			/*Retransmit the packet that failed*/
			res = cfe_write(fh,tx_packet,pktsize);
			if (res < 0) ui_showerror(res,"ERROR Could not transmit packet");
			}
		    }
		tx_count++;
		memset(tx_packet,0xEE,sizeof(tx_packet));

		}
	    else {
		/* Pack the packet */
		for (idx = 0,idx2=0; idx < (num_read+1)/2; idx++,idx2+=2) {
		    tmp_char = line[idx2 + 0];
		    hi = xtoi(&tmp_char);
		    hi = hi << 4; 
		    tmp_char = line[idx2 + 1];
		    lo = xtoi(&tmp_char); 
		    lo = lo | hi; 
		    tx_packet[idx] = lo;
		    }
		}

	    } /* for(;;) */
	}

    /* Closing the device will reset the MAC # registers */
    cfe_close(fh);

    return 0;	
}

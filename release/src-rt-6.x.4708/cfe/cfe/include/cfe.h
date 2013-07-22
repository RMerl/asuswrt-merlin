/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  CFE version # and prototypes		File: cfe.h
    *  
    *  CFE's version # temporarily lives here.
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


/*  *********************************************************************
    *  Version number
    ********************************************************************* */

#define CFE_VER_MAJOR	CFE_VER_MAJ
#define CFE_VER_MINOR	CFE_VER_MIN
#define CFE_VER_BUILD   CFE_VER_ECO

/*  *********************************************************************
    *  Some runtime startup parameters
    ********************************************************************* */

extern unsigned cfe_startflags;

#define CFE_INIT_USER	0x0000FFFF		/* these are BSP-specific flags */
#define CFE_INIT_SAFE	0x00010000		/* "Safe mode" */
#define CFE_INIT_PCI	0x00020000		/* Initialize PCI */
#define CFE_LDT_SLAVE   0x00040000		/* Select LDT slave mode */

/*  *********************************************************************
    *  Other constants
    ********************************************************************* */

#define CFE_MAX_HANDLE 64		/* max file handles */

/*  *********************************************************************
    *  prototypes
    ********************************************************************* */

void board_console_init(void);
void board_device_init(void);
void board_final_init(void);
void board_device_reset(void);
#define CFE_BUFFER_CONSOLE "buffer"
int cfe_set_console(char *);
int cfe_set_envdevice(char *);
void cfe_restart(void);
void cfe_command_loop(void);
void cfe_leds(unsigned int val);
void cfe_ledstr(const char *str);
void cfe_launch(unsigned long ept);
void cfe_start(unsigned long ept);
void cfe_warmstart(unsigned long long);
#define SETLEDS(x) cfe_ledstr(x)
const char *cfe_errortext(int err);
#ifdef CFG_NFLASH
void ui_get_boot_flashdev(char *flashdev);
void ui_get_os_flashdev(char *flashdev);
void ui_get_trx_flashdev(char *flashdev);
void dump_nflash(int block_no);
#endif /* CFG_NFLASH */

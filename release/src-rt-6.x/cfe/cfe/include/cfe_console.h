/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Console prototypes			File: cfe_console.c
    *  
    *  Prototypes for routines dealing with the console.
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
    *  Constants
    ********************************************************************* */

#define VKEY(x)		(0x100|(x))
#define VKEY_UP		VKEY(1)
#define VKEY_DOWN	VKEY(2)
#define VKEY_LEFT	VKEY(3)
#define VKEY_RIGHT	VKEY(4)
#define VKEY_PGUP	VKEY(5)
#define VKEY_PGDN	VKEY(6)
#define VKEY_HOME	VKEY(7)
#define VKEY_END	VKEY(8)
#define VKEY_F1		VKEY(0x10)
#define VKEY_F2		VKEY(0x11)
#define VKEY_F3		VKEY(0x12)
#define VKEY_F4		VKEY(0x13)
#define VKEY_F5		VKEY(0x14)
#define VKEY_F6		VKEY(0x15)
#define VKEY_F7		VKEY(0x16)
#define VKEY_F8		VKEY(0x17)
#define VKEY_F9		VKEY(0x18)
#define VKEY_F10	VKEY(0x19)
#define VKEY_F11	VKEY(0x1A)
#define VKEY_F12	VKEY(0x1B)
#define VKEY_ESC	27


/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

int console_open(char *name);
int console_close(void);
int console_read(char *buffer,int length);
int console_write(char *buffer,int length);
int console_status(void);
int console_readkey(void);
int console_readline(char *prompt,char *str,int len);
int console_readline_noedit(char *prompt,char *str,int len);
int console_readline(char *prompt,char *str,int len);
extern char *console_name;
extern int console_handle;
void console_log(const char *tmplt,...);

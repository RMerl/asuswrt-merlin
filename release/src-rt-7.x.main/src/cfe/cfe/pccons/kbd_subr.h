/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  PC-style keyboard interface		File: KBD_SUBR.C
    *  
    *  This module converts a stream of scancodes into ASCII
    *  characters.  The scan codes come from a PC-style
    *  keyboard.
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

#define KEYQUEUELEN	16

#define KBDCMD_RESET	0xFF
#define KBDCMD_SETLEDS	0xED

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

typedef struct keystate_s {
    int		ks_shiftflags;
    char	ks_queue[KEYQUEUELEN];
    int		ks_head;
    int		ks_tail;
    int		(*ks_setleds)(struct keystate_s *ks,int leds);
    void	*ks_ref;
} keystate_t;

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

int kbd_read(keystate_t *ks);
int kbd_inpstat(keystate_t *ks);
void kbd_init(keystate_t *ks,int (*setleds)(keystate_t *,int),void *ref);
void kbd_doscan(keystate_t *ks,uint8_t scan);
#define kbd_getref(x) ((x)->ks_ref)

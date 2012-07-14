/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  VGA "teletype" routines			File: VGA_SUBR.H
    *  
    *  These routines implement a simple "glass tty" interface
    *  to a vga monitor.
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
    *  Structures
    ********************************************************************* */

typedef struct vga_term_s {
    physaddr_t vga_buffer;
    int vga_nrows;
    int vga_ncols;
    int vga_cursorX;
    int vga_cursorY;
    void (*vga_outb)(unsigned int isaport,uint8_t val);
} vga_term_t;

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */


void vga_clear(vga_term_t *vga);
void vga_setcursor(vga_term_t *vga,int x,int y);
void vga_writechar(vga_term_t *vga,uint8_t ch,uint8_t attr);
void vga_writestr(vga_term_t *vga,uint8_t *str,uint8_t attr,int len);
void vga_reset(vga_term_t *vga);
void vga_init(vga_term_t *vga,physaddr_t buffer,void (*outfunc)(unsigned int port,uint8_t val));

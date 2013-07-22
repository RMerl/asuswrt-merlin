/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  VGA "teletype" routines			File: VGA_SUBR.C
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


#include "lib_types.h"
#include "lib_string.h"
#include "lib_printf.h"
#include "lib_malloc.h"

#include "lib_physio.h"

#include "vga_subr.h"
#include "vga.h"


/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define OUTB(vga,port,val) (*((vga)->vga_outb))(port,val)

#ifdef __MIPSEB
#define VGA_SPACE_CHAR 0x2007		/* belongs somewhere else */
#else
#define VGA_SPACE_CHAR 0x0720
#endif

/*  *********************************************************************
    *  Data
    ********************************************************************* */


/*  *********************************************************************
    *  VGA_CLEAR(vga)
    *  
    *  Clear the VGA screen
    *  
    *  Input parameters: 
    *  	   vga - VGA object
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void vga_clear(vga_term_t *vga)
{
    int idx;

    /* Clear the frame buffer */

    for (idx = 0; idx < VGA_TEXTBUF_SIZE; idx+=2) {
	phys_write16(vga->vga_buffer+idx,VGA_SPACE_CHAR);
	}
}


/*  *********************************************************************
    *  VGA_SETCURSOR(vga,x,y)
    *  
    *  Set the hardware cursor position
    *  
    *  Input parameters: 
    *  	   vga - VGA object
    *  	   x,y - cursor location
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void vga_setcursor(vga_term_t *vga,int x,int y)
{
    unsigned int loc = y*vga->vga_ncols + x;

    OUTB(vga,VGA_CRTC_INDEX,CRTC_CURSOR_HIGH);
    OUTB(vga,VGA_CRTC_DATA,(loc >> 8) & 0xFF);
    OUTB(vga,VGA_CRTC_INDEX,CRTC_CURSOR_LOW);
    OUTB(vga,VGA_CRTC_DATA,(loc >> 0) & 0xFF);

    vga->vga_cursorX = x;
    vga->vga_cursorY = y;
}

/*  *********************************************************************
    *  VGA_SCROLL(vga)
    *  
    *  Scroll the display up one line
    *  
    *  Input parameters: 
    *  	   vga - VGA object
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void vga_scroll(vga_term_t *vga)
{
    int idx;
    int count;
    int rowsize;
    uint32_t t;

    rowsize = vga->vga_ncols * 2;
    count = (vga->vga_nrows-1) * rowsize;

    for (idx = 0; idx < count; idx+=4) {
	t = phys_read32(vga->vga_buffer+idx+rowsize);
	phys_write32(vga->vga_buffer+idx,t);
	}

    for (idx = 0; idx < rowsize; idx += 2) {
	phys_write16(vga->vga_buffer+(vga->vga_nrows-1)*rowsize+idx,VGA_SPACE_CHAR);
	}

    vga_setcursor(vga,0,vga->vga_nrows-1);
}

/*  *********************************************************************
    *  VGA_WRITECHAR(vga,ch,attr)
    *  
    *  Write a character to the display.  This routine also
    *  interprets some rudimentary control characters, such
    *  as tab, backspace, linefeed, and carriage return.
    *  
    *  Input parameters: 
    *  	   vga - VGA object
    *  	   ch - character to write
    *  	   attr - attribute byte for new character
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void vga_writechar(vga_term_t *vga,uint8_t ch,uint8_t attr)
{
    physaddr_t addr;

    switch (ch) {
	case 0x07:
	    break;
	case 0x09:
	    vga_writechar(vga,' ',attr);
	    while (vga->vga_cursorX % 8) vga_writechar(vga,' ',attr);
	    break;
	case 0x0A:
	    vga->vga_cursorY++;
	    if (vga->vga_cursorY > (vga->vga_nrows-1)) {
		vga_scroll(vga);
		}
	    break;
	case 0x08:
	    if (vga->vga_cursorX) {
		vga->vga_cursorX--;
		addr = vga->vga_buffer + (vga->vga_cursorX*2+vga->vga_cursorY*vga->vga_ncols*2);
		phys_write8(addr,' ');
		}
	    break;
	case 0x0D:
	    vga->vga_cursorX = 0;
	    break;
	default:
	    addr = vga->vga_buffer + (vga->vga_cursorX*2+vga->vga_cursorY*vga->vga_ncols*2);
	    phys_write8(addr,ch);
	    phys_write8(addr+1,attr);
	    vga->vga_cursorX++;
	    if (vga->vga_cursorX > (vga->vga_ncols-1)) {
		vga->vga_cursorX = 0;
		vga->vga_cursorY++;
		if (vga->vga_cursorY > (vga->vga_nrows-1)) {
		    vga_scroll(vga);
		    }
		}
	    break;
	}

    vga_setcursor(vga,vga->vga_cursorX,vga->vga_cursorY);
}

/*  *********************************************************************
    *  VGA_WRITESTR(vga,str,attr,len)
    *  
    *  Write a string of characters to the VGA
    *  
    *  Input parameters: 
    *  	   vga - VGA object
    *  	   str - pointer to buffer
    *  	   attr - attribute byte for characters we're writing
    *  	   len - number of characters to write
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void vga_writestr(vga_term_t *vga,uint8_t *str,uint8_t attr,int len)
{
    while (len) {
	vga_writechar(vga,*str,attr);
	str++;
	len--;
	}
}

/*  *********************************************************************
    *  VGA_RESET(vga)
    *  
    *  (mostly unused) - reset the VGA
    *  
    *  Input parameters: 
    *  	   vga - vga object
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void vga_reset(vga_term_t *vga)
{
    vga_clear(vga);
}

/*  *********************************************************************
    *  VGA_INIT(vga,buffer,outfunc)
    *  
    *  Initialize a VGA object
    *  
    *  Input parameters: 
    *  	   vga - VGA object
    *  	   buffer - pointer to VGA-style frame buffer (physical addr)
    *  	   outfunc - pointer to function to write ISA I/O ports
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void vga_init(vga_term_t *vga,physaddr_t buffer,void (*outfunc)(unsigned int port,uint8_t val))
{
    vga->vga_buffer =  buffer;
    vga->vga_cursorX = 0;
    vga->vga_cursorY = 0;
    vga->vga_nrows = VGA_TEXTMODE_ROWS;
    vga->vga_ncols = VGA_TEXTMODE_COLS;
    vga->vga_outb = outfunc;
}

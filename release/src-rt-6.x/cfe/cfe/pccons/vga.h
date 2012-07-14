/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  VGA definitions				File: VGA.H
    *  
    *  This module contains names of the registers and bits 
    *  commonly used on VGA adapters.
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



#define VGA_GFXCTL_INDEX	0x3CE
#define VGA_GFXCTL_DATA		0x3CF
#define VGA_CRTC_INDEX		0x3D4
#define VGA_CRTC_DATA		0x3D5
#define VGA_SEQ_INDEX		0x3C4
#define VGA_SEQ_DATA		0x3C5
#define VGA_INPSTATUS_R		0x3C2
#define VGA_MISCOUTPUT_W	0x3C2
#define VGA_MISCOUTPUT_R	0x3CC
#define VGA_ATTRIB_INDEX	0x3C0
#define VGA_ATTRIB_DATA		0x3C1
#define VGA_FEATURES_W		0x3DA
#define VGA_EXT_INDEX		0x3D6
#define VGA_EXT_DATA		0x3D7

#define CRTC_CURSOR_HIGH	0x0E
#define CRTC_CURSOR_LOW		0x0F

#define VGA_TEXTBUF_COLOR	0xB8000
#define VGA_TEXTBUF_MONO	0xB0000
#define VGA_TEXTBUF_SIZE	0x8000

#define VGA_ATTRIB_MONO		7

#define VGA_TEXTMODE_COLS	80
#define VGA_TEXTMODE_ROWS	25

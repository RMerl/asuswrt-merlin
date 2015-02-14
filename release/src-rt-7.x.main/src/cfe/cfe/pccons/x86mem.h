/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  X86 simulator sparse memory		File: X86MEM.H
    *  
    *  This module implements X86 memory for the X86 emulator
    *  used by the BIOS simulator.  To avoid allocating the
    *  entire 1MB of PC's addressable memory, this is a "sparse"
    *  memory model, allocating chunks of storage as needed.
    *  VGA BIOSes seem to do all sorts of bizarre things to memory
    *  so this helps reduce the total amount we need to allocate
    *  significantly.
    *
    *  In addition, this module lets the simulator "hook"
    *  ranges of memory to be handled by a callback
    *  routine.  This is used so that we can redirect
    *  accesses to VGA memory space to the PCI bus handler.
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

#define X86MEM_CHUNKBITS    15
#define X86MEM_ADDRESSBITS  20
#define X86MEM_CHUNKSIZE    (1<<X86MEM_CHUNKBITS)
#define X86MEM_CHUNKS	    (1<<(X86MEM_ADDRESSBITS-X86MEM_CHUNKBITS))
#define X86MEM_REGION(addr) (((addr) >> X86MEM_CHUNKBITS) & (X86MEM_CHUNKS-1))
#define X86MEM_OFFSET(addr) ((addr) & (X86MEM_CHUNKSIZE-1))

#define M_BYTE	1
#define M_WORD	2
#define M_DWORD	4

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

typedef struct x86mem_s {
    uint8_t *data[X86MEM_CHUNKS];
    uint32_t (*read[X86MEM_CHUNKS])(struct x86mem_s *x86mem,
				    uint32_t addr,int size);
    void  (*write[X86MEM_CHUNKS])(struct x86mem_s *x86mem,
				  uint32_t addr,uint32_t val,int size);
} x86mem_t;

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

void x86mem_init(x86mem_t *mem);
void x86mem_uninit(x86mem_t *mem);


uint8_t x86mem_readb(x86mem_t *mem,uint32_t addr);
uint16_t x86mem_readw(x86mem_t *mem,uint32_t addr);
uint32_t x86mem_readl(x86mem_t *mem,uint32_t addr);


void x86mem_writeb(x86mem_t *mem,uint32_t addr,uint8_t data);
void x86mem_writew(x86mem_t *mem,uint32_t addr,uint16_t data);
void x86mem_writel(x86mem_t *mem,uint32_t addr,uint32_t data);
void x86mem_memcpy(x86mem_t *mem,uint32_t destaddr,uint8_t *src,int count);
void x86mem_hook(x86mem_t *mem,uint32_t chunkaddr,
		 uint32_t (*readf)(x86mem_t *mem,uint32_t addr,int size),
		 void (*writef)(x86mem_t *mem,uint32_t addr,uint32_t val,int size));
void x86mem_hook_range(x86mem_t *mem,uint32_t chunkaddr,int size,
		 uint32_t (*readf)(x86mem_t *mem,uint32_t addr,int size),
		 void (*writef)(x86mem_t *mem,uint32_t addr,uint32_t val,int size));

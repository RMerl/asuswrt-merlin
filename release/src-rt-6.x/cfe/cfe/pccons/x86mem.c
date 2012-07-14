/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  X86 simulator sparse memory		File: X86MEM.C
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


#include "lib_types.h"
#include "lib_string.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "x86mem.h"

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define BSWAP_SHORT(s) ((((s) >> 8) & 0xFF) | (((s)&0xFF) << 8))
#define BSWAP_LONG(s) ((((s) & 0xFF000000) >> 24) | \
                       (((s) & 0x00FF0000) >> 8) | \
                       (((s) & 0x0000FF00) << 8) | \
                       (((s) & 0x000000FF) << 24))
 

/*  *********************************************************************
    *  X86MEM_INIT()
    *  
    *  Initialize an X86mem object
    *  
    *  Input parameters: 
    *  	   mem - X86mem object
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void x86mem_init(x86mem_t *mem)
{
    memset(mem,0,sizeof(mem));
}

/*  *********************************************************************
    *  X86MEM_UNINIT(mem)
    *  
    *  Uninitialize an  X86mem object, freeing any storage
    *  associated with it.
    *  
    *  Input parameters: 
    *  	   mem - x86mem object
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void x86mem_uninit(x86mem_t *mem)
{
    int idx;

    for (idx = 0; idx < X86MEM_CHUNKS; idx++) {
	if (mem->data[idx]) {
	    KFREE(mem->data[idx]);
	    mem->data[idx] = NULL;
	    }
	}
}

/*  *********************************************************************
    *  X86MEM_READB(mem,addr)
    *  
    *  Read a byte of memory from the X86mem object.
    *  
    *  Input parameters: 
    *  	   mem - x86mem object
    *  	   addr - address of byte to read
    *  	   
    *  Return value:
    *  	   byte read
    ********************************************************************* */

uint8_t x86mem_readb(x86mem_t *mem,uint32_t addr)
{
    uint8_t *p;

    if (mem->read[X86MEM_REGION(addr)]) {
	return (uint8_t) (*(mem->read[X86MEM_REGION(addr)]))(mem,addr,1);
	}

    p = (mem->data[X86MEM_REGION(addr)]);

    if (p) {
	return *(p + X86MEM_OFFSET(addr));
	}
    else {
	return 0;
	}
}

/*  *********************************************************************
    *  X86MEM_READW(mem,addr)
    *  
    *  Read a 16-bit word of memory from the X86mem object.
    *  
    *  Input parameters: 
    *  	   mem - x86mem object
    *  	   addr - address of word to read
    *  	   
    *  Return value:
    *  	   word read
    ********************************************************************* */

uint16_t x86mem_readw(x86mem_t *mem,uint32_t addr)
{
    uint8_t *p;
    uint16_t ret;

    if (mem->read[X86MEM_REGION(addr)]) {
	return (uint8_t) (*(mem->read[X86MEM_REGION(addr)]))(mem,addr,2);
	}

    p = (mem->data[X86MEM_REGION(addr)]);
    if (!p) return 0;

    if ((addr & 1) || (X86MEM_OFFSET(addr) == X86MEM_CHUNKSIZE-1)) {

	ret = ((uint16_t) x86mem_readb(mem,addr+0)) |
	    (((uint16_t) x86mem_readb(mem,addr+1)) << 8);
	return ret;
	}
    else {
	ret =  *((uint16_t *) (p+X86MEM_OFFSET(addr)));
#ifdef __MIPSEB
	ret = BSWAP_SHORT(ret);
#endif
	}

    return ret;
}

/*  *********************************************************************
    *  X86MEM_READL(mem,addr)
    *  
    *  Read a 32-bit dword of memory from the X86mem object.
    *  
    *  Input parameters: 
    *  	   mem - x86mem object
    *  	   addr - address of dword to read
    *  	   
    *  Return value:
    *  	   dword read
    ********************************************************************* */

uint32_t x86mem_readl(x86mem_t *mem,uint32_t addr)
{
    uint8_t *p;
    uint32_t ret;

    if (mem->read[X86MEM_REGION(addr)]) {
	return (uint8_t) (*(mem->read[X86MEM_REGION(addr)]))(mem,addr,4);
	}

    p = (mem->data[X86MEM_REGION(addr)]);
    if (!p) return 0;

    if ((addr & 3) || (X86MEM_OFFSET(addr) >= X86MEM_CHUNKSIZE-3)) {
	ret = ((uint32_t) x86mem_readb(mem,addr+0)) |
	    (((uint32_t) x86mem_readb(mem,addr+1)) << 8) |
	    (((uint32_t) x86mem_readb(mem,addr+2)) << 16) |
	    (((uint32_t) x86mem_readb(mem,addr+3)) << 24);
	}
    else {
	ret = *((uint32_t *) (p+X86MEM_OFFSET(addr)));
#ifdef __MIPSEB
	ret = BSWAP_LONG(ret);
#endif
	} 

    return ret;
}

/*  *********************************************************************
    *  X86MEM_WRITEB(mem,addr,data)
    *  
    *  Write a byte to the X86mem object
    *  
    *  Input parameters: 
    *  	   mem - x86mem object
    *  	   addr - address of byte to write
    *      data - data to write
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void x86mem_writeb(x86mem_t *mem,uint32_t addr,uint8_t data)
{
    uint8_t *p;

    if (mem->write[X86MEM_REGION(addr)]) {
	(*(mem->write[X86MEM_REGION(addr)]))(mem,addr,data,1);
	return;
	}

    p = (mem->data[X86MEM_REGION(addr)]);

    if (p) {
        *(p + X86MEM_OFFSET(addr)) = data;
	}
    else {
	p = mem->data[X86MEM_REGION(addr)] = KMALLOC(X86MEM_CHUNKSIZE,sizeof(uint32_t));
	if (p) {
	    memset(p,0,X86MEM_CHUNKSIZE);
	    *(p + X86MEM_OFFSET(addr)) = data;
	    }
	}
}

/*  *********************************************************************
    *  X86MEM_WRITEW(mem,addr,data)
    *  
    *  Write a 16-bit word to the X86mem object
    *  
    *  Input parameters: 
    *  	   mem - x86mem object
    *  	   addr - address of word to write
    *      data - data to write
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void x86mem_writew(x86mem_t *mem,uint32_t addr,uint16_t data)
{
    uint8_t *p;

    if (mem->write[X86MEM_REGION(addr)]) {
	(*(mem->write[X86MEM_REGION(addr)]))(mem,addr,data,2);
	return;
	}

    p = (mem->data[X86MEM_REGION(addr)]);

    if (!p || (addr & 1) || (X86MEM_OFFSET(addr) == X86MEM_CHUNKSIZE-1)) {
	x86mem_writeb(mem,addr+0,(data & 0xFF));
	x86mem_writeb(mem,addr+1,((data >> 8) & 0xFF));
	}
    else {
#ifdef __MIPSEB
	data = BSWAP_SHORT(data);
#endif
	*((uint16_t *) (p+X86MEM_OFFSET(addr))) = data;
	}
}

/*  *********************************************************************
    *  X86MEM_WRITEL(mem,addr,data)
    *  
    *  Write a 32-bit dword to the X86mem object
    *  
    *  Input parameters: 
    *  	   mem - x86mem object
    *  	   addr - address of dword to write
    *      data - data to write
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void x86mem_writel(x86mem_t *mem,uint32_t addr,uint32_t data)
{
    uint8_t *p;

    if (mem->write[X86MEM_REGION(addr)]) {
	(*(mem->write[X86MEM_REGION(addr)]))(mem,addr,data,4);
	return;
	}

    p = (mem->data[X86MEM_REGION(addr)]);

    if (!p || (addr & 3) || (X86MEM_OFFSET(addr) >= X86MEM_CHUNKSIZE-3)) {
	x86mem_writeb(mem,addr+0,(data & 0xFF));
	x86mem_writeb(mem,addr+1,((data >> 8) & 0xFF));
	x86mem_writeb(mem,addr+2,((data >> 16) & 0xFF));
	x86mem_writeb(mem,addr+3,((data >> 24) & 0xFF));
	}
    else {
#ifdef __MIPSEB
	data = BSWAP_LONG(data);
#endif
	*((uint32_t *) (p+X86MEM_OFFSET(addr))) = data;
	}
}

/*  *********************************************************************
    *  X86MEM_MEMCPY(mem,dest,src,cnt)
    *  
    *  memcpy data into the X86mem object
    *  
    *  Input parameters: 
    *  	   mem - x86mem object
    *  	   destaddr - destination x86mem address
    *  	   src - source local address
    *  	   cnt - number of bytes to copy
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void x86mem_memcpy(x86mem_t *mem,uint32_t destaddr,uint8_t *src,int count)
{
    while (count) {
	x86mem_writeb(mem,destaddr,*src);
	destaddr++;
	src++;
	count--;
	}
}


/*  *********************************************************************
    *  X86MEM_HOOK(mem,addr,readf,writef)
    *  
    *  Establish a hook for a block of simulated memory
    *  
    *  Input parameters: 
    *  	   mem - x86mem object
    *  	   addr - address in memory, should be aligned on a "chunk"
    *  	          boundary.
    *  	   readf - function to call on READ accesses
    *  	   writef - function to call on WRITE accesses
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void x86mem_hook(x86mem_t *mem,uint32_t chunkaddr,
		 uint32_t (*readf)(x86mem_t *mem,uint32_t addr,int size),
		 void (*writef)(x86mem_t *mem,uint32_t addr,uint32_t val,int size))
{
    if (mem->data[X86MEM_REGION(chunkaddr)]) {
	KFREE(mem->data[X86MEM_REGION(chunkaddr)]);
	mem->data[X86MEM_REGION(chunkaddr)] = NULL;	      
	}
    mem->read[X86MEM_REGION(chunkaddr)] = readf;
    mem->write[X86MEM_REGION(chunkaddr)] = writef;
}

/*  *********************************************************************
    *  X86MEM_HOOK_RANGE(mem,addr,size,readf,writef)
    *  
    *  Establish a hook for a block of simulated memory
    *  
    *  Input parameters: 
    *  	   mem - x86mem object
    *  	   addr - address in memory, should be aligned on a "chunk"
    *  	          boundary.
    *      size - size of region to hook.  Should be a multiple
    *             of the chunk size
    *  	   readf - function to call on READ accesses
    *  	   writef - function to call on WRITE accesses
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void x86mem_hook_range(x86mem_t *mem,uint32_t chunkaddr,int size,
		 uint32_t (*readf)(x86mem_t *mem,uint32_t addr,int size),
		 void (*writef)(x86mem_t *mem,uint32_t addr,uint32_t val,int size))
{
    while (size > 0) {
	x86mem_hook(mem,chunkaddr,readf,writef);
	size -= X86MEM_CHUNKSIZE;
	chunkaddr += X86MEM_CHUNKSIZE;
	}
}

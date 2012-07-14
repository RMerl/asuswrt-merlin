/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Memory manager definitions		File: cfe_mem.h
    *  
    *  Function prototypes and contants for the memory manager
    *  (used to manage the physical memory arena)
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

#define MEMTYPE_EMPTY	0
#define MEMTYPE_DRAM_AVAILABLE	1	/* must match value in cfe_iocb.h */
#define MEMTYPE_DRAM_NOTINSTALLED 2
#define MEMTYPE_DRAM_USEDBYFIRMWARE 3
#define MEMTYPE_BOOTROM	4
#define MEMTYPE_IOREGISTERS 5
#define MEMTYPE_RESERVED 6
#define MEMTYPE_L2CACHE 7
#define MEMTYPE_LDT_PCI	8
#define MEMTYPE_DRAM_BOOTPROGRAM 9

/*  *********************************************************************
    *  External data
    ********************************************************************* */

extern unsigned int mem_bootarea_start;
extern unsigned int mem_bootarea_size;

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

void cfe_arena_init(void);
int cfe_arena_loadcheck(uintptr_t start,unsigned int size);
int cfe_arena_enum(int idx,int *type,uint64_t *start,uint64_t *size,int allrecs);
void cfe_pagetable_init(void);

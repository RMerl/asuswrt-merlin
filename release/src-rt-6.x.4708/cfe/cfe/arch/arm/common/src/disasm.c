/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  MIPS disassembler		    	File: disasm.c
    *  
    *  ARM disassembler (used by ui_examcmds.c)
    *  
    *  Author:  
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
#include "disasm.h"
#include "lib_printf.h"




/* We're pulling some trickery here.  Most of the time, this structure operates
   exactly as one would expect.  The special case, when type == DC_DREF, 
   means name points to a byte that is an index into a dereferencing array.  */

/*
 * To make matters worse, the whole module has been coded to reduce the 
 * number of relocations present, so we don't actually store pointers
 * in the dereferencing array.  Instead, we store fixed-width strings
 * and use digits to represent indicies into the deref array.
 *
 * This is all to make more things fit in the relocatable version,
 * since every initialized pointer goes into our small data segment.
 */

typedef struct {
    char name[15];
    char type;
} disasm_t;

typedef struct {
	const disasm_t *ptr;
	int shift;
	uint32_t mask;
} disasm_deref_t;
		
/* Forward declaration of deref array, we need this for the disasm_t definitions */



static const disasm_t *get_disasm_field(uint32_t inst)
{
	return NULL;
}

char *disasm_inst_name(uint32_t inst)
{
	return (char *)(get_disasm_field(inst)->name);
}

void disasm_inst(char *buf, int buf_size, uint32_t inst, uint64_t pc)
{
	buf[buf_size-1] = 0;

}

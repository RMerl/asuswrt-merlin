/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Exception Handler			File: exchandler.c       
    *  
    *  This is the "C" part of the exception handler and the
    *  associated setup routines.  We call these routines from
    *  the assembly-language exception handler.
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
#include "lib_printf.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include <hndrte_armtrap.h>
#include "exception.h"
#include "cfe.h"
#include "cfe_error.h"
#include "cfe_iocb.h"
#include "exchandler.h"
#include "cpu_config.h"
#include "bsp_config.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

/*  *********************************************************************
    *  Globals 
    ********************************************************************* */

exc_handler_t exc_handler;
//extern void _exc_entry(void);
//extern void _exc_setup_locore(long);
extern void CPUCFG_TLBHANDLER(void);
extern void cfe_flushcache(uint32_t,long,long);
extern uint32_t _getstatus(void);
extern void _setstatus(uint32_t);


/*  *********************************************************************
    *  cfe_exception(code,info)
    *  
    *  Exception handler.  This routine is called when any CPU 
    *  exception that is handled by the assembly-language
    *  vectors is reached.  The usual thing to do here is just to
    *  reboot.
    *  
    *  Input parameters: 
    *  	   code - exception type
    *  	   info - exception stack frame
    *  	   
    *  Return value:
    *  	   usually reboots
    ********************************************************************* */
void cfe_exception(trap_t *tr)
{
	/*
	 * ARM7TDMI trap types:
	 *	0=RST, 1=UND, 2=SWI, 3=IAB, 4=DAB, 5=BAD, 6=IRQ, 7=FIQ
	 *
	 * ARM CM3 trap types:
	 *	1=RST, 2=NMI, 3=FAULT, 4=MM, 5=BUS, 6=USAGE, 11=SVC,
	 *	12=DMON, 14=PENDSV, 15=SYSTICK, 16+=ISR
 	 *
	 * ARM CA9 trap types:
	 *	0=RST, 1=UND, 2=SWI, 3=IAB, 4=DAB, 5=BAD, 6=IRQ, 7=FIQ
	 */

	uint32 *stack = (uint32*)tr->r13;
	char *tr_type_str[8] = {"RST", "UND", "SWI", "IAB", "DAB", "BAD", "IRQ", "FIQ"};
	char *type_str = "UKN";

	if (tr->type < 8)
		type_str = tr_type_str[tr->type];

	/* Note that UTF parses the first line, so the format should not be changed. */
	printf("\nTRAP [%s](x)[%x]: pc[%x], lr[%x], sp[%x], cpsr[%x], spsr[%x]\n",
	       type_str, tr->type, (uint32)tr, tr->pc, tr->r14, tr->r13, tr->cpsr, tr->spsr);
	printf("  r0[%x], r1[%x], r2[%x], r3[%x], r4[%x], r5[%x], r6[%x]\n",
	       tr->r0, tr->r1, tr->r2, tr->r3, tr->r4, tr->r5, tr->r6);
	printf("  r7[%x], r8[%x], r9[%x], r10[%x], r11[%x], r12[%x]\n",
	       tr->r7, tr->r8, tr->r9, tr->r10, tr->r11, tr->r12);

	/*
	 * stack content before trap occured
	 */
	printf("\n   sp+0 %08x %08x %08x %08x\n",
		stack[0], stack[1], stack[2], stack[3]);
	printf("  sp+10 %08x %08x %08x %08x\n\n",
		stack[4], stack[5], stack[6], stack[7]);

	xprintf("\n");
	_exc_restart();
}

/*  *********************************************************************
    *  cfe_setup_exceptions()
    *  
    *  Set up the exception handlers.  
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void cfe_setup_exceptions(void)
{
	/* Set trap handler */
	hndrte_set_trap((uint32)cfe_exception);
}


/*  *********************************************************************
    *  exc_initialize_block()
    *
    *  Set up the exception handler.  Allow exceptions to be caught. 
    *  Allocate memory for jmpbuf and store it away.
    *
    *  Returns NULL if error in memory allocation.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   jmpbuf_t structure, or NULL if no memory
    ********************************************************************* */
jmpbuf_t *exc_initialize_block(void)
{
    jmpbuf_t *jmpbuf_local;

    exc_handler.catch_exc = 1;
  
    /* Create the jmpbuf_t object */
    jmpbuf_local = (jmpbuf_t *) KMALLOC((sizeof(jmpbuf_t)),0);

    if (jmpbuf_local == NULL) {
	return NULL;
	}

    q_enqueue( &(exc_handler.jmpbuf_stack), &((*jmpbuf_local).stack));

    return jmpbuf_local;
}

/*  *********************************************************************
    *  exc_cleanup_block(dq_jmpbuf)
    *  
    *  Remove dq_jmpbuf from the exception handler stack and free
    *  the memory.
    *  
    *  Input parameters: 
    *  	   dq_jmpbuf - block to deallocate
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void exc_cleanup_block(jmpbuf_t *dq_jmpbuf)
{
    int count;

    if (dq_jmpbuf == NULL) {
	return;
	}
  
    count = q_count( &(exc_handler.jmpbuf_stack));

    if( count > 0 ) {
	q_dequeue( &(*dq_jmpbuf).stack );
	KFREE(dq_jmpbuf);
	}
}

/*  *********************************************************************
    *  exc_cleanup_handler(dq_jmpbuf,chain_exc)
    *  
    *  Clean a block, then chain to the next exception if required.
    *  
    *  Input parameters: 
    *  	   dq_jmpbuf - current exception
    *  	   chain_exc - true if we should chain to the next handler
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void exc_cleanup_handler(jmpbuf_t *dq_jmpbuf, int chain_exc)
{
    exc_cleanup_block(dq_jmpbuf);

    if( chain_exc == EXC_CHAIN_EXC ) {
	/*Go to next exception on stack */
	exc_longjmp_handler();
	}
}



/*  *********************************************************************
    *  exc_longjmp_handler()
    *  
    *  This routine long jumps to the exception handler on the top
    *  of the exception stack.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
void exc_longjmp_handler(void)
{
    int count;   
    jmpbuf_t *jmpbuf_local;

    count = q_count( &(exc_handler.jmpbuf_stack));

    if( count > 0 ) {
	jmpbuf_local = (jmpbuf_t *) q_getlast(&(exc_handler.jmpbuf_stack));

	SETLEDS("CFE ");

	lib_longjmp( (*jmpbuf_local).jmpbuf, -1);
	} 
}


/*  *********************************************************************
    *  mem_peek(d,addr,type)
    *  
    *  Read memory of the specified type at the specified address.
    *  Exceptions are caught in the case of a bad memory reference.
    *  
    *  Input parameters: 
    *  	   d - pointer to where data should be placed
    *  	   addr - address to read
    *  	   type - type of read to do (MEM_BYTE, etc.)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int mem_peek(void *d, long addr, int type)
{

    jmpbuf_t *jb;

    jb = exc_initialize_block();
    if( jb == NULL ) {
	return CFE_ERR_NOMEM;
	}

    if (exc_try(jb) == 0) {
  
	switch (type) {
	    case MEM_BYTE:
		*(uint8_t *)d = *((volatile uint8_t *) addr);
		break;
	    case MEM_HALFWORD:
		*(uint16_t *)d = *((volatile uint16_t *) addr);
		break;
	    case MEM_WORD:
		*(uint32_t *)d = *((volatile uint32_t *) addr);
		break;
	    case MEM_QUADWORD:
		*(uint64_t *)d = *((volatile uint64_t *) addr);
		break;
	    default:
		return CFE_ERR_INV_PARAM;
	    }

	exc_cleanup_block(jb);
	}
    else {
	/*Exception handler*/

	exc_cleanup_handler(jb, EXC_NORMAL_RETURN);
	return CFE_ERR_GETMEM;
	}

    return 0;
}

/* *********************************************************************
   *  Write memory of type at address addr with value val. 
   *  Exceptions are caught, handled (error message) and function 
   *  returns with 0. 
   *
   *  1 success
   *  0 failure
   ********************************************************************* */

int mem_poke(long addr, uint64_t val, int type)
{

    jmpbuf_t *jb;

    jb = exc_initialize_block();
    if( jb == NULL ) {
	return CFE_ERR_NOMEM;
	}

    if (exc_try(jb) == 0) {
  
	switch (type) {
	    case MEM_BYTE:
		*((volatile uint8_t *) addr) = (uint8_t) val;
		break;
	    case MEM_HALFWORD:
		*((volatile uint16_t *) addr) = (uint16_t) val;
		break;
	    case MEM_WORD:
		*((volatile uint32_t *) addr) = (uint32_t) val;
		break;
	    case MEM_QUADWORD:
		*((volatile uint64_t *) addr) = (uint64_t) val;
		break;
	    default:
		return CFE_ERR_INV_PARAM;
	    }

	exc_cleanup_block(jb);
	}
    else {
	/*Exception handler*/

	exc_cleanup_handler(jb, EXC_NORMAL_RETURN);
	return CFE_ERR_SETMEM;
	}

    return 0;
}

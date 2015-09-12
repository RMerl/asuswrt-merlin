/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  ARM Macros				File: armmacros.h
    *
    *  Macros to deal with various arm-related things.
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

/*  *********************************************************************
    *  32/64-bit macros
    ********************************************************************* */

#ifdef __long64
#define _VECT_	.dword
#define _LONG_	.dword
#define REGSIZE	8
#define BPWSIZE 3		/* bits per word size */
#define _TBLIDX(x) ((x)*REGSIZE)
#else
#define _VECT_	.word
#define _LONG_	.word
#define REGSIZE 4
#define BPWSIZE 2
#define _TBLIDX(x) ((x)*REGSIZE)
#endif

#ifndef FUNC
  #if defined(__thumb__)
    #define FUNC(x)	THUMBLEAF(x)
  #else
    #define FUNC(x)	LEAF(x)
  #endif	/* __thumb__ */
#endif

/* Debug macro */
#ifdef TRACE
#undef TRACE
#endif

#ifdef BCMDBG
#define TRACE(x) \
	ldr	r9,=(x); \
	ldr     r10,=SI_ENUM_BASE; \
	str	r9,[r10,#0x64]

#define TRACE1(x) \
	mov	r9,x; \
	ldr     r10,=SI_ENUM_BASE; \
	str	r9,[r10,#0x68]

#define TRACE2(x) \
	mov	r9,x; \
	ldr     r10,=SI_ENUM_BASE; \
	str	r9,[r10,#0x64]
#else
#define TRACE(x)
#define TRACE1(x)
#define TRACE2(x)
#endif


/*  *********************************************************************
    *  LOADREL(reg,label)
    *  
    *  Load the address of a label, but do it in a position-independent
    *  way.
    *  
    *  Input parameters: 
    *  	   reg - register to load
    *  	   label - label whose address to load
    *  	   
    *  Return value:
    *  	   ra is trashed!
    ********************************************************************* */

#if CFG_EMBEDDED_PIC
#define LOADREL(reg,label)			\
	.set noreorder ;			\
	bal  1f	       ;			\
	nop	       ;			\
1:	nop	       ;			\
	.set reorder   ;			\
	la   reg,label-1b ;			\
	addu reg,ra
#else
#define	LOADREL(reg,label)			\
	bl 1f;					\
1:	nop;					\
	ldr reg,=1b;				\
	sub lr,lr,reg;				\
	ldr reg,label;				\
	add reg,reg,lr;
#endif

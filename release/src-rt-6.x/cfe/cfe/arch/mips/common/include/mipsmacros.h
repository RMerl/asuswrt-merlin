/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  MIPS Macros				File: mipsmacros.h
    *
    *  Macros to deal with various mips-related things.
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
    *  32/64-bit macros
    ********************************************************************* */

#ifdef __long64
#define _VECT_	.dword
#define _LONG_	.dword
#define SR	sd
#define LR	ld
#define ADD     dadd
#define SUB     dsub
#define MFC0    dmfc0
#define MTC0    dmtc0
#define REGSIZE	8
#define BPWSIZE 3		/* bits per word size */
#define _TBLIDX(x) ((x)*REGSIZE)
#else
#define _VECT_	.word
#define _LONG_	.word
#define SR	sw
#define LR	lw
#define ADD     add
#define SUB     sub
#define MFC0    mfc0
#define MTC0    mtc0
#define REGSIZE 4
#define BPWSIZE 2
#define _TBLIDX(x) ((x)*REGSIZE)
#endif


/*  *********************************************************************
    *  NORMAL_VECTOR(addr,vecname,vecdest)
    *  NORMAL_XVECTOR(addr,vecname,vecdest,code)
    *  
    *  Declare a trap or dispatch vector. There are two flavors,
    *  DECLARE_XVECTOR sets up an indentifying code in k0 before
    *  jumping to the dispatch routine.
    *  
    *  Input parameters: 
    *  	   addr - vector address
    *  	   vecname - for label at that address
    *  	   vecdest - destination (place vector jumps to)
    *  	   code - code to place in k0 before jumping
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */


#define NORMAL_VECTOR(addr,vecname,vecdest) \
       .globl vecname    ;                   \
       .org   addr       ;                   \
vecname: b    vecdest    ;                   \
       nop;

#define NORMAL_XVECTOR(addr,vecname,vecdest,code) \
       .globl vecname    ;                   \
       .org   addr       ;                   \
vecname: b    vecdest    ;                   \
	 li   k0,code    ;		     \
       nop;


/*  *********************************************************************
    *  Evil macros for bi-endian support.
    *  
    *  The magic here is in the instruction encoded as 0x10000014.
    *  
    *  This instruction in big-endian is:   "b .+0x54"
    *  this instruction in little-endian is: "bne zero,zero,.+0x44"
    *  
    *  So, depending on what the system endianness is, it will either
    *  branch to .+0x54 or not branch at all. 
    *  
    *  the instructions that follow are:
    *  
    *     0x10000014        "magic branch"  (either-endian)
    *     0x00000000        nop  (bds)      (either-endian)
    *     0xD0BF1A3C        lui k0,0xBFD0   (little-endian)
    *     0xxxxx5A27        addu k0,vector  (little-endian)
    *     0x08004003        jr k0           (little-endian)
    *     0x00000000        nop  (bds)      (little-endian)
    *  ... space up to offset 0x54
    *     .........         b vecaddr       (big-endian)
    *  
    *  The idea is that the big-endian firmware is first, from 0..1MB
    *  in the flash, and the little-endian firmware is second,
    *  from 1..2MB in the flash.  The little-endian firmware is
    *  set to load at BFD00000, so that its initial routines will
    *  work until relocation is completed.
    *  
    *  the instructions at the vectors will either jump to the 
    *  big-endian or little-endian code based on system endianness.
    *
    *  The ROM is built by compiling CFE twice, first with 
    *  CFG_BIENDIAN=1 and CFG_LITTLE=0 (big-endian) and again
    *  with CFG_BIENDIAN=1 and CFG_LITTLE=1.  The resulting
    *  cfe.bin files are located at 0xBFC00000 and 0xBFD00000
    *  for big and little-endian versions, respectively.
    * 
    *  More information about how this works can be found in the 
    *  CFE Manual.
    ********************************************************************* */

#define __SWAPW(x) ((((x) & 0xFF) << 8) | (((x) & 0xFF00) >> 8))

#define BIENDIAN_VECTOR(addr,vecname,vecdest) \
       .globl vecname    ;                   \
       .org   addr       ;                   \
vecname: .word 0x10000014  ;		     \
       .word 0		 ;		     \
       .word ((__SWAPW(BIENDIAN_LE_BASE >> 16)) << 16) | 0x1A3C ; \
       .word (0x00005A27 | (((addr) & 0xFF) << 24) | (((addr) & 0xFF00) << 8)) ; \
       .word 0x08004003 ;		    \
       .word 0          ;                   \
       .org  ((addr) + 0x54) ;		    \
        b    vecdest    ;                   \
       nop;

#define BIENDIAN_XVECTOR(addr,vecname,vecdest,code) \
       .globl vecname    ;                   \
       .org   addr       ;                   \
vecname: .word 0x10000014  ;		     \
       .word 0		 ;		     \
       .word ((__SWAPW(BIENDIAN_LE_BASE >> 16)) << 16) | 0x1A3C ; \
       .word (0x00005A27 | (((addr) & 0xFF) << 24) | (((addr) & 0xFF00) << 8)) ; \
       .word 0x08004003  ;		    \
       .word 0          ;                   \
       .org  ((addr) + 0x54) ;		    \
       b    vecdest      ;                  \
         li   k0,code    ;		    \
       nop;



/*  *********************************************************************
    *  Declare the right versions of DECLARE_VECTOR and 
    *  DECLARE_XVECTOR depending on how we're building stuff.
    *  Generally, we only use the biendian version if we're building
    *  as CFG_BIENDIAN=1 and we're doing the big-endian MIPS version.
    ********************************************************************* */

#if CFG_BIENDIAN && defined(__MIPSEB)
#define DECLARE_VECTOR BIENDIAN_VECTOR
#define DECLARE_XVECTOR BIENDIAN_XVECTOR
#else
#define DECLARE_VECTOR NORMAL_VECTOR
#define DECLARE_XVECTOR NORMAL_XVECTOR
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
	.set noreorder	;			\
	bal	1f	;			\
	nop		;			\
1:	nop		;			\
	.set reorder	;			\
	la	reg,1b	;			\
	subu	ra,reg	;			\
	la	reg,label ;			\
	addu	reg,ra	;
#endif



/*  *********************************************************************
    *  JUMPREL(reg)
    *  
    *  Jump relative to the current PC.
    *  
    *  Input parameters: 
    *  	   reg - contains linked address to fix up
    *  	   
    *  Return value:
    *  	   ra is trashed!
    ********************************************************************* */

#if CFG_EMBEDDED_PIC
#define JUMPREL1(reg)				\
	or	reg,K1BASE ;			\
	jalr	reg
#define JUMPREL(reg)				\
	jalr	reg
#else
#define __JUMPREL(reg)				\
	.set noreorder	;			\
	bal	1f	;			\
	nop		;			\
1:	nop		;			\
	addu	ra,reg	;			\
	la	reg,1b	;			\
	subu	reg,ra,reg ;			\
	.set reorder
#define JUMPREL1(reg)				\
	__JUMPREL(reg) ;			\
	or	reg,K1BASE ;			\
	jalr	reg
#define JUMPREL(reg)				\
	__JUMPREL(reg) ;			\
	jalr	reg
#endif

/*  *********************************************************************
    *  CALLINIT_KSEG1(label,table,offset)
    *  CALLINIT_KSEG0(label,table,offset)
    *  
    *  Call an initialization routine (usually in another module).
    *  If initialization routine lives in KSEG1 it may need
    *  special fixing if using the cached version of CFE (this is
    *  the default case).  CFE is linked at a KSEG0 address.
    *  
    *  Embedded PIC is especially tricky, since the "la" 
    *  instruction expands to calculations involving GP.  
    *  In that case, use our table of offsets to
    *  load the routine address from a table in memory.
    *  
    *  Input parameters: 
    *  	   label - routine to call if we can call directly
    *  	   table - symbol name of table containing routine addresses
    *  	   offset - offset within the above table 
    *  	   
    *  Return value:
    *  	   k1,ra is trashed.
    ********************************************************************* */


#define CALLINIT_KSEG0(table,tableoffset)	\
	LOADREL(k1,table) ;		        \
	LR	k1,tableoffset(k1) ;		\
	JUMPREL(k1)
#if CFG_RUNFROMKSEG0
/* Cached PIC code - call indirect through table */
#define CALLINIT_KSEG1(table,tableoffset)	\
	LOADREL(k1,table) ;		        \
        or      k1,K1BASE ;			\
	LR	k1,tableoffset(k1) ;		\
	JUMPREL1(k1)
#else
/* Uncached PIC code - call indirect through table, always same KSEG */
#define CALLINIT_KSEG1 CALLINIT_KSEG0
#endif

/*
 * CALLINIT_RELOC is used once CFE's relocation is complete and
 * the "mem_textreloc" variable is set up.  (yes, this is nasty.)
 * If 'gp' is set, we can presume that we've relocated
 * and it's safe to read "mem_textreloc", otherwise use the
 * address as-is from the table.
 */

#if CFG_EMBEDDED_PIC
#define CALLINIT_RELOC(table,tableoffset)	\
	LOADREL(k1,table) ;		        \
	LR	k1,tableoffset(k1) ;		\
	beq	gp,zero,123f ;			\
	LR	k0,mem_textreloc ;		\
	ADD	k1,k1,k0 ;                      \
123:	jal	k1	
#else
#define CALLINIT_RELOC CALLINIT_KSEG0
#endif



/*  *********************************************************************
    *  SPIN_LOCK(lock,reg1,reg2)
    *  
    *  Acquire a spin lock.
    *  
    *  Input parameters: 
    *  	   lock - symbol (address) of lock to acquire
    *  	   reg1,reg2 - registers we can use to acquire lock
    *  	   
    *  Return value:
    *  	   nothing (lock acquired)
    ********************************************************************* */

#define SPIN_LOCK(lock,reg1,reg2)                 \
        la      reg1,lock ;                       \
1:	sync ;                                    \
        ll	reg2,0(reg1) ;			  \
	bne	reg2,zero,1b ;			  \
	li	reg2,1	     ;			  \
	sc	reg2,0(reg1) ;			  \
	beq	reg2,zero,1b ;			  \
	nop

/*  *********************************************************************
    *  SPIN_UNLOCK(lock,reg1)
    *  
    *  Release a spin lock.
    *  
    *  Input parameters: 
    *  	   lock - symbol (address) of lock to release
    *  	   reg1 - a register we can use
    *  	   
    *  Return value:
    *  	   nothing (lock released)
    ********************************************************************* */


#define SPIN_UNLOCK(lock,reg1)			 \
	la	reg1,lock ;			 \
	sw	zero,0(reg1)


/*  *********************************************************************
    *  SETCCAMODE(treg,mode)
    *  
    *  Set cacheability mode.  For some of the pass1 workarounds we
    *  do this alot, so here's a handy macro.
    *  
    *  Input parameters: 
    *  	   treg - temporary register we can use
    *  	   mode - new mode (K_CFG_K0COH_xxx)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

#define SETCCAMODE(treg,mode)                     \
		mfc0	treg,C0_CONFIG		; \
		srl	treg,treg,3		; \
		sll	treg,treg,3		; \
		or	treg,treg,mode          ; \
		mtc0	treg,C0_CONFIG		; \
		HAZARD


/*  *********************************************************************
    *  Declare variables
    ********************************************************************* */

#define DECLARE_LONG(x) \
                .global x ; \
x:              _LONG_  0




/*
 * end
 */

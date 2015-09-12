/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  Global constants and macros		File: sb1250_defs.h	
    *  
    *  This file contains macros and definitions used by the other
    *  include files.
    *
    *  SB1250 specification level:  User's manual 1/02/02
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

#ifndef _SB1250_DEFS_H
#define _SB1250_DEFS_H

/*
 * These headers require ANSI C89 string concatenation, and GCC or other
 * 'long long' (64-bit integer) support.
 */
#if !defined(__STDC__) && !defined(_MSC_VER)
#error SiByte headers require ANSI C89 support
#endif


/*  *********************************************************************
    *  Macros for feature tests, used to enable include file features
    *  for chip features only present in certain chip revisions.
    *
    *  SIBYTE_HDR_FEATURES may be defined to be the mask value chip/revision
    *  which is to be exposed by the headers.  If undefined, it defaults to
    *  "all features."
    *
    *  Use like:
    *
    *    #define SIBYTE_HDR_FEATURES	SIBYTE_HDR_FMASK_112x_PASS1
    *
    *		Generate defines only for that revision of chip.
    *
    *    #if SIBYTE_HDR_FEATURE(chip,pass)
    *
    *		True if header features for that revision or later of
    *	        that particular chip type are enabled in SIBYTE_HDR_FEATURES.
    *	        (Use this to bracket #defines for features present in a given
    *		revision and later.)
    *
    *		Note that there is no implied ordering between chip types.
    *
    *		Note also that 'chip' and 'pass' must textually exactly
    *		match the defines below.  So, for example,
    *		SIBYTE_HDR_FEATURE(112x, PASS1) is OK, but
    *		SIBYTE_HDR_FEATURE(1120, pass1) is not (for two reasons).
    *
    *    #if SIBYTE_HDR_FEATURE_UP_TO(chip,pass)
    *
    *		Same as SIBYTE_HDR_FEATURE, but true for the named revision
    *		and earlier revisions of the named chip type.
    *
    *    #if SIBYTE_HDR_FEATURE_EXACT(chip,pass)
    *
    *		Same as SIBYTE_HDR_FEATURE, but only true for the named
    *		revision of the named chip type.  (Note that this CANNOT
    *		be used to verify that you're compiling only for that
    *		particular chip/revision.  It will be true any time this
    *		chip/revision is included in SIBYTE_HDR_FEATURES.)
    *
    *    #if SIBYTE_HDR_FEATURE_CHIP(chip)
    *
    *		True if header features for (any revision of) that chip type
    *		are enabled in SIBYTE_HDR_FEATURES.  (Use this to bracket
    *		#defines for features specific to a given chip type.)
    *
    *  Mask values currently include room for additional revisions of each
    *  chip type, but can be renumbered at will.  Note that they MUST fit
    *  into 31 bits and may not include C type constructs, for safe use in
    *  CPP conditionals.  Bit positions within chip types DO indicate
    *  ordering, so be careful when adding support for new minor revs.
    ********************************************************************* */

#define	SIBYTE_HDR_FMASK_1250_ALL		0x00000ff
#define	SIBYTE_HDR_FMASK_1250_PASS1		0x0000001
#define	SIBYTE_HDR_FMASK_1250_PASS2		0x0000002

#define	SIBYTE_HDR_FMASK_112x_ALL		0x0000f00
#define	SIBYTE_HDR_FMASK_112x_PASS1		0x0000100
#define SIBYTE_HDR_FMASK_112x_PASS3		0x0000200

/* Bit mask for chip/revision.  (use _ALL for all revisions of a chip).  */ 
#define	SIBYTE_HDR_FMASK(chip, pass)					\
    (SIBYTE_HDR_FMASK_ ## chip ## _ ## pass)
#define	SIBYTE_HDR_FMASK_ALLREVS(chip)					\
    (SIBYTE_HDR_FMASK_ ## chip ## _ALL)

#define	SIBYTE_HDR_FMASK_ALL						\
    (SIBYTE_HDR_FMASK_1250_ALL | SIBYTE_HDR_FMASK_112x_ALL)

#ifndef SIBYTE_HDR_FEATURES
#define	SIBYTE_HDR_FEATURES			SIBYTE_HDR_FMASK_ALL
#endif


/* Bit mask for revisions of chip exclusively before the named revision.  */
#define	SIBYTE_HDR_FMASK_BEFORE(chip, pass)				\
    ((SIBYTE_HDR_FMASK(chip, pass) - 1) & SIBYTE_HDR_FMASK_ALLREVS(chip))

/* Bit mask for revisions of chip exclusively after the named revision.  */
#define	SIBYTE_HDR_FMASK_AFTER(chip, pass)				\
    (~(SIBYTE_HDR_FMASK(chip, pass)					\
     | (SIBYTE_HDR_FMASK(chip, pass) - 1)) & SIBYTE_HDR_FMASK_ALLREVS(chip))


/* True if header features enabled for (any revision of) that chip type.  */
#define SIBYTE_HDR_FEATURE_CHIP(chip)					\
    (!! (SIBYTE_HDR_FMASK_ALLREVS(chip) & SIBYTE_HDR_FEATURES))

/* True if header features enabled for that rev or later, inclusive.  */
#define SIBYTE_HDR_FEATURE(chip, pass)					\
    (!! ((SIBYTE_HDR_FMASK(chip, pass)					\
	  | SIBYTE_HDR_FMASK_AFTER(chip, pass)) & SIBYTE_HDR_FEATURES))

/* True if header features enabled for exactly that rev.  */
#define SIBYTE_HDR_FEATURE_EXACT(chip, pass)				\
    (!! (SIBYTE_HDR_FMASK(chip, pass) & SIBYTE_HDR_FEATURES))

/* True if header features enabled for that rev or before, inclusive.  */
#define SIBYTE_HDR_FEATURE_UP_TO(chip, pass)				\
    (!! ((SIBYTE_HDR_FMASK(chip, pass)					\
	 | SIBYTE_HDR_FMASK_BEFORE(chip, pass)) & SIBYTE_HDR_FEATURES))


/*  *********************************************************************
    *  Naming schemes for constants in these files:
    *  
    *  M_xxx           MASK constant (identifies bits in a register). 
    *                  For multi-bit fields, all bits in the field will
    *                  be set.
    *
    *  K_xxx           "Code" constant (value for data in a multi-bit
    *                  field).  The value is right justified.
    *
    *  V_xxx           "Value" constant.  This is the same as the 
    *                  corresponding "K_xxx" constant, except it is
    *                  shifted to the correct position in the register.
    *
    *  S_xxx           SHIFT constant.  This is the number of bits that
    *                  a field value (code) needs to be shifted 
    *                  (towards the left) to put the value in the right
    *                  position for the register.
    *
    *  A_xxx           ADDRESS constant.  This will be a physical 
    *                  address.  Use the PHYS_TO_K1 macro to generate
    *                  a K1SEG address.
    *
    *  R_xxx           RELATIVE offset constant.  This is an offset from
    *                  an A_xxx constant (usually the first register in
    *                  a group).
    *  
    *  G_xxx(X)        GET value.  This macro obtains a multi-bit field
    *                  from a register, masks it, and shifts it to
    *                  the bottom of the register (retrieving a K_xxx
    *                  value, for example).
    *
    *  V_xxx(X)        VALUE.  This macro computes the value of a
    *                  K_xxx constant shifted to the correct position
    *                  in the register.
    ********************************************************************* */




/*
 * Cast to 64-bit number.  Presumably the syntax is different in 
 * assembly language.
 *
 * Note: you'll need to define uint32_t and uint64_t in your headers.
 */

#if !defined(__ASSEMBLER__)
#define _SB_MAKE64(x) ((uint64_t)(x))
#define _SB_MAKE32(x) ((uint32_t)(x))
#else
#define _SB_MAKE64(x) (x)
#define _SB_MAKE32(x) (x)
#endif


/*
 * Make a mask for 1 bit at position 'n'
 */

#define _SB_MAKEMASK1(n) (_SB_MAKE64(1) << _SB_MAKE64(n))
#define _SB_MAKEMASK1_32(n) (_SB_MAKE32(1) << _SB_MAKE32(n))

/*
 * Make a mask for 'v' bits at position 'n'
 */

#define _SB_MAKEMASK(v,n) (_SB_MAKE64((_SB_MAKE64(1)<<(v))-1) << _SB_MAKE64(n))
#define _SB_MAKEMASK_32(v,n) (_SB_MAKE32((_SB_MAKE32(1)<<(v))-1) << _SB_MAKE32(n))

/*
 * Make a value at 'v' at bit position 'n'
 */

#define _SB_MAKEVALUE(v,n) (_SB_MAKE64(v) << _SB_MAKE64(n))
#define _SB_MAKEVALUE_32(v,n) (_SB_MAKE32(v) << _SB_MAKE32(n))

#define _SB_GETVALUE(v,n,m) ((_SB_MAKE64(v) & _SB_MAKE64(m)) >> _SB_MAKE64(n))
#define _SB_GETVALUE_32(v,n,m) ((_SB_MAKE32(v) & _SB_MAKE32(m)) >> _SB_MAKE32(n))



#if defined(__mips64) && !defined(__ASSEMBLER__)
#define SBWRITECSR(csr,val) *((volatile uint64_t *) PHYS_TO_K1(csr)) = (val)
#define SBREADCSR(csr) (*((volatile uint64_t *) PHYS_TO_K1(csr)))
#endif /* __ASSEMBLER__ */

#endif

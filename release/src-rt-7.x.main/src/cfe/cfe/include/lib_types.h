/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Basic types				File: lib_types.h
    *  
    *  This module defines the basic types used in CFE.  Simple
    *  types, such as uint64_t, are defined here.
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


#ifdef __mips
#if ((defined(__MIPSEB)+defined(__MIPSEL)) != 1)
#error "Either __MIPSEB or __MIPSEL must be defined!"
#endif
#endif


#ifndef _LIB_TYPES_H
#define _LIB_TYPES_H

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/*  *********************************************************************
    *  Basic types
    ********************************************************************* */

#ifdef __GNUC__
#define __need_size_t
#include <stddef.h>
#else
typedef int size_t;
#endif

typedef char int8_t;
typedef unsigned char uint8_t;

typedef short int16_t;
typedef unsigned short uint16_t;

#ifdef __long64
typedef int int32_t;
typedef unsigned int uint32_t;
#else
typedef long int32_t;
typedef unsigned long uint32_t;
#endif

typedef long long int64_t;
typedef unsigned long long uint64_t;

#define unsigned signed		/* Kludge to get unsigned size-shaped type. */
typedef __SIZE_TYPE__ intptr_t;
#undef unsigned
typedef __SIZE_TYPE__ uintptr_t;

#ifdef	__ARM_ARCH_7A__
typedef unsigned long long	fl_size_t;
typedef unsigned long long	fl_offset_t;
#define FL_FMT	"ll"
#else
typedef unsigned int	fl_size_t;
typedef unsigned int	fl_offset_t;
#define FL_FMT	""
#endif /* __ARM_ARCH_7A__ */

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#ifndef offsetof
#define offsetof(type,memb) ((size_t)&((type *)0)->memb)
#endif

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

typedef struct cons_s {
    char *str;
    int num;
} cons_t;

#endif

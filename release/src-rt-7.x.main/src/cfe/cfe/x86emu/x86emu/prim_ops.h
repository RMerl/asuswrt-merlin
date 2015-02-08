/****************************************************************************
*
*						Realmode X86 Emulator Library
*
*            	Copyright (C) 1996-1999 SciTech Software, Inc.
* 				     Copyright (C) David Mosberger-Tang
* 					   Copyright (C) 1999 Egbert Eich
*
*  ========================================================================
*
*  Permission to use, copy, modify, distribute, and sell this software and
*  its documentation for any purpose is hereby granted without fee,
*  provided that the above copyright notice appear in all copies and that
*  both that copyright notice and this permission notice appear in
*  supporting documentation, and that the name of the authors not be used
*  in advertising or publicity pertaining to distribution of the software
*  without specific, written prior permission.  The authors makes no
*  representations about the suitability of this software for any purpose.
*  It is provided "as is" without express or implied warranty.
*
*  THE AUTHORS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
*  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
*  EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
*  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
*  USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
*  OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
*  PERFORMANCE OF THIS SOFTWARE.
*
*  ========================================================================
*
* Language:		ANSI C
* Environment:	Any
* Developer:    Kendall Bennett
*
* Description:  Header file for primitive operation functions.
*
****************************************************************************/

#ifndef __X86EMU_PRIM_OPS_H
#define __X86EMU_PRIM_OPS_H

#include "x86emu/prim_asm.h"

#ifdef  __cplusplus
extern "C" {            			/* Use "C" linkage when in C++ mode */
#endif

u16     aaa_word (u16 d);
u16     aas_word (u16 d);
u16     aad_word (u16 d);
u16     aam_word (u8 d);
u8      adc_byte (u8 d, u8 s);
u16     adc_word (u16 d, u16 s);
u32     adc_long (u32 d, u32 s);
u8      add_byte (u8 d, u8 s);
u16     add_word (u16 d, u16 s);
u32     add_long (u32 d, u32 s);
u8      and_byte (u8 d, u8 s);
u16     and_word (u16 d, u16 s);
u32     and_long (u32 d, u32 s);
u8      cmp_byte (u8 d, u8 s);
u16     cmp_word (u16 d, u16 s);
u32     cmp_long (u32 d, u32 s);
u8      daa_byte (u8 d);
u8      das_byte (u8 d);
u8      dec_byte (u8 d);
u16     dec_word (u16 d);
u32     dec_long (u32 d);
u8      inc_byte (u8 d);
u16     inc_word (u16 d);
u32     inc_long (u32 d);
u8      or_byte (u8 d, u8 s);
u16     or_word (u16 d, u16 s);
u32     or_long (u32 d, u32 s);
u8      neg_byte (u8 s);
u16     neg_word (u16 s);
u32     neg_long (u32 s);
u8      not_byte (u8 s);
u16     not_word (u16 s);
u32     not_long (u32 s);
u8      rcl_byte (u8 d, u8 s);
u16     rcl_word (u16 d, u8 s);
u32     rcl_long (u32 d, u8 s);
u8      rcr_byte (u8 d, u8 s);
u16     rcr_word (u16 d, u8 s);
u32     rcr_long (u32 d, u8 s);
u8      rol_byte (u8 d, u8 s);
u16     rol_word (u16 d, u8 s);
u32     rol_long (u32 d, u8 s);
u8      ror_byte (u8 d, u8 s);
u16     ror_word (u16 d, u8 s);
u32     ror_long (u32 d, u8 s);
u8      shl_byte (u8 d, u8 s);
u16     shl_word (u16 d, u8 s);
u32     shl_long (u32 d, u8 s);
u8      shr_byte (u8 d, u8 s);
u16     shr_word (u16 d, u8 s);
u32     shr_long (u32 d, u8 s);
u8      sar_byte (u8 d, u8 s);
u16     sar_word (u16 d, u8 s);
u32     sar_long (u32 d, u8 s);
u16     shld_word (u16 d, u16 fill, u8 s);
u32     shld_long (u32 d, u32 fill, u8 s);
u16     shrd_word (u16 d, u16 fill, u8 s);
u32     shrd_long (u32 d, u32 fill, u8 s);
u8      sbb_byte (u8 d, u8 s);
u16     sbb_word (u16 d, u16 s);
u32     sbb_long (u32 d, u32 s);
u8      sub_byte (u8 d, u8 s);
u16     sub_word (u16 d, u16 s);
u32     sub_long (u32 d, u32 s);
void    test_byte (u8 d, u8 s);
void    test_word (u16 d, u16 s);
void    test_long (u32 d, u32 s);
u8      xor_byte (u8 d, u8 s);
u16     xor_word (u16 d, u16 s);
u32     xor_long (u32 d, u32 s);
void    imul_byte (u8 s);
void    imul_word (u16 s);
void    imul_long (u32 s);
void 	imul_long_direct(u32 *res_lo, u32* res_hi,u32 d, u32 s);
void    mul_byte (u8 s);
void    mul_word (u16 s);
void    mul_long (u32 s);
void    idiv_byte (u8 s);
void    idiv_word (u16 s);
void    idiv_long (u32 s);
void    div_byte (u8 s);
void    div_word (u16 s);
void    div_long (u32 s);
void    ins (int size);
void    outs (int size);
u16     mem_access_word (int addr);
void    push_word (u16 w);
void    push_long (u32 w);
u16     pop_word (void);
u32		pop_long (void);

#if  defined(__HAVE_INLINE_ASSEMBLER__) && !defined(PRIM_OPS_NO_REDEFINE_ASM)

#define	aaa_word(d)		aaa_word_asm(&M.x86.R_EFLG,d)
#define aas_word(d)		aas_word_asm(&M.x86.R_EFLG,d)
#define aad_word(d)		aad_word_asm(&M.x86.R_EFLG,d)
#define aam_word(d)		aam_word_asm(&M.x86.R_EFLG,d)
#define adc_byte(d,s)	adc_byte_asm(&M.x86.R_EFLG,d,s)
#define adc_word(d,s)	adc_word_asm(&M.x86.R_EFLG,d,s)
#define adc_long(d,s)	adc_long_asm(&M.x86.R_EFLG,d,s)
#define add_byte(d,s) 	add_byte_asm(&M.x86.R_EFLG,d,s)
#define add_word(d,s)	add_word_asm(&M.x86.R_EFLG,d,s)
#define add_long(d,s)	add_long_asm(&M.x86.R_EFLG,d,s)
#define and_byte(d,s)	and_byte_asm(&M.x86.R_EFLG,d,s)
#define and_word(d,s)	and_word_asm(&M.x86.R_EFLG,d,s)
#define and_long(d,s)	and_long_asm(&M.x86.R_EFLG,d,s)
#define cmp_byte(d,s)	cmp_byte_asm(&M.x86.R_EFLG,d,s)
#define cmp_word(d,s)	cmp_word_asm(&M.x86.R_EFLG,d,s)
#define cmp_long(d,s)	cmp_long_asm(&M.x86.R_EFLG,d,s)
#define daa_byte(d)		daa_byte_asm(&M.x86.R_EFLG,d)
#define das_byte(d)		das_byte_asm(&M.x86.R_EFLG,d)
#define dec_byte(d)		dec_byte_asm(&M.x86.R_EFLG,d)
#define dec_word(d)		dec_word_asm(&M.x86.R_EFLG,d)
#define dec_long(d)		dec_long_asm(&M.x86.R_EFLG,d)
#define inc_byte(d)		inc_byte_asm(&M.x86.R_EFLG,d)
#define inc_word(d)		inc_word_asm(&M.x86.R_EFLG,d)
#define inc_long(d)		inc_long_asm(&M.x86.R_EFLG,d)
#define or_byte(d,s)	or_byte_asm(&M.x86.R_EFLG,d,s)
#define or_word(d,s)	or_word_asm(&M.x86.R_EFLG,d,s)
#define or_long(d,s)	or_long_asm(&M.x86.R_EFLG,d,s)
#define neg_byte(s)		neg_byte_asm(&M.x86.R_EFLG,s)
#define neg_word(s)		neg_word_asm(&M.x86.R_EFLG,s)
#define neg_long(s)		neg_long_asm(&M.x86.R_EFLG,s)
#define not_byte(s)		not_byte_asm(&M.x86.R_EFLG,s)
#define not_word(s)		not_word_asm(&M.x86.R_EFLG,s)
#define not_long(s)		not_long_asm(&M.x86.R_EFLG,s)
#define rcl_byte(d,s)	rcl_byte_asm(&M.x86.R_EFLG,d,s)
#define rcl_word(d,s)	rcl_word_asm(&M.x86.R_EFLG,d,s)
#define rcl_long(d,s)	rcl_long_asm(&M.x86.R_EFLG,d,s)
#define rcr_byte(d,s)	rcr_byte_asm(&M.x86.R_EFLG,d,s)
#define rcr_word(d,s)	rcr_word_asm(&M.x86.R_EFLG,d,s)
#define rcr_long(d,s)	rcr_long_asm(&M.x86.R_EFLG,d,s)
#define rol_byte(d,s)	rol_byte_asm(&M.x86.R_EFLG,d,s)
#define rol_word(d,s)	rol_word_asm(&M.x86.R_EFLG,d,s)
#define rol_long(d,s)	rol_long_asm(&M.x86.R_EFLG,d,s)
#define ror_byte(d,s)	ror_byte_asm(&M.x86.R_EFLG,d,s)
#define ror_word(d,s)	ror_word_asm(&M.x86.R_EFLG,d,s)
#define ror_long(d,s)	ror_long_asm(&M.x86.R_EFLG,d,s)
#define shl_byte(d,s)	shl_byte_asm(&M.x86.R_EFLG,d,s)
#define shl_word(d,s)	shl_word_asm(&M.x86.R_EFLG,d,s)
#define shl_long(d,s)	shl_long_asm(&M.x86.R_EFLG,d,s)
#define shr_byte(d,s)	shr_byte_asm(&M.x86.R_EFLG,d,s)
#define shr_word(d,s)	shr_word_asm(&M.x86.R_EFLG,d,s)
#define shr_long(d,s)	shr_long_asm(&M.x86.R_EFLG,d,s)
#define sar_byte(d,s)	sar_byte_asm(&M.x86.R_EFLG,d,s)
#define sar_word(d,s)	sar_word_asm(&M.x86.R_EFLG,d,s)
#define sar_long(d,s)	sar_long_asm(&M.x86.R_EFLG,d,s)
#define shld_word(d,fill,s)	shld_word_asm(&M.x86.R_EFLG,d,fill,s)
#define shld_long(d,fill,s)	shld_long_asm(&M.x86.R_EFLG,d,fill,s)
#define shrd_word(d,fill,s)	shrd_word_asm(&M.x86.R_EFLG,d,fill,s)
#define shrd_long(d,fill,s)	shrd_long_asm(&M.x86.R_EFLG,d,fill,s)
#define sbb_byte(d,s)	sbb_byte_asm(&M.x86.R_EFLG,d,s)
#define sbb_word(d,s)	sbb_word_asm(&M.x86.R_EFLG,d,s)
#define sbb_long(d,s)	sbb_long_asm(&M.x86.R_EFLG,d,s)
#define sub_byte(d,s)	sub_byte_asm(&M.x86.R_EFLG,d,s)
#define sub_word(d,s)	sub_word_asm(&M.x86.R_EFLG,d,s)
#define sub_long(d,s)	sub_long_asm(&M.x86.R_EFLG,d,s)
#define test_byte(d,s)	test_byte_asm(&M.x86.R_EFLG,d,s)
#define test_word(d,s)	test_word_asm(&M.x86.R_EFLG,d,s)
#define test_long(d,s)	test_long_asm(&M.x86.R_EFLG,d,s)
#define xor_byte(d,s)	xor_byte_asm(&M.x86.R_EFLG,d,s)
#define xor_word(d,s)	xor_word_asm(&M.x86.R_EFLG,d,s)
#define xor_long(d,s)	xor_long_asm(&M.x86.R_EFLG,d,s)
#define imul_byte(s)	imul_byte_asm(&M.x86.R_EFLG,&M.x86.R_AX,M.x86.R_AL,s)
#define imul_word(s)	imul_word_asm(&M.x86.R_EFLG,&M.x86.R_AX,&M.x86.R_DX,M.x86.R_AX,s)
#define imul_long(s)	imul_long_asm(&M.x86.R_EFLG,&M.x86.R_EAX,&M.x86.R_EDX,M.x86.R_EAX,s)
#define imul_long_direct(res_lo,res_hi,d,s)	imul_long_asm(&M.x86.R_EFLG,res_lo,res_hi,d,s)
#define mul_byte(s)		mul_byte_asm(&M.x86.R_EFLG,&M.x86.R_AX,M.x86.R_AL,s)
#define mul_word(s)		mul_word_asm(&M.x86.R_EFLG,&M.x86.R_AX,&M.x86.R_DX,M.x86.R_AX,s)
#define mul_long(s)		mul_long_asm(&M.x86.R_EFLG,&M.x86.R_EAX,&M.x86.R_EDX,M.x86.R_EAX,s)
#define idiv_byte(s)	idiv_byte_asm(&M.x86.R_EFLG,&M.x86.R_AL,&M.x86.R_AH,M.x86.R_AX,s)
#define idiv_word(s)	idiv_word_asm(&M.x86.R_EFLG,&M.x86.R_AX,&M.x86.R_DX,M.x86.R_AX,M.x86.R_DX,s)
#define idiv_long(s)	idiv_long_asm(&M.x86.R_EFLG,&M.x86.R_EAX,&M.x86.R_EDX,M.x86.R_EAX,M.x86.R_EDX,s)
#define div_byte(s)		div_byte_asm(&M.x86.R_EFLG,&M.x86.R_AL,&M.x86.R_AH,M.x86.R_AX,s)
#define div_word(s)		div_word_asm(&M.x86.R_EFLG,&M.x86.R_AX,&M.x86.R_DX,M.x86.R_AX,M.x86.R_DX,s)
#define div_long(s)		div_long_asm(&M.x86.R_EFLG,&M.x86.R_EAX,&M.x86.R_EDX,M.x86.R_EAX,M.x86.R_EDX,s)

#endif

#ifdef  __cplusplus
}                       			/* End of "C" linkage for C++   	*/
#endif

#endif /* __X86EMU_PRIM_OPS_H */

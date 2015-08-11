#ifndef __BASIC_OP_H__
#define __BASIC_OP_H__

#include "config.h"

/*___________________________________________________________________________
 |                                                                           |
 |   Constants and Globals                                                   |
 |___________________________________________________________________________|
*/

#define MAX_32 (Word32)0x7fffffffL
#define MIN_32 (Word32)0x80000000L

#define MAX_16 (Word16)0x7fff
#define MIN_16 (Word16)(pj_uint16_t)0x8000

#define UMAX_32 (UWord32)0xffffffffL
#define UMIN_32 (UWord32)0x00000000L

/*___________________________________________________________________________
 |                                                                           |
 |   Prototypes for basic arithmetic operators                               |
 |___________________________________________________________________________|
*/

PJ_INLINE(Word16) add (Word16 var1, Word16 var2);    /* Short add,           1   */
PJ_INLINE(Word16) sub (Word16 var1, Word16 var2);    /* Short sub,           1   */
PJ_INLINE(Word16) abs_s (Word16 var1);               /* Short abs,           1   */
LIBG7221_DECL(Word16) shl (Word16 var1, Word16 var2);    /* Short shift left,    1   */
PJ_INLINE(Word16) shl_nocheck(Word16 var1, Word16 var2);
LIBG7221_DECL(Word16) shr (Word16 var1, Word16 var2);    /* Short shift right,   1   */
PJ_INLINE(Word16) shr_nocheck(Word16 var1, Word16 var2);
LIBG7221_DECL(Word16) mult (Word16 var1, Word16 var2);   /* Short mult,          1   */
PJ_INLINE(Word32) L_mult (Word16 var1, Word16 var2); /* Long mult,           1   */
PJ_INLINE(Word16) negate (Word16 var1);              /* Short negate,        1   */
PJ_INLINE(Word16) extract_h (Word32 L_var1);         /* Extract high,        1   */
PJ_INLINE(Word16) extract_l (Word32 L_var1);         /* Extract low,         1   */
PJ_INLINE(Word16) itu_round (Word32 L_var1);         /* Round,               1   */
PJ_INLINE(Word32) L_mac (Word32 L_var3, Word16 var1, Word16 var2);   /* Mac,  1  */
LIBG7221_DECL(Word32) L_msu (Word32 L_var3, Word16 var1, Word16 var2);   /* Msu,  1  */
LIBG7221_DECL(Word32) L_macNs (Word32 L_var3, Word16 var1, Word16 var2); /* Mac without
								       sat, 1   */
LIBG7221_DECL(Word32) L_msuNs (Word32 L_var3, Word16 var1, Word16 var2); /* Msu without
								       sat, 1   */
//PJ_INLINE(Word32) L_add (Word32 L_var1, Word32 L_var2);    /* Long add,        2 */
PJ_INLINE(Word32) L_sub (Word32 L_var1, Word32 L_var2);    /* Long sub,        2 */
LIBG7221_DECL(Word32) L_add_c (Word32 L_var1, Word32 L_var2);  /* Long add with c, 2 */
LIBG7221_DECL(Word32) L_sub_c (Word32 L_var1, Word32 L_var2);  /* Long sub with c, 2 */
LIBG7221_DECL(Word32) L_negate (Word32 L_var1);                /* Long negate,     2 */
LIBG7221_DECL(Word16) mult_r (Word16 var1, Word16 var2);       /* Mult with round, 2 */
PJ_INLINE(Word32) L_shl (Word32 L_var1, Word16 var2);      /* Long shift left, 2 */
PJ_INLINE(Word32) L_shr (Word32 L_var1, Word16 var2);      /* Long shift right, 2*/
LIBG7221_DECL(Word16) shr_r (Word16 var1, Word16 var2);        /* Shift right with
							     round, 2           */
LIBG7221_DECL(Word16) mac_r (Word32 L_var3, Word16 var1, Word16 var2); /* Mac with
                                                           rounding,2 */
LIBG7221_DECL(Word16) msu_r (Word32 L_var3, Word16 var1, Word16 var2); /* Msu with
                                                           rounding,2 */
LIBG7221_DECL(Word32) L_deposit_h (Word16 var1);        /* 16 bit var1 -> MSB,     2 */
LIBG7221_DECL(Word32) L_deposit_l (Word16 var1);        /* 16 bit var1 -> LSB,     2 */

LIBG7221_DECL(Word32) L_shr_r (Word32 L_var1, Word16 var2); /* Long shift right with
							  round,  3             */
LIBG7221_DECL(Word32) L_abs (Word32 L_var1);            /* Long abs,              3  */
LIBG7221_DECL(Word32) L_sat (Word32 L_var1);            /* Long saturation,       4  */
LIBG7221_DECL(Word16) norm_s (Word16 var1);             /* Short norm,           15  */
LIBG7221_DECL(Word16) div_s (Word16 var1, Word16 var2); /* Short division,       18  */
LIBG7221_DECL(Word16) norm_l (Word32 L_var1);           /* Long norm,            30  */   

/*
   Additional G.723.1 operators
*/
LIBG7221_DECL(Word32) L_mls( Word32, Word16 ) ;    /* Weight FFS; currently assigned 1 */
LIBG7221_DECL(Word16) div_l( Word32, Word16 ) ;    /* Weight FFS; currently assigned 1 */
LIBG7221_DECL(Word16) i_mult(Word16 a, Word16 b);  /* Weight FFS; currently assigned 1 */

/* 
    New shiftless operators, not used in G.729/G.723.1
*/
LIBG7221_DECL(Word32) L_mult0(Word16 v1, Word16 v2); /* 32-bit Multiply w/o shift         1 */
LIBG7221_DECL(Word32) L_mac0(Word32 L_v3, Word16 v1, Word16 v2); /* 32-bit Mac w/o shift  1 */
LIBG7221_DECL(Word32) L_msu0(Word32 L_v3, Word16 v1, Word16 v2); /* 32-bit Msu w/o shift  1 */

/* 
    Additional G.722.1 operators
*/
LIBG7221_DECL(UWord32) LU_shl (UWord32 L_var1, Word16 var2);
LIBG7221_DECL(UWord32) LU_shr (UWord32 L_var1, Word16 var2);

#define INCLUDE_UNSAFE	    0

/* Local */
PJ_INLINE(Word16) saturate (Word32 L_var1);

#if INCLUDE_UNSAFE
    extern Flag g7221_Overflow;
    extern Flag g7221_Carry;
#   define SET_OVERFLOW(n)  g7221_Overflow = n
#   define SET_CARRY(n)	    g7221_Carry = n

#else
#   define SET_OVERFLOW(n)
#   define SET_CARRY(n)
#   define GET_OVERFLOW()   0
#   define GET_CARRY()	    0
#endif

#include "basic_op_i.h"

#if PJMEDIA_LIBG7221_FUNCS_INLINED
#   include "basic_op.c"
#endif

#endif /* __BASIC_OP_H__ */



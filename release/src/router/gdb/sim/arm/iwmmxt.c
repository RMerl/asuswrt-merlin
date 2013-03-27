/*  iwmmxt.c -- Intel(r) Wireless MMX(tm) technology co-processor interface.
    Copyright (C) 2002, 2007 Free Software Foundation, Inc.
    Contributed by matthew green (mrg@redhat.com).
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include "armdefs.h"
#include "armos.h"
#include "armemu.h"
#include "ansidecl.h"
#include "iwmmxt.h"

/* #define DEBUG 1 */

/* Intel(r) Wireless MMX(tm) technology co-processor.  
   It uses co-processor numbers (0 and 1).  There are 16 vector registers wRx
   and 16 control registers wCx.  Co-processors 0 and 1 are used in MCR/MRC
   to access wRx and wCx respectively.  */

static ARMdword wR[16];
static ARMword  wC[16] = { 0x69051010 };

#define SUBSTR(w,t,m,n) ((t)(w <<  ((sizeof (t) * 8 - 1) - (n))) \
                               >> (((sizeof (t) * 8 - 1) - (n)) + (m)))
#define wCBITS(w,x,y)   SUBSTR (wC[w], ARMword, x, y)
#define wRBITS(w,x,y)   SUBSTR (wR[w], ARMdword, x, y)
#define wCID   0
#define wCon   1
#define wCSSF  2
#define wCASF  3
#define wCGR0  8
#define wCGR1  9
#define wCGR2 10
#define wCGR3 11

/* Bits in the wCon register.  */
#define WCON_CUP	(1 << 0)
#define WCON_MUP	(1 << 1)

/* Set the SIMD wCASF flags for 8, 16, 32 or 64-bit operations.  */
#define SIMD8_SET(x,  v, n, b)	(x) |= ((v != 0) << ((((b) + 1) * 4) + (n)))
#define SIMD16_SET(x, v, n, h)	(x) |= ((v != 0) << ((((h) + 1) * 8) + (n)))
#define SIMD32_SET(x, v, n, w)	(x) |= ((v != 0) << ((((w) + 1) * 16) + (n)))
#define SIMD64_SET(x, v, n)	(x) |= ((v != 0) << (32 + (n)))

/* Flags to pass as "n" above.  */
#define SIMD_NBIT	-1
#define SIMD_ZBIT	-2
#define SIMD_CBIT	-3
#define SIMD_VBIT	-4

/* Various status bit macros.  */
#define NBIT8(x)	((x) & 0x80)
#define NBIT16(x)	((x) & 0x8000)
#define NBIT32(x)	((x) & 0x80000000)
#define NBIT64(x)	((x) & 0x8000000000000000ULL)
#define ZBIT8(x)	(((x) & 0xff) == 0)
#define ZBIT16(x)	(((x) & 0xffff) == 0)
#define ZBIT32(x)	(((x) & 0xffffffff) == 0)
#define ZBIT64(x)	(x == 0)

/* Access byte/half/word "n" of register "x".  */
#define wRBYTE(x,n)	wRBITS ((x), (n) * 8, (n) * 8 + 7)
#define wRHALF(x,n)	wRBITS ((x), (n) * 16, (n) * 16 + 15)
#define wRWORD(x,n)	wRBITS ((x), (n) * 32, (n) * 32 + 31)

/* Macro to handle how the G bit selects wCGR registers.  */
#define DECODE_G_BIT(state, instr, shift)	\
{						\
  unsigned int reg;				\
						\
  reg = BITS (0, 3);				\
						\
  if (BIT (8))	/* G */				\
    {						\
      if (reg < wCGR0 || reg > wCGR3)		\
	{					\
	  ARMul_UndefInstr (state, instr);	\
	  return ARMul_DONE;			\
	}					\
      shift = wC [reg];				\
    }						\
  else						\
    shift = wR [reg];				\
						\
  shift &= 0xff;				\
}

/* Index calculations for the satrv[] array.  */
#define BITIDX8(x)	(x)
#define BITIDX16(x)	(((x) + 1) * 2 - 1)
#define BITIDX32(x)	(((x) + 1) * 4 - 1)

/* Sign extension macros.  */
#define EXTEND8(a)	((a) & 0x80 ? ((a) | 0xffffff00) : (a))
#define EXTEND16(a)	((a) & 0x8000 ? ((a) | 0xffff0000) : (a))
#define EXTEND32(a)	((a) & 0x80000000ULL ? ((a) | 0xffffffff00000000ULL) : (a))

/* Set the wCSSF from 8 values.  */
#define SET_wCSSF(a,b,c,d,e,f,g,h) \
  wC[wCSSF] = (((h) != 0) << 7) | (((g) != 0) << 6) \
            | (((f) != 0) << 5) | (((e) != 0) << 4) \
            | (((d) != 0) << 3) | (((c) != 0) << 2) \
            | (((b) != 0) << 1) | (((a) != 0) << 0);

/* Set the wCSSR from an array with 8 values.  */
#define SET_wCSSFvec(v) \
  SET_wCSSF((v)[0],(v)[1],(v)[2],(v)[3],(v)[4],(v)[5],(v)[6],(v)[7])

/* Size qualifiers for vector operations.  */
#define Bqual 			0
#define Hqual 			1
#define Wqual 			2
#define Dqual 			3

/* Saturation qualifiers for vector operations.  */
#define NoSaturation 		0
#define UnsignedSaturation	1
#define SignedSaturation	3


/* Prototypes.  */
static ARMword         Add32  (ARMword,  ARMword,  int *, int *, ARMword);
static ARMdword        AddS32 (ARMdword, ARMdword, int *, int *);
static ARMdword        AddU32 (ARMdword, ARMdword, int *, int *);
static ARMword         AddS16 (ARMword,  ARMword,  int *, int *);
static ARMword         AddU16 (ARMword,  ARMword,  int *, int *);
static ARMword         AddS8  (ARMword,  ARMword,  int *, int *);
static ARMword         AddU8  (ARMword,  ARMword,  int *, int *);
static ARMword         Sub32  (ARMword,  ARMword,  int *, int *, ARMword);
static ARMdword        SubS32 (ARMdword, ARMdword, int *, int *);
static ARMdword        SubU32 (ARMdword, ARMdword, int *, int *);
static ARMword         SubS16 (ARMword,  ARMword,  int *, int *);
static ARMword         SubS8  (ARMword,  ARMword,  int *, int *);
static ARMword         SubU16 (ARMword,  ARMword,  int *, int *);
static ARMword         SubU8  (ARMword,  ARMword,  int *, int *);
static unsigned char   IwmmxtSaturateU8  (signed short, int *);
static signed char     IwmmxtSaturateS8  (signed short, int *);
static unsigned short  IwmmxtSaturateU16 (signed int, int *);
static signed short    IwmmxtSaturateS16 (signed int, int *);
static unsigned long   IwmmxtSaturateU32 (signed long long, int *);
static signed long     IwmmxtSaturateS32 (signed long long, int *);
static ARMword         Compute_Iwmmxt_Address   (ARMul_State *, ARMword, int *);
static ARMdword        Iwmmxt_Load_Double_Word  (ARMul_State *, ARMword);
static ARMword         Iwmmxt_Load_Word         (ARMul_State *, ARMword);
static ARMword         Iwmmxt_Load_Half_Word    (ARMul_State *, ARMword);
static ARMword         Iwmmxt_Load_Byte         (ARMul_State *, ARMword);
static void            Iwmmxt_Store_Double_Word (ARMul_State *, ARMword, ARMdword);
static void            Iwmmxt_Store_Word        (ARMul_State *, ARMword, ARMword);
static void            Iwmmxt_Store_Half_Word   (ARMul_State *, ARMword, ARMword);
static void            Iwmmxt_Store_Byte        (ARMul_State *, ARMword, ARMword);
static int             Process_Instruction      (ARMul_State *, ARMword);

static int TANDC    (ARMul_State *, ARMword);
static int TBCST    (ARMul_State *, ARMword);
static int TEXTRC   (ARMul_State *, ARMword);
static int TEXTRM   (ARMul_State *, ARMword);
static int TINSR    (ARMul_State *, ARMword);
static int TMCR     (ARMul_State *, ARMword);
static int TMCRR    (ARMul_State *, ARMword);
static int TMIA     (ARMul_State *, ARMword);
static int TMIAPH   (ARMul_State *, ARMword);
static int TMIAxy   (ARMul_State *, ARMword);
static int TMOVMSK  (ARMul_State *, ARMword);
static int TMRC     (ARMul_State *, ARMword);
static int TMRRC    (ARMul_State *, ARMword);
static int TORC     (ARMul_State *, ARMword);
static int WACC     (ARMul_State *, ARMword);
static int WADD     (ARMul_State *, ARMword);
static int WALIGNI  (ARMword);
static int WALIGNR  (ARMul_State *, ARMword);
static int WAND     (ARMword);
static int WANDN    (ARMword);
static int WAVG2    (ARMword);
static int WCMPEQ   (ARMul_State *, ARMword);
static int WCMPGT   (ARMul_State *, ARMword);
static int WLDR     (ARMul_State *, ARMword);
static int WMAC     (ARMword);
static int WMADD    (ARMword);
static int WMAX     (ARMul_State *, ARMword);
static int WMIN     (ARMul_State *, ARMword);
static int WMUL     (ARMword);
static int WOR      (ARMword);
static int WPACK    (ARMul_State *, ARMword);
static int WROR     (ARMul_State *, ARMword);
static int WSAD     (ARMword);
static int WSHUFH   (ARMword);
static int WSLL     (ARMul_State *, ARMword);
static int WSRA     (ARMul_State *, ARMword);
static int WSRL     (ARMul_State *, ARMword);
static int WSTR     (ARMul_State *, ARMword);
static int WSUB     (ARMul_State *, ARMword);
static int WUNPCKEH (ARMul_State *, ARMword);
static int WUNPCKEL (ARMul_State *, ARMword);
static int WUNPCKIH (ARMul_State *, ARMword);
static int WUNPCKIL (ARMul_State *, ARMword);
static int WXOR     (ARMword);

/* This function does the work of adding two 32bit values
   together, and calculating if a carry has occurred.  */

static ARMword
Add32 (ARMword a1,
       ARMword a2,
       int * carry_ptr,
       int * overflow_ptr,
       ARMword sign_mask)
{
  ARMword result = (a1 + a2);
  unsigned int uresult = (unsigned int) result;
  unsigned int ua1 = (unsigned int) a1;

  /* If (result == a1) and (a2 == 0),
     or (result > a2) then we have no carry.  */
  * carry_ptr = ((uresult == ua1) ? (a2 != 0) : (uresult < ua1));

  /* Overflow occurs when both arguments are the
     same sign, but the result is a different sign.  */
  * overflow_ptr = (   ( (result & sign_mask) && !(a1 & sign_mask) && !(a2 & sign_mask))
		    || (!(result & sign_mask) &&  (a1 & sign_mask) &&  (a2 & sign_mask)));
  
  return result;
}

static ARMdword
AddS32 (ARMdword a1, ARMdword a2, int * carry_ptr, int * overflow_ptr)
{
  ARMdword     result;
  unsigned int uresult;
  unsigned int ua1;

  a1 = EXTEND32 (a1);
  a2 = EXTEND32 (a2);

  result  = a1 + a2;
  uresult = (unsigned int) result;
  ua1     = (unsigned int) a1;

  * carry_ptr = ((uresult == a1) ? (a2 != 0) : (uresult < ua1));

  * overflow_ptr = (   ( (result & 0x80000000ULL) && !(a1 & 0x80000000ULL) && !(a2 & 0x80000000ULL))
		    || (!(result & 0x80000000ULL) &&  (a1 & 0x80000000ULL) &&  (a2 & 0x80000000ULL)));

  return result;
}

static ARMdword
AddU32 (ARMdword a1, ARMdword a2, int * carry_ptr, int * overflow_ptr)
{
  ARMdword     result;
  unsigned int uresult;
  unsigned int ua1;

  a1 &= 0xffffffff;
  a2 &= 0xffffffff;

  result  = a1 + a2;
  uresult = (unsigned int) result;
  ua1     = (unsigned int) a1;

  * carry_ptr = ((uresult == a1) ? (a2 != 0) : (uresult < ua1));

  * overflow_ptr = (   ( (result & 0x80000000ULL) && !(a1 & 0x80000000ULL) && !(a2 & 0x80000000ULL))
		    || (!(result & 0x80000000ULL) &&  (a1 & 0x80000000ULL) &&  (a2 & 0x80000000ULL)));

  return result;
}

static ARMword
AddS16 (ARMword a1, ARMword a2, int * carry_ptr, int * overflow_ptr)
{
  a1 = EXTEND16 (a1);
  a2 = EXTEND16 (a2);

  return Add32 (a1, a2, carry_ptr, overflow_ptr, 0x8000);
}

static ARMword
AddU16 (ARMword a1, ARMword a2, int * carry_ptr, int * overflow_ptr)
{
  a1 &= 0xffff;
  a2 &= 0xffff;

  return Add32 (a1, a2, carry_ptr, overflow_ptr, 0x8000);
}

static ARMword
AddS8 (ARMword a1, ARMword a2, int * carry_ptr, int * overflow_ptr)
{
  a1 = EXTEND8 (a1);
  a2 = EXTEND8 (a2);

  return Add32 (a1, a2, carry_ptr, overflow_ptr, 0x80);
}

static ARMword
AddU8 (ARMword a1, ARMword a2, int * carry_ptr, int * overflow_ptr)
{
  a1 &= 0xff;
  a2 &= 0xff;

  return Add32 (a1, a2, carry_ptr, overflow_ptr, 0x80);
}

static ARMword
Sub32 (ARMword a1,
       ARMword a2,
       int * borrow_ptr,
       int * overflow_ptr,
       ARMword sign_mask)
{
  ARMword result = (a1 - a2);
  unsigned int ua1 = (unsigned int) a1;
  unsigned int ua2 = (unsigned int) a2;

  /* A borrow occurs if a2 is (unsigned) larger than a1.
     However the carry flag is *cleared* if a borrow occurs.  */
  * borrow_ptr = ! (ua2 > ua1);

  /* Overflow occurs when a negative number is subtracted from a
     positive number and the result is negative or a positive
     number is subtracted from a negative number and the result is
     positive.  */
  * overflow_ptr = ( (! (a1 & sign_mask) &&   (a2 & sign_mask) &&   (result & sign_mask))
		    || ((a1 & sign_mask) && ! (a2 & sign_mask) && ! (result & sign_mask)));

  return result;
}

static ARMdword
SubS32 (ARMdword a1, ARMdword a2, int * borrow_ptr, int * overflow_ptr)
{
  ARMdword     result;
  unsigned int ua1;
  unsigned int ua2;

  a1 = EXTEND32 (a1);
  a2 = EXTEND32 (a2);

  result = a1 - a2;
  ua1    = (unsigned int) a1;
  ua2    = (unsigned int) a2;

  * borrow_ptr = ! (ua2 > ua1);

  * overflow_ptr = ( (! (a1 & 0x80000000ULL) &&   (a2 & 0x80000000ULL) &&   (result & 0x80000000ULL))
		    || ((a1 & 0x80000000ULL) && ! (a2 & 0x80000000ULL) && ! (result & 0x80000000ULL)));

  return result;
}

static ARMword
SubS16 (ARMword a1, ARMword a2, int * carry_ptr, int * overflow_ptr)
{
  a1 = EXTEND16 (a1);
  a2 = EXTEND16 (a2);

  return Sub32 (a1, a2, carry_ptr, overflow_ptr, 0x8000);
}

static ARMword
SubS8 (ARMword a1, ARMword a2, int * carry_ptr, int * overflow_ptr)
{
  a1 = EXTEND8 (a1);
  a2 = EXTEND8 (a2);

  return Sub32 (a1, a2, carry_ptr, overflow_ptr, 0x80);
}

static ARMword
SubU16 (ARMword a1, ARMword a2, int * carry_ptr, int * overflow_ptr)
{
  a1 &= 0xffff;
  a2 &= 0xffff;

  return Sub32 (a1, a2, carry_ptr, overflow_ptr, 0x8000);
}

static ARMword
SubU8 (ARMword a1, ARMword a2, int * carry_ptr, int * overflow_ptr)
{
  a1 &= 0xff;
  a2 &= 0xff;

  return Sub32 (a1, a2, carry_ptr, overflow_ptr, 0x80);
}

static ARMdword
SubU32 (ARMdword a1, ARMdword a2, int * borrow_ptr, int * overflow_ptr)
{
  ARMdword     result;
  unsigned int ua1;
  unsigned int ua2;

  a1 &= 0xffffffff;
  a2 &= 0xffffffff;

  result = a1 - a2;
  ua1    = (unsigned int) a1;
  ua2    = (unsigned int) a2;

  * borrow_ptr = ! (ua2 > ua1);

  * overflow_ptr = ( (! (a1 & 0x80000000ULL) &&   (a2 & 0x80000000ULL) &&   (result & 0x80000000ULL))
		    || ((a1 & 0x80000000ULL) && ! (a2 & 0x80000000ULL) && ! (result & 0x80000000ULL)));

  return result;
}

/* For the saturation.  */

static unsigned char
IwmmxtSaturateU8 (signed short val, int * sat)
{
  unsigned char rv;

  if (val < 0)
    {
      rv = 0;
      *sat = 1;
    }
  else if (val > 0xff)
    {
      rv = 0xff;
      *sat = 1;
    }
  else
    {
      rv = val & 0xff;
      *sat = 0;
    }
  return rv;
}

static signed char
IwmmxtSaturateS8 (signed short val, int * sat)
{
  signed char rv;

  if (val < -0x80)
    {
      rv = -0x80;
      *sat = 1;
    }
  else if (val > 0x7f)
    {
      rv = 0x7f;
      *sat = 1;
    }
  else
    {
      rv = val & 0xff;
      *sat = 0;
    }
  return rv;
}

static unsigned short
IwmmxtSaturateU16 (signed int val, int * sat)
{
  unsigned short rv;

  if (val < 0)
    {
      rv = 0;
      *sat = 1;
    }
  else if (val > 0xffff)
    {
      rv = 0xffff;
      *sat = 1;
    }
  else
    {
      rv = val & 0xffff;
      *sat = 0;
    }
  return rv;
}

static signed short
IwmmxtSaturateS16 (signed int val, int * sat)
{
  signed short rv;
  
  if (val < -0x8000)
    {
      rv = - 0x8000;
      *sat = 1;
    }
  else if (val > 0x7fff)
    {
      rv = 0x7fff;
      *sat = 1;
    }
  else
    {
      rv = val & 0xffff;
      *sat = 0;
    }
  return rv;
}

static unsigned long
IwmmxtSaturateU32 (signed long long val, int * sat)
{
  unsigned long rv;

  if (val < 0)
    {
      rv = 0;
      *sat = 1;
    }
  else if (val > 0xffffffff)
    {
      rv = 0xffffffff;
      *sat = 1;
    }
  else
    {
      rv = val & 0xffffffff;
      *sat = 0;
    }
  return rv;
}

static signed long
IwmmxtSaturateS32 (signed long long val, int * sat)
{
  signed long rv;
  
  if (val < -0x80000000LL)
    {
      rv = -0x80000000;
      *sat = 1;
    }
  else if (val > 0x7fffffff)
    {
      rv = 0x7fffffff;
      *sat = 1;
    }
  else
    {
      rv = val & 0xffffffff;
      *sat = 0;
    }
  return rv;
}

/* Intel(r) Wireless MMX(tm) technology Acessor functions.  */

unsigned
IwmmxtLDC (ARMul_State * state ATTRIBUTE_UNUSED,
	   unsigned      type  ATTRIBUTE_UNUSED,
	   ARMword       instr,
	   ARMword       data)
{
  return ARMul_CANT;
}

unsigned
IwmmxtSTC (ARMul_State * state ATTRIBUTE_UNUSED,
	   unsigned      type  ATTRIBUTE_UNUSED,
	   ARMword       instr,
	   ARMword *     data)
{
  return ARMul_CANT;
}

unsigned
IwmmxtMRC (ARMul_State * state ATTRIBUTE_UNUSED,
	   unsigned      type  ATTRIBUTE_UNUSED,
	   ARMword       instr,
	   ARMword *     value)
{
  return ARMul_CANT;
}

unsigned
IwmmxtMCR (ARMul_State * state ATTRIBUTE_UNUSED,
	   unsigned      type  ATTRIBUTE_UNUSED,
	   ARMword       instr,
	   ARMword       value)
{
  return ARMul_CANT;
}

unsigned
IwmmxtCDP (ARMul_State * state, unsigned type, ARMword instr)
{
  return ARMul_CANT;
}

/* Intel(r) Wireless MMX(tm) technology instruction implementations.  */

static int
TANDC (ARMul_State * state, ARMword instr)
{
  ARMword cpsr;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "tandc\n");
#endif  

  /* The Rd field must be r15.  */
  if (BITS (12, 15) != 15)
    return ARMul_CANT;

  /* The CRn field must be r3.  */
  if (BITS (16, 19) != 3)
    return ARMul_CANT;

  /* The CRm field must be r0.  */
  if (BITS (0, 3) != 0)
    return ARMul_CANT;

  cpsr = ARMul_GetCPSR (state) & 0x0fffffff;

  switch (BITS (22, 23))
    {
    case Bqual:
      cpsr |= (  (wCBITS (wCASF, 28, 31) & wCBITS (wCASF, 24, 27)
		& wCBITS (wCASF, 20, 23) & wCBITS (wCASF, 16, 19)
		& wCBITS (wCASF, 12, 15) & wCBITS (wCASF,  8, 11)
		& wCBITS (wCASF,  4,  7) & wCBITS (wCASF,  0,  3)) << 28);
      break;

    case Hqual:
      cpsr |= (  (wCBITS (wCASF, 28, 31) & wCBITS (wCASF, 20, 23)
		& wCBITS (wCASF, 12, 15) & wCBITS (wCASF,  4, 7)) << 28);
      break;

    case Wqual:
      cpsr |= ((wCBITS (wCASF, 28, 31) & wCBITS (wCASF, 12, 15)) << 28);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }
  
  ARMul_SetCPSR (state, cpsr);

  return ARMul_DONE;
}

static int
TBCST (ARMul_State * state, ARMword instr)
{
  ARMdword Rn;
  int wRd;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "tbcst\n");
#endif  

  Rn  = state->Reg [BITS (12, 15)];
  if (BITS (12, 15) == 15)
    Rn &= 0xfffffffc;

  wRd = BITS (16, 19);

  switch (BITS (6, 7))
    {
    case Bqual:
      Rn &= 0xff;
      wR [wRd] = (Rn << 56) | (Rn << 48) | (Rn << 40) | (Rn << 32)
	       | (Rn << 24) | (Rn << 16) | (Rn << 8) | Rn;
      break;

    case Hqual:
      Rn &= 0xffff;
      wR [wRd] = (Rn << 48) | (Rn << 32) | (Rn << 16) | Rn;
      break;

    case Wqual:
      Rn &= 0xffffffff;
      wR [wRd] = (Rn << 32) | Rn;
      break;

    default:
      ARMul_UndefInstr (state, instr);
      break;
    }

  wC [wCon] |= WCON_MUP;
  return ARMul_DONE;
}

static int
TEXTRC (ARMul_State * state, ARMword instr)
{
  ARMword cpsr;
  ARMword selector;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "textrc\n");
#endif  

  /* The Rd field must be r15.  */
  if (BITS (12, 15) != 15)
    return ARMul_CANT;

  /* The CRn field must be r3.  */
  if (BITS (16, 19) != 3)
    return ARMul_CANT;

  /* The CRm field must be 0xxx.  */
  if (BIT (3) != 0)
    return ARMul_CANT;

  selector = BITS (0, 2);
  cpsr = ARMul_GetCPSR (state) & 0x0fffffff;

  switch (BITS (22, 23))
    {
    case Bqual: selector *= 4; break;
    case Hqual: selector = ((selector & 3) * 8) + 4; break;
    case Wqual: selector = ((selector & 1) * 16) + 12; break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }
  
  cpsr |= wCBITS (wCASF, selector, selector + 3) << 28;
  ARMul_SetCPSR (state, cpsr);

  return ARMul_DONE;
}

static int
TEXTRM (ARMul_State * state, ARMword instr)
{
  ARMword Rd;
  int     offset;
  int     wRn;
  int     sign;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "textrm\n");
#endif  

  wRn    = BITS (16, 19);
  sign   = BIT (3);
  offset = BITS (0, 2);
  
  switch (BITS (22, 23))
    {
    case Bqual:
      offset *= 8;
      Rd = wRBITS (wRn, offset, offset + 7);
      if (sign)
	Rd = EXTEND8 (Rd);
      break;

    case Hqual:
      offset = (offset & 3) * 16;
      Rd = wRBITS (wRn, offset, offset + 15);
      if (sign)
	Rd = EXTEND16 (Rd);
      break;

    case Wqual:
      offset = (offset & 1) * 32;
      Rd = wRBITS (wRn, offset, offset + 31);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  if (BITS (12, 15) == 15)
    ARMul_UndefInstr (state, instr);
  else
    state->Reg [BITS (12, 15)] = Rd;

  return ARMul_DONE;
}

static int
TINSR (ARMul_State * state, ARMword instr)
{
  ARMdword data;
  ARMword  offset;
  int      wRd;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "tinsr\n");
#endif

  wRd = BITS (16, 19);
  data = state->Reg [BITS (12, 15)];
  offset = BITS (0, 2);

  switch (BITS (6, 7))
    {
    case Bqual:
      data &= 0xff;
      switch (offset)
	{
	case 0: wR [wRd] = data | (wRBITS (wRd, 8, 63) << 8); break;
	case 1: wR [wRd] = wRBITS (wRd, 0,  7) | (data <<  8) | (wRBITS (wRd, 16, 63) << 16); break;
	case 2: wR [wRd] = wRBITS (wRd, 0, 15) | (data << 16) | (wRBITS (wRd, 24, 63) << 24); break;
	case 3: wR [wRd] = wRBITS (wRd, 0, 23) | (data << 24) | (wRBITS (wRd, 32, 63) << 32); break;
	case 4: wR [wRd] = wRBITS (wRd, 0, 31) | (data << 32) | (wRBITS (wRd, 40, 63) << 40); break;
	case 5: wR [wRd] = wRBITS (wRd, 0, 39) | (data << 40) | (wRBITS (wRd, 48, 63) << 48); break;
	case 6: wR [wRd] = wRBITS (wRd, 0, 47) | (data << 48) | (wRBITS (wRd, 56, 63) << 56); break;
	case 7: wR [wRd] = wRBITS (wRd, 0, 55) | (data << 56); break;
	}
      break;

    case Hqual:
      data &= 0xffff;

      switch (offset & 3)
	{
	case 0: wR [wRd] = data | (wRBITS (wRd, 16, 63) << 16); break;	  
	case 1: wR [wRd] = wRBITS (wRd, 0, 15) | (data << 16) | (wRBITS (wRd, 32, 63) << 32); break;
	case 2: wR [wRd] = wRBITS (wRd, 0, 31) | (data << 32) | (wRBITS (wRd, 48, 63) << 48); break;
	case 3: wR [wRd] = wRBITS (wRd, 0, 47) | (data << 48); break;
	}
      break;

    case Wqual:
      if (offset & 1)
	wR [wRd] = wRBITS (wRd, 0, 31) | (data << 32);
      else
	wR [wRd] = (wRBITS (wRd, 32, 63) << 32) | data;
      break;

    default:
      ARMul_UndefInstr (state, instr);
      break;
    }

  wC [wCon] |= WCON_MUP;
  return ARMul_DONE;
}

static int
TMCR (ARMul_State * state, ARMword instr)
{
  ARMword val;
  int     wCreg;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "tmcr\n");
#endif  

  if (BITS (0, 3) != 0)
    return ARMul_CANT;

  val = state->Reg [BITS (12, 15)];
  if (BITS (12, 15) == 15)
    val &= 0xfffffffc;

  wCreg = BITS (16, 19);

  switch (wCreg)
    {
    case wCID:
      /* The wCID register is read only.  */
      break;

    case wCon:
      /* Writing to the MUP or CUP bits clears them.  */
      wC [wCon] &= ~ (val & 0x3);
      break;
      
    case wCSSF:
      /* Only the bottom 8 bits can be written to.
          The higher bits write as zero.  */
      wC [wCSSF] = (val & 0xff);
      wC [wCon] |= WCON_CUP;
      break;
      
    default:
      wC [wCreg] = val;
      wC [wCon] |= WCON_CUP;
      break;
    }

  return ARMul_DONE;
}

static int
TMCRR (ARMul_State * state, ARMword instr)
{
  ARMdword RdHi = state->Reg [BITS (16, 19)];
  ARMword  RdLo = state->Reg [BITS (12, 15)];

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "tmcrr\n");
#endif  

  if ((BITS (16, 19) == 15) || (BITS (12, 15) == 15))
    return ARMul_CANT;

  wR [BITS (0, 3)] = (RdHi << 32) | RdLo;

  wC [wCon] |= WCON_MUP;

  return ARMul_DONE;
}

static int
TMIA (ARMul_State * state, ARMword instr)
{
  signed long long a, b;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "tmia\n");
#endif  

  if ((BITS (0, 3) == 15) || (BITS (12, 15) == 15))
    {
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  a = state->Reg [BITS (0, 3)];
  b = state->Reg [BITS (12, 15)];

  a = EXTEND32 (a);
  b = EXTEND32 (b);

  wR [BITS (5, 8)] += a * b;
  wC [wCon] |= WCON_MUP;

  return ARMul_DONE;
}

static int
TMIAPH (ARMul_State * state, ARMword instr)
{
  signed long a, b, result;
  signed long long r;
  ARMword Rm = state->Reg [BITS (0, 3)];
  ARMword Rs = state->Reg [BITS (12, 15)];
  
  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "tmiaph\n");
#endif  

  if (BITS (0, 3) == 15 || BITS (12, 15) == 15)
    {
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  a = SUBSTR (Rs, ARMword, 16, 31);
  b = SUBSTR (Rm, ARMword, 16, 31);

  a = EXTEND16 (a);
  b = EXTEND16 (b);

  result = a * b;

  r = result;
  r = EXTEND32 (r);
  
  wR [BITS (5, 8)] += r;

  a = SUBSTR (Rs, ARMword,  0, 15);
  b = SUBSTR (Rm, ARMword,  0, 15);

  a = EXTEND16 (a);
  b = EXTEND16 (b);

  result = a * b;

  r = result;
  r = EXTEND32 (r);
  
  wR [BITS (5, 8)] += r;
  wC [wCon] |= WCON_MUP;

  return ARMul_DONE;
}

static int
TMIAxy (ARMul_State * state, ARMword instr)
{
  ARMword Rm;
  ARMword Rs;
  long long temp;
  
  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "tmiaxy\n");
#endif  

  if (BITS (0, 3) == 15 || BITS (12, 15) == 15)
    {
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  Rm = state->Reg [BITS (0, 3)];
  if (BIT (17))
    Rm >>= 16;
  else
    Rm &= 0xffff;

  Rs = state->Reg [BITS (12, 15)];
  if (BIT (16))
    Rs >>= 16;
  else
    Rs &= 0xffff;

  if (Rm & (1 << 15))
    Rm -= 1 << 16;

  if (Rs & (1 << 15))
    Rs -= 1 << 16;

  Rm *= Rs;
  temp = Rm;

  if (temp & (1 << 31))
    temp -= 1ULL << 32;

  wR [BITS (5, 8)] += temp;
  wC [wCon] |= WCON_MUP;

  return ARMul_DONE;
}

static int
TMOVMSK (ARMul_State * state, ARMword instr)
{
  ARMdword result;
  int      wRn;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "tmovmsk\n");
#endif  

  /* The CRm field must be r0.  */
  if (BITS (0, 3) != 0)
    return ARMul_CANT;

  wRn = BITS (16, 19);

  switch (BITS (22, 23))
    {
    case Bqual:
      result = (  (wRBITS (wRn, 63, 63) << 7)
		| (wRBITS (wRn, 55, 55) << 6)
		| (wRBITS (wRn, 47, 47) << 5)
		| (wRBITS (wRn, 39, 39) << 4)
		| (wRBITS (wRn, 31, 31) << 3)
		| (wRBITS (wRn, 23, 23) << 2)
		| (wRBITS (wRn, 15, 15) << 1)
		| (wRBITS (wRn,  7,  7) << 0));
      break;

    case Hqual:
      result = (  (wRBITS (wRn, 63, 63) << 3)
		| (wRBITS (wRn, 47, 47) << 2)
		| (wRBITS (wRn, 31, 31) << 1)
		| (wRBITS (wRn, 15, 15) << 0));
      break;

    case Wqual:
      result = (wRBITS (wRn, 63, 63) << 1) | wRBITS (wRn, 31, 31);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  state->Reg [BITS (12, 15)] = result;

  return ARMul_DONE;
}

static int
TMRC (ARMul_State * state, ARMword instr)
{
  int reg = BITS (12, 15);

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "tmrc\n");
#endif  

  if (BITS (0, 3) != 0)
    return ARMul_CANT;

  if (reg == 15)
    ARMul_UndefInstr (state, instr);
  else
    state->Reg [reg] = wC [BITS (16, 19)];

  return ARMul_DONE;
}

static int
TMRRC (ARMul_State * state, ARMword instr)
{
  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "tmrrc\n");
#endif  

  if ((BITS (16, 19) == 15) || (BITS (12, 15) == 15) || (BITS (4, 11) != 0))
    ARMul_UndefInstr (state, instr);
  else
    {
      state->Reg [BITS (16, 19)] = wRBITS (BITS (0, 3), 32, 63);
      state->Reg [BITS (12, 15)] = wRBITS (BITS (0, 3),  0, 31);
    }

  return ARMul_DONE;
}

static int
TORC (ARMul_State * state, ARMword instr)
{
  ARMword cpsr = ARMul_GetCPSR (state);

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "torc\n");
#endif  

  /* The Rd field must be r15.  */
  if (BITS (12, 15) != 15)
    return ARMul_CANT;
  
  /* The CRn field must be r3.  */
  if (BITS (16, 19) != 3)
    return ARMul_CANT;
  
  /* The CRm field must be r0.  */
  if (BITS (0, 3) != 0)
    return ARMul_CANT;

  cpsr &= 0x0fffffff;

  switch (BITS (22, 23))
    {
    case Bqual:
      cpsr |= (  (wCBITS (wCASF, 28, 31) | wCBITS (wCASF, 24, 27)
		| wCBITS (wCASF, 20, 23) | wCBITS (wCASF, 16, 19)
		| wCBITS (wCASF, 12, 15) | wCBITS (wCASF,  8, 11)
		| wCBITS (wCASF,  4,  7) | wCBITS (wCASF,  0,  3)) << 28);
      break;

    case Hqual:
      cpsr |= (  (wCBITS (wCASF, 28, 31) | wCBITS (wCASF, 20, 23)
		| wCBITS (wCASF, 12, 15) | wCBITS (wCASF,  4,  7)) << 28);
      break;

    case Wqual:
      cpsr |= ((wCBITS (wCASF, 28, 31) | wCBITS (wCASF, 12, 15)) << 28);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }
  
  ARMul_SetCPSR (state, cpsr);

  return ARMul_DONE;
}

static int
WACC (ARMul_State * state, ARMword instr)
{
  int wRn;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wacc\n");
#endif  

  wRn = BITS (16, 19);

  switch (BITS (22, 23))
    {
    case Bqual:
      wR [BITS (12, 15)] =
	  wRBITS (wRn, 56, 63) + wRBITS (wRn, 48, 55)
	+ wRBITS (wRn, 40, 47) + wRBITS (wRn, 32, 39)
	+ wRBITS (wRn, 24, 31) + wRBITS (wRn, 16, 23)
	+ wRBITS (wRn,  8, 15) + wRBITS (wRn,  0,  7);
      break;

    case Hqual:
      wR [BITS (12, 15)] =
	  wRBITS (wRn, 48, 63) + wRBITS (wRn, 32, 47)
	+ wRBITS (wRn, 16, 31) + wRBITS (wRn,  0, 15);
      break;

    case Wqual:
      wR [BITS (12, 15)] = wRBITS (wRn, 32, 63) + wRBITS (wRn, 0, 31);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      break;
    }

  wC [wCon] |= WCON_MUP;
  return ARMul_DONE;
}

static int
WADD (ARMul_State * state, ARMword instr)
{
  ARMdword r = 0;
  ARMdword x;
  ARMdword s;
  ARMword  psr = 0;
  int      i;
  int      carry;
  int      overflow;
  int      satrv[8];

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wadd\n");
#endif  

  /* Add two numbers using the specified function,
     leaving setting the carry bit as required.  */
#define ADDx(x, y, m, f) \
   (*f) (wRBITS (BITS (16, 19), (x), (y)) & (m), \
         wRBITS (BITS ( 0,  3), (x), (y)) & (m), \
        & carry, & overflow)

  switch (BITS (22, 23))
    {
    case Bqual:
      for (i = 0; i < 8; i++)
        {
	  switch (BITS (20, 21))
	    {
	    case NoSaturation:
	      s = ADDx ((i * 8), (i * 8) + 7, 0xff, AddS8);
	      satrv [BITIDX8 (i)] = 0;
	      r |= (s & 0xff) << (i * 8);
	      SIMD8_SET (psr, NBIT8 (s), SIMD_NBIT, i);
	      SIMD8_SET (psr, ZBIT8 (s), SIMD_ZBIT, i);
	      SIMD8_SET (psr, carry,     SIMD_CBIT, i);
	      SIMD8_SET (psr, overflow,  SIMD_VBIT, i);
	      break;

	    case UnsignedSaturation:
	      s = ADDx ((i * 8), (i * 8) + 7, 0xff, AddU8);
	      x = IwmmxtSaturateU8 (s, satrv + BITIDX8 (i));
	      r |= (x & 0xff) << (i * 8);
	      SIMD8_SET (psr, NBIT8 (x), SIMD_NBIT, i);
	      SIMD8_SET (psr, ZBIT8 (x), SIMD_ZBIT, i);
	      if (! satrv [BITIDX8 (i)])
		{
		  SIMD8_SET (psr, carry,    SIMD_CBIT, i);
		  SIMD8_SET (psr, overflow, SIMD_VBIT, i);
		}
	      break;

	    case SignedSaturation:
	      s = ADDx ((i * 8), (i * 8) + 7, 0xff, AddS8);
	      x = IwmmxtSaturateS8 (s, satrv + BITIDX8 (i));
	      r |= (x & 0xff) << (i * 8);
	      SIMD8_SET (psr, NBIT8 (x), SIMD_NBIT, i);
	      SIMD8_SET (psr, ZBIT8 (x), SIMD_ZBIT, i);
	      if (! satrv [BITIDX8 (i)])
		{
		  SIMD8_SET (psr, carry,    SIMD_CBIT, i);
		  SIMD8_SET (psr, overflow, SIMD_VBIT, i);
		}
	      break;

	    default:
	      ARMul_UndefInstr (state, instr);
	      return ARMul_DONE;
	    }
	}
      break;

    case Hqual:
      satrv[0] = satrv[2] = satrv[4] = satrv[6] = 0;

      for (i = 0; i < 4; i++)
	{
	  switch (BITS (20, 21))
	    {
	    case NoSaturation:
	      s = ADDx ((i * 16), (i * 16) + 15, 0xffff, AddS16);
	      satrv [BITIDX16 (i)] = 0;
	      r |= (s & 0xffff) << (i * 16);
	      SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
	      SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	      SIMD16_SET (psr, carry,      SIMD_CBIT, i);
	      SIMD16_SET (psr, overflow,   SIMD_VBIT, i);
	      break;

	    case UnsignedSaturation:
	      s = ADDx ((i * 16), (i * 16) + 15, 0xffff, AddU16);
	      x = IwmmxtSaturateU16 (s, satrv + BITIDX16 (i));
	      r |= (x & 0xffff) << (i * 16);
	      SIMD16_SET (psr, NBIT16 (x), SIMD_NBIT, i);
	      SIMD16_SET (psr, ZBIT16 (x), SIMD_ZBIT, i);
	      if (! satrv [BITIDX16 (i)])
		{
		  SIMD16_SET (psr, carry,    SIMD_CBIT, i);
		  SIMD16_SET (psr, overflow, SIMD_VBIT, i);
		}
	      break;

	    case SignedSaturation:
	      s = ADDx ((i * 16), (i * 16) + 15, 0xffff, AddS16);
	      x = IwmmxtSaturateS16 (s, satrv + BITIDX16 (i));
	      r |= (x & 0xffff) << (i * 16);
	      SIMD16_SET (psr, NBIT16 (x), SIMD_NBIT, i);
	      SIMD16_SET (psr, ZBIT16 (x), SIMD_ZBIT, i);
	      if (! satrv [BITIDX16 (i)])
		{
		  SIMD16_SET (psr, carry,    SIMD_CBIT, i);
		  SIMD16_SET (psr, overflow, SIMD_VBIT, i);
		}
	      break;

	    default:
	      ARMul_UndefInstr (state, instr);
	      return ARMul_DONE;
	    }
	}
      break;

    case Wqual:
      satrv[0] = satrv[1] = satrv[2] = satrv[4] = satrv[5] = satrv[6] = 0;

      for (i = 0; i < 2; i++)
	{
	  switch (BITS (20, 21))
	    {
	    case NoSaturation:
	      s = ADDx ((i * 32), (i * 32) + 31, 0xffffffff, AddS32);
	      satrv [BITIDX32 (i)] = 0;
	      r |= (s & 0xffffffff) << (i * 32);
	      SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, i);
	      SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, i);
	      SIMD32_SET (psr, carry,      SIMD_CBIT, i);
	      SIMD32_SET (psr, overflow,   SIMD_VBIT, i);
	      break;

	    case UnsignedSaturation:
	      s = ADDx ((i * 32), (i * 32) + 31, 0xffffffff, AddU32);
	      x = IwmmxtSaturateU32 (s, satrv + BITIDX32 (i));
	      r |= (x & 0xffffffff) << (i * 32);
	      SIMD32_SET (psr, NBIT32 (x), SIMD_NBIT, i);
	      SIMD32_SET (psr, ZBIT32 (x), SIMD_ZBIT, i);
	      if (! satrv [BITIDX32 (i)])
		{
		  SIMD32_SET (psr, carry,    SIMD_CBIT, i);
		  SIMD32_SET (psr, overflow, SIMD_VBIT, i);
		}
	      break;

	    case SignedSaturation:
	      s = ADDx ((i * 32), (i * 32) + 31, 0xffffffff, AddS32);
	      x = IwmmxtSaturateS32 (s, satrv + BITIDX32 (i));
	      r |= (x & 0xffffffff) << (i * 32);
	      SIMD32_SET (psr, NBIT32 (x), SIMD_NBIT, i);
	      SIMD32_SET (psr, ZBIT32 (x), SIMD_ZBIT, i);
	      if (! satrv [BITIDX32 (i)])
		{
		  SIMD32_SET (psr, carry,    SIMD_CBIT, i);
		  SIMD32_SET (psr, overflow, SIMD_VBIT, i);
		}
	      break;

	    default:
	      ARMul_UndefInstr (state, instr);
	      return ARMul_DONE;
	    }
	}
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  wC [wCon] |= (WCON_MUP | WCON_CUP);

  SET_wCSSFvec (satrv);
  
#undef ADDx

  return ARMul_DONE;
}

static int
WALIGNI (ARMword instr)
{
  int shift = BITS (20, 22) * 8;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "waligni\n");
#endif  

  if (shift)
    wR [BITS (12, 15)] =
      wRBITS (BITS (16, 19), shift, 63)
      | (wRBITS (BITS (0, 3), 0, shift) << ((64 - shift)));
  else
    wR [BITS (12, 15)] = wR [BITS (16, 19)];
	   
  wC [wCon] |= WCON_MUP;
  return ARMul_DONE;
}

static int
WALIGNR (ARMul_State * state, ARMword instr)
{
  int shift = (wC [BITS (20, 21) + 8] & 0x7) * 8;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "walignr\n");
#endif  

  if (shift)
    wR [BITS (12, 15)] =
      wRBITS (BITS (16, 19), shift, 63)
      | (wRBITS (BITS (0, 3), 0, shift) << ((64 - shift)));
  else
    wR [BITS (12, 15)] = wR [BITS (16, 19)];

  wC [wCon] |= WCON_MUP;
  return ARMul_DONE;
}

static int
WAND (ARMword instr)
{
  ARMdword result;
  ARMword  psr = 0;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wand\n");
#endif  

  result = wR [BITS (16, 19)] & wR [BITS (0, 3)];
  wR [BITS (12, 15)] = result;

  SIMD64_SET (psr, (result == 0), SIMD_ZBIT);
  SIMD64_SET (psr, (result & (1ULL << 63)), SIMD_NBIT);
  
  wC [wCASF] = psr;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WANDN (ARMword instr)
{
  ARMdword result;
  ARMword  psr = 0;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wandn\n");
#endif  

  result = wR [BITS (16, 19)] & ~ wR [BITS (0, 3)];
  wR [BITS (12, 15)] = result;

  SIMD64_SET (psr, (result == 0), SIMD_ZBIT);
  SIMD64_SET (psr, (result & (1ULL << 63)), SIMD_NBIT);
  
  wC [wCASF] = psr;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WAVG2 (ARMword instr)
{
  ARMdword r = 0;
  ARMword  psr = 0;
  ARMdword s;
  int      i;
  int      round = BIT (20) ? 1 : 0;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wavg2\n");
#endif  

#define AVG2x(x, y, m) (((wRBITS (BITS (16, 19), (x), (y)) & (m)) \
		       + (wRBITS (BITS ( 0,  3), (x), (y)) & (m)) \
		       + round) / 2)

  if (BIT (22))
    {
      for (i = 0; i < 4; i++)
	{
	  s = AVG2x ((i * 16), (i * 16) + 15, 0xffff) & 0xffff;
	  SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	  r |= s << (i * 16);
	}
    }
  else
    {
      for (i = 0; i < 8; i++)
	{
	  s = AVG2x ((i * 8), (i * 8) + 7, 0xff) & 0xff;
	  SIMD8_SET (psr, ZBIT8 (s), SIMD_ZBIT, i);
	  r |= s << (i * 8);
	}
    }

  wR [BITS (12, 15)] = r;
  wC [wCASF] = psr;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WCMPEQ (ARMul_State * state, ARMword instr)
{
  ARMdword r = 0;
  ARMword  psr = 0;
  ARMdword s;
  int      i;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wcmpeq\n");
#endif  

  switch (BITS (22, 23))
    {
    case Bqual:
      for (i = 0; i < 8; i++)
	{
	  s = wRBYTE (BITS (16, 19), i) == wRBYTE (BITS (0, 3), i) ? 0xff : 0;
	  r |= s << (i * 8);
	  SIMD8_SET (psr, NBIT8 (s), SIMD_NBIT, i);
	  SIMD8_SET (psr, ZBIT8 (s), SIMD_ZBIT, i);
	}
      break;

    case Hqual:
      for (i = 0; i < 4; i++)
	{
	  s = wRHALF (BITS (16, 19), i) == wRHALF (BITS (0, 3), i) ? 0xffff : 0;
	  r |= s << (i * 16);
	  SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
	  SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	}
      break;

    case Wqual:
      for (i = 0; i < 2; i++)
	{
	  s = wRWORD (BITS (16, 19), i) == wRWORD (BITS (0, 3), i) ? 0xffffffff : 0;
	  r |= s << (i * 32);
	  SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, i);
	  SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, i);
	}
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WCMPGT (ARMul_State * state, ARMword instr)
{
  ARMdword r = 0;
  ARMword  psr = 0;
  ARMdword s;
  int      i;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wcmpgt\n");
#endif  

  switch (BITS (22, 23))
    {
    case Bqual:
      if (BIT (21))
	{
	  /* Use a signed comparison.  */
	  for (i = 0; i < 8; i++)
	    {
	      signed char a, b;
	      
	      a = wRBYTE (BITS (16, 19), i);
	      b = wRBYTE (BITS (0, 3), i);

	      s = (a > b) ? 0xff : 0;
	      r |= s << (i * 8);
	      SIMD8_SET (psr, NBIT8 (s), SIMD_NBIT, i);
	      SIMD8_SET (psr, ZBIT8 (s), SIMD_ZBIT, i);
	    }
	}
      else
	{
	  for (i = 0; i < 8; i++)
	    {
	      s = (wRBYTE (BITS (16, 19), i) > wRBYTE (BITS (0, 3), i))
		? 0xff : 0;
	      r |= s << (i * 8);
	      SIMD8_SET (psr, NBIT8 (s), SIMD_NBIT, i);
	      SIMD8_SET (psr, ZBIT8 (s), SIMD_ZBIT, i);
	    }
	}
      break;

    case Hqual:
      if (BIT (21))
	{
	  for (i = 0; i < 4; i++)
	    {
	      signed int a, b;

	      a = wRHALF (BITS (16, 19), i);
	      a = EXTEND16 (a);

	      b = wRHALF (BITS (0, 3), i);
	      b = EXTEND16 (b);

	      s = (a > b) ? 0xffff : 0;		
	      r |= s << (i * 16);
	      SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
	      SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	    }
	}
      else
	{
	  for (i = 0; i < 4; i++)
	    {
	      s = (wRHALF (BITS (16, 19), i) > wRHALF (BITS (0, 3), i))
		? 0xffff : 0;
	      r |= s << (i * 16);
	      SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
	      SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	    }
	}
      break;

    case Wqual:
      if (BIT (21))
	{
	  for (i = 0; i < 2; i++)
	    {
	      signed long a, b;

	      a = wRWORD (BITS (16, 19), i);
	      b = wRWORD (BITS (0, 3), i);

	      s = (a > b) ? 0xffffffff : 0;
	      r |= s << (i * 32);
	      SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, i);
	      SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, i);
	    }
	}
      else
	{
	  for (i = 0; i < 2; i++)
	    {
	      s = (wRWORD (BITS (16, 19), i) > wRWORD (BITS (0, 3), i))
		? 0xffffffff : 0;
	      r |= s << (i * 32);
	      SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, i);
	      SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, i);
	    }
	}
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static ARMword
Compute_Iwmmxt_Address (ARMul_State * state, ARMword instr, int * pFailed)
{
  ARMword  Rn;
  ARMword  addr;
  ARMword  offset;
  ARMword  multiplier;

  * pFailed  = 0;
  Rn         = BITS (16, 19);
  addr       = state->Reg [Rn];
  offset     = BITS (0, 7);
  multiplier = BIT (8) ? 4 : 1;

  if (BIT (24)) /* P */
    {
      /* Pre Indexed Addressing.  */
      if (BIT (23))
	addr += offset * multiplier;
      else
	addr -= offset * multiplier;

      /* Immediate Pre-Indexed.  */
      if (BIT (21)) /* W */
	{
	  if (Rn == 15)
	    {
	      /* Writeback into R15 is UNPREDICTABLE.  */
#ifdef DEBUG
	      fprintf (stderr, "iWMMXt: writeback into r15\n");
#endif
	      * pFailed = 1;
	    }
	  else
	    state->Reg [Rn] = addr;
	}
    }
  else
    {
      /* Post Indexed Addressing.  */
      if (BIT (21)) /* W */
	{
	  /* Handle the write back of the final address.  */
	  if (Rn == 15)
	    {
	      /* Writeback into R15 is UNPREDICTABLE.  */
#ifdef DEBUG
	      fprintf (stderr, "iWMMXt: writeback into r15\n");
#endif  
	      * pFailed = 1;
	    }
	  else
	    {
	      ARMword  increment;

	      if (BIT (23))
		increment = offset * multiplier;
	      else
		increment = - (offset * multiplier);

	      state->Reg [Rn] = addr + increment;
	    }
	}
      else
	{
	  /* P == 0, W == 0, U == 0 is UNPREDICTABLE.  */
	  if (BIT (23) == 0)
	    {
#ifdef DEBUG
	      fprintf (stderr, "iWMMXt: undefined addressing mode\n");
#endif  
	      * pFailed = 1;
	    }
	}
    }

  return addr;
}

static ARMdword
Iwmmxt_Load_Double_Word (ARMul_State * state, ARMword address)
{
  ARMdword value;
  
  /* The address must be aligned on a 8 byte boundary.  */
  if (address & 0x7)
    {
      fprintf (stderr, "iWMMXt: At addr 0x%x: Unaligned double word load from 0x%x\n",
	       (state->Reg[15] - 8) & ~0x3, address);
#ifdef DEBUG
#endif
      /* No need to check for alignment traps.  An unaligned
	 double word load with alignment trapping disabled is
	 UNPREDICTABLE.  */
      ARMul_Abort (state, ARMul_DataAbortV);
    }

  /* Load the words.  */
  if (! state->bigendSig)
    {
      value = ARMul_LoadWordN (state, address + 4);
      value <<= 32;
      value |= ARMul_LoadWordN (state, address);
    }
  else
    {
      value = ARMul_LoadWordN (state, address);
      value <<= 32;
      value |= ARMul_LoadWordN (state, address + 4);
    }

  /* Check for data aborts.  */
  if (state->Aborted)
    ARMul_Abort (state, ARMul_DataAbortV);
  else
    ARMul_Icycles (state, 2, 0L);

  return value;
}

static ARMword
Iwmmxt_Load_Word (ARMul_State * state, ARMword address)
{
  ARMword value;

  /* Check for a misaligned address.  */
  if (address & 3)
    {
      if ((read_cp15_reg (1, 0, 0) & ARMul_CP15_R1_ALIGN))
	ARMul_Abort (state, ARMul_DataAbortV);
      else
	address &= ~ 3;
    }
  
  value = ARMul_LoadWordN (state, address);

  if (state->Aborted)
    ARMul_Abort (state, ARMul_DataAbortV);
  else
    ARMul_Icycles (state, 1, 0L);

  return value;
}

static ARMword
Iwmmxt_Load_Half_Word (ARMul_State * state, ARMword address)
{
  ARMword value;

  /* Check for a misaligned address.  */
  if (address & 1)
    {
      if ((read_cp15_reg (1, 0, 0) & ARMul_CP15_R1_ALIGN))
	ARMul_Abort (state, ARMul_DataAbortV);
      else
	address &= ~ 1;
    }

  value = ARMul_LoadHalfWord (state, address);

  if (state->Aborted)
    ARMul_Abort (state, ARMul_DataAbortV);
  else
    ARMul_Icycles (state, 1, 0L);

  return value;
}

static ARMword
Iwmmxt_Load_Byte (ARMul_State * state, ARMword address)
{
  ARMword value;

  value = ARMul_LoadByte (state, address);

  if (state->Aborted)
    ARMul_Abort (state, ARMul_DataAbortV);
  else
    ARMul_Icycles (state, 1, 0L);

  return value;
}

static void
Iwmmxt_Store_Double_Word (ARMul_State * state, ARMword address, ARMdword value)
{
  /* The address must be aligned on a 8 byte boundary.  */
  if (address & 0x7)
    {
      fprintf (stderr, "iWMMXt: At addr 0x%x: Unaligned double word store to 0x%x\n",
	       (state->Reg[15] - 8) & ~0x3, address);
#ifdef DEBUG
#endif
      /* No need to check for alignment traps.  An unaligned
	 double word store with alignment trapping disabled is
	 UNPREDICTABLE.  */
      ARMul_Abort (state, ARMul_DataAbortV);
    }

  /* Store the words.  */
  if (! state->bigendSig)
    {
      ARMul_StoreWordN (state, address, value);
      ARMul_StoreWordN (state, address + 4, value >> 32);
    }
  else
    {
      ARMul_StoreWordN (state, address + 4, value);
      ARMul_StoreWordN (state, address, value >> 32);
    }

  /* Check for data aborts.  */
  if (state->Aborted)
    ARMul_Abort (state, ARMul_DataAbortV);
  else
    ARMul_Icycles (state, 2, 0L);
}

static void
Iwmmxt_Store_Word (ARMul_State * state, ARMword address, ARMword value)
{
  /* Check for a misaligned address.  */
  if (address & 3)
    {
      if ((read_cp15_reg (1, 0, 0) & ARMul_CP15_R1_ALIGN))
	ARMul_Abort (state, ARMul_DataAbortV);
      else
	address &= ~ 3;
    }

  ARMul_StoreWordN (state, address, value);

  if (state->Aborted)
    ARMul_Abort (state, ARMul_DataAbortV);
}

static void
Iwmmxt_Store_Half_Word (ARMul_State * state, ARMword address, ARMword value)
{
  /* Check for a misaligned address.  */
  if (address & 1)
    {
      if ((read_cp15_reg (1, 0, 0) & ARMul_CP15_R1_ALIGN))
	ARMul_Abort (state, ARMul_DataAbortV);
      else
	address &= ~ 1;
    }

  ARMul_StoreHalfWord (state, address, value);

  if (state->Aborted)
    ARMul_Abort (state, ARMul_DataAbortV);
}

static void
Iwmmxt_Store_Byte (ARMul_State * state, ARMword address, ARMword value)
{
  ARMul_StoreByte (state, address, value);

  if (state->Aborted)
    ARMul_Abort (state, ARMul_DataAbortV);
}

static int
WLDR (ARMul_State * state, ARMword instr)
{
  ARMword address;
  int failed;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wldr\n");
#endif  

  address = Compute_Iwmmxt_Address (state, instr, & failed);
  if (failed)
    return ARMul_CANT;

  if (BITS (28, 31) == 0xf)
    {
      /* WLDRW wCx */
      wC [BITS (12, 15)] = Iwmmxt_Load_Word (state, address);
    }
  else if (BIT (8) == 0)
    {
      if (BIT (22) == 0)
	/* WLDRB */
	wR [BITS (12, 15)] = Iwmmxt_Load_Byte (state, address);
      else
	/* WLDRH */
	wR [BITS (12, 15)] = Iwmmxt_Load_Half_Word (state, address);
    }
  else
    {
      if (BIT (22) == 0)
	/* WLDRW wRd */
	wR [BITS (12, 15)] = Iwmmxt_Load_Word (state, address);
      else
	/* WLDRD */
	wR [BITS (12, 15)] = Iwmmxt_Load_Double_Word (state, address);
    }

  wC [wCon] |= WCON_MUP;

  return ARMul_DONE;
}

static int
WMAC (ARMword instr)
{
  int      i;
  ARMdword t = 0;
  ARMword  a, b;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wmac\n");
#endif  

  for (i = 0; i < 4; i++)
    {
      if (BIT (21))
        {
	  /* Signed.  */
	  signed long s;

	  a = wRHALF (BITS (16, 19), i);
	  a = EXTEND16 (a);

	  b = wRHALF (BITS (0, 3), i);
	  b = EXTEND16 (b);

	  s = (signed long) a * (signed long) b;

	  t = t + (ARMdword) s;
        }
      else
        {
	  /* Unsigned.  */
	  a = wRHALF (BITS (16, 19), i);
	  b = wRHALF (BITS ( 0,  3), i);

	  t += a * b;
        }
    }

  if (BIT (20))
    wR [BITS (12, 15)] = 0;

  if (BIT (21))	/* Signed.  */
    wR[BITS (12, 15)] += t;
  else
    wR [BITS (12, 15)] += t;

  wC [wCon] |= WCON_MUP;

  return ARMul_DONE;
}

static int
WMADD (ARMword instr)
{
  ARMdword r = 0;
  int i;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wmadd\n");
#endif  

  for (i = 0; i < 2; i++)
    {
      ARMdword s1, s2;

      if (BIT (21))	/* Signed.  */
        {
	  signed long a, b;

	  a = wRHALF (BITS (16, 19), i * 2);
	  a = EXTEND16 (a);

	  b = wRHALF (BITS (0, 3), i * 2);
	  b = EXTEND16 (b);

	  s1 = (ARMdword) (a * b);

	  a = wRHALF (BITS (16, 19), i * 2 + 1);
	  a = EXTEND16 (a);

	  b = wRHALF (BITS (0, 3), i * 2 + 1);
	  b = EXTEND16 (b);

	  s2 = (ARMdword) (a * b);
        }
      else			/* Unsigned.  */
        {
	  unsigned long a, b;

	  a = wRHALF (BITS (16, 19), i * 2);
	  b = wRHALF (BITS ( 0,  3), i * 2);

	  s1 = (ARMdword) (a * b);

	  a = wRHALF (BITS (16, 19), i * 2 + 1);
	  b = wRHALF (BITS ( 0,  3), i * 2 + 1);

	  s2 = (ARMdword) a * b;
        }

      r |= (ARMdword) ((s1 + s2) & 0xffffffff) << (i ? 32 : 0);
    }

  wR [BITS (12, 15)] = r;
  wC [wCon] |= WCON_MUP;

  return ARMul_DONE;
}

static int
WMAX (ARMul_State * state, ARMword instr)
{
  ARMdword r = 0;
  ARMdword s;
  int      i;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wmax\n");
#endif  

  switch (BITS (22, 23))
    {
    case Bqual:
      for (i = 0; i < 8; i++)
	if (BIT (21))	/* Signed.  */
	  {
	    int a, b;

	    a = wRBYTE (BITS (16, 19), i);
	    a = EXTEND8 (a);

	    b = wRBYTE (BITS (0, 3), i);
	    b = EXTEND8 (b);

	    if (a > b)
	      s = a;
	    else
	      s = b;

	    r |= (s & 0xff) << (i * 8);
	  }
	else	 	/* Unsigned.  */
	  {
	    unsigned int a, b;

	    a = wRBYTE (BITS (16, 19), i);
	    b = wRBYTE (BITS (0, 3), i);

	    if (a > b)
	      s = a;
	    else
	      s = b;

	    r |= (s & 0xff) << (i * 8);
          }
      break;

    case Hqual:
      for (i = 0; i < 4; i++)
	if (BIT (21))	/* Signed.  */
	  {
	    int a, b;

	    a = wRHALF (BITS (16, 19), i);
	    a = EXTEND16 (a);

	    b = wRHALF (BITS (0, 3), i);
	    b = EXTEND16 (b);

	    if (a > b)
	      s = a;
	    else
	      s = b;

	    r |= (s & 0xffff) << (i * 16);
	  }
	else	 	/* Unsigned.  */
	  {
	    unsigned int a, b;

	    a = wRHALF (BITS (16, 19), i);
	    b = wRHALF (BITS (0, 3), i);

	    if (a > b)
	      s = a;
	    else
	      s = b;

	    r |= (s & 0xffff) << (i * 16);
          }
      break;

    case Wqual:
      for (i = 0; i < 2; i++)
	if (BIT (21))	/* Signed.  */
	  {
	    int a, b;

	    a = wRWORD (BITS (16, 19), i);
	    b = wRWORD (BITS (0, 3), i);

	    if (a > b)
	      s = a;
	    else
	      s = b;

	    r |= (s & 0xffffffff) << (i * 32);
	  }
	else
	  {
	    unsigned int a, b;

	    a = wRWORD (BITS (16, 19), i);
	    b = wRWORD (BITS (0, 3), i);

	    if (a > b)
	      s = a;
	    else
	      s = b;

	    r |= (s & 0xffffffff) << (i * 32);
          }
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wR [BITS (12, 15)] = r;
  wC [wCon] |= WCON_MUP;

  return ARMul_DONE;
}

static int
WMIN (ARMul_State * state, ARMword instr)
{
  ARMdword r = 0;
  ARMdword s;
  int      i;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wmin\n");
#endif  

  switch (BITS (22, 23))
    {
    case Bqual:
      for (i = 0; i < 8; i++)
	if (BIT (21))	/* Signed.  */
	  {
	    int a, b;

	    a = wRBYTE (BITS (16, 19), i);
	    a = EXTEND8 (a);

	    b = wRBYTE (BITS (0, 3), i);
	    b = EXTEND8 (b);

	    if (a < b)
	      s = a;
	    else
	      s = b;

	    r |= (s & 0xff) << (i * 8);
	  }
	else	 	/* Unsigned.  */
	  {
	    unsigned int a, b;

	    a = wRBYTE (BITS (16, 19), i);
	    b = wRBYTE (BITS (0, 3), i);

	    if (a < b)
	      s = a;
	    else
	      s = b;

	    r |= (s & 0xff) << (i * 8);
          }
      break;

    case Hqual:
      for (i = 0; i < 4; i++)
	if (BIT (21))	/* Signed.  */
	  {
	    int a, b;

	    a = wRHALF (BITS (16, 19), i);
	    a = EXTEND16 (a);

	    b = wRHALF (BITS (0, 3), i);
	    b = EXTEND16 (b);

	    if (a < b)
	      s = a;
	    else
	      s = b;

	    r |= (s & 0xffff) << (i * 16);
	  }
	else
	  {
	    /* Unsigned.  */
	    unsigned int a, b;

	    a = wRHALF (BITS (16, 19), i);
	    b = wRHALF (BITS ( 0,  3), i);

	    if (a < b)
	      s = a;
	    else
	      s = b;

	    r |= (s & 0xffff) << (i * 16);
          }
      break;

    case Wqual:
      for (i = 0; i < 2; i++)
	if (BIT (21))	/* Signed.  */
	  {
	    int a, b;

	    a = wRWORD (BITS (16, 19), i);
	    b = wRWORD (BITS ( 0,  3), i);

	    if (a < b)
	      s = a;
	    else
	      s = b;

	    r |= (s & 0xffffffff) << (i * 32);
	  }
	else
	  {
	    unsigned int a, b;

	    a = wRWORD (BITS (16, 19), i);
	    b = wRWORD (BITS (0, 3), i);

	    if (a < b)
	      s = a;
	    else
	      s = b;

	    r |= (s & 0xffffffff) << (i * 32);
          }
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wR [BITS (12, 15)] = r;
  wC [wCon] |= WCON_MUP;
  
  return ARMul_DONE;
}

static int
WMUL (ARMword instr)
{
  ARMdword r = 0;
  ARMdword s;
  int      i;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wmul\n");
#endif  

  for (i = 0; i < 4; i++)
    if (BIT (21))	/* Signed.  */
      {
	long a, b;

	a = wRHALF (BITS (16, 19), i);
	a = EXTEND16 (a);

	b = wRHALF (BITS (0, 3), i);
	b = EXTEND16 (b);

	s = a * b;

	if (BIT (20))
	  r |= ((s >> 16) & 0xffff) << (i * 16);
	else
	  r |= (s & 0xffff) << (i * 16);
      }
    else		/* Unsigned.  */
      {
	unsigned long a, b;

	a = wRHALF (BITS (16, 19), i);
	b = wRHALF (BITS (0, 3), i);

	s = a * b;

	if (BIT (20))
	  r |= ((s >> 16) & 0xffff) << (i * 16);
	else
	  r |= (s & 0xffff) << (i * 16);
      }

  wR [BITS (12, 15)] = r;
  wC [wCon] |= WCON_MUP;

  return ARMul_DONE;
}

static int
WOR (ARMword instr)
{
  ARMword psr = 0;
  ARMdword result;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wor\n");
#endif  

  result = wR [BITS (16, 19)] | wR [BITS (0, 3)];
  wR [BITS (12, 15)] = result;

  SIMD64_SET (psr, (result == 0), SIMD_ZBIT);
  SIMD64_SET (psr, (result & (1ULL << 63)), SIMD_NBIT);
  
  wC [wCASF] = psr;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WPACK (ARMul_State * state, ARMword instr)
{
  ARMdword r = 0;
  ARMword  psr = 0;
  ARMdword x;
  ARMdword s;
  int      i;
  int      satrv[8];

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wpack\n");
#endif  
 
  switch (BITS (22, 23))
    {
    case Hqual:
      for (i = 0; i < 8; i++)
	{
	  x = wRHALF (i < 4 ? BITS (16, 19) : BITS (0, 3), i & 3);

	  switch (BITS (20, 21))
	    {
	    case UnsignedSaturation:
	      s = IwmmxtSaturateU8 (x, satrv + BITIDX8 (i));
	      break;

	    case SignedSaturation:
	      s = IwmmxtSaturateS8 (x, satrv + BITIDX8 (i));
	      break;

	    default:
	      ARMul_UndefInstr (state, instr);
	      return ARMul_DONE;
	    }

	  r |= (s & 0xff) << (i * 8);
	  SIMD8_SET (psr, NBIT8 (s), SIMD_NBIT, i);
	  SIMD8_SET (psr, ZBIT8 (s), SIMD_ZBIT, i);
	}
      break;

    case Wqual:
      satrv[0] = satrv[2] = satrv[4] = satrv[6] = 0;

      for (i = 0; i < 4; i++)
	{
	  x = wRWORD (i < 2 ? BITS (16, 19) : BITS (0, 3), i & 1);

	  switch (BITS (20, 21))
	    {
	    case UnsignedSaturation:
	      s = IwmmxtSaturateU16 (x, satrv + BITIDX16 (i));
	      break;

	    case SignedSaturation:
	      s = IwmmxtSaturateS16 (x, satrv + BITIDX16 (i));
	      break;

	    default:
	      ARMul_UndefInstr (state, instr);
	      return ARMul_DONE;
	    }

	  r |= (s & 0xffff) << (i * 16);
	  SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
	  SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	}
      break;

    case Dqual:
      satrv[0] = satrv[1] = satrv[2] = satrv[4] = satrv[5] = satrv[6] = 0;

      for (i = 0; i < 2; i++)
	{
	  x = wR [i ? BITS (0, 3) : BITS (16, 19)];

	  switch (BITS (20, 21))
	    {
	    case UnsignedSaturation:
	      s = IwmmxtSaturateU32 (x, satrv + BITIDX32 (i));
	      break;

	    case SignedSaturation:
	      s = IwmmxtSaturateS32 (x, satrv + BITIDX32 (i));
	      break;

	    default:
	      ARMul_UndefInstr (state, instr);
	      return ARMul_DONE;
	    }

	  r |= (s & 0xffffffff) << (i * 32);
	  SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, i);
	  SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, i);
	}
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  SET_wCSSFvec (satrv);
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WROR (ARMul_State * state, ARMword instr)
{
  ARMdword r = 0;
  ARMdword s;
  ARMword  psr = 0;
  int      i;
  int      shift;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wror\n");
#endif  

  DECODE_G_BIT (state, instr, shift);

  switch (BITS (22, 23))
    {
    case Hqual:
      shift &= 0xf;
      for (i = 0; i < 4; i++)
	{
	  s = ((wRHALF (BITS (16, 19), i) & 0xffff) << (16 - shift))
	    | ((wRHALF (BITS (16, 19), i) & 0xffff) >> shift);
	  r |= (s & 0xffff) << (i * 16);
	  SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
	  SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	}
      break;

    case Wqual:
      shift &= 0x1f;
      for (i = 0; i < 2; i++)
	{
	  s = ((wRWORD (BITS (16, 19), i) & 0xffffffff) << (32 - shift))
	    | ((wRWORD (BITS (16, 19), i) & 0xffffffff) >> shift);
	  r |= (s & 0xffffffff) << (i * 32);
	  SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, i);
	  SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, i);
	}
      break;

    case Dqual:
      shift &= 0x3f;
      r = (wR [BITS (16, 19)] >> shift)
	| (wR [BITS (16, 19)] << (64 - shift));

      SIMD64_SET (psr, NBIT64 (r), SIMD_NBIT);
      SIMD64_SET (psr, ZBIT64 (r), SIMD_ZBIT);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WSAD (ARMword instr)
{
  ARMdword r;
  int      s;
  int      i;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wsad\n");
#endif  

  /* Z bit.  */
  r = BIT (20) ? 0 : (wR [BITS (12, 15)] & 0xffffffff);

  if (BIT (22))
    /* Half.  */
    for (i = 0; i < 4; i++)
      {
	s = (wRHALF (BITS (16, 19), i) - wRHALF (BITS (0, 3), i));
	r += abs (s);
      }
  else
    /* Byte.  */
    for (i = 0; i < 8; i++)
      {
	s = (wRBYTE (BITS (16, 19), i) - wRBYTE (BITS (0, 3), i));
	r += abs (s);
      }

  wR [BITS (12, 15)] = r;
  wC [wCon] |= WCON_MUP;

  return ARMul_DONE;
}

static int
WSHUFH (ARMword instr)
{
  ARMdword r = 0;
  ARMword  psr = 0;
  ARMdword s;
  int      i;
  int      imm8;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wshufh\n");
#endif  

  imm8 = (BITS (20, 23) << 4) | BITS (0, 3);

  for (i = 0; i < 4; i++)
    {
      s = wRHALF (BITS (16, 19), ((imm8 >> (i * 2) & 3)) & 0xff);
      r |= (s & 0xffff) << (i * 16);
      SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
      SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WSLL (ARMul_State * state, ARMword instr)
{
  ARMdword r = 0;
  ARMdword s;
  ARMword  psr = 0;
  int      i;
  unsigned shift;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wsll\n");
#endif  

  DECODE_G_BIT (state, instr, shift);

  switch (BITS (22, 23))
    {
    case Hqual:
      for (i = 0; i < 4; i++)
	{
	  if (shift > 15)
	    s = 0;
	  else
	    s = ((wRHALF (BITS (16, 19), i) & 0xffff) << shift);
	  r |= (s & 0xffff) << (i * 16);
	  SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
	  SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	}
      break;

    case Wqual:
      for (i = 0; i < 2; i++)
	{
	  if (shift > 31)
	    s = 0;
	  else
	    s = ((wRWORD (BITS (16, 19), i) & 0xffffffff) << shift);
	  r |= (s & 0xffffffff) << (i * 32);
	  SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, i);
	  SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, i);
	}
      break;

    case Dqual:
      if (shift > 63)
	r = 0;
      else
	r = ((wR[BITS (16, 19)] & 0xffffffffffffffffULL) << shift);

      SIMD64_SET (psr, NBIT64 (r), SIMD_NBIT);
      SIMD64_SET (psr, ZBIT64 (r), SIMD_ZBIT);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WSRA (ARMul_State * state, ARMword instr)
{
  ARMdword     r = 0;
  ARMdword     s;
  ARMword      psr = 0;
  int          i;
  unsigned     shift;
  signed long  t;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wsra\n");
#endif  

  DECODE_G_BIT (state, instr, shift);

  switch (BITS (22, 23))
    {
    case Hqual:
      for (i = 0; i < 4; i++)
	{
	  if (shift > 15)
	    t = (wRHALF (BITS (16, 19), i) & 0x8000) ? 0xffff : 0;
	  else
	    {
	      t = wRHALF (BITS (16, 19), i);
	      t = EXTEND16 (t);
	      t >>= shift;
	    }

	  s = t;
	  r |= (s & 0xffff) << (i * 16);
	  SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
	  SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	}
      break;

    case Wqual:
      for (i = 0; i < 2; i++)
	{
	  if (shift > 31)
	    t = (wRWORD (BITS (16, 19), i) & 0x80000000) ? 0xffffffff : 0;
	  else
	    {
	      t = wRWORD (BITS (16, 19), i);
	      t >>= shift;
	    }
	  s = t;
	  r |= (s & 0xffffffff) << (i * 32);
	  SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, i);
	  SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, i);
	}
      break;
      
    case Dqual:
      if (shift > 63)
	r = (wR [BITS (16, 19)] & 0x8000000000000000ULL) ? 0xffffffffffffffffULL : 0;
      else
	r = ((signed long long) (wR[BITS (16, 19)] & 0xffffffffffffffffULL) >> shift);
      SIMD64_SET (psr, NBIT64 (r), SIMD_NBIT);
      SIMD64_SET (psr, ZBIT64 (r), SIMD_ZBIT);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WSRL (ARMul_State * state, ARMword instr)
{
  ARMdword     r = 0;
  ARMdword     s;
  ARMword      psr = 0;
  int          i;
  unsigned int shift;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wsrl\n");
#endif

  DECODE_G_BIT (state, instr, shift);

  switch (BITS (22, 23))
    {
    case Hqual:
      for (i = 0; i < 4; i++)
	{
	  if (shift > 15)
	    s = 0;
	  else
	    s = ((unsigned) (wRHALF (BITS (16, 19), i) & 0xffff) >> shift);

	  r |= (s & 0xffff) << (i * 16);
	  SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
	  SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	}
      break;

    case Wqual:
      for (i = 0; i < 2; i++)
	{
	  if (shift > 31)
	    s = 0;
	  else
	    s = ((unsigned long) (wRWORD (BITS (16, 19), i) & 0xffffffff) >> shift);

	  r |= (s & 0xffffffff) << (i * 32);
	  SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, i);
	  SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, i);
	}
      break;

    case Dqual:
      if (shift > 63)
	r = 0;
      else
	r = (wR [BITS (16, 19)] & 0xffffffffffffffffULL) >> shift;

      SIMD64_SET (psr, NBIT64 (r), SIMD_NBIT);
      SIMD64_SET (psr, ZBIT64 (r), SIMD_ZBIT);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WSTR (ARMul_State * state, ARMword instr)
{
  ARMword address;
  int failed;


  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wstr\n");
#endif
  
  address = Compute_Iwmmxt_Address (state, instr, & failed);
  if (failed)
    return ARMul_CANT;

  if (BITS (28, 31) == 0xf)
    {
      /* WSTRW wCx */
      Iwmmxt_Store_Word (state, address, wC [BITS (12, 15)]);
    }
  else if (BIT (8) == 0)
    {
      if (BIT (22) == 0)
	/* WSTRB */
	Iwmmxt_Store_Byte (state, address, wR [BITS (12, 15)]);
      else
	/* WSTRH */
	Iwmmxt_Store_Half_Word (state, address, wR [BITS (12, 15)]);
    }
  else
    {
      if (BIT (22) == 0)
	/* WSTRW wRd */
	Iwmmxt_Store_Word (state, address, wR [BITS (12, 15)]);
      else
	/* WSTRD */
	Iwmmxt_Store_Double_Word (state, address, wR [BITS (12, 15)]);
    }

  return ARMul_DONE;
}

static int
WSUB (ARMul_State * state, ARMword instr)
{
  ARMdword r = 0;
  ARMword  psr = 0;
  ARMdword x;
  ARMdword s;
  int      i;
  int      carry;
  int      overflow;
  int      satrv[8];

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wsub\n");
#endif  

/* Subtract two numbers using the specified function,
   leaving setting the carry bit as required.  */
#define SUBx(x, y, m, f) \
   (*f) (wRBITS (BITS (16, 19), (x), (y)) & (m), \
         wRBITS (BITS ( 0,  3), (x), (y)) & (m), & carry, & overflow)

  switch (BITS (22, 23))
    {
    case Bqual:
      for (i = 0; i < 8; i++)
        {
	  switch (BITS (20, 21))
	    {
	    case NoSaturation:
	      s = SUBx ((i * 8), (i * 8) + 7, 0xff, SubS8);
	      satrv [BITIDX8 (i)] = 0;
	      r |= (s & 0xff) << (i * 8);
	      SIMD8_SET (psr, NBIT8 (s), SIMD_NBIT, i);
	      SIMD8_SET (psr, ZBIT8 (s), SIMD_ZBIT, i);
	      SIMD8_SET (psr, carry, SIMD_CBIT, i);
	      SIMD8_SET (psr, overflow, SIMD_VBIT, i);
	      break;

	    case UnsignedSaturation:
	      s = SUBx ((i * 8), (i * 8) + 7, 0xff, SubU8);
	      x = IwmmxtSaturateU8 (s, satrv + BITIDX8 (i));
	      r |= (x & 0xff) << (i * 8);
	      SIMD8_SET (psr, NBIT8 (x), SIMD_NBIT, i);
	      SIMD8_SET (psr, ZBIT8 (x), SIMD_ZBIT, i);
	      if (! satrv [BITIDX8 (i)])
		{
		  SIMD8_SET (psr, carry,     SIMD_CBIT, i);
		  SIMD8_SET (psr, overflow, SIMD_VBIT, i);
		}
	      break;

	    case SignedSaturation:
	      s = SUBx ((i * 8), (i * 8) + 7, 0xff, SubS8);
	      x = IwmmxtSaturateS8 (s, satrv + BITIDX8 (i));
	      r |= (x & 0xff) << (i * 8);
	      SIMD8_SET (psr, NBIT8 (x), SIMD_NBIT, i);
	      SIMD8_SET (psr, ZBIT8 (x), SIMD_ZBIT, i);
	      if (! satrv [BITIDX8 (i)])
		{
		  SIMD8_SET (psr, carry,     SIMD_CBIT, i);
		  SIMD8_SET (psr, overflow, SIMD_VBIT, i);
		}
	      break;

	    default:
	      ARMul_UndefInstr (state, instr);
	      return ARMul_DONE;
	    }
	}
      break;

    case Hqual:
      satrv[0] = satrv[2] = satrv[4] = satrv[6] = 0;

      for (i = 0; i < 4; i++)
	{
	  switch (BITS (20, 21))
	    {
	    case NoSaturation:
	      s = SUBx ((i * 16), (i * 16) + 15, 0xffff, SubU16);
	      satrv [BITIDX16 (i)] = 0;
	      r |= (s & 0xffff) << (i * 16);
	      SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
	      SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	      SIMD16_SET (psr, carry,      SIMD_CBIT, i);
	      SIMD16_SET (psr, overflow,   SIMD_VBIT, i);
	      break;

	    case UnsignedSaturation:
	      s = SUBx ((i * 16), (i * 16) + 15, 0xffff, SubU16);
	      x = IwmmxtSaturateU16 (s, satrv + BITIDX16 (i));
	      r |= (x & 0xffff) << (i * 16);
	      SIMD16_SET (psr, NBIT16 (x & 0xffff), SIMD_NBIT, i);
	      SIMD16_SET (psr, ZBIT16 (x), SIMD_ZBIT, i);
	      if (! satrv [BITIDX16 (i)])
		{
		  SIMD16_SET (psr, carry,    SIMD_CBIT, i);
		  SIMD16_SET (psr, overflow, SIMD_VBIT, i);
		}
	      break;

	    case SignedSaturation:
	      s = SUBx ((i * 16), (i * 16) + 15, 0xffff, SubS16);
	      x = IwmmxtSaturateS16 (s, satrv + BITIDX16 (i));
	      r |= (x & 0xffff) << (i * 16);
	      SIMD16_SET (psr, NBIT16 (x), SIMD_NBIT, i);
	      SIMD16_SET (psr, ZBIT16 (x), SIMD_ZBIT, i);
	      if (! satrv [BITIDX16 (i)])
		{
		  SIMD16_SET (psr, carry,    SIMD_CBIT, i);
		  SIMD16_SET (psr, overflow, SIMD_VBIT, i);
		}
	      break;

	    default:
	      ARMul_UndefInstr (state, instr);
	      return ARMul_DONE;
	    }
	}
      break;

    case Wqual:
      satrv[0] = satrv[1] = satrv[2] = satrv[4] = satrv[5] = satrv[6] = 0;

      for (i = 0; i < 2; i++)
	{
	  switch (BITS (20, 21))
	    {
	    case NoSaturation:
	      s = SUBx ((i * 32), (i * 32) + 31, 0xffffffff, SubU32);
	      satrv[BITIDX32 (i)] = 0;
	      r |= (s & 0xffffffff) << (i * 32);
	      SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, i);
	      SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, i);
	      SIMD32_SET (psr, carry,      SIMD_CBIT, i);
	      SIMD32_SET (psr, overflow,   SIMD_VBIT, i);
	      break;

	    case UnsignedSaturation:
	      s = SUBx ((i * 32), (i * 32) + 31, 0xffffffff, SubU32);
	      x = IwmmxtSaturateU32 (s, satrv + BITIDX32 (i));
	      r |= (x & 0xffffffff) << (i * 32);
	      SIMD32_SET (psr, NBIT32 (x), SIMD_NBIT, i);
	      SIMD32_SET (psr, ZBIT32 (x), SIMD_ZBIT, i);
	      if (! satrv [BITIDX32 (i)])
		{
		  SIMD32_SET (psr, carry,    SIMD_CBIT, i);
		  SIMD32_SET (psr, overflow, SIMD_VBIT, i);
		}
	      break;

	    case SignedSaturation:
	      s = SUBx ((i * 32), (i * 32) + 31, 0xffffffff, SubS32);
	      x = IwmmxtSaturateS32 (s, satrv + BITIDX32 (i));
	      r |= (x & 0xffffffff) << (i * 32);
	      SIMD32_SET (psr, NBIT32 (x), SIMD_NBIT, i);
	      SIMD32_SET (psr, ZBIT32 (x), SIMD_ZBIT, i);
	      if (! satrv [BITIDX32 (i)])
		{
		  SIMD32_SET (psr, carry,    SIMD_CBIT, i);
		  SIMD32_SET (psr, overflow, SIMD_VBIT, i);
		}
	      break;

	    default:
	      ARMul_UndefInstr (state, instr);
	      return ARMul_DONE;
	    }
	}
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wR [BITS (12, 15)] = r;
  wC [wCASF] = psr;
  SET_wCSSFvec (satrv);
  wC [wCon] |= (WCON_CUP | WCON_MUP);

#undef SUBx

  return ARMul_DONE;
}

static int
WUNPCKEH (ARMul_State * state, ARMword instr)
{
  ARMdword r = 0;
  ARMword  psr = 0;
  ARMdword s;
  int      i;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wunpckeh\n");
#endif  

  switch (BITS (22, 23))
    {
    case Bqual:
      for (i = 0; i < 4; i++)
	{
	  s = wRBYTE (BITS (16, 19), i + 4);

	  if (BIT (21) && NBIT8 (s))
	    s |= 0xff00;

	  r |= (s & 0xffff) << (i * 16);
	  SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
	  SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	}
      break;

    case Hqual:
      for (i = 0; i < 2; i++)
	{
	  s = wRHALF (BITS (16, 19), i + 2);

	  if (BIT (21) && NBIT16 (s))
	    s |= 0xffff0000;

	  r |= (s & 0xffffffff) << (i * 32);
	  SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, i);
	  SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, i);
	}
      break;

    case Wqual:
      r = wRWORD (BITS (16, 19), 1);

      if (BIT (21) && NBIT32 (r))
	r |= 0xffffffff00000000ULL;

      SIMD64_SET (psr, NBIT64 (r), SIMD_NBIT);
      SIMD64_SET (psr, ZBIT64 (r), SIMD_ZBIT);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WUNPCKEL (ARMul_State * state, ARMword instr)
{
  ARMdword r = 0;
  ARMword  psr = 0;
  ARMdword s;
  int      i;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wunpckel\n");
#endif  

  switch (BITS (22, 23))
    {
    case Bqual:
      for (i = 0; i < 4; i++)
	{
	  s = wRBYTE (BITS (16, 19), i);

	  if (BIT (21) && NBIT8 (s))
	    s |= 0xff00;

	  r |= (s & 0xffff) << (i * 16);
	  SIMD16_SET (psr, NBIT16 (s), SIMD_NBIT, i);
	  SIMD16_SET (psr, ZBIT16 (s), SIMD_ZBIT, i);
	}
      break;

    case Hqual:
      for (i = 0; i < 2; i++)
	{
	  s = wRHALF (BITS (16, 19), i);

	  if (BIT (21) && NBIT16 (s))
	    s |= 0xffff0000;

	  r |= (s & 0xffffffff) << (i * 32);
	  SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, i);
	  SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, i);
	}
      break;

    case Wqual:
      r = wRWORD (BITS (16, 19), 0);

      if (BIT (21) && NBIT32 (r))
	r |= 0xffffffff00000000ULL;

      SIMD64_SET (psr, NBIT64 (r), SIMD_NBIT);
      SIMD64_SET (psr, ZBIT64 (r), SIMD_ZBIT);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WUNPCKIH (ARMul_State * state, ARMword instr)
{
  ARMword  a, b;
  ARMdword r = 0;
  ARMword  psr = 0;
  ARMdword s;
  int      i;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wunpckih\n");
#endif  

  switch (BITS (22, 23))
    {
    case Bqual:
      for (i = 0; i < 4; i++)
	{
	  a = wRBYTE (BITS (16, 19), i + 4);
	  b = wRBYTE (BITS ( 0,  3), i + 4);
	  s = a | (b << 8);
	  r |= (s & 0xffff) << (i * 16);
	  SIMD8_SET (psr, NBIT8 (a), SIMD_NBIT, i * 2);
	  SIMD8_SET (psr, ZBIT8 (a), SIMD_ZBIT, i * 2);
	  SIMD8_SET (psr, NBIT8 (b), SIMD_NBIT, (i * 2) + 1);
	  SIMD8_SET (psr, ZBIT8 (b), SIMD_ZBIT, (i * 2) + 1);
	}
      break;
      
    case Hqual:
      for (i = 0; i < 2; i++)
	{
	  a = wRHALF (BITS (16, 19), i + 2);
	  b = wRHALF (BITS ( 0,  3), i + 2);
	  s = a | (b << 16);
	  r |= (s & 0xffffffff) << (i * 32);
	  SIMD16_SET (psr, NBIT16 (a), SIMD_NBIT, (i * 2));
	  SIMD16_SET (psr, ZBIT16 (a), SIMD_ZBIT, (i * 2));
	  SIMD16_SET (psr, NBIT16 (b), SIMD_NBIT, (i * 2) + 1);
	  SIMD16_SET (psr, ZBIT16 (b), SIMD_ZBIT, (i * 2) + 1);
	}
      break;

    case Wqual:
      a = wRWORD (BITS (16, 19), 1);
      s = wRWORD (BITS ( 0,  3), 1);
      r = a | (s << 32);

      SIMD32_SET (psr, NBIT32 (a), SIMD_NBIT, 0);
      SIMD32_SET (psr, ZBIT32 (a), SIMD_ZBIT, 0);
      SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, 1);
      SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, 1);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WUNPCKIL (ARMul_State * state, ARMword instr)
{
  ARMword  a, b;
  ARMdword r = 0;
  ARMword  psr = 0;
  ARMdword s;
  int      i;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wunpckil\n");
#endif  

  switch (BITS (22, 23))
    {
    case Bqual:
      for (i = 0; i < 4; i++)
	{
	  a = wRBYTE (BITS (16, 19), i);
	  b = wRBYTE (BITS ( 0,  3), i);
	  s = a | (b << 8);
	  r |= (s & 0xffff) << (i * 16);
	  SIMD8_SET (psr, NBIT8 (a), SIMD_NBIT, i * 2);
	  SIMD8_SET (psr, ZBIT8 (a), SIMD_ZBIT, i * 2);
	  SIMD8_SET (psr, NBIT8 (b), SIMD_NBIT, (i * 2) + 1);
	  SIMD8_SET (psr, ZBIT8 (b), SIMD_ZBIT, (i * 2) + 1);
	}
      break;

    case Hqual:
      for (i = 0; i < 2; i++)
	{
	  a = wRHALF (BITS (16, 19), i);
	  b = wRHALF (BITS ( 0,  3), i);
	  s = a | (b << 16);
	  r |= (s & 0xffffffff) << (i * 32);
	  SIMD16_SET (psr, NBIT16 (a), SIMD_NBIT, (i * 2));
	  SIMD16_SET (psr, ZBIT16 (a), SIMD_ZBIT, (i * 2));
	  SIMD16_SET (psr, NBIT16 (b), SIMD_NBIT, (i * 2) + 1);
	  SIMD16_SET (psr, ZBIT16 (b), SIMD_ZBIT, (i * 2) + 1);
	}
      break;

    case Wqual:
      a = wRWORD (BITS (16, 19), 0);
      s = wRWORD (BITS ( 0,  3), 0);
      r = a | (s << 32);

      SIMD32_SET (psr, NBIT32 (a), SIMD_NBIT, 0);
      SIMD32_SET (psr, ZBIT32 (a), SIMD_ZBIT, 0);
      SIMD32_SET (psr, NBIT32 (s), SIMD_NBIT, 1);
      SIMD32_SET (psr, ZBIT32 (s), SIMD_ZBIT, 1);
      break;

    default:
      ARMul_UndefInstr (state, instr);
      return ARMul_DONE;
    }

  wC [wCASF] = psr;
  wR [BITS (12, 15)] = r;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

static int
WXOR (ARMword instr)
{
  ARMword psr = 0;
  ARMdword result;

  if ((read_cp15_reg (15, 0, 1) & 3) != 3)
    return ARMul_CANT;

#ifdef DEBUG
  fprintf (stderr, "wxor\n");
#endif  

  result = wR [BITS (16, 19)] ^ wR [BITS (0, 3)];
  wR [BITS (12, 15)] = result;

  SIMD64_SET (psr, (result == 0), SIMD_ZBIT);
  SIMD64_SET (psr, (result & (1ULL << 63)), SIMD_NBIT);
  
  wC [wCASF] = psr;
  wC [wCon] |= (WCON_CUP | WCON_MUP);

  return ARMul_DONE;
}

/* This switch table is moved to a seperate function in order
   to work around a compiler bug in the host compiler...  */

static int
Process_Instruction (ARMul_State * state, ARMword instr)
{
  int status = ARMul_BUSY;

  switch ((BITS (20, 23) << 8) | BITS (4, 11))
    {
    case 0x000: status = WOR (instr); break;
    case 0x011: status = TMCR (state, instr); break;
    case 0x100: status = WXOR (instr); break;
    case 0x111: status = TMRC (state, instr); break;
    case 0x300: status = WANDN (instr); break;
    case 0x200: status = WAND (instr); break;

    case 0x810: case 0xa10:
      status = WMADD (instr); break;

    case 0x10e: case 0x50e: case 0x90e: case 0xd0e:
      status = WUNPCKIL (state, instr); break;	  
    case 0x10c: case 0x50c: case 0x90c: case 0xd0c:
      status = WUNPCKIH (state, instr); break;
    case 0x012: case 0x112: case 0x412: case 0x512:
      status = WSAD (instr); break;
    case 0x010: case 0x110: case 0x210: case 0x310:
      status = WMUL (instr); break;
    case 0x410: case 0x510: case 0x610: case 0x710:
      status = WMAC (instr); break;
    case 0x006: case 0x406: case 0x806: case 0xc06:
      status = WCMPEQ (state, instr); break;
    case 0x800: case 0x900: case 0xc00: case 0xd00:
      status = WAVG2 (instr); break;
    case 0x802: case 0x902: case 0xa02: case 0xb02:
      status = WALIGNR (state, instr); break;
    case 0x601: case 0x605: case 0x609: case 0x60d:
      status = TINSR (state, instr); break;
    case 0x107: case 0x507: case 0x907: case 0xd07:
      status = TEXTRM (state, instr); break;
    case 0x117: case 0x517: case 0x917: case 0xd17:
      status = TEXTRC (state, instr); break;
    case 0x401: case 0x405: case 0x409: case 0x40d:
      status = TBCST (state, instr); break;
    case 0x113: case 0x513: case 0x913: case 0xd13:
      status = TANDC (state, instr); break;
    case 0x01c: case 0x41c: case 0x81c: case 0xc1c:
      status = WACC (state, instr); break;
    case 0x115: case 0x515: case 0x915: case 0xd15:
      status = TORC (state, instr); break;
    case 0x103: case 0x503: case 0x903: case 0xd03:
      status = TMOVMSK (state, instr); break;
    case 0x106: case 0x306: case 0x506: case 0x706:
    case 0x906: case 0xb06: case 0xd06: case 0xf06:
      status = WCMPGT (state, instr); break;
    case 0x00e: case 0x20e: case 0x40e: case 0x60e:
    case 0x80e: case 0xa0e: case 0xc0e: case 0xe0e:
      status = WUNPCKEL (state, instr); break;
    case 0x00c: case 0x20c: case 0x40c: case 0x60c:
    case 0x80c: case 0xa0c: case 0xc0c: case 0xe0c:
      status = WUNPCKEH (state, instr); break;
    case 0x204: case 0x604: case 0xa04: case 0xe04:
    case 0x214: case 0x614: case 0xa14: case 0xe14:
      status = WSRL (state, instr); break;
    case 0x004: case 0x404: case 0x804: case 0xc04:
    case 0x014: case 0x414: case 0x814: case 0xc14:
      status = WSRA (state, instr); break;
    case 0x104: case 0x504: case 0x904: case 0xd04:
    case 0x114: case 0x514: case 0x914: case 0xd14:
      status = WSLL (state, instr); break;
    case 0x304: case 0x704: case 0xb04: case 0xf04:
    case 0x314: case 0x714: case 0xb14: case 0xf14:
      status = WROR (state, instr); break;
    case 0x116: case 0x316: case 0x516: case 0x716:
    case 0x916: case 0xb16: case 0xd16: case 0xf16:
      status = WMIN (state, instr); break;
    case 0x016: case 0x216: case 0x416: case 0x616:
    case 0x816: case 0xa16: case 0xc16: case 0xe16:
      status = WMAX (state, instr); break;
    case 0x002: case 0x102: case 0x202: case 0x302:
    case 0x402: case 0x502: case 0x602: case 0x702:
      status = WALIGNI (instr); break;
    case 0x01a: case 0x11a: case 0x21a: case 0x31a:
    case 0x41a: case 0x51a: case 0x61a: case 0x71a:
    case 0x81a: case 0x91a: case 0xa1a: case 0xb1a:
    case 0xc1a: case 0xd1a: case 0xe1a: case 0xf1a:
      status = WSUB (state, instr); break;
    case 0x01e: case 0x11e: case 0x21e: case 0x31e:	  
    case 0x41e: case 0x51e: case 0x61e: case 0x71e:
    case 0x81e: case 0x91e: case 0xa1e: case 0xb1e:
    case 0xc1e: case 0xd1e: case 0xe1e: case 0xf1e:
      status = WSHUFH (instr); break;
    case 0x018: case 0x118: case 0x218: case 0x318:
    case 0x418: case 0x518: case 0x618: case 0x718:
    case 0x818: case 0x918: case 0xa18: case 0xb18:
    case 0xc18: case 0xd18: case 0xe18: case 0xf18:
      status = WADD (state, instr); break;
    case 0x008: case 0x108: case 0x208: case 0x308:
    case 0x408: case 0x508: case 0x608: case 0x708:
    case 0x808: case 0x908: case 0xa08: case 0xb08:
    case 0xc08: case 0xd08: case 0xe08: case 0xf08:
      status = WPACK (state, instr); break;
    case 0x201: case 0x203: case 0x205: case 0x207:
    case 0x209: case 0x20b: case 0x20d: case 0x20f:
    case 0x211: case 0x213: case 0x215: case 0x217:	  
    case 0x219: case 0x21b: case 0x21d: case 0x21f:	  
      switch (BITS (16, 19))
	{
	case 0x0: status = TMIA (state, instr); break;
	case 0x8: status = TMIAPH (state, instr); break;
	case 0xc:
	case 0xd:
	case 0xe:
	case 0xf: status = TMIAxy (state, instr); break;
	default: break;
	}
      break;
    default:
      break;
    }
  return status;
}

/* Process a possibly Intel(r) Wireless MMX(tm) technology instruction.
   Return true if the instruction was handled.  */

int
ARMul_HandleIwmmxt (ARMul_State * state, ARMword instr)
{     
  int status = ARMul_BUSY;

  if (BITS (24, 27) == 0xe)
    {
      status = Process_Instruction (state, instr);
    }
  else if (BITS (25, 27) == 0x6)
    {
      if (BITS (4, 11) == 0x0 && BITS (20, 24) == 0x4)
	status = TMCRR (state, instr);
      else if (BITS (9, 11) == 0x0)
	{
	  if (BIT (20) == 0x0)
	    status = WSTR (state, instr);
	  else if (BITS (20, 24) == 0x5)
	    status = TMRRC (state, instr);
	  else
	    status = WLDR (state, instr);
	}
    }

  if (status == ARMul_CANT)
    {
      /* If the instruction was a recognised but illegal,
	 perform the abort here rather than returning false.
	 If we return false then ARMul_MRC may be called which
	 will still abort, but which also perform the register
	 transfer...  */
      ARMul_Abort (state, ARMul_UndefinedInstrV);
      status = ARMul_DONE;
    }

  return status == ARMul_DONE;
}

int
Fetch_Iwmmxt_Register (unsigned int regnum, unsigned char * memory)
{
  if (regnum >= 16)
    {
      memcpy (memory, wC + (regnum - 16), sizeof wC [0]);
      return sizeof wC [0];
    }
  else
    {
      memcpy (memory, wR + regnum, sizeof wR [0]);
      return sizeof wR [0];
    }
}

int
Store_Iwmmxt_Register (unsigned int regnum, unsigned char * memory)
{
  if (regnum >= 16)
    {
      memcpy (wC + (regnum - 16), memory, sizeof wC [0]);
      return sizeof wC [0];
    }
  else
    {
      memcpy (wR + regnum, memory, sizeof wR [0]);
      return sizeof wR [0];
    }
}

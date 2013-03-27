/* Semantics ops support for CGEN-based simulators.
   Copyright (C) 1996, 1997, 1998, 1999, 2002, 2007
   Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

This file is part of the GNU Simulators.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef CGEN_SEM_OPS_H
#define CGEN_SEM_OPS_H

#include <assert.h>

#if defined (__GNUC__) && ! defined (SEMOPS_DEFINE_INLINE)
#define SEMOPS_DEFINE_INLINE
#define SEMOPS_INLINE extern inline
#else
#define SEMOPS_INLINE
#endif

/* Semantic operations.
   At one point this file was machine generated.  Maybe it will be again.  */

/* TODO: Lazy encoding/decoding of fp values.  */

/* These don't really have a mode.  */
#define ANDIF(x, y) ((x) && (y))
#define ORIF(x, y) ((x) || (y))

#define SUBBI(x, y) ((x) - (y))
#define ANDBI(x, y) ((x) & (y))
#define ORBI(x, y) ((x) | (y))
#define XORBI(x, y) ((x) ^ (y))
#define NEGBI(x) (- (x))
#define NOTBI(x) (! (BI) (x))
#define INVBI(x) (~ (x))
#define EQBI(x, y) ((BI) (x) == (BI) (y))
#define NEBI(x, y) ((BI) (x) != (BI) (y))
#define LTBI(x, y) ((BI) (x) < (BI) (y))
#define LEBI(x, y) ((BI) (x) <= (BI) (y))
#define GTBI(x, y) ((BI) (x) > (BI) (y))
#define GEBI(x, y) ((BI) (x) >= (BI) (y))
#define LTUBI(x, y) ((BI) (x) < (BI) (y))
#define LEUBI(x, y) ((BI) (x) <= (BI) (y))
#define GTUBI(x, y) ((BI) (x) > (BI) (y))
#define GEUBI(x, y) ((BI) (x) >= (BI) (y))

#define ADDQI(x, y) ((x) + (y))
#define SUBQI(x, y) ((x) - (y))
#define MULQI(x, y) ((x) * (y))
#define DIVQI(x, y) ((QI) (x) / (QI) (y))
#define UDIVQI(x, y) ((UQI) (x) / (UQI) (y))
#define MODQI(x, y) ((QI) (x) % (QI) (y))
#define UMODQI(x, y) ((UQI) (x) % (UQI) (y))
#define SRAQI(x, y) ((QI) (x) >> (y))
#define SRLQI(x, y) ((UQI) (x) >> (y))
#define SLLQI(x, y) ((UQI) (x) << (y))
extern QI RORQI (QI, int);
extern QI ROLQI (QI, int);
#define ANDQI(x, y) ((x) & (y))
#define ORQI(x, y) ((x) | (y))
#define XORQI(x, y) ((x) ^ (y))
#define NEGQI(x) (- (x))
#define NOTQI(x) (! (QI) (x))
#define INVQI(x) (~ (x))
#define ABSQI(x) ((x) < 0 ? -(x) : (x))
#define EQQI(x, y) ((QI) (x) == (QI) (y))
#define NEQI(x, y) ((QI) (x) != (QI) (y))
#define LTQI(x, y) ((QI) (x) < (QI) (y))
#define LEQI(x, y) ((QI) (x) <= (QI) (y))
#define GTQI(x, y) ((QI) (x) > (QI) (y))
#define GEQI(x, y) ((QI) (x) >= (QI) (y))
#define LTUQI(x, y) ((UQI) (x) < (UQI) (y))
#define LEUQI(x, y) ((UQI) (x) <= (UQI) (y))
#define GTUQI(x, y) ((UQI) (x) > (UQI) (y))
#define GEUQI(x, y) ((UQI) (x) >= (UQI) (y))

#define ADDHI(x, y) ((x) + (y))
#define SUBHI(x, y) ((x) - (y))
#define MULHI(x, y) ((x) * (y))
#define DIVHI(x, y) ((HI) (x) / (HI) (y))
#define UDIVHI(x, y) ((UHI) (x) / (UHI) (y))
#define MODHI(x, y) ((HI) (x) % (HI) (y))
#define UMODHI(x, y) ((UHI) (x) % (UHI) (y))
#define SRAHI(x, y) ((HI) (x) >> (y))
#define SRLHI(x, y) ((UHI) (x) >> (y))
#define SLLHI(x, y) ((UHI) (x) << (y))
extern HI RORHI (HI, int);
extern HI ROLHI (HI, int);
#define ANDHI(x, y) ((x) & (y))
#define ORHI(x, y) ((x) | (y))
#define XORHI(x, y) ((x) ^ (y))
#define NEGHI(x) (- (x))
#define NOTHI(x) (! (HI) (x))
#define INVHI(x) (~ (x))
#define ABSHI(x) ((x) < 0 ? -(x) : (x))
#define EQHI(x, y) ((HI) (x) == (HI) (y))
#define NEHI(x, y) ((HI) (x) != (HI) (y))
#define LTHI(x, y) ((HI) (x) < (HI) (y))
#define LEHI(x, y) ((HI) (x) <= (HI) (y))
#define GTHI(x, y) ((HI) (x) > (HI) (y))
#define GEHI(x, y) ((HI) (x) >= (HI) (y))
#define LTUHI(x, y) ((UHI) (x) < (UHI) (y))
#define LEUHI(x, y) ((UHI) (x) <= (UHI) (y))
#define GTUHI(x, y) ((UHI) (x) > (UHI) (y))
#define GEUHI(x, y) ((UHI) (x) >= (UHI) (y))

#define ADDSI(x, y) ((x) + (y))
#define SUBSI(x, y) ((x) - (y))
#define MULSI(x, y) ((x) * (y))
#define DIVSI(x, y) ((SI) (x) / (SI) (y))
#define UDIVSI(x, y) ((USI) (x) / (USI) (y))
#define MODSI(x, y) ((SI) (x) % (SI) (y))
#define UMODSI(x, y) ((USI) (x) % (USI) (y))
#define SRASI(x, y) ((SI) (x) >> (y))
#define SRLSI(x, y) ((USI) (x) >> (y))
#define SLLSI(x, y) ((USI) (x) << (y))
extern SI RORSI (SI, int);
extern SI ROLSI (SI, int);
#define ANDSI(x, y) ((x) & (y))
#define ORSI(x, y) ((x) | (y))
#define XORSI(x, y) ((x) ^ (y))
#define NEGSI(x) (- (x))
#define NOTSI(x) (! (SI) (x))
#define INVSI(x) (~ (x))
#define ABSSI(x) ((x) < 0 ? -(x) : (x))
#define EQSI(x, y) ((SI) (x) == (SI) (y))
#define NESI(x, y) ((SI) (x) != (SI) (y))
#define LTSI(x, y) ((SI) (x) < (SI) (y))
#define LESI(x, y) ((SI) (x) <= (SI) (y))
#define GTSI(x, y) ((SI) (x) > (SI) (y))
#define GESI(x, y) ((SI) (x) >= (SI) (y))
#define LTUSI(x, y) ((USI) (x) < (USI) (y))
#define LEUSI(x, y) ((USI) (x) <= (USI) (y))
#define GTUSI(x, y) ((USI) (x) > (USI) (y))
#define GEUSI(x, y) ((USI) (x) >= (USI) (y))

#ifdef DI_FN_SUPPORT
extern DI ADDDI (DI, DI);
extern DI SUBDI (DI, DI);
extern DI MULDI (DI, DI);
extern DI DIVDI (DI, DI);
extern DI UDIVDI (DI, DI);
extern DI MODDI (DI, DI);
extern DI UMODDI (DI, DI);
extern DI SRADI (DI, int);
extern UDI SRLDI (UDI, int);
extern UDI SLLDI (UDI, int);
extern DI RORDI (DI, int);
extern DI ROLDI (DI, int);
extern DI ANDDI (DI, DI);
extern DI ORDI (DI, DI);
extern DI XORDI (DI, DI);
extern DI NEGDI (DI);
extern int NOTDI (DI);
extern DI INVDI (DI);
extern int EQDI (DI, DI);
extern int NEDI (DI, DI);
extern int LTDI (DI, DI);
extern int LEDI (DI, DI);
extern int GTDI (DI, DI);
extern int GEDI (DI, DI);
extern int LTUDI (UDI, UDI);
extern int LEUDI (UDI, UDI);
extern int GTUDI (UDI, UDI);
extern int GEUDI (UDI, UDI);
#else /* ! DI_FN_SUPPORT */
#define ADDDI(x, y) ((x) + (y))
#define SUBDI(x, y) ((x) - (y))
#define MULDI(x, y) ((x) * (y))
#define DIVDI(x, y) ((DI) (x) / (DI) (y))
#define UDIVDI(x, y) ((UDI) (x) / (UDI) (y))
#define MODDI(x, y) ((DI) (x) % (DI) (y))
#define UMODDI(x, y) ((UDI) (x) % (UDI) (y))
#define SRADI(x, y) ((DI) (x) >> (y))
#define SRLDI(x, y) ((UDI) (x) >> (y))
#define SLLDI(x, y) ((UDI) (x) << (y))
extern DI RORDI (DI, int);
extern DI ROLDI (DI, int);
#define ANDDI(x, y) ((x) & (y))
#define ORDI(x, y) ((x) | (y))
#define XORDI(x, y) ((x) ^ (y))
#define NEGDI(x) (- (x))
#define NOTDI(x) (! (DI) (x))
#define INVDI(x) (~ (x))
#define ABSDI(x) ((x) < 0 ? -(x) : (x))
#define EQDI(x, y) ((DI) (x) == (DI) (y))
#define NEDI(x, y) ((DI) (x) != (DI) (y))
#define LTDI(x, y) ((DI) (x) < (DI) (y))
#define LEDI(x, y) ((DI) (x) <= (DI) (y))
#define GTDI(x, y) ((DI) (x) > (DI) (y))
#define GEDI(x, y) ((DI) (x) >= (DI) (y))
#define LTUDI(x, y) ((UDI) (x) < (UDI) (y))
#define LEUDI(x, y) ((UDI) (x) <= (UDI) (y))
#define GTUDI(x, y) ((UDI) (x) > (UDI) (y))
#define GEUDI(x, y) ((UDI) (x) >= (UDI) (y))
#endif /* DI_FN_SUPPORT */

#define EXTBIQI(x) ((QI) (BI) (x))
#define EXTBIHI(x) ((HI) (BI) (x))
#define EXTBISI(x) ((SI) (BI) (x))
#if defined (DI_FN_SUPPORT)
extern DI EXTBIDI (BI);
#else
#define EXTBIDI(x) ((DI) (BI) (x))
#endif
#define EXTQIHI(x) ((HI) (QI) (x))
#define EXTQISI(x) ((SI) (QI) (x))
#if defined (DI_FN_SUPPORT)
extern DI EXTQIDI (QI);
#else
#define EXTQIDI(x) ((DI) (QI) (x))
#endif
#define EXTHIHI(x) ((HI) (HI) (x))
#define EXTHISI(x) ((SI) (HI) (x))
#define EXTSISI(x) ((SI) (SI) (x))
#if defined (DI_FN_SUPPORT)
extern DI EXTHIDI (HI);
#else
#define EXTHIDI(x) ((DI) (HI) (x))
#endif
#if defined (DI_FN_SUPPORT)
extern DI EXTSIDI (SI);
#else
#define EXTSIDI(x) ((DI) (SI) (x))
#endif

#define ZEXTBIQI(x) ((QI) (BI) (x))
#define ZEXTBIHI(x) ((HI) (BI) (x))
#define ZEXTBISI(x) ((SI) (BI) (x))
#if defined (DI_FN_SUPPORT)
extern DI ZEXTBIDI (BI);
#else
#define ZEXTBIDI(x) ((DI) (BI) (x))
#endif
#define ZEXTQIHI(x) ((HI) (UQI) (x))
#define ZEXTQISI(x) ((SI) (UQI) (x))
#if defined (DI_FN_SUPPORT)
extern DI ZEXTQIDI (QI);
#else
#define ZEXTQIDI(x) ((DI) (UQI) (x))
#endif
#define ZEXTHISI(x) ((SI) (UHI) (x))
#define ZEXTHIHI(x) ((HI) (UHI) (x))
#define ZEXTSISI(x) ((SI) (USI) (x))
#if defined (DI_FN_SUPPORT)
extern DI ZEXTHIDI (HI);
#else
#define ZEXTHIDI(x) ((DI) (UHI) (x))
#endif
#if defined (DI_FN_SUPPORT)
extern DI ZEXTSIDI (SI);
#else
#define ZEXTSIDI(x) ((DI) (USI) (x))
#endif

#define TRUNCQIBI(x) ((BI) (QI) (x))
#define TRUNCHIBI(x) ((BI) (HI) (x))
#define TRUNCHIQI(x) ((QI) (HI) (x))
#define TRUNCSIBI(x) ((BI) (SI) (x))
#define TRUNCSIQI(x) ((QI) (SI) (x))
#define TRUNCSIHI(x) ((HI) (SI) (x))
#define TRUNCSISI(x) ((SI) (SI) (x))
#if defined (DI_FN_SUPPORT)
extern BI TRUNCDIBI (DI);
#else
#define TRUNCDIBI(x) ((BI) (DI) (x))
#endif
#if defined (DI_FN_SUPPORT)
extern QI TRUNCDIQI (DI);
#else
#define TRUNCDIQI(x) ((QI) (DI) (x))
#endif
#if defined (DI_FN_SUPPORT)
extern HI TRUNCDIHI (DI);
#else
#define TRUNCDIHI(x) ((HI) (DI) (x))
#endif
#if defined (DI_FN_SUPPORT)
extern SI TRUNCDISI (DI);
#else
#define TRUNCDISI(x) ((SI) (DI) (x))
#endif

/* Composing/decomposing the various types.
   Word ordering is endian-independent.  Words are specified most to least
   significant and word number 0 is the most significant word.
   ??? May also wish an endian-dependent version.  Later.  */

#ifdef SEMOPS_DEFINE_INLINE

SEMOPS_INLINE SF
SUBWORDSISF (SI in)
{
  union { SI in; SF out; } x;
  x.in = in;
  return x.out;
}

SEMOPS_INLINE DF
SUBWORDDIDF (DI in)
{
  union { DI in; DF out; } x;
  x.in = in;
  return x.out;
}

SEMOPS_INLINE QI
SUBWORDSIQI (SI in, int byte)
{
  assert (byte >= 0 && byte <= 3);
  return (UQI) (in >> (8 * (3 - byte))) & 0xFF;
}

SEMOPS_INLINE UQI
SUBWORDSIUQI (SI in, int byte)
{
  assert (byte >= 0 && byte <= 3);
  return (UQI) (in >> (8 * (3 - byte))) & 0xFF;
}

SEMOPS_INLINE QI
SUBWORDDIQI (DI in, int byte)
{
  assert (byte >= 0 && byte <= 7);
  return (UQI) (in >> (8 * (7 - byte))) & 0xFF;
}

SEMOPS_INLINE HI
SUBWORDDIHI (DI in, int word)
{
  assert (word >= 0 && word <= 3);
  return (UHI) (in >> (16 * (3 - word))) & 0xFFFF;
}

SEMOPS_INLINE HI
SUBWORDSIHI (SI in, int word)
{
  if (word == 0)
    return (USI) in >> 16;
  else
    return in;
}

SEMOPS_INLINE SI
SUBWORDSFSI (SF in)
{
  union { SF in; SI out; } x;
  x.in = in;
  return x.out;
}

SEMOPS_INLINE DI
SUBWORDDFDI (DF in)
{
  union { DF in; DI out; } x;
  x.in = in;
  return x.out;
}

SEMOPS_INLINE UQI
SUBWORDDIUQI (DI in, int byte)
{
  assert (byte >= 0 && byte <= 7);
  return (UQI) (in >> (8 * (7 - byte)));
}

SEMOPS_INLINE SI
SUBWORDDISI (DI in, int word)
{
  if (word == 0)
    return (UDI) in >> 32;
  else
    return in;
}

SEMOPS_INLINE SI
SUBWORDDFSI (DF in, int word)
{
  /* Note: typedef UDI DF; */
  if (word == 0)
    return (UDI) in >> 32;
  else
    return in;
}

SEMOPS_INLINE SI
SUBWORDXFSI (XF in, int word)
{
  /* Note: typedef struct { SI parts[3]; } XF; */
  union { XF in; SI out[3]; } x;
  x.in = in;
  return x.out[word];
}

SEMOPS_INLINE SI
SUBWORDTFSI (TF in, int word)
{
  /* Note: typedef struct { SI parts[4]; } TF; */
  union { TF in; SI out[4]; } x;
  x.in = in;
  return x.out[word];
}

SEMOPS_INLINE DI
JOINSIDI (SI x0, SI x1)
{
  if (CURRENT_TARGET_BYTE_ORDER == BIG_ENDIAN)
    return MAKEDI (x0, x1);
  else
    return MAKEDI (x1, x0);
}

SEMOPS_INLINE DF
JOINSIDF (SI x0, SI x1)
{
  union { SI in[2]; DF out; } x;
  if (CURRENT_TARGET_BYTE_ORDER == BIG_ENDIAN)
    x.in[0] = x0, x.in[1] = x1;
  else
    x.in[1] = x0, x.in[0] = x1;
  return x.out;
}

SEMOPS_INLINE XF
JOINSIXF (SI x0, SI x1, SI x2)
{
  union { SI in[3]; XF out; } x;
  if (CURRENT_TARGET_BYTE_ORDER == BIG_ENDIAN)
    x.in[0] = x0, x.in[1] = x1, x.in[2] = x2;
  else
    x.in[2] = x0, x.in[1] = x1, x.in[0] = x2;
  return x.out;
}

SEMOPS_INLINE TF
JOINSITF (SI x0, SI x1, SI x2, SI x3)
{
  union { SI in[4]; TF out; } x;
  if (CURRENT_TARGET_BYTE_ORDER == BIG_ENDIAN)
    x.in[0] = x0, x.in[1] = x1, x.in[2] = x2, x.in[3] = x3;
  else
    x.in[3] = x0, x.in[2] = x1, x.in[1] = x2, x.in[0] = x3;
  return x.out;
}

#else

QI SUBWORDSIQI (SI);
HI SUBWORDSIHI (HI);
SI SUBWORDSFSI (SF);
SF SUBWORDSISF (SI);
DI SUBWORDDFDI (DF);
DF SUBWORDDIDF (DI);
QI SUBWORDDIQI (DI, int);
HI SUBWORDDIHI (DI, int);
SI SUBWORDDISI (DI, int);
SI SUBWORDDFSI (DF, int);
SI SUBWORDXFSI (XF, int);
SI SUBWORDTFSI (TF, int);

UQI SUBWORDSIUQI (SI);
UQI SUBWORDDIUQI (DI);

DI JOINSIDI (SI, SI);
DF JOINSIDF (SI, SI);
XF JOINSIXF (SI, SI, SI);
TF JOINSITF (SI, SI, SI, SI);

#endif /* SUBWORD,JOIN */

/* Semantic support utilities.  */

#ifdef SEMOPS_DEFINE_INLINE

SEMOPS_INLINE SI
ADDCSI (SI a, SI b, BI c)
{
  SI res = ADDSI (a, ADDSI (b, c));
  return res;
}

SEMOPS_INLINE BI
ADDCFSI (SI a, SI b, BI c)
{
  SI tmp = ADDSI (a, ADDSI (b, c));
  BI res = ((USI) tmp < (USI) a) || (c && tmp == a);
  return res;
}

SEMOPS_INLINE BI
ADDOFSI (SI a, SI b, BI c)
{
  SI tmp = ADDSI (a, ADDSI (b, c));
  BI res = (((a < 0) == (b < 0))
	    && ((a < 0) != (tmp < 0)));
  return res;
}

SEMOPS_INLINE SI
SUBCSI (SI a, SI b, BI c)
{
  SI res = SUBSI (a, ADDSI (b, c));
  return res;
}

SEMOPS_INLINE BI
SUBCFSI (SI a, SI b, BI c)
{
  BI res = ((USI) a < (USI) b) || (c && a == b);
  return res;
}

SEMOPS_INLINE BI
SUBOFSI (SI a, SI b, BI c)
{
  SI tmp = SUBSI (a, ADDSI (b, c));
  BI res = (((a < 0) != (b < 0))
	    && ((a < 0) != (tmp < 0)));
  return res;
}

SEMOPS_INLINE HI
ADDCHI (HI a, HI b, BI c)
{
  HI res = ADDHI (a, ADDHI (b, c));
  return res;
}

SEMOPS_INLINE BI
ADDCFHI (HI a, HI b, BI c)
{
  HI tmp = ADDHI (a, ADDHI (b, c));
  BI res = ((UHI) tmp < (UHI) a) || (c && tmp == a);
  return res;
}

SEMOPS_INLINE BI
ADDOFHI (HI a, HI b, BI c)
{
  HI tmp = ADDHI (a, ADDHI (b, c));
  BI res = (((a < 0) == (b < 0))
	    && ((a < 0) != (tmp < 0)));
  return res;
}

SEMOPS_INLINE HI
SUBCHI (HI a, HI b, BI c)
{
  HI res = SUBHI (a, ADDHI (b, c));
  return res;
}

SEMOPS_INLINE BI
SUBCFHI (HI a, HI b, BI c)
{
  BI res = ((UHI) a < (UHI) b) || (c && a == b);
  return res;
}

SEMOPS_INLINE BI
SUBOFHI (HI a, HI b, BI c)
{
  HI tmp = SUBHI (a, ADDHI (b, c));
  BI res = (((a < 0) != (b < 0))
	    && ((a < 0) != (tmp < 0)));
  return res;
}

SEMOPS_INLINE QI
ADDCQI (QI a, QI b, BI c)
{
  QI res = ADDQI (a, ADDQI (b, c));
  return res;
}

SEMOPS_INLINE BI
ADDCFQI (QI a, QI b, BI c)
{
  QI tmp = ADDQI (a, ADDQI (b, c));
  BI res = ((UQI) tmp < (UQI) a) || (c && tmp == a);
  return res;
}

SEMOPS_INLINE BI
ADDOFQI (QI a, QI b, BI c)
{
  QI tmp = ADDQI (a, ADDQI (b, c));
  BI res = (((a < 0) == (b < 0))
	    && ((a < 0) != (tmp < 0)));
  return res;
}

SEMOPS_INLINE QI
SUBCQI (QI a, QI b, BI c)
{
  QI res = SUBQI (a, ADDQI (b, c));
  return res;
}

SEMOPS_INLINE BI
SUBCFQI (QI a, QI b, BI c)
{
  BI res = ((UQI) a < (UQI) b) || (c && a == b);
  return res;
}

SEMOPS_INLINE BI
SUBOFQI (QI a, QI b, BI c)
{
  QI tmp = SUBQI (a, ADDQI (b, c));
  BI res = (((a < 0) != (b < 0))
	    && ((a < 0) != (tmp < 0)));
  return res;
}

#else

SI ADDCSI (SI, SI, BI);
UBI ADDCFSI (SI, SI, BI);
UBI ADDOFSI (SI, SI, BI);
SI SUBCSI (SI, SI, BI);
UBI SUBCFSI (SI, SI, BI);
UBI SUBOFSI (SI, SI, BI);
HI ADDCHI (HI, HI, BI);
UBI ADDCFHI (HI, HI, BI);
UBI ADDOFHI (HI, HI, BI);
HI SUBCHI (HI, HI, BI);
UBI SUBCFHI (HI, HI, BI);
UBI SUBOFHI (HI, HI, BI);
QI ADDCQI (QI, QI, BI);
UBI ADDCFQI (QI, QI, BI);
UBI ADDOFQI (QI, QI, BI);
QI SUBCQI (QI, QI, BI);
UBI SUBCFQI (QI, QI, BI);
UBI SUBOFQI (QI, QI, BI);

#endif

#endif /* CGEN_SEM_OPS_H */

/* $Id: resamplesubs.c 3085 2010-02-01 11:23:54Z nanang $ */
/*
 * Digital Audio Resampling Home Page located at
 * http://www-ccrma.stanford.edu/~jos/resample/.
 *
 * SOFTWARE FOR SAMPLING-RATE CONVERSION AND FIR DIGITAL FILTER DESIGN
 *
 * Snippet from the resample.1 man page:
 * 
 * HISTORY
 *
 * The first version of this software was written by Julius O. Smith III
 * <jos@ccrma.stanford.edu> at CCRMA <http://www-ccrma.stanford.edu> in
 * 1981.  It was called SRCONV and was written in SAIL for PDP-10
 * compatible machines.  The algorithm was first published in
 * 
 * Smith, Julius O. and Phil Gossett. ``A Flexible Sampling-Rate
 * Conversion Method,'' Proceedings (2): 19.4.1-19.4.4, IEEE Conference
 * on Acoustics, Speech, and Signal Processing, San Diego, March 1984.
 * 
 * An expanded tutorial based on this paper is available at the Digital
 * Audio Resampling Home Page given above.
 * 
 * Circa 1988, the SRCONV program was translated from SAIL to C by
 * Christopher Lee Fraley working with Roger Dannenberg at CMU.
 * 
 * Since then, the C version has been maintained by jos.
 * 
 * Sndlib support was added 6/99 by John Gibson <jgg9c@virginia.edu>.
 * 
 * The resample program is free software distributed in accordance
 * with the Lesser GNU Public License (LGPL).  There is NO warranty; not
 * even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/* PJMEDIA modification:
 *  - remove resample(), just use SrcUp, SrcUD, and SrcLinear directly.
 *  - move FilterUp() and FilterUD() from filterkit.c
 *  - move stddefs.h and resample.h to this file.
 *  - const correctness.
 */

#include <resamplesubs.h>
#include "config.h"
#include "stddefs.h"
#include "resample.h"


#ifdef _MSC_VER
#   pragma warning(push, 3)
//#   pragma warning(disable: 4245)   // Conversion from uint to ushort
#   pragma warning(disable: 4244)   // Conversion from double to uint
#   pragma warning(disable: 4146)   // unary minus operator applied to unsigned type, result still unsigned
#   pragma warning(disable: 4761)   // integral size mismatch in argument; conversion supplied
#endif

#if defined(RESAMPLE_HAS_SMALL_FILTER) && RESAMPLE_HAS_SMALL_FILTER!=0
#   include "smallfilter.h"
#else
#   define SMALL_FILTER_NMULT	0
#   define SMALL_FILTER_SCALE	0
#   define SMALL_FILTER_NWING	0
#   define SMALL_FILTER_IMP	NULL
#   define SMALL_FILTER_IMPD	NULL
#endif

#if defined(RESAMPLE_HAS_LARGE_FILTER) && RESAMPLE_HAS_LARGE_FILTER!=0
#   include "largefilter.h"
#else
#   define LARGE_FILTER_NMULT	0
#   define LARGE_FILTER_SCALE	0
#   define LARGE_FILTER_NWING	0
#   define LARGE_FILTER_IMP	NULL
#   define LARGE_FILTER_IMPD	NULL
#endif


#undef INLINE
#define INLINE
#define HAVE_FILTER 0    

#ifndef NULL
#   define NULL	0
#endif


static INLINE RES_HWORD WordToHword(RES_WORD v, int scl)
{
    RES_HWORD out;
    RES_WORD llsb = (1<<(scl-1));
    v += llsb;		/* round */
    v >>= scl;
    if (v>MAX_HWORD) {
	v = MAX_HWORD;
    } else if (v < MIN_HWORD) {
	v = MIN_HWORD;
    }	
    out = (RES_HWORD) v;
    return out;
}

/* Sampling rate conversion using linear interpolation for maximum speed.
 */
static int 
  SrcLinear(const RES_HWORD X[], RES_HWORD Y[], double pFactor, RES_UHWORD nx)
{
    RES_HWORD iconst;
    RES_UWORD time = 0;
    const RES_HWORD *xp;
    RES_HWORD *Ystart, *Yend;
    RES_WORD v,x1,x2;
    
    double dt;                  /* Step through input signal */ 
    RES_UWORD dtb;                  /* Fixed-point version of Dt */
    RES_UWORD endTime;              /* When time reaches EndTime, return to user */
    
    dt = 1.0/pFactor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */
    
    Ystart = Y;
    Yend = Ystart + (unsigned)(nx * pFactor + 0.5);
    endTime = time + (1<<Np)*(RES_WORD)nx;
    
    // Integer round down in dtb calculation may cause (endTime % dtb > 0), 
    // so it may cause resample write pass the output buffer (Y >= Yend).
    // while (time < endTime)
    while (Y < Yend)
    {
	iconst = (time) & Pmask;
	xp = &X[(time)>>Np];      /* Ptr to current input sample */
	x1 = *xp++;
	x2 = *xp;
	x1 *= ((1<<Np)-iconst);
	x2 *= iconst;
	v = x1 + x2;
	*Y++ = WordToHword(v,Np);   /* Deposit output */
	time += dtb;		    /* Move to next sample by time increment */
    }
    return (Y - Ystart);            /* Return number of output samples */
}

static RES_WORD FilterUp(const RES_HWORD Imp[], const RES_HWORD ImpD[], 
		     RES_UHWORD Nwing, RES_BOOL Interp,
		     const RES_HWORD *Xp, RES_HWORD Ph, RES_HWORD Inc)
{
    const RES_HWORD *Hp;
    const RES_HWORD *Hdp = NULL;
    const RES_HWORD *End;
    RES_HWORD a = 0;
    RES_WORD v, t;
    
    v=0;
    Hp = &Imp[Ph>>Na];
    End = &Imp[Nwing];
    if (Interp) {
	Hdp = &ImpD[Ph>>Na];
	a = Ph & Amask;
    }
    if (Inc == 1)		/* If doing right wing...              */
    {				/* ...drop extra coeff, so when Ph is  */
	End--;			/*    0.5, we don't do too many mult's */
	if (Ph == 0)		/* If the phase is zero...           */
	{			/* ...then we've already skipped the */
	    Hp += Npc;		/*    first sample, so we must also  */
	    Hdp += Npc;		/*    skip ahead in Imp[] and ImpD[] */
	}
    }
    if (Interp)
      while (Hp < End) {
	  t = *Hp;		/* Get filter coeff */
	  t += (((RES_WORD)*Hdp)*a)>>Na; /* t is now interp'd filter coeff */
	  Hdp += Npc;		/* Filter coeff differences step */
	  t *= *Xp;		/* Mult coeff by input sample */
	  if (t & (1<<(Nhxn-1)))  /* Round, if needed */
	    t += (1<<(Nhxn-1));
	  t >>= Nhxn;		/* Leave some guard bits, but come back some */
	  v += t;			/* The filter output */
	  Hp += Npc;		/* Filter coeff step */

	  Xp += Inc;		/* Input signal step. NO CHECK ON BOUNDS */
      } 
    else 
      while (Hp < End) {
	  t = *Hp;		/* Get filter coeff */
	  t *= *Xp;		/* Mult coeff by input sample */
	  if (t & (1<<(Nhxn-1)))  /* Round, if needed */
	    t += (1<<(Nhxn-1));
	  t >>= Nhxn;		/* Leave some guard bits, but come back some */
	  v += t;			/* The filter output */
	  Hp += Npc;		/* Filter coeff step */
	  Xp += Inc;		/* Input signal step. NO CHECK ON BOUNDS */
      }
    return(v);
}


static RES_WORD FilterUD(const RES_HWORD Imp[], const RES_HWORD ImpD[],
		     RES_UHWORD Nwing, RES_BOOL Interp,
		     const RES_HWORD *Xp, RES_HWORD Ph, RES_HWORD Inc, RES_UHWORD dhb)
{
    RES_HWORD a;
    const RES_HWORD *Hp, *Hdp, *End;
    RES_WORD v, t;
    RES_UWORD Ho;
    
    v=0;
    Ho = (Ph*(RES_UWORD)dhb)>>Np;
    End = &Imp[Nwing];
    if (Inc == 1)		/* If doing right wing...              */
    {				/* ...drop extra coeff, so when Ph is  */
	End--;			/*    0.5, we don't do too many mult's */
	if (Ph == 0)		/* If the phase is zero...           */
	  Ho += dhb;		/* ...then we've already skipped the */
    }				/*    first sample, so we must also  */
				/*    skip ahead in Imp[] and ImpD[] */
    if (Interp)
      while ((Hp = &Imp[Ho>>Na]) < End) {
	  t = *Hp;		/* Get IR sample */
	  Hdp = &ImpD[Ho>>Na];  /* get interp (lower Na) bits from diff table*/
	  a = Ho & Amask;	/* a is logically between 0 and 1 */
	  t += (((RES_WORD)*Hdp)*a)>>Na; /* t is now interp'd filter coeff */
	  t *= *Xp;		/* Mult coeff by input sample */
	  if (t & 1<<(Nhxn-1))	/* Round, if needed */
	    t += 1<<(Nhxn-1);
	  t >>= Nhxn;		/* Leave some guard bits, but come back some */
	  v += t;			/* The filter output */
	  Ho += dhb;		/* IR step */
	  Xp += Inc;		/* Input signal step. NO CHECK ON BOUNDS */
      }
    else 
      while ((Hp = &Imp[Ho>>Na]) < End) {
	  t = *Hp;		/* Get IR sample */
	  t *= *Xp;		/* Mult coeff by input sample */
	  if (t & 1<<(Nhxn-1))	/* Round, if needed */
	    t += 1<<(Nhxn-1);
	  t >>= Nhxn;		/* Leave some guard bits, but come back some */
	  v += t;			/* The filter output */
	  Ho += dhb;		/* IR step */
	  Xp += Inc;		/* Input signal step. NO CHECK ON BOUNDS */
      }
    return(v);
}

/* Sampling rate up-conversion only subroutine;
 * Slightly faster than down-conversion;
 */
static int SrcUp(const RES_HWORD X[], RES_HWORD Y[], double pFactor, 
		 RES_UHWORD nx, RES_UHWORD pNwing, RES_UHWORD pLpScl,
		 const RES_HWORD pImp[], const RES_HWORD pImpD[], RES_BOOL Interp)
{
    const RES_HWORD *xp;
    RES_HWORD *Ystart, *Yend;
    RES_WORD v;
    
    double dt;                  /* Step through input signal */ 
    RES_UWORD dtb;                  /* Fixed-point version of Dt */
    RES_UWORD time = 0;
    RES_UWORD endTime;              /* When time reaches EndTime, return to user */
    
    dt = 1.0/pFactor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */
    
    Ystart = Y;
    Yend = Ystart + (unsigned)(nx * pFactor + 0.5);
    endTime = time + (1<<Np)*(RES_WORD)nx;

    // Integer round down in dtb calculation may cause (endTime % dtb > 0), 
    // so it may cause resample write pass the output buffer (Y >= Yend).
    // while (time < endTime)
    while (Y < Yend)
    {
	xp = &X[time>>Np];      /* Ptr to current input sample */
	/* Perform left-wing inner product */
	v = 0;
	v = FilterUp(pImp, pImpD, pNwing, Interp, xp, (RES_HWORD)(time&Pmask),-1);

	/* Perform right-wing inner product */
	v += FilterUp(pImp, pImpD, pNwing, Interp, xp+1,  (RES_HWORD)((-time)&Pmask),1);

	v >>= Nhg;		/* Make guard bits */
	v *= pLpScl;		/* Normalize for unity filter gain */
	*Y++ = WordToHword(v,NLpScl);   /* strip guard bits, deposit output */
	time += dtb;		/* Move to next sample by time increment */
    }
    return (Y - Ystart);        /* Return the number of output samples */
}


/* Sampling rate conversion subroutine */

static int SrcUD(const RES_HWORD X[], RES_HWORD Y[], double pFactor, 
		 RES_UHWORD nx, RES_UHWORD pNwing, RES_UHWORD pLpScl,
		 const RES_HWORD pImp[], const RES_HWORD pImpD[], RES_BOOL Interp)
{
    const RES_HWORD *xp;
    RES_HWORD *Ystart, *Yend;
    RES_WORD v;
    
    double dh;                  /* Step through filter impulse response */
    double dt;                  /* Step through input signal */
    RES_UWORD time = 0;
    RES_UWORD endTime;          /* When time reaches EndTime, return to user */
    RES_UWORD dhb, dtb;         /* Fixed-point versions of Dh,Dt */
    
    dt = 1.0/pFactor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */
    
    dh = MIN(Npc, pFactor*Npc);  /* Filter sampling period */
    dhb = dh*(1<<Na) + 0.5;     /* Fixed-point representation */
    
    Ystart = Y;
    Yend = Ystart + (unsigned)(nx * pFactor + 0.5);
    endTime = time + (1<<Np)*(RES_WORD)nx;

    // Integer round down in dtb calculation may cause (endTime % dtb > 0), 
    // so it may cause resample write pass the output buffer (Y >= Yend).
    // while (time < endTime)
    while (Y < Yend)
    {
	xp = &X[time>>Np];	/* Ptr to current input sample */
	v = FilterUD(pImp, pImpD, pNwing, Interp, xp, (RES_HWORD)(time&Pmask),
		     -1, dhb);	/* Perform left-wing inner product */
	v += FilterUD(pImp, pImpD, pNwing, Interp, xp+1, (RES_HWORD)((-time)&Pmask),
		      1, dhb);	/* Perform right-wing inner product */
	v >>= Nhg;		/* Make guard bits */
	v *= pLpScl;		/* Normalize for unity filter gain */
	*Y++ = WordToHword(v,NLpScl);   /* strip guard bits, deposit output */
	time += dtb;		/* Move to next sample by time increment */
    }
    return (Y - Ystart);        /* Return the number of output samples */
}


DECL(int) res_SrcLinear(const RES_HWORD X[], RES_HWORD Y[], 
		        double pFactor, RES_UHWORD nx)
{
    return SrcLinear(X, Y, pFactor, nx);
}

DECL(int) res_Resample(const RES_HWORD X[], RES_HWORD Y[], double pFactor, 
		       RES_UHWORD nx, RES_BOOL LargeF, RES_BOOL Interp)
{
    if (pFactor >= 1) {

	if (LargeF)
	    return SrcUp(X, Y, pFactor, nx,
			 LARGE_FILTER_NWING, LARGE_FILTER_SCALE,
			 LARGE_FILTER_IMP, LARGE_FILTER_IMPD, Interp);
	else
	    return SrcUp(X, Y, pFactor, nx,
			 SMALL_FILTER_NWING, SMALL_FILTER_SCALE,
			 SMALL_FILTER_IMP, SMALL_FILTER_IMPD, Interp);

    } else {

	if (LargeF)
	    return SrcUD(X, Y, pFactor, nx, 
			 LARGE_FILTER_NWING, LARGE_FILTER_SCALE * pFactor + 0.5,
			 LARGE_FILTER_IMP, LARGE_FILTER_IMPD, Interp);
	else
	    return SrcUD(X, Y, pFactor, nx, 
			 SMALL_FILTER_NWING, SMALL_FILTER_SCALE * pFactor + 0.5,
			 SMALL_FILTER_IMP, SMALL_FILTER_IMPD, Interp);

    }
}

DECL(int) res_GetXOFF(double pFactor, RES_BOOL LargeF)
{
    if (LargeF)
	return (LARGE_FILTER_NMULT + 1) / 2.0  *  
	        MAX(1.0, 1.0/pFactor);
    else
	return (SMALL_FILTER_NMULT + 1) / 2.0  *  
		MAX(1.0, 1.0/pFactor);
}


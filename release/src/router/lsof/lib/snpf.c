/*
 * snpf.c -- snprintf() empulation functions for lsof library
 *
 * V. Abell
 * Purdue University Computing Center
 */

/*
 * Copyright 2000 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * This software has been adapted from snprintf.c in sendmail 8.9.3.  It
 * is subject to the sendmail copyright statements listed below, and the
 * sendmail licensing terms stated in the sendmail LICENSE file comment
 * section of this file.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */

#include "../machine.h"

#ifdef	USE_LIB_SNPF

/*
 * Sendmail copyright statements:
 *
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 * The LICENSE file may be found in the following comment section.
 */


/*
 * Begin endmail LICENSE file.

			     SENDMAIL LICENSE

The following license terms and conditions apply, unless a different
license is obtained from Sendmail, Inc., 1401 Park Avenue, Emeryville, CA
94608, or by electronic mail at license@sendmail.com.

License Terms:

Use, Modification and Redistribution (including distribution of any
modified or derived work) in source and binary forms is permitted only if
each of the following conditions is met:

1. Redistributions qualify as "freeware" or "Open Source Software" under
   one of the following terms:

   (a) Redistributions are made at no charge beyond the reasonable cost of
       materials and delivery.

   (b) Redistributions are accompanied by a copy of the Source Code or by an
       irrevocable offer to provide a copy of the Source Code for up to three
       years at the cost of materials and delivery.  Such redistributions
       must allow further use, modification, and redistribution of the Source
       Code under substantially the same terms as this license.  For the
       purposes of redistribution "Source Code" means the complete source
       code of sendmail including all modifications.

   Other forms of redistribution are allowed only under a separate royalty-
   free agreement permitting such redistribution subject to standard
   commercial terms and conditions.  A copy of such agreement may be
   obtained from Sendmail, Inc. at the above address.

2. Redistributions of source code must retain the copyright notices as they
   appear in each source code file, these license terms, and the
   disclaimer/limitation of liability set forth as paragraph 6 below.

3. Redistributions in binary form must reproduce the Copyright Notice,
   these license terms, and the disclaimer/limitation of liability set
   forth as paragraph 6 below, in the documentation and/or other materials
   provided with the distribution.  For the purposes of binary distribution
   the "Copyright Notice" refers to the following language:
   "Copyright (c) 1998 Sendmail, Inc.  All rights reserved."

4. Neither the name of Sendmail, Inc. nor the University of California nor
   the names of their contributors may be used to endorse or promote
   products derived from this software without specific prior written
   permission.  The name "sendmail" is a trademark of Sendmail, Inc.

5. All redistributions must comply with the conditions imposed by the
   University of California on certain embedded code, whose copyright
   notice and conditions for redistribution are as follows:

   (a) Copyright (c) 1988, 1993 The Regents of the University of
       California.  All rights reserved.

   (b) Redistribution and use in source and binary forms, with or without
       modification, are permitted provided that the following conditions
       are met:

      (i)   Redistributions of source code must retain the above copyright
            notice, this list of conditions and the following disclaimer.

      (ii)  Redistributions in binary form must reproduce the above
            copyright notice, this list of conditions and the following
            disclaimer in the documentation and/or other materials provided
            with the distribution.

      (iii) All advertising materials mentioning features or use of this
            software must display the following acknowledgement:  "This
            product includes software developed by the University of
            California, Berkeley and its contributors."

      (iv)  Neither the name of the University nor the names of its
            contributors may be used to endorse or promote products derived
            from this software without specific prior written permission.

6. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY
   SENDMAIL, INC. AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
   NO EVENT SHALL SENDMAIL, INC., THE REGENTS OF THE UNIVERSITY OF
   CALIFORNIA OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

(Version 8.6, last updated 6/24/1998)

 * End endmail LICENSE file.
 */


/*
 * If "ll" format support is not possible -- e.g., the long long type isn't
 * supported -- define HAS_NO_LONG_LONG.
 */

# ifndef lint
static char copyright[] =
"@(#) Copyright 2000 Purdue Research Foundation.\nAll rights reserved.\n";
# endif /* !defined(lint) */

#include <varargs.h>

#if	defined(__STDC__)
#define	_PROTOTYPE(function, params)	function params
#else	/* !defined(__STDC__) */
#define	_PROTOTYPE(function, params)	function()
#endif /* defined(__STDC__) */


/*
**  SNPRINTF, VSNPRINT -- counted versions of printf
**
**	These versions have been grabbed off the net.  They have been
**	cleaned up to compile properly and support for .precision and
**	%lx has been added.
*/

/**************************************************************
 * Original:
 * Patrick Powell Tue Apr 11 09:48:21 PDT 1995
 * A bombproof version of doprnt (dopr) included.
 * Sigh.  This sort of thing is always nasty do deal with.  Note that
 * the version here does not include floating point...
 *
 * snprintf() is used instead of sprintf() as it does limit checks
 * for string length.  This covers a nasty loophole.
 *
 * The other functions are there to prevent NULL pointers from
 * causing nast effects.
 **************************************************************/

/*static char _id[] = "$Id: snpf.c,v 1.5 2008/10/21 16:13:23 abe Exp $";*/


/*
 * Local function prototypes
 */

_PROTOTYPE(static void dopr,(char *bp, char *ep, char *fmt, va_list args));
_PROTOTYPE(static void dopr_outch,(char **bp, char *ep, int c));
_PROTOTYPE(static void dostr,(char **bp, char *ep, char *str, int));

# if	!defined(HAS_NO_LONG_LONG)
_PROTOTYPE(static void fmtllnum,(char **bp, char *ep, long long value,
				 int base, int dosign, int ljust, int len,
				 int zpad));
# endif	/* !defined(HAS_NO_LONG_LONG) */

_PROTOTYPE(static void fmtnum,(char **bp, char *ep, long value, int base,
			       int dosign, int ljust, int len, int zpad));
_PROTOTYPE(static void fmtstr,(char **bp, char *ep, char *value, int ljust,
			       int len, int zpad,
			       int maxwidth));


/*
 * Local variables
 */

static int Length;


/*
 * snpf() -- count-controlled sprintf()
 */

int
snpf(va_alist)
	va_dcl				/* requires at least three arguments:
					 *   bp =  receiving buffer pointer
					 *   ct =  length of buffer
					 *   fmt = format string
					 */
{
	va_list args;
	char *bp, *fmt;
	int ct, len;

	va_start(args);
	bp = va_arg(args, char *);
	ct = va_arg(args, int);
	fmt = va_arg(args, char *);
	len = vsnpf(bp, ct, fmt, args);
	va_end(args);
	return(len);
}


/*
 * vsnpf() -- count-controlled vsprintf()
 */

int
vsnpf(str, count, fmt, args)
	char *str;			/* result buffer */
	int count;			/* size of buffer */
	char *fmt;			/* format */
	va_list args;			/* variable length argument list */
{
	char *ep = str + count - 1;

	*str = '\0';
	(void) dopr(str, ep, fmt, args);
	if (count > 0)
	    *ep = '\0';
	return(Length);
}


/*
 * dopr() -- poor man's version of doprintf
 */


static void
dopr(bp, ep, fmt, args)
	char *bp;			/* buffer start */
	char *ep;			/* buffer end (start + length - 1) */
	char *fmt;			/* format */
	va_list args;			/* variable length argument list */
{
	int ch;
	char ebuf[64];
	int ebufl = (int)(sizeof(ebuf) - 1);
	long value;
	int longflag  = 0;
	int longlongflag  = 0;
	int pointflag = 0;
	int maxwidth  = 0;
	char *strvalue;
	int ljust;
	int len;
	int zpad;
	int zxflag = 0;

# if	!defined(HAS_NO_LONG_LONG)
	long long llvalue;
# endif	/* !defined(HAS_NO_LONG_LONG) */

	Length = 0;
	while((ch = *fmt++)) {
	    switch (ch) {
	    case '%':
		ljust = len = zpad = zxflag = maxwidth = 0;
		longflag = longlongflag = pointflag = 0;

nextch:

		ch = *fmt++;
		switch (ch) {
		case '\0':
		    dostr(&bp, ep, "**end of format**" , 0);
		    return;
		case '-':
		    ljust = 1;
		    goto nextch;
		case '0': /* set zero padding if len not set */
		    if ((len == 0) && !pointflag)
			zpad = '0';
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		    if (pointflag)
			maxwidth = (maxwidth * 10) + (int)(ch - '0');
		    else
			 len = (len * 10) + (int)(ch - '0');
		    goto nextch;
		case '*': 
		   if (pointflag)
			maxwidth = va_arg(args, int);
		    else
			len = va_arg(args, int);
		    goto nextch;
		case '#':
		    zxflag = 1;
		    goto nextch;
		case '.':
		    pointflag = 1;
		    goto nextch;
		case 'l':
		    if (longflag) {
			longflag = 0;
			longlongflag = 1;
			goto nextch;
		    }
		    longflag = 1;
		    goto nextch;
		case 'u':
		case 'U':
		    if (longlongflag) {

# if	!defined(HAS_NO_LONG_LONG)
			llvalue = va_arg(args, long long);
			(void) fmtllnum(&bp,ep,llvalue,10,0,ljust,len,zpad);
# else	/* defined(HAS_NO_LONG_LONG) */
			(void) strncpy(ebuf, "ll is unsupported", ebufl);
			ebuf[(int)ebufl] = '\0';
			(void) dostr(&bp, ep, ebuf, 0);
# endif	/* !defined(HAS_NO_LONG_LONG) */

			break;
		    }
		    if (longflag)
			value = va_arg(args, long);
		    else
			value = va_arg(args, int);
		    (void) fmtnum(&bp, ep, value, 10,0, ljust, len, zpad);
		    break;
		case 'o':
		case 'O':
		    if (longlongflag) {

# if	!defined(HAS_NO_LONG_LONG)
			llvalue = va_arg(args, long long);
			(void) fmtllnum(&bp,ep,llvalue,8,0,ljust,len,zpad);
# else	/* defined(HAS_NO_LONG_LONG) */
			(void) strncpy(ebuf, "ll is unsupported", ebufl);
			ebuf[(int)ebufl] = '\0';
			(void) dostr(&bp, ep, ebuf, 0);
# endif	/* !defined(HAS_NO_LONG_LONG) */

			break;
		    }
		    if (longflag)
			value = va_arg(args, long);
		    else
			value = va_arg(args, int);
		    (void) fmtnum(&bp, ep, value, 8,0, ljust, len, zpad);
		    break;
		case 'd':
		case 'D':
		    if (longlongflag) {

# if	!defined(HAS_NO_LONG_LONG)
			llvalue = va_arg(args, long long);
			(void) fmtllnum(&bp,ep,llvalue,10,1,ljust,len,zpad);
# else	/* defined(HAS_NO_LONG_LONG) */
			(void) strncpy(ebuf, "ll is unsupported", ebufl);
			ebuf[(int)ebufl] = '\0';
			(void) dostr(&bp, ep, ebuf, 0);
# endif	/* !defined(HAS_NO_LONG_LONG) */

			break;
		    }
		    if (longflag)
			value = va_arg(args, long);
		    else
			value = va_arg(args, int);
		    (void) fmtnum(&bp, ep, value, 10,1, ljust, len, zpad);
		    break;
		case 'x':
		    if (longlongflag) {

# if	!defined(HAS_NO_LONG_LONG)
			llvalue = va_arg(args, long long);
			if (zxflag && llvalue) {
			    (void) dostr(&bp, ep, "0x", 0);
			    if (len >= 2)
				len -= 2;
			}
			(void) fmtllnum(&bp,ep,llvalue,16,0,ljust,len,zpad);
# else	/* defined(HAS_NO_LONG_LONG) */
			(void) strncpy(ebuf, "ll is unsupported", ebufl);
			ebuf[(int)ebufl] = '\0';
			(void) dostr(&bp, ep, ebuf, 0);
# endif	/* !defined(HAS_NO_LONG_LONG) */

			break;
		    }
		    if (longflag)
			value = va_arg(args, long);
		    else
			value = va_arg(args, int);
		    if (zxflag && value) {
			(void) dostr(&bp, ep, "0x", 0);
			if (len >= 2)
			    len -= 2;
		    }
		    (void) fmtnum(&bp, ep, value, 16,0, ljust, len, zpad);
		    break;
		case 'X':
		    if (longlongflag) {

# if	!defined(HAS_NO_LONG_LONG)
			llvalue = va_arg(args, long long);
			if (zxflag && llvalue) {
			    (void) dostr(&bp, ep, "0x", 0);
			    if (len >= 2)
				len -= 2;
			}
			(void) fmtllnum(&bp,ep,llvalue,-16,0,ljust,len,zpad);
# else	/* defined(HAS_NO_LONG_LONG) */
			(void) strncpy(ebuf, "ll is unsupported", ebufl);
			ebuf[(int)ebufl] = '\0';
			(void) dostr(&bp, ep, ebuf, 0);
# endif	/* !defined(HAS_NO_LONG_LONG) */

			break;
		    }
		    if (longflag)
			value = va_arg(args, long);
		    else
			value = va_arg(args, int);
		    if (zxflag && value) {
			(void) dostr(&bp, ep, "0x", 0);
			if (len >= 2)
			    len -= 2;
		    }
		    (void) fmtnum(&bp, ep, value,-16,0, ljust, len, zpad);
		    break;
		case 's':
		    strvalue = va_arg(args, char *);
		    if (maxwidth > 0 || !pointflag) {
			if (pointflag && len > maxwidth)
			    len = maxwidth; /* Adjust padding */
			(void) fmtstr(&bp, ep, strvalue, ljust, len, zpad,
				      maxwidth);
		    }
		    break;
		case 'c':
		    ch = va_arg(args, int);
		    dopr_outch(&bp, ep, ch);
		    break;
		case '%':
		    (void) dopr_outch(&bp, ep, ch);
		    continue;
		default:
		    ebuf[0] = ch;
		    (void) strncpy(&ebuf[1], " is unsupported", ebufl);
		    ebuf[(int)ebufl] = '\0';
		    (void) dostr(&bp, ep, ebuf, 0);
		}
		break;
	    default:
		(void) dopr_outch(&bp, ep, ch);
		break;
	    }
	}
	*bp = '\0';
}


# if	!defined(HAS_NO_LONG_LONG)
/*
 * fmtllnum() -- format long long number for output
 */

static void
fmtllnum(bp, ep, value, base, dosign, ljust, len, zpad)
	char **bp;			/* current buffer pointer */
	char *ep;			/* end of buffer (-1) */
	long long value;		/* number to format */
	int base;			/* number base */
	int dosign;			/* sign request */
	int ljust;			/* left justfication request */
	int len;			/* length request */
	int zpad;			/* zero padding request */
{
	int signvalue = 0;
	unsigned long long uvalue;
	char convert[20];
	int place = 0;
	int padlen = 0; /* amount to pad */
	int caps = 0;

	uvalue = value;
	if (dosign) {
	    if (value < 0) {
		signvalue = '-';
		uvalue = -value;
	    }
	}
	if (base < 0) {
	    caps = 1;
	    base = -base;
	}
	do {
	    convert[place++] = 
		(caps ? "0123456789ABCDEF" : "0123456789abcdef")
		    [uvalue % (unsigned)base];
	    uvalue = (uvalue / (unsigned)base);
	} while (uvalue && (place < (int)(sizeof(convert) - 1)));
	convert[place] = 0;
	padlen = len - place;
	if (padlen < 0)
	    padlen = 0;
	if(ljust)
	    padlen = -padlen;
	if (zpad && padlen > 0) {
	    if (signvalue) {
		(void) dopr_outch(bp, ep, signvalue);
		--padlen;
		signvalue = 0;
	    }
	    while (padlen > 0) {
		(void) dopr_outch(bp, ep, zpad);
		--padlen;
	    }
	}
	while (padlen > 0) {
	    (void) dopr_outch(bp, ep, ' ');
	     --padlen;
	}
	if (signvalue)
	    (void) dopr_outch(bp, ep, signvalue);
	while (place > 0)
	    (void) dopr_outch(bp, ep, convert[--place]);
	while (padlen < 0) {
	    (void) dopr_outch(bp, ep, ' ');
	    ++padlen;
	}
}
# endif	/* !defined(HAS_NO_LONG_LONG) */


/*
 * fmtnum() -- format number for output
 */

static void
fmtnum(bp, ep, value, base, dosign, ljust, len, zpad)
	char **bp;			/* current buffer pointer */
	char *ep;			/* end of buffer (-1) */
	long value;			/* number to format */
	int base;			/* number base */
	int dosign;			/* sign request */
	int ljust;			/* left justfication request */
	int len;			/* length request */
	int zpad;			/* zero padding request */
{
	int signvalue = 0;
	unsigned long uvalue;
	char convert[20];
	int place = 0;
	int padlen = 0; /* amount to pad */
	int caps = 0;

	uvalue = value;
	if (dosign) {
	    if (value < 0) {
		signvalue = '-';
		uvalue = -value;
	    }
	}
	if (base < 0) {
	    caps = 1;
	    base = -base;
	}
	do {
	    convert[place++] = 
		(caps ? "0123456789ABCDEF" : "0123456789abcdef")
		    [uvalue % (unsigned)base];
	    uvalue = (uvalue / (unsigned)base);
	} while (uvalue && (place < (int)(sizeof(convert) - 1)));
	convert[place] = 0;
	padlen = len - place;
	if (padlen < 0)
	    padlen = 0;
	if(ljust)
	    padlen = -padlen;
	if (zpad && padlen > 0) {
	    if (signvalue) {
		(void) dopr_outch(bp, ep, signvalue);
		--padlen;
		signvalue = 0;
	    }
	    while (padlen > 0) {
		(void) dopr_outch(bp, ep, zpad);
		--padlen;
	    }
	}
	while (padlen > 0) {
	    (void) dopr_outch(bp, ep, ' ');
	     --padlen;
	}
	if (signvalue)
	    (void) dopr_outch(bp, ep, signvalue);
	while (place > 0)
	    (void) dopr_outch(bp, ep, convert[--place]);
	while (padlen < 0) {
	    (void) dopr_outch(bp, ep, ' ');
	    ++padlen;
	}
}


/*
 * fmtstr() -- format string for output
 */

static void
fmtstr(bp, ep, value, ljust, len, zpad, maxwidth)
	char **bp;			/* current buffer pointer */
	char *ep;			/* end of buffer (-1) */
	char *value;			/* string to format */
	int ljust;			/* left justification request */
	int len;			/* length request */
	int zpad;			/* zero padding request */
	int maxwidth;			/* maximum width request */
{
	int padlen, strlen;     /* amount to pad */

	if (value == 0)
	    value = "<NULL>";
	for (strlen = 0; value[strlen]; ++ strlen)	/* strlen() */
	    ;
	if ((strlen > maxwidth) && maxwidth)
	    strlen = maxwidth;
	padlen = len - strlen;
	if (padlen < 0)
	    padlen = 0;
	if (ljust)
	    padlen = -padlen;
	while (padlen > 0) {
	    (void) dopr_outch(bp, ep, ' ');
	    --padlen;
	}
	(void) dostr(bp, ep, value, maxwidth);
	while (padlen < 0) {
	    (void) dopr_outch(bp, ep, ' ');
	    ++padlen;
	}
}


/*
 * dostr() -- do string output
 */

static void
dostr(bp, ep, str, cut)
	char **bp;			/* current buffer pointer */
	char *ep;			/* end of buffer (-1) */
	char *str;			/* string to output */
	int cut;			/* limit on amount of string to output:
					 *   0 == no limit */
{
	int f;

	f = cut ? 1 : 0;
	while (*str) {
	    if (f) {
		if (cut-- > 0)
		    (void) dopr_outch(bp, ep, *str);
	    } else
		(void) dopr_outch(bp, ep, *str);
	    str++;
	}
}


/*
 * dopr_outch() -- output a character (or two)
 */

static void
dopr_outch(bp, ep, c)
	char **bp;			/* current buffer pointer */
	char *ep;			/* end of buffer (-1) */
	int c;				/* character to output */
{
	register char *cp = *bp;

	if (iscntrl(c) && c != '\n' && c != '\t') {
	    c = '@' + (c & 0x1F);
	    if (cp < ep)
		*cp++ = '^';
	    Length++;
	}
	if (cp < ep)
	    *cp++ = c;
	*bp = cp;
	Length++;
}

#else	/* !defined(USE_LIB_SNPF) */
char snpf_d1[] = "d"; char *snpf_d2 = snpf_d1;
#endif	/* defined(USE_LIB_SNPF) */

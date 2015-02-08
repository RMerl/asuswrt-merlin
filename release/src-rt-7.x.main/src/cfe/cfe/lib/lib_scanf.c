/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Reentrant string scan function			File: lib_scanf.c
    *  
    *  This routine is used to scan formatted patterns of a given
    *  string.
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

#include <stdarg.h>
#include "lib_types.h"
#include "lib_scanf.h"

#define GETAP(ap) \
    ap = (char *) va_arg(args,char *);

static int
match(char *fmt, char cc)
{
    int exc = 0;
    char *cp1;

    if (!cc)
	return 0;
    if (*fmt == '^') {
	exc = 1;
	fmt++;
    }

    for (cp1 = fmt; *cp1 && *cp1 != ']';) {
	if (cp1[1] == '-' && cp1[2] > cp1[0]) {
	    if (cc >= cp1[0] && cc <= cp1[2])
		return exc^1;
	    cp1 += 3;
	}
	else {
	    if (cc == *cp1)
		return exc^1;
	    cp1++;
	}
    }
    return exc;
}

static long
asclng(char **cp, int width, int base)
{
    long answer;
    char cc, sign, *cp1;

    answer = sign = 0;
    for (cp1=*cp; *cp1; cp1++) {
	if (*cp1 > ' ')
	    break;
    }
    if (*cp1 == '-' || *cp1 == '+')
	sign = *cp1++, width--;
    if (!*cp1 || !width)
	goto exi1;
    if (!base) {
	base = 10;
	if (*cp1 == '0') {
	    base = 8;
	    goto lab4;
	}
    }
    else if (base == 16) {
	if (*cp1 == '0') {
lab4:	    cp1++, width--;
	    if (width > 0) {
		if (*cp1 == 'x' || *cp1 == 'X')
		    base = 16, cp1++, width--;
	    }
	}
    }
    for (; width && *cp1; cp1++,width--) {
	if ((cc = *cp1) < '0')
	    break;
        if (cc <= '9')
	    cc &= 0xf;
        else {
	    cc &= 0xdf;
	    if (cc >= 'A')
	        cc -= 'A' - 10;
        }
        if (cc >= base)
	    break;
	answer = base * answer + cc;
    }

exi1:
    *cp = cp1;
    if (sign == '-')
	answer = -answer;
    return answer;
}

/*
 * Reentrant Scan Routine
 *	entry:		&input buffer
 *			&format string
 *			&argument list
 *	exit:		return value is number of fields scanned
 */
int
lib_sscanf(char *buf, char *fmt, ...)
{
    int field;		/* field flag: 0 = background, 1 = %field */
    int sizdef;		/* size: 0 = default, 1 = short, 2 = long, 3 = long double */
    int width;		/* field width */
    int par1;
    long l1;
    int nfields;
    char fch;
    char *orgbuf, *prebuf;
    char *ap;
    va_list args;

    va_start(args,fmt);	/* get variable arg list address */
    if (!*buf)
	return -1;
    nfields = field = sizdef = 0;
    orgbuf = buf;
    while ((fch = *fmt++) != 0) {
	if (!field) {
	    if (fch == '%') {
		if (*fmt != '%') {
		    field = 1;
		    continue;
		}
		fch = *fmt++;
	    }
	    if (fch <= ' ')
		for (; *buf== ' '||*buf=='\t'; buf++) ;
	    else if (fch == *buf)
		buf++;
	}
	else {
	    width = 0x7fff;
	    if (fch == '*') {
		width = va_arg(args,int);
		goto lab6;
	    }
	    else if (fch >= '0' && fch <= '9') {
		fmt--;
		width = asclng(&fmt,9,10);
lab6:		fch = *fmt++;
	    }
	    if (fch == 'h') {
		sizdef = 1;
		goto lab7;
	    }
	    else if (fch == 'l') {
		sizdef = 2;
lab7:		fch = *fmt++;
	    }

	    prebuf = buf;
            switch (fch) {
	    case 'd':		/* signed integer */
		par1 = 10;
		goto lab3;
	    case 'o':		/* signed integer */
		par1 = 8;
		goto lab3;
	    case 'x':		/* signed integer */
	    case 'X':		/* long signed integer */
		par1 = 16;
		goto lab3;
	    case 'u':		/* unsigned integer */
	    case 'i':		/* signed integer */
		par1 = 0;
lab3:		GETAP(ap);
		l1 = asclng(&buf, width, par1);
		if (prebuf == buf)
		    break;
		if (sizdef == 2)
		    *(long *)ap = l1;
		else if (sizdef == 1)
		    *(short *)ap = l1;
		else
		    *(int *)ap = l1;
		goto lab12;
	    case 'c':		/* character */
		GETAP(ap);
		for (; width && *buf; width--) {
		    *ap++ = *buf++;
		    if (width == 0x7fff)
			break;
		}
		goto lab12;
	    case '[':		/* search set */
		GETAP(ap);
		for (; width && match(fmt,*buf); width--)
		    *ap++ = *buf++;
		while (*fmt++ != ']');
		goto lab11;
	    case 's':		/* character array */
		GETAP(ap);
		for (; *buf==' '||*buf==0x07; buf++) ;
		for (; width && *buf && *buf>' '; width--)
		    *ap++ = *buf++;
lab11:		if (prebuf == buf)
		    break;
		*(char *)ap = 0;
		goto lab12;
	    case 'n':		/* store # chars */
		GETAP(ap);
		*(int *)ap = buf - orgbuf;
		break;
	    case 'p':		/* pointer */
		GETAP(ap);
		*(long *)ap = asclng(&buf, width, 16);
lab12:		nfields++;
		break;
	    default:		/* illegal */
		goto term;
	    }
	    field = 0;
	}
	if (!fch)
	    break;
    }
term:
    return nfields;
}

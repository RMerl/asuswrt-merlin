/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  simple printf				File: lib_printf.c
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *  This module contains a very, very, very simple printf
    *  suitable for use in the boot ROM.
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
#include "lib_printf.h"

/*  *********************************************************************
    *  Externs								*
    ********************************************************************* */

/*  *********************************************************************
    *  Globals								*
    ********************************************************************* */

static const char digits[17] = "0123456789ABCDEF";
static const char ldigits[17] = "0123456789abcdef";

int (*xprinthook)(const char *str) = NULL;

/*  *********************************************************************
    *  __atox(buf,num,radix,width)
    *
    *  Convert a number to a string	
    * 
    *  Input Parameters: 
    *      buf - where to put characters
    *      num - number to convert	
    *      radix - radix to convert number to (usually 10 or 16)
    *      width - width in characters	
    * 
    *  Return Value:	
    *      number of digits placed in output buffer
    ********************************************************************* */
static int __atox(char *buf,unsigned int num,unsigned int radix,int width,
		     const char *digits)
{
    char buffer[16];
    char *op;
    int retval;

    op = &buffer[0];
    retval = 0;

    do {
	*op++ = digits[num % radix];
	retval++;
	num /= radix;
	} while (num != 0);

    if (width && (width > retval)) {
	width = width - retval;
	while (width) {
	    *op++ = '0';
	    retval++;
	    width--;
	    }
	}

    while (op != buffer) {
	op--;
	*buf++ = *op;
	}

    return retval;
}


/*  *********************************************************************
    *  __llatox(buf,num,radix,width)
    *  	
    *  Convert a long number to a string
    * 
    *  Input Parameters:
    *      buf - where to put characters	
    *      num - number to convert	
    *      radix - radix to convert number to (usually 10 or 16)
    *      width - width in characters	
    *      
    *  Return Value:	
    *      number of digits placed in output buffer
    ********************************************************************* */
static int __llatox(char *buf,unsigned long long num,unsigned int radix,
		    int width,const char *digits)
{
    char buffer[16];
    char *op;
    int retval;

    op = &buffer[0];
    retval = 0;

#ifdef _MIPSREGS32_
    /* 
     * Hack: to avoid pulling in the helper library that isn't necessarily
     * compatible with PIC code, force radix to 16, use shifts and masks 
     */
    do {
	*op++ = digits[num & 0x0F];
	retval++;
	num >>= 4;
	} while (num != 0);
#else
    do {
	*op++ = digits[num % radix];
	retval++;
	num /= radix;
	} while (num != 0);
#endif

    if (width && (width > retval)) {
	width = width - retval;
	while (width) {
	    *op++ = '0';
	    retval++;
	    width--;
	    }
	}

    while (op != buffer) {
	op--;
	*buf++ = *op;
	}

    return retval;
}

/*  *********************************************************************
    *  xvsprintf(outbuf,template,arglist)
    *  			
    *  Format a string into the output buffer	
    *      
    *  Input Parameters: 
    *	   outbuf - output buffer	
    *      template - template string	
    *      arglist - parameters	
    *  	
    *  Return Value:
    *      number of characters copied	
    ********************************************************************* */
#define isdigit(x) (((x) >= '0') && ((x) <= '9'))
int xvsprintf(char *outbuf,const char *templat,va_list marker)
{
    char *optr;
    const char *iptr;
    unsigned char *tmpptr;
    unsigned int x;
    unsigned long long lx;
    int i;
    long long ll;
    int leadingzero;
    int leadingnegsign;
    int islong;
    int width;
    int width2 = 0;
    int hashash = 0;

    optr = outbuf;
    iptr = templat;

    while (*iptr) {
	if (*iptr != '%') {*optr++ = *iptr++; continue;}

	iptr++;

	if (*iptr == '#') { hashash = 1; iptr++; }
	if (*iptr == '-') {
	    leadingnegsign = 1;
	    iptr++;
	    }
	else leadingnegsign = 0;

	if (*iptr == '0') leadingzero = 1;
	else leadingzero = 0;

	width = 0;
	while (*iptr && isdigit(*iptr)) {
	    width += (*iptr - '0');
	    iptr++;
	    if (isdigit(*iptr)) width *= 10;
	    }
	if (*iptr == '.') {
	    iptr++;
	    width2 = 0;
	    while (*iptr && isdigit(*iptr)) {
		width2 += (*iptr - '0');
		iptr++;
		if (isdigit(*iptr)) width2 *= 10;
		}
	    }

	islong = 0;
	if (*iptr == 'l') { islong++; iptr++; }
	if (*iptr == 'l') { islong++; iptr++; }

	switch (*iptr) {
	    case 'I':
		tmpptr = (unsigned char *) va_arg(marker,unsigned char *);
		optr += __atox(optr,*tmpptr++,10,0,digits);
		*optr++ = '.';
		optr += __atox(optr,*tmpptr++,10,0,digits);
		*optr++ = '.';
		optr += __atox(optr,*tmpptr++,10,0,digits);
		*optr++ = '.';
		optr += __atox(optr,*tmpptr++,10,0,digits);
		break;
	    case 's':
		tmpptr = (unsigned char *) va_arg(marker,unsigned char *);
		if (!tmpptr) tmpptr = (unsigned char *) "(null)";
		if ((width == 0) & (width2 == 0)) {
		    while (*tmpptr) *optr++ = *tmpptr++;
		    break;
		    }
		while (width && *tmpptr) {
		    *optr++ = *tmpptr++;
		    width--;
		    }
		while (width) {
		    *optr++ = ' ';
		    width--;
		    }
		break;
	    case 'a':
		tmpptr = (unsigned char *) va_arg(marker,unsigned char *);
		for (x = 0; x < 5; x++) {
		    optr += __atox(optr,*tmpptr++,16,2,digits);
		    *optr++ = '-';
		    }
		optr += __atox(optr,*tmpptr++,16,2,digits);
		break;
	    case 'd':
		switch (islong) {
		    case 0:
		    case 1:
			i = va_arg(marker,int);
			if (i < 0) { *optr++='-'; i = -i;}
			optr += __atox(optr,i,10,width,digits);
			break;
		    case 2:
			ll = va_arg(marker,long long int);
			if (ll < 0) { *optr++='-'; ll = -ll;}
			optr += __llatox(optr,ll,10,width,digits);
			break;
		    }
		break;
	    case 'u':
		switch (islong) {
		    case 0:
		    case 1:
			x = va_arg(marker,unsigned int);
			optr += __atox(optr,x,10,width,digits);
			break;
		    case 2:
			lx = va_arg(marker,unsigned long long);
			optr += __llatox(optr,lx,10,width,digits);
			break;
		    }
		break;
	    case 'X':
	    case 'x':
		switch (islong) {
		    case 0:
		    case 1:
			x = va_arg(marker,unsigned int);
			optr += __atox(optr,x,16,width,
				       (*iptr == 'X') ? digits : ldigits);
			break;
		    case 2:
			lx = va_arg(marker,unsigned long long);
			optr += __llatox(optr,lx,16,width,
				       (*iptr == 'X') ? digits : ldigits);
			break;
		    }
		break;
	    case 'p':
	    case 'P':
#ifdef __long64		
		lx = va_arg(marker,unsigned long long);
		optr += __llatox(optr,lx,16,16,
				 (*iptr == 'P') ? digits : ldigits);
#else
		x = va_arg(marker,unsigned int);
		optr += __atox(optr,x,16,8,
			       (*iptr == 'P') ? digits : ldigits);
#endif
		break;
	    case 'w':
		x = va_arg(marker,unsigned int);
	        x &= 0x0000FFFF;
		optr += __atox(optr,x,16,4,digits);
		break;
	    case 'b':
		x = va_arg(marker,unsigned int);
	        x &= 0x0000FF;
		optr += __atox(optr,x,16,2,digits);
		break;
	    case 'Z':
		x = va_arg(marker,unsigned int);
		tmpptr = va_arg(marker,unsigned char *);
		while (x) {
		    optr += __atox(optr,*tmpptr++,16,2,digits);
		    x--;
		    }
		break;
	    case 'c':
		x = va_arg(marker, int);
		*optr++ = x & 0xff;
		break;

	    default:
		*optr++ = *iptr;
		break;
	    }
	iptr++;
	}

    *optr = '\0';

    return (optr - outbuf);
}


/*  *********************************************************************
    *  xsprintf(buf,template,params..)
    *  		
    *  format messages from template into a buffer.	
    *  	
    *  Input Parameters: 	
    *      buf - output buffer	
    *      template - template string
    *      params... parameters	
    *      	
    *  Return Value:	
    *      number of bytes copied to buffer
    ********************************************************************* */
int xsprintf(char *buf,const char *templat,...)
{
    va_list marker;
    int count;

    va_start(marker,templat);
    count = xvsprintf(buf,templat,marker);
    va_end(marker);

    return count;
}

/*  *********************************************************************
    *  xprintf(template,...)
    *  	
    *  A miniature printf.	
    *  	
    *      %a - Ethernet address (16 bytes)	
    *      %s - unpacked string, null terminated
    *      %x - hex word (machine size)	
    *      %w - hex word (16 bits)
    *      %b - hex byte (8 bits)
    *      %Z - buffer (put length first, then buffer address)
    *  	
    *  Return value:	
    *  	   number of bytes written	
    ********************************************************************* */

int xprintf(const char *templat,...)
{
    va_list marker;
    int count;
    char buffer[512];

    va_start(marker,templat);
    count = xvsprintf(buffer,templat,marker);
    va_end(marker);

    if (xprinthook) (*xprinthook)(buffer);

    return count;
}


int xvprintf(const char *templat,va_list marker)
{
    int count;
    char buffer[512];

    count = xvsprintf(buffer,templat,marker);

    if (xprinthook) (*xprinthook)(buffer);

    return count;
}

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/**************************************************************************
 * Copyright 1994-2003 Patrick Powell, San Diego, CA <papowell@lprng.com>
 **************************************************************************/

/*
   Overview:

   plp_snprintf( char *buffer, int len, const char *format,...)
   plp_unsafe_snprintf( char *buffer, int len, const char *format,...)
     its horribly unsafe companion that does NOT protect you from
     the printing of evil control characters,  but may be necessary
     See the man page documentation below
   
   This version of snprintf was developed originally for printing
   on a motley collection of specialized hardware that had NO IO
   library.  Due to contractual restrictions,  a clean room implementation
   of the printf() code had to be developed.
   
   The method chosen for printf was to be as paranoid as possible,
   as these platforms had NO memory protection,  and very small
   address spaces.  This made it possible to try to print
   very long strings, i.e. - all of memory, very easily.  To guard
   against this,  all printing was done via a buffer, generous enough
   to hold strings,  but small enough to protect against overruns,
   etc.
   
   Strangely enough,  this proved to be of immense importance when
   SPRINTFing to a buffer on a stack...  The rest,  of course,  is
   well known,  as buffer overruns in the stack are a common way to
   do horrible things to operating systems, security, etc etc.
   
   This version of snprintf is VERY limited by modern standards.

   Revision History:
   First Released Version - 1994.  This version had NO comments.
   First Released Version - 1994.  This version had NO comments.
   Second Major Released Version - Tue May 23 10:43:44 PDT 2000
    Configuration and other items changed.  Read this doc.
    Treat this as a new version.
   Minor Revision - Mon Apr  1 09:41:28 PST 2002
     - fixed up some constants and casts 
   
   COPYRIGHT AND TERMS OF USE:
   
   You may use, copy, distribute, or otherwise incorporate this software
   and documentation into any product or other item,  provided that
   the copyright in the documentation and source code as well as the
   source code generated constant strings in the object, executable
   or other code remain in place and are present in executable modules
   or objects.
   
   You may modify this code as appropriate to your usage; however the
   modified version must be identified by changing the various source
   and object code identification strings as is appropriately noted
   in the source code.
   
   You can use this with the GNU CONFIGURE utility.
   This should define the following macros appropriately:
   
   HAVE_STDARG_H - if the <stdargs.h> include file is available
   HAVE_VARARG_H - if the <varargs.h> include file is available
   
   HAVE_STRERROR - if the strerror() routine is available.
     If it is not available, then examine the lines containing
     the tests below.

   HAVE_SYS_ERRLIST  - have sys_errlist available
   HAVE_DECL_SYS_ERRLIST - sys_errlist declaration in include files
   HAVE_SYS_NERR - have sys_nerr available
   HAVE_DECL_SYS_NERR -  sys_nerr declaration in include files
   
   HAVE_QUAD_T      - if the quad_t type is defined
   HAVE_LONG_LONG   - if the long long type is defined
   HAVE_LONG_DOUBLE - if the long double type is defined
   
     If you are using the GNU configure (autoconf) facility, add the
     following line to the configure.in file, to force checking for the
     quad_t and long long  data types:


    AC_CHECK_HEADERS(stdlib.h,stdio.h,unistd.h,errno.h)
	AC_CHECK_FUNCS(strerror)
	AC_CACHE_CHECK(for errno,
	ac_cv_errno,
	[
	AC_TRY_LINK(,[extern int errno; return (errno);],
		ac_cv_errno=yes, ac_cv_errno=no)
	])
	if test "$ac_cv_errno" = yes; then
		AC_DEFINE(HAVE_ERRNO)
		AC_CACHE_CHECK(for errno declaration,
		ac_cv_decl_errno,
		[
		AC_TRY_COMPILE([
		#include <stdio.h>
		#ifdef HAVE_STDLIB_H
		#include <stdlib.h>
		#endif
		#ifdef HAVE_UNISTD_H
		#include <unistd.h>
		#endif
		#ifdef HAVE_ERRNO_H
		#include <errno.h>
		],[return(sys_nerr);],
			ac_cv_decl_errno=yes, ac_cv_decl_errno=no)
		])
		if test "$ac_cv_decl_errno" = yes; then
			AC_DEFINE(HAVE_DECL_ERRNO)
		fi;
	fi

	AC_CACHE_CHECK(for sys_nerr,
	ac_cv_sys_nerr,
	[
	AC_TRY_LINK(,[extern int sys_nerr; return (sys_nerr);],
		ac_cv_sys_nerr=yes, ac_cv_sys_nerr=no)
	])
	if test "$ac_cv_sys_nerr" = yes; then
		AC_DEFINE(HAVE_SYS_NERR)
		AC_CACHE_CHECK(for sys_nerr declaration,
		ac_cv_decl_sys_nerr,
		[
		AC_TRY_COMPILE([
		#include <stdio.h>
		#ifdef HAVE_STDLIB_H
		#include <stdlib.h>
		#endif
		#ifdef HAVE_UNISTD_H
		#include <unistd.h>
		#endif],[return(sys_nerr);],
		ac_cv_decl_sys_nerr_def=yes, ac_cv_decl_sys_nerr_def=no)
		])
		if test "$ac_cv_decl_sys_nerr" = yes; then
			AC_DEFINE(HAVE_DECL_SYS_NERR)
		fi
	fi


	AC_CACHE_CHECK(for sys_errlist array,
	ac_cv_sys_errlist,
	[AC_TRY_LINK(,[extern char *sys_errlist[];
		sys_errlist[0];],
		ac_cv_sys_errlist=yes, ac_cv_sys_errlist=no)
	])
	if test "$ac_cv_sys_errlist" = yes; then
		AC_DEFINE(HAVE_SYS_ERRLIST)
		AC_CACHE_CHECK(for sys_errlist declaration,
		ac_cv_sys_errlist_def,
		[AC_TRY_COMPILE([
		#include <stdio.h>
		#include <errno.h>
		#ifdef HAVE_STDLIB_H
		#include <stdlib.h>
		#endif
		#ifdef HAVE_UNISTD_H
		#include <unistd.h>
		#endif],[char *s = sys_errlist[0]; return(*s);],
		ac_cv_decl_sys_errlist=yes, ac_cv_decl_sys_errlist=no)
		])
		if test "$ac_cv_decl_sys_errlist" = yes; then
			AC_DEFINE(HAVE_DECL_SYS_ERRLIST)
		fi
	fi



	AC_CACHE_CHECK(checking for long long,
	ac_cv_long_long,
	[
	AC_TRY_COMPILE([
	#include <stdio.h>
	#include <sys/types.h>
	], [printf("%d",sizeof(long long));],
	ac_cv_long_long=yes, ac_cv_long_long=no)
	])
	if test $ac_cv_long_long = yes; then
	  AC_DEFINE(HAVE_LONG_LONG)
	fi

	AC_CACHE_CHECK(checking for long double,
	ac_cv_long_double,
	[
	AC_TRY_COMPILE([
	#include <stdio.h>
	#include <sys/types.h>
	], [printf("%d",sizeof(long double));],
	ac_cv_long_double=yes, ac_cv_long_double=no)
	])
	if test $ac_cv_long_double = yes; then
	  AC_DEFINE(HAVE_LONG_DOUBLE)
	fi

	AC_CACHE_CHECK(checking for quad_t,
	ac_cv_quad_t,
	[
	AC_TRY_COMPILE([
	#include <stdio.h>
	#include <sys/types.h>
	], [printf("%d",sizeof(quad_t));],
	ac_cv_quad_t=yes, ac_cv_quad_t=no)
	])
	if test $ac_cv_quad_t = yes; then
	  AC_DEFINE(HAVE_QUAD_T)
	fi



 NAME
     plp_snprintf, plp_vsnprintf - formatted output conversion

 SYNOPSIS
     #include <stdio.h>
     #include <stdarg.h>

     int
     plp_snprintf(const char *format, size_t size, va_list ap);
     int
     plp_unsafe_snprintf(const char *format, size_t size, va_list ap);

     AKA snprintf and unsafe_snprintf in the documentation below

     int
     vsnprintf(char *str, size_t size, const char *format, va_list ap);
     int
     unsafe_vsnprintf(char *str, size_t size, const char *format, va_list ap);

     AKA vsnprintf and unsafe_vsnprintf in the documentation below

     (Multithreaded Safe)

 DESCRIPTION
     The printf() family of functions produces output according to
     a format as described below.  Snprintf(), and vsnprintf()
     write to the character string str. These functions write the
     output under the control of a format string that specifies
     how subsequent arguments (or arguments accessed via the
     variable-length argument facilities of stdarg(3))  are converted
     for output.  These functions return the number of characters
     printed (not including the trailing `\0' used to end output
     to strings).  Snprintf() and vsnprintf() will write at most
     size-1 of the characters printed into the output string (the
     size'th character then gets the terminating `\0'); if the
     return value is greater than or equal to the size argument,
     the string was too short and some of the printed characters
     were discarded.  The size or str may be given as zero to find
     out how many characters are needed; in this case, the str
     argument is ignored.

     By default, the snprintf function will not format control
     characters (except new line and tab) in strings.  This is a
     safety feature that has proven to be extremely critical when
     using snprintf for secure applications and when debugging.
     If you MUST have control characters formatted or printed,
     then use the unsafe_snprintf() and unsafe_vsnprintf() and on
     your own head be the consequences.  You have been warned.

     There is one exception to the comments above, and that is
     the "%c" (character) format.  It brutally assumes that the
     user will have performed the necessary 'isprint()' or other
     checks and uses the integer value as a character.

     The format string is composed of zero or more directives:
     ordinary characters (not %), which are copied unchanged to
     the output stream; and conversion specifications, each
     of which results in fetching zero or more subsequent arguments.
     Each conversion specification is introduced by the character
     %. The arguments must correspond properly (after type promotion)
     with the conversion specifier.  After the %, the following
     appear in sequence:

     o   Zero or more of the following flags:

	   -   A zero `0' character specifying zero padding.  For
	     all conversions except n, the converted value is padded
	     on the left with zeros rather than blanks.  If a
	     precision is given with a numeric conversion (d, i,
	     o, u, i, x, and X), the `0' flag is ignored.

       -   A negative field width flag `-' indicates the converted
	     value is to be left adjusted on the field boundary.  Except
	     for n conversions, the converted value is padded on
	     the right with blanks, rather than on the left with
	     blanks or zeros.  A `-' overrides a `0' if both are
	     given.

	   -   A space, specifying that a blank should be left before
	     a positive number produced by a signed conversion (d, e, E, f,
	     g, G, or i).

	   -   A `+' character specifying that a sign always be placed
	     before a number produced by a signed conversion.  A `+' overrides
	     a space if both are used.

     o   An optional decimal digit string specifying a minimum
		 field width.  If the converted value has fewer
		 characters than the field width, it will be padded
		 with spaces on the left (or right, if the
		 left-adjustment flag has been given) to fill out
		 the field width.

     o   An optional precision, in the form of a period `.' followed
		 by an optional digit string.  If the digit string
		 is omitted, the precision is taken as zero.  This
		 gives the minimum number of digits to appear for
		 d, i, o, u, x, and X conversions, the number of
		 digits to appear after the decimal-point for e,
		 E, and f conversions, the maximum number of
		 significant digits for g and G conversions, or
		 the maximum number of characters to be printed
		 from a string for s conversions.

     o   The optional character h, specifying that a following d,
		 i, o, u, x, or X conversion corresponds to a short
		 int or unsigned short int argument, or that a
		 following n conversion corresponds to a pointer
		 to a short int argument.

     o   The optional character l (ell) specifying that a following
		 d, i, o, u, x, or X conversion applies to a pointer
		 to a long int or unsigned long int argument, or
		 that a following n conversion corresponds to a
		 pointer to a long int argument.

     o   The optional character q, specifying that a following d,
		 i, o, u, x, or X conversion corresponds to a quad_t
		 or u_quad_t argument, or that a following n
		 conversion corresponds to a quad_t argument.
         This value is always printed in HEX notation.  Tough.
         quad_t's are an OS system implementation, and should
         not be allowed.

     o   The character L specifying that a following e, E, f, g,
		 or G conversion corresponds to a long double
		 argument.

     o   A character that specifies the type of conversion to be applied.


     A field width or precision, or both, may be indicated by an asterisk `*'
     instead of a digit string.  In this case, an int argument supplies the
     field width or precision.  A negative field width is treated as a left
     adjustment flag followed by a positive field width; a negative precision
     is treated as though it were missing.

     The conversion specifiers and their meanings are:

     diouxX  The int (or appropriate variant) argument is converted to signed
			 decimal (d and i), unsigned octal (o), unsigned decimal
			 (u), or unsigned hexadecimal (x and X) notation.  The
			 letters abcdef are used for x conversions; the letters
			 ABCDEF are used for X conversions.  The precision, if
			 any, gives the minimum number of digits that must
			 appear; if the converted value requires fewer digits,
			 it is padded on the left with zeros.

     eE      The double argument is rounded and converted in the style
             [-]d.ddde+-dd where there is one digit before the decimal-point
			 character and the number of digits after it is equal
			 to the precision; if the precision is missing, it is
			 taken as 6; if the precision is zero, no decimal-point
			 character appears.  An E conversion uses the letter
			 E (rather than e) to introduce the exponent.
			 The exponent always contains at least two digits; if
			 the value is zero, the exponent is 00.

     f       The double argument is rounded and converted to decimal notation
             in the style [-]ddd.ddd, where the number of digits after the
             decimal-point character is equal to the precision specification.
             If the precision is missing, it is taken as 6; if the precision
             is explicitly zero, no decimal-point character appears.  If a
             decimal point appears, at least one digit appears before it.

     g       The double argument is converted in style f or e (or
			 E for G conversions).  The precision specifies the
			 number of significant digits.  If the precision is
			 missing, 6 digits are given; if the precision is zero,
			 it is treated as 1.  Style e is used if the exponent
			 from its conversion is less than -4 or greater than
			 or equal to the precision.  Trailing zeros are removed
			 from the fractional part of the result; a decimal
			 point appears only if it is followed by at least one
			 digit.

     c       The int argument is converted to an unsigned char,
             and the resulting character is written.

     s       The ``char *'' argument is expected to be a pointer to an array
			 of character type (pointer to a string).  Characters
			 from the array are written up to (but not including)
			 a terminating NUL character; if a precision is
			 specified, no more than the number specified are
			 written.  If a precision is given, no null character
			 need be present; if the precision is not specified,
			 or is greater than the size of the array, the array
			 must contain a terminating NUL character.

     %       A `%' is written. No argument is converted. The complete
             conversion specification is `%%'.

     In no case does a non-existent or small field width cause truncation of a
     field; if the result of a conversion is wider than the field width, the
     field is expanded to contain the conversion result.

 EXAMPLES
     To print a date and time in the form `Sunday, July 3, 10:02', where
     weekday and month are pointers to strings:

           #include <stdio.h>
           fprintf(stdout, "%s, %s %d, %.2d:%.2d\n",
                   weekday, month, day, hour, min);

     To print pi to five decimal places:

           #include <math.h>
           #include <stdio.h>
           fprintf(stdout, "pi = %.5f\n", 4 * atan(1.0));

     To allocate a 128 byte string and print into it:

           #include <stdio.h>
           #include <stdlib.h>
           #include <stdarg.h>
           char *newfmt(const char *fmt, ...)
           {
               char *p;
               va_list ap;
               if ((p = malloc(128)) == NULL)
                       return (NULL);
               va_start(ap, fmt);
               (void) vsnprintf(p, 128, fmt, ap);
               va_end(ap);
               return (p);
           }

 SEE ALSO
     printf(1),  scanf(3)

 STANDARDS
     Turkey C Standardization and wimpy POSIX folks did not define
     snprintf or vsnprintf().

 BUGS
     The conversion formats %D, %O, and %U are not standard and are provided
     only for backward compatibility.  The effect of padding the %p format
     with zeros (either by the `0' flag or by specifying a precision), and the
     benign effect (i.e., none) of the `#' flag on %n and %p conversions, as
     well as other nonsensical combinations such as %Ld, are not standard;
     such combinations should be avoided.

     The typedef names quad_t and u_quad_t are infelicitous.

*/


#include "config.h"
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#if defined(HAVE_STRING_H)
# include <string.h>
#endif
#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif
#if defined(HAVE_ERRNO_H)
#include <errno.h>
#endif

/*
 * For testing, define these values
 */
#if 0
#define HAVE_STDARG_H 1
#define TEST 1
#define HAVE_QUAD_T 1
#endif

/**** ENDINCLUDE ****/

/*************************************************
 * KEEP THIS STRING - MODIFY AT THE END WITH YOUR REVISIONS
 * i.e. - the LOCAL REVISIONS part is for your use
 *************************************************/
 
 
 static char *const _id = "plp_snprintf V2000.08.18 Copyright Patrick Powell 1988-2000 "
 "$Id: plp_snprintf.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $"
 " LOCAL REVISIONS: <NONE>";

/* varargs declarations: */

# undef HAVE_STDARGS    /* let's hope that works everywhere (mj) */
# undef VA_LOCAL_DECL
# undef VA_START
# undef VA_SHIFT
# undef VA_END

#if defined(HAVE_STDARG_H)
# include <stdarg.h>
# define HAVE_STDARGS    /* let's hope that works everywhere (mj) */
# define VA_LOCAL_DECL   va_list ap;
# define VA_START(f)     va_start(ap, f)
# define VA_SHIFT(v,t)	;	/* no-op for ANSI */
# define VA_END          va_end(ap)
#else
# if defined(HAVE_VARARGS_H)
#  include <varargs.h>
#  undef HAVE_STDARGS
#  define VA_LOCAL_DECL   va_list ap;
#  define VA_START(f)     va_start(ap)		/* f is ignored! */
#  define VA_SHIFT(v,t)	v = va_arg(ap,t)
#  define VA_END		va_end(ap)
# else
 XX ** NO VARARGS ** XX
# endif
#endif

 union value {
#if defined(HAVE_QUAD_T)
	quad_t qvalue;
#endif
#if defined(HAVE_LONG_LONG)
	long long value;
#else
	long value;
#endif
	double dvalue;
};

#undef CVAL 
#define CVAL(s) (*((unsigned char *)s))
#define safestrlen(s) ((s)?strlen(s):0)


 static char * plp_Errormsg ( int err, char *buffer );
 static void dopr( int visible_control, char **buffer, int *left,
	const char *format, va_list args );
 static void fmtstr( int visible_control, char **buffer, int *left,
	char *value, int ljust, int len, int zpad, int precision );
 static void fmtnum(  char **buffer, int *left,
	union value *value, int base, int dosign,
	int ljust, int len, int zpad, int precision );
#if defined(HAVE_QUAD_T)
 static void fmtquad(  char **buffer, int *left,
	union value *value, int base, int dosign,
	int ljust, int len, int zpad, int precision );
#endif
 static void fmtdouble( char **bufer, int *left,
	int fmt, double value,
	int ljust, int len, int zpad, int precision );
 static void dostr(  char **buffer, int *left, char *str );
 static void dopr_outch(  char **buffer, int *left, int c );
/* VARARGS3 */
#ifdef HAVE_STDARGS
 int plp_vsnprintf(char *str, size_t count, const char *fmt, va_list args)
#else
 int plp_vsnprintf(char *str, size_t count, const char *fmt, va_list args)
#endif

{
	int left;
	char *buffer;
	if( (int)count < 0 ) count = 0;
	left = count;
	if( count == 0 ) str = 0;
	buffer = str;
	dopr( 1, &buffer, &left, fmt, args );
	/* fprintf(stderr,"str 0x%x, buffer 0x%x, count %d, left %d\n",
		(int)str, (int)buffer, count, left ); */
	if( str && count > 0 ){
		if( left > 0 ){
			str[count-left] = 0;
		} else {
			str[count-1] = 0;
		}
	}
	return(count - left);
}

/* VARARGS3 */
#ifdef HAVE_STDARGS
 int plp_unsafe_vsnprintf(char *str, size_t count, const char *fmt, va_list args)
#else
 int plp_unsafe_vsnprintf(char *str, size_t count, const char *fmt, va_list args)
#endif
{
	int left;
	char *buffer;
	if( (int)count < 0 ) count = 0;
	left = count;
	if( count == 0 ) str = 0;
	buffer = str;
	dopr( 0, &buffer, &left, fmt, args );
	/* fprintf(stderr,"str 0x%x, buffer 0x%x, count %d, left %d\n",
		(int)str, (int)buffer, count, left ); */
	if( str && count > 0 ){
		if( left > 0 ){
			str[count-left] = 0;
		} else {
			str[count-1] = 0;
		}
	}
	return(count - left);
}

/* VARARGS3 */
#ifdef HAVE_STDARGS
 int plp_snprintf (char *str,size_t count,const char *fmt,...)
#else
 int plp_snprintf (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    char *str;
	size_t count;
    char *fmt;
#endif
	int n = 0;
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (str, char *);
    VA_SHIFT (count, size_t );
    VA_SHIFT (fmt, char *);
    n = plp_vsnprintf ( str, count, fmt, ap);
    VA_END;
	return( n );
}


/* VARARGS3 */
#ifdef HAVE_STDARGS
 int plp_unsafe_snprintf (char *str,size_t count,const char *fmt,...)
#else
 int plp_unsafe_snprintf (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    char *str;
	size_t count;
    char *fmt;
#endif
	int n = 0;
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (str, char *);
    VA_SHIFT (count, size_t );
    VA_SHIFT (fmt, char *);
    n = plp_unsafe_vsnprintf ( str, count, fmt, ap);
    VA_END;
	return( n );
}
 static void dopr( int visible_control, char **buffer, int *left, const char *format, va_list args )
{
	int ch;
	union value value;
	int longflag = 0;
	int quadflag = 0;
	char *strvalue;
	int ljust;
	int len;
	int zpad;
	int precision;
	int set_precision;
	double dval;
	int err = errno;
	int base = 0;
	int signed_val = 0;

	while( (ch = *format++) ){
		switch( ch ){
		case '%':
			longflag = quadflag =
			ljust = len = zpad = base = signed_val = 0;
			precision = -1; set_precision = 0;
		nextch: 
			ch = *format++;
			switch( ch ){
			case 0:
				dostr( buffer, left, "**end of format**" );
				return;
			case '-': ljust = 1; goto nextch;
			case '.': set_precision = 1; precision = 0; goto nextch;
			case '*':
				if( set_precision ){
					precision = va_arg( args, int );
				} else {
					len = va_arg( args, int );
				}
				goto nextch;
			case '0': /* set zero padding if len not set */
				if(len==0 && set_precision == 0 ) zpad = '0';
			case '1': case '2': case '3':
			case '4': case '5': case '6':
			case '7': case '8': case '9':
				if( set_precision ){
					precision = precision*10 + ch - '0';
				} else {
					len = len*10 + ch - '0';
				}
				goto nextch;
			case 'l': ++longflag; goto nextch;
			case 'q':
#if !defined( HAVE_QUAD_T )
					dostr( buffer, left, "*no quad_t support *");
					return;
#endif
					quadflag = 1;
					goto nextch;
			case 'u': case 'U':
				if( base == 0 ){ base = 10; signed_val = 0; }
			case 'o': case 'O':
				if( base == 0 ){ base = 8; signed_val = 0; }
			case 'd': case 'D':
				if( base == 0 ){ base = 10; signed_val = 1; }
			case 'x':
				if( base == 0 ){ base = 16; signed_val = 0; }
			case 'X':
				if( base == 0 ){ base = -16; signed_val = 0; }
#if defined( HAVE_QUAD_T )
				if( quadflag ){
					value.qvalue = va_arg( args, quad_t );
					fmtquad( buffer, left,  &value,base,signed_val, ljust, len, zpad, precision );
					break;
				} else
#endif
				if( longflag > 1 ){
#if defined(HAVE_LONG_LONG)
					if( signed_val ){
					value.value = va_arg( args, long long );
					} else {
					value.value = va_arg( args, unsigned long long );
					}
#else
					if( signed_val ){
					value.value = va_arg( args, long );
					} else {
					value.value = va_arg( args, unsigned long );
					}
#endif
				} else if( longflag ){
					if( signed_val ){
						value.value = va_arg( args, long );
					} else {
						value.value = va_arg( args, unsigned long );
					}
				} else {
					if( signed_val ){
						value.value = va_arg( args, int );
					} else {
						value.value = va_arg( args, unsigned int );
					}
				}
				fmtnum( buffer, left,  &value,base,signed_val, ljust, len, zpad, precision ); break;
			case 's':
				strvalue = va_arg( args, char *);
				fmtstr( visible_control, buffer, left, strvalue,ljust,len, zpad, precision );
				break;
			case 'c':
				ch = va_arg( args, int );
				{ char b[2];
					b[0] = ch;
					b[1] = 0;
					fmtstr( 0, buffer, left, b,ljust,len, zpad, precision );
				}
				break;
			case 'f': case 'g': case 'e':
				dval = va_arg( args, double );
				fmtdouble( buffer, left, ch, dval,ljust,len, zpad, precision ); break;
			case 'm':
				{ char shortbuffer[32];
				fmtstr( visible_control, buffer, left,
					plp_Errormsg(err, shortbuffer),ljust,len, zpad, precision );
				}
				break;
			case '%': dopr_outch( buffer, left, ch ); continue;
			default:
				dostr(  buffer, left, "???????" );
			}
			longflag = 0;
			break;
		default:
			dopr_outch( buffer, left, ch );
			break;
		}
	}
}

/*
 * Format '%[-]len[.precision]s'
 * -   = left justify (ljust)
 * len = minimum length
 * precision = numbers of chars in string to use
 */
 static void
 fmtstr( int visible_control, char **buffer, int *left,
	 char *value, int ljust, int len, int zpad, int precision )
{
	int padlen, strlenv, i, c;	/* amount to pad */

	if( value == 0 ){
		value = "<NULL>";
	}
	/* cheap strlen so you do not have library call */
	for( strlenv = i = 0; (c=CVAL(value+i)); ++i ){
		if( visible_control && iscntrl( c ) && c != '\t' && c != '\n' ){
			++strlenv;
		}
		++strlenv;
	}
	if( precision > 0 && strlenv > precision ){
		strlenv = precision;
	}
	padlen = len - strlenv;
	if( padlen < 0 ) padlen = 0;
	if( ljust ) padlen = -padlen;
	while( padlen > 0 ) {
		dopr_outch( buffer, left, ' ' );
		--padlen;
	}
	/* output characters */
	for( i = 0; i < strlenv && (c = CVAL(value+i)); ++i ){
		if( visible_control && iscntrl( c ) && c != '\t' && c != '\n' ){
			dopr_outch(buffer, left, '^');
			c = ('@' | (c & 0x1F));
		}
		dopr_outch(buffer, left, c);
	}
	while( padlen < 0 ) {
		dopr_outch( buffer, left, ' ' );
		++padlen;
	}
}

 static void
 fmtnum( char **buffer, int *left,
	union value *value, int base, int dosign, int ljust,
	int len, int zpad, int precision )
{
	int signvalue = 0;
#if defined(HAVE_LONG_LONG)
	unsigned long long uvalue;
#else
	unsigned long uvalue;
#endif
	char convert[sizeof( union value) * 8 + 16];
	int place = 0;
	int padlen = 0;	/* amount to pad */
	int caps = 0;

	/* fprintf(stderr,"value 0x%x, base %d, dosign %d, ljust %d, len %d, zpad %d\n",
		value, base, dosign, ljust, len, zpad );/ **/
	uvalue = value->value;
	if( dosign ){
		if( value->value < 0 ) {
			signvalue = '-';
			uvalue = -value->value; 
		}
	}
	if( base < 0 ){
		caps = 1;
		base = -base;
	}
	do{
		convert[place++] =
			(caps? "0123456789ABCDEF":"0123456789abcdef")
			 [uvalue % (unsigned)base  ];
		uvalue = (uvalue / (unsigned)base );
	}while(uvalue);
	convert[place] = 0;
	padlen = len - place;
	if( padlen < 0 ) padlen = 0;
	if( ljust ) padlen = -padlen;
	/* fprintf( stderr, "str '%s', place %d, sign %c, padlen %d\n",
		convert,place,signvalue,padlen); / **/
	if( zpad && padlen > 0 ){
		if( signvalue ){
			dopr_outch( buffer, left, signvalue );
			--padlen;
			signvalue = 0;
		}
		while( padlen > 0 ){
			dopr_outch( buffer, left, zpad );
			--padlen;
		}
	}
	while( padlen > 0 ) {
		dopr_outch( buffer, left, ' ' );
		--padlen;
	}
	if( signvalue ) dopr_outch( buffer, left, signvalue );
	while( place > 0 ) dopr_outch( buffer, left, convert[--place] );
	while( padlen < 0 ){
		dopr_outch( buffer, left, ' ' );
		++padlen;
	}
}

#if defined(HAVE_QUAD_T)

 static void
 fmtquad( char **buffer, int *left,
	union value *value, int base, int dosign, int ljust,
	int len, int zpad, int precision )
{
	int signvalue = 0;
	int place = 0;
	int padlen = 0;	/* amount to pad */
	int caps = 0;
	int i, c;
	union {
		quad_t qvalue;
		unsigned char qconvert[sizeof(quad_t)];
	} vvalue;
	char convert[2*sizeof(quad_t)+1];

	/* fprintf(stderr,"value 0x%x, base %d, dosign %d, ljust %d, len %d, zpad %d\n",
		value, base, dosign, ljust, len, zpad );/ **/
	vvalue.qvalue = value->qvalue;

	if( base < 0 ){
		caps = 1;
	}

	for( i = 0; i < (int)sizeof(quad_t); ++i ){
		c = vvalue.qconvert[i];
		convert[2*i] = 
			(caps? "0123456789ABCDEF":"0123456789abcdef")[ (c >> 4) & 0xF];
		convert[2*i+1] = 
			(caps? "0123456789ABCDEF":"0123456789abcdef")[ c  & 0xF];
	}
	convert[2*i] = 0;

	place = safestrlen(convert);
	padlen = len - place;
	if( padlen < 0 ) padlen = 0;
	if( ljust ) padlen = -padlen;
	/* fprintf( stderr, "str '%s', place %d, sign %c, padlen %d\n",
		convert,place,signvalue,padlen); / **/
	if( zpad && padlen > 0 ){
		if( signvalue ){
			dopr_outch( buffer, left, signvalue );
			--padlen;
			signvalue = 0;
		}
		while( padlen > 0 ){
			dopr_outch( buffer, left, zpad );
			--padlen;
		}
	}
	while( padlen > 0 ) {
		dopr_outch( buffer, left, ' ' );
		--padlen;
	}
	if( signvalue ) dopr_outch( buffer, left, signvalue );
	while( place > 0 ) dopr_outch( buffer, left, convert[--place] );
	while( padlen < 0 ){
		dopr_outch( buffer, left, ' ' );
		++padlen;
	}
}

#endif

 static void mystrcat(char *dest, char *src )
{
	if( dest && src ){
		dest += safestrlen(dest);
		strcpy(dest,src);
	}
}

 static void
 fmtdouble( char **buffer, int *left,
	int fmt, double value, int ljust, int len, int zpad, int precision )
{
	char convert[sizeof( union value) * 8 + 512];
	char formatstr[128];

	/* fprintf(stderr,"len %d, precision %d\n", len, precision ); */
	if( len > 255 ){
		len = 255;
	}
	if( precision > 255 ){
		precision = 255;
	}
	if( precision >= 0 && len > 0 && precision > len ) precision = len;
	strcpy( formatstr, "%" );		/* 1 */
	if( ljust ) mystrcat(formatstr, "-" ); /* 1 */
	if( zpad ) mystrcat(formatstr, "0" );	/* 1 */
	if( len >= 0 ){
		sprintf( formatstr+safestrlen(formatstr), "%d", len ); /* 3 */
	}
	if( precision >= 0 ){
		sprintf( formatstr+safestrlen(formatstr), ".%d", precision ); /* 3 */
	}
	/* format string will be at most 10 chars long ... */
	sprintf( formatstr+safestrlen(formatstr), "%c", fmt );
	/* this is easier than trying to do the portable dtostr */
	/* fprintf(stderr,"format string '%s'\n", formatstr); */
	sprintf( convert, formatstr, value );
	dostr( buffer, left, convert );
}

 static void dostr( char **buffer, int *left, char *str  )
{
	if(str)while(*str) dopr_outch( buffer, left, *str++ );
}

 static void dopr_outch( char **buffer, int *left, int c )
{
	if( *left > 0 ){
		*(*buffer)++ = c;
	}
	*left -= 1;
}


/****************************************************************************
 * static char *plp_errormsg( int err )
 *  returns a printable form of the
 *  errormessage corresponding to the valie of err.
 *  This is the poor man's version of sperror(), not available on all systems
 *  Patrick Powell Tue Apr 11 08:05:05 PDT 1995
 ****************************************************************************/
/****************************************************************************/

#if !defined(HAVE_STRERROR)
# undef  num_errors
# if defined(HAVE_SYS_ERRLIST)
#  if !defined(HAVE_DECL_SYS_ERRLIST)
     extern const char *const sys_errlist[];
#  endif
#  if defined(HAVE_SYS_NERR)
#   if !defined(HAVE_DECL_SYS_NERR)
      extern int sys_nerr;
#   endif
#   define num_errors    (sys_nerr)
#  endif
# endif
# if !defined(num_errors)
#   define num_errors   (-1)            /* always use "errno=%d" */
# endif
#endif

 static char * plp_Errormsg ( int err, char *buffer /* int maxlen = 32 */)
{
    char *cp;

#if defined(HAVE_STRERROR)
	cp = (void *)strerror(err);
#else
# if defined(HAVE_SYS_ERRLIST)
    if (err >= 0 && err < num_errors) {
		cp = (void *)sys_errlist[err];
    } else
# endif
	{
		(void) sprintf (buffer, "errno=%d", err);
		cp = buffer;
    }
#endif
    return (cp);
}

#if defined(TEST)
#include <stdio.h>
 int main( void )
{
	char buffer[128];
	char *t;
	char *test1 = "01234";
	int n;
	errno = 1;
	buffer[0] = 0;
	n = plp_snprintf( buffer, 0, (t="test")); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t="errno '%m'")); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%s"), test1 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%12s"), test1 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%-12s"), test1 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%12.2s"), test1 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%-12.2s"), test1 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%g"), 1.25 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%g"), 1.2345 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%12g"), 1.25 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%12.1g"), 1.25 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%12.2g"), 1.25 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%12.3g"), 1.25 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%0*d"), 6, 1 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
#if defined(HAVE_LONG_LONG)
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%llx"), 1, 2, 3, 4 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%llx"), (long long)1, (long long)2 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%qx"), 1, 2, 3, 4 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%qx"), (quad_t)1, (quad_t)2 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
#endif
	n = plp_snprintf( buffer, sizeof(buffer), (t = "0%x, 0%x"), (char *)(0x01234567), (char *)0, 0, 0, 0); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "0%x, 0%x"), (char *)(0x01234567), (char *)0x89ABCDEF, 0, 0, 0); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "0%x, 0%x"), t, 0, 0, 0, 0); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%f"), 1.25 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%f"), 1.2345 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%12f"), 1.25 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%12.2f"), 1.25 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%f"), 1.0 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%.0f"), 1.0 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%0.0f"), 1.0 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%1.0f"), 1.0 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%1.5f"), 1.0 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	n = plp_snprintf( buffer, sizeof(buffer), (t = "%5.5f"), 1.0 ); printf( "[%d] %s = '%s'\n", n, t, buffer );
	return(0);
}
#endif

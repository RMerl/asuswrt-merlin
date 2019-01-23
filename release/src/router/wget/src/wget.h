/* Miscellaneous declarations.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011, 2015 Free Software
   Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

/* This file contains declarations that are universally useful and
   those that don't fit elsewhere.  It also includes sysdep.h which
   includes some often-needed system includes, like the obnoxious
   <time.h> inclusion.  */

#ifndef WGET_H
#define WGET_H

#include "config.h"

#if ((defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__)
# define WINDOWS
#endif

/* Include these, so random files need not include them.  */
#include "sysdep.h"

/* Disable assertions when debug support is not compiled in. */
#ifndef ENABLE_DEBUG
#ifndef NDEBUG
# define NDEBUG
#endif
#endif

/* Is OpenSSL or GNUTLS available? */
#if defined HAVE_LIBSSL || defined HAVE_LIBSSL32 || defined HAVE_LIBGNUTLS
# define HAVE_SSL
# define HAVE_HSTS /* There's no sense in enabling HSTS without SSL */
#endif

/* `gettext (FOO)' is long to write, so we use `_(FOO)'.  If NLS is
   unavailable, _(STRING) simply returns STRING.  */
#include "gettext.h"
#define _(STRING) gettext(STRING)

/* A pseudo function call that serves as a marker for the automated
   extraction of messages, but does not call gettext().  The run-time
   translation is done at a different place in the code.  The purpose
   of the N_("...") call is to make the message snarfer aware that the
   "..." string needs to be translated.  STRING should be a string
   literal.  Concatenated strings and other string expressions won't
   work.  The macro's expansion is not parenthesized, so that it is
   suitable as initializer for static 'char[]' or 'const char[]'
   variables.  -- explanation partly taken from GNU make.  */
#define N_(string) string

#if HAVE_WCWIDTH && HAVE_MBTOWC
# define USE_NLS_PROGRESS_BAR 1
#else
/* Just to be a little paranoid about it. */
# undef  USE_NLS_PROGRESS_BAR
#endif

/* I18N NOTE: You will notice that none of the DEBUGP messages are
   marked as translatable.  This is intentional, for a few reasons:

   1) The debug messages are not meant for the users to look at, but
   for the developers; as such, they should be considered more like
   source comments than real program output.

   2) The messages are numerous, and yet they are random and frivolous
   ("double yuck!" and such).  There would be a lot of work with no
   gain.

   3) Finally, the debug messages are meant to be a clue for me to
   debug problems with Wget.  If I get them in a language I don't
   understand, debugging will become a new challenge of its own!  */

/* locale independent replacement for ctype.h */
#include "c-ctype.h"

/* Conditionalize the use of GCC's __attribute__((format)) and
   __builtin_expect features using macros.  */

#if defined(__GNUC__) && __GNUC__ >= 3
# define GCC_FORMAT_ATTR(a, b) __attribute__ ((format (printf, a, b)))
# define LIKELY(exp)   __builtin_expect (!!(exp), 1)
# define UNLIKELY(exp) __builtin_expect ((exp), 0)
#else
# define GCC_FORMAT_ATTR(a, b)
# define LIKELY(exp)   (exp)
# define UNLIKELY(exp) (exp)
#endif

/* Execute the following statement if debugging is both enabled at
   compile-time and requested at run-time; a no-op otherwise.  */

#ifdef ENABLE_DEBUG
# define IF_DEBUG if (UNLIKELY (opt.debug))
#else
# define IF_DEBUG if (0)
#endif

/* Print ARGS if debugging is enabled and requested, otherwise do
   nothing.  This must be called with an extra level of parentheses
   because it's not possible to pass a variable number of arguments to
   a macro (in portable C89).  ARGS are like arguments to printf.  */

#define DEBUGP(args) do { IF_DEBUG { debug_logprintf args; } } while (0)

/* Pick an integer type large enough for file sizes, content lengths,
   and such.  Because today's files can be very large, it should be a
   signed integer at least 64 bits wide.  This can't be typedeffed to
   off_t because: a) off_t is always 32-bit on Windows, and b) we
   don't necessarily want to tie having a 64-bit type for internal
   calculations to having LFS support.  */

#ifdef WINDOWS
  /* nothing to do, see mswindows.h */
#elif SIZEOF_LONG >= 8
  /* long is large enough, so use it. */
  typedef long wgint;
# define SIZEOF_WGINT SIZEOF_LONG
#elif SIZEOF_LONG_LONG >= 8
  /* long long is large enough and available, use that */
  typedef long long wgint;
# define SIZEOF_WGINT SIZEOF_LONG_LONG
#elif HAVE_INT64_T
  typedef int64_t wgint;
# define SIZEOF_WGINT 8
#elif SIZEOF_OFF_T >= 8
  /* In case off_t is typedeffed to a large non-standard type that our
     tests don't find. */
  typedef off_t wgint;
# define SIZEOF_WGINT SIZEOF_OFF_T
#else
  /* Fall back to using long, which is always available and in most
     cases large enough. */
  typedef long wgint;
# define SIZEOF_WGINT SIZEOF_LONG
#endif

/* Pick a strtol-compatible function that will work with wgint.  The
   choices are strtol, strtoll, or our own implementation of strtoll
   in cmpt.c, activated with NEED_STRTOLL.  */

#ifdef WINDOWS
  /* nothing to do, see mswindows.h */
#elif SIZEOF_WGINT == SIZEOF_LONG
# define str_to_wgint strtol
#elif SIZEOF_WGINT == SIZEOF_LONG_LONG
# define str_to_wgint strtoll
# ifndef HAVE_STRTOLL
#  define NEED_STRTOLL
#  define strtoll_type long long
# endif
#else
  /* wgint has a strange size; synthesize strtoll and use it. */
# define str_to_wgint strtoll
# define NEED_STRTOLL
# define strtoll_type wgint
#endif

#define WGINT_MAX TYPE_MAXIMUM (wgint)

/* Declare our strtoll replacement. */
#ifdef NEED_STRTOLL
strtoll_type strtoll (const char *, char **, int);
#endif

/* Now define a large numeric type useful for storing sizes of *sums*
   of downloads, such as the value of the --quota option.  This should
   be a type able to hold 2G+ values even on systems without large
   file support.  (It is useful to limit Wget's download quota to say
   10G even if a single file cannot be that large.)

   To make sure we get the largest size possible, we use `double' on
   systems without a 64-bit integral type.  (Since it is used in very
   few places in Wget, this is acceptable.)  */

#if SIZEOF_WGINT >= 8
/* just use wgint */
typedef wgint SUM_SIZE_INT;
#else
/* On systems without LFS, use double, which buys us integers up to 2^53. */
typedef double SUM_SIZE_INT;
#endif

#include "options.h"

/* Everything uses this, so include them here directly.  */
#include <alloca.h>
#include "xalloc.h"

/* Likewise for logging functions.  */
#include "log.h"

/* Likewise for quoting functions.  */
#include "quote.h"
#include "quotearg.h"

/* Likewise for struct iri definition */
#include "iri.h"

/* Useful macros used across the code: */

/* The number of elements in an array.  For example:
   static char a[] = "foo";     -- countof(a) == 4 (note terminating \0)
   int a[5] = {1, 2};           -- countof(a) == 5
   char *a[] = {                -- countof(a) == 3
     "foo", "bar", "baz"
   }; */
#define countof(array) (sizeof (array) / sizeof ((array)[0]))

/* Zero out a value.  */
#define xzero(x) memset (&(x), '\0', sizeof (x))

/* Convert an ASCII hex digit to the corresponding number between 0
   and 15.  H should be a hexadecimal digit that satisfies isxdigit;
   otherwise, the result is undefined.  */
#define XDIGIT_TO_NUM(h) ((h) < 'A' ? (h) - '0' : c_toupper (h) - 'A' + 10)
#define X2DIGITS_TO_NUM(h1, h2) ((XDIGIT_TO_NUM (h1) << 4) + XDIGIT_TO_NUM (h2))

/* The reverse of the above: convert a number in the [0, 16) range to
   the ASCII representation of the corresponding hexadecimal digit.
   `+ 0' is there so you can't accidentally use it as an lvalue.  */
#define XNUM_TO_DIGIT(x) ("0123456789ABCDEF"[x] + 0)
#define XNUM_TO_digit(x) ("0123456789abcdef"[x] + 0)

/* Copy the data delimited with BEG and END to alloca-allocated
   storage, and zero-terminate it.  Arguments are evaluated only once,
   in the order BEG, END, PLACE.  */
#define BOUNDED_TO_ALLOCA(beg, end, place) do { \
  const char *BTA_beg = (beg);                  \
  int BTA_len = (end) - BTA_beg;                \
  char **BTA_dest = &(place);                   \
  *BTA_dest = alloca (BTA_len + 1);             \
  memcpy (*BTA_dest, BTA_beg, BTA_len);         \
  (*BTA_dest)[BTA_len] = '\0';                  \
} while (0)

/* Return non-zero if string bounded between BEG and END is equal to
   STRING_LITERAL.  The comparison is case-sensitive.  */
#define BOUNDED_EQUAL(beg, end, string_literal)             \
  ((end) - (beg) == sizeof (string_literal) - 1             \
   && !memcmp (beg, string_literal, sizeof (string_literal) - 1))

/* The same as above, except the comparison is case-insensitive. */
#define BOUNDED_EQUAL_NO_CASE(beg, end, string_literal)         \
  ((end) - (beg) == sizeof (string_literal) - 1                 \
   && !c_strncasecmp (beg, string_literal, sizeof (string_literal) - 1))

/* Like ptr=strdup(str), but allocates the space for PTR on the stack.
   This cannot be an expression because this is not portable:
     #define STRDUP_ALLOCA(str) (strcpy (alloca (strlen (str) + 1), str))
   The problem is that some compilers can't handle alloca() being an
   argument to a function.  */

#define STRDUP_ALLOCA(ptr, str) do {                \
  char **SA_dest = &(ptr);                          \
  const char *SA_src = (str);                       \
  *SA_dest = (char *)alloca (strlen (SA_src) + 1);  \
  strcpy (*SA_dest, SA_src);                        \
} while (0)

/* Generally useful if you want to avoid arbitrary size limits but
   don't need a full dynamic array.  Assumes that BASEVAR points to a
   malloced array of TYPE objects (or possibly a NULL pointer, if
   SIZEVAR is 0), with the total size stored in SIZEVAR.  This macro
   will realloc BASEVAR as necessary so that it can hold at least
   NEEDED_SIZE objects.  The reallocing is done by doubling, which
   ensures constant amortized time per element.  */

#define DO_REALLOC(basevar, sizevar, needed_size, type) do {    \
  long DR_needed_size = (needed_size);                          \
  long DR_newsize = 0;                                          \
  while ((sizevar) < (DR_needed_size)) {                        \
    DR_newsize = sizevar << 1;                                  \
    if (DR_newsize < 16)                                        \
      DR_newsize = 16;                                          \
    (sizevar) = DR_newsize;                                     \
  }                                                             \
  if (DR_newsize)                                               \
    basevar = xrealloc (basevar, DR_newsize * sizeof (type));   \
} while (0)

/* Used to print pointers (usually for debugging).  Print pointers
   using printf ("0x%0*lx", PTR_FORMAT (p)).  (%p is too unpredictable;
   some implementations prepend 0x, while some don't, and most don't
   0-pad the address.)  */
#define PTR_FORMAT(p) (int) (2 * sizeof (void *)), (unsigned long) (p)

/* Find the maximum buffer length needed to print an integer of type `x'
   in base 10. 24082 / 10000 = 8*log_{10}(2).  */
#define MAX_INT_TO_STRING_LEN(x) ((sizeof(x) * 24082 / 10000) + 2)

/* Find the minimum or maximum of two provided values */
# define MIN(i, j) ((i) <= (j) ? (i) : (j))
# define MAX(i, j) ((i) >= (j) ? (i) : (j))


extern const char *exec_name;
extern const char *program_name;
extern const char *program_argstring;

/* Document type ("dt") flags */
enum
{
  TEXTHTML             = 0x0001,        /* document is of type text/html
                                           or application/xhtml+xml */
  RETROKF              = 0x0002,        /* retrieval was OK */
  HEAD_ONLY            = 0x0004,        /* only send the HEAD request */
  SEND_NOCACHE         = 0x0008,        /* send Pragma: no-cache directive */
  ACCEPTRANGES         = 0x0010,        /* Accept-ranges header was found */
  ADDED_HTML_EXTENSION = 0x0020,        /* added ".html" extension due to -E */
  TEXTCSS              = 0x0040,        /* document is of type text/css */
  IF_MODIFIED_SINCE    = 0x0080,        /* use if-modified-since header */
  METALINK_METADATA    = 0x0100         /* use HTTP response for Metalink metadata */
};

/* Universal error type -- used almost everywhere.  Error reporting of
   this detail is not generally used or needed and should be
   simplified.  */
typedef enum
{
  NOCONERROR, HOSTERR, CONSOCKERR, CONERROR, CONSSLERR,
  CONIMPOSSIBLE, NEWLOCATION,
  FTPOK, FTPLOGINC, FTPLOGREFUSED, FTPPORTERR, FTPSYSERR,
  FTPNSFOD, FTPUNKNOWNTYPE, FTPRERR,
  FTPSRVERR, FTPRETRINT, FTPRESTFAIL, URLERROR, FOPENERR,
  FOPEN_EXCL_ERR, FWRITEERR, HEOF, GATEWAYTIMEOUT,
  HERR, RETROK, RECLEVELEXC, WRONGCODE,
  FTPINVPASV, FTPNOPASV, FTPNOPBSZ, FTPNOPROT, FTPNOAUTH,
  CONTNOTSUPPORTED, RETRUNNEEDED, RETRFINISHED,
  READERR, TRYLIMEXC, FILEBADFILE, RANGEERR,
  RETRBADPATTERN, PROXERR,
  AUTHFAILED, QUOTEXC, WRITEFAILED, SSLINITFAILED, VERIFCERTERR,
  UNLINKERR, NEWLOCATION_KEEP_POST, CLOSEFAILED, ATTRMISSING, UNKNOWNATTR,
  WARC_ERR, WARC_TMP_FOPENERR, WARC_TMP_FWRITEERR,
  TIMECONV_ERR,
  METALINK_PARSE_ERROR, METALINK_RETR_ERROR,
  METALINK_CHKSUM_ERROR, METALINK_SIG_ERROR, METALINK_MISSING_RESOURCE,
  RETR_WITH_METALINK,
  METALINK_SIZE_ERROR
} uerr_t;

/* 2005-02-19 SMS.
   Select an appropriate "orig" suffix and a separator character for
   adding a unique suffix to a file name.

   A VMS ODS2 file system can't tolerate multiple dots.  An ODS5 file
   system can, but even there not all dots are equal, and heroic effort
   would be needed to get ".html^.orig" rather than (the less desirable)
   "^.html.orig".  It's more satisfactory always to use "_orig" on VMS
   (rather than including "vms.h", testing "ods5_dest", and acting
   accordingly).

   Note that code in various places assumes that this string is five
   characters long.
*/
# ifdef __VMS
#  define ORIG_SFX "_orig"
# else /* def __VMS */
#  define ORIG_SFX ".orig"
# endif /* def __VMS [else] */

/* ".NNN" unique-ifying suffix separator character for unique_name() in
   url.c (and anywhere else).  Note that on VMS, the file system's
   version numbers solve the problem that unique_name() is designed to
   handle, obviating this whole exercise.  Other systems may specify a
   character different from "." here, if desired.
*/
# ifndef __VMS
#  define UNIQ_SEP '.'
# endif /* ndef __VMS */

#endif /* WGET_H */

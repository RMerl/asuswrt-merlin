/* URL handling.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011, 2015 Free Software
   Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

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

#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "utils.h"
#include "url.h"
#include "host.h"  /* for is_valid_ipv6_address */
#include "c-strcase.h"

#ifdef HAVE_ICONV
# include <iconv.h>
#endif
#include <langinfo.h>

#ifdef __VMS
#include "vms.h"
#endif /* def __VMS */

#ifdef TESTING
#include "test.h"
#endif

enum {
  scm_disabled = 1,             /* for https when OpenSSL fails to init. */
  scm_has_params = 2,           /* whether scheme has ;params */
  scm_has_query = 4,            /* whether scheme has ?query */
  scm_has_fragment = 8          /* whether scheme has #fragment */
};

struct scheme_data
{
  /* Short name of the scheme, such as "http" or "ftp". */
  const char *name;
  /* Leading string that identifies the scheme, such as "https://". */
  const char *leading_string;
  /* Default port of the scheme when none is specified. */
  int default_port;
  /* Various flags. */
  int flags;
};

/* Supported schemes: */
static struct scheme_data supported_schemes[] =
{
  { "http",     "http://",  DEFAULT_HTTP_PORT,  scm_has_query|scm_has_fragment },
#ifdef HAVE_SSL
  { "https",    "https://", DEFAULT_HTTPS_PORT, scm_has_query|scm_has_fragment },
#endif
  { "ftp",      "ftp://",   DEFAULT_FTP_PORT,   scm_has_params|scm_has_fragment },
#ifdef HAVE_SSL
  /*
   * Explicit FTPS uses the same port as FTP.
   * Implicit FTPS has its own port (990), but it is disabled by default.
   */
  { "ftps",     "ftps://",  DEFAULT_FTP_PORT,  scm_has_params|scm_has_fragment },
#endif

  /* SCHEME_INVALID */
  { NULL,       NULL,       -1,                 0 }
};

/* Forward declarations: */

static bool path_simplify (enum url_scheme, char *);

/* Support for escaping and unescaping of URL strings.  */

/* Table of "reserved" and "unsafe" characters.  Those terms are
   rfc1738-speak, as such largely obsoleted by rfc2396 and later
   specs, but the general idea remains.

   A reserved character is the one that you can't decode without
   changing the meaning of the URL.  For example, you can't decode
   "/foo/%2f/bar" into "/foo///bar" because the number and contents of
   path components is different.  Non-reserved characters can be
   changed, so "/foo/%78/bar" is safe to change to "/foo/x/bar".  The
   unsafe characters are loosely based on rfc1738, plus "$" and ",",
   as recommended by rfc2396, and minus "~", which is very frequently
   used (and sometimes unrecognized as %7E by broken servers).

   An unsafe character is the one that should be encoded when URLs are
   placed in foreign environments.  E.g. space and newline are unsafe
   in HTTP contexts because HTTP uses them as separator and line
   terminator, so they must be encoded to %20 and %0A respectively.
   "*" is unsafe in shell context, etc.

   We determine whether a character is unsafe through static table
   lookup.  This code assumes ASCII character set and 8-bit chars.  */

enum {
  /* rfc1738 reserved chars + "$" and ",".  */
  urlchr_reserved = 1,

  /* rfc1738 unsafe chars, plus non-printables.  */
  urlchr_unsafe   = 2
};

#define urlchr_test(c, mask) (urlchr_table[(unsigned char)(c)] & (mask))
#define URL_RESERVED_CHAR(c) urlchr_test(c, urlchr_reserved)
#define URL_UNSAFE_CHAR(c) urlchr_test(c, urlchr_unsafe)

/* Shorthands for the table: */
#define R  urlchr_reserved
#define U  urlchr_unsafe
#define RU R|U

static const unsigned char urlchr_table[256] =
{
  U,  U,  U,  U,   U,  U,  U,  U,   /* NUL SOH STX ETX  EOT ENQ ACK BEL */
  U,  U,  U,  U,   U,  U,  U,  U,   /* BS  HT  LF  VT   FF  CR  SO  SI  */
  U,  U,  U,  U,   U,  U,  U,  U,   /* DLE DC1 DC2 DC3  DC4 NAK SYN ETB */
  U,  U,  U,  U,   U,  U,  U,  U,   /* CAN EM  SUB ESC  FS  GS  RS  US  */
  U,  0,  U, RU,   R,  U,  R,  0,   /* SP  !   "   #    $   %   &   '   */
  0,  0,  0,  R,   R,  0,  0,  R,   /* (   )   *   +    ,   -   .   /   */
  0,  0,  0,  0,   0,  0,  0,  0,   /* 0   1   2   3    4   5   6   7   */
  0,  0, RU,  R,   U,  R,  U,  R,   /* 8   9   :   ;    <   =   >   ?   */
 RU,  0,  0,  0,   0,  0,  0,  0,   /* @   A   B   C    D   E   F   G   */
  0,  0,  0,  0,   0,  0,  0,  0,   /* H   I   J   K    L   M   N   O   */
  0,  0,  0,  0,   0,  0,  0,  0,   /* P   Q   R   S    T   U   V   W   */
  0,  0,  0, RU,   U, RU,  U,  0,   /* X   Y   Z   [    \   ]   ^   _   */
  U,  0,  0,  0,   0,  0,  0,  0,   /* `   a   b   c    d   e   f   g   */
  0,  0,  0,  0,   0,  0,  0,  0,   /* h   i   j   k    l   m   n   o   */
  0,  0,  0,  0,   0,  0,  0,  0,   /* p   q   r   s    t   u   v   w   */
  0,  0,  0,  U,   U,  U,  0,  U,   /* x   y   z   {    |   }   ~   DEL */

  U, U, U, U,  U, U, U, U,  U, U, U, U,  U, U, U, U,
  U, U, U, U,  U, U, U, U,  U, U, U, U,  U, U, U, U,
  U, U, U, U,  U, U, U, U,  U, U, U, U,  U, U, U, U,
  U, U, U, U,  U, U, U, U,  U, U, U, U,  U, U, U, U,

  U, U, U, U,  U, U, U, U,  U, U, U, U,  U, U, U, U,
  U, U, U, U,  U, U, U, U,  U, U, U, U,  U, U, U, U,
  U, U, U, U,  U, U, U, U,  U, U, U, U,  U, U, U, U,
  U, U, U, U,  U, U, U, U,  U, U, U, U,  U, U, U, U,
};
#undef R
#undef U
#undef RU

static void
url_unescape_1 (char *s, unsigned char mask)
{
  char *t = s;                  /* t - tortoise */
  char *h = s;                  /* h - hare     */

  for (; *h; h++, t++)
    {
      if (*h != '%')
        {
        copychar:
          *t = *h;
        }
      else
        {
          char c;
          /* Do nothing if '%' is not followed by two hex digits. */
          if (!h[1] || !h[2] || !(c_isxdigit (h[1]) && c_isxdigit (h[2])))
            goto copychar;
          c = X2DIGITS_TO_NUM (h[1], h[2]);
          if (urlchr_test(c, mask))
            goto copychar;
          /* Don't unescape %00 because there is no way to insert it
             into a C string without effectively truncating it. */
          if (c == '\0')
            goto copychar;
          *t = c;
          h += 2;
        }
    }
  *t = '\0';
}

/* URL-unescape the string S.

   This is done by transforming the sequences "%HH" to the character
   represented by the hexadecimal digits HH.  If % is not followed by
   two hexadecimal digits, it is inserted literally.

   The transformation is done in place.  If you need the original
   string intact, make a copy before calling this function.  */
void
url_unescape (char *s)
{
  url_unescape_1 (s, 0);
}

/* URL-unescape the string S.

   This functions behaves identically as url_unescape(), but does not
   convert characters from "reserved". In other words, it only converts
   "unsafe" characters.  */
void
url_unescape_except_reserved (char *s)
{
  url_unescape_1 (s, urlchr_reserved);
}

/* The core of url_escape_* functions.  Escapes the characters that
   match the provided mask in urlchr_table.

   If ALLOW_PASSTHROUGH is true, a string with no unsafe chars will be
   returned unchanged.  If ALLOW_PASSTHROUGH is false, a freshly
   allocated string will be returned in all cases.  */

static char *
url_escape_1 (const char *s, unsigned char mask, bool allow_passthrough)
{
  const char *p1;
  char *p2, *newstr;
  int newlen;
  int addition = 0;

  for (p1 = s; *p1; p1++)
    if (urlchr_test (*p1, mask))
      addition += 2;            /* Two more characters (hex digits) */

  if (!addition)
    return allow_passthrough ? (char *)s : xstrdup (s);

  newlen = (p1 - s) + addition;
  newstr = xmalloc (newlen + 1);

  p1 = s;
  p2 = newstr;
  while (*p1)
    {
      /* Quote the characters that match the test mask. */
      if (urlchr_test (*p1, mask))
        {
          unsigned char c = *p1++;
          *p2++ = '%';
          *p2++ = XNUM_TO_DIGIT (c >> 4);
          *p2++ = XNUM_TO_DIGIT (c & 0xf);
        }
      else
        *p2++ = *p1++;
    }
  assert (p2 - newstr == newlen);
  *p2 = '\0';

  return newstr;
}

/* URL-escape the unsafe characters (see urlchr_table) in a given
   string, returning a freshly allocated string.  */

char *
url_escape (const char *s)
{
  return url_escape_1 (s, urlchr_unsafe, false);
}

/* URL-escape the unsafe and reserved characters (see urlchr_table) in
   a given string, returning a freshly allocated string.  */

char *
url_escape_unsafe_and_reserved (const char *s)
{
  return url_escape_1 (s, urlchr_unsafe|urlchr_reserved, false);
}

/* URL-escape the unsafe characters (see urlchr_table) in a given
   string.  If no characters are unsafe, S is returned.  */

static char *
url_escape_allow_passthrough (const char *s)
{
  return url_escape_1 (s, urlchr_unsafe, true);
}

/* Decide whether the char at position P needs to be encoded.  (It is
   not enough to pass a single char *P because the function may need
   to inspect the surrounding context.)

   Return true if the char should be escaped as %XX, false otherwise.  */

static inline bool
char_needs_escaping (const char *p)
{
  if (*p == '%')
    {
      if (c_isxdigit (*(p + 1)) && c_isxdigit (*(p + 2)))
        return false;
      else
        /* Garbled %.. sequence: encode `%'. */
        return true;
    }
  else if (URL_UNSAFE_CHAR (*p) && !URL_RESERVED_CHAR (*p))
    return true;
  else
    return false;
}

/* Translate a %-escaped (but possibly non-conformant) input string S
   into a %-escaped (and conformant) output string.  If no characters
   are encoded or decoded, return the same string S; otherwise, return
   a freshly allocated string with the new contents.

   After a URL has been run through this function, the protocols that
   use `%' as the quote character can use the resulting string as-is,
   while those that don't can use url_unescape to get to the intended
   data.  This function is stable: once the input is transformed,
   further transformations of the result yield the same output.

   Let's discuss why this function is needed.

   Imagine Wget is asked to retrieve `http://abc.xyz/abc def'.  Since
   a raw space character would mess up the HTTP request, it needs to
   be quoted, like this:

       GET /abc%20def HTTP/1.0

   It would appear that the unsafe chars need to be quoted, for
   example with url_escape.  But what if we're requested to download
   `abc%20def'?  url_escape transforms "%" to "%25", which would leave
   us with `abc%2520def'.  This is incorrect -- since %-escapes are
   part of URL syntax, "%20" is the correct way to denote a literal
   space on the Wget command line.  This leads to the conclusion that
   in that case Wget should not call url_escape, but leave the `%20'
   as is.  This is clearly contradictory, but it only gets worse.

   What if the requested URI is `abc%20 def'?  If we call url_escape,
   we end up with `/abc%2520%20def', which is almost certainly not
   intended.  If we don't call url_escape, we are left with the
   embedded space and cannot complete the request.  What the user
   meant was for Wget to request `/abc%20%20def', and this is where
   reencode_escapes kicks in.

   Wget used to solve this by first decoding %-quotes, and then
   encoding all the "unsafe" characters found in the resulting string.
   This was wrong because it didn't preserve certain URL special
   (reserved) characters.  For instance, URI containing "a%2B+b" (0x2b
   == '+') would get translated to "a%2B%2Bb" or "a++b" depending on
   whether we considered `+' reserved (it is).  One of these results
   is inevitable because by the second step we would lose information
   on whether the `+' was originally encoded or not.  Both results
   were wrong because in CGI parameters + means space, while %2B means
   literal plus.  reencode_escapes correctly translates the above to
   "a%2B+b", i.e. returns the original string.

   This function uses a modified version of the algorithm originally
   proposed by Anon Sricharoenchai:

   * Encode all "unsafe" characters, except those that are also
     "reserved", to %XX.  See urlchr_table for which characters are
     unsafe and reserved.

   * Encode the "%" characters not followed by two hex digits to
     "%25".

   * Pass through all other characters and %XX escapes as-is.  (Up to
     Wget 1.10 this decoded %XX escapes corresponding to "safe"
     characters, but that was obtrusive and broke some servers.)

   Anon's test case:

   "http://abc.xyz/%20%3F%%36%31%25aa% a?a=%61+a%2Ba&b=b%26c%3Dc"
   ->
   "http://abc.xyz/%20%3F%25%36%31%25aa%25%20a?a=%61+a%2Ba&b=b%26c%3Dc"

   Simpler test cases:

   "foo bar"         -> "foo%20bar"
   "foo%20bar"       -> "foo%20bar"
   "foo %20bar"      -> "foo%20%20bar"
   "foo%%20bar"      -> "foo%25%20bar"       (0x25 == '%')
   "foo%25%20bar"    -> "foo%25%20bar"
   "foo%2%20bar"     -> "foo%252%20bar"
   "foo+bar"         -> "foo+bar"            (plus is reserved!)
   "foo%2b+bar"      -> "foo%2b+bar"  */

static char *
reencode_escapes (const char *s)
{
  const char *p1;
  char *newstr, *p2;
  int oldlen, newlen;

  int encode_count = 0;

  /* First pass: inspect the string to see if there's anything to do,
     and to calculate the new length.  */
  for (p1 = s; *p1; p1++)
    if (char_needs_escaping (p1))
      ++encode_count;

  if (!encode_count)
    /* The string is good as it is. */
    return (char *) s;          /* C const model sucks. */

  oldlen = p1 - s;
  /* Each encoding adds two characters (hex digits).  */
  newlen = oldlen + 2 * encode_count;
  newstr = xmalloc (newlen + 1);

  /* Second pass: copy the string to the destination address, encoding
     chars when needed.  */
  p1 = s;
  p2 = newstr;

  while (*p1)
    if (char_needs_escaping (p1))
      {
        unsigned char c = *p1++;
        *p2++ = '%';
        *p2++ = XNUM_TO_DIGIT (c >> 4);
        *p2++ = XNUM_TO_DIGIT (c & 0xf);
      }
    else
      *p2++ = *p1++;

  *p2 = '\0';
  assert (p2 - newstr == newlen);
  return newstr;
}

/* Returns the scheme type if the scheme is supported, or
   SCHEME_INVALID if not.  */

enum url_scheme
url_scheme (const char *url)
{
  int i;

  for (i = 0; supported_schemes[i].leading_string; i++)
    if (0 == c_strncasecmp (url, supported_schemes[i].leading_string,
                          strlen (supported_schemes[i].leading_string)))
      {
        if (!(supported_schemes[i].flags & scm_disabled))
          return (enum url_scheme) i;
        else
          return SCHEME_INVALID;
      }

  return SCHEME_INVALID;
}

#define SCHEME_CHAR(ch) (c_isalnum (ch) || (ch) == '-' || (ch) == '+')

/* Return 1 if the URL begins with any "scheme", 0 otherwise.  As
   currently implemented, it returns true if URL begins with
   [-+a-zA-Z0-9]+: .  */

bool
url_has_scheme (const char *url)
{
  const char *p = url;

  /* The first char must be a scheme char. */
  if (!*p || !SCHEME_CHAR (*p))
    return false;
  ++p;
  /* Followed by 0 or more scheme chars. */
  while (*p && SCHEME_CHAR (*p))
    ++p;
  /* Terminated by ':'. */
  return *p == ':';
}

bool
url_valid_scheme (const char *url)
{
  enum url_scheme scheme = url_scheme (url);
  return scheme != SCHEME_INVALID;
}

int
scheme_default_port (enum url_scheme scheme)
{
  return supported_schemes[scheme].default_port;
}

void
scheme_disable (enum url_scheme scheme)
{
  supported_schemes[scheme].flags |= scm_disabled;
}

const char *
scheme_leading_string (enum url_scheme scheme)
{
  return supported_schemes[scheme].leading_string;
}

/* Skip the username and password, if present in the URL.  The
   function should *not* be called with the complete URL, but with the
   portion after the scheme.

   If no username and password are found, return URL.  */

static const char *
url_skip_credentials (const char *url)
{
  /* Look for '@' that comes before terminators, such as '/', '?',
     '#', or ';'.  */
  const char *p = (const char *)strpbrk (url, "@/?#;");
  if (!p || *p != '@')
    return url;
  return p + 1;
}

/* Parse credentials contained in [BEG, END).  The region is expected
   to have come from a URL and is unescaped.  */

static bool
parse_credentials (const char *beg, const char *end, char **user, char **passwd)
{
  char *colon;
  const char *userend;

  if (beg == end)
    return false;               /* empty user name */

  colon = memchr (beg, ':', end - beg);
  if (colon == beg)
    return false;               /* again empty user name */

  if (colon)
    {
      *passwd = strdupdelim (colon + 1, end);
      userend = colon;
      url_unescape (*passwd);
    }
  else
    {
      *passwd = NULL;
      userend = end;
    }
  *user = strdupdelim (beg, userend);
  url_unescape (*user);
  return true;
}

/* Used by main.c: detect URLs written using the "shorthand" URL forms
   originally popularized by Netscape and NcFTP.  HTTP shorthands look
   like this:

   www.foo.com[:port]/dir/file   -> http://www.foo.com[:port]/dir/file
   www.foo.com[:port]            -> http://www.foo.com[:port]

   FTP shorthands look like this:

   foo.bar.com:dir/file          -> ftp://foo.bar.com/dir/file
   foo.bar.com:/absdir/file      -> ftp://foo.bar.com//absdir/file

   If the URL needs not or cannot be rewritten, return NULL.  */

char *
rewrite_shorthand_url (const char *url)
{
  const char *p;
  char *ret;

  if (url_scheme (url) != SCHEME_INVALID)
    return NULL;

  /* Look for a ':' or '/'.  The former signifies NcFTP syntax, the
     latter Netscape.  */
  p = strpbrk (url, ":/");
  if (p == url)
    return NULL;

  /* If we're looking at "://", it means the URL uses a scheme we
     don't support, which may include "https" when compiled without
     SSL support.  Don't bogusly rewrite such URLs.  */
  if (p && p[0] == ':' && p[1] == '/' && p[2] == '/')
    return NULL;

  if (p && *p == ':')
    {
      /* Colon indicates ftp, as in foo.bar.com:path.  Check for
         special case of http port number ("localhost:10000").  */
      int digits = strspn (p + 1, "0123456789");
      if (digits && (p[1 + digits] == '/' || p[1 + digits] == '\0'))
        goto http;

      /* Turn "foo.bar.com:path" to "ftp://foo.bar.com/path". */
      if ((ret = aprintf ("ftp://%s", url)) != NULL)
        ret[6 + (p - url)] = '/';
    }
  else
    {
    http:
      /* Just prepend "http://" to URL. */
      ret = aprintf ("http://%s", url);
    }
  return ret;
}

static void split_path (const char *, char **, char **);

/* Like strpbrk, with the exception that it returns the pointer to the
   terminating zero (end-of-string aka "eos") if no matching character
   is found.  */

static inline char *
strpbrk_or_eos (const char *s, const char *accept)
{
  char *p = strpbrk (s, accept);
  if (!p)
    p = strchr (s, '\0');
  return p;
}

/* Turn STR into lowercase; return true if a character was actually
   changed. */

static bool
lowercase_str (char *str)
{
  bool changed = false;
  for (; *str; str++)
    if (c_isupper (*str))
      {
        changed = true;
        *str = c_tolower (*str);
      }
  return changed;
}

static const char *
init_seps (enum url_scheme scheme)
{
  static char seps[8] = ":/";
  char *p = seps + 2;
  int flags = supported_schemes[scheme].flags;

  if (flags & scm_has_params)
    *p++ = ';';
  if (flags & scm_has_query)
    *p++ = '?';
  if (flags & scm_has_fragment)
    *p++ = '#';
  *p = '\0';
  return seps;
}

static const char *parse_errors[] = {
#define PE_NO_ERROR                     0
  N_("No error"),
#define PE_UNSUPPORTED_SCHEME           1
  N_("Unsupported scheme %s"), /* support for format token only here */
#define PE_MISSING_SCHEME               2
  N_("Scheme missing"),
#define PE_INVALID_HOST_NAME            3
  N_("Invalid host name"),
#define PE_BAD_PORT_NUMBER              4
  N_("Bad port number"),
#define PE_INVALID_USER_NAME            5
  N_("Invalid user name"),
#define PE_UNTERMINATED_IPV6_ADDRESS    6
  N_("Unterminated IPv6 numeric address"),
#define PE_IPV6_NOT_SUPPORTED           7
  N_("IPv6 addresses not supported"),
#define PE_INVALID_IPV6_ADDRESS         8
  N_("Invalid IPv6 numeric address")
};

/* Parse a URL.

   Return a new struct url if successful, NULL on error.  In case of
   error, and if ERROR is not NULL, also set *ERROR to the appropriate
   error code. */
struct url *
url_parse (const char *url, int *error, struct iri *iri, bool percent_encode)
{
  struct url *u;
  const char *p;
  bool path_modified, host_modified;

  enum url_scheme scheme;
  const char *seps;

  const char *uname_b,     *uname_e;
  const char *host_b,      *host_e;
  const char *path_b,      *path_e;
  const char *params_b,    *params_e;
  const char *query_b,     *query_e;
  const char *fragment_b,  *fragment_e;

  int port;
  char *user = NULL, *passwd = NULL;

  const char *url_encoded = NULL;

  int error_code;

  scheme = url_scheme (url);
  if (scheme == SCHEME_INVALID)
    {
      if (url_has_scheme (url))
        error_code = PE_UNSUPPORTED_SCHEME;
      else
        error_code = PE_MISSING_SCHEME;
      goto error;
    }

  url_encoded = url;

  if (iri && iri->utf8_encode)
    {
      char *new_url = NULL;

      iri->utf8_encode = remote_to_utf8 (iri, iri->orig_url ? iri->orig_url : url, &new_url);
      if (!iri->utf8_encode)
        new_url = NULL;
      else
        {
          xfree (iri->orig_url);
          iri->orig_url = xstrdup (url);
          url_encoded = reencode_escapes (new_url);
          if (url_encoded != new_url)
            xfree (new_url);
          percent_encode = false;
        }
    }

  if (percent_encode)
    url_encoded = reencode_escapes (url);

  p = url_encoded;
  p += strlen (supported_schemes[scheme].leading_string);
  uname_b = p;
  p = url_skip_credentials (p);
  uname_e = p;

  /* scheme://user:pass@host[:port]... */
  /*                    ^              */

  /* We attempt to break down the URL into the components path,
     params, query, and fragment.  They are ordered like this:

       scheme://host[:port][/path][;params][?query][#fragment]  */

  path_b     = path_e     = NULL;
  params_b   = params_e   = NULL;
  query_b    = query_e    = NULL;
  fragment_b = fragment_e = NULL;

  /* Initialize separators for optional parts of URL, depending on the
     scheme.  For example, FTP has params, and HTTP and HTTPS have
     query string and fragment. */
  seps = init_seps (scheme);

  host_b = p;

  if (*p == '[')
    {
      /* Handle IPv6 address inside square brackets.  Ideally we'd
         just look for the terminating ']', but rfc2732 mandates
         rejecting invalid IPv6 addresses.  */

      /* The address begins after '['. */
      host_b = p + 1;
      host_e = strchr (host_b, ']');

      if (!host_e)
        {
          error_code = PE_UNTERMINATED_IPV6_ADDRESS;
          goto error;
        }

#ifdef ENABLE_IPV6
      /* Check if the IPv6 address is valid. */
      if (!is_valid_ipv6_address(host_b, host_e))
        {
          error_code = PE_INVALID_IPV6_ADDRESS;
          goto error;
        }

      /* Continue parsing after the closing ']'. */
      p = host_e + 1;
#else
      error_code = PE_IPV6_NOT_SUPPORTED;
      goto error;
#endif

      /* The closing bracket must be followed by a separator or by the
         null char.  */
      /* http://[::1]... */
      /*             ^   */
      if (!strchr (seps, *p))
        {
          /* Trailing garbage after []-delimited IPv6 address. */
          error_code = PE_INVALID_HOST_NAME;
          goto error;
        }
    }
  else
    {
      p = strpbrk_or_eos (p, seps);
      host_e = p;
    }
  ++seps;                       /* advance to '/' */

  if (host_b == host_e)
    {
      error_code = PE_INVALID_HOST_NAME;
      goto error;
    }

  port = scheme_default_port (scheme);
  if (*p == ':')
    {
      const char *port_b, *port_e, *pp;

      /* scheme://host:port/tralala */
      /*              ^             */
      ++p;
      port_b = p;
      p = strpbrk_or_eos (p, seps);
      port_e = p;

      /* Allow empty port, as per rfc2396. */
      if (port_b != port_e)
        for (port = 0, pp = port_b; pp < port_e; pp++)
          {
            if (!c_isdigit (*pp))
              {
                /* http://host:12randomgarbage/blah */
                /*               ^                  */
                error_code = PE_BAD_PORT_NUMBER;
                goto error;
              }
            port = 10 * port + (*pp - '0');
            /* Check for too large port numbers here, before we have
               a chance to overflow on bogus port values.  */
            if (port > 0xffff)
              {
                error_code = PE_BAD_PORT_NUMBER;
                goto error;
              }
          }
    }
  /* Advance to the first separator *after* '/' (either ';' or '?',
     depending on the scheme).  */
  ++seps;

  /* Get the optional parts of URL, each part being delimited by
     current location and the position of the next separator.  */
#define GET_URL_PART(sepchar, var) do {                         \
  if (*p == sepchar)                                            \
    var##_b = ++p, var##_e = p = strpbrk_or_eos (p, seps);      \
  ++seps;                                                       \
} while (0)

  GET_URL_PART ('/', path);
  if (supported_schemes[scheme].flags & scm_has_params)
    GET_URL_PART (';', params);
  if (supported_schemes[scheme].flags & scm_has_query)
    GET_URL_PART ('?', query);
  if (supported_schemes[scheme].flags & scm_has_fragment)
    GET_URL_PART ('#', fragment);

#undef GET_URL_PART
  assert (*p == 0);

  if (uname_b != uname_e)
    {
      /* http://user:pass@host */
      /*        ^         ^    */
      /*     uname_b   uname_e */
      if (!parse_credentials (uname_b, uname_e - 1, &user, &passwd))
        {
          error_code = PE_INVALID_USER_NAME;
          goto error;
        }
    }

  u = xnew0 (struct url);
  u->scheme = scheme;
  u->host   = strdupdelim (host_b, host_e);
  u->port   = port;
  u->user   = user;
  u->passwd = passwd;

  u->path = strdupdelim (path_b, path_e);
  path_modified = path_simplify (scheme, u->path);
  split_path (u->path, &u->dir, &u->file);

  host_modified = lowercase_str (u->host);

  /* Decode %HH sequences in host name.  This is important not so much
     to support %HH sequences in host names (which other browser
     don't), but to support binary characters (which will have been
     converted to %HH by reencode_escapes).  */
  if (strchr (u->host, '%'))
    {
      url_unescape (u->host);
      host_modified = true;

      /* check for invalid control characters in host name */
      for (p = u->host; *p; p++)
        {
          if (c_iscntrl(*p))
            {
              url_free(u);
              error_code = PE_INVALID_HOST_NAME;
              goto error;
            }
        }

      /* Apply IDNA regardless of iri->utf8_encode status */
      if (opt.enable_iri && iri)
        {
          char *new = idn_encode (iri, u->host);
          if (new)
            {
              xfree (u->host);
              u->host = new;
              host_modified = true;
            }
        }
    }

  if (params_b)
    u->params = strdupdelim (params_b, params_e);
  if (query_b)
    u->query = strdupdelim (query_b, query_e);
  if (fragment_b)
    u->fragment = strdupdelim (fragment_b, fragment_e);

  if (opt.enable_iri || path_modified || u->fragment || host_modified || path_b == path_e)
    {
      /* If we suspect that a transformation has rendered what
         url_string might return different from URL_ENCODED, rebuild
         u->url using url_string.  */
      u->url = url_string (u, URL_AUTH_SHOW);

      if (url_encoded != url)
        xfree (url_encoded);
    }
  else
    {
      if (url_encoded == url)
        u->url = xstrdup (url);
      else
        u->url = (char *) url_encoded;
    }

  return u;

 error:
  /* Cleanup in case of error: */
  if (url_encoded && url_encoded != url)
    xfree (url_encoded);

  /* Transmit the error code to the caller, if the caller wants to
     know.  */
  if (error)
    *error = error_code;
  return NULL;
}

/* Return the error message string from ERROR_CODE, which should have
   been retrieved from url_parse.  The error message is translated.  */

char *
url_error (const char *url, int error_code)
{
  assert (error_code >= 0 && ((size_t) error_code) < countof (parse_errors));

  if (error_code == PE_UNSUPPORTED_SCHEME)
    {
      char *error, *p;
      char *scheme = xstrdup (url);
      assert (url_has_scheme (url));

      if ((p = strchr (scheme, ':')))
        *p = '\0';
      if (!c_strcasecmp (scheme, "https"))
        error = aprintf (_("HTTPS support not compiled in"));
      else
        error = aprintf (_(parse_errors[error_code]), quote (scheme));
      xfree (scheme);

      return error;
    }
  else
    return xstrdup (_(parse_errors[error_code]));
}

/* Split PATH into DIR and FILE.  PATH comes from the URL and is
   expected to be URL-escaped.

   The path is split into directory (the part up to the last slash)
   and file (the part after the last slash), which are subsequently
   unescaped.  Examples:

   PATH                 DIR           FILE
   "foo/bar/baz"        "foo/bar"     "baz"
   "foo/bar/"           "foo/bar"     ""
   "foo"                ""            "foo"
   "foo/bar/baz%2fqux"  "foo/bar"     "baz/qux" (!)

   DIR and FILE are freshly allocated.  */

static void
split_path (const char *path, char **dir, char **file)
{
  char *last_slash = strrchr (path, '/');
  if (!last_slash)
    {
      *dir = xstrdup ("");
      *file = xstrdup (path);
    }
  else
    {
      *dir = strdupdelim (path, last_slash);
      *file = xstrdup (last_slash + 1);
    }
  url_unescape (*dir);
  url_unescape (*file);
}

/* Note: URL's "full path" is the path with the query string and
   params appended.  The "fragment" (#foo) is intentionally ignored,
   but that might be changed.  For example, if the original URL was
   "http://host:port/foo/bar/baz;bullshit?querystring#uselessfragment",
   the full path will be "/foo/bar/baz;bullshit?querystring".  */

/* Return the length of the full path, without the terminating
   zero.  */

static int
full_path_length (const struct url *url)
{
  int len = 0;

#define FROB(el) if (url->el) len += 1 + strlen (url->el)

  FROB (path);
  FROB (params);
  FROB (query);

#undef FROB

  return len;
}

/* Write out the full path. */

static void
full_path_write (const struct url *url, char *where)
{
#define FROB(el, chr) do {                      \
  char *f_el = url->el;                         \
  if (f_el) {                                   \
    int l = strlen (f_el);                      \
    *where++ = chr;                             \
    memcpy (where, f_el, l);                    \
    where += l;                                 \
  }                                             \
} while (0)

  FROB (path, '/');
  FROB (params, ';');
  FROB (query, '?');

#undef FROB
}

/* Public function for getting the "full path".  E.g. if u->path is
   "foo/bar" and u->query is "param=value", full_path will be
   "/foo/bar?param=value". */

char *
url_full_path (const struct url *url)
{
  int length = full_path_length (url);
  char *full_path = xmalloc (length + 1);

  full_path_write (url, full_path);
  full_path[length] = '\0';

  return full_path;
}

/* Unescape CHR in an otherwise escaped STR.  Used to selectively
   escaping of certain characters, such as "/" and ":".  Returns a
   count of unescaped chars.  */

static void
unescape_single_char (char *str, char chr)
{
  const char c1 = XNUM_TO_DIGIT (chr >> 4);
  const char c2 = XNUM_TO_DIGIT (chr & 0xf);
  char *h = str;                /* hare */
  char *t = str;                /* tortoise */
  for (; *h; h++, t++)
    {
      if (h[0] == '%' && h[1] == c1 && h[2] == c2)
        {
          *t = chr;
          h += 2;
        }
      else
        *t = *h;
    }
  *t = '\0';
}

/* Escape unsafe and reserved characters, except for the slash
   characters.  */

static char *
url_escape_dir (const char *dir)
{
  char *newdir = url_escape_1 (dir, urlchr_unsafe | urlchr_reserved, 1);
  if (newdir == dir)
    return (char *)dir;

  unescape_single_char (newdir, '/');
  return newdir;
}

/* Sync u->path and u->url with u->dir and u->file.  Called after
   u->file or u->dir have been changed, typically by the FTP code.  */

static void
sync_path (struct url *u)
{
  char *newpath, *efile, *edir;

  xfree (u->path);

  /* u->dir and u->file are not escaped.  URL-escape them before
     reassembling them into u->path.  That way, if they contain
     separators like '?' or even if u->file contains slashes, the
     path will be correctly assembled.  (u->file can contain slashes
     if the URL specifies it with %2f, or if an FTP server returns
     it.)  */
  edir = url_escape_dir (u->dir);
  efile = url_escape_1 (u->file, urlchr_unsafe | urlchr_reserved, 1);

  if (!*edir)
    newpath = xstrdup (efile);
  else
    {
      int dirlen = strlen (edir);
      int filelen = strlen (efile);

      /* Copy "DIR/FILE" to newpath. */
      char *p = newpath = xmalloc (dirlen + 1 + filelen + 1);
      memcpy (p, edir, dirlen);
      p += dirlen;
      *p++ = '/';
      memcpy (p, efile, filelen);
      p += filelen;
      *p = '\0';
    }

  u->path = newpath;

  if (edir != u->dir)
    xfree (edir);
  if (efile != u->file)
    xfree (efile);

  /* Regenerate u->url as well.  */
  xfree (u->url);
  u->url = url_string (u, URL_AUTH_SHOW);
}

/* Mutators.  Code in ftp.c insists on changing u->dir and u->file.
   This way we can sync u->path and u->url when they get changed.  */

void
url_set_dir (struct url *url, const char *newdir)
{
  xfree (url->dir);
  url->dir = xstrdup (newdir);
  sync_path (url);
}

void
url_set_file (struct url *url, const char *newfile)
{
  xfree (url->file);
  url->file = xstrdup (newfile);
  sync_path (url);
}

void
url_free (struct url *url)
{
  if (url)
    {
      xfree (url->host);

      xfree (url->path);
      xfree (url->url);

      xfree (url->params);
      xfree (url->query);
      xfree (url->fragment);
      xfree (url->user);
      xfree (url->passwd);

      xfree (url->dir);
      xfree (url->file);

      xfree (url);
    }
}

/* Create all the necessary directories for PATH (a file).  Calls
   make_directory internally.  */
int
mkalldirs (const char *path)
{
  const char *p;
  char *t;
  struct stat st;
  int res;

  p = path + strlen (path);
  for (; *p != '/' && p != path; p--)
    ;

  /* Don't create if it's just a file.  */
  if ((p == path) && (*p != '/'))
    return 0;
  t = strdupdelim (path, p);

  /* Check whether the directory exists.  */
  if ((stat (t, &st) == 0))
    {
      if (S_ISDIR (st.st_mode))
        {
          xfree (t);
          return 0;
        }
      else
        {
          /* If the dir exists as a file name, remove it first.  This
             is *only* for Wget to work with buggy old CERN http
             servers.  Here is the scenario: When Wget tries to
             retrieve a directory without a slash, e.g.
             http://foo/bar (bar being a directory), CERN server will
             not redirect it too http://foo/bar/ -- it will generate a
             directory listing containing links to bar/file1,
             bar/file2, etc.  Wget will lose because it saves this
             HTML listing to a file `bar', so it cannot create the
             directory.  To work around this, if the file of the same
             name exists, we just remove it and create the directory
             anyway.  */
          DEBUGP (("Removing %s because of directory danger!\n", t));
          if (unlink (t))
            logprintf (LOG_NOTQUIET, "Failed to unlink %s (%d): %s\n",
                       t, errno, strerror(errno));
        }
    }
  res = make_directory (t);
  if (res != 0)
    logprintf (LOG_NOTQUIET, "%s: %s\n", t, strerror (errno));
  xfree (t);
  return res;
}

/* Functions for constructing the file name out of URL components.  */

/* A growable string structure, used by url_file_name and friends.
   This should perhaps be moved to utils.c.

   The idea is to have a convenient and efficient way to construct a
   string by having various functions append data to it.  Instead of
   passing the obligatory BASEVAR, SIZEVAR and TAILPOS to all the
   functions in questions, we pass the pointer to this struct.

   Functions that write to the members in this struct must make sure
   that base remains null terminated by calling append_null().
   */

struct growable {
  char *base;
  int size;   /* memory allocated */
  int tail;   /* string length */
};

/* Ensure that the string can accept APPEND_COUNT more characters past
   the current TAIL position.  If necessary, this will grow the string
   and update its allocated size.  If the string is already large
   enough to take TAIL+APPEND_COUNT characters, this does nothing.  */
#define GROW(g, append_size) do {                                       \
  struct growable *G_ = g;                                              \
  DO_REALLOC (G_->base, G_->size, G_->tail + append_size, char);        \
} while (0)

/* Return the tail position of the string. */
#define TAIL(r) ((r)->base + (r)->tail)

/* Move the tail position by APPEND_COUNT characters. */
#define TAIL_INCR(r, append_count) ((r)->tail += append_count)


/* Append NULL to DEST. */
static void
append_null (struct growable *dest)
{
  GROW (dest, 1);
  *TAIL (dest) = 0;
}

/* Append CH to DEST. */
static void
append_char (char ch, struct growable *dest)
{
  if (ch)
    {
      GROW (dest, 1);
      *TAIL (dest) = ch;
      TAIL_INCR (dest, 1);
    }

  append_null (dest);
}

/* Append the string STR to DEST. */
static void
append_string (const char *str, struct growable *dest)
{
  int l = strlen (str);

  if (l)
    {
      GROW (dest, l);
      memcpy (TAIL (dest), str, l);
      TAIL_INCR (dest, l);
    }

  append_null (dest);
}


enum {
  filechr_not_unix    = 1,      /* unusable on Unix, / and \0 */
  filechr_not_vms     = 2,      /* unusable on VMS (ODS5), 0x00-0x1F * ? */
  filechr_not_windows = 4,      /* unusable on Windows, one of \|/<>?:*" */
  filechr_control     = 8       /* a control character, e.g. 0-31 */
};

#define FILE_CHAR_TEST(c, mask) \
    ((opt.restrict_files_nonascii && !c_isascii ((unsigned char)(c))) || \
    (filechr_table[(unsigned char)(c)] & (mask)))

/* Shorthands for the table: */
#define U filechr_not_unix
#define V filechr_not_vms
#define W filechr_not_windows
#define C filechr_control

#define UVWC U|V|W|C
#define UW U|W
#define VC V|C
#define VW V|W

/* Table of characters unsafe under various conditions (see above).

   Arguably we could also claim `%' to be unsafe, since we use it as
   the escape character.  If we ever want to be able to reliably
   translate file name back to URL, this would become important
   crucial.  Right now, it's better to be minimal in escaping.  */

static const unsigned char filechr_table[256] =
{
UVWC, VC, VC, VC,  VC, VC, VC, VC,   /* NUL SOH STX ETX  EOT ENQ ACK BEL */
  VC, VC, VC, VC,  VC, VC, VC, VC,   /* BS  HT  LF  VT   FF  CR  SO  SI  */
  VC, VC, VC, VC,  VC, VC, VC, VC,   /* DLE DC1 DC2 DC3  DC4 NAK SYN ETB */
  VC, VC, VC, VC,  VC, VC, VC, VC,   /* CAN EM  SUB ESC  FS  GS  RS  US  */
   0,  0,  W,  0,   0,  0,  0,  0,   /* SP  !   "   #    $   %   &   '   */
   0,  0, VW,  0,   0,  0,  0, UW,   /* (   )   *   +    ,   -   .   /   */
   0,  0,  0,  0,   0,  0,  0,  0,   /* 0   1   2   3    4   5   6   7   */
   0,  0,  W,  0,   W,  0,  W, VW,   /* 8   9   :   ;    <   =   >   ?   */
   0,  0,  0,  0,   0,  0,  0,  0,   /* @   A   B   C    D   E   F   G   */
   0,  0,  0,  0,   0,  0,  0,  0,   /* H   I   J   K    L   M   N   O   */
   0,  0,  0,  0,   0,  0,  0,  0,   /* P   Q   R   S    T   U   V   W   */
   0,  0,  0,  0,   W,  0,  0,  0,   /* X   Y   Z   [    \   ]   ^   _   */
   0,  0,  0,  0,   0,  0,  0,  0,   /* `   a   b   c    d   e   f   g   */
   0,  0,  0,  0,   0,  0,  0,  0,   /* h   i   j   k    l   m   n   o   */
   0,  0,  0,  0,   0,  0,  0,  0,   /* p   q   r   s    t   u   v   w   */
   0,  0,  0,  0,   W,  0,  0,  C,   /* x   y   z   {    |   }   ~   DEL */

  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, /* 128-143 */
  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, /* 144-159 */
  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,

  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
};
#undef U
#undef V
#undef W
#undef C
#undef UW
#undef UVWC
#undef VC
#undef VW

/* FN_PORT_SEP is the separator between host and port in file names
   for non-standard port numbers.  On Unix this is normally ':', as in
   "www.xemacs.org:4001/index.html".  Under Windows, we set it to +
   because Windows can't handle ':' in file names.  */
#define FN_PORT_SEP  (opt.restrict_files_os != restrict_windows ? ':' : '+')

/* FN_QUERY_SEP is the separator between the file name and the URL
   query, normally '?'.  Because VMS and Windows cannot handle '?' in a
   file name, we use '@' instead there.  */
#define FN_QUERY_SEP \
 (((opt.restrict_files_os != restrict_vms) && \
   (opt.restrict_files_os != restrict_windows)) ? '?' : '@')
#define FN_QUERY_SEP_STR \
 (((opt.restrict_files_os != restrict_vms) && \
   (opt.restrict_files_os != restrict_windows)) ? "?" : "@")

/* Quote path element, characters in [b, e), as file name, and append
   the quoted string to DEST.  Each character is quoted as per
   file_unsafe_char and the corresponding table.

   If ESCAPED is true, the path element is considered to be
   URL-escaped and will be unescaped prior to inspection.  */

static void
append_uri_pathel (const char *b, const char *e, bool escaped,
                   struct growable *dest)
{
  const char *p;
  int quoted, outlen;

  int mask;
  if (opt.restrict_files_os == restrict_unix)
    mask = filechr_not_unix;
  else if (opt.restrict_files_os == restrict_vms)
    mask = filechr_not_vms;
  else
    mask = filechr_not_windows;
  if (opt.restrict_files_ctrl)
    mask |= filechr_control;

  /* Copy [b, e) to PATHEL and URL-unescape it. */
  if (escaped)
    {
      char *unescaped;
      BOUNDED_TO_ALLOCA (b, e, unescaped);
      url_unescape (unescaped);
      b = unescaped;
      e = unescaped + strlen (unescaped);
    }

  /* Defang ".." when found as component of path.  Remember that path
     comes from the URL and might contain malicious input.  */
  if (e - b == 2 && b[0] == '.' && b[1] == '.')
    {
      b = "%2E%2E";
      e = b + 6;
    }

  /* Walk the PATHEL string and check how many characters we'll need
     to quote.  */
  quoted = 0;
  for (p = b; p < e; p++)
    if (FILE_CHAR_TEST (*p, mask))
      ++quoted;

  /* Calculate the length of the output string.  e-b is the input
     string length.  Each quoted char introduces two additional
     characters in the string, hence 2*quoted.  */
  outlen = (e - b) + (2 * quoted);
  GROW (dest, outlen);

  if (!quoted)
    {
      /* If there's nothing to quote, we can simply append the string
         without processing it again.  */
      memcpy (TAIL (dest), b, outlen);
    }
  else
    {
      char *q = TAIL (dest);
      for (p = b; p < e; p++)
        {
          if (!FILE_CHAR_TEST (*p, mask))
            *q++ = *p;
          else
            {
              unsigned char ch = *p;
              *q++ = '%';
              *q++ = XNUM_TO_DIGIT (ch >> 4);
              *q++ = XNUM_TO_DIGIT (ch & 0xf);
            }
        }
      assert (q - TAIL (dest) == outlen);
    }

  /* Perform inline case transformation if required.  */
  if (opt.restrict_files_case == restrict_lowercase
      || opt.restrict_files_case == restrict_uppercase)
    {
      char *q;
      for (q = TAIL (dest); q < TAIL (dest) + outlen; ++q)
        {
          if (opt.restrict_files_case == restrict_lowercase)
            *q = c_tolower (*q);
          else
            *q = c_toupper (*q);
        }
    }

  TAIL_INCR (dest, outlen);
  append_null (dest);
}

#ifdef HAVE_ICONV
static char *
convert_fname (char *fname)
{
  char *converted_fname = fname;
  const char *from_encoding = opt.encoding_remote;
  const char *to_encoding = opt.locale;
  iconv_t cd;
  size_t len, done, inlen, outlen;
  char *s;
  const char *orig_fname = fname;

  /* Defaults for remote and local encodings.  */
  if (!from_encoding)
    from_encoding = "UTF-8";
  if (!to_encoding)
    to_encoding = nl_langinfo (CODESET);

  cd = iconv_open (to_encoding, from_encoding);
  if (cd == (iconv_t) (-1))
    logprintf (LOG_VERBOSE, _ ("Conversion from %s to %s isn't supported\n"),
               quote (from_encoding), quote (to_encoding));
  else
    {
      inlen = strlen (fname);
      len = outlen = inlen * 2;
      converted_fname = s = xmalloc (outlen + 1);
      done = 0;

      for (;;)
        {
          errno = 0;
          if (iconv (cd, (ICONV_CONST char **) &fname, &inlen, &s, &outlen) == 0
              && iconv (cd, NULL, NULL, &s, &outlen) == 0)
            {
              *(converted_fname + len - outlen - done) = '\0';
              iconv_close (cd);
              DEBUGP (("Converted file name '%s' (%s) -> '%s' (%s)\n",
                       orig_fname, from_encoding, converted_fname, to_encoding));
              xfree (orig_fname);
              return converted_fname;
            }

          /* Incomplete or invalid multibyte sequence */
          if (errno == EINVAL || errno == EILSEQ || errno == 0)
            {
              if (errno)
                logprintf (LOG_VERBOSE,
                           _ ("Incomplete or invalid multibyte sequence encountered\n"));
              else
                logprintf (LOG_VERBOSE,
                           _ ("Unconvertable multibyte sequence encountered\n"));
              xfree (converted_fname);
              converted_fname = (char *) orig_fname;
              break;
            }
          else if (errno == E2BIG) /* Output buffer full */
            {
              done = len;
              len = outlen = done + inlen * 2;
              converted_fname = xrealloc (converted_fname, outlen + 1);
              s = converted_fname + done;
            }
          else /* Weird, we got an unspecified error */
            {
              logprintf (LOG_VERBOSE, _ ("Unhandled errno %d\n"), errno);
              xfree (converted_fname);
              converted_fname = (char *) orig_fname;
              break;
            }
        }
      DEBUGP (("Failed to convert file name '%s' (%s) -> '?' (%s)\n",
               orig_fname, from_encoding, to_encoding));
    }

    iconv_close(cd);

  return converted_fname;
}
#else
static char *
convert_fname (char *fname)
{
  return fname;
}
#endif

/* Append to DEST the directory structure that corresponds the
   directory part of URL's path.  For example, if the URL is
   http://server/dir1/dir2/file, this appends "/dir1/dir2".

   Each path element ("dir1" and "dir2" in the above example) is
   examined, url-unescaped, and re-escaped as file name element.

   Additionally, it cuts as many directories from the path as
   specified by opt.cut_dirs.  For example, if opt.cut_dirs is 1, it
   will produce "bar" for the above example.  For 2 or more, it will
   produce "".

   Each component of the path is quoted for use as file name.  */

static void
append_dir_structure (const struct url *u, struct growable *dest)
{
  char *pathel, *next;
  int cut = opt.cut_dirs;

  /* Go through the path components, de-URL-quote them, and quote them
     (if necessary) as file names.  */

  pathel = u->path;
  for (; (next = strchr (pathel, '/')) != NULL; pathel = next + 1)
    {
      if (cut-- > 0)
        continue;
      if (pathel == next)
        /* Ignore empty pathels.  */
        continue;

      if (dest->tail)
        append_char ('/', dest);
      append_uri_pathel (pathel, next, true, dest);
    }
}

/* Return a unique file name that matches the given URL as well as
   possible.  Does not create directories on the file system.  */

char *
url_file_name (const struct url *u, char *replaced_filename)
{
  struct growable fnres;        /* stands for "file name result" */
  struct growable temp_fnres;

  const char *u_file;
  char *fname, *unique, *fname_len_check;
  const char *index_filename = "index.html"; /* The default index file is index.html */
  size_t max_length;

  fnres.base = NULL;
  fnres.size = 0;
  fnres.tail = 0;

  temp_fnres.base = NULL;
  temp_fnres.size = 0;
  temp_fnres.tail = 0;

  /* If an alternative index file was defined, change index_filename */
  if (opt.default_page)
    index_filename = opt.default_page;


  /* Start with the directory prefix, if specified. */
  if (opt.dir_prefix)
    append_string (opt.dir_prefix, &fnres);

  /* If "dirstruct" is turned on (typically the case with -r), add
     the host and port (unless those have been turned off) and
     directory structure.  */
  if (opt.dirstruct)
    {
      if (opt.protocol_directories)
        {
          if (fnres.tail)
            append_char ('/', &fnres);
          append_string (supported_schemes[u->scheme].name, &fnres);
        }
      if (opt.add_hostdir)
        {
          if (fnres.tail)
            append_char ('/', &fnres);
          if (0 != strcmp (u->host, ".."))
            append_string (u->host, &fnres);
          else
            /* Host name can come from the network; malicious DNS may
               allow ".." to be resolved, causing us to write to
               "../<file>".  Defang such host names.  */
            append_string ("%2E%2E", &fnres);
          if (u->port != scheme_default_port (u->scheme))
            {
              char portstr[24];
              number_to_string (portstr, u->port);
              append_char (FN_PORT_SEP, &fnres);
              append_string (portstr, &fnres);
            }
        }

      append_dir_structure (u, &fnres);
    }

  if (!replaced_filename)
    {
      /* Create the filename. */
      u_file = *u->file ? u->file : index_filename;

      /* Append "?query" to the file name, even if empty,
       * and create fname_len_check. */
      if (u->query)
        fname_len_check = concat_strings (u_file, FN_QUERY_SEP_STR, u->query, NULL);
      else
        fname_len_check = strdupdelim (u_file, u_file + strlen (u_file));

      /* convert before concat with local path */
      fname_len_check = convert_fname (fname_len_check);
    }
  else
    {
      u_file = replaced_filename;
      fname_len_check = strdupdelim (u_file, u_file + strlen (u_file));
    }

  append_uri_pathel (fname_len_check,
    fname_len_check + strlen (fname_len_check), false, &temp_fnres);

  /* Zero-terminate the temporary file name. */
  append_char ('\0', &temp_fnres);

  /* Check that the length of the file name is acceptable. */
#ifdef WINDOWS
  if (MAX_PATH > (fnres.tail + CHOMP_BUFFER + 2))
    {
      max_length = MAX_PATH - (fnres.tail + CHOMP_BUFFER + 2);
      /* FIXME: In Windows a filename is usually limited to 255 characters.
      To really be accurate you could call GetVolumeInformation() to get
      lpMaximumComponentLength
      */
      if (max_length > 255)
        {
          max_length = 255;
        }
    }
  else
    {
      max_length = 0;
    }
#else
  max_length = get_max_length (fnres.base, fnres.tail, _PC_NAME_MAX) - CHOMP_BUFFER;
#endif
  if (max_length > 0 && strlen (temp_fnres.base) > max_length)
    {
      logprintf (LOG_NOTQUIET, "The name is too long, %lu chars total.\n",
          (unsigned long) strlen (temp_fnres.base));
      logprintf (LOG_NOTQUIET, "Trying to shorten...\n");

      /* Shorten the file name. */
      temp_fnres.base[max_length] = '\0';

      logprintf (LOG_NOTQUIET, "New name is %s.\n", temp_fnres.base);
    }

  xfree (fname_len_check);

  /* The filename has already been 'cleaned' by append_uri_pathel() above.  So,
   * just append it. */
  if (fnres.tail)
    append_char ('/', &fnres);
  append_string (temp_fnres.base, &fnres);

  fname = fnres.base;

  /* Make a final check that the path length is acceptable? */
  /* TODO: check fnres.base for path length problem */

  xfree (temp_fnres.base);

  /* Check the cases in which the unique extensions are not used:
     1) Clobbering is turned off (-nc).
     2) Retrieval with regetting.
     3) Timestamping is used.
     4) Hierarchy is built.
     5) Backups are specified.

     The exception is the case when file does exist and is a
     directory (see `mkalldirs' for explanation).  */

  if (ALLOW_CLOBBER
      && !(file_exists_p (fname, NULL) && !file_non_directory_p (fname)))
    {
      unique = fname;
    }
  else
    {
      unique = unique_name (fname, true);
      if (unique != fname)
        xfree (fname);
    }

/* On VMS, alter the name as required. */
#ifdef __VMS
  {
    char *unique2;

    unique2 = ods_conform( unique);
    if (unique2 != unique)
      {
        xfree (unique);
        unique = unique2;
      }
  }
#endif /* def __VMS */

  return unique;
}

/* Resolve "." and ".." elements of PATH by destructively modifying
   PATH and return true if PATH has been modified, false otherwise.

   The algorithm is in spirit similar to the one described in rfc1808,
   although implemented differently, in one pass.  To recap, path
   elements containing only "." are removed, and ".." is taken to mean
   "back up one element".  Single leading and trailing slashes are
   preserved.

   For example, "a/b/c/./../d/.." will yield "a/b/".  More exhaustive
   test examples are provided below.  If you change anything in this
   function, run test_path_simplify to make sure you haven't broken a
   test case.  */

static bool
path_simplify (enum url_scheme scheme, char *path)
{
  char *h = path;               /* hare */
  char *t = path;               /* tortoise */
  char *beg = path;
  char *end = strchr (path, '\0');

  while (h < end)
    {
      /* Hare should be at the beginning of a path element. */

      if (h[0] == '.' && (h[1] == '/' || h[1] == '\0'))
        {
          /* Ignore "./". */
          h += 2;
        }
      else if (h[0] == '.' && h[1] == '.' && (h[2] == '/' || h[2] == '\0'))
        {
          /* Handle "../" by retreating the tortoise by one path
             element -- but not past beginning.  */
          if (t > beg)
            {
              /* Move backwards until T hits the beginning of the
                 previous path element or the beginning of path. */
              for (--t; t > beg && t[-1] != '/'; t--)
                ;
            }
          else if (scheme == SCHEME_FTP
#ifdef HAVE_SSL
              || scheme == SCHEME_FTPS
#endif
              )
            {
              /* If we're at the beginning, copy the "../" literally
                 and move the beginning so a later ".." doesn't remove
                 it.  This violates RFC 3986; but we do it for FTP
                 anyway because there is otherwise no way to get at a
                 parent directory, when the FTP server drops us in a
                 non-root directory (which is not uncommon). */
              beg = t + 3;
              goto regular;
            }
          h += 3;
        }
      else
        {
        regular:
          /* A regular path element.  If H hasn't advanced past T,
             simply skip to the next path element.  Otherwise, copy
             the path element until the next slash.  */
          if (t == h)
            {
              /* Skip the path element, including the slash.  */
              while (h < end && *h != '/')
                t++, h++;
              if (h < end)
                t++, h++;
            }
          else
            {
              /* Copy the path element, including the final slash.  */
              while (h < end && *h != '/')
                *t++ = *h++;
              if (h < end)
                *t++ = *h++;
            }
        }
    }

  if (t != h)
    *t = '\0';

  return t != h;
}

/* Return the length of URL's path.  Path is considered to be
   terminated by one or more of the ?query or ;params or #fragment,
   depending on the scheme.  */

static const char *
path_end (const char *url)
{
  enum url_scheme scheme = url_scheme (url);
  const char *seps;
  if (scheme == SCHEME_INVALID)
    scheme = SCHEME_HTTP;       /* use http semantics for rel links */
  /* +2 to ignore the first two separators ':' and '/' */
  seps = init_seps (scheme) + 2;
  return strpbrk_or_eos (url, seps);
}

/* Find the last occurrence of character C in the range [b, e), or
   NULL, if none are present.  */
#define find_last_char(b, e, c) memrchr ((b), (c), (e) - (b))

/* Merge BASE with LINK and return the resulting URI.

   Either of the URIs may be absolute or relative, complete with the
   host name, or path only.  This tries to reasonably handle all
   foreseeable cases.  It only employs minimal URL parsing, without
   knowledge of the specifics of schemes.

   I briefly considered making this function call path_simplify after
   the merging process, as rfc1738 seems to suggest.  This is a bad
   idea for several reasons: 1) it complexifies the code, and 2)
   url_parse has to simplify path anyway, so it's wasteful to boot.  */

char *
uri_merge (const char *base, const char *link)
{
  int linklength;
  const char *end;
  char *merge;

  if (url_has_scheme (link))
    return xstrdup (link);

  /* We may not examine BASE past END. */
  end = path_end (base);
  linklength = strlen (link);

  if (!*link)
    {
      /* Empty LINK points back to BASE, query string and all. */
      return xstrdup (base);
    }
  else if (*link == '?')
    {
      /* LINK points to the same location, but changes the query
         string.  Examples: */
      /* uri_merge("path",         "?new") -> "path?new"     */
      /* uri_merge("path?foo",     "?new") -> "path?new"     */
      /* uri_merge("path?foo#bar", "?new") -> "path?new"     */
      /* uri_merge("path#foo",     "?new") -> "path?new"     */
      int baselength = end - base;
      merge = xmalloc (baselength + linklength + 1);
      memcpy (merge, base, baselength);
      memcpy (merge + baselength, link, linklength);
      merge[baselength + linklength] = '\0';
    }
  else if (*link == '#')
    {
      /* uri_merge("path",         "#new") -> "path#new"     */
      /* uri_merge("path#foo",     "#new") -> "path#new"     */
      /* uri_merge("path?foo",     "#new") -> "path?foo#new" */
      /* uri_merge("path?foo#bar", "#new") -> "path?foo#new" */
      int baselength;
      const char *end1 = strchr (base, '#');
      if (!end1)
        end1 = base + strlen (base);
      baselength = end1 - base;
      merge = xmalloc (baselength + linklength + 1);
      memcpy (merge, base, baselength);
      memcpy (merge + baselength, link, linklength);
      merge[baselength + linklength] = '\0';
    }
  else if (*link == '/' && *(link + 1) == '/')
    {
      /* LINK begins with "//" and so is a net path: we need to
         replace everything after (and including) the double slash
         with LINK. */

      /* uri_merge("foo", "//new/bar")            -> "//new/bar"      */
      /* uri_merge("//old/foo", "//new/bar")      -> "//new/bar"      */
      /* uri_merge("http://old/foo", "//new/bar") -> "http://new/bar" */

      int span;
      const char *slash;
      const char *start_insert;

      /* Look for first slash. */
      slash = memchr (base, '/', end - base);
      /* If found slash and it is a double slash, then replace
         from this point, else default to replacing from the
         beginning.  */
      if (slash && *(slash + 1) == '/')
        start_insert = slash;
      else
        start_insert = base;

      span = start_insert - base;
      merge = xmalloc (span + linklength + 1);
      if (span)
        memcpy (merge, base, span);
      memcpy (merge + span, link, linklength);
      merge[span + linklength] = '\0';
    }
  else if (*link == '/')
    {
      /* LINK is an absolute path: we need to replace everything
         after (and including) the FIRST slash with LINK.

         So, if BASE is "http://host/whatever/foo/bar", and LINK is
         "/qux/xyzzy", our result should be
         "http://host/qux/xyzzy".  */
      int span;
      const char *slash;
      const char *start_insert = NULL; /* for gcc to shut up. */
      const char *pos = base;
      bool seen_slash_slash = false;
      /* We're looking for the first slash, but want to ignore
         double slash. */
    again:
      slash = memchr (pos, '/', end - pos);
      if (slash && !seen_slash_slash)
        if (*(slash + 1) == '/')
          {
            pos = slash + 2;
            seen_slash_slash = true;
            goto again;
          }

      /* At this point, SLASH is the location of the first / after
         "//", or the first slash altogether.  START_INSERT is the
         pointer to the location where LINK will be inserted.  When
         examining the last two examples, keep in mind that LINK
         begins with '/'. */

      if (!slash && !seen_slash_slash)
        /* example: "foo" */
        /*           ^    */
        start_insert = base;
      else if (!slash && seen_slash_slash)
        /* example: "http://foo" */
        /*                     ^ */
        start_insert = end;
      else if (slash && !seen_slash_slash)
        /* example: "foo/bar" */
        /*           ^        */
        start_insert = base;
      else if (slash && seen_slash_slash)
        /* example: "http://something/" */
        /*                           ^  */
        start_insert = slash;

      span = start_insert - base;
      merge = xmalloc (span + linklength + 1);
      if (span)
        memcpy (merge, base, span);
      memcpy (merge + span, link, linklength);
      merge[span + linklength] = '\0';
    }
  else
    {
      /* LINK is a relative URL: we need to replace everything
         after last slash (possibly empty) with LINK.

         So, if BASE is "whatever/foo/bar", and LINK is "qux/xyzzy",
         our result should be "whatever/foo/qux/xyzzy".  */
      bool need_explicit_slash = false;
      int span;
      const char *start_insert;
      const char *last_slash = find_last_char (base, end, '/');
      if (!last_slash)
        {
          /* No slash found at all.  Replace what we have with LINK. */
          start_insert = base;
        }
      else if (last_slash && last_slash >= base + 2
               && last_slash[-2] == ':' && last_slash[-1] == '/')
        {
          /* example: http://host"  */
          /*                      ^ */
          start_insert = end + 1;
          need_explicit_slash = true;
        }
      else
        {
          /* example: "whatever/foo/bar" */
          /*                        ^    */
          start_insert = last_slash + 1;
        }

      span = start_insert - base;
      merge = xmalloc (span + linklength + 1);
      if (span)
        memcpy (merge, base, span);
      if (need_explicit_slash)
        merge[span - 1] = '/';
      memcpy (merge + span, link, linklength);
      merge[span + linklength] = '\0';
    }

  return merge;
}

#define APPEND(p, s) do {                       \
  int len = strlen (s);                         \
  memcpy (p, s, len);                           \
  p += len;                                     \
} while (0)

/* Use this instead of password when the actual password is supposed
   to be hidden.  We intentionally use a generic string without giving
   away the number of characters in the password, like previous
   versions did.  */
#define HIDDEN_PASSWORD "*password*"

/* Recreate the URL string from the data in URL.

   If HIDE is true (as it is when we're calling this on a URL we plan
   to print, but not when calling it to canonicalize a URL for use
   within the program), password will be hidden.  Unsafe characters in
   the URL will be quoted.  */

char *
url_string (const struct url *url, enum url_auth_mode auth_mode)
{
  int size;
  char *result, *p;
  char *quoted_host, *quoted_user = NULL, *quoted_passwd = NULL;

  int scheme_port = supported_schemes[url->scheme].default_port;
  const char *scheme_str = supported_schemes[url->scheme].leading_string;
  int fplen = full_path_length (url);

  bool brackets_around_host;

  assert (scheme_str != NULL);

  /* Make sure the user name and password are quoted. */
  if (url->user)
    {
      if (auth_mode != URL_AUTH_HIDE)
        {
          quoted_user = url_escape_allow_passthrough (url->user);
          if (url->passwd)
            {
              if (auth_mode == URL_AUTH_HIDE_PASSWD)
                quoted_passwd = (char *) HIDDEN_PASSWORD;
              else
                quoted_passwd = url_escape_allow_passthrough (url->passwd);
            }
        }
    }

  /* In the unlikely event that the host name contains non-printable
     characters, quote it for displaying to the user.  */
  quoted_host = url_escape_allow_passthrough (url->host);

  /* Undo the quoting of colons that URL escaping performs.  IPv6
     addresses may legally contain colons, and in that case must be
     placed in square brackets.  */
  if (quoted_host != url->host)
    unescape_single_char (quoted_host, ':');
  brackets_around_host = strchr (quoted_host, ':') != NULL;

  size = (strlen (scheme_str)
          + strlen (quoted_host)
          + (brackets_around_host ? 2 : 0)
          + fplen
          + 1);
  if (url->port != scheme_port)
    size += 1 + numdigit (url->port);
  if (quoted_user)
    {
      size += 1 + strlen (quoted_user);
      if (quoted_passwd)
        size += 1 + strlen (quoted_passwd);
    }

  p = result = xmalloc (size);

  APPEND (p, scheme_str);
  if (quoted_user)
    {
      APPEND (p, quoted_user);
      if (quoted_passwd)
        {
          *p++ = ':';
          APPEND (p, quoted_passwd);
        }
      *p++ = '@';
    }

  if (brackets_around_host)
    *p++ = '[';
  APPEND (p, quoted_host);
  if (brackets_around_host)
    *p++ = ']';
  if (url->port != scheme_port)
    {
      *p++ = ':';
      p = number_to_string (p, url->port);
    }

  full_path_write (url, p);
  p += fplen;
  *p++ = '\0';

  assert (p - result == size);

  if (quoted_user && quoted_user != url->user)
    xfree (quoted_user);
  if (quoted_passwd && auth_mode == URL_AUTH_SHOW
      && quoted_passwd != url->passwd)
    xfree (quoted_passwd);
  if (quoted_host != url->host)
    xfree (quoted_host);

  return result;
}

/* Return true if scheme a is similar to scheme b.

   Schemes are similar if they are equal.  If SSL is supported, schemes
   are also similar if one is http (SCHEME_HTTP) and the other is https
   (SCHEME_HTTPS).  */
bool
schemes_are_similar_p (enum url_scheme a, enum url_scheme b)
{
  if (a == b)
    return true;
#ifdef HAVE_SSL
  if ((a == SCHEME_HTTP && b == SCHEME_HTTPS)
      || (a == SCHEME_HTTPS && b == SCHEME_HTTP))
    return true;
#endif
  return false;
}

static int
getchar_from_escaped_string (const char *str, char *c)
{
  const char *p = str;

  assert (str && *str);
  assert (c);

  if (p[0] == '%')
    {
      if (!c_isxdigit(p[1]) || !c_isxdigit(p[2]))
        {
          *c = '%';
          return 1;
        }
      else
        {
          if (p[2] == 0)
            return 0; /* error: invalid string */

          *c = X2DIGITS_TO_NUM (p[1], p[2]);
          if (URL_RESERVED_CHAR(*c))
            {
              *c = '%';
              return 1;
            }
          else
            return 3;
        }
    }
  else
    {
      *c = p[0];
    }

  return 1;
}

bool
are_urls_equal (const char *u1, const char *u2)
{
  const char *p, *q;
  int pp, qq;
  char ch1, ch2;
  assert(u1 && u2);

  p = u1;
  q = u2;

  while (*p && *q
         && (pp = getchar_from_escaped_string (p, &ch1))
         && (qq = getchar_from_escaped_string (q, &ch2))
         && (c_tolower(ch1) == c_tolower(ch2)))
    {
      p += pp;
      q += qq;
    }

  return (*p == 0 && *q == 0 ? true : false);
}

#ifdef TESTING
/* Debugging and testing support for path_simplify. */

#if 0
/* Debug: run path_simplify on PATH and return the result in a new
   string.  Useful for calling from the debugger.  */
static char *
ps (char *path)
{
  char *copy = xstrdup (path);
  path_simplify (copy);
  return copy;
}
#endif

static const char *
run_test (const char *test, const char *expected_result, enum url_scheme scheme,
          bool expected_change)
{
  char *test_copy = xstrdup (test);
  bool modified = path_simplify (scheme, test_copy);

  if (0 != strcmp (test_copy, expected_result))
    {
      printf ("Failed path_simplify(\"%s\"): expected \"%s\", got \"%s\".\n",
              test, expected_result, test_copy);
      mu_assert ("", 0);
    }
  if (modified != expected_change)
    {
      if (expected_change)
        printf ("Expected modification with path_simplify(\"%s\").\n",
                test);
      else
        printf ("Expected no modification with path_simplify(\"%s\").\n",
                test);
    }
  xfree (test_copy);
  mu_assert ("", modified == expected_change);
  return NULL;
}

const char *
test_path_simplify (void)
{
  static const struct {
    const char *test, *result;
    enum url_scheme scheme;
    bool should_modify;
  } tests[] = {
    { "",                       "",             SCHEME_HTTP, false },
    { ".",                      "",             SCHEME_HTTP, true },
    { "./",                     "",             SCHEME_HTTP, true },
    { "..",                     "",             SCHEME_HTTP, true },
    { "../",                    "",             SCHEME_HTTP, true },
    { "..",                     "..",           SCHEME_FTP,  false },
    { "../",                    "../",          SCHEME_FTP,  false },
    { "foo",                    "foo",          SCHEME_HTTP, false },
    { "foo/bar",                "foo/bar",      SCHEME_HTTP, false },
    { "foo///bar",              "foo///bar",    SCHEME_HTTP, false },
    { "foo/.",                  "foo/",         SCHEME_HTTP, true },
    { "foo/./",                 "foo/",         SCHEME_HTTP, true },
    { "foo./",                  "foo./",        SCHEME_HTTP, false },
    { "foo/../bar",             "bar",          SCHEME_HTTP, true },
    { "foo/../bar/",            "bar/",         SCHEME_HTTP, true },
    { "foo/bar/..",             "foo/",         SCHEME_HTTP, true },
    { "foo/bar/../x",           "foo/x",        SCHEME_HTTP, true },
    { "foo/bar/../x/",          "foo/x/",       SCHEME_HTTP, true },
    { "foo/..",                 "",             SCHEME_HTTP, true },
    { "foo/../..",              "",             SCHEME_HTTP, true },
    { "foo/../../..",           "",             SCHEME_HTTP, true },
    { "foo/../../bar/../../baz", "baz",         SCHEME_HTTP, true },
    { "foo/../..",              "..",           SCHEME_FTP,  true },
    { "foo/../../..",           "../..",        SCHEME_FTP,  true },
    { "foo/../../bar/../../baz", "../../baz",   SCHEME_FTP,  true },
    { "a/b/../../c",            "c",            SCHEME_HTTP, true },
    { "./a/../b",               "b",            SCHEME_HTTP, true }
  };
  unsigned i;

  for (i = 0; i < countof (tests); i++)
    {
      const char *message;
      const char *test = tests[i].test;
      const char *expected_result = tests[i].result;
      enum url_scheme scheme = tests[i].scheme;
      bool  expected_change = tests[i].should_modify;

      message = run_test (test, expected_result, scheme, expected_change);
      if (message) return message;
    }
  return NULL;
}

const char *
test_append_uri_pathel(void)
{
  unsigned i;
  static const struct {
    const char *original_url;
    const char *input;
    bool escaped;
    const char *expected_result;
  } test_array[] = {
    { "http://www.yoyodyne.com/path/", "somepage.html", false, "http://www.yoyodyne.com/path/somepage.html" },
  };

  for (i = 0; i < countof(test_array); ++i)
    {
      struct growable dest;
      const char *p = test_array[i].input;

      memset (&dest, 0, sizeof (dest));

      append_string (test_array[i].original_url, &dest);
      append_uri_pathel (p, p + strlen(p), test_array[i].escaped, &dest);

      mu_assert ("test_append_uri_pathel: wrong result",
                 strcmp (dest.base, test_array[i].expected_result) == 0);
      xfree (dest.base);
    }

  return NULL;
}

const char *
test_are_urls_equal(void)
{
  unsigned i;
  static const struct {
    const char *url1;
    const char *url2;
    bool expected_result;
  } test_array[] = {
    { "http://www.adomain.com/apath/", "http://www.adomain.com/apath/",       true },
    { "http://www.adomain.com/apath/", "http://www.adomain.com/anotherpath/", false },
    { "http://www.adomain.com/apath/", "http://www.anotherdomain.com/path/",  false },
    { "http://www.adomain.com/~path/", "http://www.adomain.com/%7epath/",     true },
    { "http://www.adomain.com/longer-path/", "http://www.adomain.com/path/",  false },
    { "http://www.adomain.com/path%2f", "http://www.adomain.com/path/",       false },
  };

  for (i = 0; i < countof(test_array); ++i)
    {
      mu_assert ("test_are_urls_equal: wrong result",
                 are_urls_equal (test_array[i].url1, test_array[i].url2) == test_array[i].expected_result);
    }

  return NULL;
}

#endif /* TESTING */

/*
 * vim: et ts=2 sw=2
 */

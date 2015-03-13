/* IRI related functions.
   Copyright (C) 2008, 2009, 2010, 2011 Free Software Foundation, Inc.

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
#include <iconv.h>
#include <stringprep.h>
#include <idna.h>
#include <errno.h>

#include "utils.h"
#include "url.h"

/* RFC3987 section 3.1 mandates STD3 ASCII RULES */
#define IDNA_FLAGS  IDNA_USE_STD3_ASCII_RULES

/* Note: locale encoding is kept in options struct (opt.locale) */

/* Given a string containing "charset=XXX", return the encoding if found,
   or NULL otherwise */
char *
parse_charset (char *str)
{
  char *charset;

  if (!str || !*str)
    return NULL;

  str = strcasestr (str, "charset=");
  if (!str)
    return NULL;

  str += 8;
  charset = str;

  /* sXXXav: which chars should be banned ??? */
  while (*charset && !c_isspace (*charset))
    charset++;

  /* sXXXav: could strdupdelim return NULL ? */
  charset = strdupdelim (str, charset);

  /* Do a minimum check on the charset value */
  if (!check_encoding_name (charset))
    {
      xfree (charset);
      return NULL;
    }

  /*logprintf (LOG_VERBOSE, "parse_charset: %s\n", quote (charset));*/

  return charset;
}

/* Find the locale used, or fall back on a default value */
char *
find_locale (void)
{
  return (char *) stringprep_locale_charset ();
}

/* Basic check of an encoding name. */
bool
check_encoding_name (char *encoding)
{
  char *s = encoding;

  while (*s)
    {
      if (!c_isascii (*s) || c_isspace (*s))
        {
          logprintf (LOG_VERBOSE, _("Encoding %s isn't valid\n"), quote (encoding));
          return false;
        }

      s++;
    }

  return true;
}

/* Do the conversion according to the passed conversion descriptor cd. *out
   will contain the transcoded string on success. *out content is
   unspecified otherwise. */
static bool
do_conversion (const char *tocode, const char *fromcode, char *in, size_t inlen, char **out)
{
  iconv_t cd;
  /* sXXXav : hummm hard to guess... */
  size_t len, done, outlen;
  int invalid = 0, tooshort = 0;
  char *s, *in_org, *in_save;

  cd = iconv_open (tocode, fromcode);
  if (cd == (iconv_t)(-1))
    {
      logprintf (LOG_VERBOSE, _("Conversion from %s to %s isn't supported\n"),
                 quote (opt.locale), quote ("UTF-8"));
      return false;
    }

  /* iconv() has to work on an unescaped string */
  in_org = in;
  in_save = in = strndup(in, inlen);
  url_unescape(in);
  inlen = strlen(in);

  len = outlen = inlen * 2;
  *out = s = xmalloc (outlen + 1);
  done = 0;

  for (;;)
    {
      if (iconv (cd, &in, &inlen, out, &outlen) != (size_t)(-1))
        {
          *out = s;
          *(s + len - outlen - done) = '\0';
          xfree(in_save);
          iconv_close(cd);
          logprintf (LOG_VERBOSE, _("converted '%s' (%s) -> '%s' (%s)\n"), in_org, fromcode, *out, tocode);
          return true;
        }

      /* Incomplete or invalid multibyte sequence */
      if (errno == EINVAL || errno == EILSEQ)
        {
          if (!invalid)
            logprintf (LOG_VERBOSE,
                      _("Incomplete or invalid multibyte sequence encountered\n"));

          invalid++;
          **out = *in;
          in++;
          inlen--;
          (*out)++;
          outlen--;
        }
      else if (errno == E2BIG) /* Output buffer full */
        {
          char *new;

          tooshort++;
          done = len;
          outlen = done + inlen * 2;
          new = xmalloc (outlen + 1);
          memcpy (new, s, done);
          xfree (s);
          s = new;
          len = outlen;
          *out = s + done;
        }
      else /* Weird, we got an unspecified error */
        {
          logprintf (LOG_VERBOSE, _("Unhandled errno %d\n"), errno);
          break;
        }
    }

    xfree(in_save);
    iconv_close(cd);
    logprintf (LOG_VERBOSE, _("converted '%s' (%s) -> '%s' (%s)\n"), in_org, fromcode, *out, tocode);
    return false;
}

/* Try converting string str from locale to UTF-8. Return a new string
   on success, or str on error or if conversion isn't needed. */
const char *
locale_to_utf8 (const char *str)
{
  char *new;

  /* That shouldn't happen, just in case */
  if (!opt.locale)
    {
      logprintf (LOG_VERBOSE, _("locale_to_utf8: locale is unset\n"));
      opt.locale = find_locale ();
    }

  if (!opt.locale || !strcasecmp (opt.locale, "utf-8"))
    return str;

  if (do_conversion ("UTF-8", opt.locale, (char *) str, strlen ((char *) str), &new))
    return (const char *) new;

  return str;
}

/* Try to "ASCII encode" UTF-8 host. Return the new domain on success or NULL
   on error. */
char *
idn_encode (struct iri *i, char *host)
{
  char *new;
  int ret;

  /* Encode to UTF-8 if not done */
  if (!i->utf8_encode)
    {
      if (!remote_to_utf8 (i, (const char *) host, (const char **) &new))
          return NULL;  /* Nothing to encode or an error occured */
      host = new;
    }

  /* toASCII UTF-8 NULL terminated string */
  ret = idna_to_ascii_8z (host, &new, IDNA_FLAGS);
  if (ret != IDNA_SUCCESS)
    {
      /* sXXXav : free new when needed ! */
      logprintf (LOG_VERBOSE, _("idn_encode failed (%d): %s\n"), ret,
                 quote (idna_strerror (ret)));
      return NULL;
    }

  return new;
}

/* Try to decode an "ASCII encoded" host. Return the new domain in the locale
   on success or NULL on error. */
char *
idn_decode (char *host)
{
  char *new;
  int ret;

  ret = idna_to_unicode_8zlz (host, &new, IDNA_FLAGS);
  if (ret != IDNA_SUCCESS)
    {
      logprintf (LOG_VERBOSE, _("idn_decode failed (%d): %s\n"), ret,
                 quote (idna_strerror (ret)));
      return NULL;
    }

  return new;
}

/* Try to transcode string str from remote encoding to UTF-8. On success, *new
   contains the transcoded string. *new content is unspecified otherwise. */
bool
remote_to_utf8 (struct iri *iri, const char *str, const char **new)
{
  bool ret = false;

  if (!iri->uri_encoding)
    return false;

  /* When `i->uri_encoding' == "UTF-8" there is nothing to convert.  But we must
     test for non-ASCII symbols for correct hostname processing in `idn_encode'
     function. */
  if (!strcasecmp (iri->uri_encoding, "UTF-8"))
    {
      const char *p = str;
      for (p = str; *p; p++)
        if (*p < 0)
          {
            *new = strdup (str);
            return true;
          }
      return false;
    }

  if (do_conversion ("UTF-8", iri->uri_encoding, (char *) str, strlen (str), (char **) new))
    ret = true;

  /* Test if something was converted */
  if (!strcmp (str, *new))
    {
      xfree ((char *) *new);
      return false;
    }

  return ret;
}

/* Allocate a new iri structure and return a pointer to it. */
struct iri *
iri_new (void)
{
  struct iri *i = xmalloc (sizeof *i);
  i->uri_encoding = opt.encoding_remote ? xstrdup (opt.encoding_remote) : NULL;
  i->content_encoding = NULL;
  i->orig_url = NULL;
  i->utf8_encode = opt.enable_iri;
  return i;
}

struct iri *iri_dup (const struct iri *src)
{
  struct iri *i = xmalloc (sizeof *i);
  i->uri_encoding = src->uri_encoding ? xstrdup (src->uri_encoding) : NULL;
  i->content_encoding = (src->content_encoding ?
                         xstrdup (src->content_encoding) : NULL);
  i->orig_url = src->orig_url ? xstrdup (src->orig_url) : NULL;
  i->utf8_encode = src->utf8_encode;
  return i;
}

/* Completely free an iri structure. */
void
iri_free (struct iri *i)
{
  xfree_null (i->uri_encoding);
  xfree_null (i->content_encoding);
  xfree_null (i->orig_url);
  xfree (i);
}

/* Set uri_encoding of struct iri i. If a remote encoding was specified, use
   it unless force is true. */
void
set_uri_encoding (struct iri *i, char *charset, bool force)
{
  DEBUGP (("URI encoding = %s\n", charset ? quote (charset) : "None"));
  if (!force && opt.encoding_remote)
    return;
  if (i->uri_encoding)
    {
      if (charset && !strcasecmp (i->uri_encoding, charset))
        return;
      xfree (i->uri_encoding);
    }

  i->uri_encoding = charset ? xstrdup (charset) : NULL;
}

/* Set content_encoding of struct iri i. */
void
set_content_encoding (struct iri *i, char *charset)
{
  DEBUGP (("URI content encoding = %s\n", charset ? quote (charset) : "None"));
  if (opt.encoding_remote)
    return;
  if (i->content_encoding)
    {
      if (charset && !strcasecmp (i->content_encoding, charset))
        return;
      xfree (i->content_encoding);
    }

  i->content_encoding = charset ? xstrdup (charset) : NULL;
}

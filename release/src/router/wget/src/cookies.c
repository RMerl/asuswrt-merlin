/* Support for cookies.
   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
   2010, 2011 Free Software Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

GNU Wget is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

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

/* Written by Hrvoje Niksic.  Parts are loosely inspired by the
   cookie patch submitted by Tomasz Wegrzanowski.

   This implements the client-side cookie support, as specified
   (loosely) by Netscape's "preliminary specification", currently
   available at:

       http://wp.netscape.com/newsref/std/cookie_spec.html

   rfc2109 is not supported because of its incompatibilities with the
   above widely-used specification.  rfc2965 is entirely ignored,
   since popular client software doesn't implement it, and even the
   sites that do send Set-Cookie2 also emit Set-Cookie for
   compatibility.  */

#include "wget.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#ifdef HAVE_LIBPSL
# include <libpsl.h>
#endif
#include "utils.h"
#include "hash.h"
#include "cookies.h"
#include "http.h"               /* for http_atotm */

/* Declarations of `struct cookie' and the most basic functions. */

/* Cookie jar serves as cookie storage and a means of retrieving
   cookies efficiently.  All cookies with the same domain are stored
   in a linked list called "chain".  A cookie chain can be reached by
   looking up the domain in the cookie jar's chains_by_domain table.

   For example, to reach all the cookies under google.com, one must
   execute hash_table_get(jar->chains_by_domain, "google.com").  Of
   course, when sending a cookie to `www.google.com', one must search
   for cookies that belong to either `www.google.com' or `google.com'
   -- but the point is that the code doesn't need to go through *all*
   the cookies.  */

struct cookie_jar {
  /* Cookie chains indexed by domain.  */
  struct hash_table *chains;

  int cookie_count;             /* number of cookies in the jar. */
};

/* Value set by entry point functions, so that the low-level
   routines don't need to call time() all the time.  */
static time_t cookies_now;

struct cookie_jar *
cookie_jar_new (void)
{
  struct cookie_jar *jar = xnew (struct cookie_jar);
  jar->chains = make_nocase_string_hash_table (0);
  jar->cookie_count = 0;
  return jar;
}

struct cookie {
  char *domain;                 /* domain of the cookie */
  int port;                     /* port number */
  char *path;                   /* path prefix of the cookie */

  unsigned discard_requested :1;/* whether cookie was created to
                                   request discarding another
                                   cookie. */

  unsigned secure :1;           /* whether cookie should be
                                   transmitted over non-https
                                   connections. */
  unsigned domain_exact :1;     /* whether DOMAIN must match as a
                                   whole. */

  unsigned permanent :1;        /* whether the cookie should outlive
                                   the session. */
  time_t expiry_time;           /* time when the cookie expires, 0
                                   means undetermined. */

  char *attr;                   /* cookie attribute name */
  char *value;                  /* cookie attribute value */

  struct cookie *next;          /* used for chaining of cookies in the
                                   same domain. */
};

#define PORT_ANY (-1)

/* Allocate and return a new, empty cookie structure. */

static struct cookie *
cookie_new (void)
{
  struct cookie *cookie = xnew0 (struct cookie);

  /* Both cookie->permanent and cookie->expiry_time are now 0.  This
     means that the cookie doesn't expire, but is only valid for this
     session (i.e. not written out to disk).  */

  cookie->port = PORT_ANY;
  return cookie;
}

/* Non-zero if the cookie has expired.  Assumes cookies_now has been
   set by one of the entry point functions.  */

static bool
cookie_expired_p (const struct cookie *c)
{
  return c->expiry_time != 0 && c->expiry_time < cookies_now;
}

/* Deallocate COOKIE and its components. */

static void
delete_cookie (struct cookie *cookie)
{
  xfree_null (cookie->domain);
  xfree_null (cookie->path);
  xfree_null (cookie->attr);
  xfree_null (cookie->value);
  xfree (cookie);
}

/* Functions for storing cookies.

   All cookies can be reached beginning with jar->chains.  The key in
   that table is the domain name, and the value is a linked list of
   all cookies from that domain.  Every new cookie is placed on the
   head of the list.  */

/* Find and return a cookie in JAR whose domain, path, and attribute
   name correspond to COOKIE.  If found, PREVPTR will point to the
   location of the cookie previous in chain, or NULL if the found
   cookie is the head of a chain.

   If no matching cookie is found, return NULL. */

static struct cookie *
find_matching_cookie (struct cookie_jar *jar, struct cookie *cookie,
                      struct cookie **prevptr)
{
  struct cookie *chain, *prev;

  chain = hash_table_get (jar->chains, cookie->domain);
  if (!chain)
    goto nomatch;

  prev = NULL;
  for (; chain; prev = chain, chain = chain->next)
    if (0 == strcmp (cookie->path, chain->path)
        && 0 == strcmp (cookie->attr, chain->attr)
        && cookie->port == chain->port)
      {
        *prevptr = prev;
        return chain;
      }

 nomatch:
  *prevptr = NULL;
  return NULL;
}

/* Store COOKIE to the jar.

   This is done by placing COOKIE at the head of its chain.  However,
   if COOKIE matches a cookie already in memory, as determined by
   find_matching_cookie, the old cookie is unlinked and destroyed.

   The key of each chain's hash table entry is allocated only the
   first time; next hash_table_put's reuse the same key.  */

static void
store_cookie (struct cookie_jar *jar, struct cookie *cookie)
{
  struct cookie *chain_head;
  char *chain_key;

  if (hash_table_get_pair (jar->chains, cookie->domain,
                           &chain_key, &chain_head))
    {
      /* A chain of cookies in this domain already exists.  Check for
         duplicates -- if an extant cookie exactly matches our domain,
         port, path, and name, replace it.  */
      struct cookie *prev;
      struct cookie *victim = find_matching_cookie (jar, cookie, &prev);

      if (victim)
        {
          /* Remove VICTIM from the chain.  COOKIE will be placed at
             the head. */
          if (prev)
            {
              prev->next = victim->next;
              cookie->next = chain_head;
            }
          else
            {
              /* prev is NULL; apparently VICTIM was at the head of
                 the chain.  This place will be taken by COOKIE, so
                 all we need to do is:  */
              cookie->next = victim->next;
            }
          delete_cookie (victim);
          --jar->cookie_count;
          DEBUGP (("Deleted old cookie (to be replaced.)\n"));
        }
      else
        cookie->next = chain_head;
    }
  else
    {
      /* We are now creating the chain.  Use a copy of cookie->domain
         as the key for the life-time of the chain.  Using
         cookie->domain would be unsafe because the life-time of the
         chain may exceed the life-time of the cookie.  (Cookies may
         be deleted from the chain by this very function.)  */
      cookie->next = NULL;
      chain_key = xstrdup (cookie->domain);
    }

  hash_table_put (jar->chains, chain_key, cookie);
  ++jar->cookie_count;

  IF_DEBUG
    {
      time_t exptime = cookie->expiry_time;
      DEBUGP (("\nStored cookie %s %d%s %s <%s> <%s> [expiry %s] %s %s\n",
               cookie->domain, cookie->port,
               cookie->port == PORT_ANY ? " (ANY)" : "",
               cookie->path,
               cookie->permanent ? "permanent" : "session",
               cookie->secure ? "secure" : "insecure",
               cookie->expiry_time ? datetime_str (exptime) : "none",
               cookie->attr, cookie->value));
    }
}

/* Discard a cookie matching COOKIE's domain, port, path, and
   attribute name.  This gets called when we encounter a cookie whose
   expiry date is in the past, or whose max-age is set to 0.  The
   former corresponds to netscape cookie spec, while the latter is
   specified by rfc2109.  */

static void
discard_matching_cookie (struct cookie_jar *jar, struct cookie *cookie)
{
  struct cookie *prev, *victim;

  if (!hash_table_count (jar->chains))
    /* No elements == nothing to discard. */
    return;

  victim = find_matching_cookie (jar, cookie, &prev);
  if (victim)
    {
      if (prev)
        /* Simply unchain the victim. */
        prev->next = victim->next;
      else
        {
          /* VICTIM was head of its chain.  We need to place a new
             cookie at the head.  */
          char *chain_key = NULL;
          int res;

          res = hash_table_get_pair (jar->chains, victim->domain,
                                     &chain_key, NULL);
          assert (res != 0);
          if (!victim->next)
            {
              /* VICTIM was the only cookie in the chain.  Destroy the
                 chain and deallocate the chain key.  */
              hash_table_remove (jar->chains, victim->domain);
              xfree (chain_key);
            }
          else
            hash_table_put (jar->chains, chain_key, victim->next);
        }
      delete_cookie (victim);
      DEBUGP (("Discarded old cookie.\n"));
    }
}

/* Functions for parsing the `Set-Cookie' header, and creating new
   cookies from the wire.  */

#define TOKEN_IS(token, string_literal)                         \
  BOUNDED_EQUAL_NO_CASE (token.b, token.e, string_literal)

#define TOKEN_NON_EMPTY(token) (token.b != NULL && token.b != token.e)

/* Parse the contents of the `Set-Cookie' header.  The header looks
   like this:

   name1=value1; name2=value2; ...

   Trailing semicolon is optional; spaces are allowed between all
   tokens.  Additionally, values may be quoted.

   A new cookie is returned upon success, NULL otherwise.

   The first name-value pair will be used to set the cookie's
   attribute name and value.  Subsequent parameters will be checked
   against field names such as `domain', `path', etc.  Recognized
   fields will be parsed and the corresponding members of COOKIE
   filled.  */

static struct cookie *
parse_set_cookie (const char *set_cookie, bool silent)
{
  const char *ptr = set_cookie;
  struct cookie *cookie = cookie_new ();
  param_token name, value;

  if (!extract_param (&ptr, &name, &value, ';', NULL))
    goto error;
  if (!value.b)
    goto error;

  /* If the value is quoted, do not modify it.  */
  if (*(value.b - 1) == '"')
    value.b--;
  if (*value.e == '"')
    value.e++;

  cookie->attr = strdupdelim (name.b, name.e);
  cookie->value = strdupdelim (value.b, value.e);

  while (extract_param (&ptr, &name, &value, ';', NULL))
    {
      if (TOKEN_IS (name, "domain"))
        {
          if (!TOKEN_NON_EMPTY (value))
            goto error;
          xfree_null (cookie->domain);
          /* Strictly speaking, we should set cookie->domain_exact if the
             domain doesn't begin with a dot.  But many sites set the
             domain to "foo.com" and expect "subhost.foo.com" to get the
             cookie, and it apparently works in browsers.  */
          if (*value.b == '.')
            ++value.b;
          cookie->domain = strdupdelim (value.b, value.e);
        }
      else if (TOKEN_IS (name, "path"))
        {
          if (!TOKEN_NON_EMPTY (value))
            goto error;
          xfree_null (cookie->path);
          cookie->path = strdupdelim (value.b, value.e);
        }
      else if (TOKEN_IS (name, "expires"))
        {
          char *value_copy;
          time_t expires;

          if (!TOKEN_NON_EMPTY (value))
            goto error;
          BOUNDED_TO_ALLOCA (value.b, value.e, value_copy);

          /* Check if expiration spec is valid.
             If not, assume default (cookie doesn't expire, but valid only for
             this session.) */
          expires = http_atotm (value_copy);
          if (expires != (time_t) -1)
            {
              cookie->permanent = 1;
              cookie->expiry_time = expires;
              /* According to netscape's specification, expiry time in
                 the past means that discarding of a matching cookie
                 is requested.  */
              if (cookie->expiry_time < cookies_now)
                cookie->discard_requested = 1;
            }
        }
      else if (TOKEN_IS (name, "max-age"))
        {
          double maxage = -1;
          char *value_copy;

          if (!TOKEN_NON_EMPTY (value))
            goto error;
          BOUNDED_TO_ALLOCA (value.b, value.e, value_copy);

          sscanf (value_copy, "%lf", &maxage);
          if (maxage == -1)
            /* something went wrong. */
            goto error;
          cookie->permanent = 1;
          cookie->expiry_time = cookies_now + maxage;

          /* According to rfc2109, a cookie with max-age of 0 means that
             discarding of a matching cookie is requested.  */
          if (maxage == 0)
            cookie->discard_requested = 1;
        }
      else if (TOKEN_IS (name, "secure"))
        {
          /* ignore value completely */
          cookie->secure = 1;
        }
      /* else: Ignore unrecognized attribute. */
    }
  if (*ptr)
    /* extract_param has encountered a syntax error */
    goto error;

  /* The cookie has been successfully constructed; return it. */
  return cookie;

 error:
  if (!silent)
    logprintf (LOG_NOTQUIET,
               _("Syntax error in Set-Cookie: %s at position %d.\n"),
               quotearg_style (escape_quoting_style, set_cookie),
               (int) (ptr - set_cookie));
  delete_cookie (cookie);
  return NULL;
}

#undef TOKEN_IS
#undef TOKEN_NON_EMPTY

/* Sanity checks.  These are important, otherwise it is possible for
   mailcious attackers to destroy important cookie information and/or
   violate your privacy.  */


#define REQUIRE_DIGITS(p) do {                  \
  if (!c_isdigit (*p))                          \
    return false;                               \
  for (++p; c_isdigit (*p); p++)                \
    ;                                           \
} while (0)

#define REQUIRE_DOT(p) do {                     \
  if (*p++ != '.')                              \
    return false;                               \
} while (0)

/* Check whether ADDR matches <digits>.<digits>.<digits>.<digits>.

   We don't want to call network functions like inet_addr() because
   all we need is a check, preferrably one that is small, fast, and
   well-defined.  */

static bool
numeric_address_p (const char *addr)
{
  const char *p = addr;

  REQUIRE_DIGITS (p);           /* A */
  REQUIRE_DOT (p);              /* . */
  REQUIRE_DIGITS (p);           /* B */
  REQUIRE_DOT (p);              /* . */
  REQUIRE_DIGITS (p);           /* C */
  REQUIRE_DOT (p);              /* . */
  REQUIRE_DIGITS (p);           /* D */

  if (*p != '\0')
    return false;
  return true;
}

/* Check whether COOKIE_DOMAIN is an appropriate domain for HOST.
   Originally I tried to make the check compliant with rfc2109, but
   the sites deviated too often, so I had to fall back to "tail
   matching", as defined by the original Netscape's cookie spec.

   Wget now uses libpsl to check domain names against a public suffix
   list to see if they are valid. However, since we don't provide a
   psl on our own, if libpsl is compiled without a public suffix list,
   fall back to using the original "tail matching" heuristic. Also if
   libpsl is unable to convert the domain to lowercase, which means that
   it doesnt have any runtime conversion support, we again fall back to
   "tail matching" since libpsl states the results are unpredictable with
   upper case strings.
   */

static bool
check_domain_match (const char *cookie_domain, const char *host)
{

#ifdef HAVE_LIBPSL
  DEBUGP (("cdm: 1"));
  char *cookie_domain_lower = NULL;
  char *host_lower = NULL;
  const psl_ctx_t *psl;
  int is_acceptable;

  if (!(psl = psl_builtin()))
    {
      DEBUGP (("\nlibpsl not built with a public suffix list. "
               "Falling back to simple heuristics.\n"));
      goto no_psl;
    }

  if (psl_str_to_utf8lower (cookie_domain, NULL, NULL, &cookie_domain_lower) == PSL_SUCCESS &&
      psl_str_to_utf8lower (host, NULL, NULL, &host_lower) == PSL_SUCCESS)
    {
      is_acceptable = psl_is_cookie_domain_acceptable (psl, host_lower, cookie_domain_lower);
    }
  else
    {
        DEBUGP (("libpsl unable to parse domain name. "
                 "Falling back to simple heuristics.\n"));
        goto no_psl;
    }

  xfree (cookie_domain_lower);
  xfree (host_lower);

  return is_acceptable == 1;

no_psl:
  /* Cleanup the PSL pointers first */
  xfree (cookie_domain_lower);
  xfree (host_lower);
#endif

  /* For efficiency make some elementary checks first */
  DEBUGP (("cdm: 2"));

  /* For the sake of efficiency, check for exact match first. */
  if (0 == strcasecmp (cookie_domain, host))
    return true;

  DEBUGP ((" 3"));

  /* HOST must match the tail of cookie_domain. */
  if (!match_tail (host, cookie_domain, true))
    return false;

  /* We know that COOKIE_DOMAIN is a subset of HOST; however, we must
     make sure that somebody is not trying to set the cookie for a
     subdomain shared by many entities.  For example, "company.co.uk"
     must not be allowed to set a cookie for ".co.uk".  On the other
     hand, "sso.redhat.de" should be able to set a cookie for
     ".redhat.de".

     The only marginally sane way to handle this I can think of is to
     reject on the basis of the length of the second-level domain name
     (but when the top-level domain is unknown), with the assumption
     that those of three or less characters could be reserved.  For
     example:

          .co.org -> works because the TLD is known
           .co.uk -> doesn't work because "co" is only two chars long
          .com.au -> doesn't work because "com" is only 3 chars long
          .cnn.uk -> doesn't work because "cnn" is also only 3 chars long (ugh)
          .cnn.de -> doesn't work for the same reason (ugh!!)
         .abcd.de -> works because "abcd" is 4 chars long
      .img.cnn.de -> works because it's not trying to set the 2nd level domain
       .cnn.co.uk -> works for the same reason

    That should prevent misuse, while allowing reasonable usage.  If
    someone knows of a better way to handle this, please let me
    know.  */
  {
    const char *p = cookie_domain;
    int dccount = 1;            /* number of domain components */
    int ldcl  = 0;              /* last domain component length */
    int nldcl = 0;              /* next to last domain component length */
    int out;
    if (*p == '.')
      /* Ignore leading period in this calculation. */
      ++p;
    DEBUGP ((" 4"));
    for (out = 0; !out; p++)
      switch (*p)
        {
        case '\0':
          out = 1;
          break;
        case '.':
          if (ldcl == 0)
            /* Empty domain component found -- the domain is invalid. */
            return false;
          if (*(p + 1) == '\0')
            {
              /* Tolerate trailing '.' by not treating the domain as
                 one ending with an empty domain component.  */
              out = 1;
              break;
            }
          nldcl = ldcl;
          ldcl  = 0;
          ++dccount;
          break;
        default:
          ++ldcl;
        }

    DEBUGP ((" 5"));

    if (dccount < 2)
      return false;

    DEBUGP ((" 6"));

    if (dccount == 2)
      {
        size_t i;
        int known_toplevel = false;
        static const char *known_toplevel_domains[] = {
          ".com", ".edu", ".net", ".org", ".gov", ".mil", ".int"
        };
        for (i = 0; i < countof (known_toplevel_domains); i++)
          if (match_tail (cookie_domain, known_toplevel_domains[i], true))
            {
              known_toplevel = true;
              break;
            }
        if (!known_toplevel && nldcl <= 3)
          return false;
      }
  }

  DEBUGP ((" 7"));

  /* Don't allow the host "foobar.com" to set a cookie for domain
     "bar.com".  */
  if (*cookie_domain != '.')
    {
      int dlen = strlen (cookie_domain);
      int hlen = strlen (host);
      /* cookie host:    hostname.foobar.com */
      /* desired domain:             bar.com */
      /* '.' must be here in host-> ^        */
      if (hlen > dlen && host[hlen - dlen - 1] != '.')
        return false;
    }

  DEBUGP ((" 8"));

  return true;
}

static int path_matches (const char *, const char *);

/* Check whether PATH begins with COOKIE_PATH. */

static bool
check_path_match (const char *cookie_path, const char *path)
{
  return path_matches (path, cookie_path) != 0;
}

/* Prepend '/' to string S.  S is copied to fresh stack-allocated
   space and its value is modified to point to the new location.  */

#define PREPEND_SLASH(s) do {                                   \
  char *PS_newstr = (char *) alloca (1 + strlen (s) + 1);       \
  *PS_newstr = '/';                                             \
  strcpy (PS_newstr + 1, s);                                    \
  s = PS_newstr;                                                \
} while (0)


/* Process the HTTP `Set-Cookie' header.  This results in storing the
   cookie or discarding a matching one, or ignoring it completely, all
   depending on the contents.  */

void
cookie_handle_set_cookie (struct cookie_jar *jar,
                          const char *host, int port,
                          const char *path, const char *set_cookie)
{
  struct cookie *cookie;
  cookies_now = time (NULL);

  /* Wget's paths don't begin with '/' (blame rfc1808), but cookie
     usage assumes /-prefixed paths.  Until the rest of Wget is fixed,
     simply prepend slash to PATH.  */
  PREPEND_SLASH (path);

  cookie = parse_set_cookie (set_cookie, false);
  if (!cookie)
    goto out;

  /* Sanitize parts of cookie. */

  if (!cookie->domain)
    {
      cookie->domain = xstrdup (host);
      cookie->domain_exact = 1;
      /* Set the port, but only if it's non-default. */
      if (port != 80 && port != 443)
        cookie->port = port;
    }
  else
    {
      if (!check_domain_match (cookie->domain, host))
        {
          logprintf (LOG_NOTQUIET,
                     _("Cookie coming from %s attempted to set domain to "),
                     quotearg_style (escape_quoting_style, host));
          logprintf (LOG_NOTQUIET,
                     _("%s\n"),
                     quotearg_style (escape_quoting_style, cookie->domain));
          cookie->discard_requested = true;
        }
    }

  if (!cookie->path)
    {
      /* The cookie doesn't set path: set it to the URL path, sans the
         file part ("/dir/file" truncated to "/dir/").  */
      char *trailing_slash = strrchr (path, '/');
      if (trailing_slash)
        cookie->path = strdupdelim (path, trailing_slash + 1);
      else
        /* no slash in the string -- can this even happen? */
        cookie->path = xstrdup (path);
    }
  else
    {
      /* The cookie sets its own path; verify that it is legal. */
      if (!check_path_match (cookie->path, path))
        {
          DEBUGP (("Attempt to fake the path: %s, %s\n",
                   cookie->path, path));
          goto out;
        }
    }

  /* Now store the cookie, or discard an existing cookie, if
     discarding was requested.  */

  if (cookie->discard_requested)
    {
      discard_matching_cookie (jar, cookie);
      goto out;
    }

  store_cookie (jar, cookie);
  return;

 out:
  if (cookie)
    delete_cookie (cookie);
}

/* Support for sending out cookies in HTTP requests, based on
   previously stored cookies.  Entry point is
   `build_cookies_request'.  */

/* Return a count of how many times CHR occurs in STRING. */

static int
count_char (const char *string, char chr)
{
  const char *p;
  int count = 0;
  for (p = string; *p; p++)
    if (*p == chr)
      ++count;
  return count;
}

/* Find the cookie chains whose domains match HOST and store them to
   DEST.

   A cookie chain is the head of a list of cookies that belong to a
   host/domain.  Given HOST "img.search.xemacs.org", this function
   will return the chains for "img.search.xemacs.org",
   "search.xemacs.org", and "xemacs.org" -- those of them that exist
   (if any), that is.

   DEST should be large enough to accept (in the worst case) as many
   elements as there are domain components of HOST.  */

static int
find_chains_of_host (struct cookie_jar *jar, const char *host,
                     struct cookie *dest[])
{
  int dest_count = 0;
  int passes, passcnt;

  /* Bail out quickly if there are no cookies in the jar.  */
  if (!hash_table_count (jar->chains))
    return 0;

  if (numeric_address_p (host))
    /* If host is an IP address, only check for the exact match. */
    passes = 1;
  else
    /* Otherwise, check all the subdomains except the top-level (last)
       one.  As a domain with N components has N-1 dots, the number of
       passes equals the number of dots.  */
    passes = count_char (host, '.');

  passcnt = 0;

  /* Find chains that match HOST, starting with exact match and
     progressing to less specific domains.  For instance, given HOST
     fly.srk.fer.hr, first look for fly.srk.fer.hr's chain, then
     srk.fer.hr's, then fer.hr's.  */
  while (1)
    {
      struct cookie *chain = hash_table_get (jar->chains, host);
      if (chain)
        dest[dest_count++] = chain;
      if (++passcnt >= passes)
        break;
      host = strchr (host, '.') + 1;
    }

  return dest_count;
}

/* If FULL_PATH begins with PREFIX, return the length of PREFIX, zero
   otherwise.  */

static int
path_matches (const char *full_path, const char *prefix)
{
  int len = strlen (prefix);

  if (0 != strncmp (full_path, prefix, len))
    /* FULL_PATH doesn't begin with PREFIX. */
    return 0;

  /* Length of PREFIX determines the quality of the match. */
  return len + 1;
}

/* Return true iff COOKIE matches the provided parameters of the URL
   being downloaded: HOST, PORT, PATH, and SECFLAG.

   If PATH_GOODNESS is non-NULL, store the "path goodness" value
   there.  That value is a measure of how closely COOKIE matches PATH,
   used for ordering cookies.  */

static bool
cookie_matches_url (const struct cookie *cookie,
                    const char *host, int port, const char *path,
                    bool secflag, int *path_goodness)
{
  int pg;

  if (cookie_expired_p (cookie))
    /* Ignore stale cookies.  Don't bother unchaining the cookie at
       this point -- Wget is a relatively short-lived application, and
       stale cookies will not be saved by `save_cookies'.  On the
       other hand, this function should be as efficient as
       possible.  */
    return false;

  if (cookie->secure && !secflag)
    /* Don't transmit secure cookies over insecure connections.  */
    return false;
  if (cookie->port != PORT_ANY && cookie->port != port)
    return false;

  /* If exact domain match is required, verify that cookie's domain is
     equal to HOST.  If not, assume success on the grounds of the
     cookie's chain having been found by find_chains_of_host.  */
  if (cookie->domain_exact
      && 0 != strcasecmp (host, cookie->domain))
    return false;

  pg = path_matches (path, cookie->path);
  if (pg == 0)
    return false;

  if (path_goodness)
    /* If the caller requested path_goodness, we return it.  This is
       an optimization, so that the caller doesn't need to call
       path_matches() again.  */
    *path_goodness = pg;
  return true;
}

/* A structure that points to a cookie, along with the additional
   information about the cookie's "goodness".  This allows us to sort
   the cookies when returning them to the server, as required by the
   spec.  */

struct weighed_cookie {
  struct cookie *cookie;
  int domain_goodness;
  int path_goodness;
};

/* Comparator used for uniquifying the list. */

static int
equality_comparator (const void *p1, const void *p2)
{
  struct weighed_cookie *wc1 = (struct weighed_cookie *)p1;
  struct weighed_cookie *wc2 = (struct weighed_cookie *)p2;

  int namecmp  = strcmp (wc1->cookie->attr, wc2->cookie->attr);
  int valuecmp = strcmp (wc1->cookie->value, wc2->cookie->value);

  /* We only really care whether both name and value are equal.  We
     return them in this order only for consistency...  */
  return namecmp ? namecmp : valuecmp;
}

/* Eliminate duplicate cookies.  "Duplicate cookies" are any two
   cookies with the same attr name and value.  Whenever a duplicate
   pair is found, one of the cookies is removed.  */

static int
eliminate_dups (struct weighed_cookie *outgoing, int count)
{
  struct weighed_cookie *h;     /* hare */
  struct weighed_cookie *t;     /* tortoise */
  struct weighed_cookie *end = outgoing + count;

  /* We deploy a simple uniquify algorithm: first sort the array
     according to our sort criteria, then copy it to itself, comparing
     each cookie to its neighbor and ignoring the duplicates.  */

  qsort (outgoing, count, sizeof (struct weighed_cookie), equality_comparator);

  /* "Hare" runs through all the entries in the array, followed by
     "tortoise".  If a duplicate is found, the hare skips it.
     Non-duplicate entries are copied to the tortoise ptr.  */

  for (h = t = outgoing; h < end; h++)
    {
      if (h != end - 1)
        {
          struct cookie *c0 = h[0].cookie;
          struct cookie *c1 = h[1].cookie;
          if (!strcmp (c0->attr, c1->attr) && !strcmp (c0->value, c1->value))
            continue;           /* ignore the duplicate */
        }

      /* If the hare has advanced past the tortoise (because of
         previous dups), make sure the values get copied.  Otherwise,
         no copying is necessary.  */
      if (h != t)
        *t++ = *h;
      else
        t++;
    }
  return t - outgoing;
}

/* Comparator used for sorting by quality. */

static int
goodness_comparator (const void *p1, const void *p2)
{
  struct weighed_cookie *wc1 = (struct weighed_cookie *)p1;
  struct weighed_cookie *wc2 = (struct weighed_cookie *)p2;

  /* Subtractions take `wc2' as the first argument becauase we want a
     sort in *decreasing* order of goodness.  */
  int dgdiff = wc2->domain_goodness - wc1->domain_goodness;
  int pgdiff = wc2->path_goodness - wc1->path_goodness;

  /* Sort by domain goodness; if these are the same, sort by path
     goodness.  (The sorting order isn't really specified; maybe it
     should be the other way around.)  */
  return dgdiff ? dgdiff : pgdiff;
}

/* Generate a `Cookie' header for a request that goes to HOST:PORT and
   requests PATH from the server.  The resulting string is allocated
   with `malloc', and the caller is responsible for freeing it.  If no
   cookies pertain to this request, i.e. no cookie header should be
   generated, NULL is returned.  */

char *
cookie_header (struct cookie_jar *jar, const char *host,
               int port, const char *path, bool secflag)
{
  struct cookie **chains;
  int chain_count;

  struct cookie *cookie;
  struct weighed_cookie *outgoing;
  int count, i, ocnt;
  char *result;
  int result_size, pos;
  PREPEND_SLASH (path);         /* see cookie_handle_set_cookie */

  /* First, find the cookie chains whose domains match HOST. */

  /* Allocate room for find_chains_of_host to write to.  The number of
     chains can at most equal the number of subdomains, hence
     1+<number of dots>.  */
  chains = alloca_array (struct cookie *, 1 + count_char (host, '.'));
  chain_count = find_chains_of_host (jar, host, chains);

  /* No cookies for this host. */
  if (!chain_count)
    return NULL;

  cookies_now = time (NULL);

  /* Now extract from the chains those cookies that match our host
     (for domain_exact cookies), port (for cookies with port other
     than PORT_ANY), etc.  See matching_cookie for details.  */

  /* Count the number of matching cookies. */
  count = 0;
  for (i = 0; i < chain_count; i++)
    for (cookie = chains[i]; cookie; cookie = cookie->next)
      if (cookie_matches_url (cookie, host, port, path, secflag, NULL))
        ++count;
  if (!count)
    return NULL;                /* no cookies matched */

  /* Allocate the array. */
  outgoing = alloca_array (struct weighed_cookie, count);

  /* Fill the array with all the matching cookies from the chains that
     match HOST. */
  ocnt = 0;
  for (i = 0; i < chain_count; i++)
    for (cookie = chains[i]; cookie; cookie = cookie->next)
      {
        int pg;
        if (!cookie_matches_url (cookie, host, port, path, secflag, &pg))
          continue;
        outgoing[ocnt].cookie = cookie;
        outgoing[ocnt].domain_goodness = strlen (cookie->domain);
        outgoing[ocnt].path_goodness   = pg;
        ++ocnt;
      }
  assert (ocnt == count);

  /* Eliminate duplicate cookies; that is, those whose name and value
     are the same.  */
  count = eliminate_dups (outgoing, count);

  /* Sort the array so that best-matching domains come first, and
     that, within one domain, best-matching paths come first. */
  qsort (outgoing, count, sizeof (struct weighed_cookie), goodness_comparator);

  /* Count the space the name=value pairs will take. */
  result_size = 0;
  for (i = 0; i < count; i++)
    {
      struct cookie *c = outgoing[i].cookie;
      /* name=value */
      result_size += strlen (c->attr) + 1 + strlen (c->value);
    }

  /* Allocate output buffer:
     name=value pairs -- result_size
     "; " separators  -- (count - 1) * 2
     \0 terminator    -- 1 */
  result_size = result_size + (count - 1) * 2 + 1;
  result = xmalloc (result_size);
  pos = 0;
  for (i = 0; i < count; i++)
    {
      struct cookie *c = outgoing[i].cookie;
      int namlen = strlen (c->attr);
      int vallen = strlen (c->value);

      memcpy (result + pos, c->attr, namlen);
      pos += namlen;
      result[pos++] = '=';
      memcpy (result + pos, c->value, vallen);
      pos += vallen;
      if (i < count - 1)
        {
          result[pos++] = ';';
          result[pos++] = ' ';
        }
    }
  result[pos++] = '\0';
  assert (pos == result_size);
  return result;
}

/* Support for loading and saving cookies.  The format used for
   loading and saving should be the format of the `cookies.txt' file
   used by Netscape and Mozilla, at least the Unix versions.
   (Apparently IE can export cookies in that format as well.)  The
   format goes like this:

       DOMAIN DOMAIN-FLAG PATH SECURE-FLAG TIMESTAMP ATTR-NAME ATTR-VALUE

     DOMAIN      -- cookie domain, optionally followed by :PORT
     DOMAIN-FLAG -- whether all hosts in the domain match
     PATH        -- cookie path
     SECURE-FLAG -- whether cookie requires secure connection
     TIMESTAMP   -- expiry timestamp, number of seconds since epoch
     ATTR-NAME   -- name of the cookie attribute
     ATTR-VALUE  -- value of the cookie attribute (empty if absent)

   The fields are separated by TABs.  All fields are mandatory, except
   for ATTR-VALUE.  The `-FLAG' fields are boolean, their legal values
   being "TRUE" and "FALSE'.  Empty lines, lines consisting of
   whitespace only, and comment lines (beginning with # optionally
   preceded by whitespace) are ignored.

   Example line from cookies.txt (split in two lines for readability):

       .google.com      TRUE    /       FALSE   2147368447      \
       PREF     ID=34bb47565bbcd47b:LD=en:NR=20:TM=985172580:LM=985739012

*/

/* If the region [B, E) ends with :<digits>, parse the number, return
   it, and store new boundary (location of the `:') to DOMAIN_E_PTR.
   If port is not specified, return 0.  */

static int
domain_port (const char *domain_b, const char *domain_e,
             const char **domain_e_ptr)
{
  int port = 0;
  const char *p;
  const char *colon = memchr (domain_b, ':', domain_e - domain_b);
  if (!colon)
    return 0;
  for (p = colon + 1; p < domain_e && c_isdigit (*p); p++)
    port = 10 * port + (*p - '0');
  if (p < domain_e)
    /* Garbage following port number. */
    return 0;
  *domain_e_ptr = colon;
  return port;
}

#define GET_WORD(p, b, e) do {                  \
  b = p;                                        \
  while (*p && *p != '\t')                      \
    ++p;                                        \
  e = p;                                        \
  if (b == e || !*p)                            \
    goto next;                                  \
  ++p;                                          \
} while (0)

/* Load cookies from FILE.  */

void
cookie_jar_load (struct cookie_jar *jar, const char *file)
{
  char *line = NULL;
  size_t bufsize = 0;

  FILE *fp = fopen (file, "r");
  if (!fp)
    {
      logprintf (LOG_NOTQUIET, _("Cannot open cookies file %s: %s\n"),
                 quote (file), strerror (errno));
      return;
    }

  cookies_now = time (NULL);

  while (getline (&line, &bufsize, fp) > 0)
    {
      struct cookie *cookie;
      char *p = line;

      double expiry;
      int port;

      char *domain_b  = NULL, *domain_e  = NULL;
      char *domflag_b = NULL, *domflag_e = NULL;
      char *path_b    = NULL, *path_e    = NULL;
      char *secure_b  = NULL, *secure_e  = NULL;
      char *expires_b = NULL, *expires_e = NULL;
      char *name_b    = NULL, *name_e    = NULL;
      char *value_b   = NULL, *value_e   = NULL;

      /* Skip leading white-space. */
      while (*p && c_isspace (*p))
        ++p;
      /* Ignore empty lines.  */
      if (!*p || *p == '#')
        continue;

      GET_WORD (p, domain_b,  domain_e);
      GET_WORD (p, domflag_b, domflag_e);
      GET_WORD (p, path_b,    path_e);
      GET_WORD (p, secure_b,  secure_e);
      GET_WORD (p, expires_b, expires_e);
      GET_WORD (p, name_b,    name_e);

      /* Don't use GET_WORD for value because it ends with newline,
         not TAB.  */
      value_b = p;
      value_e = p + strlen (p);
      if (value_e > value_b && value_e[-1] == '\n')
        --value_e;
      if (value_e > value_b && value_e[-1] == '\r')
        --value_e;
      /* Empty values are legal (I think), so don't bother checking. */

      cookie = cookie_new ();

      cookie->attr    = strdupdelim (name_b, name_e);
      cookie->value   = strdupdelim (value_b, value_e);
      cookie->path    = strdupdelim (path_b, path_e);
      cookie->secure  = BOUNDED_EQUAL (secure_b, secure_e, "TRUE");

      /* Curl source says, quoting Andre Garcia: "flag: A TRUE/FALSE
         value indicating if all machines within a given domain can
         access the variable.  This value is set automatically by the
         browser, depending on the value set for the domain."  */
      cookie->domain_exact = !BOUNDED_EQUAL (domflag_b, domflag_e, "TRUE");

      /* DOMAIN needs special treatment because we might need to
         extract the port.  */
      port = domain_port (domain_b, domain_e, (const char **)&domain_e);
      if (port)
        cookie->port = port;

      if (*domain_b == '.')
        ++domain_b;             /* remove leading dot internally */
      cookie->domain  = strdupdelim (domain_b, domain_e);

      /* safe default in case EXPIRES field is garbled. */
      expiry = (double)cookies_now - 1;

      /* I don't like changing the line, but it's safe here.  (line is
         malloced.)  */
      *expires_e = '\0';
      sscanf (expires_b, "%lf", &expiry);

      if (expiry == 0)
        {
          /* EXPIRY can be 0 for session cookies saved because the
             user specified `--keep-session-cookies' in the past.
             They remain session cookies, and will be saved only if
             the user has specified `keep-session-cookies' again.  */
        }
      else
        {
          if (expiry < cookies_now)
            goto abort_cookie;  /* ignore stale cookie. */
          cookie->expiry_time = expiry;
          cookie->permanent = 1;
        }

      store_cookie (jar, cookie);

    next:
      continue;

    abort_cookie:
      delete_cookie (cookie);
    }

  xfree(line);
  fclose (fp);
}

/* Save cookies, in format described above, to FILE. */

void
cookie_jar_save (struct cookie_jar *jar, const char *file)
{
  FILE *fp;
  hash_table_iterator iter;

  DEBUGP (("Saving cookies to %s.\n", file));

  cookies_now = time (NULL);

  fp = fopen (file, "w");
  if (!fp)
    {
      logprintf (LOG_NOTQUIET, _("Cannot open cookies file %s: %s\n"),
                 quote (file), strerror (errno));
      return;
    }

  fputs ("# HTTP cookie file.\n", fp);
  fprintf (fp, "# Generated by Wget on %s.\n", datetime_str (cookies_now));
  fputs ("# Edit at your own risk.\n\n", fp);

  for (hash_table_iterate (jar->chains, &iter);
       hash_table_iter_next (&iter);
       )
    {
      const char *domain = iter.key;
      struct cookie *cookie = iter.value;
      for (; cookie; cookie = cookie->next)
        {
          if (!cookie->permanent && !opt.keep_session_cookies)
            continue;
          if (cookie_expired_p (cookie))
            continue;
          if (!cookie->domain_exact)
            fputc ('.', fp);
          fputs (domain, fp);
          if (cookie->port != PORT_ANY)
            fprintf (fp, ":%d", cookie->port);
          fprintf (fp, "\t%s\t%s\t%s\t%.0f\t%s\t%s\n",
                   cookie->domain_exact ? "FALSE" : "TRUE",
                   cookie->path, cookie->secure ? "TRUE" : "FALSE",
                   (double)cookie->expiry_time,
                   cookie->attr, cookie->value);
          if (ferror (fp))
            goto out;
        }
    }
 out:
  if (ferror (fp))
    logprintf (LOG_NOTQUIET, _("Error writing to %s: %s\n"),
               quote (file), strerror (errno));
  if (fclose (fp) < 0)
    logprintf (LOG_NOTQUIET, _("Error closing %s: %s\n"),
               quote (file), strerror (errno));

  DEBUGP (("Done saving cookies.\n"));
}

/* Clean up cookie-related data. */

void
cookie_jar_delete (struct cookie_jar *jar)
{
  /* Iterate over chains (indexed by domain) and free them. */
  hash_table_iterator iter;
  for (hash_table_iterate (jar->chains, &iter); hash_table_iter_next (&iter); )
    {
      struct cookie *chain = iter.value;
      xfree (iter.key);
      /* Then all cookies in this chain. */
      while (chain)
        {
          struct cookie *next = chain->next;
          delete_cookie (chain);
          chain = next;
        }
    }
  hash_table_destroy (jar->chains);
  xfree (jar);
}

/* Test cases.  Currently this is only tests parse_set_cookies.  To
   use, recompile Wget with -DTEST_COOKIES and call test_cookies()
   from main.  */

#ifdef TEST_COOKIES
void
test_cookies (void)
{
  /* Tests expected to succeed: */
  static struct {
    const char *data;
    const char *results[10];
  } tests_succ[] = {
    { "arg=value", {"arg", "value", NULL} },
    { "arg1=value1;arg2=value2", {"arg1", "value1", "arg2", "value2", NULL} },
    { "arg1=value1; arg2=value2", {"arg1", "value1", "arg2", "value2", NULL} },
    { "arg1=value1;  arg2=value2;", {"arg1", "value1", "arg2", "value2", NULL} },
    { "arg1=value1;  arg2=value2;  ", {"arg1", "value1", "arg2", "value2", NULL} },
    { "arg1=\"value1\"; arg2=\"\"", {"arg1", "value1", "arg2", "", NULL} },
    { "arg=", {"arg", "", NULL} },
    { "arg1=; arg2=", {"arg1", "", "arg2", "", NULL} },
    { "arg1 = ; arg2= ", {"arg1", "", "arg2", "", NULL} },
  };

  /* Tests expected to fail: */
  static char *tests_fail[] = {
    ";",
    "arg=\"unterminated",
    "=empty-name",
    "arg1=;=another-empty-name",
  };
  int i;

  for (i = 0; i < countof (tests_succ); i++)
    {
      int ind;
      const char *data = tests_succ[i].data;
      const char **expected = tests_succ[i].results;
      struct cookie *c;

      c = parse_set_cookie (data, true);
      if (!c)
        {
          printf ("NULL cookie returned for valid data: %s\n", data);
          continue;
        }

      /* Test whether extract_param handles these cases correctly. */
      {
        param_token name, value;
        const char *ptr = data;
        int j = 0;
        while (extract_param (&ptr, &name, &value, ';', NULL))
          {
            char *n = strdupdelim (name.b, name.e);
            char *v = strdupdelim (value.b, value.e);
            if (!expected[j])
              {
                printf ("Too many parameters for '%s'\n", data);
                break;
              }
            if (0 != strcmp (expected[j], n))
              printf ("Invalid name %d for '%s' (expected '%s', got '%s')\n",
                      j / 2 + 1, data, expected[j], n);
            if (0 != strcmp (expected[j + 1], v))
              printf ("Invalid value %d for '%s' (expected '%s', got '%s')\n",
                      j / 2 + 1, data, expected[j + 1], v);
            j += 2;
            free (n);
            free (v);
          }
        if (expected[j])
          printf ("Too few parameters for '%s'\n", data);
      }
    }

  for (i = 0; i < countof (tests_fail); i++)
    {
      struct cookie *c;
      char *data = tests_fail[i];
      c = parse_set_cookie (data, true);
      if (c)
        printf ("Failed to report error on invalid data: %s\n", data);
    }
}
#endif /* TEST_COOKIES */

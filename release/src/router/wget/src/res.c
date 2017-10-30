/* Support for Robot Exclusion Standard (RES).
   Copyright (C) 2001, 2006, 2007, 2008, 2009, 2010, 2011, 2015 Free
   Software Foundation, Inc.

This file is part of Wget.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
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

/* This file implements the Robot Exclusion Standard (RES).

   RES is a simple protocol that enables site admins to signalize to
   the web crawlers that certain parts of the site should not be
   accessed.  All the admin needs to do is create a "robots.txt" file
   in the web server root, and use simple commands to allow or
   disallow access to certain parts of the site.

   The first specification was written by Martijn Koster in 1994, and
   is still available at <http://www.robotstxt.org/orig.html>.
   In 1996, Martijn wrote an Internet Draft specifying an improved RES
   specification; however, that work was apparently abandoned since
   the draft has expired in 1997 and hasn't been replaced since.  The
   draft is available at
   <http://www.robotstxt.org/norobots-rfc.txt>.

   This file implements RES as specified by the draft.  Note that this
   only handles the "robots.txt" support.  The META tag that controls
   whether the links should be followed is handled in `html-url.c'.

   Known deviations:

   * The end-of-line comment recognition is more in the spirit of the
     Bourne Shell (as specified by RES-1994).  That means that
     "foo#bar" is taken literally, whereas "foo #bar" is interpreted
     as "foo".  The Draft apparently specifies that both should be
     interpreted as "foo".

   * We don't recognize sole CR as the line ending.

   * We don't implement expiry mechanism for /robots.txt specs.  I
     consider it non-necessary for a relatively short-lived
     application such as Wget.  Besides, it is highly questionable
     whether anyone deploys the recommended expiry scheme for
     robots.txt.

   Entry points are functions res_parse, res_parse_from_file,
   res_match_path, res_register_specs, res_get_specs, and
   res_retrieve_file.  */

#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "utils.h"
#include "hash.h"
#include "url.h"
#include "retr.h"
#include "res.h"
#include "c-strcase.h"

#ifdef TESTING
#include "test.h"
#endif

struct path_info {
  char *path;
  bool allowedp;
  bool user_agent_exact_p;
};

struct robot_specs {
  int count;
  int size;
  struct path_info *paths;
};

/* Parsing the robot spec. */

/* Check whether AGENT (a string of length LENGTH) equals "wget" or
   "*".  If it is either of them, *matches is set to one.  If it is
   "wget", *exact_match is set to one.  */

static void
match_user_agent (const char *agent, int length,
                  bool *matches, bool *exact_match)
{
  if (length == 1 && *agent == '*')
    {
      *matches = true;
      *exact_match = false;
    }
  else if (BOUNDED_EQUAL_NO_CASE (agent, agent + length, "wget"))
    {
      *matches = true;
      *exact_match = true;
    }
  else
    {
      *matches = false;
      *exact_match = false;
    }
}

/* Add a path specification between PATH_B and PATH_E as one of the
   paths in SPECS.  */

static void
add_path (struct robot_specs *specs, const char *path_b, const char *path_e,
          bool allowedp, bool exactp)
{
  struct path_info pp;
  if (path_b < path_e && *path_b == '/')
    /* Our path representation doesn't use a leading slash, so remove
       one from theirs. */
    ++path_b;
  pp.path     = strdupdelim (path_b, path_e);
  pp.allowedp = allowedp;
  pp.user_agent_exact_p = exactp;
  ++specs->count;
  if (specs->count > specs->size)
    {
      if (specs->size == 0)
        specs->size = 1;
      else
        specs->size <<= 1;
      specs->paths = xrealloc (specs->paths,
                               specs->size * sizeof (struct path_info));
    }
  specs->paths[specs->count - 1] = pp;
}

/* Recreate SPECS->paths with only those paths that have
   user_agent_exact_p set to true.  */

static void
prune_non_exact (struct robot_specs *specs)
{
  struct path_info *newpaths;
  int i, j, cnt;
  cnt = 0;
  for (i = 0; i < specs->count; i++)
    if (specs->paths[i].user_agent_exact_p)
      ++cnt;
  newpaths = xnew_array (struct path_info, cnt);
  for (i = 0, j = 0; i < specs->count; i++)
    if (specs->paths[i].user_agent_exact_p)
      newpaths[j++] = specs->paths[i];
  assert (j == cnt);
  xfree (specs->paths);
  specs->paths = newpaths;
  specs->count = cnt;
  specs->size  = cnt;
}

#define EOL(p) ((p) >= lineend)

#define SKIP_SPACE(p) do {              \
  while (!EOL (p) && c_isspace (*p))      \
    ++p;                                \
} while (0)

#define FIELD_IS(string_literal)        \
  BOUNDED_EQUAL_NO_CASE (field_b, field_e, string_literal)

/* Parse textual RES specs beginning with SOURCE of length LENGTH.
   Return a specs objects ready to be fed to res_match_path.

   The parsing itself is trivial, but creating a correct SPECS object
   is trickier than it seems, because RES is surprisingly byzantine if
   you attempt to implement it correctly.

   A "record" is a block of one or more `User-Agent' lines followed by
   one or more `Allow' or `Disallow' lines.  Record is accepted by
   Wget if one of the `User-Agent' lines was "wget", or if the user
   agent line was "*".

   After all the lines have been read, we examine whether an exact
   ("wget") user-agent field was specified.  If so, we delete all the
   lines read under "User-Agent: *" blocks because we have our own
   Wget-specific blocks.  This enables the admin to say:

       User-Agent: *
       Disallow: /

       User-Agent: google
       User-Agent: wget
       Disallow: /cgi-bin

   This means that to Wget and to Google, /cgi-bin is disallowed,
   whereas for all other crawlers, everything is disallowed.
   res_parse is implemented so that the order of records doesn't
   matter.  In the case above, the "User-Agent: *" could have come
   after the other one.  */

struct robot_specs *
res_parse (const char *source, int length)
{
  int line_count = 1;

  const char *p   = source;
  const char *end = source + length;

  /* true if last applicable user-agent field matches Wget. */
  bool user_agent_applies = false;

  /* true if last applicable user-agent field *exactly* matches
     Wget.  */
  bool user_agent_exact = false;

  /* whether we ever encountered exact user agent. */
  bool found_exact = false;

  /* count of allow/disallow lines in the current "record", i.e. after
     the last `user-agent' instructions.  */
  int record_count = 0;

  struct robot_specs *specs = xnew0 (struct robot_specs);

  while (1)
    {
      const char *lineend, *lineend_real;
      const char *field_b, *field_e;
      const char *value_b, *value_e;

      if (p == end)
        break;
      lineend_real = memchr (p, '\n', end - p);
      if (lineend_real)
        ++lineend_real;
      else
        lineend_real = end;
      lineend = lineend_real;

      /* Before doing anything else, check whether the line is empty
         or comment-only. */
      SKIP_SPACE (p);
      if (EOL (p) || *p == '#')
        goto next;

      /* Make sure the end-of-line comments are respected by setting
         lineend to a location preceding the first comment.  Real line
         ending remains in lineend_real.  */
      for (lineend = p; lineend < lineend_real; lineend++)
        if ((lineend == p || c_isspace (*(lineend - 1)))
            && *lineend == '#')
          break;

      /* Ignore trailing whitespace in the same way. */
      while (lineend > p && c_isspace (*(lineend - 1)))
        --lineend;

      assert (!EOL (p));

      field_b = p;
      while (!EOL (p) && (c_isalnum (*p) || *p == '-'))
        ++p;
      field_e = p;

      SKIP_SPACE (p);
      if (field_b == field_e || EOL (p) || *p != ':')
        {
          DEBUGP (("Ignoring malformed line %d\n", line_count));
          goto next;
        }
      ++p;                      /* skip ':' */
      SKIP_SPACE (p);

      value_b = p;
      while (!EOL (p))
        ++p;
      value_e = p;

      /* Finally, we have a syntactically valid line. */
      if (FIELD_IS ("user-agent"))
        {
          /* We have to support several cases:

             --previous records--

             User-Agent: foo
             User-Agent: Wget
             User-Agent: bar
             ... matching record ...

             User-Agent: baz
             User-Agent: qux
             ... non-matching record ...

             User-Agent: *
             ... matching record, but will be pruned later ...

             We have to respect `User-Agent' at the beginning of each
             new record simply because we don't know if we're going to
             encounter "Wget" among the agents or not.  Hence,
             match_user_agent is called when record_count != 0.

             But if record_count is 0, we have to keep calling it
             until it matches, and if that happens, we must not call
             it any more, until the next record.  Hence the other part
             of the condition.  */
          if (record_count != 0 || user_agent_applies == false)
            match_user_agent (value_b, value_e - value_b,
                              &user_agent_applies, &user_agent_exact);
          if (user_agent_exact)
            found_exact = true;
          record_count = 0;
        }
      else if (FIELD_IS ("allow"))
        {
          if (user_agent_applies)
            {
              add_path (specs, value_b, value_e, true, user_agent_exact);
            }
          ++record_count;
        }
      else if (FIELD_IS ("disallow"))
        {
          if (user_agent_applies)
            {
              bool allowed = false;
              if (value_b == value_e)
                /* Empty "disallow" line means everything is *allowed*!  */
                allowed = true;
              add_path (specs, value_b, value_e, allowed, user_agent_exact);
            }
          ++record_count;
        }
      else
        {
          DEBUGP (("Ignoring unknown field at line %d\n", line_count));
          goto next;
        }

    next:
      p = lineend_real;
      ++line_count;
    }

  if (found_exact)
    {
      /* We've encountered an exactly matching user-agent.  Throw out
         all the stuff with user-agent: *.  */
      prune_non_exact (specs);
    }
  else if (specs->size > specs->count)
    {
      /* add_path normally over-allocates specs->paths.  Reallocate it
         to the correct size in order to conserve some memory.  */
      specs->paths = xrealloc (specs->paths,
                               specs->count * sizeof (struct path_info));
      specs->size = specs->count;
    }

  return specs;
}

/* The same like res_parse, but first map the FILENAME into memory,
   and then parse it.  */

struct robot_specs *
res_parse_from_file (const char *filename)
{
  struct robot_specs *specs;
  struct file_memory *fm = wget_read_file (filename);
  if (!fm)
    {
      logprintf (LOG_NOTQUIET, _("Cannot open %s: %s"),
                 filename, strerror (errno));
      return NULL;
    }
  specs = res_parse (fm->content, fm->length);
  wget_read_file_free (fm);
  return specs;
}

static void
free_specs (struct robot_specs *specs)
{
  int i;
  for (i = 0; i < specs->count; i++)
    xfree (specs->paths[i].path);
  xfree (specs->paths);
  xfree (specs);
}

/* Matching of a path according to the specs. */

/* If C is '%' and (ptr[1], ptr[2]) form a hexadecimal number, and if
   that number is not a numerical representation of '/', decode C and
   advance the pointer.  */

#define DECODE_MAYBE(c, ptr) do {                               \
  if (c == '%' && c_isxdigit (ptr[1]) && c_isxdigit (ptr[2]))       \
    {                                                           \
      char decoded = X2DIGITS_TO_NUM (ptr[1], ptr[2]);          \
      if (decoded != '/')                                       \
        {                                                       \
          c = decoded;                                          \
          ptr += 2;                                             \
        }                                                       \
    }                                                           \
} while (0)

/* The inner matching engine: return true if RECORD_PATH matches
   URL_PATH.  The rules for matching are described at
   <http://www.robotstxt.org/norobots-rfc.txt>, section 3.2.2.  */

static bool
matches (const char *record_path, const char *url_path)
{
  const char *rp = record_path;
  const char *up = url_path;

  for (; ; ++rp, ++up)
    {
      char rc = *rp;
      char uc = *up;
      if (!rc)
        return true;
      if (!uc)
        return false;
      DECODE_MAYBE(rc, rp);
      DECODE_MAYBE(uc, up);
      if (rc != uc)
        return false;
    }
}

/* Iterate through all paths in SPECS.  For the first one that
   matches, return its allow/reject status.  If none matches,
   retrieval is by default allowed.  */

bool
res_match_path (const struct robot_specs *specs, const char *path)
{
  int i;
  if (!specs)
    return true;
  for (i = 0; i < specs->count; i++)
    if (matches (specs->paths[i].path, path))
      {
        bool allowedp = specs->paths[i].allowedp;
        DEBUGP (("%s path %s because of rule %s.\n",
                 allowedp ? "Allowing" : "Rejecting",
                 path, quote (specs->paths[i].path)));
        return allowedp;
      }
  return true;
}

/* Registering the specs. */

static struct hash_table *registered_specs;

/* Stolen from cookies.c. */
#define SET_HOSTPORT(host, port, result) do {           \
  int HP_len = strlen (host);                           \
  result = alloca (HP_len + 1 + numdigit (port) + 1);   \
  memcpy (result, host, HP_len);                        \
  result[HP_len] = ':';                                 \
  number_to_string (result + HP_len + 1, port);         \
} while (0)

/* Register RES specs that below to server on HOST:PORT.  They will
   later be retrievable using res_get_specs.  */

void
res_register_specs (const char *host, int port, struct robot_specs *specs)
{
  struct robot_specs *old;
  char *hp, *hp_old;
  SET_HOSTPORT (host, port, hp);

  if (!registered_specs)
    registered_specs = make_nocase_string_hash_table (0);

  if (hash_table_get_pair (registered_specs, hp, &hp_old, &old))
    {
      if (old)
        free_specs (old);
      hash_table_put (registered_specs, hp_old, specs);
    }
  else
    {
      hash_table_put (registered_specs, xstrdup (hp), specs);
    }
}

/* Get the specs that belong to HOST:PORT. */

struct robot_specs *
res_get_specs (const char *host, int port)
{
  char *hp;
  SET_HOSTPORT (host, port, hp);
  if (!registered_specs)
    return NULL;
  return hash_table_get (registered_specs, hp);
}

/* Loading the robots file.  */

#define RES_SPECS_LOCATION "/robots.txt"

/* Retrieve the robots.txt from the server root of the server that
   serves URL.  The file will be named according to the currently
   active rules, and the file name will be returned in *file.

   Return true if robots were retrieved OK, false otherwise.  */

bool
res_retrieve_file (const char *url, char **file, struct iri *iri)
{
  struct iri *i = iri_new ();
  uerr_t err;
  char *robots_url = uri_merge (url, RES_SPECS_LOCATION);
  int saved_ts_val = opt.timestamping;
  int saved_sp_val = opt.spider, url_err;
  struct url * url_parsed;

  /* Copy server URI encoding for a possible IDNA transformation, no need to
     encode the full URI in UTF-8 because "robots.txt" is plain ASCII */
  set_uri_encoding (i, iri->uri_encoding, false);
  i->utf8_encode = false;

  logputs (LOG_VERBOSE, _("Loading robots.txt; please ignore errors.\n"));
  *file = NULL;
  opt.timestamping = false;
  opt.spider       = false;

  url_parsed = url_parse (robots_url, &url_err, i, true);
  if (!url_parsed)
    {
      char *error = url_error (robots_url, url_err);
      logprintf (LOG_NOTQUIET, "%s: %s.\n", robots_url, error);
      xfree (error);
      err = URLERROR;
    }
  else
    {
      err = retrieve_url (url_parsed, robots_url, file, NULL, NULL, NULL,
                          false, i, false);
      url_free(url_parsed);
    }

  opt.timestamping = saved_ts_val;
  opt.spider       = saved_sp_val;
  xfree (robots_url);
  iri_free (i);

  if (err != RETROK && *file != NULL)
    {
      /* If the file is not retrieved correctly, but retrieve_url
         allocated the file name, deallocate is here so that the
         caller doesn't have to worry about it.  */
      xfree (*file);
    }
  return err == RETROK;
}

bool
is_robots_txt_url (const char *url)
{
  char *robots_url = uri_merge (url, RES_SPECS_LOCATION);
  bool ret = are_urls_equal (url, robots_url);

  xfree (robots_url);

  return ret;
}

void
res_cleanup (void)
{
  if (registered_specs)
    {
      hash_table_iterator iter;
      for (hash_table_iterate (registered_specs, &iter);
           hash_table_iter_next (&iter);
           )
        {
          xfree (iter.key);
          free_specs (iter.value);
        }
      hash_table_destroy (registered_specs);
      registered_specs = NULL;
    }
}

#ifdef TESTING

const char *
test_is_robots_txt_url(void)
{
  unsigned i;
  static const struct {
    const char *url;
    bool expected_result;
  } test_array[] = {
    { "http://www.yoyodyne.com/robots.txt", true },
    { "http://www.yoyodyne.com/somepath/", false },
    { "http://www.yoyodyne.com/somepath/robots.txt", false },
  };

  for (i = 0; i < countof(test_array); ++i)
    {
      mu_assert ("test_is_robots_txt_url: wrong result",
                 is_robots_txt_url (test_array[i].url) == test_array[i].expected_result);
    }

  return NULL;
}

#endif /* TESTING */

/*
 * vim: et ts=2 sw=2
 */

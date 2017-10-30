/* Handling of recursive HTTP retrieving.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2015 Free Software
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

#include "wget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "url.h"
#include "recur.h"
#include "utils.h"
#include "retr.h"
#include "ftp.h"
#include "host.h"
#include "hash.h"
#include "res.h"
#include "convert.h"
#include "html-url.h"
#include "css-url.h"
#include "spider.h"
#include "exits.h"

/* Functions for maintaining the URL queue.  */

struct queue_element {
  const char *url;              /* the URL to download */
  const char *referer;          /* the referring document */
  int depth;                    /* the depth */
  bool html_allowed;            /* whether the document is allowed to
                                   be treated as HTML. */
  struct iri *iri;                /* sXXXav */
  bool css_allowed;             /* whether the document is allowed to
                                   be treated as CSS. */
  struct queue_element *next;   /* next element in queue */
};

struct url_queue {
  struct queue_element *head;
  struct queue_element *tail;
  int count, maxcount;
};

/* Create a URL queue. */

static struct url_queue *
url_queue_new (void)
{
  struct url_queue *queue = xnew0 (struct url_queue);
  return queue;
}

/* Delete a URL queue. */

static void
url_queue_delete (struct url_queue *queue)
{
  xfree (queue);
}

/* Enqueue a URL in the queue.  The queue is FIFO: the items will be
   retrieved ("dequeued") from the queue in the order they were placed
   into it.  */

static void
url_enqueue (struct url_queue *queue, struct iri *i,
             const char *url, const char *referer, int depth,
             bool html_allowed, bool css_allowed)
{
  struct queue_element *qel = xnew (struct queue_element);
  qel->iri = i;
  qel->url = url;
  qel->referer = referer;
  qel->depth = depth;
  qel->html_allowed = html_allowed;
  qel->css_allowed = css_allowed;
  qel->next = NULL;

  ++queue->count;
  if (queue->count > queue->maxcount)
    queue->maxcount = queue->count;

  DEBUGP (("Enqueuing %s at depth %d\n",
           quotearg_n_style (0, escape_quoting_style, url), depth));
  DEBUGP (("Queue count %d, maxcount %d.\n", queue->count, queue->maxcount));

  if (i)
    DEBUGP (("[IRI Enqueuing %s with %s\n", quote_n (0, url),
             i->uri_encoding ? quote_n (1, i->uri_encoding) : "None"));

  if (queue->tail)
    queue->tail->next = qel;
  queue->tail = qel;

  if (!queue->head)
    queue->head = queue->tail;
}

/* Take a URL out of the queue.  Return true if this operation
   succeeded, or false if the queue is empty.  */

static bool
url_dequeue (struct url_queue *queue, struct iri **i,
             const char **url, const char **referer, int *depth,
             bool *html_allowed, bool *css_allowed)
{
  struct queue_element *qel = queue->head;

  if (!qel)
    return false;

  queue->head = queue->head->next;
  if (!queue->head)
    queue->tail = NULL;

  *i = qel->iri;
  *url = qel->url;
  *referer = qel->referer;
  *depth = qel->depth;
  *html_allowed = qel->html_allowed;
  *css_allowed = qel->css_allowed;

  --queue->count;

  DEBUGP (("Dequeuing %s at depth %d\n",
           quotearg_n_style (0, escape_quoting_style, qel->url), qel->depth));
  DEBUGP (("Queue count %d, maxcount %d.\n", queue->count, queue->maxcount));

  xfree (qel);
  return true;
}

static void blacklist_add (struct hash_table *blacklist, const char *url)
{
  char *url_unescaped = xstrdup (url);

  url_unescape (url_unescaped);
  string_set_add (blacklist, url_unescaped);
  xfree (url_unescaped);
}

static int blacklist_contains (struct hash_table *blacklist, const char *url)
{
  char *url_unescaped = xstrdup(url);
  int ret;

  url_unescape (url_unescaped);
  ret = string_set_contains (blacklist, url_unescaped);
  xfree (url_unescaped);

  return ret;
}

typedef enum
{
  WG_RR_SUCCESS, WG_RR_BLACKLIST, WG_RR_NOTHTTPS, WG_RR_NONHTTP, WG_RR_ABSOLUTE,
  WG_RR_DOMAIN, WG_RR_PARENT, WG_RR_LIST, WG_RR_REGEX, WG_RR_RULES,
  WG_RR_SPANNEDHOST, WG_RR_ROBOTS
} reject_reason;

static reject_reason download_child (const struct urlpos *, struct url *, int,
                              struct url *, struct hash_table *, struct iri *);
static reject_reason descend_redirect (const char *, struct url *, int,
                              struct url *, struct hash_table *, struct iri *);
static void write_reject_log_header (FILE *);
static void write_reject_log_reason (FILE *, reject_reason,
                              const struct url *, const struct url *);

/* Retrieve a part of the web beginning with START_URL.  This used to
   be called "recursive retrieval", because the old function was
   recursive and implemented depth-first search.  retrieve_tree on the
   other hand implements breadth-search traversal of the tree, which
   results in much nicer ordering of downloads.

   The algorithm this function uses is simple:

   1. put START_URL in the queue.
   2. while there are URLs in the queue:

     3. get next URL from the queue.
     4. download it.
     5. if the URL is HTML and its depth does not exceed maximum depth,
        get the list of URLs embedded therein.
     6. for each of those URLs do the following:

       7. if the URL is not one of those downloaded before, and if it
          satisfies the criteria specified by the various command-line
          options, add it to the queue. */

uerr_t
retrieve_tree (struct url *start_url_parsed, struct iri *pi)
{
  uerr_t status = RETROK;

  /* The queue of URLs we need to load. */
  struct url_queue *queue;

  /* The URLs we do not wish to enqueue, because they are already in
     the queue, but haven't been downloaded yet.  */
  struct hash_table *blacklist;

  struct iri *i = iri_new ();

  FILE *rejectedlog = NULL; /* Don't write a rejected log. */

  /* Duplicate pi struct if not NULL */
  if (pi)
    {
#define COPYSTR(x)  (x) ? xstrdup(x) : NULL;
      i->uri_encoding = COPYSTR (pi->uri_encoding);
      i->content_encoding = COPYSTR (pi->content_encoding);
      i->utf8_encode = pi->utf8_encode;
#undef COPYSTR
    }
#ifdef ENABLE_IRI
  else
    set_uri_encoding (i, opt.locale, true);
#endif

  queue = url_queue_new ();
  blacklist = make_string_hash_table (0);

  /* Enqueue the starting URL.  Use start_url_parsed->url rather than
     just URL so we enqueue the canonical form of the URL.  */
  url_enqueue (queue, i, xstrdup (start_url_parsed->url), NULL, 0, true,
               false);
  blacklist_add (blacklist, start_url_parsed->url);

  if (opt.rejected_log)
    {
      rejectedlog = fopen (opt.rejected_log, "w");
      write_reject_log_header (rejectedlog);
      if (!rejectedlog)
        logprintf (LOG_NOTQUIET, "%s: %s\n", opt.rejected_log, strerror (errno));
    }

  while (1)
    {
      bool descend = false;
      char *url, *referer, *file = NULL;
      int depth;
      bool html_allowed, css_allowed;
      bool is_css = false;
      bool dash_p_leaf_HTML = false;

      if (opt.quota && total_downloaded_bytes > opt.quota)
        break;
      if (status == FWRITEERR)
        break;

      /* Get the next URL from the queue... */

      if (!url_dequeue (queue, (struct iri **) &i,
                        (const char **)&url, (const char **)&referer,
                        &depth, &html_allowed, &css_allowed))
        break;

      /* ...and download it.  Note that this download is in most cases
         unconditional, as download_child already makes sure a file
         doesn't get enqueued twice -- and yet this check is here, and
         not in download_child.  This is so that if you run `wget -r
         URL1 URL2', and a random URL is encountered once under URL1
         and again under URL2, but at a different (possibly smaller)
         depth, we want the URL's children to be taken into account
         the second time.  */
      if (dl_url_file_map && hash_table_contains (dl_url_file_map, url))
        {
          bool is_css_bool;

          file = xstrdup (hash_table_get (dl_url_file_map, url));

          DEBUGP (("Already downloaded \"%s\", reusing it from \"%s\".\n",
                   url, file));

          if ((is_css_bool = (css_allowed
                  && downloaded_css_set
                  && string_set_contains (downloaded_css_set, file)))
              || (html_allowed
                && downloaded_html_set
                && string_set_contains (downloaded_html_set, file)))
            {
              descend = true;
              is_css = is_css_bool;
            }
        }
      else
        {
          int dt = 0, url_err;
          char *redirected = NULL;
          struct url *url_parsed = url_parse (url, &url_err, i, true);

          if (!url_parsed)
            {
              char *error = url_error (url, url_err);
              logprintf (LOG_NOTQUIET, "%s: %s.\n",url, error);
              xfree (error);
              inform_exit_status (URLERROR);
            }
          else
            {

              status = retrieve_url (url_parsed, url, &file, &redirected, referer,
                                     &dt, false, i, true);

              if (html_allowed && file && status == RETROK
                  && (dt & RETROKF) && (dt & TEXTHTML))
                {
                  descend = true;
                  is_css = false;
                }

              /* a little different, css_allowed can override content type
                 lots of web servers serve css with an incorrect content type
              */
              if (file && status == RETROK
                  && (dt & RETROKF) &&
                  ((dt & TEXTCSS) || css_allowed))
                {
                  descend = true;
                  is_css = true;
                }

              if (redirected)
                {
                  /* We have been redirected, possibly to another host, or
                     different path, or wherever.  Check whether we really
                     want to follow it.  */
                  if (descend)
                    {
                      reject_reason r = descend_redirect (redirected, url_parsed,
                                        depth, start_url_parsed, blacklist, i);
                      if (r == WG_RR_SUCCESS)
                        {
                          /* Make sure that the old pre-redirect form gets
                             blacklisted. */
                          blacklist_add (blacklist, url);
                        }
                      else
                        {
                          write_reject_log_reason (rejectedlog, r, url_parsed, start_url_parsed);
                          descend = false;
                        }
                    }

                  xfree (url);
                  url = redirected;
                }
              else
                {
                  xfree (url);
                  url = xstrdup (url_parsed->url);
                }
              url_free (url_parsed);
            }
        }

      if (opt.spider)
        {
          visited_url (url, referer);
        }

      if (descend
          && depth >= opt.reclevel && opt.reclevel != INFINITE_RECURSION)
        {
          if (opt.page_requisites
              && (depth == opt.reclevel || depth == opt.reclevel + 1))
            {
              /* When -p is specified, we are allowed to exceed the
                 maximum depth, but only for the "inline" links,
                 i.e. those that are needed to display the page.
                 Originally this could exceed the depth at most by
                 one, but we allow one more level so that the leaf
                 pages that contain frames can be loaded
                 correctly.  */
              dash_p_leaf_HTML = true;
            }
          else
            {
              /* Either -p wasn't specified or it was and we've
                 already spent the two extra (pseudo-)levels that it
                 affords us, so we need to bail out. */
              DEBUGP (("Not descending further; at depth %d, max. %d.\n",
                       depth, opt.reclevel));
              descend = false;
            }
        }

      /* If the downloaded document was HTML or CSS, parse it and enqueue the
         links it contains. */

      if (descend)
        {
          bool meta_disallow_follow = false;
          struct urlpos *children
            = is_css ? get_urls_css_file (file, url) :
                       get_urls_html (file, url, &meta_disallow_follow, i);

          if (opt.use_robots && meta_disallow_follow)
            {
              free_urlpos (children);
              children = NULL;
            }

          if (children)
            {
              struct urlpos *child = children;
              struct url *url_parsed = url_parse (url, NULL, i, true);
              struct iri *ci;
              char *referer_url = url;
              bool strip_auth;

              assert (url_parsed != NULL);

              if (!url_parsed)
                continue;

              strip_auth = (url_parsed && url_parsed->user);

              /* Strip auth info if present */
              if (strip_auth)
                referer_url = url_string (url_parsed, URL_AUTH_HIDE);

              for (; child; child = child->next)
                {
                  reject_reason r;

                  if (child->ignore_when_downloading)
                    {
                      DEBUGP (("Not following due to 'ignore' flag: %s\n", child->url->url));
                      continue;
                    }

                  if (dash_p_leaf_HTML && !child->link_inline_p)
                    {
                      DEBUGP (("Not following due to 'link inline' flag: %s\n", child->url->url));
                      continue;
                    }

                  r = download_child (child, url_parsed, depth,
                                      start_url_parsed, blacklist, i);
                  if (r == WG_RR_SUCCESS)
                    {
                      ci = iri_new ();
                      set_uri_encoding (ci, i->content_encoding, false);
                      url_enqueue (queue, ci, xstrdup (child->url->url),
                                   xstrdup (referer_url), depth + 1,
                                   child->link_expect_html,
                                   child->link_expect_css);
                      /* We blacklist the URL we have enqueued, because we
                         don't want to enqueue (and hence download) the
                         same URL twice.  */
                      blacklist_add (blacklist, child->url->url);
                    }
                  else
                    {
                      write_reject_log_reason (rejectedlog, r, child->url, url_parsed);
                    }
                }

              if (strip_auth)
                xfree (referer_url);
              url_free (url_parsed);
              free_urlpos (children);
            }
        }

      if (file
          && (opt.delete_after
              || opt.spider /* opt.recursive is implicitly true */
              || !acceptable (file)))
        {
          /* Either --delete-after was specified, or we loaded this
             (otherwise unneeded because of --spider or rejected by -R)
             HTML file just to harvest its hyperlinks -- in either case,
             delete the local file. */
          DEBUGP (("Removing file due to %s in recursive_retrieve():\n",
                   opt.delete_after ? "--delete-after" :
                   (opt.spider ? "--spider" :
                    "recursive rejection criteria")));
          logprintf (LOG_VERBOSE,
                     (opt.delete_after || opt.spider
                      ? _("Removing %s.\n")
                      : _("Removing %s since it should be rejected.\n")),
                     file);
          if (unlink (file))
            logprintf (LOG_NOTQUIET, "unlink: %s\n", strerror (errno));
          logputs (LOG_VERBOSE, "\n");
          register_delete_file (file);
        }

      xfree (url);
      xfree (referer);
      xfree (file);
      iri_free (i);
    }

  if (rejectedlog)
    fclose (rejectedlog);

  /* If anything is left of the queue due to a premature exit, free it
     now.  */
  {
    char *d1, *d2;
    int d3;
    bool d4, d5;
    struct iri *d6;
    while (url_dequeue (queue, (struct iri **)&d6,
                        (const char **)&d1, (const char **)&d2, &d3, &d4, &d5))
      {
        iri_free (d6);
        xfree (d1);
        xfree (d2);
      }
  }
  url_queue_delete (queue);

  string_set_free (blacklist);

  if (opt.quota && total_downloaded_bytes > opt.quota)
    return QUOTEXC;
  else if (status == FWRITEERR)
    return FWRITEERR;
  else
    return RETROK;
}

/* Based on the context provided by retrieve_tree, decide whether a
   URL is to be descended to.  This is only ever called from
   retrieve_tree, but is in a separate function for clarity.

   The most expensive checks (such as those for robots) are memoized
   by storing these URLs to BLACKLIST.  This may or may not help.  It
   will help if those URLs are encountered many times.  */

static reject_reason
download_child (const struct urlpos *upos, struct url *parent, int depth,
                  struct url *start_url_parsed, struct hash_table *blacklist,
                  struct iri *iri)
{
  struct url *u = upos->url;
  const char *url = u->url;
  bool u_scheme_like_http;
  reject_reason reason = WG_RR_SUCCESS;

  DEBUGP (("Deciding whether to enqueue \"%s\".\n", url));

  if (blacklist_contains (blacklist, url))
    {
      if (opt.spider)
        {
          char *referrer = url_string (parent, URL_AUTH_HIDE_PASSWD);
          DEBUGP (("download_child: parent->url is: %s\n", quote (parent->url)));
          visited_url (url, referrer);
          xfree (referrer);
        }
      DEBUGP (("Already on the black list.\n"));
      reason = WG_RR_BLACKLIST;
      goto out;
    }

  /* Several things to check for:
     1. if scheme is not https and https_only requested
     2. if scheme is not http, and we don't load it
     3. check for relative links (if relative_only is set)
     4. check for domain
     5. check for no-parent
     6. check for excludes && includes
     7. check for suffix
     8. check for same host (if spanhost is unset), with possible
     gethostbyname baggage
     9. check for robots.txt

     Addendum: If the URL is FTP, and it is to be loaded, only the
     domain and suffix settings are "stronger".

     Note that .html files will get loaded regardless of suffix rules
     (but that is remedied later with unlink) unless the depth equals
     the maximum depth.

     More time- and memory- consuming tests should be put later on
     the list.  */

#ifdef HAVE_SSL
  if (opt.https_only && u->scheme != SCHEME_HTTPS)
    {
      DEBUGP (("Not following non-HTTPS links.\n"));
      reason = WG_RR_NOTHTTPS;
      goto out;
    }
#endif

  /* Determine whether URL under consideration has a HTTP-like scheme. */
  u_scheme_like_http = schemes_are_similar_p (u->scheme, SCHEME_HTTP);

  /* 1. Schemes other than HTTP are normally not recursed into. */
  if (!u_scheme_like_http && !((u->scheme == SCHEME_FTP
#ifdef HAVE_SSL
      || u->scheme == SCHEME_FTPS
#endif
      ) && opt.follow_ftp))
    {
      DEBUGP (("Not following non-HTTP schemes.\n"));
      reason = WG_RR_NONHTTP;
      goto out;
    }

  /* 2. If it is an absolute link and they are not followed, throw it
     out.  */
  if (u_scheme_like_http)
    if (opt.relative_only && !upos->link_relative_p)
      {
        DEBUGP (("It doesn't really look like a relative link.\n"));
        reason = WG_RR_ABSOLUTE;
        goto out;
      }

  /* 3. If its domain is not to be accepted/looked-up, chuck it
     out.  */
  if (!accept_domain (u))
    {
      DEBUGP (("The domain was not accepted.\n"));
      reason = WG_RR_DOMAIN;
      goto out;
    }

  /* 4. Check for parent directory.

     If we descended to a different host or changed the scheme, ignore
     opt.no_parent.  Also ignore it for documents needed to display
     the parent page when in -p mode.  */
  if (opt.no_parent
      && schemes_are_similar_p (u->scheme, start_url_parsed->scheme)
      && 0 == strcasecmp (u->host, start_url_parsed->host)
      && (u->scheme != start_url_parsed->scheme
          || u->port == start_url_parsed->port)
      && !(opt.page_requisites && upos->link_inline_p))
    {
      if (!subdir_p (start_url_parsed->dir, u->dir))
        {
          DEBUGP (("Going to \"%s\" would escape \"%s\" with no_parent on.\n",
                   u->dir, start_url_parsed->dir));
          reason = WG_RR_PARENT;
          goto out;
        }
    }

  /* 5. If the file does not match the acceptance list, or is on the
     rejection list, chuck it out.  The same goes for the directory
     exclusion and inclusion lists.  */
  if (opt.includes || opt.excludes)
    {
      if (!accdir (u->dir))
        {
          DEBUGP (("%s (%s) is excluded/not-included.\n", url, u->dir));
          reason = WG_RR_LIST;
          goto out;
        }
    }
  if (!accept_url (url))
    {
      DEBUGP (("%s is excluded/not-included through regex.\n", url));
      reason = WG_RR_REGEX;
      goto out;
    }

  /* 6. Check for acceptance/rejection rules.  We ignore these rules
     for directories (no file name to match) and for non-leaf HTMLs,
     which can lead to other files that do need to be downloaded.  (-p
     automatically implies non-leaf because with -p we can, if
     necessary, overstep the maximum depth to get the page requisites.)  */
  if (u->file[0] != '\0'
      && !(has_html_suffix_p (u->file)
           /* The exception only applies to non-leaf HTMLs (but -p
              always implies non-leaf because we can overstep the
              maximum depth to get the requisites): */
           && (/* non-leaf */
               opt.reclevel == INFINITE_RECURSION
               /* also non-leaf */
               || depth < opt.reclevel - 1
               /* -p, which implies non-leaf (see above) */
               || opt.page_requisites)))
    {
      if (!acceptable (u->file))
        {
          DEBUGP (("%s (%s) does not match acc/rej rules.\n",
                   url, u->file));
          reason = WG_RR_RULES;
          goto out;
        }
    }

  /* 7. */
  if (schemes_are_similar_p (u->scheme, parent->scheme))
    if (!opt.spanhost && 0 != strcasecmp (parent->host, u->host))
      {
        DEBUGP (("This is not the same hostname as the parent's (%s and %s).\n",
                 u->host, parent->host));
        reason = WG_RR_SPANNEDHOST;
        goto out;
      }

  /* 8. */
  if (opt.use_robots && u_scheme_like_http)
    {
      struct robot_specs *specs = res_get_specs (u->host, u->port);
      if (!specs)
        {
          char *rfile;
          if (res_retrieve_file (url, &rfile, iri))
            {
              specs = res_parse_from_file (rfile);

              /* Delete the robots.txt file if we chose to either delete the
                 files after downloading or we're just running a spider. */
              if (opt.delete_after || opt.spider)
                {
                  logprintf (LOG_VERBOSE, _("Removing %s.\n"), rfile);
                  if (unlink (rfile))
                      logprintf (LOG_NOTQUIET, "unlink: %s\n",
                                 strerror (errno));
                }

              xfree (rfile);
            }
          else
            {
              /* If we cannot get real specs, at least produce
                 dummy ones so that we can register them and stop
                 trying to retrieve them.  */
              specs = res_parse ("", 0);
            }
          res_register_specs (u->host, u->port, specs);
        }

      /* Now that we have (or don't have) robots.txt specs, we can
         check what they say.  */
      if (!res_match_path (specs, u->path))
        {
          DEBUGP (("Not following %s because robots.txt forbids it.\n", url));
          blacklist_add (blacklist, url);
          reason = WG_RR_ROBOTS;
          goto out;
        }
    }

  out:

  if (reason == WG_RR_SUCCESS)
    /* The URL has passed all the tests.  It can be placed in the
       download queue. */
    DEBUGP (("Decided to load it.\n"));
  else
    DEBUGP (("Decided NOT to load it.\n"));

  return reason;
}

/* This function determines whether we will consider downloading the
   children of a URL whose download resulted in a redirection,
   possibly to another host, etc.  It is needed very rarely, and thus
   it is merely a simple-minded wrapper around download_child.  */

static reject_reason
descend_redirect (const char *redirected, struct url *orig_parsed, int depth,
                    struct url *start_url_parsed, struct hash_table *blacklist,
                    struct iri *iri)
{
  struct url *new_parsed;
  struct urlpos *upos;
  reject_reason reason;

  assert (orig_parsed != NULL);

  new_parsed = url_parse (redirected, NULL, NULL, false);
  assert (new_parsed != NULL);

  upos = xnew0 (struct urlpos);
  upos->url = new_parsed;

  reason = download_child (upos, orig_parsed, depth,
                              start_url_parsed, blacklist, iri);

  if (reason == WG_RR_SUCCESS)
    blacklist_add (blacklist, upos->url->url);
  else if (reason == WG_RR_LIST || reason == WG_RR_REGEX)
    {
      DEBUGP (("Ignoring decision for redirects, decided to load it.\n"));
      blacklist_add (blacklist, upos->url->url);
      reason = WG_RR_SUCCESS;
    }
  else
    DEBUGP (("Redirection \"%s\" failed the test.\n", redirected));

  url_free (new_parsed);
  xfree (upos);

  return reason;
}


/* This function writes the rejected log header. */
static void
write_reject_log_header (FILE *f)
{
  if (!f)
    return;

  /* Note: Update this header when columns change in any way. */
  fprintf (f, "REASON\t"
    "U_URL\tU_SCHEME\tU_HOST\tU_PORT\tU_PATH\tU_PARAMS\tU_QUERY\tU_FRAGMENT\t"
    "P_URL\tP_SCHEME\tP_HOST\tP_PORT\tP_PATH\tP_PARAMS\tP_QUERY\tP_FRAGMENT\n");
}

/* This function writes a URL to the reject log. Internal use only. */
static void
write_reject_log_url (FILE *fp, const struct url *url)
{
  const char *escaped_str;
  const char *scheme_str;

  if (!fp)
    return;

  escaped_str = url_escape (url->url);

  switch (url->scheme)
    {
      case SCHEME_HTTP:  scheme_str = "SCHEME_HTTP";    break;
#ifdef HAVE_SSL
      case SCHEME_HTTPS: scheme_str = "SCHEME_HTTPS";   break;
      case SCHEME_FTPS:  scheme_str = "SCHEME_FTPS";    break;
#endif
      case SCHEME_FTP:   scheme_str = "SCHEME_FTP";     break;
      default:           scheme_str = "SCHEME_INVALID"; break;
    }

  fprintf (fp, "%s\t%s\t%s\t%i\t%s\t%s\t%s\t%s",
    escaped_str,
    scheme_str,
    url->host,
    url->port,
    url->path,
    url->params ? url->params : "",
    url->query ? url->query : "",
    url->fragment ? url->fragment : "");

  xfree (escaped_str);
}

/* This function writes out information on why a URL was rejected and its
   context from download_child such as the URL being rejected and it's
   parent's URL. The format it uses is comma separated values but with tabs. */
static void
write_reject_log_reason (FILE *fp, reject_reason reason,
                         const struct url *url, const struct url *parent)
{
  const char *reason_str;

  if (!fp)
    return;

  switch (reason)
    {
      case WG_RR_SUCCESS:     reason_str = "SUCCESS";     break;
      case WG_RR_BLACKLIST:   reason_str = "BLACKLIST";   break;
      case WG_RR_NOTHTTPS:    reason_str = "NOTHTTPS";    break;
      case WG_RR_NONHTTP:     reason_str = "NONHTTP";     break;
      case WG_RR_ABSOLUTE:    reason_str = "ABSOLUTE";    break;
      case WG_RR_DOMAIN:      reason_str = "DOMAIN";      break;
      case WG_RR_PARENT:      reason_str = "PARENT";      break;
      case WG_RR_LIST:        reason_str = "LIST";        break;
      case WG_RR_REGEX:       reason_str = "REGEX";       break;
      case WG_RR_RULES:       reason_str = "RULES";       break;
      case WG_RR_SPANNEDHOST: reason_str = "SPANNEDHOST"; break;
      case WG_RR_ROBOTS:      reason_str = "ROBOTS";      break;
      default:                reason_str = "UNKNOWN";     break;
    }

  fprintf (fp, "%s\t", reason_str);
  write_reject_log_url (fp, url);
  fprintf (fp, "\t");
  write_reject_log_url (fp, parent);
  fprintf (fp, "\n");
}

/* vim:set sts=2 sw=2 cino+={s: */

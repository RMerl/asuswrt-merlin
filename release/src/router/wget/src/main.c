/* Command line parsing.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Free
   Software Foundation, Inc.

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
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <spawn.h>
#ifdef ENABLE_NLS
# include <locale.h>
#endif
#include <assert.h>
#include <errno.h>
#include <time.h>

#include "exits.h"
#include "utils.h"
#include "init.h"
#include "retr.h"
#include "recur.h"
#include "host.h"
#include "url.h"
#include "progress.h"           /* for progress_handle_sigwinch */
#include "convert.h"
#include "spider.h"
#include "http.h"               /* for save_cookies */
#include "hsts.h"               /* for initializing hsts_store to NULL */
#include "ptimer.h"
#include "warc.h"
#include "version.h"
#include "c-strcase.h"
#include "dirname.h"
#include "xmemdup0.h"
#include <getopt.h>
#include <getpass.h>
#include <quote.h>

#ifdef HAVE_METALINK
# include <metalink/metalink_parser.h>
# include "metalink.h"
#endif

#ifdef WINDOWS
# include <io.h>
# include <fcntl.h>
#endif

#ifdef __VMS
# include "vms.h"
#endif /* __VMS */

#ifndef PATH_SEPARATOR
# define PATH_SEPARATOR '/'
#endif

#ifndef ENABLE_IRI
struct iri dummy_iri;
#endif

#ifdef HAVE_LIBCARES
#include <ares.h>
ares_channel ares;
#else
void *ares;
#endif

struct options opt;

/* defined in version.c */
extern char *system_getrc;
/* Used for --version output in print_version */
#define MAX_CHARS_PER_LINE      72
#define TABULATION              4

const char *exec_name;

/* Number of successfully downloaded URLs */
int numurls = 0;

#ifndef TESTING
/* Initialize I18N/L10N.  That amounts to invoking setlocale, and
   setting up gettext's message catalog using bindtextdomain and
   textdomain.  Does nothing if NLS is disabled or missing.  */

#if defined(SIGHUP) || defined(SIGUSR1)
/* Hangup signal handler.  When wget receives SIGHUP or SIGUSR1, it
   will proceed operation as usual, trying to write into a log file.
   If that is impossible, the output will be turned off.  */

static void
redirect_output_signal (int sig)
{
  const char *signal_name = "WTF?!";

#ifdef SIGHUP
  if (sig == SIGHUP)
    signal_name = "SIGHUP";
#endif
#ifdef SIGUSR1
  if (sig == SIGUSR1)
    signal_name = "SIGUSR1";
#endif

  redirect_output (true,signal_name);
  progress_schedule_redirect ();
  signal (sig, redirect_output_signal);
}
#endif /* defined(SIGHUP) || defined(SIGUSR1) */

static void
i18n_initialize (void)
{
  /* ENABLE_NLS implies existence of functions invoked here.  */
#ifdef ENABLE_NLS
  /* Set the current locale.  */
  setlocale (LC_ALL, "");
  /* Set the text message domain.  */
  bindtextdomain ("wget", LOCALEDIR);
  textdomain ("wget");
#endif /* ENABLE_NLS */
}

#ifdef HAVE_HSTS
/* make the HSTS store global */
hsts_store_t hsts_store;

static char*
get_hsts_database (void)
{
  char *home;

  if (opt.hsts_file)
    return xstrdup (opt.hsts_file);

  home = home_dir ();
  if (home)
    {
      char *dir = aprintf ("%s/.wget-hsts", home);
      xfree(home);
      return dir;
    }

  return NULL;
}

static void
load_hsts (void)
{
  if (!hsts_store)
    {
      char *filename = get_hsts_database ();

      if (filename)
        {
          DEBUGP (("Reading HSTS entries from %s\n", filename));

          hsts_store = hsts_store_open (filename);

          if (!hsts_store)
            logprintf (LOG_NOTQUIET, "ERROR: could not open HSTS store at '%s'. "
                       "HSTS will be disabled.\n",
                       filename);
        }
      else
        logprintf (LOG_NOTQUIET, "ERROR: could not open HSTS store. HSTS will be disabled.\n");

      xfree (filename);
    }
}

static void
save_hsts (void)
{
  if (hsts_store)
    {
      char *filename = get_hsts_database ();

      if (filename && hsts_store_has_changed (hsts_store))
        {
          DEBUGP (("Saving HSTS entries to %s\n", filename));
          hsts_store_save (hsts_store, filename);
        }

      hsts_store_close (hsts_store);

      xfree (filename);
    }
}
#endif

/* Definition of command-line options. */

_Noreturn static void print_help (void);
_Noreturn static void print_version (void);

#ifdef HAVE_SSL
# define IF_SSL(x) x
#else
# define IF_SSL(x) NULL
#endif

struct cmdline_option {
  const char *long_name;
  char short_name;
  enum {
    OPT_VALUE,
    OPT_BOOLEAN,
    OPT_FUNCALL,
    /* Non-standard options that have to be handled specially in
       main().  */
    OPT__APPEND_OUTPUT,
    OPT__CLOBBER,
    OPT__DONT_REMOVE_LISTING,
    OPT__EXECUTE,
    OPT__NO,
    OPT__PARENT
  } type;
  const void *data;             /* for standard options */
  int argtype;                  /* for non-standard options */
};

static struct cmdline_option option_data[] =
  {
    { "accept", 'A', OPT_VALUE, "accept", -1 },
    { "accept-regex", 0, OPT_VALUE, "acceptregex", -1 },
    { "adjust-extension", 'E', OPT_BOOLEAN, "adjustextension", -1 },
    { "append-output", 'a', OPT__APPEND_OUTPUT, NULL, required_argument },
    { "ask-password", 0, OPT_BOOLEAN, "askpassword", -1 },
    { "auth-no-challenge", 0, OPT_BOOLEAN, "authnochallenge", -1 },
    { "background", 'b', OPT_BOOLEAN, "background", -1 },
    { "backup-converted", 'K', OPT_BOOLEAN, "backupconverted", -1 },
    { "backups", 0, OPT_BOOLEAN, "backups", -1 },
    { "base", 'B', OPT_VALUE, "base", -1 },
    { "bind-address", 0, OPT_VALUE, "bindaddress", -1 },
#ifdef HAVE_LIBCARES
    { "bind-dns-address", 0, OPT_VALUE, "binddnsaddress", -1 },
#endif
    { "body-data", 0, OPT_VALUE, "bodydata", -1 },
    { "body-file", 0, OPT_VALUE, "bodyfile", -1 },
    { IF_SSL ("ca-certificate"), 0, OPT_VALUE, "cacertificate", -1 },
    { IF_SSL ("ca-directory"), 0, OPT_VALUE, "cadirectory", -1 },
    { "cache", 0, OPT_BOOLEAN, "cache", -1 },
    { IF_SSL ("certificate"), 0, OPT_VALUE, "certificate", -1 },
    { IF_SSL ("certificate-type"), 0, OPT_VALUE, "certificatetype", -1 },
    { IF_SSL ("check-certificate"), 0, OPT_BOOLEAN, "checkcertificate", -1 },
    { "clobber", 0, OPT__CLOBBER, NULL, optional_argument },
#ifdef HAVE_LIBZ
    { "compression", 0, OPT_VALUE, "compression", -1 },
#endif
    { "config", 0, OPT_VALUE, "chooseconfig", -1 },
    { "connect-timeout", 0, OPT_VALUE, "connecttimeout", -1 },
    { "continue", 'c', OPT_BOOLEAN, "continue", -1 },
    { "convert-file-only", 0, OPT_BOOLEAN, "convertfileonly", -1 },
    { "convert-links", 'k', OPT_BOOLEAN, "convertlinks", -1 },
    { "content-disposition", 0, OPT_BOOLEAN, "contentdisposition", -1 },
    { "content-on-error", 0, OPT_BOOLEAN, "contentonerror", -1 },
    { "cookies", 0, OPT_BOOLEAN, "cookies", -1 },
    { IF_SSL ("crl-file"), 0, OPT_VALUE, "crlfile", -1 },
    { "cut-dirs", 0, OPT_VALUE, "cutdirs", -1 },
    { "debug", 'd', OPT_BOOLEAN, "debug", -1 },
    { "default-page", 0, OPT_VALUE, "defaultpage", -1 },
    { "delete-after", 0, OPT_BOOLEAN, "deleteafter", -1 },
    { "directories", 0, OPT_BOOLEAN, "dirstruct", -1 },
    { "directory-prefix", 'P', OPT_VALUE, "dirprefix", -1 },
    { "dns-cache", 0, OPT_BOOLEAN, "dnscache", -1 },
#ifdef HAVE_LIBCARES
    { "dns-servers", 0, OPT_VALUE, "dnsservers", -1 },
#endif
    { "dns-timeout", 0, OPT_VALUE, "dnstimeout", -1 },
    { "domains", 'D', OPT_VALUE, "domains", -1 },
    { "dont-remove-listing", 0, OPT__DONT_REMOVE_LISTING, NULL, no_argument },
    { "dot-style", 0, OPT_VALUE, "dotstyle", -1 }, /* deprecated */
    { "egd-file", 0, OPT_VALUE, "egdfile", -1 },
    { "exclude-directories", 'X', OPT_VALUE, "excludedirectories", -1 },
    { "exclude-domains", 0, OPT_VALUE, "excludedomains", -1 },
    { "execute", 'e', OPT__EXECUTE, NULL, required_argument },
    { "follow-ftp", 0, OPT_BOOLEAN, "followftp", -1 },
    { "follow-tags", 0, OPT_VALUE, "followtags", -1 },
    { "force-directories", 'x', OPT_BOOLEAN, "dirstruct", -1 },
    { "force-html", 'F', OPT_BOOLEAN, "forcehtml", -1 },
    { "ftp-password", 0, OPT_VALUE, "ftppassword", -1 },
#ifdef __VMS
    { "ftp-stmlf", 0, OPT_BOOLEAN, "ftpstmlf", -1 },
#endif /* def __VMS */
    { "ftp-user", 0, OPT_VALUE, "ftpuser", -1 },
#ifdef HAVE_SSL
    { "ftps-clear-data-connection", 0, OPT_BOOLEAN, "ftpscleardataconnection", -1 },
    { "ftps-fallback-to-ftp", 0, OPT_BOOLEAN, "ftpsfallbacktoftp", -1 },
    { "ftps-implicit", 0, OPT_BOOLEAN, "ftpsimplicit", -1 },
    { "ftps-resume-ssl", 0, OPT_BOOLEAN, "ftpsresumessl", -1 },
#endif
    { "glob", 0, OPT_BOOLEAN, "glob", -1 },
    { "header", 0, OPT_VALUE, "header", -1 },
    { "help", 'h', OPT_FUNCALL, (void *)print_help, no_argument },
    { "host-directories", 0, OPT_BOOLEAN, "addhostdir", -1 },
#ifdef HAVE_HSTS
    { "hsts", 0, OPT_BOOLEAN, "hsts", -1},
    { "hsts-file", 0, OPT_VALUE, "hstsfile", -1 },
#endif
    { "html-extension", 'E', OPT_BOOLEAN, "adjustextension", -1 }, /* deprecated */
    { "htmlify", 0, OPT_BOOLEAN, "htmlify", -1 },
    { "http-keep-alive", 0, OPT_BOOLEAN, "httpkeepalive", -1 },
    { "http-passwd", 0, OPT_VALUE, "httppassword", -1 }, /* deprecated */
    { "http-password", 0, OPT_VALUE, "httppassword", -1 },
    { "http-user", 0, OPT_VALUE, "httpuser", -1 },
    { IF_SSL ("https-only"), 0, OPT_BOOLEAN, "httpsonly", -1 },
    { "ignore-case", 0, OPT_BOOLEAN, "ignorecase", -1 },
    { "ignore-length", 0, OPT_BOOLEAN, "ignorelength", -1 },
    { "ignore-tags", 0, OPT_VALUE, "ignoretags", -1 },
    { "include-directories", 'I', OPT_VALUE, "includedirectories", -1 },
#ifdef ENABLE_IPV6
    { "inet4-only", '4', OPT_BOOLEAN, "inet4only", -1 },
    { "inet6-only", '6', OPT_BOOLEAN, "inet6only", -1 },
#endif
    { "input-file", 'i', OPT_VALUE, "input", -1 },
#ifdef HAVE_METALINK
    { "input-metalink", 0, OPT_VALUE, "inputmetalink", -1 },
#endif
    { "iri", 0, OPT_BOOLEAN, "iri", -1 },
    { "keep-badhash", 0, OPT_BOOLEAN, "keepbadhash", -1 },
    { "keep-session-cookies", 0, OPT_BOOLEAN, "keepsessioncookies", -1 },
    { "level", 'l', OPT_VALUE, "reclevel", -1 },
    { "limit-rate", 0, OPT_VALUE, "limitrate", -1 },
    { "load-cookies", 0, OPT_VALUE, "loadcookies", -1 },
    { "local-encoding", 0, OPT_VALUE, "localencoding", -1 },
    { "rejected-log", 0, OPT_VALUE, "rejectedlog", -1 },
    { "max-redirect", 0, OPT_VALUE, "maxredirect", -1 },
#ifdef HAVE_METALINK
    { "metalink-index", 0, OPT_VALUE, "metalinkindex", -1 },
    { "metalink-over-http", 0, OPT_BOOLEAN, "metalinkoverhttp", -1 },
#endif
    { "method", 0, OPT_VALUE, "method", -1 },
    { "mirror", 'm', OPT_BOOLEAN, "mirror", -1 },
    { "netrc", 0, OPT_BOOLEAN, "netrc", -1 },
    { "no", 'n', OPT__NO, NULL, required_argument },
    { "no-clobber", 0, OPT_BOOLEAN, "noclobber", -1 },
    { "no-config", 0, OPT_BOOLEAN, "noconfig", -1},
    { "no-parent", 0, OPT_BOOLEAN, "noparent", -1 },
    { "output-document", 'O', OPT_VALUE, "outputdocument", -1 },
    { "output-file", 'o', OPT_VALUE, "logfile", -1 },
    { "page-requisites", 'p', OPT_BOOLEAN, "pagerequisites", -1 },
    { "parent", 0, OPT__PARENT, NULL, optional_argument },
    { "passive-ftp", 0, OPT_BOOLEAN, "passiveftp", -1 },
    { "password", 0, OPT_VALUE, "password", -1 },
    { IF_SSL ("pinnedpubkey"), 0, OPT_VALUE, "pinnedpubkey", -1 },
    { "post-data", 0, OPT_VALUE, "postdata", -1 },
    { "post-file", 0, OPT_VALUE, "postfile", -1 },
    { "prefer-family", 0, OPT_VALUE, "preferfamily", -1 },
#ifdef HAVE_METALINK
    { "preferred-location", 0, OPT_VALUE, "preferredlocation", -1 },
#endif
    { "preserve-permissions", 0, OPT_BOOLEAN, "preservepermissions", -1 },
    { IF_SSL ("private-key"), 0, OPT_VALUE, "privatekey", -1 },
    { IF_SSL ("private-key-type"), 0, OPT_VALUE, "privatekeytype", -1 },
    { "progress", 0, OPT_VALUE, "progress", -1 },
    { "show-progress", 0, OPT_BOOLEAN, "showprogress", -1 },
    { "protocol-directories", 0, OPT_BOOLEAN, "protocoldirectories", -1 },
    { "proxy", 0, OPT_BOOLEAN, "useproxy", -1 },
    { "proxy__compat", 'Y', OPT_VALUE, "useproxy", -1 }, /* back-compatible */
    { "proxy-passwd", 0, OPT_VALUE, "proxypassword", -1 }, /* deprecated */
    { "proxy-password", 0, OPT_VALUE, "proxypassword", -1 },
    { "proxy-user", 0, OPT_VALUE, "proxyuser", -1 },
    { "quiet", 'q', OPT_BOOLEAN, "quiet", -1 },
    { "quota", 'Q', OPT_VALUE, "quota", -1 },
    { "random-file", 0, OPT_VALUE, "randomfile", -1 },
    { "random-wait", 0, OPT_BOOLEAN, "randomwait", -1 },
    { "read-timeout", 0, OPT_VALUE, "readtimeout", -1 },
    { "recursive", 'r', OPT_BOOLEAN, "recursive", -1 },
    { "referer", 0, OPT_VALUE, "referer", -1 },
    { "regex-type", 0, OPT_VALUE, "regextype", -1 },
    { "reject", 'R', OPT_VALUE, "reject", -1 },
    { "reject-regex", 0, OPT_VALUE, "rejectregex", -1 },
    { "relative", 'L', OPT_BOOLEAN, "relativeonly", -1 },
    { "remote-encoding", 0, OPT_VALUE, "remoteencoding", -1 },
    { "remove-listing", 0, OPT_BOOLEAN, "removelisting", -1 },
    { "report-speed", 0, OPT_BOOLEAN, "reportspeed", -1 },
    { "restrict-file-names", 0, OPT_BOOLEAN, "restrictfilenames", -1 },
    { "retr-symlinks", 0, OPT_BOOLEAN, "retrsymlinks", -1 },
    { "retry-connrefused", 0, OPT_BOOLEAN, "retryconnrefused", -1 },
    { "retry-on-http-error", 0, OPT_VALUE, "retryonhttperror", -1 },
    { "save-cookies", 0, OPT_VALUE, "savecookies", -1 },
    { "save-headers", 0, OPT_BOOLEAN, "saveheaders", -1 },
    { IF_SSL ("secure-protocol"), 0, OPT_VALUE, "secureprotocol", -1 },
    { "server-response", 'S', OPT_BOOLEAN, "serverresponse", -1 },
    { "span-hosts", 'H', OPT_BOOLEAN, "spanhosts", -1 },
    { "spider", 0, OPT_BOOLEAN, "spider", -1 },
    { "start-pos", 0, OPT_VALUE, "startpos", -1 },
    { "strict-comments", 0, OPT_BOOLEAN, "strictcomments", -1 },
    { "timeout", 'T', OPT_VALUE, "timeout", -1 },
    { "timestamping", 'N', OPT_BOOLEAN, "timestamping", -1 },
    { "if-modified-since", 0, OPT_BOOLEAN, "ifmodifiedsince", -1 },
    { "tries", 't', OPT_VALUE, "tries", -1 },
    { "unlink", 0, OPT_BOOLEAN, "unlink", -1 },
    { "trust-server-names", 0, OPT_BOOLEAN, "trustservernames", -1 },
    { "use-askpass", 0, OPT_VALUE, "useaskpass", -1},
    { "use-server-timestamps", 0, OPT_BOOLEAN, "useservertimestamps", -1 },
    { "user", 0, OPT_VALUE, "user", -1 },
    { "user-agent", 'U', OPT_VALUE, "useragent", -1 },
    { "verbose", 'v', OPT_BOOLEAN, "verbose", -1 },
    { "version", 'V', OPT_FUNCALL, (void *) print_version, no_argument },
    { "wait", 'w', OPT_VALUE, "wait", -1 },
    { "waitretry", 0, OPT_VALUE, "waitretry", -1 },
    { "warc-cdx", 0, OPT_BOOLEAN, "warccdx", -1 },
#ifdef HAVE_LIBZ
    { "warc-compression", 0, OPT_BOOLEAN, "warccompression", -1 },
#endif
    { "warc-dedup", 0, OPT_VALUE, "warccdxdedup", -1 },
    { "warc-digests", 0, OPT_BOOLEAN, "warcdigests", -1 },
    { "warc-file", 0, OPT_VALUE, "warcfile", -1 },
    { "warc-header", 0, OPT_VALUE, "warcheader", -1 },
    { "warc-keep-log", 0, OPT_BOOLEAN, "warckeeplog", -1 },
    { "warc-max-size", 0, OPT_VALUE, "warcmaxsize", -1 },
    { "warc-tempdir", 0, OPT_VALUE, "warctempdir", -1 },
#ifdef USE_WATT32
    { "wdebug", 0, OPT_BOOLEAN, "wdebug", -1 },
#endif
#ifdef ENABLE_XATTR
    { "xattr", 0, OPT_BOOLEAN, "xattr", -1 },
#endif
  };

#undef IF_SSL

/* Return a string that contains S with "no-" prepended.  The string
   is NUL-terminated and allocated off static storage at Wget
   startup.  */

static char *
no_prefix (const char *s)
{
  static char buffer[2048];
  static char *p = buffer;

  char *cp = p;
  int size = 3 + strlen (s) + 1;  /* "no-STRING\0" */
  assert(p + size <= buffer + sizeof (buffer));

  cp[0] = 'n', cp[1] = 'o', cp[2] = '-';
  strcpy (cp + 3, s);
  p += size;
  return cp;
}

/* The arguments that that main passes to getopt_long. */
static struct option long_options[2 * countof (option_data) + 1];
static char short_options[128];

/* Mapping between short option chars and option_data indices. */
static unsigned char optmap[96];

/* Marker for `--no-FOO' values in long_options.  */
#define BOOLEAN_NEG_MARKER 1024

/* Initialize the long_options array used by getopt_long from the data
   in option_data.  */

static void
init_switches (void)
{
  char *p = short_options;
  size_t i, o = 0;
  for (i = 0; i < countof (option_data); i++)
    {
      struct cmdline_option *cmdopt = &option_data[i];
      struct option *longopt;

      if (!cmdopt->long_name)
        /* The option is disabled. */
        continue;

      longopt = &long_options[o++];
      longopt->name = cmdopt->long_name;
      longopt->val = i;
      if (cmdopt->short_name)
        {
          *p++ = cmdopt->short_name;
          optmap[cmdopt->short_name - 32] = longopt - long_options;
        }
      switch (cmdopt->type)
        {
        case OPT_VALUE:
          longopt->has_arg = required_argument;
          if (cmdopt->short_name)
            *p++ = ':';
          break;
        case OPT_BOOLEAN:
          /* Specify an optional argument for long options, so that
             --option=off works the same as --no-option, for
             compatibility with pre-1.10 Wget.  However, don't specify
             optional arguments short-option booleans because they
             prevent combining of short options.  */
          longopt->has_arg = optional_argument;
          /* For Boolean options, add the "--no-FOO" variant, which is
             identical to "--foo", except it has opposite meaning and
             it doesn't allow an argument.  */
          longopt = &long_options[o++];
          longopt->name = no_prefix (cmdopt->long_name);
          longopt->has_arg = no_argument;
          /* Mask the value so we'll be able to recognize that we're
             dealing with the false value.  */
          longopt->val = i | BOOLEAN_NEG_MARKER;
          break;
        default:
          assert (cmdopt->argtype != -1);
          longopt->has_arg = cmdopt->argtype;
          if (cmdopt->short_name)
            {
              if (longopt->has_arg == required_argument)
                *p++ = ':';
              /* Don't handle optional_argument */
            }
        }
    }
  /* Terminate short_options. */
  *p = '\0';
  /* No need for xzero(long_options[o]) because its storage is static
     and it will be zeroed by default.  */
  assert (o <= countof (long_options));
}

/* Print the usage message.  */
static int
print_usage (int error)
{
  return fprintf (error ? stderr : stdout,
                  _("Usage: %s [OPTION]... [URL]...\n"), exec_name);
}

/* Print the help message, describing all the available options.  If
   you add an option, be sure to update this list.  */
_Noreturn static void
print_help (void)
{
  /* We split the help text this way to ease translation of individual
     entries.  */
  static const char *help[] = {
    "\n",
    N_("\
Mandatory arguments to long options are mandatory for short options too.\n\n"),
    N_("\
Startup:\n"),
    N_("\
  -V,  --version                   display the version of Wget and exit\n"),
    N_("\
  -h,  --help                      print this help\n"),
    N_("\
  -b,  --background                go to background after startup\n"),
    N_("\
  -e,  --execute=COMMAND           execute a `.wgetrc'-style command\n"),
    "\n",

    N_("\
Logging and input file:\n"),
    N_("\
  -o,  --output-file=FILE          log messages to FILE\n"),
    N_("\
  -a,  --append-output=FILE        append messages to FILE\n"),
#ifdef ENABLE_DEBUG
    N_("\
  -d,  --debug                     print lots of debugging information\n"),
#endif
#ifdef USE_WATT32
    N_("\
       --wdebug                    print Watt-32 debug output\n"),
#endif
    N_("\
  -q,  --quiet                     quiet (no output)\n"),
    N_("\
  -v,  --verbose                   be verbose (this is the default)\n"),
    N_("\
  -nv, --no-verbose                turn off verboseness, without being quiet\n"),
    N_("\
       --report-speed=TYPE         output bandwidth as TYPE.  TYPE can be bits\n"),
    N_("\
  -i,  --input-file=FILE           download URLs found in local or external FILE\n"),
#ifdef HAVE_METALINK
    N_("\
       --input-metalink=FILE       download files covered in local Metalink FILE\n"),
#endif
    N_("\
  -F,  --force-html                treat input file as HTML\n"),
    N_("\
  -B,  --base=URL                  resolves HTML input-file links (-i -F)\n\
                                     relative to URL\n"),
    N_("\
       --config=FILE               specify config file to use\n"),
    N_("\
       --no-config                 do not read any config file\n"),
    N_("\
       --rejected-log=FILE         log reasons for URL rejection to FILE\n"),
    "\n",

    N_("\
Download:\n"),
    N_("\
  -t,  --tries=NUMBER              set number of retries to NUMBER (0 unlimits)\n"),
    N_("\
       --retry-connrefused         retry even if connection is refused\n"),
    N_("\
  -O,  --output-document=FILE      write documents to FILE\n"),
    N_("\
  -nc, --no-clobber                skip downloads that would download to\n\
                                     existing files (overwriting them)\n"),
    N_("\
       --no-netrc                  don't try to obtain credentials from .netrc\n"),
    N_("\
  -c,  --continue                  resume getting a partially-downloaded file\n"),
    N_("\
       --start-pos=OFFSET          start downloading from zero-based position OFFSET\n"),
    N_("\
       --progress=TYPE             select progress gauge type\n"),
    N_("\
       --show-progress             display the progress bar in any verbosity mode\n"),
    N_("\
  -N,  --timestamping              don't re-retrieve files unless newer than\n\
                                     local\n"),
    N_("\
       --no-if-modified-since      don't use conditional if-modified-since get\n\
                                     requests in timestamping mode\n"),
    N_("\
       --no-use-server-timestamps  don't set the local file's timestamp by\n\
                                     the one on the server\n"),
    N_("\
  -S,  --server-response           print server response\n"),
    N_("\
       --spider                    don't download anything\n"),
    N_("\
  -T,  --timeout=SECONDS           set all timeout values to SECONDS\n"),
#ifdef HAVE_LIBCARES
    N_("\
       --dns-servers=ADDRESSES     list of DNS servers to query (comma separated)\n"),
    N_("\
       --bind-dns-address=ADDRESS  bind DNS resolver to ADDRESS (hostname or IP) on local host\n"),
#endif
    N_("\
       --dns-timeout=SECS          set the DNS lookup timeout to SECS\n"),
    N_("\
       --connect-timeout=SECS      set the connect timeout to SECS\n"),
    N_("\
       --read-timeout=SECS         set the read timeout to SECS\n"),
    N_("\
  -w,  --wait=SECONDS              wait SECONDS between retrievals\n"),
    N_("\
       --waitretry=SECONDS         wait 1..SECONDS between retries of a retrieval\n"),
    N_("\
       --random-wait               wait from 0.5*WAIT...1.5*WAIT secs between retrievals\n"),
    N_("\
       --no-proxy                  explicitly turn off proxy\n"),
    N_("\
  -Q,  --quota=NUMBER              set retrieval quota to NUMBER\n"),
    N_("\
       --bind-address=ADDRESS      bind to ADDRESS (hostname or IP) on local host\n"),
    N_("\
       --limit-rate=RATE           limit download rate to RATE\n"),
    N_("\
       --no-dns-cache              disable caching DNS lookups\n"),
    N_("\
       --restrict-file-names=OS    restrict chars in file names to ones OS allows\n"),
    N_("\
       --ignore-case               ignore case when matching files/directories\n"),
#ifdef ENABLE_IPV6
    N_("\
  -4,  --inet4-only                connect only to IPv4 addresses\n"),
    N_("\
  -6,  --inet6-only                connect only to IPv6 addresses\n"),
    N_("\
       --prefer-family=FAMILY      connect first to addresses of specified family,\n\
                                     one of IPv6, IPv4, or none\n"),
#endif
    N_("\
       --user=USER                 set both ftp and http user to USER\n"),
    N_("\
       --password=PASS             set both ftp and http password to PASS\n"),
    N_("\
       --ask-password              prompt for passwords\n"),
    N_("\
       --use-askpass=COMMAND       specify credential handler for requesting \n\
                                     username and password.  If no COMMAND is \n\
                                     specified the WGET_ASKPASS or the SSH_ASKPASS \n\
                                     environment variable is used.\n"),
    N_("\
       --no-iri                    turn off IRI support\n"),
    N_("\
       --local-encoding=ENC        use ENC as the local encoding for IRIs\n"),
    N_("\
       --remote-encoding=ENC       use ENC as the default remote encoding\n"),
    N_("\
       --unlink                    remove file before clobber\n"),
#ifdef HAVE_METALINK
    N_("\
       --keep-badhash              keep files with checksum mismatch (append .badhash)\n"),
    N_("\
       --metalink-index=NUMBER     Metalink application/metalink4+xml metaurl ordinal NUMBER\n"),
    N_("\
       --metalink-over-http        use Metalink metadata from HTTP response headers\n"),
    N_("\
       --preferred-location        preferred location for Metalink resources\n"),
#endif
#ifdef ENABLE_XATTR
    N_("\
       --no-xattr                  turn off storage of metadata in extended file attributes\n"),
#endif
    "\n",

    N_("\
Directories:\n"),
    N_("\
  -nd, --no-directories            don't create directories\n"),
    N_("\
  -x,  --force-directories         force creation of directories\n"),
    N_("\
  -nH, --no-host-directories       don't create host directories\n"),
    N_("\
       --protocol-directories      use protocol name in directories\n"),
    N_("\
  -P,  --directory-prefix=PREFIX   save files to PREFIX/..\n"),
    N_("\
       --cut-dirs=NUMBER           ignore NUMBER remote directory components\n"),
    "\n",

    N_("\
HTTP options:\n"),
    N_("\
       --http-user=USER            set http user to USER\n"),
    N_("\
       --http-password=PASS        set http password to PASS\n"),
    N_("\
       --no-cache                  disallow server-cached data\n"),
    N_ ("\
       --default-page=NAME         change the default page name (normally\n\
                                     this is 'index.html'.)\n"),
    N_("\
  -E,  --adjust-extension          save HTML/CSS documents with proper extensions\n"),
    N_("\
       --ignore-length             ignore 'Content-Length' header field\n"),
    N_("\
       --header=STRING             insert STRING among the headers\n"),
#ifdef HAVE_LIBZ
    N_("\
       --compression=TYPE          choose compression, one of auto, gzip and none\n"),
#endif
    N_("\
       --max-redirect              maximum redirections allowed per page\n"),
    N_("\
       --proxy-user=USER           set USER as proxy username\n"),
    N_("\
       --proxy-password=PASS       set PASS as proxy password\n"),
    N_("\
       --referer=URL               include 'Referer: URL' header in HTTP request\n"),
    N_("\
       --save-headers              save the HTTP headers to file\n"),
    N_("\
  -U,  --user-agent=AGENT          identify as AGENT instead of Wget/VERSION\n"),
    N_("\
       --no-http-keep-alive        disable HTTP keep-alive (persistent connections)\n"),
    N_("\
       --no-cookies                don't use cookies\n"),
    N_("\
       --load-cookies=FILE         load cookies from FILE before session\n"),
    N_("\
       --save-cookies=FILE         save cookies to FILE after session\n"),
    N_("\
       --keep-session-cookies      load and save session (non-permanent) cookies\n"),
    N_("\
       --post-data=STRING          use the POST method; send STRING as the data\n"),
    N_("\
       --post-file=FILE            use the POST method; send contents of FILE\n"),
    N_("\
       --method=HTTPMethod         use method \"HTTPMethod\" in the request\n"),
    N_("\
       --body-data=STRING          send STRING as data. --method MUST be set\n"),
    N_("\
       --body-file=FILE            send contents of FILE. --method MUST be set\n"),
    N_("\
       --content-disposition       honor the Content-Disposition header when\n\
                                     choosing local file names (EXPERIMENTAL)\n"),
    N_("\
       --content-on-error          output the received content on server errors\n"),
    N_("\
       --auth-no-challenge         send Basic HTTP authentication information\n\
                                     without first waiting for the server's\n\
                                     challenge\n"),
    "\n",

#ifdef HAVE_SSL
    N_("\
HTTPS (SSL/TLS) options:\n"),
    N_("\
       --secure-protocol=PR        choose secure protocol, one of auto, SSLv2,\n\
                                     SSLv3, TLSv1, TLSv1_1, TLSv1_2 and PFS\n"),
    N_("\
       --https-only                only follow secure HTTPS links\n"),
    N_("\
       --no-check-certificate      don't validate the server's certificate\n"),
    N_("\
       --certificate=FILE          client certificate file\n"),
    N_("\
       --certificate-type=TYPE     client certificate type, PEM or DER\n"),
    N_("\
       --private-key=FILE          private key file\n"),
    N_("\
       --private-key-type=TYPE     private key type, PEM or DER\n"),
    N_("\
       --ca-certificate=FILE       file with the bundle of CAs\n"),
    N_("\
       --ca-directory=DIR          directory where hash list of CAs is stored\n"),
    N_("\
       --crl-file=FILE             file with bundle of CRLs\n"),
    N_("\
       --pinnedpubkey=FILE/HASHES  Public key (PEM/DER) file, or any number\n\
                                   of base64 encoded sha256 hashes preceded by\n\
                                   \'sha256//\' and separated by \';\', to verify\n\
                                   peer against\n"),
#if defined(HAVE_LIBSSL) || defined(HAVE_LIBSSL32)
    N_("\
       --random-file=FILE          file with random data for seeding the SSL PRNG\n"),
#endif
#if (defined(HAVE_LIBSSL) || defined(HAVE_LIBSSL32)) && defined(HAVE_RAND_EGD)
    N_("\
       --egd-file=FILE             file naming the EGD socket with random data\n"),
#endif
    "\n",
#endif /* HAVE_SSL */

#ifdef HAVE_HSTS
    N_("\
HSTS options:\n"),
    N_("\
       --no-hsts                   disable HSTS\n"),
    N_("\
       --hsts-file                 path of HSTS database (will override default)\n"),
    "\n",
#endif

    N_("\
FTP options:\n"),
#ifdef __VMS
    N_("\
       --ftp-stmlf                 use Stream_LF format for all binary FTP files\n"),
#endif /* def __VMS */
    N_("\
       --ftp-user=USER             set ftp user to USER\n"),
    N_("\
       --ftp-password=PASS         set ftp password to PASS\n"),
    N_("\
       --no-remove-listing         don't remove '.listing' files\n"),
    N_("\
       --no-glob                   turn off FTP file name globbing\n"),
    N_("\
       --no-passive-ftp            disable the \"passive\" transfer mode\n"),
    N_("\
       --preserve-permissions      preserve remote file permissions\n"),
    N_("\
       --retr-symlinks             when recursing, get linked-to files (not dir)\n"),
    "\n",

#ifdef HAVE_SSL
    N_("\
FTPS options:\n"),
    N_("\
       --ftps-implicit                 use implicit FTPS (default port is 990)\n"),
    N_("\
       --ftps-resume-ssl               resume the SSL/TLS session started in the control connection when\n"
        "                                         opening a data connection\n"),
    N_("\
       --ftps-clear-data-connection    cipher the control channel only; all the data will be in plaintext\n"),
    N_("\
       --ftps-fallback-to-ftp          fall back to FTP if FTPS is not supported in the target server\n"),
#endif

    N_("\
WARC options:\n"),
    N_("\
       --warc-file=FILENAME        save request/response data to a .warc.gz file\n"),
    N_("\
       --warc-header=STRING        insert STRING into the warcinfo record\n"),
    N_("\
       --warc-max-size=NUMBER      set maximum size of WARC files to NUMBER\n"),
    N_("\
       --warc-cdx                  write CDX index files\n"),
    N_("\
       --warc-dedup=FILENAME       do not store records listed in this CDX file\n"),
#ifdef HAVE_LIBZ
    N_("\
       --no-warc-compression       do not compress WARC files with GZIP\n"),
#endif
    N_("\
       --no-warc-digests           do not calculate SHA1 digests\n"),
    N_("\
       --no-warc-keep-log          do not store the log file in a WARC record\n"),
    N_("\
       --warc-tempdir=DIRECTORY    location for temporary files created by the\n\
                                     WARC writer\n"),
    "\n",

    N_("\
Recursive download:\n"),
    N_("\
  -r,  --recursive                 specify recursive download\n"),
    N_("\
  -l,  --level=NUMBER              maximum recursion depth (inf or 0 for infinite)\n"),
    N_("\
       --delete-after              delete files locally after downloading them\n"),
    N_("\
  -k,  --convert-links             make links in downloaded HTML or CSS point to\n\
                                     local files\n"),
    N_("\
       --convert-file-only         convert the file part of the URLs only (usually known as the basename)\n"),
    N_("\
       --backups=N                 before writing file X, rotate up to N backup files\n"),

#ifdef __VMS
    N_("\
  -K,  --backup-converted          before converting file X, back up as X_orig\n"),
#else /* def __VMS */
    N_("\
  -K,  --backup-converted          before converting file X, back up as X.orig\n"),
#endif /* def __VMS [else] */
    N_("\
  -m,  --mirror                    shortcut for -N -r -l inf --no-remove-listing\n"),
    N_("\
  -p,  --page-requisites           get all images, etc. needed to display HTML page\n"),
    N_("\
       --strict-comments           turn on strict (SGML) handling of HTML comments\n"),
    "\n",

    N_("\
Recursive accept/reject:\n"),
    N_("\
  -A,  --accept=LIST               comma-separated list of accepted extensions\n"),
    N_("\
  -R,  --reject=LIST               comma-separated list of rejected extensions\n"),
    N_("\
       --accept-regex=REGEX        regex matching accepted URLs\n"),
    N_("\
       --reject-regex=REGEX        regex matching rejected URLs\n"),
#ifdef HAVE_LIBPCRE
    N_("\
       --regex-type=TYPE           regex type (posix|pcre)\n"),
#else
    N_("\
       --regex-type=TYPE           regex type (posix)\n"),
#endif
    N_("\
  -D,  --domains=LIST              comma-separated list of accepted domains\n"),
    N_("\
       --exclude-domains=LIST      comma-separated list of rejected domains\n"),
    N_("\
       --follow-ftp                follow FTP links from HTML documents\n"),
    N_("\
       --follow-tags=LIST          comma-separated list of followed HTML tags\n"),
    N_("\
       --ignore-tags=LIST          comma-separated list of ignored HTML tags\n"),
    N_("\
  -H,  --span-hosts                go to foreign hosts when recursive\n"),
    N_("\
  -L,  --relative                  follow relative links only\n"),
    N_("\
  -I,  --include-directories=LIST  list of allowed directories\n"),
    N_("\
       --trust-server-names        use the name specified by the redirection\n\
                                     URL's last component\n"),
    N_("\
  -X,  --exclude-directories=LIST  list of excluded directories\n"),
    N_("\
  -np, --no-parent                 don't ascend to the parent directory\n"),
    "\n",
    N_("Mail bug reports and suggestions to <bug-wget@gnu.org>\n")
  };

  size_t i;

  if (printf (_("GNU Wget %s, a non-interactive network retriever.\n"),
              version_string) < 0)
    exit (WGET_EXIT_IO_FAIL);
  if (print_usage (0) < 0)
    exit (WGET_EXIT_IO_FAIL);

  for (i = 0; i < countof (help); i++)
    if (fputs (_(help[i]), stdout) < 0)
      exit (WGET_EXIT_IO_FAIL);

  exit (WGET_EXIT_SUCCESS);
}

/* Return a human-readable printed representation of INTERVAL,
   measured in seconds.  */

static char *
secs_to_human_time (double interval)
{
  static char buf[32];
  int secs = (int) (interval + 0.5);
  int hours, mins, days;

  days = secs / 86400, secs %= 86400;
  hours = secs / 3600, secs %= 3600;
  mins = secs / 60, secs %= 60;

  if (days)
    sprintf (buf, "%dd %dh %dm %ds", days, hours, mins, secs);
  else if (hours)
    sprintf (buf, "%dh %dm %ds", hours, mins, secs);
  else if (mins)
    sprintf (buf, "%dm %ds", mins, secs);
  else
    sprintf (buf, "%ss", print_decimal (interval));

  return buf;
}

static char *
prompt_for_password (void)
{
  if (opt.user)
    fprintf (stderr, _("Password for user %s: "), quote (opt.user));
  else
    fprintf (stderr, _("Password: "));
  return getpass("");
}


/* Execute external application opt.use_askpass */
static void
run_use_askpass (char *question, char **answer)
{
  char tmp[1024];
  pid_t pid;
  int status;
  int com[2];
  ssize_t bytes = 0;
  char *argv[3], *p;
  posix_spawn_file_actions_t fa;

  if (pipe (com) == -1)
    {
      fprintf (stderr, _("Cannot create pipe\n"));
      exit (WGET_EXIT_GENERIC_ERROR);
    }

  status = posix_spawn_file_actions_init (&fa);
  if (status)
    {
      fprintf (stderr,
              _("Error initializing spawn file actions for use-askpass: %d\n"),
              status);
      exit (WGET_EXIT_GENERIC_ERROR);
    }

  status = posix_spawn_file_actions_adddup2 (&fa, com[1], STDOUT_FILENO);
  if (status)
    {
      fprintf (stderr,
              _("Error setting spawn file actions for use-askpass: %d\n"),
              status);
      exit (WGET_EXIT_GENERIC_ERROR);
    }

  /* C89 initializer lists must be computable at load time,
   * thus this explicit initialization. */
  argv[0] = opt.use_askpass;
  argv[1] = question;
  argv[2] = NULL;

  status = posix_spawnp (&pid, opt.use_askpass, &fa, NULL, argv, environ);
  if (status)
    {
      fprintf (stderr, "Error spawning %s: %d\n", opt.use_askpass, status);
      exit (WGET_EXIT_GENERIC_ERROR);
    }

  /* Parent process reads from child. */
  close (com[1]);
  bytes = read (com[0], tmp, sizeof (tmp) - 1);
  if (bytes <= 0)
    {
      fprintf (stderr,
              _("Error reading response from command \"%s %s\": %s\n"),
              opt.use_askpass, question, strerror (errno));
      exit (WGET_EXIT_GENERIC_ERROR);
    }

  /* Make sure there is a trailing 0 */
  tmp[bytes] = '\0';

  /* Remove a possible new line */
  if ((p = strpbrk (tmp, "\r\n")))
    bytes = p - tmp;

  *answer = xmemdup0 (tmp, bytes);
}

/* set the user name and password*/
static void
use_askpass (struct url *u)
{
  static char question[1024];

  if (u->user == NULL || u->user[0] == '\0')
    {
      snprintf (question, sizeof (question),  _("Username for '%s%s': "),
                scheme_leading_string(u->scheme), u->host);
      /* Prompt for username */
      run_use_askpass (question, &u->user);
      if (opt.recursive)
        opt.user = xstrdup (u->user);
    }

  if (u->passwd == NULL || u->passwd[0] == '\0')
    {
      snprintf(question, sizeof (question), _("Password for '%s%s@%s': "),
               scheme_leading_string (u->scheme), u->user, u->host);
      /* Prompt for password */
      run_use_askpass (question, &u->passwd);
      if (opt.recursive)
        opt.passwd = xstrdup (u->passwd);
    }
}
/* Function that prints the line argument while limiting it
   to at most line_length. prefix is printed on the first line
   and an appropriate number of spaces are added on subsequent
   lines.*/
static int
format_and_print_line (const char *prefix, const char *line,
                       int line_length)
{
  int remaining_chars;
  char *line_dup, *token;

  assert (prefix != NULL);
  assert (line != NULL);
  assert (line_length > TABULATION);

  line_dup = xstrdup (line);

  if (printf ("%s", prefix) < 0)
    {
      xfree (line_dup);
      return -1;
    }

  /* Wrap to new line after prefix. */
  remaining_chars = 0;

  /* We break on spaces. */
  token = strtok (line_dup, " ");
  while (token != NULL)
    {
      /* If however a token is much larger than the maximum
         line length, all bets are off and we simply print the
         token on the next line. */
      if (remaining_chars <= (int) strlen (token))
        {
          if (printf ("\n%*c", TABULATION, ' ') < 0)
            {
              xfree (line_dup);
              return -1;
            }
          remaining_chars = line_length - TABULATION;
        }
      if (printf ("%s ", token) < 0)
        {
          xfree (line_dup);
          return -1;
        }
      remaining_chars -= strlen (token) + 1;  /* account for " " */
      token = strtok (NULL, " ");
    }

  if (printf ("\n") < 0)
    {
      xfree (line_dup);
      return -1;
    }

  xfree (line_dup);
  return 0;
}

_Noreturn static void
print_version (void)
{
  const char *wgetrc_title  = _("Wgetrc: ");
  const char *locale_title  = _("Locale: ");
  const char *compile_title = _("Compile: ");
  const char *link_title    = _("Link: ");
  char *env_wgetrc, *user_wgetrc;
  int i;

  if (printf (_("GNU Wget %s built on %s.\n\n"), version_string, OS_TYPE) < 0)
    exit (WGET_EXIT_IO_FAIL);

  for (i = 0; compiled_features[i] != NULL; )
    {
      int line_length = MAX_CHARS_PER_LINE;
      while ((line_length > 0) && (compiled_features[i] != NULL))
        {
          if (printf ("%s ", compiled_features[i]) < 0)
            exit (WGET_EXIT_IO_FAIL);
          line_length -= strlen (compiled_features[i]) + 2;
          i++;
        }
      if (printf ("\n") < 0)
        exit (WGET_EXIT_IO_FAIL);
    }
  if (printf ("\n") < 0)
    exit (WGET_EXIT_IO_FAIL);

  /* Handle the case when $WGETRC is unset and $HOME/.wgetrc is
     absent. */
  if (printf ("%s\n", wgetrc_title) < 0)
    exit (WGET_EXIT_IO_FAIL);

  env_wgetrc = wgetrc_env_file_name ();
  if (env_wgetrc && *env_wgetrc)
    {
      if (printf (_("    %s (env)\n"), env_wgetrc) < 0)
        exit (WGET_EXIT_IO_FAIL);
      xfree (env_wgetrc);
    }
  user_wgetrc = wgetrc_user_file_name ();
  if (user_wgetrc)
    {
      if (printf (_("    %s (user)\n"), user_wgetrc) < 0)
        exit (WGET_EXIT_IO_FAIL);
      xfree (user_wgetrc);
    }
#ifdef SYSTEM_WGETRC
  if (printf (_("    %s (system)\n"), SYSTEM_WGETRC) < 0)
    exit (WGET_EXIT_IO_FAIL);
#endif

#ifdef ENABLE_NLS
  if (format_and_print_line (locale_title,
                             LOCALEDIR,
                             MAX_CHARS_PER_LINE) < 0)
    exit (WGET_EXIT_IO_FAIL);
#endif /* def ENABLE_NLS */

  if (compilation_string != NULL)
    if (format_and_print_line (compile_title,
                               compilation_string,
                               MAX_CHARS_PER_LINE) < 0)
      exit (WGET_EXIT_IO_FAIL);

  if (link_string != NULL)
    if (format_and_print_line (link_title,
                               link_string,
                               MAX_CHARS_PER_LINE) < 0)
      exit (WGET_EXIT_IO_FAIL);

  if (printf ("\n") < 0)
    exit (WGET_EXIT_IO_FAIL);

  /* TRANSLATORS: When available, an actual copyright character
     (circle-c) should be used in preference to "(C)". */
  if (printf (_("\
Copyright (C) %s Free Software Foundation, Inc.\n"), "2015") < 0)
    exit (WGET_EXIT_IO_FAIL);
  if (fputs (_("\
License GPLv3+: GNU GPL version 3 or later\n\
<http://www.gnu.org/licenses/gpl.html>.\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n"), stdout) < 0)
    exit (WGET_EXIT_IO_FAIL);
  /* TRANSLATORS: When available, please use the proper diacritics for
     names such as this one. See en_US.po for reference. */
  if (fputs (_("\nOriginally written by Hrvoje Niksic <hniksic@xemacs.org>.\n"),
             stdout) < 0)
    exit (WGET_EXIT_IO_FAIL);
  if (fputs (_("Please send bug reports and questions to <bug-wget@gnu.org>.\n"),
             stdout) < 0)
    exit (WGET_EXIT_IO_FAIL);

  exit (WGET_EXIT_SUCCESS);
}

const char *program_name; /* Needed by lib/error.c. */
const char *program_argstring; /* Needed by wget_warc.c. */

int
main (int argc, char **argv)
{
  char **url, **t, *p;
  int i, ret, longindex;
  int nurl;
  int retconf;
  int argstring_length;
  bool use_userconfig = false;
  bool noconfig = false;
  bool append_to_log = false;

  struct ptimer *timer = ptimer_new ();
  double start_time = ptimer_measure (timer);

  total_downloaded_bytes = 0;
  program_name = argv[0];

  i18n_initialize ();

  /* Construct the name of the executable, without the directory part.  */
#ifdef __VMS
  /* On VMS, lose the "dev:[dir]" prefix and the ".EXE;nnn" suffix. */
  exec_name = vms_basename (argv[0]);
#else /* def __VMS */
  exec_name = base_name (argv[0]);
#endif /* def __VMS [else] */

#ifdef WINDOWS
  /* Drop extension (typically .EXE) from executable filename. */
  windows_main ((char **) &exec_name);
#endif

  /* Construct the arguments string. */
  for (argstring_length = 1, i = 1; i < argc; i++)
    argstring_length += strlen (argv[i]) + 3 + 1;
  program_argstring = p = malloc (argstring_length);
  if (p == NULL)
    {
      fprintf (stderr, _("Memory allocation problem\n"));
      exit (WGET_EXIT_PARSE_ERROR);
    }
  for (i = 1; i < argc; i++)
    {
      int arglen;

      *p++ = '"';
      arglen = strlen (argv[i]);
      memcpy (p, argv[i], arglen);
      p += arglen;
      *p++ = '"';
      *p++ = ' ';
    }
  *p = '\0';

  /* Load the hard-coded defaults.  */
  defaults ();

  init_switches ();

  /* This separate getopt_long is needed to find the user config file
     option ("--config") and parse it before the other user options. */
  longindex = -1;

  while ((retconf = getopt_long (argc, argv,
                                short_options, long_options, &longindex)) != -1)
    {
      int confval;
      struct cmdline_option *config_opt;

      /* There is no short option for "--config". */
      if (longindex >= 0)
        {
          confval = long_options[longindex].val;
          config_opt = &option_data[confval & ~BOOLEAN_NEG_MARKER];
          if (strcmp (config_opt->long_name, "no-config") == 0)
            {
              noconfig = true;
              break;
            }
          else if (strcmp (config_opt->long_name, "config") == 0)
            {
              file_stats_t flstats;
              use_userconfig = true;
              memset(&flstats, 0, sizeof(flstats));
              if (file_exists_p(optarg, &flstats) && run_wgetrc (optarg, &flstats))
                break;
              else
                {
                  fprintf (stderr, _("Exiting due to error in %s\n"), optarg);
                  exit (WGET_EXIT_PARSE_ERROR);
                }
            }
        }
    }

  /* If the user did not specify a config, read the system wgetrc and ~/.wgetrc. */
  if (noconfig == false && use_userconfig == false)
    initialize ();

  opterr = 0;
  optind = 0;

  longindex = -1;
  while ((ret = getopt_long (argc, argv,
                             short_options, long_options, &longindex)) != -1)
    {
      int val;
      struct cmdline_option *cmdopt;

      /* If LONGINDEX is unchanged, it means RET is referring a short
         option.  */
      if (longindex == -1)
        {
          if (ret == '?')
            {
              print_usage (1);
              fprintf (stderr, "\n");
              fprintf (stderr, _("Try `%s --help' for more options.\n"),
                       exec_name);
              exit (WGET_EXIT_PARSE_ERROR);
            }
          /* Find the short option character in the mapping.  */
          longindex = optmap[ret - 32];
        }
      val = long_options[longindex].val;

      /* Use the retrieved value to locate the option in the
         option_data array, and to see if we're dealing with the
         negated "--no-FOO" variant of the boolean option "--foo".  */
      cmdopt = &option_data[val & ~BOOLEAN_NEG_MARKER];
      switch (cmdopt->type)
        {
        case OPT_VALUE:
          setoptval (cmdopt->data, optarg, cmdopt->long_name);
          break;
        case OPT_BOOLEAN:
          if (optarg)
            /* The user has specified a value -- use it. */
            setoptval (cmdopt->data, optarg, cmdopt->long_name);
          else
            {
              /* NEG is true for `--no-FOO' style boolean options. */
              bool neg = !!(val & BOOLEAN_NEG_MARKER);
              setoptval (cmdopt->data, neg ? "0" : "1", cmdopt->long_name);
            }
          break;
        case OPT_FUNCALL:
          {
            void (*func) (void) = (void (*) (void)) cmdopt->data;
            func ();
          }
          break;
        case OPT__APPEND_OUTPUT:
          setoptval ("logfile", optarg, cmdopt->long_name);
          append_to_log = true;
          break;
        case OPT__EXECUTE:
          if (optarg) /* check silences static analyzer */
            run_command (optarg);
          break;
        case OPT__NO:
          {
            /* We support real --no-FOO flags now, but keep these
               short options for convenience and backward
               compatibility.  */
            for (p = optarg; p && *p; p++)
              switch (*p)
                {
                case 'v':
                  setoptval ("verbose", "0", cmdopt->long_name);
                  break;
                case 'H':
                  setoptval ("addhostdir", "0", cmdopt->long_name);
                  break;
                case 'd':
                  setoptval ("dirstruct", "0", cmdopt->long_name);
                  break;
                case 'c':
                  setoptval ("noclobber", "1", cmdopt->long_name);
                  break;
                case 'p':
                  setoptval ("noparent", "1", cmdopt->long_name);
                  break;
                default:
                  fprintf (stderr, _("%s: illegal option -- `-n%c'\n"),
                           exec_name, *p);
                  print_usage (1);
                  fprintf (stderr, "\n");
                  fprintf (stderr, _("Try `%s --help' for more options.\n"),
                           exec_name);
                  exit (WGET_EXIT_GENERIC_ERROR);
                }
            break;
          }
        case OPT__PARENT:
        case OPT__CLOBBER:
          {
            /* The wgetrc commands are named noparent and noclobber,
               so we must revert the meaning of the cmdline options
               before passing the value to setoptval.  */
            bool flag = true;
            if (optarg)
              flag = (*optarg == '1' || c_tolower (*optarg) == 'y'
                      || (c_tolower (optarg[0]) == 'o'
                          && c_tolower (optarg[1]) == 'n'));
            setoptval (cmdopt->type == OPT__PARENT ? "noparent" : "noclobber",
                       flag ? "0" : "1", cmdopt->long_name);
            break;
          }
        case OPT__DONT_REMOVE_LISTING:
          setoptval ("removelisting", "0", cmdopt->long_name);
          break;
        }

      longindex = -1;
    }

  nurl = argc - optind;

  /* If we do not have Debug support compiled in AND Wget is invoked with the
   * --debug switch, instead of failing, we silently turn it into a no-op. For
   *  this no-op, we explicitly set opt.debug to false and hence none of the
   *  Debug output messages will be printed.
   */
#ifndef ENABLE_DEBUG
  if (opt.debug)
    {
      fprintf (stderr, _("Debugging support not compiled in. "
                         "Ignoring --debug flag.\n"));
      opt.debug = false;
    }
#endif

  /* All user options have now been processed, so it's now safe to do
     interoption dependency checks. */

  if (opt.noclobber && (opt.convert_links || opt.convert_file_only))
    {
      fprintf (stderr,
               opt.convert_links ?
                   _("Both --no-clobber and --convert-links were specified,"
                     " only --convert-links will be used.\n") :
                    _("Both --no-clobber and --convert-file-only were specified,"
                      " only --convert-file-only will be used.\n"));
      opt.noclobber = false;
    }

  if (opt.reclevel == 0)
      opt.reclevel = INFINITE_RECURSION; /* see recur.h for commentary */

  if (opt.spider || opt.delete_after)
      opt.no_dirstruct = true;

  if (opt.page_requisites && !opt.recursive)
    {
      /* Don't set opt.recursive here because it would confuse the FTP
         code.  Instead, call retrieve_tree below when either
         page_requisites or recursive is requested.  */
      opt.reclevel = 0;
      if (!opt.no_dirstruct)
        opt.dirstruct = 1;      /* normally handled by cmd_spec_recursive() */
    }

  if (opt.verbose == -1)
    opt.verbose = !opt.quiet;

  if (!opt.verbose && opt.show_progress == -1)
    opt.show_progress = false;

  if (opt.quiet && opt.show_progress == -1)
    opt.show_progress = false;

  /* Sanity checks.  */
  if (opt.verbose && opt.quiet)
    {
      fprintf (stderr, _("Can't be verbose and quiet at the same time.\n"));
      print_usage (1);
      exit (WGET_EXIT_GENERIC_ERROR);
    }
  if (opt.timestamping && opt.noclobber)
    {
      fprintf (stderr, _("\
Can't timestamp and not clobber old files at the same time.\n"));
      print_usage (1);
      exit (WGET_EXIT_GENERIC_ERROR);
    }
#ifdef ENABLE_IPV6
  if (opt.ipv4_only && opt.ipv6_only)
    {
      fprintf (stderr,
               _("Cannot specify both --inet4-only and --inet6-only.\n"));
      print_usage (1);
      exit (WGET_EXIT_GENERIC_ERROR);
    }
#endif
  if (opt.output_document)
    {
      if ((opt.convert_links || opt.convert_file_only)
          && (nurl > 1 || opt.page_requisites || opt.recursive))
        {
          fputs (_("\
Cannot specify both -k or --convert-file-only and -O if multiple URLs are given, or in combination\n\
with -p or -r. See the manual for details.\n\n"), stderr);
          print_usage (1);
          exit (WGET_EXIT_GENERIC_ERROR);
        }
      if (opt.page_requisites
          || opt.recursive)
        {
          logprintf (LOG_NOTQUIET, "%s", _("\
WARNING: combining -O with -r or -p will mean that all downloaded content\n\
will be placed in the single file you specified.\n\n"));
        }
      if (opt.timestamping)
        {
          logprintf (LOG_NOTQUIET, "%s", _("\
WARNING: timestamping does nothing in combination with -O. See the manual\n\
for details.\n\n"));
          opt.timestamping = false;
        }
      if (opt.noclobber && file_exists_p(opt.output_document, NULL))
           {
              /* Check if output file exists; if it does, exit. */
              logprintf (LOG_VERBOSE,
                         _("File `%s' already there; not retrieving.\n"),
                         opt.output_document);
              exit (WGET_EXIT_GENERIC_ERROR);
           }
    }

  if (opt.warc_filename != 0)
    {
      if (opt.noclobber)
        {
          fprintf (stderr,
                   _("WARC output does not work with --no-clobber, "
                     "--no-clobber will be disabled.\n"));
          opt.noclobber = false;
        }
      if (opt.timestamping)
        {
          fprintf (stderr,
                   _("WARC output does not work with timestamping, "
                     "timestamping will be disabled.\n"));
          opt.timestamping = false;
        }
      if (opt.spider)
        {
          fprintf (stderr,
                   _("WARC output does not work with --spider.\n"));
          exit (WGET_EXIT_GENERIC_ERROR);
        }
      if (opt.always_rest || opt.start_pos >= 0)
        {
          fprintf (stderr,
                   _("WARC output does not work with --continue or"
                     " --start-pos, they will be disabled.\n"));
          opt.always_rest = false;
          opt.start_pos = -1;
        }
      if (opt.warc_cdx_dedup_filename != 0 && !opt.warc_digests_enabled)
        {
          fprintf (stderr,
                   _("Digests are disabled; WARC deduplication will "
                     "not find duplicate records.\n"));
        }
      if (opt.warc_keep_log)
        {
          opt.progress_type = xstrdup ("dot");
        }
    }

#ifdef HAVE_LIBZ
  if (opt.always_rest || opt.start_pos >= 0)
    {
      if (opt.compression == compression_auto)
        {
          /* Compression does not work with --continue or --start-pos.
             Since compression was not explicitly set, it will be disabled. */
          opt.compression = compression_none;
        }
      else if (opt.compression != compression_none)
        {
          fprintf (stderr,
                   _("Compression does not work with --continue or"
                     " --start-pos, they will be disabled.\n"));
          opt.always_rest = false;
          opt.start_pos = -1;
        }
    }
#endif

  if (opt.ask_passwd && opt.passwd)
    {
      fprintf (stderr,
               _("Cannot specify both --ask-password and --password.\n"));
      print_usage (1);
      exit (WGET_EXIT_GENERIC_ERROR);
    }

  if (opt.start_pos >= 0 && opt.always_rest)
    {
      fprintf (stderr,
               _("Specifying both --start-pos and --continue is not "
                 "recommended; --continue will be disabled.\n"));
      opt.always_rest = false;
    }

  if (!nurl && !opt.input_filename
#ifdef HAVE_METALINK
      && !opt.input_metalink
#endif
      )
    {
      /* No URL specified.  */
      fprintf (stderr, _("%s: missing URL\n"), exec_name);
      print_usage (1);
      fprintf (stderr, "\n");
      /* #### Something nicer should be printed here -- similar to the
         pre-1.5 `--help' page.  */
      fprintf (stderr, _("Try `%s --help' for more options.\n"), exec_name);
      exit (WGET_EXIT_GENERIC_ERROR);
    }

  /* Compile the regular expressions.  */
  switch (opt.regex_type)
    {
#ifdef HAVE_LIBPCRE
      case regex_type_pcre:
        opt.regex_compile_fun = compile_pcre_regex;
        opt.regex_match_fun = match_pcre_regex;
        break;
#endif

      case regex_type_posix:
      default:
        opt.regex_compile_fun = compile_posix_regex;
        opt.regex_match_fun = match_posix_regex;
        break;
    }
  if (opt.acceptregex_s)
    {
      opt.acceptregex = opt.regex_compile_fun (opt.acceptregex_s);
      if (!opt.acceptregex)
        exit (WGET_EXIT_GENERIC_ERROR);
    }
  if (opt.rejectregex_s)
    {
      opt.rejectregex = opt.regex_compile_fun (opt.rejectregex_s);
      if (!opt.rejectregex)
        exit (WGET_EXIT_GENERIC_ERROR);
    }
  if (opt.post_data || opt.post_file_name)
    {
      if (opt.post_data && opt.post_file_name)
        {
          fprintf (stderr, _("You cannot specify both --post-data and --post-file.\n"));
          exit (WGET_EXIT_GENERIC_ERROR);
        }
      else if (opt.method)
        {
          fprintf (stderr, _("You cannot use --post-data or --post-file along with --method. "
                             "--method expects data through --body-data and --body-file options\n"));
          exit (WGET_EXIT_GENERIC_ERROR);
        }
    }
  if (opt.body_data || opt.body_file)
    {
      if (!opt.method)
        {
          fprintf (stderr, _("You must specify a method through --method=HTTPMethod "
                              "to use with --body-data or --body-file.\n"));
          exit (WGET_EXIT_GENERIC_ERROR);
        }
      else if (opt.body_data && opt.body_file)
        {
          fprintf (stderr, _("You cannot specify both --body-data and --body-file.\n"));
          exit (WGET_EXIT_GENERIC_ERROR);
        }
    }

  /* Set various options as required for opt.method.  */

  /* When user specifies HEAD as the method, we do not wish to download any
     files. Hence, set wget to run in spider mode.  */
  if (opt.method && c_strcasecmp (opt.method, "HEAD") == 0)
    setoptval ("spider", "1", "spider");

  /* Convert post_data to body-data and post_file_name to body-file options.
     This is required so as to remove redundant code later on in gethttp().
     The --post-data and --post-file options may also be removed in
     the future hence it makes sense to convert them to aliases for
     the more generic --method options.
     This MUST occur only after the sanity checks so as to prevent the
     user from setting both post and body options simultaneously.
  */
  if (opt.post_data || opt.post_file_name)
    {
        setoptval ("method", "POST", "method");
        if (opt.post_data)
          {
            setoptval ("bodydata", opt.post_data, "body-data");
            xfree(opt.post_data);
          }
        else
          {
            setoptval ("bodyfile", opt.post_file_name, "body-file");
            xfree(opt.post_file_name);
          }
    }

#ifdef ENABLE_IRI
  if (opt.enable_iri)
    {
      if (opt.locale && !check_encoding_name (opt.locale))
        opt.locale = NULL;

      if (!opt.locale)
        opt.locale = find_locale ();

      if (opt.encoding_remote && !check_encoding_name (opt.encoding_remote))
        opt.encoding_remote = NULL;
    }
#else
  memset (&dummy_iri, 0, sizeof (dummy_iri));
  if (opt.enable_iri || opt.locale || opt.encoding_remote)
    {
      /* sXXXav : be more specific... */
      fprintf (stderr, _("This version does not have support for IRIs\n"));
      exit (WGET_EXIT_GENERIC_ERROR);
    }
#endif

  if (opt.ask_passwd)
    {
      opt.passwd = prompt_for_password ();

      if (opt.passwd == NULL || opt.passwd[0] == '\0')
        exit (WGET_EXIT_GENERIC_ERROR);
    }

  if (opt.use_askpass)
  {
    if (opt.use_askpass[0] == '\0')
      {
        fprintf (stderr,
                 _("use-askpass requires a string or either environment variable WGET_ASKPASS or SSH_ASKPASS to be set.\n"));
        exit (WGET_EXIT_GENERIC_ERROR);
      }
  }

#ifdef USE_WATT32
  if (opt.wdebug)
     dbug_init();
  sock_init();
#else
  if (opt.background)
    fork_to_background ();
#endif

  /* Initialize progress.  Have to do this after the options are
     processed so we know where the log file is.  */
  if (opt.show_progress)
    set_progress_implementation (opt.progress_type);

  /* Fill in the arguments.  */
  url = alloca_array (char *, nurl + 1);
  if (url == NULL)
    {
      fprintf (stderr, _("Memory allocation problem\n"));
      exit (WGET_EXIT_PARSE_ERROR);
    }
  for (i = 0; i < nurl; i++, optind++)
    {
      char *rewritten = rewrite_shorthand_url (argv[optind]);
      if (rewritten)
        url[i] = rewritten;
      else
        url[i] = xstrdup (argv[optind]);
    }
  url[i] = NULL;

  /* Initialize logging.  */
  log_init (opt.lfilename, append_to_log);

  /* Open WARC file. */
  if (opt.warc_filename != 0)
    warc_init ();

  DEBUGP (("DEBUG output created by Wget %s on %s.\n\n",
           version_string, OS_TYPE));

  /* Open the output filename if necessary.  */

/* 2005-04-17 SMS.
   Note that having the output_stream ("-O") file opened here for an FTP
   URL rather than in getftp() (ftp.c) (and the http equivalent) rather
   limits the ability in VMS to open the file differently for ASCII
   versus binary FTP there.  (Of course, doing it here allows a open
   failure to be detected immediately, without first connecting to the
   server.)
*/
  if (opt.output_document)
    {
      if (HYPHENP (opt.output_document))
        {
#ifdef WINDOWS
          _setmode (_fileno (stdout), _O_BINARY);
#endif
          output_stream = stdout;
        }
      else
        {
          struct stat st;

#ifdef __VMS
/* Common fopen() optional arguments:
   sequential access only, access callback function.
*/
# define FOPEN_OPT_ARGS , "fop=sqo", "acc", acc_cb, &open_id
          int open_id = 7;
#else /* def __VMS */
# define FOPEN_OPT_ARGS
#endif /* def __VMS [else] */

          output_stream = fopen (opt.output_document,
                                 opt.always_rest ? "ab" : "wb"
                                 FOPEN_OPT_ARGS);
          if (output_stream == NULL)
            {
              perror (opt.output_document);
              exit (WGET_EXIT_GENERIC_ERROR);
            }
          if (fstat (fileno (output_stream), &st) == 0 && S_ISREG (st.st_mode))
            output_stream_regular = true;
        }
      if (!output_stream_regular && (opt.convert_links || opt.recursive))
        {
          fprintf (stderr, _("-k or -r can be used together with -O only if \
outputting to a regular file.\n"));
          exit (WGET_EXIT_GENERIC_ERROR);
        }
      if (!output_stream_regular && (opt.convert_links || opt.convert_file_only))
        {
          fprintf (stderr, _("--convert-links or --convert-file-only can be used together \
only if outputting to a regular file.\n"));
          exit (WGET_EXIT_GENERIC_ERROR);
        }
    }

#ifdef HAVE_LIBCARES
  if (opt.bind_dns_address || opt.dns_servers)
    {
      if (ares_library_init (ARES_LIB_INIT_ALL))
        {
          fprintf (stderr, _("Failed to init libcares\n"));
          exit (WGET_EXIT_GENERIC_ERROR);
        }

      if (ares_init (&ares) != ARES_SUCCESS)
        {
          fprintf (stderr, _("Failed to init c-ares channel\n"));
          exit (WGET_EXIT_GENERIC_ERROR);
        }

      if (opt.bind_dns_address)
        {
          struct in_addr a4;
#ifdef ENABLE_IPV6
          struct in6_addr a6;
#endif

          if (inet_pton (AF_INET, opt.bind_dns_address, &a4) == 1)
            {
              ares_set_local_ip4 (ares, ntohl (a4.s_addr));
            }
#ifdef ENABLE_IPV6
          else if (inet_pton (AF_INET6, opt.bind_dns_address, &a6) == 1)
            {
              ares_set_local_ip6 (ares, (unsigned char *) &a6);
            }
#endif
          else
            {
              fprintf (stderr, _("Failed to parse IP address '%s'\n"), opt.bind_dns_address);
              exit (WGET_EXIT_GENERIC_ERROR);
            }
        }

      if (opt.dns_servers)
        {
          int result;

          if ((result = ares_set_servers_csv (ares, opt.dns_servers)) != ARES_SUCCESS)
            {
              fprintf (stderr, _("Failed to set DNS server(s) '%s' (%d)\n"), opt.dns_servers, result);
              exit (WGET_EXIT_GENERIC_ERROR);
            }
        }
    }
#endif

#ifdef __VMS
  /* Set global ODS5 flag according to the specified destination (if
     any), otherwise according to the current default device.
  */
  if (output_stream == NULL)
    set_ods5_dest( "SYS$DISK");
  else if (output_stream != stdout)
    set_ods5_dest( opt.output_document);
#endif /* def __VMS */

#ifdef WINDOWS
  ws_startup ();
#endif

#ifdef SIGHUP
  /* Setup the signal handler to redirect output when hangup is
     received.  */
  if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
    signal(SIGHUP, redirect_output_signal);
#endif
  /* ...and do the same for SIGUSR1.  */
#ifdef SIGUSR1
  signal (SIGUSR1, redirect_output_signal);
#endif
#ifdef SIGPIPE
  /* Writing to a closed socket normally signals SIGPIPE, and the
     process exits.  What we want is to ignore SIGPIPE and just check
     for the return value of write().  */
  signal (SIGPIPE, SIG_IGN);
#endif
#ifdef SIGWINCH
  signal (SIGWINCH, progress_handle_sigwinch);
#endif

#ifdef HAVE_HSTS
  /* Load the HSTS database.
     Maybe all the URLs are FTP(S), in which case HSTS would not be needed,
     but this is the best place to do it, and it shouldn't be a critical
     performance hit.
   */
  if (opt.hsts)
    load_hsts ();
#endif

  /* Retrieve the URLs from argument list.  */
  for (t = url; *t; t++)
    {
      char *filename = NULL, *redirected_URL = NULL;
      int dt, url_err;
      /* Need to do a new struct iri every time, because
       * retrieve_url may modify it in some circumstances,
       * currently. */
      struct iri *iri = iri_new ();
      struct url *url_parsed;

      set_uri_encoding (iri, opt.locale, true);
      url_parsed = url_parse (*t, &url_err, iri, true);

      if (!url_parsed)
        {
          char *error = url_error (*t, url_err);
          logprintf (LOG_NOTQUIET, "%s: %s.\n",*t, error);
          xfree (error);
          inform_exit_status (URLERROR);
        }
      else
        {
          /* Request credentials if use_askpass is set. */
          if (opt.use_askpass)
            use_askpass (url_parsed);

          if ((opt.recursive || opt.page_requisites)
              && ((url_scheme (*t) != SCHEME_FTP
#ifdef HAVE_SSL
              && url_scheme (*t) != SCHEME_FTPS
#endif
              )
                  || url_uses_proxy (url_parsed)))
            {
              int old_follow_ftp = opt.follow_ftp;

              /* Turn opt.follow_ftp on in case of recursive FTP retrieval */
              if (url_scheme (*t) == SCHEME_FTP
#ifdef HAVE_SSL
                  || url_scheme (*t) == SCHEME_FTPS
#endif
                  )
                opt.follow_ftp = 1;

              retrieve_tree (url_parsed, NULL);

              opt.follow_ftp = old_follow_ftp;
            }
          else
            {
              retrieve_url (url_parsed, *t, &filename, &redirected_URL, NULL,
                            &dt, opt.recursive, iri, true);
            }

          if (opt.delete_after && filename != NULL && file_exists_p (filename, NULL))
            {
              DEBUGP (("Removing file due to --delete-after in main():\n"));
              logprintf (LOG_VERBOSE, _("Removing %s.\n"), filename);
              if (unlink (filename))
                logprintf (LOG_NOTQUIET, "unlink: %s\n", strerror (errno));
            }
          xfree (redirected_URL);
          xfree (filename);
          url_free (url_parsed);
        }
      iri_free (iri);
    }

  /* And then from the input file, if any.  */
  if (opt.input_filename)
    {
      int count;
      int status;
      status = retrieve_from_file (opt.input_filename, opt.force_html, &count);
      inform_exit_status (status);
      if (!count)
        logprintf (LOG_NOTQUIET, _("No URLs found in %s.\n"),
                   opt.input_filename);
    }

#ifdef HAVE_METALINK
  /* Finally, from metlink file, if any.  */
  if (opt.input_metalink)
    {
      metalink_error_t meta_err;
      uerr_t retr_err;
      metalink_t *metalink;

      meta_err = metalink_parse_file (opt.input_metalink, &metalink);

      if (meta_err)
        {
          logprintf (LOG_NOTQUIET, _("Unable to parse metalink file %s.\n"),
                     opt.input_metalink);
          retr_err = METALINK_PARSE_ERROR;
        }
      else
        {
          /* We need to sort the resources if preferred location
             was specified by the user.  */
          if (opt.preferred_location && opt.preferred_location[0])
            {
              metalink_file_t **mfile_ptr;
              for (mfile_ptr = metalink->files; *mfile_ptr; mfile_ptr++)
                {
                  metalink_resource_t **mres_ptr;
                  metalink_file_t *mfile = *mfile_ptr;
                  size_t mres_count = 0;

                  for (mres_ptr = mfile->resources; *mres_ptr; mres_ptr++)
                    mres_count++;

                  stable_sort (mfile->resources,
                               mres_count,
                               sizeof (metalink_resource_t *),
                               metalink_res_cmp);
                }
            }
          retr_err = retrieve_from_metalink (metalink);
          if (retr_err != RETROK)
            {
              logprintf (LOG_NOTQUIET,
                         _("Could not download all resources from %s.\n"),
                         quote (opt.input_metalink));
            }
          metalink_delete (metalink);
        }
      inform_exit_status (retr_err);
    }
#endif /* HAVE_METALINK */

  /* Print broken links. */
  if (opt.recursive && opt.spider)
    print_broken_links ();

  /* Print the downloaded sum.  */
  if ((opt.recursive || opt.page_requisites
       || nurl > 1
       || (opt.input_filename && total_downloaded_bytes != 0))
      &&
      total_downloaded_bytes != 0)
    {
      double end_time = ptimer_measure (timer);
      char *wall_time = xstrdup (secs_to_human_time (end_time - start_time));
      char *download_time = xstrdup (secs_to_human_time (total_download_time));

      ptimer_destroy (timer);

      logprintf (LOG_NOTQUIET,
                 _("FINISHED --%s--\nTotal wall clock time: %s\n"
                   "Downloaded: %d files, %s in %s (%s)\n"),
                 datetime_str (time (NULL)),
                 wall_time,
                 numurls,
                 human_readable (total_downloaded_bytes, 10, 1),
                 download_time,
                 retr_rate (total_downloaded_bytes, total_download_time));
      xfree (wall_time);
      xfree (download_time);

      /* Print quota warning, if exceeded.  */
      if (opt.quota && total_downloaded_bytes > opt.quota)
        logprintf (LOG_NOTQUIET,
                   _("Download quota of %s EXCEEDED!\n"),
                   human_readable (opt.quota, 10, 1));
    }

  if (opt.cookies_output)
    save_cookies ();

#ifdef HAVE_HSTS
  if (opt.hsts && hsts_store)
    save_hsts ();
#endif

  if ((opt.convert_links || opt.convert_file_only) && !opt.delete_after)
    convert_all_links ();

  cleanup ();

  exit (get_exit_status ());
}

#endif /* TESTING */

/*
 * vim: et ts=2 sw=2
 */

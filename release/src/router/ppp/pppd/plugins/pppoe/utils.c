
/*
 * utils.c - various utility functions used in pppoed.
 *
 * mostly stolen from ppp-2.3.10 by Marc Boucher <marc@mbsi.ca>
 *
 * Feb 18/2000 Made fully re-entrant (JHS)
 *
 * Copyright (c) 1999 The Australian National University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright poe_notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Australian National University.  The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>		/* stdio               */
#include <stdlib.h>		/* strtoul(), realloc() */
#include <string.h>		/* memcpy()             */
#include <unistd.h>		/* STDIN_FILENO,exec    */
#include <errno.h>		/* errno                */

#include <sys/time.h>

#include <net/ethernet.h>
#include <netinet/in.h>

#include <stdarg.h>
#include <ctype.h>
#include <syslog.h>
#include <limits.h>
#include <paths.h>

#include "pppoe.h"

static char pidfilename[PATH_MAX];	/* name of pid file */

/*
static int detached = 0;
   log_to_fd = -1;
 */

static void vslp_printer (void *, char *,...);
static void format_packet (struct pppoe_packet *, int, void (*)(void *, char *,...), void *);
static void format_tag (struct pppoe_tag *, void (*)(void *, char *,...), void *);
struct buffer_poe_info {
  char *ptr;
  int len;
};

void poe_die (int status);


/*
 * vpoe_slprintf - like vsprintf, except we
 * also specify the length of the output buffer, and we handle
 * %r (recursive format), %m (poe_error message), %v (visible string),
 * %q (quoted string), %t (current time) and %E (Ether address) formats.
 * Doesn't do floating-point formats.
 * Returns the number of chars put into buf.
 */
#define OUTCHAR(c)	(buflen > 0? (--buflen, *buf++ = (c)): 0)

int
vpoe_slprintf (char *buf, int buflen, char *fmt, va_list args)
{
  int c, i, n;
  int width, prec, fillch;
  int base, len, neg, quoted;
  unsigned long val = 0;
  char *str, *f, *buf0;
  unsigned char *p;
  char num[32];
  time_t t;
  static char hexchars[] = "0123456789abcdef";
  struct buffer_poe_info bufpoe_info;

  buf0 = buf;
  --buflen;
  while (buflen > 0) {
    for (f = fmt; *f != '%' && *f != 0; ++f);
    if (f > fmt) {
      len = f - fmt;
      if (len > buflen)
	len = buflen;
      memcpy (buf, fmt, len);
      buf += len;
      buflen -= len;
      fmt = f;
    }
    if (*fmt == 0)
      break;
    c = *++fmt;
    width = 0;
    prec = -1;
    fillch = ' ';
    if (c == '0') {
      fillch = '0';
      c = *++fmt;
    }
    if (c == '*') {
      width = va_arg (args, int);
      c = *++fmt;
    }
    else {
      while (isdigit (c)) {
	width = width * 10 + c - '0';
	c = *++fmt;
      }
    }
    if (c == '.') {
      c = *++fmt;
      if (c == '*') {
	prec = va_arg (args, int);
	c = *++fmt;
      }
      else {
	prec = 0;
	while (isdigit (c)) {
	  prec = prec * 10 + c - '0';
	  c = *++fmt;
	}
      }
    }
    str = 0;
    base = 0;
    neg = 0;
    ++fmt;
    switch (c) {
    case 'd':
      i = va_arg (args, int);
      if (i < 0) {
	neg = 1;
	val = -i;
      }
      else
	val = i;
      base = 10;
      break;
    case 'o':
      val = va_arg (args, unsigned int);
      base = 8;
      break;
    case 'x':
    case 'X':
      val = va_arg (args, unsigned int);
      base = 16;
      break;
    case 'p':
      val = (unsigned long) va_arg (args, void *);
      base = 16;
      neg = 2;
      break;
    case 's':
      str = va_arg (args, char *);
      break;
    case 'c':
      num[0] = va_arg (args, int);
      num[1] = 0;
      str = num;
      break;
    case 'm':
      str = strerror (errno);
      break;
    case 'E':
      p = va_arg (args, unsigned char *);
      for (n = ETH_ALEN; n > 0; --n) {
	c = *p++;
	OUTCHAR (hexchars[(c >> 4) & 0xf]);
	OUTCHAR (hexchars[c & 0xf]);
	if (n > 1)
	  OUTCHAR (':');
      }
      continue;
    case 'r':
      f = va_arg (args, char *);
#ifndef __powerpc__
      n = vpoe_slprintf (buf, buflen + 1, f, va_arg (args, va_list));
#else
      /* On the powerpc, a va_list is an array of 1 structure */
      n = vpoe_slprintf (buf, buflen + 1, f, va_arg (args, void *));
#endif
      buf += n;
      buflen -= n;
      continue;
    case 't':
      time (&t);
      str = ctime (&t);
      str += 4;			/* chop off the day name */
      str[15] = 0;		/* chop off year and newline */
      break;
    case 'v':			/* "visible" string */
    case 'q':			/* quoted string */
      quoted = c == 'q';
      p = va_arg (args, unsigned char *);
      if (fillch == '0' && prec >= 0) {
	n = prec;
      }
      else {
	n = strlen ((char *) p);
	if (prec >= 0 && n > prec)
	  n = prec;
      }
      while (n > 0 && buflen > 0) {
	c = *p++;
	--n;
	if (!quoted && c >= 0x80) {
	  OUTCHAR ('M');
	  OUTCHAR ('-');
	  c -= 0x80;
	}
	if (quoted && (c == '"' || c == '\\'))
	  OUTCHAR ('\\');
	if (c < 0x20 || (0x7f <= c && c < 0xa0)) {
	  if (quoted) {
	    OUTCHAR ('\\');
	    switch (c) {
	    case '\t':
	      OUTCHAR ('t');
	      break;
	    case '\n':
	      OUTCHAR ('n');
	      break;
	    case '\b':
	      OUTCHAR ('b');
	      break;
	    case '\f':
	      OUTCHAR ('f');
	      break;
	    default:
	      OUTCHAR ('x');
	      OUTCHAR (hexchars[c >> 4]);
	      OUTCHAR (hexchars[c & 0xf]);
	    }
	  }
	  else {
	    if (c == '\t')
	      OUTCHAR (c);
	    else {
	      OUTCHAR ('^');
	      OUTCHAR (c ^ 0x40);
	    }
	  }
	}
	else
	  OUTCHAR (c);
      }
      continue;
    case 'P':			/* print PPPoE packet */
      bufpoe_info.ptr = buf;
      bufpoe_info.len = buflen + 1;
      p = va_arg (args, unsigned char *);
      n = va_arg (args, int);
      format_packet ((struct pppoe_packet *) p, n, vslp_printer, &bufpoe_info);
      buf = bufpoe_info.ptr;
      buflen = bufpoe_info.len - 1;
      continue;
    case 'T':			/* print PPPoE tag */
      bufpoe_info.ptr = buf;
      bufpoe_info.len = buflen + 1;
      p = va_arg (args, unsigned char *);
      format_tag ((struct pppoe_tag *) p, vslp_printer, &bufpoe_info);
      buf = bufpoe_info.ptr;
      buflen = bufpoe_info.len - 1;
      continue;
    case 'B':
      p = va_arg (args, unsigned char *);
      for (n = prec; n > 0; --n) {
	c = *p++;
	if (fillch == ' ')
	  OUTCHAR (' ');
	OUTCHAR (hexchars[(c >> 4) & 0xf]);
	OUTCHAR (hexchars[c & 0xf]);
      }
      continue;
    default:
      *buf++ = '%';
      if (c != '%')
	--fmt;			/* so %z outputs %z etc. */
      --buflen;
      continue;
    }
    if (base != 0) {
      str = num + sizeof (num);
      *--str = 0;
      while (str > num + neg) {
	*--str = hexchars[val % base];
	val = val / base;
	if (--prec <= 0 && val == 0)
	  break;
      }
      switch (neg) {
      case 1:
	*--str = '-';
	break;
      case 2:
	*--str = 'x';
	*--str = '0';
	break;
      }
      len = num + sizeof (num) - 1 - str;
    }
    else {
      len = strlen (str);
      if (prec >= 0 && len > prec)
	len = prec;
    }
    if (width > 0) {
      if (width > buflen)
	width = buflen;
      if ((n = width - len) > 0) {
	buflen -= n;
	for (; n > 0; --n)
	  *buf++ = fillch;
      }
    }
    if (len > buflen)
      len = buflen;
    memcpy (buf, str, len);
    buf += len;
    buflen -= len;
  }
  *buf = 0;
  return buf - buf0;
}

/*
 * vslp_printer - used in processing a %P format
 */
static void
vslp_printer (void *arg, char *fmt,...)
{
  int n;
  va_list pvar;
  struct buffer_poe_info *bi;

  va_start (pvar, fmt);

  bi = (struct buffer_poe_info *) arg;
  n = vpoe_slprintf (bi->ptr, bi->len, fmt, pvar);
  va_end (pvar);

  bi->ptr += n;
  bi->len -= n;
}

/*
 * format_packet - make a readable representation of a packet,
 * calling `printer(arg, format, ...)' to output it.
 */
static void
format_packet (struct pppoe_packet *p,
	       int len,
	       void (*printer) (void *, char *,...),
	       void *arg)
{
  struct pppoe_tag *t;

  printer (arg, "Ether addr: %E\n", p->addr.sll_addr);

  switch ((unsigned) ntohs (p->addr.sll_protocol)) {
  case ETH_P_PPPOE_DISC:
    printer (arg, " (PPPOE Discovery)\n");
    break;
  case ETH_P_PPPOE_SESS:
    printer (arg, " (PPPOE Session)\n");
    break;
  }

  printer (arg, " PPPoE hdr: ver=0x%01x type=0x%01x code=0x%02x "
	   "sid=0x%04x length=0x%04x ", (unsigned) p->hdr->ver,
	   (unsigned) p->hdr->type, (unsigned) p->hdr->code, (unsigned) p->hdr->sid,
	   (unsigned) ntohs (p->hdr->length));

  switch (p->hdr->code) {
  case PADI_CODE:
    printer (arg, "(PADI)\n");
    break;
  case PADO_CODE:
    printer (arg, "(PADO)\n");
    break;
  case PADR_CODE:
    printer (arg, "(PADR)\n");
    break;
  case PADS_CODE:
    printer (arg, "(PADS)\n");
    break;
  case PADT_CODE:
    printer (arg, "(PADT)\n");
    break;
  default:
    printer (arg, "(Unknown)\n");
  }


  for(t = (struct pppoe_tag *) (&p->hdr->tag);
      (t < (struct pppoe_tag *) ((char *) (&p->hdr->tag) + ntohs (p->hdr->length))) &&
	  ntohs (t->tag_type) != PTT_EOL;
      t = (struct pppoe_tag *) ((char *) (t + 1) + ntohs (t->tag_len))) {
      format_tag (t, printer, arg);
  }
}

/*
 * format_tag - make a readable representation of a tag,
 * calling `printer(arg, format, ...)' to output it.
 */
static void
format_tag (struct pppoe_tag *t,
	       void (*printer) (void *, char *,...),
	       void *arg)
{
    printer (arg, " PPPoE tag: type=%04x length=%04x ",
	     ntohs (t->tag_type), ntohs (t->tag_len));
    switch ( t->tag_type ) {
    case PTT_EOL:
      printer (arg, "(End of list)");
      break;
    case PTT_SRV_NAME:
      printer (arg, "(Service name)");
      break;
    case PTT_AC_NAME:
      printer (arg, "(AC Name)");
      break;
    case PTT_HOST_UNIQ:
      printer (arg, "(Host Uniq)");
      break;
    case PTT_AC_COOKIE:
      printer (arg, "(AC Cookie)");
      break;
    case PTT_VENDOR:
      printer (arg, "(Vendor Specific)");
      break;
    case PTT_RELAY_SID:
      printer (arg, "(Relay Session ID)");
      break;
    case PTT_SRV_ERR:
      printer (arg, "(Service Name Error)");
      break;
    case PTT_SYS_ERR:
      printer (arg, "(AC System Error)");
      break;
    case PTT_GEN_ERR:
      printer (arg, "(Generic Error)");
      break;
    default:
      printer (arg, "(Unknown)");
    }
    if (ntohs (t->tag_len) > 0)
      switch ( t->tag_type ) {
      case PTT_SRV_NAME:
      case PTT_AC_NAME:
      case PTT_SRV_ERR:
      case PTT_SYS_ERR:
      case PTT_GEN_ERR:	/* ascii data */
	{
	  char *buf;
	  buf = malloc (ntohs (t->tag_len) + 1);
	  memset (buf, 0, ntohs (t->tag_len) + 1);
	  strncpy (buf, (char *) (t + 1), ntohs (t->tag_len));
//	  buf[ntohs (t->tag_len)] = '\0';
	  printer (arg, " data (UTF-8): %s", buf);
	  free (buf);
	  break;
	}

      case PTT_HOST_UNIQ:
      case PTT_AC_COOKIE:
      case PTT_RELAY_SID:
	printer (arg, " data (bin): %.*B", ntohs (t->tag_len), (char *) (t + 1));
	break;

      default:
	printer (arg, " unrecognized data");
      }
}

/*
 * poe_logit - does the hard work for poe_fatal et al.
 */
static void
poe_logit (struct session *ses,int level, char *fmt, va_list args)
{
  int n;
  char buf[256];

  n = vpoe_slprintf (buf, sizeof (buf), fmt, args);
  syslog (level, "%s", buf);
  if (log_to_fd >= 0 && (level != LOG_DEBUG || ses->opt_debug)) {
    if (buf[n - 1] != '\n')
      buf[n++] = '\n';
    if (write (log_to_fd, buf, n) != n)
      log_to_fd = -1;
  }
}

/*
 * poe_fatal - log an poe_error message and poe_die horribly.
 */
void
poe_fatal (struct session *ses, char *fmt,...)
{
  va_list pvar;

  va_start (pvar, fmt);

  poe_logit (ses,LOG_ERR, fmt, pvar);
  va_end (pvar);

  poe_die(1);			/* as promised */
}

/*
 * poe_error - log an poe_error message.
 */
void
poe_error (struct session *ses,char *fmt,...)
{
  va_list pvar;

  va_start (pvar, fmt);

  poe_logit (ses,LOG_ERR, fmt, pvar);
  va_end (pvar);
}

/*
 * poe_warn - log a poe_warning message.
 */
void
poe_warn (struct session *ses,char *fmt,...)
{
  va_list pvar;

  va_start (pvar, fmt);

  poe_logit (ses,LOG_WARNING, fmt, pvar);
  va_end (pvar);
}

/*
 * poe_info - log an poe_informational message.
 */
void
poe_info (struct session *ses,char *fmt,...)
{
  va_list pvar;

  va_start (pvar, fmt);

  poe_logit (ses,LOG_INFO, fmt, pvar);
  va_end (pvar);
}

/*
 * poe_dbglog - log a debug message.
 */
void
poe_dbglog (struct session *ses ,char *fmt,...)
{
  va_list pvar;

  va_start (pvar, fmt);

  poe_logit (ses,LOG_DEBUG, fmt, pvar);
  va_end (pvar);
}

/*
 * Create a file containing our process ID.
 */
void
poe_create_pidfile (struct session *ses)
{
  FILE *pidfile;

  sprintf (pidfilename, "%s%s.pid", _PATH_VARRUN, "pppoed");
  if ((pidfile = fopen (pidfilename, "w")) != NULL) {
    fprintf (pidfile, "%d\n", getpid ());
    (void) fclose (pidfile);
  }
  else {
    poe_error (ses,"Failed to create pid file %s: %m", pidfilename);
    pidfilename[0] = 0;
  }
}

/*
 * detach - detach us from the controlling terminal.
 */
void
poe_detach (struct session *ses)
{
  if (ses->detached)
    return;

  if ((daemon (0, 0)) < 0) {
    poe_error (ses,"Couldn't detach (daemon failed: %m)");
  }
  ses->detached = 1;
  ses->log_to_fd = -1;
  /* update pid files if they have been written already */
  if (pidfilename[0])
    poe_create_pidfile (ses);
}

/*
 * cleanup - restore anything which needs to be restored before we exit
 */
/* ARGSUSED */
static void
cleanup ()
{
  if (pidfilename[0] != 0 && unlink (pidfilename) < 0 && errno != ENOENT)
    syslog (LOG_INFO,"unable to delete pid file ");
  pidfilename[0] = 0;
}

/*
 * poe_die - clean up state and exit with the specified status.
 */
void
poe_die (int status)
{
  cleanup ();
  syslog (LOG_INFO, "Exit.");
  exit (status);
}

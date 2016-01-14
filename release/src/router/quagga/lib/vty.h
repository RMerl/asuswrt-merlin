/* Virtual terminal [aka TeletYpe] interface routine
   Copyright (C) 1997 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#ifndef _ZEBRA_VTY_H
#define _ZEBRA_VTY_H

#include "thread.h"
#include "log.h"
#include "sockunion.h"

#define VTY_BUFSIZ 512
#define VTY_MAXHIST 20

/* VTY struct. */
struct vty 
{
  /* File descripter of this vty. */
  int fd;

  /* Is this vty connect to file or not */
  enum {VTY_TERM, VTY_FILE, VTY_SHELL, VTY_SHELL_SERV} type;

  /* Node status of this vty */
  int node;

  /* Failure count */
  int fail;

  /* Output buffer. */
  struct buffer *obuf;

  /* Command input buffer */
  char *buf;

  /* Command cursor point */
  int cp;

  /* Command length */
  int length;

  /* Command max length. */
  int max;

  /* Histry of command */
  char *hist[VTY_MAXHIST];

  /* History lookup current point */
  int hp;

  /* History insert end point */
  int hindex;

  /* For current referencing point of interface, route-map,
     access-list etc... */
  void *index;

  /* For multiple level index treatment such as key chain and key. */
  void *index_sub;

  /* For escape character. */
  unsigned char escape;

  /* Current vty status. */
  enum {VTY_NORMAL, VTY_CLOSE, VTY_MORE, VTY_MORELINE} status;

  /* IAC handling: was the last character received the
     IAC (interpret-as-command) escape character (and therefore the next
     character will be the command code)?  Refer to Telnet RFC 854. */
  unsigned char iac;

  /* IAC SB (option subnegotiation) handling */
  unsigned char iac_sb_in_progress;
  /* At the moment, we care only about the NAWS (window size) negotiation,
     and that requires just a 5-character buffer (RFC 1073):
       <NAWS char> <16-bit width> <16-bit height> */
#define TELNET_NAWS_SB_LEN 5
  unsigned char sb_buf[TELNET_NAWS_SB_LEN];
  /* How many subnegotiation characters have we received?  We just drop
     those that do not fit in the buffer. */
  size_t sb_len;

  /* Window width/height. */
  int width;
  int height;

  /* Configure lines. */
  int lines;

  /* Terminal monitor. */
  int monitor;

  /* In configure mode. */
  int config;

  /* Read and write thread. */
  struct thread *t_read;
  struct thread *t_write;

  /* Timeout seconds and thread. */
  unsigned long v_timeout;
  struct thread *t_timeout;

  /* What address is this vty comming from. */
  char address[SU_ADDRSTRLEN];
};

/* Integrated configuration file. */
#define INTEGRATE_DEFAULT_CONFIG "Quagga.conf"

/* Small macro to determine newline is newline only or linefeed needed. */
#define VTY_NEWLINE  ((vty->type == VTY_TERM) ? "\r\n" : "\n")

/* Default time out value */
#define VTY_TIMEOUT_DEFAULT 600

/* Vty read buffer size. */
#define VTY_READ_BUFSIZ 512

/* Directory separator. */
#ifndef DIRECTORY_SEP
#define DIRECTORY_SEP '/'
#endif /* DIRECTORY_SEP */

#ifndef IS_DIRECTORY_SEP
#define IS_DIRECTORY_SEP(c) ((c) == DIRECTORY_SEP)
#endif

/* GCC have printf type attribute check.  */
#ifdef __GNUC__
#define PRINTF_ATTRIBUTE(a,b) __attribute__ ((__format__ (__printf__, a, b)))
#else
#define PRINTF_ATTRIBUTE(a,b)
#endif /* __GNUC__ */

/* Utility macros to convert VTY argument to unsigned long */
#define VTY_GET_ULONG(NAME,V,STR) \
do { \
  char *endptr = NULL; \
  errno = 0; \
  (V) = strtoul ((STR), &endptr, 10); \
  if (*(STR) == '-' || *endptr != '\0' || errno) \
    { \
      vty_out (vty, "%% Invalid %s value%s", NAME, VTY_NEWLINE); \
      return CMD_WARNING; \
    } \
} while (0)

/*
 * The logic below ((TMPL) <= ((MIN) && (TMPL) != (MIN)) is
 * done to circumvent the compiler complaining about
 * comparing unsigned numbers against zero, if MIN is zero.
 * NB: The compiler isn't smart enough to supress the warning
 * if you write (MIN) != 0 && tmpl < (MIN).
 */
#define VTY_GET_INTEGER_RANGE_HEART(NAME,TMPL,STR,MIN,MAX)      \
do {                                                            \
  VTY_GET_ULONG(NAME, (TMPL), STR);                             \
  if ( ((TMPL) <= (MIN) && (TMPL) != (MIN)) || (TMPL) > (MAX) ) \
    {                                                           \
      vty_out (vty, "%% Invalid %s value%s", NAME, VTY_NEWLINE);\
      return CMD_WARNING;                                       \
    }                                                           \
} while (0)

#define VTY_GET_INTEGER_RANGE(NAME,V,STR,MIN,MAX)               \
do {                                                            \
  unsigned long tmpl;                                           \
  VTY_GET_INTEGER_RANGE_HEART(NAME,tmpl,STR,MIN,MAX);           \
  (V) = tmpl;                                                   \
} while (0)

#define VTY_CHECK_INTEGER_RANGE(NAME,STR,MIN,MAX)               \
do {                                                            \
  unsigned long tmpl;                                           \
  VTY_GET_INTEGER_RANGE_HEART(NAME,tmpl,STR,MIN,MAX);           \
} while (0)

#define VTY_GET_INTEGER(NAME,V,STR)                             \
    VTY_GET_INTEGER_RANGE(NAME,V,STR,0U,UINT32_MAX)

#define VTY_GET_IPV4_ADDRESS(NAME,V,STR)                                      \
do {                                                                             \
  int retv;                                                                   \
  retv = inet_aton ((STR), &(V));                                             \
  if (!retv)                                                                  \
    {                                                                         \
      vty_out (vty, "%% Invalid %s value%s", NAME, VTY_NEWLINE);              \
      return CMD_WARNING;                                                     \
    }                                                                         \
} while (0)

#define VTY_GET_IPV4_PREFIX(NAME,V,STR)                                       \
do {                                                                             \
  int retv;                                                                   \
  retv = str2prefix_ipv4 ((STR), &(V));                                       \
  if (retv <= 0)                                                              \
    {                                                                         \
      vty_out (vty, "%% Invalid %s value%s", NAME, VTY_NEWLINE);              \
      return CMD_WARNING;                                                     \
    }                                                                         \
} while (0)

#define VTY_WARN_EXPERIMENTAL()                                               \
do {                                                                          \
  vty_out (vty, "%% WARNING: this command is experimental. Both its name and" \
                " parameters may%s%% change in a future version of Quagga,"   \
                " possibly breaking your configuration!%s",                   \
                VTY_NEWLINE, VTY_NEWLINE);                                    \
} while (0)

/* Exported variables */
extern char integrate_default[];

/* Prototypes. */
extern void vty_init (struct thread_master *);
extern void vty_init_vtysh (void);
extern void vty_terminate (void);
extern void vty_reset (void);
extern struct vty *vty_new (void);
extern int vty_out (struct vty *, const char *, ...) PRINTF_ATTRIBUTE(2, 3);
extern void vty_read_config (char *, char *);
extern void vty_time_print (struct vty *, int);
extern void vty_serv_sock (const char *, unsigned short, const char *);
extern void vty_close (struct vty *);
extern char *vty_get_cwd (void);
extern void vty_log (const char *level, const char *proto, 
                     const char *fmt, struct timestamp_control *, va_list);
extern int vty_config_lock (struct vty *);
extern int vty_config_unlock (struct vty *);
extern int vty_shell (struct vty *);
extern int vty_shell_serv (struct vty *);
extern void vty_hello (struct vty *);

/* Send a fixed-size message to all vty terminal monitors; this should be
   an async-signal-safe function. */
extern void vty_log_fixed (char *buf, size_t len);

#endif /* _ZEBRA_VTY_H */

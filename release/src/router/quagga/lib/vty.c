/*
 * Virtual terminal [aka TeletYpe] interface routine.
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

#include "linklist.h"
#include "thread.h"
#include "buffer.h"
#include <lib/version.h>
#include "command.h"
#include "sockunion.h"
#include "memory.h"
#include "str.h"
#include "log.h"
#include "prefix.h"
#include "filter.h"
#include "vty.h"
#include "privs.h"
#include "network.h"

#include <arpa/telnet.h>

/* Vty events */
enum event 
{
  VTY_SERV,
  VTY_READ,
  VTY_WRITE,
  VTY_TIMEOUT_RESET,
#ifdef VTYSH
  VTYSH_SERV,
  VTYSH_READ,
  VTYSH_WRITE
#endif /* VTYSH */
};

static void vty_event (enum event, int, struct vty *);

/* Extern host structure from command.c */
extern struct host host;

/* Vector which store each vty structure. */
static vector vtyvec;

/* Vty timeout value. */
static unsigned long vty_timeout_val = VTY_TIMEOUT_DEFAULT;

/* Vty access-class command */
static char *vty_accesslist_name = NULL;

/* Vty access-calss for IPv6. */
static char *vty_ipv6_accesslist_name = NULL;

/* VTY server thread. */
static vector Vvty_serv_thread;

/* Current directory. */
char *vty_cwd = NULL;

/* Configure lock. */
static int vty_config;

/* Login password check. */
static int no_password_check = 0;

/* Restrict unauthenticated logins? */
static const u_char restricted_mode_default = 0;
static u_char restricted_mode = 0;

/* Integrated configuration file path */
char integrate_default[] = SYSCONFDIR INTEGRATE_DEFAULT_CONFIG;


/* VTY standard output function. */
int
vty_out (struct vty *vty, const char *format, ...)
{
  va_list args;
  int len = 0;
  int size = 1024;
  char buf[1024];
  char *p = NULL;

  if (vty_shell (vty))
    {
      va_start (args, format);
      vprintf (format, args);
      va_end (args);
    }
  else
    {
      /* Try to write to initial buffer.  */
      va_start (args, format);
      len = vsnprintf (buf, sizeof buf, format, args);
      va_end (args);

      /* Initial buffer is not enough.  */
      if (len < 0 || len >= size)
	{
	  while (1)
	    {
	      if (len > -1)
		size = len + 1;
	      else
		size = size * 2;

	      p = XREALLOC (MTYPE_VTY_OUT_BUF, p, size);
	      if (! p)
		return -1;

	      va_start (args, format);
	      len = vsnprintf (p, size, format, args);
	      va_end (args);

	      if (len > -1 && len < size)
		break;
	    }
	}

      /* When initial buffer is enough to store all output.  */
      if (! p)
	p = buf;

      /* Pointer p must point out buffer. */
      buffer_put (vty->obuf, (u_char *) p, len);

      /* If p is not different with buf, it is allocated buffer.  */
      if (p != buf)
	XFREE (MTYPE_VTY_OUT_BUF, p);
    }

  return len;
}

static int
vty_log_out (struct vty *vty, const char *level, const char *proto_str,
	     const char *format, struct timestamp_control *ctl, va_list va)
{
  int ret;
  int len;
  char buf[1024];

  if (!ctl->already_rendered)
    {
      ctl->len = quagga_timestamp(ctl->precision, ctl->buf, sizeof(ctl->buf));
      ctl->already_rendered = 1;
    }
  if (ctl->len+1 >= sizeof(buf))
    return -1;
  memcpy(buf, ctl->buf, len = ctl->len);
  buf[len++] = ' ';
  buf[len] = '\0';

  if (level)
    ret = snprintf(buf+len, sizeof(buf)-len, "%s: %s: ", level, proto_str);
  else
    ret = snprintf(buf+len, sizeof(buf)-len, "%s: ", proto_str);
  if ((ret < 0) || ((size_t)(len += ret) >= sizeof(buf)))
    return -1;

  if (((ret = vsnprintf(buf+len, sizeof(buf)-len, format, va)) < 0) ||
      ((size_t)((len += ret)+2) > sizeof(buf)))
    return -1;

  buf[len++] = '\r';
  buf[len++] = '\n';

  if (write(vty->fd, buf, len) < 0)
    {
      if (ERRNO_IO_RETRY(errno))
	/* Kernel buffer is full, probably too much debugging output, so just
	   drop the data and ignore. */
	return -1;
      /* Fatal I/O error. */
      vty->monitor = 0; /* disable monitoring to avoid infinite recursion */
      zlog_warn("%s: write failed to vty client fd %d, closing: %s",
		__func__, vty->fd, safe_strerror(errno));
      buffer_reset(vty->obuf);
      /* cannot call vty_close, because a parent routine may still try
         to access the vty struct */
      vty->status = VTY_CLOSE;
      shutdown(vty->fd, SHUT_RDWR);
      return -1;
    }
  return 0;
}

/* Output current time to the vty. */
void
vty_time_print (struct vty *vty, int cr)
{
  char buf [25];
  
  if (quagga_timestamp(0, buf, sizeof(buf)) == 0)
    {
      zlog (NULL, LOG_INFO, "quagga_timestamp error");
      return;
    }
  if (cr)
    vty_out (vty, "%s\n", buf);
  else
    vty_out (vty, "%s ", buf);

  return;
}

/* Say hello to vty interface. */
void
vty_hello (struct vty *vty)
{
  if (host.motdfile)
    {
      FILE *f;
      char buf[4096];

      f = fopen (host.motdfile, "r");
      if (f)
	{
	  while (fgets (buf, sizeof (buf), f))
	    {
	      char *s;
	      /* work backwards to ignore trailling isspace() */
	      for (s = buf + strlen (buf); (s > buf) && isspace ((int)*(s - 1));
		   s--);
	      *s = '\0';
	      vty_out (vty, "%s%s", buf, VTY_NEWLINE);
	    }
	  fclose (f);
	}
      else
	vty_out (vty, "MOTD file not found%s", VTY_NEWLINE);
    }
  else if (host.motd)
    vty_out (vty, "%s", host.motd);
}

/* Put out prompt and wait input from user. */
static void
vty_prompt (struct vty *vty)
{
  struct utsname names;
  const char*hostname;

  if (vty->type == VTY_TERM)
    {
      hostname = host.name;
      if (!hostname)
	{
	  uname (&names);
	  hostname = names.nodename;
	}
      vty_out (vty, cmd_prompt (vty->node), hostname);
    }
}

/* Send WILL TELOPT_ECHO to remote server. */
static void
vty_will_echo (struct vty *vty)
{
  unsigned char cmd[] = { IAC, WILL, TELOPT_ECHO, '\0' };
  vty_out (vty, "%s", cmd);
}

/* Make suppress Go-Ahead telnet option. */
static void
vty_will_suppress_go_ahead (struct vty *vty)
{
  unsigned char cmd[] = { IAC, WILL, TELOPT_SGA, '\0' };
  vty_out (vty, "%s", cmd);
}

/* Make don't use linemode over telnet. */
static void
vty_dont_linemode (struct vty *vty)
{
  unsigned char cmd[] = { IAC, DONT, TELOPT_LINEMODE, '\0' };
  vty_out (vty, "%s", cmd);
}

/* Use window size. */
static void
vty_do_window_size (struct vty *vty)
{
  unsigned char cmd[] = { IAC, DO, TELOPT_NAWS, '\0' };
  vty_out (vty, "%s", cmd);
}

#if 0 /* Currently not used. */
/* Make don't use lflow vty interface. */
static void
vty_dont_lflow_ahead (struct vty *vty)
{
  unsigned char cmd[] = { IAC, DONT, TELOPT_LFLOW, '\0' };
  vty_out (vty, "%s", cmd);
}
#endif /* 0 */

/* Allocate new vty struct. */
struct vty *
vty_new ()
{
  struct vty *new = XCALLOC (MTYPE_VTY, sizeof (struct vty));

  new->obuf = buffer_new(0);	/* Use default buffer size. */
  new->buf = XCALLOC (MTYPE_VTY, VTY_BUFSIZ);
  new->max = VTY_BUFSIZ;

  return new;
}

/* Authentication of vty */
static void
vty_auth (struct vty *vty, char *buf)
{
  char *passwd = NULL;
  enum node_type next_node = 0;
  int fail;
  char *crypt (const char *, const char *);

  switch (vty->node)
    {
    case AUTH_NODE:
      if (host.encrypt)
	passwd = host.password_encrypt;
      else
	passwd = host.password;
      if (host.advanced)
	next_node = host.enable ? VIEW_NODE : ENABLE_NODE;
      else
	next_node = VIEW_NODE;
      break;
    case AUTH_ENABLE_NODE:
      if (host.encrypt)
	passwd = host.enable_encrypt;
      else
	passwd = host.enable;
      next_node = ENABLE_NODE;
      break;
    }

  if (passwd)
    {
      if (host.encrypt)
	fail = strcmp (crypt(buf, passwd), passwd);
      else
	fail = strcmp (buf, passwd);
    }
  else
    fail = 1;

  if (! fail)
    {
      vty->fail = 0;
      vty->node = next_node;	/* Success ! */
    }
  else
    {
      vty->fail++;
      if (vty->fail >= 3)
	{
	  if (vty->node == AUTH_NODE)
	    {
	      vty_out (vty, "%% Bad passwords, too many failures!%s", VTY_NEWLINE);
	      vty->status = VTY_CLOSE;
	    }
	  else			
	    {
	      /* AUTH_ENABLE_NODE */
	      vty->fail = 0;
	      vty_out (vty, "%% Bad enable passwords, too many failures!%s", VTY_NEWLINE);
	      vty->node = restricted_mode ? RESTRICTED_NODE : VIEW_NODE;
	    }
	}
    }
}

/* Command execution over the vty interface. */
static int
vty_command (struct vty *vty, char *buf)
{
  int ret;
  vector vline;
  const char *protocolname;

  /* Split readline string up into the vector */
  vline = cmd_make_strvec (buf);

  if (vline == NULL)
    return CMD_SUCCESS;

#ifdef CONSUMED_TIME_CHECK
  {
    RUSAGE_T before;
    RUSAGE_T after;
    unsigned long realtime, cputime;

    GETRUSAGE(&before);
#endif /* CONSUMED_TIME_CHECK */

  ret = cmd_execute_command (vline, vty, NULL, 0);

  /* Get the name of the protocol if any */
  if (zlog_default)
      protocolname = zlog_proto_names[zlog_default->protocol];
  else
      protocolname = zlog_proto_names[ZLOG_NONE];
                                                                           
#ifdef CONSUMED_TIME_CHECK
    GETRUSAGE(&after);
    if ((realtime = thread_consumed_time(&after, &before, &cputime)) >
    	CONSUMED_TIME_CHECK)
      /* Warn about CPU hog that must be fixed. */
      zlog_warn("SLOW COMMAND: command took %lums (cpu time %lums): %s",
      		realtime/1000, cputime/1000, buf);
  }
#endif /* CONSUMED_TIME_CHECK */

  if (ret != CMD_SUCCESS)
    switch (ret)
      {
      case CMD_WARNING:
	if (vty->type == VTY_FILE)
	  vty_out (vty, "Warning...%s", VTY_NEWLINE);
	break;
      case CMD_ERR_AMBIGUOUS:
	vty_out (vty, "%% Ambiguous command.%s", VTY_NEWLINE);
	break;
      case CMD_ERR_NO_MATCH:
	vty_out (vty, "%% [%s] Unknown command: %s%s", protocolname, buf, VTY_NEWLINE);
	break;
      case CMD_ERR_INCOMPLETE:
	vty_out (vty, "%% Command incomplete.%s", VTY_NEWLINE);
	break;
      }
  cmd_free_strvec (vline);

  return ret;
}

static const char telnet_backward_char = 0x08;
static const char telnet_space_char = ' ';

/* Basic function to write buffer to vty. */
static void
vty_write (struct vty *vty, const char *buf, size_t nbytes)
{
  if ((vty->node == AUTH_NODE) || (vty->node == AUTH_ENABLE_NODE))
    return;

  /* Should we do buffering here ?  And make vty_flush (vty) ? */
  buffer_put (vty->obuf, buf, nbytes);
}

/* Ensure length of input buffer.  Is buffer is short, double it. */
static void
vty_ensure (struct vty *vty, int length)
{
  if (vty->max <= length)
    {
      vty->max *= 2;
      vty->buf = XREALLOC (MTYPE_VTY, vty->buf, vty->max);
    }
}

/* Basic function to insert character into vty. */
static void
vty_self_insert (struct vty *vty, char c)
{
  int i;
  int length;

  vty_ensure (vty, vty->length + 1);
  length = vty->length - vty->cp;
  memmove (&vty->buf[vty->cp + 1], &vty->buf[vty->cp], length);
  vty->buf[vty->cp] = c;

  vty_write (vty, &vty->buf[vty->cp], length + 1);
  for (i = 0; i < length; i++)
    vty_write (vty, &telnet_backward_char, 1);

  vty->cp++;
  vty->length++;
}

/* Self insert character 'c' in overwrite mode. */
static void
vty_self_insert_overwrite (struct vty *vty, char c)
{
  vty_ensure (vty, vty->length + 1);
  vty->buf[vty->cp++] = c;

  if (vty->cp > vty->length)
    vty->length++;

  if ((vty->node == AUTH_NODE) || (vty->node == AUTH_ENABLE_NODE))
    return;

  vty_write (vty, &c, 1);
}

/* Insert a word into vty interface with overwrite mode. */
static void
vty_insert_word_overwrite (struct vty *vty, char *str)
{
  int len = strlen (str);
  vty_write (vty, str, len);
  strcpy (&vty->buf[vty->cp], str);
  vty->cp += len;
  vty->length = vty->cp;
}

/* Forward character. */
static void
vty_forward_char (struct vty *vty)
{
  if (vty->cp < vty->length)
    {
      vty_write (vty, &vty->buf[vty->cp], 1);
      vty->cp++;
    }
}

/* Backward character. */
static void
vty_backward_char (struct vty *vty)
{
  if (vty->cp > 0)
    {
      vty->cp--;
      vty_write (vty, &telnet_backward_char, 1);
    }
}

/* Move to the beginning of the line. */
static void
vty_beginning_of_line (struct vty *vty)
{
  while (vty->cp)
    vty_backward_char (vty);
}

/* Move to the end of the line. */
static void
vty_end_of_line (struct vty *vty)
{
  while (vty->cp < vty->length)
    vty_forward_char (vty);
}

static void vty_kill_line_from_beginning (struct vty *);
static void vty_redraw_line (struct vty *);

/* Print command line history.  This function is called from
   vty_next_line and vty_previous_line. */
static void
vty_history_print (struct vty *vty)
{
  int length;

  vty_kill_line_from_beginning (vty);

  /* Get previous line from history buffer */
  length = strlen (vty->hist[vty->hp]);
  memcpy (vty->buf, vty->hist[vty->hp], length);
  vty->cp = vty->length = length;

  /* Redraw current line */
  vty_redraw_line (vty);
}

/* Show next command line history. */
static void
vty_next_line (struct vty *vty)
{
  int try_index;

  if (vty->hp == vty->hindex)
    return;

  /* Try is there history exist or not. */
  try_index = vty->hp;
  if (try_index == (VTY_MAXHIST - 1))
    try_index = 0;
  else
    try_index++;

  /* If there is not history return. */
  if (vty->hist[try_index] == NULL)
    return;
  else
    vty->hp = try_index;

  vty_history_print (vty);
}

/* Show previous command line history. */
static void
vty_previous_line (struct vty *vty)
{
  int try_index;

  try_index = vty->hp;
  if (try_index == 0)
    try_index = VTY_MAXHIST - 1;
  else
    try_index--;

  if (vty->hist[try_index] == NULL)
    return;
  else
    vty->hp = try_index;

  vty_history_print (vty);
}

/* This function redraw all of the command line character. */
static void
vty_redraw_line (struct vty *vty)
{
  vty_write (vty, vty->buf, vty->length);
  vty->cp = vty->length;
}

/* Forward word. */
static void
vty_forward_word (struct vty *vty)
{
  while (vty->cp != vty->length && vty->buf[vty->cp] != ' ')
    vty_forward_char (vty);
  
  while (vty->cp != vty->length && vty->buf[vty->cp] == ' ')
    vty_forward_char (vty);
}

/* Backward word without skipping training space. */
static void
vty_backward_pure_word (struct vty *vty)
{
  while (vty->cp > 0 && vty->buf[vty->cp - 1] != ' ')
    vty_backward_char (vty);
}

/* Backward word. */
static void
vty_backward_word (struct vty *vty)
{
  while (vty->cp > 0 && vty->buf[vty->cp - 1] == ' ')
    vty_backward_char (vty);

  while (vty->cp > 0 && vty->buf[vty->cp - 1] != ' ')
    vty_backward_char (vty);
}

/* When '^D' is typed at the beginning of the line we move to the down
   level. */
static void
vty_down_level (struct vty *vty)
{
  vty_out (vty, "%s", VTY_NEWLINE);
  (*config_exit_cmd.func)(NULL, vty, 0, NULL);
  vty_prompt (vty);
  vty->cp = 0;
}

/* When '^Z' is received from vty, move down to the enable mode. */
static void
vty_end_config (struct vty *vty)
{
  vty_out (vty, "%s", VTY_NEWLINE);

  switch (vty->node)
    {
    case VIEW_NODE:
    case ENABLE_NODE:
    case RESTRICTED_NODE:
      /* Nothing to do. */
      break;
    case CONFIG_NODE:
    case INTERFACE_NODE:
    case ZEBRA_NODE:
    case RIP_NODE:
    case RIPNG_NODE:
    case BABEL_NODE:
    case BGP_NODE:
    case BGP_VPNV4_NODE:
    case BGP_IPV4_NODE:
    case BGP_IPV4M_NODE:
    case BGP_IPV6_NODE:
    case BGP_IPV6M_NODE:
    case RMAP_NODE:
    case OSPF_NODE:
    case OSPF6_NODE:
    case ISIS_NODE:
    case KEYCHAIN_NODE:
    case KEYCHAIN_KEY_NODE:
    case MASC_NODE:
    case PIM_NODE:
    case VTY_NODE:
      vty_config_unlock (vty);
      vty->node = ENABLE_NODE;
      break;
    default:
      /* Unknown node, we have to ignore it. */
      break;
    }

  vty_prompt (vty);
  vty->cp = 0;
}

/* Delete a charcter at the current point. */
static void
vty_delete_char (struct vty *vty)
{
  int i;
  int size;

  if (vty->length == 0)
    {
      vty_down_level (vty);
      return;
    }

  if (vty->cp == vty->length)
    return;			/* completion need here? */

  size = vty->length - vty->cp;

  vty->length--;
  memmove (&vty->buf[vty->cp], &vty->buf[vty->cp + 1], size - 1);
  vty->buf[vty->length] = '\0';
  
  if (vty->node == AUTH_NODE || vty->node == AUTH_ENABLE_NODE)
    return;

  vty_write (vty, &vty->buf[vty->cp], size - 1);
  vty_write (vty, &telnet_space_char, 1);

  for (i = 0; i < size; i++)
    vty_write (vty, &telnet_backward_char, 1);
}

/* Delete a character before the point. */
static void
vty_delete_backward_char (struct vty *vty)
{
  if (vty->cp == 0)
    return;

  vty_backward_char (vty);
  vty_delete_char (vty);
}

/* Kill rest of line from current point. */
static void
vty_kill_line (struct vty *vty)
{
  int i;
  int size;

  size = vty->length - vty->cp;
  
  if (size == 0)
    return;

  for (i = 0; i < size; i++)
    vty_write (vty, &telnet_space_char, 1);
  for (i = 0; i < size; i++)
    vty_write (vty, &telnet_backward_char, 1);

  memset (&vty->buf[vty->cp], 0, size);
  vty->length = vty->cp;
}

/* Kill line from the beginning. */
static void
vty_kill_line_from_beginning (struct vty *vty)
{
  vty_beginning_of_line (vty);
  vty_kill_line (vty);
}

/* Delete a word before the point. */
static void
vty_forward_kill_word (struct vty *vty)
{
  while (vty->cp != vty->length && vty->buf[vty->cp] == ' ')
    vty_delete_char (vty);
  while (vty->cp != vty->length && vty->buf[vty->cp] != ' ')
    vty_delete_char (vty);
}

/* Delete a word before the point. */
static void
vty_backward_kill_word (struct vty *vty)
{
  while (vty->cp > 0 && vty->buf[vty->cp - 1] == ' ')
    vty_delete_backward_char (vty);
  while (vty->cp > 0 && vty->buf[vty->cp - 1] != ' ')
    vty_delete_backward_char (vty);
}

/* Transpose chars before or at the point. */
static void
vty_transpose_chars (struct vty *vty)
{
  char c1, c2;

  /* If length is short or point is near by the beginning of line then
     return. */
  if (vty->length < 2 || vty->cp < 1)
    return;

  /* In case of point is located at the end of the line. */
  if (vty->cp == vty->length)
    {
      c1 = vty->buf[vty->cp - 1];
      c2 = vty->buf[vty->cp - 2];

      vty_backward_char (vty);
      vty_backward_char (vty);
      vty_self_insert_overwrite (vty, c1);
      vty_self_insert_overwrite (vty, c2);
    }
  else
    {
      c1 = vty->buf[vty->cp];
      c2 = vty->buf[vty->cp - 1];

      vty_backward_char (vty);
      vty_self_insert_overwrite (vty, c1);
      vty_self_insert_overwrite (vty, c2);
    }
}

/* Do completion at vty interface. */
static void
vty_complete_command (struct vty *vty)
{
  int i;
  int ret;
  char **matched = NULL;
  vector vline;

  if (vty->node == AUTH_NODE || vty->node == AUTH_ENABLE_NODE)
    return;

  vline = cmd_make_strvec (vty->buf);
  if (vline == NULL)
    return;

  /* In case of 'help \t'. */
  if (isspace ((int) vty->buf[vty->length - 1]))
    vector_set (vline, '\0');

  matched = cmd_complete_command (vline, vty, &ret);
  
  cmd_free_strvec (vline);

  vty_out (vty, "%s", VTY_NEWLINE);
  switch (ret)
    {
    case CMD_ERR_AMBIGUOUS:
      vty_out (vty, "%% Ambiguous command.%s", VTY_NEWLINE);
      vty_prompt (vty);
      vty_redraw_line (vty);
      break;
    case CMD_ERR_NO_MATCH:
      /* vty_out (vty, "%% There is no matched command.%s", VTY_NEWLINE); */
      vty_prompt (vty);
      vty_redraw_line (vty);
      break;
    case CMD_COMPLETE_FULL_MATCH:
      vty_prompt (vty);
      vty_redraw_line (vty);
      vty_backward_pure_word (vty);
      vty_insert_word_overwrite (vty, matched[0]);
      vty_self_insert (vty, ' ');
      XFREE (MTYPE_TMP, matched[0]);
      break;
    case CMD_COMPLETE_MATCH:
      vty_prompt (vty);
      vty_redraw_line (vty);
      vty_backward_pure_word (vty);
      vty_insert_word_overwrite (vty, matched[0]);
      XFREE (MTYPE_TMP, matched[0]);
      vector_only_index_free (matched);
      return;
      break;
    case CMD_COMPLETE_LIST_MATCH:
      for (i = 0; matched[i] != NULL; i++)
	{
	  if (i != 0 && ((i % 6) == 0))
	    vty_out (vty, "%s", VTY_NEWLINE);
	  vty_out (vty, "%-10s ", matched[i]);
	  XFREE (MTYPE_TMP, matched[i]);
	}
      vty_out (vty, "%s", VTY_NEWLINE);

      vty_prompt (vty);
      vty_redraw_line (vty);
      break;
    case CMD_ERR_NOTHING_TODO:
      vty_prompt (vty);
      vty_redraw_line (vty);
      break;
    default:
      break;
    }
  if (matched)
    vector_only_index_free (matched);
}

static void
vty_describe_fold (struct vty *vty, int cmd_width,
		   unsigned int desc_width, struct cmd_token *token)
{
  char *buf;
  const char *cmd, *p;
  int pos;

  cmd = token->cmd[0] == '.' ? token->cmd + 1 : token->cmd;

  if (desc_width <= 0)
    {
      vty_out (vty, "  %-*s  %s%s", cmd_width, cmd, token->desc, VTY_NEWLINE);
      return;
    }

  buf = XCALLOC (MTYPE_TMP, strlen (token->desc) + 1);

  for (p = token->desc; strlen (p) > desc_width; p += pos + 1)
    {
      for (pos = desc_width; pos > 0; pos--)
      if (*(p + pos) == ' ')
        break;

      if (pos == 0)
      break;

      strncpy (buf, p, pos);
      buf[pos] = '\0';
      vty_out (vty, "  %-*s  %s%s", cmd_width, cmd, buf, VTY_NEWLINE);

      cmd = "";
    }

  vty_out (vty, "  %-*s  %s%s", cmd_width, cmd, p, VTY_NEWLINE);

  XFREE (MTYPE_TMP, buf);
}

/* Describe matched command function. */
static void
vty_describe_command (struct vty *vty)
{
  int ret;
  vector vline;
  vector describe;
  unsigned int i, width, desc_width;
  struct cmd_token *token, *token_cr = NULL;

  vline = cmd_make_strvec (vty->buf);

  /* In case of '> ?'. */
  if (vline == NULL)
    {
      vline = vector_init (1);
      vector_set (vline, '\0');
    }
  else 
    if (isspace ((int) vty->buf[vty->length - 1]))
      vector_set (vline, '\0');

  describe = cmd_describe_command (vline, vty, &ret);

  vty_out (vty, "%s", VTY_NEWLINE);

  /* Ambiguous error. */
  switch (ret)
    {
    case CMD_ERR_AMBIGUOUS:
      vty_out (vty, "%% Ambiguous command.%s", VTY_NEWLINE);
      goto out;
      break;
    case CMD_ERR_NO_MATCH:
      vty_out (vty, "%% There is no matched command.%s", VTY_NEWLINE);
      goto out;
      break;
    }  

  /* Get width of command string. */
  width = 0;
  for (i = 0; i < vector_active (describe); i++)
    if ((token = vector_slot (describe, i)) != NULL)
      {
	unsigned int len;

	if (token->cmd[0] == '\0')
	  continue;

	len = strlen (token->cmd);
	if (token->cmd[0] == '.')
	  len--;

	if (width < len)
	  width = len;
      }

  /* Get width of description string. */
  desc_width = vty->width - (width + 6);

  /* Print out description. */
  for (i = 0; i < vector_active (describe); i++)
    if ((token = vector_slot (describe, i)) != NULL)
      {
	if (token->cmd[0] == '\0')
	  continue;
	
	if (strcmp (token->cmd, command_cr) == 0)
	  {
	    token_cr = token;
	    continue;
	  }

	if (!token->desc)
	  vty_out (vty, "  %-s%s",
		   token->cmd[0] == '.' ? token->cmd + 1 : token->cmd,
		   VTY_NEWLINE);
	else if (desc_width >= strlen (token->desc))
	  vty_out (vty, "  %-*s  %s%s", width,
		   token->cmd[0] == '.' ? token->cmd + 1 : token->cmd,
		   token->desc, VTY_NEWLINE);
	else
	  vty_describe_fold (vty, width, desc_width, token);

#if 0
	vty_out (vty, "  %-*s %s%s", width
		 desc->cmd[0] == '.' ? desc->cmd + 1 : desc->cmd,
		 desc->str ? desc->str : "", VTY_NEWLINE);
#endif /* 0 */
      }

  if ((token = token_cr))
    {
      if (!token->desc)
	vty_out (vty, "  %-s%s",
		 token->cmd[0] == '.' ? token->cmd + 1 : token->cmd,
		 VTY_NEWLINE);
      else if (desc_width >= strlen (token->desc))
	vty_out (vty, "  %-*s  %s%s", width,
		 token->cmd[0] == '.' ? token->cmd + 1 : token->cmd,
		 token->desc, VTY_NEWLINE);
      else
	vty_describe_fold (vty, width, desc_width, token);
    }

out:
  cmd_free_strvec (vline);
  if (describe)
    vector_free (describe);

  vty_prompt (vty);
  vty_redraw_line (vty);
}

static void
vty_clear_buf (struct vty *vty)
{
  memset (vty->buf, 0, vty->max);
}

/* ^C stop current input and do not add command line to the history. */
static void
vty_stop_input (struct vty *vty)
{
  vty->cp = vty->length = 0;
  vty_clear_buf (vty);
  vty_out (vty, "%s", VTY_NEWLINE);

  switch (vty->node)
    {
    case VIEW_NODE:
    case ENABLE_NODE:
    case RESTRICTED_NODE:
      /* Nothing to do. */
      break;
    case CONFIG_NODE:
    case INTERFACE_NODE:
    case ZEBRA_NODE:
    case RIP_NODE:
    case RIPNG_NODE:
    case BABEL_NODE:
    case BGP_NODE:
    case RMAP_NODE:
    case OSPF_NODE:
    case OSPF6_NODE:
    case ISIS_NODE:
    case KEYCHAIN_NODE:
    case KEYCHAIN_KEY_NODE:
    case MASC_NODE:
    case PIM_NODE:
    case VTY_NODE:
      vty_config_unlock (vty);
      vty->node = ENABLE_NODE;
      break;
    default:
      /* Unknown node, we have to ignore it. */
      break;
    }
  vty_prompt (vty);

  /* Set history pointer to the latest one. */
  vty->hp = vty->hindex;
}

/* Add current command line to the history buffer. */
static void
vty_hist_add (struct vty *vty)
{
  int index;

  if (vty->length == 0)
    return;

  index = vty->hindex ? vty->hindex - 1 : VTY_MAXHIST - 1;

  /* Ignore the same string as previous one. */
  if (vty->hist[index])
    if (strcmp (vty->buf, vty->hist[index]) == 0)
      {
      vty->hp = vty->hindex;
      return;
      }

  /* Insert history entry. */
  if (vty->hist[vty->hindex])
    XFREE (MTYPE_VTY_HIST, vty->hist[vty->hindex]);
  vty->hist[vty->hindex] = XSTRDUP (MTYPE_VTY_HIST, vty->buf);

  /* History index rotation. */
  vty->hindex++;
  if (vty->hindex == VTY_MAXHIST)
    vty->hindex = 0;

  vty->hp = vty->hindex;
}

/* #define TELNET_OPTION_DEBUG */

/* Get telnet window size. */
static int
vty_telnet_option (struct vty *vty, unsigned char *buf, int nbytes)
{
#ifdef TELNET_OPTION_DEBUG
  int i;

  for (i = 0; i < nbytes; i++)
    {
      switch (buf[i])
	{
	case IAC:
	  vty_out (vty, "IAC ");
	  break;
	case WILL:
	  vty_out (vty, "WILL ");
	  break;
	case WONT:
	  vty_out (vty, "WONT ");
	  break;
	case DO:
	  vty_out (vty, "DO ");
	  break;
	case DONT:
	  vty_out (vty, "DONT ");
	  break;
	case SB:
	  vty_out (vty, "SB ");
	  break;
	case SE:
	  vty_out (vty, "SE ");
	  break;
	case TELOPT_ECHO:
	  vty_out (vty, "TELOPT_ECHO %s", VTY_NEWLINE);
	  break;
	case TELOPT_SGA:
	  vty_out (vty, "TELOPT_SGA %s", VTY_NEWLINE);
	  break;
	case TELOPT_NAWS:
	  vty_out (vty, "TELOPT_NAWS %s", VTY_NEWLINE);
	  break;
	default:
	  vty_out (vty, "%x ", buf[i]);
	  break;
	}
    }
  vty_out (vty, "%s", VTY_NEWLINE);

#endif /* TELNET_OPTION_DEBUG */

  switch (buf[0])
    {
    case SB:
      vty->sb_len = 0;
      vty->iac_sb_in_progress = 1;
      return 0;
      break;
    case SE: 
      {
	if (!vty->iac_sb_in_progress)
	  return 0;

	if ((vty->sb_len == 0) || (vty->sb_buf[0] == '\0'))
	  {
	    vty->iac_sb_in_progress = 0;
	    return 0;
	  }
	switch (vty->sb_buf[0])
	  {
	  case TELOPT_NAWS:
	    if (vty->sb_len != TELNET_NAWS_SB_LEN)
	      zlog_warn("RFC 1073 violation detected: telnet NAWS option "
			"should send %d characters, but we received %lu",
			TELNET_NAWS_SB_LEN, (u_long)vty->sb_len);
	    else if (sizeof(vty->sb_buf) < TELNET_NAWS_SB_LEN)
	      zlog_err("Bug detected: sizeof(vty->sb_buf) %lu < %d, "
		       "too small to handle the telnet NAWS option",
		       (u_long)sizeof(vty->sb_buf), TELNET_NAWS_SB_LEN);
	    else
	      {
		vty->width = ((vty->sb_buf[1] << 8)|vty->sb_buf[2]);
		vty->height = ((vty->sb_buf[3] << 8)|vty->sb_buf[4]);
#ifdef TELNET_OPTION_DEBUG
		vty_out(vty, "TELNET NAWS window size negotiation completed: "
			      "width %d, height %d%s",
			vty->width, vty->height, VTY_NEWLINE);
#endif
	      }
	    break;
	  }
	vty->iac_sb_in_progress = 0;
	return 0;
	break;
      }
    default:
      break;
    }
  return 1;
}

/* Execute current command line. */
static int
vty_execute (struct vty *vty)
{
  int ret;

  ret = CMD_SUCCESS;

  switch (vty->node)
    {
    case AUTH_NODE:
    case AUTH_ENABLE_NODE:
      vty_auth (vty, vty->buf);
      break;
    default:
      ret = vty_command (vty, vty->buf);
      if (vty->type == VTY_TERM)
	vty_hist_add (vty);
      break;
    }

  /* Clear command line buffer. */
  vty->cp = vty->length = 0;
  vty_clear_buf (vty);

  if (vty->status != VTY_CLOSE )
    vty_prompt (vty);

  return ret;
}

#define CONTROL(X)  ((X) - '@')
#define VTY_NORMAL     0
#define VTY_PRE_ESCAPE 1
#define VTY_ESCAPE     2

/* Escape character command map. */
static void
vty_escape_map (unsigned char c, struct vty *vty)
{
  switch (c)
    {
    case ('A'):
      vty_previous_line (vty);
      break;
    case ('B'):
      vty_next_line (vty);
      break;
    case ('C'):
      vty_forward_char (vty);
      break;
    case ('D'):
      vty_backward_char (vty);
      break;
    default:
      break;
    }

  /* Go back to normal mode. */
  vty->escape = VTY_NORMAL;
}

/* Quit print out to the buffer. */
static void
vty_buffer_reset (struct vty *vty)
{
  buffer_reset (vty->obuf);
  vty_prompt (vty);
  vty_redraw_line (vty);
}

/* Read data via vty socket. */
static int
vty_read (struct thread *thread)
{
  int i;
  int nbytes;
  unsigned char buf[VTY_READ_BUFSIZ];

  int vty_sock = THREAD_FD (thread);
  struct vty *vty = THREAD_ARG (thread);
  vty->t_read = NULL;

  /* Read raw data from socket */
  if ((nbytes = read (vty->fd, buf, VTY_READ_BUFSIZ)) <= 0)
    {
      if (nbytes < 0)
	{
	  if (ERRNO_IO_RETRY(errno))
	    {
	      vty_event (VTY_READ, vty_sock, vty);
	      return 0;
	    }
	  vty->monitor = 0; /* disable monitoring to avoid infinite recursion */
	  zlog_warn("%s: read error on vty client fd %d, closing: %s",
		    __func__, vty->fd, safe_strerror(errno));
	}
      buffer_reset(vty->obuf);
      vty->status = VTY_CLOSE;
    }

  for (i = 0; i < nbytes; i++) 
    {
      if (buf[i] == IAC)
	{
	  if (!vty->iac)
	    {
	      vty->iac = 1;
	      continue;
	    }
	  else
	    {
	      vty->iac = 0;
	    }
	}
      
      if (vty->iac_sb_in_progress && !vty->iac)
	{
	    if (vty->sb_len < sizeof(vty->sb_buf))
	      vty->sb_buf[vty->sb_len] = buf[i];
	    vty->sb_len++;
	    continue;
	}

      if (vty->iac)
	{
	  /* In case of telnet command */
	  int ret = 0;
	  ret = vty_telnet_option (vty, buf + i, nbytes - i);
	  vty->iac = 0;
	  i += ret;
	  continue;
	}
	        

      if (vty->status == VTY_MORE)
	{
	  switch (buf[i])
	    {
	    case CONTROL('C'):
	    case 'q':
	    case 'Q':
	      vty_buffer_reset (vty);
	      break;
#if 0 /* More line does not work for "show ip bgp".  */
	    case '\n':
	    case '\r':
	      vty->status = VTY_MORELINE;
	      break;
#endif
	    default:
	      break;
	    }
	  continue;
	}

      /* Escape character. */
      if (vty->escape == VTY_ESCAPE)
	{
	  vty_escape_map (buf[i], vty);
	  continue;
	}

      /* Pre-escape status. */
      if (vty->escape == VTY_PRE_ESCAPE)
	{
	  switch (buf[i])
	    {
	    case '[':
	      vty->escape = VTY_ESCAPE;
	      break;
	    case 'b':
	      vty_backward_word (vty);
	      vty->escape = VTY_NORMAL;
	      break;
	    case 'f':
	      vty_forward_word (vty);
	      vty->escape = VTY_NORMAL;
	      break;
	    case 'd':
	      vty_forward_kill_word (vty);
	      vty->escape = VTY_NORMAL;
	      break;
	    case CONTROL('H'):
	    case 0x7f:
	      vty_backward_kill_word (vty);
	      vty->escape = VTY_NORMAL;
	      break;
	    default:
	      vty->escape = VTY_NORMAL;
	      break;
	    }
	  continue;
	}

      switch (buf[i])
	{
	case CONTROL('A'):
	  vty_beginning_of_line (vty);
	  break;
	case CONTROL('B'):
	  vty_backward_char (vty);
	  break;
	case CONTROL('C'):
	  vty_stop_input (vty);
	  break;
	case CONTROL('D'):
	  vty_delete_char (vty);
	  break;
	case CONTROL('E'):
	  vty_end_of_line (vty);
	  break;
	case CONTROL('F'):
	  vty_forward_char (vty);
	  break;
	case CONTROL('H'):
	case 0x7f:
	  vty_delete_backward_char (vty);
	  break;
	case CONTROL('K'):
	  vty_kill_line (vty);
	  break;
	case CONTROL('N'):
	  vty_next_line (vty);
	  break;
	case CONTROL('P'):
	  vty_previous_line (vty);
	  break;
	case CONTROL('T'):
	  vty_transpose_chars (vty);
	  break;
	case CONTROL('U'):
	  vty_kill_line_from_beginning (vty);
	  break;
	case CONTROL('W'):
	  vty_backward_kill_word (vty);
	  break;
	case CONTROL('Z'):
	  vty_end_config (vty);
	  break;
	case '\n':
	case '\r':
	  vty_out (vty, "%s", VTY_NEWLINE);
	  vty_execute (vty);
	  break;
	case '\t':
	  vty_complete_command (vty);
	  break;
	case '?':
	  if (vty->node == AUTH_NODE || vty->node == AUTH_ENABLE_NODE)
	    vty_self_insert (vty, buf[i]);
	  else
	    vty_describe_command (vty);
	  break;
	case '\033':
	  if (i + 1 < nbytes && buf[i + 1] == '[')
	    {
	      vty->escape = VTY_ESCAPE;
	      i++;
	    }
	  else
	    vty->escape = VTY_PRE_ESCAPE;
	  break;
	default:
	  if (buf[i] > 31 && buf[i] < 127)
	    vty_self_insert (vty, buf[i]);
	  break;
	}
    }

  /* Check status. */
  if (vty->status == VTY_CLOSE)
    vty_close (vty);
  else
    {
      vty_event (VTY_WRITE, vty_sock, vty);
      vty_event (VTY_READ, vty_sock, vty);
    }
  return 0;
}

/* Flush buffer to the vty. */
static int
vty_flush (struct thread *thread)
{
  int erase;
  buffer_status_t flushrc;
  int vty_sock = THREAD_FD (thread);
  struct vty *vty = THREAD_ARG (thread);

  vty->t_write = NULL;

  /* Tempolary disable read thread. */
  if ((vty->lines == 0) && vty->t_read)
    {
      thread_cancel (vty->t_read);
      vty->t_read = NULL;
    }

  /* Function execution continue. */
  erase = ((vty->status == VTY_MORE || vty->status == VTY_MORELINE));

  /* N.B. if width is 0, that means we don't know the window size. */
  if ((vty->lines == 0) || (vty->width == 0))
    flushrc = buffer_flush_available(vty->obuf, vty->fd);
  else if (vty->status == VTY_MORELINE)
    flushrc = buffer_flush_window(vty->obuf, vty->fd, vty->width,
				  1, erase, 0);
  else
    flushrc = buffer_flush_window(vty->obuf, vty->fd, vty->width,
				  vty->lines >= 0 ? vty->lines :
						    vty->height,
				  erase, 0);
  switch (flushrc)
    {
    case BUFFER_ERROR:
      vty->monitor = 0; /* disable monitoring to avoid infinite recursion */
      zlog_warn("buffer_flush failed on vty client fd %d, closing",
		vty->fd);
      buffer_reset(vty->obuf);
      vty_close(vty);
      return 0;
    case BUFFER_EMPTY:
      if (vty->status == VTY_CLOSE)
	vty_close (vty);
      else
	{
	  vty->status = VTY_NORMAL;
	  if (vty->lines == 0)
	    vty_event (VTY_READ, vty_sock, vty);
	}
      break;
    case BUFFER_PENDING:
      /* There is more data waiting to be written. */
      vty->status = VTY_MORE;
      if (vty->lines == 0)
	vty_event (VTY_WRITE, vty_sock, vty);
      break;
    }

  return 0;
}

/* Create new vty structure. */
static struct vty *
vty_create (int vty_sock, union sockunion *su)
{
  char buf[SU_ADDRSTRLEN];
  struct vty *vty;

  sockunion2str(su, buf, SU_ADDRSTRLEN);

  /* Allocate new vty structure and set up default values. */
  vty = vty_new ();
  vty->fd = vty_sock;
  vty->type = VTY_TERM;
  strcpy (vty->address, buf);
  if (no_password_check)
    {
      if (restricted_mode)
        vty->node = RESTRICTED_NODE;
      else if (host.advanced)
	vty->node = ENABLE_NODE;
      else
	vty->node = VIEW_NODE;
    }
  else
    vty->node = AUTH_NODE;
  vty->fail = 0;
  vty->cp = 0;
  vty_clear_buf (vty);
  vty->length = 0;
  memset (vty->hist, 0, sizeof (vty->hist));
  vty->hp = 0;
  vty->hindex = 0;
  vector_set_index (vtyvec, vty_sock, vty);
  vty->status = VTY_NORMAL;
  vty->v_timeout = vty_timeout_val;
  if (host.lines >= 0)
    vty->lines = host.lines;
  else
    vty->lines = -1;
  vty->iac = 0;
  vty->iac_sb_in_progress = 0;
  vty->sb_len = 0;

  if (! no_password_check)
    {
      /* Vty is not available if password isn't set. */
      if (host.password == NULL && host.password_encrypt == NULL)
	{
	  vty_out (vty, "Vty password is not set.%s", VTY_NEWLINE);
	  vty->status = VTY_CLOSE;
	  vty_close (vty);
	  return NULL;
	}
    }

  /* Say hello to the world. */
  vty_hello (vty);
  if (! no_password_check)
    vty_out (vty, "%sUser Access Verification%s%s", VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);

  /* Setting up terminal. */
  vty_will_echo (vty);
  vty_will_suppress_go_ahead (vty);

  vty_dont_linemode (vty);
  vty_do_window_size (vty);
  /* vty_dont_lflow_ahead (vty); */

  vty_prompt (vty);

  /* Add read/write thread. */
  vty_event (VTY_WRITE, vty_sock, vty);
  vty_event (VTY_READ, vty_sock, vty);

  return vty;
}

/* Accept connection from the network. */
static int
vty_accept (struct thread *thread)
{
  int vty_sock;
  union sockunion su;
  int ret;
  unsigned int on;
  int accept_sock;
  struct prefix *p = NULL;
  struct access_list *acl = NULL;
  char buf[SU_ADDRSTRLEN];

  accept_sock = THREAD_FD (thread);

  /* We continue hearing vty socket. */
  vty_event (VTY_SERV, accept_sock, NULL);

  memset (&su, 0, sizeof (union sockunion));

  /* We can handle IPv4 or IPv6 socket. */
  vty_sock = sockunion_accept (accept_sock, &su);
  if (vty_sock < 0)
    {
      zlog_warn ("can't accept vty socket : %s", safe_strerror (errno));
      return -1;
    }
  set_nonblocking(vty_sock);

  p = sockunion2hostprefix (&su);

  /* VTY's accesslist apply. */
  if (p->family == AF_INET && vty_accesslist_name)
    {
      if ((acl = access_list_lookup (AFI_IP, vty_accesslist_name)) &&
	  (access_list_apply (acl, p) == FILTER_DENY))
	{
	  zlog (NULL, LOG_INFO, "Vty connection refused from %s",
		sockunion2str (&su, buf, SU_ADDRSTRLEN));
	  close (vty_sock);
	  
	  /* continue accepting connections */
	  vty_event (VTY_SERV, accept_sock, NULL);
	  
	  prefix_free (p);

	  return 0;
	}
    }

#ifdef HAVE_IPV6
  /* VTY's ipv6 accesslist apply. */
  if (p->family == AF_INET6 && vty_ipv6_accesslist_name)
    {
      if ((acl = access_list_lookup (AFI_IP6, vty_ipv6_accesslist_name)) &&
	  (access_list_apply (acl, p) == FILTER_DENY))
	{
	  zlog (NULL, LOG_INFO, "Vty connection refused from %s",
		sockunion2str (&su, buf, SU_ADDRSTRLEN));
	  close (vty_sock);
	  
	  /* continue accepting connections */
	  vty_event (VTY_SERV, accept_sock, NULL);
	  
	  prefix_free (p);

	  return 0;
	}
    }
#endif /* HAVE_IPV6 */
  
  prefix_free (p);

  on = 1;
  ret = setsockopt (vty_sock, IPPROTO_TCP, TCP_NODELAY, 
		    (char *) &on, sizeof (on));
  if (ret < 0)
    zlog (NULL, LOG_INFO, "can't set sockopt to vty_sock : %s", 
	  safe_strerror (errno));

  zlog (NULL, LOG_INFO, "Vty connection from %s",
	sockunion2str (&su, buf, SU_ADDRSTRLEN));

  vty_create (vty_sock, &su);

  return 0;
}

#ifdef HAVE_IPV6
static void
vty_serv_sock_addrinfo (const char *hostname, unsigned short port)
{
  int ret;
  struct addrinfo req;
  struct addrinfo *ainfo;
  struct addrinfo *ainfo_save;
  int sock;
  char port_str[BUFSIZ];

  memset (&req, 0, sizeof (struct addrinfo));
  req.ai_flags = AI_PASSIVE;
  req.ai_family = AF_UNSPEC;
  req.ai_socktype = SOCK_STREAM;
  sprintf (port_str, "%d", port);
  port_str[sizeof (port_str) - 1] = '\0';

  ret = getaddrinfo (hostname, port_str, &req, &ainfo);

  if (ret != 0)
    {
      fprintf (stderr, "getaddrinfo failed: %s\n", gai_strerror (ret));
      exit (1);
    }

  ainfo_save = ainfo;

  do
    {
      if (ainfo->ai_family != AF_INET
#ifdef HAVE_IPV6
	  && ainfo->ai_family != AF_INET6
#endif /* HAVE_IPV6 */
	  )
	continue;

      sock = socket (ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
      if (sock < 0)
	continue;

      sockopt_v6only (ainfo->ai_family, sock);
      sockopt_reuseaddr (sock);
      sockopt_reuseport (sock);

      ret = bind (sock, ainfo->ai_addr, ainfo->ai_addrlen);
      if (ret < 0)
	{
	  close (sock);	/* Avoid sd leak. */
	continue;
	}

      ret = listen (sock, 3);
      if (ret < 0) 
	{
	  close (sock);	/* Avoid sd leak. */
	continue;
	}

      vty_event (VTY_SERV, sock, NULL);
    }
  while ((ainfo = ainfo->ai_next) != NULL);

  freeaddrinfo (ainfo_save);
}
#else /* HAVE_IPV6 */

/* Make vty server socket. */
static void
vty_serv_sock_family (const char* addr, unsigned short port, int family)
{
  int ret;
  union sockunion su;
  int accept_sock;
  void* naddr=NULL;

  memset (&su, 0, sizeof (union sockunion));
  su.sa.sa_family = family;
  if(addr)
    switch(family)
    {
      case AF_INET:
        naddr=&su.sin.sin_addr;
        break;
#ifdef HAVE_IPV6
      case AF_INET6:
        naddr=&su.sin6.sin6_addr;
        break;
#endif	
    }

  if(naddr)
    switch(inet_pton(family,addr,naddr))
    {
      case -1:
        zlog_err("bad address %s",addr);
	naddr=NULL;
	break;
      case 0:
        zlog_err("error translating address %s: %s",addr,safe_strerror(errno));
	naddr=NULL;
    }

  /* Make new socket. */
  accept_sock = sockunion_stream_socket (&su);
  if (accept_sock < 0)
    return;

  /* This is server, so reuse address. */
  sockopt_reuseaddr (accept_sock);
  sockopt_reuseport (accept_sock);

  /* Bind socket to universal address and given port. */
  ret = sockunion_bind (accept_sock, &su, port, naddr);
  if (ret < 0)
    {
      zlog_warn("can't bind socket");
      close (accept_sock);	/* Avoid sd leak. */
      return;
    }

  /* Listen socket under queue 3. */
  ret = listen (accept_sock, 3);
  if (ret < 0) 
    {
      zlog (NULL, LOG_WARNING, "can't listen socket");
      close (accept_sock);	/* Avoid sd leak. */
      return;
    }

  /* Add vty server event. */
  vty_event (VTY_SERV, accept_sock, NULL);
}
#endif /* HAVE_IPV6 */

#ifdef VTYSH
/* For sockaddr_un. */
#include <sys/un.h>

/* VTY shell UNIX domain socket. */
static void
vty_serv_un (const char *path)
{
  int ret;
  int sock, len;
  struct sockaddr_un serv;
  mode_t old_mask;
  struct zprivs_ids_t ids;
  
  /* First of all, unlink existing socket */
  unlink (path);

  /* Set umask */
  old_mask = umask (0007);

  /* Make UNIX domain socket. */
  sock = socket (AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0)
    {
      zlog_err("Cannot create unix stream socket: %s", safe_strerror(errno));
      return;
    }

  /* Make server socket. */
  memset (&serv, 0, sizeof (struct sockaddr_un));
  serv.sun_family = AF_UNIX;
  strncpy (serv.sun_path, path, strlen (path));
#ifdef HAVE_STRUCT_SOCKADDR_UN_SUN_LEN
  len = serv.sun_len = SUN_LEN(&serv);
#else
  len = sizeof (serv.sun_family) + strlen (serv.sun_path);
#endif /* HAVE_STRUCT_SOCKADDR_UN_SUN_LEN */

  ret = bind (sock, (struct sockaddr *) &serv, len);
  if (ret < 0)
    {
      zlog_err("Cannot bind path %s: %s", path, safe_strerror(errno));
      close (sock);	/* Avoid sd leak. */
      return;
    }

  ret = listen (sock, 5);
  if (ret < 0)
    {
      zlog_err("listen(fd %d) failed: %s", sock, safe_strerror(errno));
      close (sock);	/* Avoid sd leak. */
      return;
    }

  umask (old_mask);

  zprivs_get_ids(&ids);
  
  if (ids.gid_vty > 0)
    {
      /* set group of socket */
      if ( chown (path, -1, ids.gid_vty) )
        {
          zlog_err ("vty_serv_un: could chown socket, %s",
                     safe_strerror (errno) );
        }
    }

  vty_event (VTYSH_SERV, sock, NULL);
}

/* #define VTYSH_DEBUG 1 */

static int
vtysh_accept (struct thread *thread)
{
  int accept_sock;
  int sock;
  int client_len;
  struct sockaddr_un client;
  struct vty *vty;
  
  accept_sock = THREAD_FD (thread);

  vty_event (VTYSH_SERV, accept_sock, NULL);

  memset (&client, 0, sizeof (struct sockaddr_un));
  client_len = sizeof (struct sockaddr_un);

  sock = accept (accept_sock, (struct sockaddr *) &client,
		 (socklen_t *) &client_len);

  if (sock < 0)
    {
      zlog_warn ("can't accept vty socket : %s", safe_strerror (errno));
      return -1;
    }

  if (set_nonblocking(sock) < 0)
    {
      zlog_warn ("vtysh_accept: could not set vty socket %d to non-blocking,"
                 " %s, closing", sock, safe_strerror (errno));
      close (sock);
      return -1;
    }
  
#ifdef VTYSH_DEBUG
  printf ("VTY shell accept\n");
#endif /* VTYSH_DEBUG */

  vty = vty_new ();
  vty->fd = sock;
  vty->type = VTY_SHELL_SERV;
  vty->node = VIEW_NODE;

  vty_event (VTYSH_READ, sock, vty);

  return 0;
}

static int
vtysh_flush(struct vty *vty)
{
  switch (buffer_flush_available(vty->obuf, vty->fd))
    {
    case BUFFER_PENDING:
      vty_event(VTYSH_WRITE, vty->fd, vty);
      break;
    case BUFFER_ERROR:
      vty->monitor = 0; /* disable monitoring to avoid infinite recursion */
      zlog_warn("%s: write error to fd %d, closing", __func__, vty->fd);
      buffer_reset(vty->obuf);
      vty_close(vty);
      return -1;
      break;
    case BUFFER_EMPTY:
      break;
    }
  return 0;
}

static int
vtysh_read (struct thread *thread)
{
  int ret;
  int sock;
  int nbytes;
  struct vty *vty;
  unsigned char buf[VTY_READ_BUFSIZ];
  unsigned char *p;
  u_char header[4] = {0, 0, 0, 0};

  sock = THREAD_FD (thread);
  vty = THREAD_ARG (thread);
  vty->t_read = NULL;

  if ((nbytes = read (sock, buf, VTY_READ_BUFSIZ)) <= 0)
    {
      if (nbytes < 0)
	{
	  if (ERRNO_IO_RETRY(errno))
	    {
	      vty_event (VTYSH_READ, sock, vty);
	      return 0;
	    }
	  vty->monitor = 0; /* disable monitoring to avoid infinite recursion */
	  zlog_warn("%s: read failed on vtysh client fd %d, closing: %s",
		    __func__, sock, safe_strerror(errno));
	}
      buffer_reset(vty->obuf);
      vty_close (vty);
#ifdef VTYSH_DEBUG
      printf ("close vtysh\n");
#endif /* VTYSH_DEBUG */
      return 0;
    }

#ifdef VTYSH_DEBUG
  printf ("line: %.*s\n", nbytes, buf);
#endif /* VTYSH_DEBUG */

  for (p = buf; p < buf+nbytes; p++)
    {
      vty_ensure(vty, vty->length+1);
      vty->buf[vty->length++] = *p;
      if (*p == '\0')
	{
	  /* Pass this line to parser. */
	  ret = vty_execute (vty);
	  /* Note that vty_execute clears the command buffer and resets
	     vty->length to 0. */

	  /* Return result. */
#ifdef VTYSH_DEBUG
	  printf ("result: %d\n", ret);
	  printf ("vtysh node: %d\n", vty->node);
#endif /* VTYSH_DEBUG */

	  header[3] = ret;
	  buffer_put(vty->obuf, header, 4);

	  if (!vty->t_write && (vtysh_flush(vty) < 0))
	    /* Try to flush results; exit if a write error occurs. */
	    return 0;
	}
    }

  vty_event (VTYSH_READ, sock, vty);

  return 0;
}

static int
vtysh_write (struct thread *thread)
{
  struct vty *vty = THREAD_ARG (thread);

  vty->t_write = NULL;
  vtysh_flush(vty);
  return 0;
}

#endif /* VTYSH */

/* Determine address family to bind. */
void
vty_serv_sock (const char *addr, unsigned short port, const char *path)
{
  /* If port is set to 0, do not listen on TCP/IP at all! */
  if (port)
    {

#ifdef HAVE_IPV6
      vty_serv_sock_addrinfo (addr, port);
#else /* ! HAVE_IPV6 */
      vty_serv_sock_family (addr,port, AF_INET);
#endif /* HAVE_IPV6 */
    }

#ifdef VTYSH
  vty_serv_un (path);
#endif /* VTYSH */
}

/* Close vty interface.  Warning: call this only from functions that
   will be careful not to access the vty afterwards (since it has
   now been freed).  This is safest from top-level functions (called
   directly by the thread dispatcher). */
void
vty_close (struct vty *vty)
{
  int i;

  /* Cancel threads.*/
  if (vty->t_read)
    thread_cancel (vty->t_read);
  if (vty->t_write)
    thread_cancel (vty->t_write);
  if (vty->t_timeout)
    thread_cancel (vty->t_timeout);

  /* Flush buffer. */
  buffer_flush_all (vty->obuf, vty->fd);

  /* Free input buffer. */
  buffer_free (vty->obuf);

  /* Free command history. */
  for (i = 0; i < VTY_MAXHIST; i++)
    if (vty->hist[i])
      XFREE (MTYPE_VTY_HIST, vty->hist[i]);

  /* Unset vector. */
  vector_unset (vtyvec, vty->fd);

  /* Close socket. */
  if (vty->fd > 0)
    close (vty->fd);

  if (vty->buf)
    XFREE (MTYPE_VTY, vty->buf);

  /* Check configure. */
  vty_config_unlock (vty);

  /* OK free vty. */
  XFREE (MTYPE_VTY, vty);
}

/* When time out occur output message then close connection. */
static int
vty_timeout (struct thread *thread)
{
  struct vty *vty;

  vty = THREAD_ARG (thread);
  vty->t_timeout = NULL;
  vty->v_timeout = 0;

  /* Clear buffer*/
  buffer_reset (vty->obuf);
  vty_out (vty, "%sVty connection is timed out.%s", VTY_NEWLINE, VTY_NEWLINE);

  /* Close connection. */
  vty->status = VTY_CLOSE;
  vty_close (vty);

  return 0;
}

/* Read up configuration file from file_name. */
static void
vty_read_file (FILE *confp)
{
  int ret;
  struct vty *vty;
  unsigned int line_num = 0;

  vty = vty_new ();
  vty->fd = dup(STDERR_FILENO); /* vty_close() will close this */
  if (vty->fd < 0)
  {
    /* Fine, we couldn't make a new fd. vty_close doesn't close stdout. */
    vty->fd = STDOUT_FILENO;
  }
  vty->type = VTY_FILE;
  vty->node = CONFIG_NODE;
  
  /* Execute configuration file */
  ret = config_from_file (vty, confp, &line_num);

  /* Flush any previous errors before printing messages below */
  buffer_flush_all (vty->obuf, vty->fd);

  if ( !((ret == CMD_SUCCESS) || (ret == CMD_ERR_NOTHING_TODO)) ) 
    {
      switch (ret)
       {
         case CMD_ERR_AMBIGUOUS:
           fprintf (stderr, "*** Error reading config: Ambiguous command.\n");
           break;
         case CMD_ERR_NO_MATCH:
           fprintf (stderr, "*** Error reading config: There is no such command.\n");
           break;
       }
      fprintf (stderr, "*** Error occured processing line %u, below:\n%s\n",
		       line_num, vty->buf);
      vty_close (vty);
      exit (1);
    }

  vty_close (vty);
}

static FILE *
vty_use_backup_config (char *fullpath)
{
  char *fullpath_sav, *fullpath_tmp;
  FILE *ret = NULL;
  struct stat buf;
  int tmp, sav;
  int c;
  char buffer[512];
  
  fullpath_sav = malloc (strlen (fullpath) + strlen (CONF_BACKUP_EXT) + 1);
  strcpy (fullpath_sav, fullpath);
  strcat (fullpath_sav, CONF_BACKUP_EXT);
  if (stat (fullpath_sav, &buf) == -1)
    {
      free (fullpath_sav);
      return NULL;
    }

  fullpath_tmp = malloc (strlen (fullpath) + 8);
  sprintf (fullpath_tmp, "%s.XXXXXX", fullpath);
  
  /* Open file to configuration write. */
  tmp = mkstemp (fullpath_tmp);
  if (tmp < 0)
    {
      free (fullpath_sav);
      free (fullpath_tmp);
      return NULL;
    }

  sav = open (fullpath_sav, O_RDONLY);
  if (sav < 0)
    {
      unlink (fullpath_tmp);
      free (fullpath_sav);
      free (fullpath_tmp);
      return NULL;
    }
  
  while((c = read (sav, buffer, 512)) > 0)
    write (tmp, buffer, c);
  
  close (sav);
  close (tmp);
  
  if (chmod(fullpath_tmp, CONFIGFILE_MASK) != 0)
    {
      unlink (fullpath_tmp);
      free (fullpath_sav);
      free (fullpath_tmp);
      return NULL;
    }
  
  if (link (fullpath_tmp, fullpath) == 0)
    ret = fopen (fullpath, "r");

  unlink (fullpath_tmp);
  
  free (fullpath_sav);
  free (fullpath_tmp);
  return ret;
}

/* Read up configuration file from file_name. */
void
vty_read_config (char *config_file,
                 char *config_default_dir)
{
  char cwd[MAXPATHLEN];
  FILE *confp = NULL;
  char *fullpath;
  char *tmp = NULL;

  /* If -f flag specified. */
  if (config_file != NULL)
    {
      if (! IS_DIRECTORY_SEP (config_file[0]))
        {
          getcwd (cwd, MAXPATHLEN);
          tmp = XMALLOC (MTYPE_TMP, 
 			      strlen (cwd) + strlen (config_file) + 2);
          sprintf (tmp, "%s/%s", cwd, config_file);
          fullpath = tmp;
        }
      else
        fullpath = config_file;

      confp = fopen (fullpath, "r");

      if (confp == NULL)
        {
          fprintf (stderr, "%s: failed to open configuration file %s: %s\n",
                   __func__, fullpath, safe_strerror (errno));
          
          confp = vty_use_backup_config (fullpath);
          if (confp)
            fprintf (stderr, "WARNING: using backup configuration file!\n");
          else
            {
              fprintf (stderr, "can't open configuration file [%s]\n", 
  	               config_file);
              exit(1);
            }
        }
    }
  else
    {
#ifdef VTYSH
      int ret;
      struct stat conf_stat;

      /* !!!!PLEASE LEAVE!!!!
       * This is NEEDED for use with vtysh -b, or else you can get
       * a real configuration food fight with a lot garbage in the
       * merged configuration file it creates coming from the per
       * daemon configuration files.  This also allows the daemons
       * to start if there default configuration file is not
       * present or ignore them, as needed when using vtysh -b to
       * configure the daemons at boot - MAG
       */

      /* Stat for vtysh Zebra.conf, if found startup and wait for
       * boot configuration
       */

      if ( strstr(config_default_dir, "vtysh") == NULL)
        {
          ret = stat (integrate_default, &conf_stat);
          if (ret >= 0)
            return;
        }
#endif /* VTYSH */

      confp = fopen (config_default_dir, "r");
      if (confp == NULL)
        {
          fprintf (stderr, "%s: failed to open configuration file %s: %s\n",
                   __func__, config_default_dir, safe_strerror (errno));
          
          confp = vty_use_backup_config (config_default_dir);
          if (confp)
            {
              fprintf (stderr, "WARNING: using backup configuration file!\n");
              fullpath = config_default_dir;
            }
          else
            {
              fprintf (stderr, "can't open configuration file [%s]\n",
  		                 config_default_dir);
  	          exit (1);
            }
        }      
      else
        fullpath = config_default_dir;
    }

  vty_read_file (confp);

  fclose (confp);

  host_config_set (fullpath);
  
  if (tmp)
    XFREE (MTYPE_TMP, fullpath);
}

/* Small utility function which output log to the VTY. */
void
vty_log (const char *level, const char *proto_str,
	 const char *format, struct timestamp_control *ctl, va_list va)
{
  unsigned int i;
  struct vty *vty;
  
  if (!vtyvec)
    return;

  for (i = 0; i < vector_active (vtyvec); i++)
    if ((vty = vector_slot (vtyvec, i)) != NULL)
      if (vty->monitor)
	{
	  va_list ac;
	  va_copy(ac, va);
	  vty_log_out (vty, level, proto_str, format, ctl, ac);
	  va_end(ac);
	}
}

/* Async-signal-safe version of vty_log for fixed strings. */
void
vty_log_fixed (char *buf, size_t len)
{
  unsigned int i;
  struct iovec iov[2];

  /* vty may not have been initialised */
  if (!vtyvec)
    return;
  
  iov[0].iov_base = buf;
  iov[0].iov_len = len;
  iov[1].iov_base = (void *)"\r\n";
  iov[1].iov_len = 2;

  for (i = 0; i < vector_active (vtyvec); i++)
    {
      struct vty *vty;
      if (((vty = vector_slot (vtyvec, i)) != NULL) && vty->monitor)
	/* N.B. We don't care about the return code, since process is
	   most likely just about to die anyway. */
	writev(vty->fd, iov, 2);
    }
}

int
vty_config_lock (struct vty *vty)
{
  if (vty_config == 0)
    {
      vty->config = 1;
      vty_config = 1;
    }
  return vty->config;
}

int
vty_config_unlock (struct vty *vty)
{
  if (vty_config == 1 && vty->config == 1)
    {
      vty->config = 0;
      vty_config = 0;
    }
  return vty->config;
}

/* Master of the threads. */
static struct thread_master *master;

static void
vty_event (enum event event, int sock, struct vty *vty)
{
  struct thread *vty_serv_thread;

  switch (event)
    {
    case VTY_SERV:
      vty_serv_thread = thread_add_read (master, vty_accept, vty, sock);
      vector_set_index (Vvty_serv_thread, sock, vty_serv_thread);
      break;
#ifdef VTYSH
    case VTYSH_SERV:
      vty_serv_thread = thread_add_read (master, vtysh_accept, vty, sock);
      vector_set_index (Vvty_serv_thread, sock, vty_serv_thread);
      break;
    case VTYSH_READ:
      vty->t_read = thread_add_read (master, vtysh_read, vty, sock);
      break;
    case VTYSH_WRITE:
      vty->t_write = thread_add_write (master, vtysh_write, vty, sock);
      break;
#endif /* VTYSH */
    case VTY_READ:
      vty->t_read = thread_add_read (master, vty_read, vty, sock);

      /* Time out treatment. */
      if (vty->v_timeout)
	{
	  if (vty->t_timeout)
	    thread_cancel (vty->t_timeout);
	  vty->t_timeout = 
	    thread_add_timer (master, vty_timeout, vty, vty->v_timeout);
	}
      break;
    case VTY_WRITE:
      if (! vty->t_write)
	vty->t_write = thread_add_write (master, vty_flush, vty, sock);
      break;
    case VTY_TIMEOUT_RESET:
      if (vty->t_timeout)
	{
	  thread_cancel (vty->t_timeout);
	  vty->t_timeout = NULL;
	}
      if (vty->v_timeout)
	{
	  vty->t_timeout = 
	    thread_add_timer (master, vty_timeout, vty, vty->v_timeout);
	}
      break;
    }
}

DEFUN (config_who,
       config_who_cmd,
       "who",
       "Display who is on vty\n")
{
  unsigned int i;
  struct vty *v;

  for (i = 0; i < vector_active (vtyvec); i++)
    if ((v = vector_slot (vtyvec, i)) != NULL)
      vty_out (vty, "%svty[%d] connected from %s.%s",
	       v->config ? "*" : " ",
	       i, v->address, VTY_NEWLINE);
  return CMD_SUCCESS;
}

/* Move to vty configuration mode. */
DEFUN (line_vty,
       line_vty_cmd,
       "line vty",
       "Configure a terminal line\n"
       "Virtual terminal\n")
{
  vty->node = VTY_NODE;
  return CMD_SUCCESS;
}

/* Set time out value. */
static int
exec_timeout (struct vty *vty, const char *min_str, const char *sec_str)
{
  unsigned long timeout = 0;

  /* min_str and sec_str are already checked by parser.  So it must be
     all digit string. */
  if (min_str)
    {
      timeout = strtol (min_str, NULL, 10);
      timeout *= 60;
    }
  if (sec_str)
    timeout += strtol (sec_str, NULL, 10);

  vty_timeout_val = timeout;
  vty->v_timeout = timeout;
  vty_event (VTY_TIMEOUT_RESET, 0, vty);


  return CMD_SUCCESS;
}

DEFUN (exec_timeout_min,
       exec_timeout_min_cmd,
       "exec-timeout <0-35791>",
       "Set timeout value\n"
       "Timeout value in minutes\n")
{
  return exec_timeout (vty, argv[0], NULL);
}

DEFUN (exec_timeout_sec,
       exec_timeout_sec_cmd,
       "exec-timeout <0-35791> <0-2147483>",
       "Set the EXEC timeout\n"
       "Timeout in minutes\n"
       "Timeout in seconds\n")
{
  return exec_timeout (vty, argv[0], argv[1]);
}

DEFUN (no_exec_timeout,
       no_exec_timeout_cmd,
       "no exec-timeout",
       NO_STR
       "Set the EXEC timeout\n")
{
  return exec_timeout (vty, NULL, NULL);
}

/* Set vty access class. */
DEFUN (vty_access_class,
       vty_access_class_cmd,
       "access-class WORD",
       "Filter connections based on an IP access list\n"
       "IP access list\n")
{
  if (vty_accesslist_name)
    XFREE(MTYPE_VTY, vty_accesslist_name);

  vty_accesslist_name = XSTRDUP(MTYPE_VTY, argv[0]);

  return CMD_SUCCESS;
}

/* Clear vty access class. */
DEFUN (no_vty_access_class,
       no_vty_access_class_cmd,
       "no access-class [WORD]",
       NO_STR
       "Filter connections based on an IP access list\n"
       "IP access list\n")
{
  if (! vty_accesslist_name || (argc && strcmp(vty_accesslist_name, argv[0])))
    {
      vty_out (vty, "Access-class is not currently applied to vty%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  XFREE(MTYPE_VTY, vty_accesslist_name);

  vty_accesslist_name = NULL;

  return CMD_SUCCESS;
}

#ifdef HAVE_IPV6
/* Set vty access class. */
DEFUN (vty_ipv6_access_class,
       vty_ipv6_access_class_cmd,
       "ipv6 access-class WORD",
       IPV6_STR
       "Filter connections based on an IP access list\n"
       "IPv6 access list\n")
{
  if (vty_ipv6_accesslist_name)
    XFREE(MTYPE_VTY, vty_ipv6_accesslist_name);

  vty_ipv6_accesslist_name = XSTRDUP(MTYPE_VTY, argv[0]);

  return CMD_SUCCESS;
}

/* Clear vty access class. */
DEFUN (no_vty_ipv6_access_class,
       no_vty_ipv6_access_class_cmd,
       "no ipv6 access-class [WORD]",
       NO_STR
       IPV6_STR
       "Filter connections based on an IP access list\n"
       "IPv6 access list\n")
{
  if (! vty_ipv6_accesslist_name ||
      (argc && strcmp(vty_ipv6_accesslist_name, argv[0])))
    {
      vty_out (vty, "IPv6 access-class is not currently applied to vty%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  XFREE(MTYPE_VTY, vty_ipv6_accesslist_name);

  vty_ipv6_accesslist_name = NULL;

  return CMD_SUCCESS;
}
#endif /* HAVE_IPV6 */

/* vty login. */
DEFUN (vty_login,
       vty_login_cmd,
       "login",
       "Enable password checking\n")
{
  no_password_check = 0;
  return CMD_SUCCESS;
}

DEFUN (no_vty_login,
       no_vty_login_cmd,
       "no login",
       NO_STR
       "Enable password checking\n")
{
  no_password_check = 1;
  return CMD_SUCCESS;
}

/* initial mode. */
DEFUN (vty_restricted_mode,
       vty_restricted_mode_cmd,
       "anonymous restricted",
       "Restrict view commands available in anonymous, unauthenticated vty\n")
{
  restricted_mode = 1;
  return CMD_SUCCESS;
}

DEFUN (vty_no_restricted_mode,
       vty_no_restricted_mode_cmd,
       "no anonymous restricted",
       NO_STR
       "Enable password checking\n")
{
  restricted_mode = 0;
  return CMD_SUCCESS;
}

DEFUN (service_advanced_vty,
       service_advanced_vty_cmd,
       "service advanced-vty",
       "Set up miscellaneous service\n"
       "Enable advanced mode vty interface\n")
{
  host.advanced = 1;
  return CMD_SUCCESS;
}

DEFUN (no_service_advanced_vty,
       no_service_advanced_vty_cmd,
       "no service advanced-vty",
       NO_STR
       "Set up miscellaneous service\n"
       "Enable advanced mode vty interface\n")
{
  host.advanced = 0;
  return CMD_SUCCESS;
}

DEFUN (terminal_monitor,
       terminal_monitor_cmd,
       "terminal monitor",
       "Set terminal line parameters\n"
       "Copy debug output to the current terminal line\n")
{
  vty->monitor = 1;
  return CMD_SUCCESS;
}

DEFUN (terminal_no_monitor,
       terminal_no_monitor_cmd,
       "terminal no monitor",
       "Set terminal line parameters\n"
       NO_STR
       "Copy debug output to the current terminal line\n")
{
  vty->monitor = 0;
  return CMD_SUCCESS;
}

ALIAS (terminal_no_monitor,
       no_terminal_monitor_cmd,
       "no terminal monitor",
       NO_STR
       "Set terminal line parameters\n"
       "Copy debug output to the current terminal line\n")

DEFUN (show_history,
       show_history_cmd,
       "show history",
       SHOW_STR
       "Display the session command history\n")
{
  int index;

  for (index = vty->hindex + 1; index != vty->hindex;)
    {
      if (index == VTY_MAXHIST)
	{
	  index = 0;
	  continue;
	}

      if (vty->hist[index] != NULL)
	vty_out (vty, "  %s%s", vty->hist[index], VTY_NEWLINE);

      index++;
    }

  return CMD_SUCCESS;
}

/* Display current configuration. */
static int
vty_config_write (struct vty *vty)
{
  vty_out (vty, "line vty%s", VTY_NEWLINE);

  if (vty_accesslist_name)
    vty_out (vty, " access-class %s%s",
	     vty_accesslist_name, VTY_NEWLINE);

  if (vty_ipv6_accesslist_name)
    vty_out (vty, " ipv6 access-class %s%s",
	     vty_ipv6_accesslist_name, VTY_NEWLINE);

  /* exec-timeout */
  if (vty_timeout_val != VTY_TIMEOUT_DEFAULT)
    vty_out (vty, " exec-timeout %ld %ld%s", 
	     vty_timeout_val / 60,
	     vty_timeout_val % 60, VTY_NEWLINE);

  /* login */
  if (no_password_check)
    vty_out (vty, " no login%s", VTY_NEWLINE);
    
  if (restricted_mode != restricted_mode_default)
    {
      if (restricted_mode_default)
        vty_out (vty, " no anonymous restricted%s", VTY_NEWLINE);
      else
        vty_out (vty, " anonymous restricted%s", VTY_NEWLINE);
    }
  
  vty_out (vty, "!%s", VTY_NEWLINE);

  return CMD_SUCCESS;
}

struct cmd_node vty_node =
{
  VTY_NODE,
  "%s(config-line)# ",
  1,
};

/* Reset all VTY status. */
void
vty_reset ()
{
  unsigned int i;
  struct vty *vty;
  struct thread *vty_serv_thread;

  for (i = 0; i < vector_active (vtyvec); i++)
    if ((vty = vector_slot (vtyvec, i)) != NULL)
      {
	buffer_reset (vty->obuf);
	vty->status = VTY_CLOSE;
	vty_close (vty);
      }

  for (i = 0; i < vector_active (Vvty_serv_thread); i++)
    if ((vty_serv_thread = vector_slot (Vvty_serv_thread, i)) != NULL)
      {
	thread_cancel (vty_serv_thread);
	vector_slot (Vvty_serv_thread, i) = NULL;
        close (i);
      }

  vty_timeout_val = VTY_TIMEOUT_DEFAULT;

  if (vty_accesslist_name)
    {
      XFREE(MTYPE_VTY, vty_accesslist_name);
      vty_accesslist_name = NULL;
    }

  if (vty_ipv6_accesslist_name)
    {
      XFREE(MTYPE_VTY, vty_ipv6_accesslist_name);
      vty_ipv6_accesslist_name = NULL;
    }
}

static void
vty_save_cwd (void)
{
  char cwd[MAXPATHLEN];
  char *c;

  c = getcwd (cwd, MAXPATHLEN);

  if (!c)
    {
      chdir (SYSCONFDIR);
      getcwd (cwd, MAXPATHLEN);
    }

  vty_cwd = XMALLOC (MTYPE_TMP, strlen (cwd) + 1);
  strcpy (vty_cwd, cwd);
}

char *
vty_get_cwd ()
{
  return vty_cwd;
}

int
vty_shell (struct vty *vty)
{
  return vty->type == VTY_SHELL ? 1 : 0;
}

int
vty_shell_serv (struct vty *vty)
{
  return vty->type == VTY_SHELL_SERV ? 1 : 0;
}

void
vty_init_vtysh ()
{
  vtyvec = vector_init (VECTOR_MIN_SIZE);
}

/* Install vty's own commands like `who' command. */
void
vty_init (struct thread_master *master_thread)
{
  /* For further configuration read, preserve current directory. */
  vty_save_cwd ();

  vtyvec = vector_init (VECTOR_MIN_SIZE);

  master = master_thread;

  /* Initilize server thread vector. */
  Vvty_serv_thread = vector_init (VECTOR_MIN_SIZE);

  /* Install bgp top node. */
  install_node (&vty_node, vty_config_write);

  install_element (RESTRICTED_NODE, &config_who_cmd);
  install_element (RESTRICTED_NODE, &show_history_cmd);
  install_element (VIEW_NODE, &config_who_cmd);
  install_element (VIEW_NODE, &show_history_cmd);
  install_element (ENABLE_NODE, &config_who_cmd);
  install_element (CONFIG_NODE, &line_vty_cmd);
  install_element (CONFIG_NODE, &service_advanced_vty_cmd);
  install_element (CONFIG_NODE, &no_service_advanced_vty_cmd);
  install_element (CONFIG_NODE, &show_history_cmd);
  install_element (ENABLE_NODE, &terminal_monitor_cmd);
  install_element (ENABLE_NODE, &terminal_no_monitor_cmd);
  install_element (ENABLE_NODE, &no_terminal_monitor_cmd);
  install_element (ENABLE_NODE, &show_history_cmd);

  install_default (VTY_NODE);
  install_element (VTY_NODE, &exec_timeout_min_cmd);
  install_element (VTY_NODE, &exec_timeout_sec_cmd);
  install_element (VTY_NODE, &no_exec_timeout_cmd);
  install_element (VTY_NODE, &vty_access_class_cmd);
  install_element (VTY_NODE, &no_vty_access_class_cmd);
  install_element (VTY_NODE, &vty_login_cmd);
  install_element (VTY_NODE, &no_vty_login_cmd);
  install_element (VTY_NODE, &vty_restricted_mode_cmd);
  install_element (VTY_NODE, &vty_no_restricted_mode_cmd);
#ifdef HAVE_IPV6
  install_element (VTY_NODE, &vty_ipv6_access_class_cmd);
  install_element (VTY_NODE, &no_vty_ipv6_access_class_cmd);
#endif /* HAVE_IPV6 */
}

void
vty_terminate (void)
{
  if (vty_cwd)
    XFREE (MTYPE_TMP, vty_cwd);

  if (vtyvec && Vvty_serv_thread)
    {
      vty_reset ();
      vector_free (vtyvec);
      vector_free (Vvty_serv_thread);
    }
}

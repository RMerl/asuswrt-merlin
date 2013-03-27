/* Remote utility routines for the remote server for GDB.
   Copyright (C) 1986, 1989, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002, 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "server.h"
#include "terminal.h"
#include <stdio.h>
#include <string.h>
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <sys/time.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <sys/stat.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#if USE_WIN32API
#include <winsock.h>
#endif

#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

#if USE_WIN32API
# define INVALID_DESCRIPTOR INVALID_SOCKET
#else
# define INVALID_DESCRIPTOR -1
#endif

/* A cache entry for a successfully looked-up symbol.  */
struct sym_cache
{
  const char *name;
  CORE_ADDR addr;
  struct sym_cache *next;
};

/* The symbol cache.  */
static struct sym_cache *symbol_cache;

/* If this flag has been set, assume cache misses are
   failures.  */
int all_symbols_looked_up;

int remote_debug = 0;
struct ui_file *gdb_stdlog;

static int remote_desc = INVALID_DESCRIPTOR;

/* FIXME headerize? */
extern int using_threads;
extern int debug_threads;

#ifdef USE_WIN32API
# define read(fd, buf, len) recv (fd, (char *) buf, len, 0)
# define write(fd, buf, len) send (fd, (char *) buf, len, 0)
#endif

/* Open a connection to a remote debugger.
   NAME is the filename used for communication.  */

void
remote_open (char *name)
{
#if defined(F_SETFL) && defined (FASYNC)
  int save_fcntl_flags;
#endif
  char *port_str;

  port_str = strchr (name, ':');
  if (port_str == NULL)
    {
#ifdef USE_WIN32API
      error ("Only <host>:<port> is supported on this platform.");
#else
      struct stat statbuf;

      if (stat (name, &statbuf) == 0
	  && (S_ISCHR (statbuf.st_mode) || S_ISFIFO (statbuf.st_mode)))
	remote_desc = open (name, O_RDWR);
      else
	{
	  errno = EINVAL;
	  remote_desc = -1;
	}

      if (remote_desc < 0)
	perror_with_name ("Could not open remote device");

#ifdef HAVE_TERMIOS
      {
	struct termios termios;
	tcgetattr (remote_desc, &termios);

	termios.c_iflag = 0;
	termios.c_oflag = 0;
	termios.c_lflag = 0;
	termios.c_cflag &= ~(CSIZE | PARENB);
	termios.c_cflag |= CLOCAL | CS8;
	termios.c_cc[VMIN] = 1;
	termios.c_cc[VTIME] = 0;

	tcsetattr (remote_desc, TCSANOW, &termios);
      }
#endif

#ifdef HAVE_TERMIO
      {
	struct termio termio;
	ioctl (remote_desc, TCGETA, &termio);

	termio.c_iflag = 0;
	termio.c_oflag = 0;
	termio.c_lflag = 0;
	termio.c_cflag &= ~(CSIZE | PARENB);
	termio.c_cflag |= CLOCAL | CS8;
	termio.c_cc[VMIN] = 1;
	termio.c_cc[VTIME] = 0;

	ioctl (remote_desc, TCSETA, &termio);
      }
#endif

#ifdef HAVE_SGTTY
      {
	struct sgttyb sg;

	ioctl (remote_desc, TIOCGETP, &sg);
	sg.sg_flags = RAW;
	ioctl (remote_desc, TIOCSETP, &sg);
      }
#endif

      fprintf (stderr, "Remote debugging using %s\n", name);
#endif /* USE_WIN32API */
    }
  else
    {
#ifdef USE_WIN32API
      static int winsock_initialized;
#endif
      char *port_str;
      int port;
      struct sockaddr_in sockaddr;
      socklen_t tmp;
      int tmp_desc;

      port_str = strchr (name, ':');

      port = atoi (port_str + 1);

#ifdef USE_WIN32API
      if (!winsock_initialized)
	{
	  WSADATA wsad;

	  WSAStartup (MAKEWORD (1, 0), &wsad);
	  winsock_initialized = 1;
	}
#endif

      tmp_desc = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (tmp_desc < 0)
	perror_with_name ("Can't open socket");

      /* Allow rapid reuse of this port. */
      tmp = 1;
      setsockopt (tmp_desc, SOL_SOCKET, SO_REUSEADDR, (char *) &tmp,
		  sizeof (tmp));

      sockaddr.sin_family = PF_INET;
      sockaddr.sin_port = htons (port);
      sockaddr.sin_addr.s_addr = INADDR_ANY;

      if (bind (tmp_desc, (struct sockaddr *) &sockaddr, sizeof (sockaddr))
	  || listen (tmp_desc, 1))
	perror_with_name ("Can't bind address");

      /* If port is zero, a random port will be selected, and the
	 fprintf below needs to know what port was selected.  */
      if (port == 0)
	{
	  socklen_t len = sizeof (sockaddr);
	  if (getsockname (tmp_desc, (struct sockaddr *) &sockaddr, &len) < 0
	      || len < sizeof (sockaddr))
	    perror_with_name ("Can't determine port");
	  port = ntohs (sockaddr.sin_port);
	}

      fprintf (stderr, "Listening on port %d\n", port);
      fflush (stderr);

      tmp = sizeof (sockaddr);
      remote_desc = accept (tmp_desc, (struct sockaddr *) &sockaddr, &tmp);
      if (remote_desc == -1)
	perror_with_name ("Accept failed");

      /* Enable TCP keep alive process. */
      tmp = 1;
      setsockopt (remote_desc, SOL_SOCKET, SO_KEEPALIVE,
		  (char *) &tmp, sizeof (tmp));

      /* Tell TCP not to delay small packets.  This greatly speeds up
         interactive response. */
      tmp = 1;
      setsockopt (remote_desc, IPPROTO_TCP, TCP_NODELAY,
		  (char *) &tmp, sizeof (tmp));


#ifndef USE_WIN32API
      close (tmp_desc);		/* No longer need this */

      signal (SIGPIPE, SIG_IGN);	/* If we don't do this, then gdbserver simply
					   exits when the remote side dies.  */
#else
      closesocket (tmp_desc);	/* No longer need this */
#endif

      /* Convert IP address to string.  */
      fprintf (stderr, "Remote debugging from host %s\n", 
         inet_ntoa (sockaddr.sin_addr));
    }

#if defined(F_SETFL) && defined (FASYNC)
  save_fcntl_flags = fcntl (remote_desc, F_GETFL, 0);
  fcntl (remote_desc, F_SETFL, save_fcntl_flags | FASYNC);
#if defined (F_SETOWN)
  fcntl (remote_desc, F_SETOWN, getpid ());
#endif
#endif
  disable_async_io ();
}

void
remote_close (void)
{
#ifdef USE_WIN32API
  closesocket (remote_desc);
#else
  close (remote_desc);
#endif
}

/* Convert hex digit A to a number.  */

static int
fromhex (int a)
{
  if (a >= '0' && a <= '9')
    return a - '0';
  else if (a >= 'a' && a <= 'f')
    return a - 'a' + 10;
  else
    error ("Reply contains invalid hex digit");
  return 0;
}

int
unhexify (char *bin, const char *hex, int count)
{
  int i;

  for (i = 0; i < count; i++)
    {
      if (hex[0] == 0 || hex[1] == 0)
        {
          /* Hex string is short, or of uneven length.
             Return the count that has been converted so far. */
          return i;
        }
      *bin++ = fromhex (hex[0]) * 16 + fromhex (hex[1]);
      hex += 2;
    }
  return i;
}

void
decode_address (CORE_ADDR *addrp, const char *start, int len)
{
  CORE_ADDR addr;
  char ch;
  int i;

  addr = 0;
  for (i = 0; i < len; i++)
    {
      ch = start[i];
      addr = addr << 4;
      addr = addr | (fromhex (ch) & 0x0f);
    }
  *addrp = addr;
}

const char *
decode_address_to_semicolon (CORE_ADDR *addrp, const char *start)
{
  const char *end;

  end = start;
  while (*end != '\0' && *end != ';')
    end++;

  decode_address (addrp, start, end - start);

  if (*end == ';')
    end++;
  return end;
}

/* Convert number NIB to a hex digit.  */

static int
tohex (int nib)
{
  if (nib < 10)
    return '0' + nib;
  else
    return 'a' + nib - 10;
}

int
hexify (char *hex, const char *bin, int count)
{
  int i;

  /* May use a length, or a nul-terminated string as input. */
  if (count == 0)
    count = strlen (bin);

  for (i = 0; i < count; i++)
    {
      *hex++ = tohex ((*bin >> 4) & 0xf);
      *hex++ = tohex (*bin++ & 0xf);
    }
  *hex = 0;
  return i;
}

/* Convert BUFFER, binary data at least LEN bytes long, into escaped
   binary data in OUT_BUF.  Set *OUT_LEN to the length of the data
   encoded in OUT_BUF, and return the number of bytes in OUT_BUF
   (which may be more than *OUT_LEN due to escape characters).  The
   total number of bytes in the output buffer will be at most
   OUT_MAXLEN.  */

int
remote_escape_output (const gdb_byte *buffer, int len,
		      gdb_byte *out_buf, int *out_len,
		      int out_maxlen)
{
  int input_index, output_index;

  output_index = 0;
  for (input_index = 0; input_index < len; input_index++)
    {
      gdb_byte b = buffer[input_index];

      if (b == '$' || b == '#' || b == '}' || b == '*')
	{
	  /* These must be escaped.  */
	  if (output_index + 2 > out_maxlen)
	    break;
	  out_buf[output_index++] = '}';
	  out_buf[output_index++] = b ^ 0x20;
	}
      else
	{
	  if (output_index + 1 > out_maxlen)
	    break;
	  out_buf[output_index++] = b;
	}
    }

  *out_len = input_index;
  return output_index;
}

/* Convert BUFFER, escaped data LEN bytes long, into binary data
   in OUT_BUF.  Return the number of bytes written to OUT_BUF.
   Raise an error if the total number of bytes exceeds OUT_MAXLEN.

   This function reverses remote_escape_output.  It allows more
   escaped characters than that function does, in particular because
   '*' must be escaped to avoid the run-length encoding processing
   in reading packets.  */

static int
remote_unescape_input (const gdb_byte *buffer, int len,
		       gdb_byte *out_buf, int out_maxlen)
{
  int input_index, output_index;
  int escaped;

  output_index = 0;
  escaped = 0;
  for (input_index = 0; input_index < len; input_index++)
    {
      gdb_byte b = buffer[input_index];

      if (output_index + 1 > out_maxlen)
	error ("Received too much data from the target.");

      if (escaped)
	{
	  out_buf[output_index++] = b ^ 0x20;
	  escaped = 0;
	}
      else if (b == '}')
	escaped = 1;
      else
	out_buf[output_index++] = b;
    }

  if (escaped)
    error ("Unmatched escape character in target response.");

  return output_index;
}

/* Look for a sequence of characters which can be run-length encoded.
   If there are any, update *CSUM and *P.  Otherwise, output the
   single character.  Return the number of characters consumed.  */

static int
try_rle (char *buf, int remaining, unsigned char *csum, char **p)
{
  int n;

  /* Always output the character.  */
  *csum += buf[0];
  *(*p)++ = buf[0];

  /* Don't go past '~'.  */
  if (remaining > 97)
    remaining = 97;

  for (n = 1; n < remaining; n++)
    if (buf[n] != buf[0])
      break;

  /* N is the index of the first character not the same as buf[0].
     buf[0] is counted twice, so by decrementing N, we get the number
     of characters the RLE sequence will replace.  */
  n--;

  if (n < 3)
    return 1;

  /* Skip the frame characters.  The manual says to skip '+' and '-'
     also, but there's no reason to.  Unfortunately these two unusable
     characters double the encoded length of a four byte zero
     value.  */
  while (n + 29 == '$' || n + 29 == '#')
    n--;

  *csum += '*';
  *(*p)++ = '*';
  *csum += n + 29;
  *(*p)++ = n + 29;

  return n + 1;
}

/* Send a packet to the remote machine, with error checking.
   The data of the packet is in BUF, and the length of the
   packet is in CNT.  Returns >= 0 on success, -1 otherwise.  */

int
putpkt_binary (char *buf, int cnt)
{
  int i;
  unsigned char csum = 0;
  char *buf2;
  char buf3[1];
  char *p;

  buf2 = malloc (PBUFSIZ);

  /* Copy the packet into buffer BUF2, encapsulating it
     and giving it a checksum.  */

  p = buf2;
  *p++ = '$';

  for (i = 0; i < cnt;)
    i += try_rle (buf + i, cnt - i, &csum, &p);

  *p++ = '#';
  *p++ = tohex ((csum >> 4) & 0xf);
  *p++ = tohex (csum & 0xf);

  *p = '\0';

  /* Send it over and over until we get a positive ack.  */

  do
    {
      int cc;

      if (write (remote_desc, buf2, p - buf2) != p - buf2)
	{
	  perror ("putpkt(write)");
	  free (buf2);
	  return -1;
	}

      if (remote_debug)
	{
	  fprintf (stderr, "putpkt (\"%s\"); [looking for ack]\n", buf2);
	  fflush (stderr);
	}
      cc = read (remote_desc, buf3, 1);
      if (remote_debug)
	{
	  fprintf (stderr, "[received '%c' (0x%x)]\n", buf3[0], buf3[0]);
	  fflush (stderr);
	}

      if (cc <= 0)
	{
	  if (cc == 0)
	    fprintf (stderr, "putpkt(read): Got EOF\n");
	  else
	    perror ("putpkt(read)");

	  free (buf2);
	  return -1;
	}

      /* Check for an input interrupt while we're here.  */
      if (buf3[0] == '\003')
	(*the_target->request_interrupt) ();
    }
  while (buf3[0] != '+');

  free (buf2);
  return 1;			/* Success! */
}

/* Send a packet to the remote machine, with error checking.  The data
   of the packet is in BUF, and the packet should be a NUL-terminated
   string.  Returns >= 0 on success, -1 otherwise.  */

int
putpkt (char *buf)
{
  return putpkt_binary (buf, strlen (buf));
}

/* Come here when we get an input interrupt from the remote side.  This
   interrupt should only be active while we are waiting for the child to do
   something.  About the only thing that should come through is a ^C, which
   will cause us to request child interruption.  */

static void
input_interrupt (int unused)
{
  fd_set readset;
  struct timeval immediate = { 0, 0 };

  /* Protect against spurious interrupts.  This has been observed to
     be a problem under NetBSD 1.4 and 1.5.  */

  FD_ZERO (&readset);
  FD_SET (remote_desc, &readset);
  if (select (remote_desc + 1, &readset, 0, 0, &immediate) > 0)
    {
      int cc;
      char c = 0;

      cc = read (remote_desc, &c, 1);

      if (cc != 1 || c != '\003')
	{
	  fprintf (stderr, "input_interrupt, count = %d c = %d ('%c')\n",
		   cc, c, c);
	  return;
	}

      (*the_target->request_interrupt) ();
    }
}

/* Check if the remote side sent us an interrupt request (^C).  */
void
check_remote_input_interrupt_request (void)
{
  /* This function may be called before establishing communications,
     therefore we need to validate the remote descriptor.  */

  if (remote_desc == INVALID_DESCRIPTOR)
    return;

  input_interrupt (0);
}

/* Asynchronous I/O support.  SIGIO must be enabled when waiting, in order to
   accept Control-C from the client, and must be disabled when talking to
   the client.  */

void
block_async_io (void)
{
#ifndef USE_WIN32API
  sigset_t sigio_set;
  sigemptyset (&sigio_set);
  sigaddset (&sigio_set, SIGIO);
  sigprocmask (SIG_BLOCK, &sigio_set, NULL);
#endif
}

void
unblock_async_io (void)
{
#ifndef USE_WIN32API
  sigset_t sigio_set;
  sigemptyset (&sigio_set);
  sigaddset (&sigio_set, SIGIO);
  sigprocmask (SIG_UNBLOCK, &sigio_set, NULL);
#endif
}

/* Current state of asynchronous I/O.  */
static int async_io_enabled;

/* Enable asynchronous I/O.  */
void
enable_async_io (void)
{
  if (async_io_enabled)
    return;

#ifndef USE_WIN32API
  signal (SIGIO, input_interrupt);
#endif
  async_io_enabled = 1;
}

/* Disable asynchronous I/O.  */
void
disable_async_io (void)
{
  if (!async_io_enabled)
    return;

#ifndef USE_WIN32API
  signal (SIGIO, SIG_IGN);
#endif
  async_io_enabled = 0;
}

/* Returns next char from remote GDB.  -1 if error.  */

static int
readchar (void)
{
  static unsigned char buf[BUFSIZ];
  static int bufcnt = 0;
  static unsigned char *bufp;

  if (bufcnt-- > 0)
    return *bufp++;

  bufcnt = read (remote_desc, buf, sizeof (buf));

  if (bufcnt <= 0)
    {
      if (bufcnt == 0)
	fprintf (stderr, "readchar: Got EOF\n");
      else
	perror ("readchar");

      return -1;
    }

  bufp = buf;
  bufcnt--;
  return *bufp++ & 0x7f;
}

/* Read a packet from the remote machine, with error checking,
   and store it in BUF.  Returns length of packet, or negative if error. */

int
getpkt (char *buf)
{
  char *bp;
  unsigned char csum, c1, c2;
  int c;

  while (1)
    {
      csum = 0;

      while (1)
	{
	  c = readchar ();
	  if (c == '$')
	    break;
	  if (remote_debug)
	    {
	      fprintf (stderr, "[getpkt: discarding char '%c']\n", c);
	      fflush (stderr);
	    }

	  if (c < 0)
	    return -1;
	}

      bp = buf;
      while (1)
	{
	  c = readchar ();
	  if (c < 0)
	    return -1;
	  if (c == '#')
	    break;
	  *bp++ = c;
	  csum += c;
	}
      *bp = 0;

      c1 = fromhex (readchar ());
      c2 = fromhex (readchar ());

      if (csum == (c1 << 4) + c2)
	break;

      fprintf (stderr, "Bad checksum, sentsum=0x%x, csum=0x%x, buf=%s\n",
	       (c1 << 4) + c2, csum, buf);
      write (remote_desc, "-", 1);
    }

  if (remote_debug)
    {
      fprintf (stderr, "getpkt (\"%s\");  [sending ack] \n", buf);
      fflush (stderr);
    }

  write (remote_desc, "+", 1);

  if (remote_debug)
    {
      fprintf (stderr, "[sent ack]\n");
      fflush (stderr);
    }

  return bp - buf;
}

void
write_ok (char *buf)
{
  buf[0] = 'O';
  buf[1] = 'K';
  buf[2] = '\0';
}

void
write_enn (char *buf)
{
  /* Some day, we should define the meanings of the error codes... */
  buf[0] = 'E';
  buf[1] = '0';
  buf[2] = '1';
  buf[3] = '\0';
}

void
convert_int_to_ascii (unsigned char *from, char *to, int n)
{
  int nib;
  int ch;
  while (n--)
    {
      ch = *from++;
      nib = ((ch & 0xf0) >> 4) & 0x0f;
      *to++ = tohex (nib);
      nib = ch & 0x0f;
      *to++ = tohex (nib);
    }
  *to++ = 0;
}


void
convert_ascii_to_int (char *from, unsigned char *to, int n)
{
  int nib1, nib2;
  while (n--)
    {
      nib1 = fromhex (*from++);
      nib2 = fromhex (*from++);
      *to++ = (((nib1 & 0x0f) << 4) & 0xf0) | (nib2 & 0x0f);
    }
}

static char *
outreg (int regno, char *buf)
{
  if ((regno >> 12) != 0)
    *buf++ = tohex ((regno >> 12) & 0xf);
  if ((regno >> 8) != 0)
    *buf++ = tohex ((regno >> 8) & 0xf);
  *buf++ = tohex ((regno >> 4) & 0xf);
  *buf++ = tohex (regno & 0xf);
  *buf++ = ':';
  collect_register_as_string (regno, buf);
  buf += 2 * register_size (regno);
  *buf++ = ';';

  return buf;
}

void
new_thread_notify (int id)
{
  char own_buf[256];

  /* The `n' response is not yet part of the remote protocol.  Do nothing.  */
  if (1)
    return;

  if (server_waiting == 0)
    return;

  sprintf (own_buf, "n%x", id);
  disable_async_io ();
  putpkt (own_buf);
  enable_async_io ();
}

void
dead_thread_notify (int id)
{
  char own_buf[256];

  /* The `x' response is not yet part of the remote protocol.  Do nothing.  */
  if (1)
    return;

  sprintf (own_buf, "x%x", id);
  disable_async_io ();
  putpkt (own_buf);
  enable_async_io ();
}

void
prepare_resume_reply (char *buf, char status, unsigned char sig)
{
  int nib;

  *buf++ = status;

  nib = ((sig & 0xf0) >> 4);
  *buf++ = tohex (nib);
  nib = sig & 0x0f;
  *buf++ = tohex (nib);

  if (status == 'T')
    {
      const char **regp = gdbserver_expedite_regs;

      if (the_target->stopped_by_watchpoint != NULL
	  && (*the_target->stopped_by_watchpoint) ())
	{
	  CORE_ADDR addr;
	  int i;

	  strncpy (buf, "watch:", 6);
	  buf += 6;

	  addr = (*the_target->stopped_data_address) ();

	  /* Convert each byte of the address into two hexadecimal chars.
	     Note that we take sizeof (void *) instead of sizeof (addr);
	     this is to avoid sending a 64-bit address to a 32-bit GDB.  */
	  for (i = sizeof (void *) * 2; i > 0; i--)
	    {
	      *buf++ = tohex ((addr >> (i - 1) * 4) & 0xf);
	    }
	  *buf++ = ';';
	}

      while (*regp)
	{
	  buf = outreg (find_regno (*regp), buf);
	  regp ++;
	}

      /* Formerly, if the debugger had not used any thread features we would not
	 burden it with a thread status response.  This was for the benefit of
	 GDB 4.13 and older.  However, in recent GDB versions the check
	 (``if (cont_thread != 0)'') does not have the desired effect because of
	 sillyness in the way that the remote protocol handles specifying a thread.
	 Since thread support relies on qSymbol support anyway, assume GDB can handle
	 threads.  */

      if (using_threads)
	{
	  unsigned int gdb_id_from_wait;

	  /* FIXME right place to set this? */
	  thread_from_wait = ((struct inferior_list_entry *)current_inferior)->id;
	  gdb_id_from_wait = thread_to_gdb_id (current_inferior);

	  if (debug_threads)
	    fprintf (stderr, "Writing resume reply for %ld\n\n", thread_from_wait);
	  /* This if (1) ought to be unnecessary.  But remote_wait in GDB
	     will claim this event belongs to inferior_ptid if we do not
	     specify a thread, and there's no way for gdbserver to know
	     what inferior_ptid is.  */
	  if (1 || old_thread_from_wait != thread_from_wait)
	    {
	      general_thread = thread_from_wait;
	      sprintf (buf, "thread:%x;", gdb_id_from_wait);
	      buf += strlen (buf);
	      old_thread_from_wait = thread_from_wait;
	    }
	}

      if (dlls_changed)
	{
	  strcpy (buf, "library:;");
	  buf += strlen (buf);
	  dlls_changed = 0;
	}
    }
  /* For W and X, we're done.  */
  *buf++ = 0;
}

void
decode_m_packet (char *from, CORE_ADDR *mem_addr_ptr, unsigned int *len_ptr)
{
  int i = 0, j = 0;
  char ch;
  *mem_addr_ptr = *len_ptr = 0;

  while ((ch = from[i++]) != ',')
    {
      *mem_addr_ptr = *mem_addr_ptr << 4;
      *mem_addr_ptr |= fromhex (ch) & 0x0f;
    }

  for (j = 0; j < 4; j++)
    {
      if ((ch = from[i++]) == 0)
	break;
      *len_ptr = *len_ptr << 4;
      *len_ptr |= fromhex (ch) & 0x0f;
    }
}

void
decode_M_packet (char *from, CORE_ADDR *mem_addr_ptr, unsigned int *len_ptr,
		 unsigned char *to)
{
  int i = 0;
  char ch;
  *mem_addr_ptr = *len_ptr = 0;

  while ((ch = from[i++]) != ',')
    {
      *mem_addr_ptr = *mem_addr_ptr << 4;
      *mem_addr_ptr |= fromhex (ch) & 0x0f;
    }

  while ((ch = from[i++]) != ':')
    {
      *len_ptr = *len_ptr << 4;
      *len_ptr |= fromhex (ch) & 0x0f;
    }

  convert_ascii_to_int (&from[i++], to, *len_ptr);
}

int
decode_X_packet (char *from, int packet_len, CORE_ADDR *mem_addr_ptr,
		 unsigned int *len_ptr, unsigned char *to)
{
  int i = 0;
  char ch;
  *mem_addr_ptr = *len_ptr = 0;

  while ((ch = from[i++]) != ',')
    {
      *mem_addr_ptr = *mem_addr_ptr << 4;
      *mem_addr_ptr |= fromhex (ch) & 0x0f;
    }

  while ((ch = from[i++]) != ':')
    {
      *len_ptr = *len_ptr << 4;
      *len_ptr |= fromhex (ch) & 0x0f;
    }

  if (remote_unescape_input ((const gdb_byte *) &from[i], packet_len - i,
			     to, *len_ptr) != *len_ptr)
    return -1;

  return 0;
}

/* Decode a qXfer write request.  */
int
decode_xfer_write (char *buf, int packet_len, char **annex, CORE_ADDR *offset,
		   unsigned int *len, unsigned char *data)
{
  char ch;

  /* Extract and NUL-terminate the annex.  */
  *annex = buf;
  while (*buf && *buf != ':')
    buf++;
  if (*buf == '\0')
    return -1;
  *buf++ = 0;

  /* Extract the offset.  */
  *offset = 0;
  while ((ch = *buf++) != ':')
    {
      *offset = *offset << 4;
      *offset |= fromhex (ch) & 0x0f;
    }

  /* Get encoded data.  */
  packet_len -= buf - *annex;
  *len = remote_unescape_input ((const gdb_byte *) buf, packet_len,
				data, packet_len);
  return 0;
}

/* Ask GDB for the address of NAME, and return it in ADDRP if found.
   Returns 1 if the symbol is found, 0 if it is not, -1 on error.  */

int
look_up_one_symbol (const char *name, CORE_ADDR *addrp)
{
  char own_buf[266], *p, *q;
  int len;
  struct sym_cache *sym;

  /* Check the cache first.  */
  for (sym = symbol_cache; sym; sym = sym->next)
    if (strcmp (name, sym->name) == 0)
      {
	*addrp = sym->addr;
	return 1;
      }

  /* If we've passed the call to thread_db_look_up_symbols, then
     anything not in the cache must not exist; we're not interested
     in any libraries loaded after that point, only in symbols in
     libpthread.so.  It might not be an appropriate time to look
     up a symbol, e.g. while we're trying to fetch registers.  */
  if (all_symbols_looked_up)
    return 0;

  /* Send the request.  */
  strcpy (own_buf, "qSymbol:");
  hexify (own_buf + strlen ("qSymbol:"), name, strlen (name));
  if (putpkt (own_buf) < 0)
    return -1;

  /* FIXME:  Eventually add buffer overflow checking (to getpkt?)  */
  len = getpkt (own_buf);
  if (len < 0)
    return -1;

  /* We ought to handle pretty much any packet at this point while we
     wait for the qSymbol "response".  That requires re-entering the
     main loop.  For now, this is an adequate approximation; allow
     GDB to read from memory while it figures out the address of the
     symbol.  */
  while (own_buf[0] == 'm')
    {
      CORE_ADDR mem_addr;
      unsigned char *mem_buf;
      unsigned int mem_len;

      decode_m_packet (&own_buf[1], &mem_addr, &mem_len);
      mem_buf = malloc (mem_len);
      if (read_inferior_memory (mem_addr, mem_buf, mem_len) == 0)
	convert_int_to_ascii (mem_buf, own_buf, mem_len);
      else
	write_enn (own_buf);
      free (mem_buf);
      if (putpkt (own_buf) < 0)
	return -1;
      len = getpkt (own_buf);
      if (len < 0)
	return -1;
    }
  
  if (strncmp (own_buf, "qSymbol:", strlen ("qSymbol:")) != 0)
    {
      warning ("Malformed response to qSymbol, ignoring: %s\n", own_buf);
      return -1;
    }

  p = own_buf + strlen ("qSymbol:");
  q = p;
  while (*q && *q != ':')
    q++;

  /* Make sure we found a value for the symbol.  */
  if (p == q || *q == '\0')
    return 0;

  decode_address (addrp, p, q - p);

  /* Save the symbol in our cache.  */
  sym = malloc (sizeof (*sym));
  sym->name = strdup (name);
  sym->addr = *addrp;
  sym->next = symbol_cache;
  symbol_cache = sym;

  return 1;
}

void
monitor_output (const char *msg)
{
  char *buf = malloc (strlen (msg) * 2 + 2);

  buf[0] = 'O';
  hexify (buf + 1, msg, 0);

  putpkt (buf);
  free (buf);
}

/* Return a malloc allocated string with special characters from TEXT
   replaced by entity references.  */

char *
xml_escape_text (const char *text)
{
  char *result;
  int i, special;

  /* Compute the length of the result.  */
  for (i = 0, special = 0; text[i] != '\0'; i++)
    switch (text[i])
      {
      case '\'':
      case '\"':
	special += 5;
	break;
      case '&':
	special += 4;
	break;
      case '<':
      case '>':
	special += 3;
	break;
      default:
	break;
      }

  /* Expand the result.  */
  result = malloc (i + special + 1);
  for (i = 0, special = 0; text[i] != '\0'; i++)
    switch (text[i])
      {
      case '\'':
	strcpy (result + i + special, "&apos;");
	special += 5;
	break;
      case '\"':
	strcpy (result + i + special, "&quot;");
	special += 5;
	break;
      case '&':
	strcpy (result + i + special, "&amp;");
	special += 4;
	break;
      case '<':
	strcpy (result + i + special, "&lt;");
	special += 3;
	break;
      case '>':
	strcpy (result + i + special, "&gt;");
	special += 3;
	break;
      default:
	result[i + special] = text[i];
	break;
      }
  result[i + special] = '\0';

  return result;
}

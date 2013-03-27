/* The remote-virtual-component simulator framework
   for GDB, the GNU Debugger.

   Copyright 2006, 2007 Free Software Foundation, Inc.

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


#include "sim-main.h"
#include "hw-main.h"

#include "hw-tree.h"

#include <ctype.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

/* Not guarded in dv-sockser.c, so why here.  */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>


/* DEVICE


   rv - Remote Virtual component


   DESCRIPTION


   Socket connection to a remote simulator component, for example one
   for testing a verilog construction.  Protocol defined below.

   There is a set of 32-bit I/O ports, with a mapping from local to
   remote addresses.  There is a set of interrupts expressed as a
   bit-mask, with a mapping from remote to local.  There is a set of
   memory ranges (actual memory defined elsewhere), also with a
   mapping from remote to local addresses, that is expected to be
   accessible to the remote simulator in 32-byte chunks (simulating
   DMA).  There is a mapping from remote cycles (or an appropriate
   elsewhere defined time-slice) to local cycles.

   PROPERTIES

   reg = <address> <size>
   The address (within the parent bus) that this device is to
   be located.

   remote-reg = <remote-address>
   The address of reg on the remote side.  Defaults to 0.

   mem = <address> <size>
   Specify an address-range (within the parent bus) that the remote
   device can access.  The memory is assumed to be already defined.
   If there's no memory defined but the remote side asks for a memory
   access, the simulation is aborted.

   remote-mem = <remote-address>
   The address of mem on the remote side.  Defaults to 0.

   mbox = <address>
   Address of the mailbox interface.  Writes to this address with the
   local address of a mailbox command, a complete packet with length
   and command; (4 or 6)) invokes the mailbox interface.  Reads are
   invalid.  Replies are written to the same address.  Address space
   from <address> up-to-and-including <address>+3 is allocated.

   max-poll-ticks = <local-count>
   Sets the maximum interval between polling the external component,
   expressed in internal cycles.  Defaults to 10000.

   watchdog-interval = <seconds>
   Sets the wallclock seconds between watchdog packets sent to the
   remote side (may be larger if there's no rv activity in that time).
   Defaults to 30.  If set to 0, no watchdog packets are sent.

   intnum = <local-int-0> <local-int-1> ... <local-int-31>
   Defines a map from remote bit numbers to local values to be emitted
   on the "int" port, with the external bit number as the ordinal - 1
   of the local translation.  E.g. 43 121 would mean map external
   (1<<0) to internal 43 and external (1<<1) to internal 121.  The
   default is unity; no translation.  If more than one bit is set in
   the remote interrupt word, the intmultiple property can be used to
   control the translation.

   intmultiple = <intvalue>
   When more than one bit is set in the remote interrupt word, you may
   want to map this situation to a separate interrupt value.  If this
   property is non-zero, it is used as that value.  If it is zero, the
   local value for the "int" port is the bitwise-or of the translated
   local values.

   host = <hostid>
   The hostname or address where the simulator to be used listens.
   Defaults to "127.0.0.1"

   port = <portnumber>
   The hostname or address where the simulator to be used listens.
   Defaults to 10000.

   dummy = <value>
    or
   dummy = <filename>
   Don't connect to a remote side; use initial dummy contents from
   <filename> (which has to be at least as big as the <size> argument
   of reg above) or filled with byte-value <value>.  Mailboxes are not
   supported (can be defined but can not be used) and remote-memory
   accesses don't apply.  The main purpose for this property is to
   simplify use of configuration and simulated hardware that is
   e.g. only trivially initialized but not actually used.


   PORTS

   int (output)
   Driven as a result of a remote interrupt request.  The value is a
   32-bit bitset of active interrupts.


   BUGS

   All and none.


   PROTOCOL

   This is version 1.0 of this protocol, defining packet format and
   actions in a supposedly upward-compatible manner where client and
   servers of different versions are expected to interoperate; the
   format and the definitions below are hopefully generic enough to
   allow this.

   Each connection has a server and a client (this code); the roles
   are known beforehand.  The client usually corresponds to a CPU and
   memory system and the server corresponds to a memory-mapped
   register hardware interface and/or a DMA controller.  They
   communicate using packets with specific commands, of which some
   require replies from the other side; most are intiated by the
   client with one exception.  A reply uses the same format as the
   command.

   Packets are at least three bytes long, where the first two bytes
   form a header, a 16-bit little-endian number that is the total
   length of the packet including the header.  There is also a
   one-byte command.  The payload is optional, depending on the
   command.

   [[16-bit-low-byte-of-length] [16-bit-high-byte-of-length]
    [command/reply] [payload byte 0] [payload byte 1]
    ... [payload byte (length-of-packet - 3)]]

   Commands:

   A client or server that reads an undocumented command may exit with
   a hard error.  Payload not defined or disallowed below is ignored.

   It is expected that future client versions find out the version of
   the server side by polling with base commands, assuming earlier
   versions if a certain reply isn't seen, with newly defined payload
   parts where earlier versions left it undefined.  New commands and
   formats are sent only to the other side after the client and server
   has found out each others version.  Not all servers support all
   commands; the type of server and supported set of commands is
   expected to be known beforehand.

   RV_READ_CMD = 0
   Initiated by the client, requires a reply from the server.  The
   payload from the client is at least 4 bytes, forming a 4-byte
   little-endian address, the rest being undefined.  The reply from
   the server is at least 8 bytes, forming the same address data as in
   the request and the second 4-byte data being the little-endian
   contents.

   RV_WRITE_CMD = 1
   Initiated by the client, requires a reply from the server.  Payload
   from the client is at least 8 bytes, forming a 4-byte little-endian
   word being the address, the rest being the little-endian contents
   to write.  The reply from the server is 8 bytes unless elsewhere
   agreed otherwise, forming the same address and data as in the
   request.  The data sent back may have been altered to correspond to
   defined parts but can safely be discarded.

   RV_IRQ_CMD = 2
   Initiated by the server, no reply.  The payload is 4 bytes, forming
   a little-endian word with bits numbers corresponding to currently
   active interrupt sources; value (1<<N) indicating interrupt source
   N being active.

   RV_MEM_RD_CMD = 3
   Initiated by the server, requires a reply.  A client must know
   beforehand when (in command sequence or constant) the server can
   send this command and if so must then not send any commands of its
   own (including watchdog commands); the server is allowed to assume
   that incoming data is only replies to this command.  The format is
   8 bytes of data; 4 bytes of little-endian address followed by a
   32-bit little endian word with the number of bytes to read.  The
   reply is the same address and number of bytes, followed by the data
   that had been read.

   RV_MEM_WR_CMD = 4
   Initiated by the server, no reply.  The format is the same as a
   reply to RV_MEM_RD_CMD; a 32-bit little-endian address, followed by
   the 32-bit little-endian number of bytes to write (redundant
   information but must be consistent with the packet header).

   RV_MBOX_HANDLE_CMD = 5
   Initiated by the client, requires a reply.  The payload is 4
   undefined bytes followed by an binary blob, the size of the
   blob given by the packet header.  The reply is a 32-bit little
   endian number at the same index as the undefined bytes.  Actual
   semantics are application-specific.

   RV_MBOX_PUT_CMD = 6
   Initiated by the client, requires a reply, with the reply using the
   RV_MBOX_HANDLE_CMD reply format (i.e. *both* that command and
   32-bit little-endian number).  The payload is a 32-bit little
   endian number followed by an undefined payload, at most 20 bytes
   long.  The reply is a 32-bit little endian number.  Actual
   semantics are application-specific.

   RV_WATCHDOG_CMD = 7
   Initiated by the client, no reply.  A version 1.0 client sends no
   payload; a version 1.0 server should ignore any such payload.  A
   version 1.0 server must not send a reply.


   Possible future enhancements:

   Synchronization; server and client reports the number of elapsed
   cycles (unit to-be-defined) at each request or notification.
   Pretty much the top-of-the-todo-list item.

   Large addresses; 1.0 being restricted to 32-bit addresses.

   Variable-size data; currently restricted to 32-bit register
   accesses.

   Specified data endianness (not the packet header) perhaps as part
   of an initial format request; currently little-endian only.


   Usage notes:
   When used with servers sending RV_MEM_RD_CMD but being
   narrow-minded about indata, set watchdog-interval to 0.  Use
   multiple rv instances when there are e.g. separate register and
   memory servers.  Alway log, setting "/rv/trace? true", at the
   development phase.  Borrow from the test-suite.
   */

#define RV_FAMILY_NAME "rv"

enum rv_command {
  RV_READ_CMD = 0,
  RV_WRITE_CMD = 1,
  RV_IRQ_CMD = 2,
  RV_MEM_RD_CMD = 3,
  RV_MEM_WR_CMD = 4,
  RV_MBOX_HANDLE_CMD = 5,
  RV_MBOX_PUT_CMD = 6,
  RV_WATCHDOG_CMD = 7
};


typedef struct _hw_rv_device
{
  /* Mapping of remote interrupt bit-numbers to local ones.  */
  unsigned32 remote_to_local_int[32];

  /* When multiple bits are set, a non-zero value here indicates that
     this value should be used instead.  */
  unsigned32 intmultiple;

  /* Local address of registers.  */
  unsigned32 reg_address;

  /* Size of register bank in bytes.  */
  unsigned32 reg_size;

  /* Remote address of registers.  */
  unsigned32 remote_reg_address;

  /* Local address of DMA:able memory.  */
  unsigned32 mem_address;

  /* Size of DMA:able memory in bytes.  */
  unsigned32 mem_size;

  /* Bitmask for valid DMA request size.  */
  unsigned32 mem_burst_mask;

  /* Remote address of DMA:able memory.  */
  unsigned32 remote_mem_address;

  /* (Local) address of mbox; where to put a pointer to the mbox to be
     sent.  */
  unsigned32 mbox_address;

  /* Probably not 127.0.0.1:10000.  */
  const char *host;
  int port;

  /* If non-NULL, points to memory to use instead of connection.  */
  unsigned8 *dummy;

  /* File descriptor for the socket.  Set to -1 when error.  Only one
     of dummy and this is active.  */
  int fd;

  /* Stashed errno, as we don't emit an error right away.  */
  int saved_errno;

  /* This, plus latency because the CPU might not be checking until a
     CTI insn (usually a branch or a jump) is the interval in cycles
     between the rv is polled for e.g. DMA requests.  */
  unsigned32 max_tick_poll_interval;

  /* Running counter for exponential backoff up to
     max_tick_poll_interval to avoid polling the connection
     unnecessarily often.  Set to 1 when rv activity (read/write
     register, DMA request) is detected.  */
  unsigned32 next_period;

  /* This is the interval in wall-clock seconds between watchdog
     packets are sent to the remote side.  Zero means no watchdog
     packets. */
  unsigned32 watchdog_interval;

  /* Last time we sent a watchdog packet.  */
  struct timeval last_wdog_time;

  /* Mostly used as a kludge for knowing which rv:s have poll events
     active.  */
  struct hw_event *poll_callback;
} hw_rv_device;


/* We might add ports in the future, so keep this an enumeration.  */
enum
  {
    INT_PORT
  };

/* Our ports.  */
static const struct hw_port_descriptor hw_rv_ports[] = {
  { "int", INT_PORT, 0, output_port },
  { NULL }
};

/* Send LEN bytes of data from BUF to the socket.  Abort on
   errors.  */

static void
hw_rv_write (struct hw *me,
	     void *buf,
	     unsigned int len)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  unsigned8 *bufp = buf;

  /* If we don't have a valid fd here, it's because we got an error
     initially, and we suppressed that error.  */
  if (rv->fd < 0)
    hw_abort (me, "couldn't open a connection to %s:%d because: %s",
	      rv->host, rv->port, strerror (rv->saved_errno));

  while (len > 0)
    {
      ssize_t ret = write (rv->fd, bufp, len);
      if (ret < 0)
	/* FIXME: More graceful exit.  */
	hw_abort (me, "write to %s:%d failed: %s\n", rv->host, rv->port,
		  strerror (errno));

      len -= ret;
      bufp += ret;
    }
}

/* Read LEN bytes of data into BUF from the socket.  Set the file
   descriptor to -1 if there's an error.  */

static void
hw_rv_read (struct hw *me,
	    void *buf,
	    unsigned int len)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  unsigned8 *bufp = buf;

  while (len > 0)
    {
      ssize_t ret = read (rv->fd, bufp, len);

      /* We get all zero if the remote end quits, but no error
	 indication; even select says there's data active.  */
      if (ret <= 0)
	{
	  if (close (rv->fd) != 0)
	    /* FIXME: More graceful exit.  */
	    hw_abort (me, "read from %s:%d failed: %d\n", rv->host, rv->port, errno);
	  rv->fd = -1;
	  return;
	}

      len -= ret;
      bufp += ret;
    }
}

/* Construct and send a packet of data of type CMD and len
   LEN_NOHEADER (not counting the header...).  */

static void
hw_rv_send (struct hw *me,
	    unsigned int cmd,
	    void *msg,
	    unsigned int len_noheader)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  unsigned8 buf[32+3];
  unsigned8 *bufp;
  unsigned int len = len_noheader + 3;
  int ret;

  buf[0] = len & 255;
  buf[1] = (len >> 8) & 255;
  buf[2] = cmd;

  if (len > sizeof (buf))
    {
      hw_rv_write (me, buf, 3);
      len = len_noheader;
      bufp = msg;
    }
  else
    {
      memcpy (buf + 3, msg, len_noheader);
      bufp = buf;
    }

  hw_rv_write (me, bufp, len);
}

/* Handle incoming DMA requests as per the RV_MEM_RD_CMD packet.
   Abort on errors.  */

static void
hw_rv_read_mem (struct hw *me, unsigned int len)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  /* If you change this size, please adjust the mem2 testcase.  */
  unsigned8 buf[32+8];
  unsigned8 *bufp = buf;
  unsigned32 leaddr;
  unsigned32 addr;
  unsigned32 lelen;
  unsigned32 i;

  if (len != 8)
    hw_abort (me, "expected DMA read request len 8+3, got %d+3", len);

  hw_rv_read (me, &leaddr, 4);
  hw_rv_read (me, &lelen, 4);
  len = LE2H_4 (lelen);
  addr = LE2H_4 (leaddr);

  if (addr < rv->remote_mem_address
      || addr >= rv->remote_mem_address + rv->mem_size)
    hw_abort (me, "DMA read at remote 0x%x; outside [0x%x..0x%x-1]",
	      (unsigned) addr, (unsigned) rv->remote_mem_address,
	      (unsigned) (rv->remote_mem_address + rv->mem_size));
  addr = addr - rv->remote_mem_address + rv->mem_address;

  if (len == 0)
    hw_abort (me, "DMA read request for 0 bytes isn't supported");

  if (len & ~rv->mem_burst_mask)
    hw_abort (me, "DMA trying to read %d bytes; not matching mask of 0x%x",
	      len, rv->mem_burst_mask);
  if (len + 8 > sizeof (buf))
    bufp = hw_malloc (me, len + 8);

  HW_TRACE ((me, "DMA R 0x%x..0x%x", addr, addr + len -1));
  hw_dma_read_buffer (me, bufp + 8, 0, addr, len);
  if (hw_trace_p (me))
    for (i = 0; i < len; i += 4)
      HW_TRACE ((me, "0x%x: %02x %02x %02x %02x",
		 addr + i,
		 bufp[i+8], bufp[i+9], bufp[i+10], bufp[i+11]));

  memcpy (bufp, &leaddr, 4);
  memcpy (bufp + 4, &lelen, 4);
  hw_rv_send (me, RV_MEM_RD_CMD, bufp, len + 8);
  if (bufp != buf)
    hw_free (me, bufp);
}

/* Handle incoming DMA requests as per the RV_MEM_WR_CMD packet.
   Abort on errors.  */

static void
hw_rv_write_mem (struct hw *me, unsigned int plen)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  /* If you change this size, please adjust the mem2 testcase.  */
  unsigned8 buf[32+8];
  unsigned8 *bufp = buf;
  unsigned32 leaddr;
  unsigned32 addr;
  unsigned32 lelen;
  unsigned32 len;
  unsigned32 i;

  hw_rv_read (me, &leaddr, 4);
  hw_rv_read (me, &lelen, 4);
  len = LE2H_4 (lelen);
  addr = LE2H_4 (leaddr);

  if (len != plen - 8)
    hw_abort (me,
	      "inconsistency in DMA write request packet: "
	      "envelope %d+3, inner %d bytes", plen, len);

  if (addr < rv->remote_mem_address
      || addr >= rv->remote_mem_address + rv->mem_size)
    hw_abort (me, "DMA write at remote 0x%x; outside [0x%x..0x%x-1]",
	      (unsigned) addr, (unsigned) rv->remote_mem_address,
	      (unsigned) (rv->remote_mem_address + rv->mem_size));

  addr = addr - rv->remote_mem_address + rv->mem_address;
  if (len == 0)
    hw_abort (me, "DMA write request for 0 bytes isn't supported");

  if (len & ~rv->mem_burst_mask)
    hw_abort (me, "DMA trying to write %d bytes; not matching mask of 0x%x",
	      len, rv->mem_burst_mask);
  if (len + 8 > sizeof (buf))
    bufp = hw_malloc (me, len + 8);

  hw_rv_read (me, bufp + 8, len);
  HW_TRACE ((me, "DMA W 0x%x..0x%x", addr, addr + len - 1));
  hw_dma_write_buffer (me, bufp + 8, 0, addr, len, 0);
  if (hw_trace_p (me))
    for (i = 0; i < len; i += 4)
      HW_TRACE ((me, "0x%x: %02x %02x %02x %02x",
		 addr + i,
		 bufp[i+8], bufp[i+9], bufp[i+10], bufp[i+11]));
  if (bufp != buf)
    hw_free (me, bufp);
}

static void
hw_rv_irq (struct hw *me, unsigned int len)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  unsigned32 intbitsle;
  unsigned32 intbits_ext;
  unsigned32 intval = 0;
  int i;

  if (len != 4)
    hw_abort (me, "IRQ with %d data not supported", len);

  hw_rv_read (me, &intbitsle, 4);
  intbits_ext = LE2H_4 (intbitsle);
  for (i = 0; i < 32; i++)
    if ((intbits_ext & (1 << i)) != 0)
      intval |= rv->remote_to_local_int[i];
  if ((intbits_ext & ~(intbits_ext - 1)) != intbits_ext
      && rv->intmultiple != 0)
    intval = rv->intmultiple;

  HW_TRACE ((me, "IRQ 0x%x", intval));
  hw_port_event (me, INT_PORT, intval);
}

/* Handle incoming interrupt notifications as per the RV_IRQ_CMD
   packet.  Abort on errors.  */

static void
hw_rv_handle_incoming (struct hw *me,
		       int expected_type,
		       unsigned8 *buf,
		       unsigned int *return_len)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  unsigned8 cbuf[32];
  unsigned int len;
  unsigned int cmd;

  while (1)
    {
      hw_rv_read (me, cbuf, 3);

      if (rv->fd < 0)
	return;

      len = cbuf[0] + cbuf[1] * 256 - 3;
      cmd = cbuf[2];

      /* These come in "asynchronously"; not as a reply.  */
      switch (cmd)
	{
	case RV_IRQ_CMD:
	  hw_rv_irq (me, len);
	  break;

	case RV_MEM_RD_CMD:
	  hw_rv_read_mem (me, len);
	  break;

	case RV_MEM_WR_CMD:
	  hw_rv_write_mem (me, len);
	  break;
	}

      /* Something is incoming from the other side, so tighten up all
	 slack at the next wait.  */
      rv->next_period = 1;

      switch (cmd)
	{
	case RV_MEM_RD_CMD:
	case RV_MEM_WR_CMD:
	case RV_IRQ_CMD:
	  /* Don't try to handle more than one of these if we were'nt
	     expecting a reply.  */
	  if (expected_type == -1)
	    return;
	  continue;
	}

      /* Require a match between this supposed-reply and the command
	 for the rest.  */
      if (cmd != expected_type)
	hw_abort (me, "unexpected reply, expected command %d, got %d",
		  expected_type, cmd);

      switch (cmd)
	{
	case RV_MBOX_PUT_CMD:
	case RV_MBOX_HANDLE_CMD:
	case RV_WRITE_CMD:
	case RV_READ_CMD:
	  hw_rv_read (me, buf, len <= *return_len ? len : *return_len);
	  *return_len = len;
	  break;
	}
      break;
    }
}

/* Send a watchdog packet.  Make a note of wallclock time.  */

static void
hw_rv_send_wdog (struct hw *me)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  HW_TRACE ((me, "WD"));
  gettimeofday (&rv->last_wdog_time, NULL);
  hw_rv_send (me, RV_WATCHDOG_CMD, "", 0);
}

/* Poll the remote side: see if there's any incoming traffic; handle a
   packet if so.  Send a watchdog packet if it's time to do so.
   Beware that the Linux select call indicates traffic for a socket
   that the remote side has closed (which may be because it was
   finished; don't hork until we need to write something just because
   we're polling).  */

static void
hw_rv_poll_once (struct hw *me)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  fd_set rfds;
  fd_set efds;
  struct timeval now;
  int ret;
  struct timeval tv;

  if (rv->fd < 0)
    /* Connection has died or was never initiated.  */
    return;

  FD_ZERO (&rfds);
  FD_SET (rv->fd, &rfds);
  FD_ZERO (&efds);
  FD_SET (rv->fd, &efds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  ret = select (rv->fd + 1, &rfds, NULL, &efds, &tv);
  gettimeofday (&now, NULL);

  if (ret < 0)
    hw_abort (me, "select failed: %d\n", errno);

  if (rv->watchdog_interval != 0
      && now.tv_sec - rv->last_wdog_time.tv_sec >= rv->watchdog_interval)
    hw_rv_send_wdog (me);

  if (FD_ISSET (rv->fd, &rfds))
    hw_rv_handle_incoming (me, -1, NULL, NULL);
}

/* Initialize mapping of remote-to-local interrupt data.  */

static void
hw_rv_map_ints (struct hw *me)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  int i;

  for (i = 0; i < 32; i++)
    rv->remote_to_local_int[i] = 1 << i;

  if (hw_find_property (me, "intnum") != NULL)
    for (i = 0; i < 32; i++)
      {
	signed_cell val = -1;
	if (hw_find_integer_array_property (me, "intnum", i, &val) > 0)
	  {
	    if (val > 0)
	      rv->remote_to_local_int[i] = val;
	    else
	      hw_abort (me, "property \"intnum@%d\" must be > 0; is %d",
			i, (int) val);
	  }
      }
}

/* Handle the after-N-ticks "poll event", calling the poll-the-fd
   method.  Update the period.  */

static void
do_poll_event (struct hw *me, void *data)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  unsigned32 new_period;

  if (rv->dummy != NULL)
    return;

  hw_rv_poll_once (me);
  if (rv->fd >= 0)
    rv->poll_callback
      = hw_event_queue_schedule (me, rv->next_period, do_poll_event, NULL);

  new_period = rv->next_period * 2;
  if (new_period <= rv->max_tick_poll_interval)
    rv->next_period = new_period;
}

/* HW tree traverse function for hw_rv_add_init.  */

static void
hw_rv_add_poller (struct hw *me, void *data)
{
  hw_rv_device *rv;

  if (hw_family (me) == NULL
      || strcmp (hw_family (me), RV_FAMILY_NAME) != 0)
    return;

  rv = (hw_rv_device *) hw_data (me);
  if (rv->poll_callback != NULL)
    return;

  rv->poll_callback
    = hw_event_queue_schedule (me, 1, do_poll_event, NULL);
}

/* Simulator module init function for hw_rv_add_init.  */

/* FIXME: For the call so hw_tree_traverse, we need to know that the
   first member of struct sim_hw is the struct hw *root, but there's
   no accessor method and struct sim_hw is defined in sim-hw.c only.
   Hence this hack, until an accessor is added, or there's a traverse
   function that takes a SIM_DESC argument.  */
struct sim_hw { struct hw *tree; };

static SIM_RC
hw_rv_add_rv_pollers (SIM_DESC sd)
{
  hw_tree_traverse (STATE_HW (sd)->tree, hw_rv_add_poller, NULL, NULL);
  return SIM_RC_OK;
}

/* We need to add events for polling, but we can't add one from the
   finish-function, and there are no other call points, at least for
   instances without "reg" (when there are just DMA requests from the
   remote end; no locally initiated activity).  Therefore we add a
   simulator module init function, but those don't have private
   payload arguments; just a SD argument.  We cope by parsing the HW
   root and making sure *all* "rv":s have poll callbacks installed.
   Luckily, this is just an initialization step, and not many
   simultaneous instances of rv are expected: we get a N**2 complexity
   for visits to each rv node by this method.  */

static void
hw_rv_add_init (struct hw *me)
{
  sim_module_add_init_fn (hw_system (me), hw_rv_add_rv_pollers);
}

/* Open up a connection to the other side.  Abort on errors.  */

static void
hw_rv_init_socket (struct hw *me)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  int sock;
  struct sockaddr_in server;

  rv->fd = -1;

  if (rv->dummy != NULL)
    return;

  memset (&server, 0, sizeof (server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr (rv->host);

  /* Solaris 2.7 lacks this macro.  */
#ifndef INADDR_NONE
#define INADDR_NONE -1
#endif

  if (server.sin_addr.s_addr == INADDR_NONE)
    {
      struct hostent *h;
      h = gethostbyname (rv->host);
      if (h != NULL)
	{
	  memcpy (&server.sin_addr, h->h_addr, h->h_length);
	  server.sin_family = h->h_addrtype;
	}
      else
	hw_abort (me, "can't resolve host %s", rv->host);
    }

  server.sin_port = htons (rv->port);
  sock = socket (AF_INET, SOCK_STREAM, 0);

  if (sock < 0)
    hw_abort (me, "can't get a socket for %s:%d connection",
	      rv->host, rv->port);

  if (connect (sock, (struct sockaddr *) &server, sizeof server) >= 0)
    {
      rv->fd = sock;

      /* FIXME: init packet here.  Maybe start packet too.  */
      if (rv->watchdog_interval != 0)
	hw_rv_send_wdog (me);
    }
  else
    /* Stash the errno for later display, if some connection activity
       is requested.  Don't emit an error here; we might have been
       called just for test purposes.  */
    rv->saved_errno = errno;
}

/* Local rv register reads end up here.  */

static unsigned int
hw_rv_reg_read (struct hw *me,
		void *dest,
		int space,
		unsigned_word addr,
		unsigned int nr_bytes)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);
  unsigned8 addr_data[8] = "";
  unsigned32 a_l = H2LE_4 (addr - rv->reg_address + rv->remote_reg_address);
  unsigned int len = 8;

  if (nr_bytes != 4)
    hw_abort (me, "must be four byte read");

  if (addr == rv->mbox_address)
    hw_abort (me, "invalid read of mbox address 0x%x",
	      (unsigned) rv->mbox_address);

  memcpy (addr_data, &a_l, 4);
  HW_TRACE ((me, "REG R 0x%x", addr));
  if (rv->dummy != NULL)
    {
      len = 8;
      memcpy (addr_data + 4, rv->dummy + addr - rv->reg_address, 4);
    }
  else
    {
      hw_rv_send (me, RV_READ_CMD, addr_data, len);
      hw_rv_handle_incoming (me, RV_READ_CMD, addr_data, &len);
    }

  if (len != 8)
    hw_abort (me, "read %d != 8 bytes returned", len);
  HW_TRACE ((me, ":= 0x%02x%02x%02x%02x",
	     addr_data[7], addr_data[6], addr_data[5], addr_data[4]));
  memcpy (dest, addr_data + 4, 4);
  return nr_bytes;
}

/* Local rv mbox requests (handle or put) end up here.  */

static void
hw_rv_mbox (struct hw *me, unsigned_word address)
{
  unsigned8 buf[256+3];
  unsigned int cmd;
  unsigned int rlen;
  unsigned32 i;
  unsigned int len
    = hw_dma_read_buffer (me, buf, 0, address, 3);

  if (len != 3)
    hw_abort (me, "mbox read %d != 3 bytes returned", len);

  cmd = buf[2];
  if (cmd != RV_MBOX_HANDLE_CMD && cmd != RV_MBOX_PUT_CMD)
    hw_abort (me, "unsupported mbox command %d", cmd);

  len = buf[0] + buf[1]*256;

  if (len > sizeof (buf))
    hw_abort (me, "mbox cmd %d send size %d unsupported", cmd, len);

  rlen = hw_dma_read_buffer (me, buf + 3, 0, address + 3, len - 3);
  if (rlen != len - 3)
    hw_abort (me, "mbox read %d != %d bytes returned", rlen, len - 3);

  HW_TRACE ((me, "MBOX %s 0x%x..0x%x",
	     cmd == RV_MBOX_HANDLE_CMD ? "H" : "P",
	     address, address + len - 1));
  for (i = 0; i < rlen; i += 8)
    HW_TRACE ((me, "0x%x: %02x %02x %02x %02x %02x %02x %02x %02x",
	       address + 3 + i,
	       buf[3+i], buf[4+i], buf[5+i], buf[6+i], buf[7+i], buf[8+i],
	       buf[9+i], buf[10+i]));

  len -= 3;
  hw_rv_send (me, cmd, buf + 3, len);

  /* Note: both ..._PUT and ..._HANDLE get the ..._HANDLE reply.  */
  hw_rv_handle_incoming (me, RV_MBOX_HANDLE_CMD, buf + 3, &len);
  if (len > sizeof (buf))
    hw_abort (me, "mbox cmd %d receive size %d unsupported", cmd, len);
  HW_TRACE ((me, "-> 0x%x..0x%x", address, address + len + 3 - 1));
  for (i = 0; i < len; i += 8)
    HW_TRACE ((me, "0x%x: %02x %02x %02x %02x %02x %02x %02x %02x",
	       address + 3 + i,
	       buf[3+i], buf[4+i], buf[5+i], buf[6+i], buf[7+i], buf[8+i],
	       buf[9+i], buf[10+i]));

  len += 3;
  buf[0] = len & 255;
  buf[1] = len / 256;
  rlen = hw_dma_write_buffer (me, buf, 0, address, len, 0);
  if (rlen != len)
    hw_abort (me, "mbox write %d != %d bytes", rlen, len);
}

/* Local rv register writes end up here.  */

static unsigned int
hw_rv_reg_write (struct hw *me,
		 const void *source,
		 int space,
		 unsigned_word addr,
		 unsigned int nr_bytes)
{
  hw_rv_device *rv = (hw_rv_device *) hw_data (me);

  unsigned8 addr_data[8] = "";
  unsigned32 a_l = H2LE_4 (addr - rv->reg_address + rv->remote_reg_address);
  unsigned int len = 8;

  if (nr_bytes != 4)
    hw_abort (me, "must be four byte write");

  memcpy (addr_data, &a_l, 4);
  memcpy (addr_data + 4, source, 4);

  if (addr == rv->mbox_address)
    {
      unsigned32 mbox_addr_le;
      if (rv->dummy != NULL)
	hw_abort (me, "mbox not supported for a dummy instance");
      memcpy (&mbox_addr_le, source, 4);
      hw_rv_mbox (me, LE2H_4 (mbox_addr_le));
      return nr_bytes;
    }

  HW_TRACE ((me, "REG W 0x%x := 0x%02x%02x%02x%02x", addr,
	     addr_data[7], addr_data[6], addr_data[5], addr_data[4]));
  if (rv->dummy != NULL)
    {
      len = 8;
      memcpy (rv->dummy + addr - rv->reg_address, addr_data + 4, 4);
    }
  else
    {
      hw_rv_send (me, RV_WRITE_CMD, addr_data, len);
      hw_rv_handle_incoming (me, RV_WRITE_CMD, addr_data, &len);
    }

  if (len != 8)
    hw_abort (me, "read %d != 8 bytes returned", len);

  /* We had an access: tighten up all slack.  */
  rv->next_period = 1;

  return nr_bytes;
}

/* Instance initializer function.  */

static void
hw_rv_finish (struct hw *me)
{
  hw_rv_device *rv = HW_ZALLOC (me, hw_rv_device);
  int i;
  const struct hw_property *mem_prop;
  const struct hw_property *dummy_prop;
  const struct hw_property *mbox_prop;

  set_hw_data (me, rv);

#undef RV_GET_IPROP
#undef RV_GET_PROP
#define RV_GET_PROP(T, N, M, D)				\
  do							\
    {							\
      if (hw_find_property (me, N) != NULL)		\
	rv->M = hw_find_ ## T ## _property (me, N);	\
      else						\
	rv->M = (D);					\
    }							\
  while (0)
#define RV_GET_IPROP(N, M, D) RV_GET_PROP (integer, N, M, D)

  RV_GET_PROP (string, "host", host, "127.0.0.1");
  RV_GET_IPROP ("port", port, 10000);
  RV_GET_IPROP ("remote-reg", remote_reg_address, 0);
  RV_GET_IPROP ("max-poll-ticks", max_tick_poll_interval, 10000);
  RV_GET_IPROP ("watchdog-interval", watchdog_interval, 30);
  RV_GET_IPROP ("remote-mem", remote_mem_address, 0);
  RV_GET_IPROP ("mem-burst-mask", mem_burst_mask, 0xffff);
  RV_GET_IPROP ("intmultiple", intmultiple, 0);

  set_hw_io_read_buffer (me, hw_rv_reg_read);
  set_hw_io_write_buffer (me, hw_rv_reg_write);
  set_hw_ports (me, hw_rv_ports);
  rv->next_period = 1;

  /* FIXME: We only support zero or one reg and zero or one mem area.  */
  if (hw_find_property (me, "reg") != NULL)
    {
      reg_property_spec reg;
      if (hw_find_reg_array_property (me, "reg", 0, &reg))
	{
	  unsigned_word attach_address;
	  int attach_space;
	  unsigned int attach_size;

	  hw_unit_address_to_attach_address (hw_parent (me),
					     &reg.address,
					     &attach_space,
					     &attach_address,
					     me);
	  rv->reg_address = attach_address;
	  hw_unit_size_to_attach_size (hw_parent (me),
				       &reg.size,
				       &attach_size, me);
	  rv->reg_size = attach_size;
	  if ((attach_address & 3) != 0)
	    hw_abort (me, "register block must be 4 byte aligned");
	  hw_attach_address (hw_parent (me),
			     0,
			     attach_space, attach_address, attach_size,
			     me);
	}
      else
	hw_abort (me, "property \"reg\" has the wrong type");
    }

  dummy_prop = hw_find_property (me, "dummy");
  if (dummy_prop != NULL)
    {
      if (rv->reg_size == 0)
	hw_abort (me, "dummy argument requires a \"reg\" property");

      if (hw_property_type (dummy_prop) == integer_property)
	{
	  unsigned32 dummyfill = hw_find_integer_property (me, "dummy");
	  unsigned8 *dummymem = hw_malloc (me, rv->reg_size);
	  memset (dummymem, dummyfill, rv->reg_size);
	  rv->dummy = dummymem;
	}
      else
	{
	  const char *dummyarg = hw_find_string_property (me, "dummy");
	  unsigned8 *dummymem = hw_malloc (me, rv->reg_size);
	  FILE *f = fopen (dummyarg, "rb");

	  if (f == NULL)
	    hw_abort (me, "opening dummy-file \"%s\": %s",
		      dummyarg, strerror (errno));
	  if (fread (dummymem, 1, rv->reg_size, f) != rv->reg_size)
	    hw_abort (me, "reading dummy-file \"%s\": %s",
		      dummyarg, strerror (errno));
	  fclose (f);
	  rv->dummy = dummymem;
	}
    }

  mbox_prop = hw_find_property (me, "mbox");
  if (mbox_prop != NULL)
    {
      if (hw_property_type (mbox_prop) == integer_property)
	{
	  signed_cell attach_address_sc
	    = hw_find_integer_property (me, "mbox");

	  rv->mbox_address = (unsigned32) attach_address_sc;
	  hw_attach_address (hw_parent (me),
			     0,
			     0, (unsigned32) attach_address_sc, 4, me);
	}
      else
	hw_abort (me, "property \"mbox\" has the wrong type");
    }

  mem_prop = hw_find_property (me, "mem");
  if (mem_prop != NULL)
    {
      signed_cell attach_address_sc;
      signed_cell attach_size_sc;

      /* Only specific names are reg_array_properties, the rest are
	 array_properties.  */
      if (hw_property_type (mem_prop) == array_property
	  && hw_property_sizeof_array (mem_prop) == 2 * sizeof (attach_address_sc)
	  && hw_find_integer_array_property (me, "mem", 0, &attach_address_sc)
	  && hw_find_integer_array_property (me, "mem", 1, &attach_size_sc))
	{
	  /* Unfortunate choice of types forces us to dance around a bit.  */
	  rv->mem_address = (unsigned32) attach_address_sc;
	  rv->mem_size = (unsigned32) attach_size_sc;
	  if ((attach_address_sc & 3) != 0)
	    hw_abort (me, "memory block must be 4 byte aligned");
	}
      else
	hw_abort (me, "property \"mem\" has the wrong type");
    }

  hw_rv_map_ints (me);

  hw_rv_init_socket (me);

  /* We need an extra initialization pass, after all others currently
     scheduled (mostly, after the simulation events machinery has been
     initialized so the events we want don't get thrown out).  */
  hw_rv_add_init (me);
}

/* Our root structure; see dv-* build machinery for usage.  */

const struct hw_descriptor dv_rv_descriptor[] = {
  { RV_FAMILY_NAME, hw_rv_finish },
  { NULL }
};

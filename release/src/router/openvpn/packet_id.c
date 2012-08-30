/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single TCP/UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2010 OpenVPN Technologies, Inc. <sales@openvpn.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * These routines are designed to catch replay attacks,
 * where a man-in-the-middle captures packets and then
 * attempts to replay them back later.
 *
 * We use the "sliding-window" algorithm, similar
 * to IPSec.
 */

#include "syshead.h"

#ifdef USE_CRYPTO

#include "packet_id.h"
#include "misc.h"
#include "integer.h"

#include "memdbg.h"

/*
 * Special time_t value that indicates that
 * sequence number has expired.
 */
#define SEQ_UNSEEN  ((time_t)0)
#define SEQ_EXPIRED ((time_t)1)

void
packet_id_init (struct packet_id *p, int seq_backtrack, int time_backtrack)
{
  dmsg (D_PID_DEBUG_LOW, "PID packet_id_init seq_backtrack=%d time_backtrack=%d",
       seq_backtrack,
       time_backtrack);

  ASSERT (p);
  CLEAR (*p);

  if (seq_backtrack)
    {
      ASSERT (MIN_SEQ_BACKTRACK <= seq_backtrack && seq_backtrack <= MAX_SEQ_BACKTRACK);
      ASSERT (MIN_TIME_BACKTRACK <= time_backtrack && time_backtrack <= MAX_TIME_BACKTRACK);
      CIRC_LIST_ALLOC (p->rec.seq_list, struct seq_list, seq_backtrack);
      p->rec.seq_backtrack = seq_backtrack;
      p->rec.time_backtrack = time_backtrack;
    }
  p->rec.initialized = true;
}

void
packet_id_free (struct packet_id *p)
{
  if (p)
    {
      dmsg (D_PID_DEBUG_LOW, "PID packet_id_free");
      if (p->rec.seq_list)
	free (p->rec.seq_list);
      CLEAR (*p);
    }
}

void
packet_id_add (struct packet_id_rec *p, const struct packet_id_net *pin)
{
  const time_t local_now = now;
  if (p->seq_list)
    {
      packet_id_type diff;

      /*
       * If time value increases, start a new
       * sequence number sequence.
       */
      if (!CIRC_LIST_SIZE (p->seq_list)
	  || pin->time > p->time
	  || (pin->id >= (packet_id_type)p->seq_backtrack
	      && pin->id - (packet_id_type)p->seq_backtrack > p->id))
	{
	  p->time = pin->time;
	  p->id = 0;
	  if (pin->id > (packet_id_type)p->seq_backtrack)
	    p->id = pin->id - (packet_id_type)p->seq_backtrack;
	  CIRC_LIST_RESET (p->seq_list);
	}

      while (p->id < pin->id)
	{
	  CIRC_LIST_PUSH (p->seq_list, SEQ_UNSEEN);
	  ++p->id;
	}

      diff = p->id - pin->id;
      if (diff < (packet_id_type) CIRC_LIST_SIZE (p->seq_list)
	  && local_now > SEQ_EXPIRED)
	CIRC_LIST_ITEM (p->seq_list, diff) = local_now;
    }
  else
    {
      p->time = pin->time;
      p->id = pin->id;
    }
}

/*
 * Expire sequence numbers which can no longer
 * be accepted because they would violate
 * time_backtrack.
 */
void
packet_id_reap (struct packet_id_rec *p)
{
  const time_t local_now = now;
  if (p->time_backtrack)
    {
      int i;
      bool expire = false;
      for (i = 0; i < CIRC_LIST_SIZE (p->seq_list); ++i)
	{
	  const time_t t = CIRC_LIST_ITEM (p->seq_list, i);
	  if (t == SEQ_EXPIRED)
	    break;
	  if (!expire && t && t + p->time_backtrack < local_now)
	    expire = true;
	  if (expire)
	    CIRC_LIST_ITEM (p->seq_list, i) = SEQ_EXPIRED;
	}
    }
  p->last_reap = local_now;
}

/*
 * Return true if packet id is ok, or false if
 * it is a replay.
 */
bool
packet_id_test (const struct packet_id_rec *p,
		const struct packet_id_net *pin)
{
  static int max_backtrack_stat;
  packet_id_type diff;

  dmsg (D_PID_DEBUG,
       "PID TEST " time_format ":" packet_id_format " " time_format ":" packet_id_format "",
       (time_type)p->time, (packet_id_print_type)p->id, (time_type)pin->time,
       (packet_id_print_type)pin->id);

  ASSERT (p->initialized);

  if (!pin->id)
    return false;

  if (p->seq_backtrack)
    {
      /*
       * In backtrack mode, we allow packet reordering subject
       * to the seq_backtrack and time_backtrack constraints.
       *
       * This mode is used with UDP.
       */
      if (pin->time == p->time)
	{
	  /* is packet-id greater than any one we've seen yet? */
	  if (pin->id > p->id)
	    return true;

	  /* check packet-id sliding window for original/replay status */
	  diff = p->id - pin->id;

	  /* keep track of maximum backtrack seen for debugging purposes */
	  if ((int)diff > max_backtrack_stat)
	    {
	      max_backtrack_stat = (int)diff;
	      msg (D_BACKTRACK, "Replay-window backtrack occurred [%d]", max_backtrack_stat);
	    }

	  if (diff >= (packet_id_type) CIRC_LIST_SIZE (p->seq_list))
	    return false;

	  return CIRC_LIST_ITEM (p->seq_list, diff) == 0;
	}
      else if (pin->time < p->time) /* if time goes back, reject */
	return false;
      else                          /* time moved forward */
	return true;
    }
  else
    {
      /*
       * In non-backtrack mode, all sequence number series must
       * begin at some number n > 0 and must increment linearly without gaps.
       *
       * This mode is used with TCP.
       */
      if (pin->time == p->time)
	return !p->id || pin->id == p->id + 1;
      else if (pin->time < p->time) /* if time goes back, reject */
	return false;
      else                          /* time moved forward */
	return pin->id == 1;
    }
}

/*
 * Read/write a packet ID to/from the buffer.  Short form is sequence number
 * only.  Long form is sequence number and timestamp.
 */

bool
packet_id_read (struct packet_id_net *pin, struct buffer *buf, bool long_form)
{
  packet_id_type net_id;
  net_time_t net_time;

  pin->id = 0;
  pin->time = 0;

  if (!buf_read (buf, &net_id, sizeof (net_id)))
    return false;
  pin->id = ntohpid (net_id);
  if (long_form)
    {
      if (!buf_read (buf, &net_time, sizeof (net_time)))
	return false;
      pin->time = ntohtime (net_time);
    }
  return true;
}

bool
packet_id_write (const struct packet_id_net *pin, struct buffer *buf, bool long_form, bool prepend)
{
  packet_id_type net_id = htonpid (pin->id);
  net_time_t net_time = htontime (pin->time);

  if (prepend)
    {
      if (long_form)
	{
	  if (!buf_write_prepend (buf, &net_time, sizeof (net_time)))
	    return false;
	}
      if (!buf_write_prepend (buf, &net_id, sizeof (net_id)))
	return false;
    }
  else
    {
      if (!buf_write (buf, &net_id, sizeof (net_id)))
	return false;
      if (long_form)
	{
	  if (!buf_write (buf, &net_time, sizeof (net_time)))
	    return false;
	}
    }
  return true;
}

const char *
packet_id_net_print (const struct packet_id_net *pin, bool print_timestamp, struct gc_arena *gc)
{
  struct buffer out = alloc_buf_gc (256, gc);

  buf_printf (&out, "[ #" packet_id_format, (packet_id_print_type)pin->id);
  if (print_timestamp && pin->time)
      buf_printf (&out, " / time = (" packet_id_format ") %s", 
		  (packet_id_print_type)pin->time,
		  time_string (pin->time, 0, false, gc));

  buf_printf (&out, " ]");
  return BSTR (&out);
}

/* initialize the packet_id_persist structure in a disabled state */
void
packet_id_persist_init (struct packet_id_persist *p)
{
  p->filename = NULL;
  p->fd = -1;
  p->time = p->time_last_written = 0;
  p->id = p->id_last_written = 0;
}

/* close the file descriptor if it is open, and switch to disabled state */
void
packet_id_persist_close (struct packet_id_persist *p)
{
  if (packet_id_persist_enabled (p))
    {
      if (close (p->fd))
	msg (D_PID_PERSIST | M_ERRNO, "Close error on --replay-persist file %s", p->filename);
      packet_id_persist_init (p);
    }
}

/* load persisted rec packet_id (time and id) only once from file, and set state to enabled */
void
packet_id_persist_load (struct packet_id_persist *p, const char *filename)
{
  struct gc_arena gc = gc_new ();
  if (!packet_id_persist_enabled (p))
    {
      /* open packet-id persist file for both read and write */
      p->fd = open (filename,
		    O_CREAT | O_RDWR | O_BINARY,
		    S_IRUSR | S_IWUSR);
      if (p->fd == -1)
	{
	  msg (D_PID_PERSIST | M_ERRNO,
	       "Cannot open --replay-persist file %s for read/write",
	       filename);
	}
      else
	{
	  struct packet_id_persist_file_image image;
	  ssize_t n;

#if defined(HAVE_FLOCK) && defined(LOCK_EX) && defined(LOCK_NB)
	  if (flock (p->fd, LOCK_EX | LOCK_NB))
	    msg (M_ERR, "Cannot obtain exclusive lock on --replay-persist file %s", filename);
#endif

	  p->filename = filename;
	  n = read (p->fd, &image, sizeof(image));
	  if (n == sizeof(image))
	    {
	      p->time = p->time_last_written = image.time;
	      p->id = p->id_last_written = image.id;
	      dmsg (D_PID_PERSIST_DEBUG, "PID Persist Read from %s: %s",
		   p->filename, packet_id_persist_print (p, &gc));
	    }
	  else if (n == -1)
	    {
	      msg (D_PID_PERSIST | M_ERRNO,
		   "Read error on --replay-persist file %s",
		   p->filename);
	    }
	}
    }
  gc_free (&gc);
}

/* save persisted rec packet_id (time and id) to file (only if enabled state) */
void
packet_id_persist_save (struct packet_id_persist *p)
{
  if (packet_id_persist_enabled (p) && p->time && (p->time != p->time_last_written ||
						   p->id != p->id_last_written))
    {
      struct packet_id_persist_file_image image;
      ssize_t n;
      off_t seek_ret;
      struct gc_arena gc = gc_new ();

      image.time = p->time;
      image.id = p->id;
      seek_ret = lseek(p->fd, (off_t)0, SEEK_SET);
      if (seek_ret == (off_t)0)
	{
	  n = write(p->fd, &image, sizeof(image));
	  if (n == sizeof(image))
	    {
	      p->time_last_written = p->time;
	      p->id_last_written = p->id;
	      dmsg (D_PID_PERSIST_DEBUG, "PID Persist Write to %s: %s",
		   p->filename, packet_id_persist_print (p, &gc));
	    }
	  else
	    {
	      msg (D_PID_PERSIST | M_ERRNO,
		   "Cannot write to --replay-persist file %s",
		   p->filename);
	    }
	}
      else
	{
	  msg (D_PID_PERSIST | M_ERRNO,
	       "Cannot seek to beginning of --replay-persist file %s",
	       p->filename);
	}
      gc_free (&gc);
    }
}

/* transfer packet_id_persist -> packet_id */
void
packet_id_persist_load_obj (const struct packet_id_persist *p, struct packet_id *pid)
{
  if (p && pid && packet_id_persist_enabled (p) && p->time)
    {
      pid->rec.time = p->time;
      pid->rec.id = p->id;
    }
}

const char *
packet_id_persist_print (const struct packet_id_persist *p, struct gc_arena *gc)
{
  struct buffer out = alloc_buf_gc (256, gc);

  buf_printf (&out, "[");

  if (packet_id_persist_enabled (p))
    {
      buf_printf (&out, " #" packet_id_format, (packet_id_print_type)p->id);
      if (p->time)
	buf_printf (&out, " / time = (" packet_id_format ") %s",
		    (packet_id_print_type)p->time,
		    time_string (p->time, 0, false, gc));
    }

  buf_printf (&out, " ]");
  return (char *)out.data;
}

#ifdef PID_TEST

void
packet_id_interactive_test ()
{
  struct packet_id pid;
  struct packet_id_net pin;
  bool long_form;
  bool count = 0;
  bool test;

  const int seq_backtrack = 10;
  const int time_backtrack = 10;

  packet_id_init (&pid, seq_backtrack, time_backtrack);

  while (true) {
    char buf[80];
    if (!fgets(buf, sizeof(buf), stdin))
      break;
    update_time ();
    if (sscanf (buf, "%lu,%u", &pin.time, &pin.id) == 2)
      {
	packet_id_reap_test (&pid.rec);
	test = packet_id_test (&pid.rec, &pin);
	printf ("packet_id_test (" time_format ", " packet_id_format ") returned %d\n",
		(time_type)pin.time,
		(packet_id_print_type)pin.id,
		test);
	if (test)
	  packet_id_add (&pid.rec, &pin);
      }
    else
      {
	long_form = (count < 20);
	packet_id_alloc_outgoing (&pid.send, &pin, long_form);
	printf ("(" time_format "(" packet_id_format "), %d)\n",
		(time_type)pin.time,
		(packet_id_print_type)pin.id,
		long_form);
	if (pid.send.id == 10)
	  pid.send.id = 0xFFFFFFF8;
	++count;
      }
  }
  packet_id_free (&pid);
}
#endif

#endif /* USE_CRYPTO */

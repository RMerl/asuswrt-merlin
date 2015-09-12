/* BGP-4 dump routine
   Copyright (C) 1999 Kunihiro Ishiguro

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

#include <zebra.h>

#include "log.h"
#include "stream.h"
#include "sockunion.h"
#include "command.h"
#include "prefix.h"
#include "thread.h"
#include "linklist.h"
#include "bgpd/bgp_table.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_dump.h"

enum bgp_dump_type
{
  BGP_DUMP_ALL,
  BGP_DUMP_UPDATES,
  BGP_DUMP_ROUTES
};

enum MRT_MSG_TYPES {
   MSG_NULL,
   MSG_START,                   /* sender is starting up */
   MSG_DIE,                     /* receiver should shut down */
   MSG_I_AM_DEAD,               /* sender is shutting down */
   MSG_PEER_DOWN,               /* sender's peer is down */
   MSG_PROTOCOL_BGP,            /* msg is a BGP packet */
   MSG_PROTOCOL_RIP,            /* msg is a RIP packet */
   MSG_PROTOCOL_IDRP,           /* msg is an IDRP packet */
   MSG_PROTOCOL_RIPNG,          /* msg is a RIPNG packet */
   MSG_PROTOCOL_BGP4PLUS,       /* msg is a BGP4+ packet */
   MSG_PROTOCOL_BGP4PLUS_01,    /* msg is a BGP4+ (draft 01) packet */
   MSG_PROTOCOL_OSPF,           /* msg is an OSPF packet */
   MSG_TABLE_DUMP,              /* routing table dump */
   MSG_TABLE_DUMP_V2            /* routing table dump, version 2 */
};

static int bgp_dump_interval_func (struct thread *);

struct bgp_dump
{
  enum bgp_dump_type type;

  char *filename;

  FILE *fp;

  unsigned int interval;

  char *interval_str;

  struct thread *t_interval;
};

/* BGP packet dump output buffer. */
struct stream *bgp_dump_obuf;

/* BGP dump strucuture for 'dump bgp all' */
struct bgp_dump bgp_dump_all;

/* BGP dump structure for 'dump bgp updates' */
struct bgp_dump bgp_dump_updates;

/* BGP dump structure for 'dump bgp routes' */
struct bgp_dump bgp_dump_routes;

/* Dump whole BGP table is very heavy process.  */
struct thread *t_bgp_dump_routes;

/* Some define for BGP packet dump. */
static FILE *
bgp_dump_open_file (struct bgp_dump *bgp_dump)
{
  int ret;
  time_t clock;
  struct tm *tm;
  char fullpath[MAXPATHLEN];
  char realpath[MAXPATHLEN];
  mode_t oldumask;

  time (&clock);
  tm = localtime (&clock);

  if (bgp_dump->filename[0] != DIRECTORY_SEP)
    {
      sprintf (fullpath, "%s/%s", vty_get_cwd (), bgp_dump->filename);
      ret = strftime (realpath, MAXPATHLEN, fullpath, tm);
    }
  else
    ret = strftime (realpath, MAXPATHLEN, bgp_dump->filename, tm);

  if (ret == 0)
    {
      zlog_warn ("bgp_dump_open_file: strftime error");
      return NULL;
    }

  if (bgp_dump->fp)
    fclose (bgp_dump->fp);


  oldumask = umask(0777 & ~LOGFILE_MASK);
  bgp_dump->fp = fopen (realpath, "w");

  if (bgp_dump->fp == NULL)
    {
      zlog_warn ("bgp_dump_open_file: %s: %s", realpath, strerror (errno));
      umask(oldumask);
      return NULL;
    }
  umask(oldumask);  

  return bgp_dump->fp;
}

static int
bgp_dump_interval_add (struct bgp_dump *bgp_dump, int interval)
{
  int secs_into_day;
  time_t t;
  struct tm *tm;

  if (interval > 0)
    {
      /* Periodic dump every interval seconds */
      if ((interval < 86400) && ((86400 % interval) == 0))
	{
	  /* Dump at predictable times: if a day has a whole number of
	   * intervals, dump every interval seconds starting from midnight
	   */
	  (void) time(&t);
	  tm = localtime(&t);
	  secs_into_day = tm->tm_sec + 60*tm->tm_min + 60*60*tm->tm_hour;
	  interval = interval - secs_into_day % interval; /* always > 0 */
	}
      bgp_dump->t_interval = thread_add_timer (master, bgp_dump_interval_func, 
					       bgp_dump, interval);
    }
  else
    {
      /* One-off dump: execute immediately, don't affect any scheduled dumps */
      bgp_dump->t_interval = thread_add_event (master, bgp_dump_interval_func,
					       bgp_dump, 0);
    }

  return 0;
}

/* Dump common header. */
static void
bgp_dump_header (struct stream *obuf, int type, int subtype)
{
  time_t now;

  /* Set header. */
  time (&now);

  /* Put dump packet header. */
  stream_putl (obuf, now);	
  stream_putw (obuf, type);
  stream_putw (obuf, subtype);

  stream_putl (obuf, 0);	/* len */
}

static void
bgp_dump_set_size (struct stream *s, int type)
{
  stream_putl_at (s, 8, stream_get_endp (s) - BGP_DUMP_HEADER_SIZE);
}

static void
bgp_dump_routes_index_table(struct bgp *bgp)
{
  struct peer *peer;
  struct listnode *node;
  uint16_t peerno = 0;
  struct stream *obuf;

  obuf = bgp_dump_obuf;
  stream_reset (obuf);

  /* MRT header */
  bgp_dump_header (obuf, MSG_TABLE_DUMP_V2, TABLE_DUMP_V2_PEER_INDEX_TABLE);

  /* Collector BGP ID */
  stream_put_in_addr (obuf, &bgp->router_id);

  /* View name */
  if(bgp->name)
    {
      stream_putw (obuf, strlen(bgp->name));
      stream_put(obuf, bgp->name, strlen(bgp->name));
    }
  else
    {
      stream_putw(obuf, 0);
    }

  /* Peer count */
  stream_putw (obuf, listcount(bgp->peer));

  /* Walk down all peers */
  for(ALL_LIST_ELEMENTS_RO (bgp->peer, node, peer))
    {

      /* Peer's type */
      if (sockunion_family(&peer->su) == AF_INET)
        {
          stream_putc (obuf, TABLE_DUMP_V2_PEER_INDEX_TABLE_AS4+TABLE_DUMP_V2_PEER_INDEX_TABLE_IP);
        }
#ifdef HAVE_IPV6
      else if (sockunion_family(&peer->su) == AF_INET6)
        {
          stream_putc (obuf, TABLE_DUMP_V2_PEER_INDEX_TABLE_AS4+TABLE_DUMP_V2_PEER_INDEX_TABLE_IP6);
        }
#endif /* HAVE_IPV6 */

      /* Peer's BGP ID */
      stream_put_in_addr (obuf, &peer->remote_id);

      /* Peer's IP address */
      if (sockunion_family(&peer->su) == AF_INET)
        {
          stream_put_in_addr (obuf, &peer->su.sin.sin_addr);
        }
#ifdef HAVE_IPV6
      else if (sockunion_family(&peer->su) == AF_INET6)
        {
          stream_write (obuf, (u_char *)&peer->su.sin6.sin6_addr,
                        IPV6_MAX_BYTELEN);
        }
#endif /* HAVE_IPV6 */

      /* Peer's AS number. */
      /* Note that, as this is an AS4 compliant quagga, the RIB is always AS4 */
      stream_putl (obuf, peer->as);

      /* Store the peer number for this peer */
      peer->table_dump_index = peerno;
      peerno++;
    }

  bgp_dump_set_size(obuf, MSG_TABLE_DUMP_V2);

  fwrite (STREAM_DATA (obuf), stream_get_endp (obuf), 1, bgp_dump_routes.fp);
  fflush (bgp_dump_routes.fp);
}


/* Runs under child process. */
static unsigned int
bgp_dump_routes_func (int afi, int first_run, unsigned int seq)
{
  struct stream *obuf;
  struct bgp_info *info;
  struct bgp_node *rn;
  struct bgp *bgp;
  struct bgp_table *table;

  bgp = bgp_get_default ();
  if (!bgp)
    return seq;

  if (bgp_dump_routes.fp == NULL)
    return seq;

  /* Note that bgp_dump_routes_index_table will do ipv4 and ipv6 peers,
     so this should only be done on the first call to bgp_dump_routes_func.
     ( this function will be called once for ipv4 and once for ipv6 ) */
  if(first_run)
    bgp_dump_routes_index_table(bgp);

  obuf = bgp_dump_obuf;
  stream_reset(obuf);

  /* Walk down each BGP route. */
  table = bgp->rib[afi][SAFI_UNICAST];

  for (rn = bgp_table_top (table); rn; rn = bgp_route_next (rn))
    {
      if(!rn->info)
        continue;

      stream_reset(obuf);

      /* MRT header */
      if (afi == AFI_IP)
        {
          bgp_dump_header (obuf, MSG_TABLE_DUMP_V2, TABLE_DUMP_V2_RIB_IPV4_UNICAST);
        }
#ifdef HAVE_IPV6
      else if (afi == AFI_IP6)
        {
          bgp_dump_header (obuf, MSG_TABLE_DUMP_V2, TABLE_DUMP_V2_RIB_IPV6_UNICAST);
        }
#endif /* HAVE_IPV6 */

      /* Sequence number */
      stream_putl(obuf, seq);

      /* Prefix length */
      stream_putc (obuf, rn->p.prefixlen);

      /* Prefix */
      if (afi == AFI_IP)
        {
          /* We'll dump only the useful bits (those not 0), but have to align on 8 bits */
          stream_write(obuf, (u_char *)&rn->p.u.prefix4, (rn->p.prefixlen+7)/8);
        }
#ifdef HAVE_IPV6
      else if (afi == AFI_IP6)
        {
          /* We'll dump only the useful bits (those not 0), but have to align on 8 bits */
          stream_write (obuf, (u_char *)&rn->p.u.prefix6, (rn->p.prefixlen+7)/8);
        }
#endif /* HAVE_IPV6 */

      /* Save where we are now, so we can overwride the entry count later */
      int sizep = stream_get_endp(obuf);

      /* Entry count */
      uint16_t entry_count = 0;

      /* Entry count, note that this is overwritten later */
      stream_putw(obuf, 0);

      for (info = rn->info; info; info = info->next)
        {
          entry_count++;

          /* Peer index */
          stream_putw(obuf, info->peer->table_dump_index);

          /* Originated */
#ifdef HAVE_CLOCK_MONOTONIC
          stream_putl (obuf, time(NULL) - (bgp_clock() - info->uptime));
#else
          stream_putl (obuf, info->uptime);
#endif /* HAVE_CLOCK_MONOTONIC */

          /* Dump attribute. */
          /* Skip prefix & AFI/SAFI for MP_NLRI */
          bgp_dump_routes_attr (obuf, info->attr, &rn->p);
        }

      /* Overwrite the entry count, now that we know the right number */
      stream_putw_at (obuf, sizep, entry_count);

      seq++;

      bgp_dump_set_size(obuf, MSG_TABLE_DUMP_V2);
      fwrite (STREAM_DATA (obuf), stream_get_endp (obuf), 1, bgp_dump_routes.fp);

    }

  fflush (bgp_dump_routes.fp);

  return seq;
}

static int
bgp_dump_interval_func (struct thread *t)
{
  struct bgp_dump *bgp_dump;
  bgp_dump = THREAD_ARG (t);
  bgp_dump->t_interval = NULL;

  /* Reschedule dump even if file couldn't be opened this time... */
  if (bgp_dump_open_file (bgp_dump) != NULL)
    {
      /* In case of bgp_dump_routes, we need special route dump function. */
      if (bgp_dump->type == BGP_DUMP_ROUTES)
	{
	  unsigned int seq = bgp_dump_routes_func (AFI_IP, 1, 0);
#ifdef HAVE_IPV6
	  bgp_dump_routes_func (AFI_IP6, 0, seq);
#endif /* HAVE_IPV6 */
	  /* Close the file now. For a RIB dump there's no point in leaving
	   * it open until the next scheduled dump starts. */
	  fclose(bgp_dump->fp); bgp_dump->fp = NULL;
	}
    }

  /* if interval is set reschedule */
  if (bgp_dump->interval > 0)
    bgp_dump_interval_add (bgp_dump, bgp_dump->interval);

  return 0;
}

/* Dump common information. */
static void
bgp_dump_common (struct stream *obuf, struct peer *peer, int forceas4)
{
  char empty[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

  /* Source AS number and Destination AS number. */
  if (forceas4 || CHECK_FLAG (peer->cap, PEER_CAP_AS4_RCV) )
    {
      stream_putl (obuf, peer->as);
      stream_putl (obuf, peer->local_as);
    }
  else
    {
      stream_putw (obuf, peer->as);
      stream_putw (obuf, peer->local_as);
    }

  if (peer->su.sa.sa_family == AF_INET)
    {
      stream_putw (obuf, peer->ifindex);
      stream_putw (obuf, AFI_IP);

      stream_put (obuf, &peer->su.sin.sin_addr, IPV4_MAX_BYTELEN);

      if (peer->su_local)
	stream_put (obuf, &peer->su_local->sin.sin_addr, IPV4_MAX_BYTELEN);
      else
	stream_put (obuf, empty, IPV4_MAX_BYTELEN);
    }
#ifdef HAVE_IPV6
  else if (peer->su.sa.sa_family == AF_INET6)
    {
      /* Interface Index and Address family. */
      stream_putw (obuf, peer->ifindex);
      stream_putw (obuf, AFI_IP6);

      /* Source IP Address and Destination IP Address. */
      stream_put (obuf, &peer->su.sin6.sin6_addr, IPV6_MAX_BYTELEN);

      if (peer->su_local)
	stream_put (obuf, &peer->su_local->sin6.sin6_addr, IPV6_MAX_BYTELEN);
      else
	stream_put (obuf, empty, IPV6_MAX_BYTELEN);
    }
#endif /* HAVE_IPV6 */
}

/* Dump BGP status change. */
void
bgp_dump_state (struct peer *peer, int status_old, int status_new)
{
  struct stream *obuf;

  /* If dump file pointer is disabled return immediately. */
  if (bgp_dump_all.fp == NULL)
    return;

  /* Make dump stream. */
  obuf = bgp_dump_obuf;
  stream_reset (obuf);

  bgp_dump_header (obuf, MSG_PROTOCOL_BGP4MP, BGP4MP_STATE_CHANGE_AS4);
  bgp_dump_common (obuf, peer, 1);/* force this in as4speak*/

  stream_putw (obuf, status_old);
  stream_putw (obuf, status_new);

  /* Set length. */
  bgp_dump_set_size (obuf, MSG_PROTOCOL_BGP4MP);

  /* Write to the stream. */
  fwrite (STREAM_DATA (obuf), stream_get_endp (obuf), 1, bgp_dump_all.fp);
  fflush (bgp_dump_all.fp);
}

static void
bgp_dump_packet_func (struct bgp_dump *bgp_dump, struct peer *peer,
		      struct stream *packet)
{
  struct stream *obuf;

  /* If dump file pointer is disabled return immediately. */
  if (bgp_dump->fp == NULL)
    return;

  /* Make dump stream. */
  obuf = bgp_dump_obuf;
  stream_reset (obuf);

  /* Dump header and common part. */
  if (CHECK_FLAG (peer->cap, PEER_CAP_AS4_RCV) )
    { 
      bgp_dump_header (obuf, MSG_PROTOCOL_BGP4MP, BGP4MP_MESSAGE_AS4);
    }
  else
    {
      bgp_dump_header (obuf, MSG_PROTOCOL_BGP4MP, BGP4MP_MESSAGE);
    }
  bgp_dump_common (obuf, peer, 0);

  /* Packet contents. */
  stream_put (obuf, STREAM_DATA (packet), stream_get_endp (packet));
  
  /* Set length. */
  bgp_dump_set_size (obuf, MSG_PROTOCOL_BGP4MP);

  /* Write to the stream. */
  fwrite (STREAM_DATA (obuf), stream_get_endp (obuf), 1, bgp_dump->fp);
  fflush (bgp_dump->fp);
}

/* Called from bgp_packet.c when BGP packet is received. */
void
bgp_dump_packet (struct peer *peer, int type, struct stream *packet)
{
  /* bgp_dump_all. */
  bgp_dump_packet_func (&bgp_dump_all, peer, packet);

  /* bgp_dump_updates. */
  if (type == BGP_MSG_UPDATE)
    bgp_dump_packet_func (&bgp_dump_updates, peer, packet);
}

static unsigned int
bgp_dump_parse_time (const char *str)
{
  int i;
  int len;
  int seen_h;
  int seen_m;
  int time;
  unsigned int total;

  time = 0;
  total = 0;
  seen_h = 0;
  seen_m = 0;
  len = strlen (str);

  for (i = 0; i < len; i++)
    {
      if (isdigit ((int) str[i]))
	{
	  time *= 10;
	  time += str[i] - '0';
	}
      else if (str[i] == 'H' || str[i] == 'h')
	{
	  if (seen_h)
	    return 0;
	  if (seen_m)
	    return 0;
	  total += time * 60 *60;
	  time = 0;
	  seen_h = 1;
	}
      else if (str[i] == 'M' || str[i] == 'm')
	{
	  if (seen_m)
	    return 0;
	  total += time * 60;
	  time = 0;
	  seen_h = 1;
	}
      else
	return 0;
    }
  return total + time;
}

static int
bgp_dump_set (struct vty *vty, struct bgp_dump *bgp_dump,
              enum bgp_dump_type type, const char *path,
              const char *interval_str)
{
  unsigned int interval;
  
  if (interval_str)
    {
      
      /* Check interval string. */
      interval = bgp_dump_parse_time (interval_str);
      if (interval == 0)
	{
	  vty_out (vty, "Malformed interval string%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}

      /* Don't schedule duplicate dumps if the dump command is given twice */
      if (interval == bgp_dump->interval &&
	  type == bgp_dump->type &&
          path && bgp_dump->filename && !strcmp (path, bgp_dump->filename))
	{
          return CMD_SUCCESS;
	}

      /* Set interval. */
      bgp_dump->interval = interval;
      if (bgp_dump->interval_str)
	free (bgp_dump->interval_str);
      bgp_dump->interval_str = strdup (interval_str);
      
    }
  else
    {
      interval = 0;
    }
    
  /* Create interval thread. */
  bgp_dump_interval_add (bgp_dump, interval);

  /* Set type. */
  bgp_dump->type = type;

  /* Set file name. */
  if (bgp_dump->filename)
    free (bgp_dump->filename);
  bgp_dump->filename = strdup (path);

  /* This should be called when interval is expired. */
  bgp_dump_open_file (bgp_dump);

  return CMD_SUCCESS;
}

static int
bgp_dump_unset (struct vty *vty, struct bgp_dump *bgp_dump)
{
  /* Set file name. */
  if (bgp_dump->filename)
    {
      free (bgp_dump->filename);
      bgp_dump->filename = NULL;
    }

  /* This should be called when interval is expired. */
  if (bgp_dump->fp)
    {
      fclose (bgp_dump->fp);
      bgp_dump->fp = NULL;
    }

  /* Create interval thread. */
  if (bgp_dump->t_interval)
    {
      thread_cancel (bgp_dump->t_interval);
      bgp_dump->t_interval = NULL;
    }

  bgp_dump->interval = 0;

  if (bgp_dump->interval_str)
    {
      free (bgp_dump->interval_str);
      bgp_dump->interval_str = NULL;
    }
  

  return CMD_SUCCESS;
}

DEFUN (dump_bgp_all,
       dump_bgp_all_cmd,
       "dump bgp all PATH",
       "Dump packet\n"
       "BGP packet dump\n"
       "Dump all BGP packets\n"
       "Output filename\n")
{
  return bgp_dump_set (vty, &bgp_dump_all, BGP_DUMP_ALL, argv[0], NULL);
}

DEFUN (dump_bgp_all_interval,
       dump_bgp_all_interval_cmd,
       "dump bgp all PATH INTERVAL",
       "Dump packet\n"
       "BGP packet dump\n"
       "Dump all BGP packets\n"
       "Output filename\n"
       "Interval of output\n")
{
  return bgp_dump_set (vty, &bgp_dump_all, BGP_DUMP_ALL, argv[0], argv[1]);
}

DEFUN (no_dump_bgp_all,
       no_dump_bgp_all_cmd,
       "no dump bgp all [PATH] [INTERVAL]",
       NO_STR
       "Dump packet\n"
       "BGP packet dump\n"
       "Dump all BGP packets\n")
{
  return bgp_dump_unset (vty, &bgp_dump_all);
}

DEFUN (dump_bgp_updates,
       dump_bgp_updates_cmd,
       "dump bgp updates PATH",
       "Dump packet\n"
       "BGP packet dump\n"
       "Dump BGP updates only\n"
       "Output filename\n")
{
  return bgp_dump_set (vty, &bgp_dump_updates, BGP_DUMP_UPDATES, argv[0], NULL);
}

DEFUN (dump_bgp_updates_interval,
       dump_bgp_updates_interval_cmd,
       "dump bgp updates PATH INTERVAL",
       "Dump packet\n"
       "BGP packet dump\n"
       "Dump BGP updates only\n"
       "Output filename\n"
       "Interval of output\n")
{
  return bgp_dump_set (vty, &bgp_dump_updates, BGP_DUMP_UPDATES, argv[0], argv[1]);
}

DEFUN (no_dump_bgp_updates,
       no_dump_bgp_updates_cmd,
       "no dump bgp updates [PATH] [INTERVAL]",
       NO_STR
       "Dump packet\n"
       "BGP packet dump\n"
       "Dump BGP updates only\n")
{
  return bgp_dump_unset (vty, &bgp_dump_updates);
}

DEFUN (dump_bgp_routes,
       dump_bgp_routes_cmd,
       "dump bgp routes-mrt PATH",
       "Dump packet\n"
       "BGP packet dump\n"
       "Dump whole BGP routing table\n"
       "Output filename\n")
{
  return bgp_dump_set (vty, &bgp_dump_routes, BGP_DUMP_ROUTES, argv[0], NULL);
}

DEFUN (dump_bgp_routes_interval,
       dump_bgp_routes_interval_cmd,
       "dump bgp routes-mrt PATH INTERVAL",
       "Dump packet\n"
       "BGP packet dump\n"
       "Dump whole BGP routing table\n"
       "Output filename\n"
       "Interval of output\n")
{
  return bgp_dump_set (vty, &bgp_dump_routes, BGP_DUMP_ROUTES, argv[0], argv[1]);
}

DEFUN (no_dump_bgp_routes,
       no_dump_bgp_routes_cmd,
       "no dump bgp routes-mrt [PATH] [INTERVAL]",
       NO_STR
       "Dump packet\n"
       "BGP packet dump\n"
       "Dump whole BGP routing table\n")
{
  return bgp_dump_unset (vty, &bgp_dump_routes);
}

/* BGP node structure. */
static struct cmd_node bgp_dump_node =
{
  DUMP_NODE,
  "",
  1
};

#if 0
char *
config_time2str (unsigned int interval)
{
  static char buf[BUFSIZ];

  buf[0] = '\0';

  if (interval / 3600)
    {
      sprintf (buf, "%dh", interval / 3600);
      interval %= 3600;
    }
  if (interval / 60)
    {
      sprintf (buf + strlen (buf), "%dm", interval /60);
      interval %= 60;
    }
  if (interval)
    {
      sprintf (buf + strlen (buf), "%d", interval);
    }
  return buf;
}
#endif

static int
config_write_bgp_dump (struct vty *vty)
{
  if (bgp_dump_all.filename)
    {
      if (bgp_dump_all.interval_str)
	vty_out (vty, "dump bgp all %s %s%s", 
		 bgp_dump_all.filename, bgp_dump_all.interval_str,
		 VTY_NEWLINE);
      else
	vty_out (vty, "dump bgp all %s%s", 
		 bgp_dump_all.filename, VTY_NEWLINE);
    }
  if (bgp_dump_updates.filename)
    {
      if (bgp_dump_updates.interval_str)
	vty_out (vty, "dump bgp updates %s %s%s", 
		 bgp_dump_updates.filename, bgp_dump_updates.interval_str,
		 VTY_NEWLINE);
      else
	vty_out (vty, "dump bgp updates %s%s", 
		 bgp_dump_updates.filename, VTY_NEWLINE);
    }
  if (bgp_dump_routes.filename)
    {
      if (bgp_dump_routes.interval_str)
	vty_out (vty, "dump bgp routes-mrt %s %s%s", 
		 bgp_dump_routes.filename, bgp_dump_routes.interval_str,
		 VTY_NEWLINE);
      else
	vty_out (vty, "dump bgp routes-mrt %s%s", 
		 bgp_dump_routes.filename, VTY_NEWLINE);
    }
  return 0;
}

/* Initialize BGP packet dump functionality. */
void
bgp_dump_init (void)
{
  memset (&bgp_dump_all, 0, sizeof (struct bgp_dump));
  memset (&bgp_dump_updates, 0, sizeof (struct bgp_dump));
  memset (&bgp_dump_routes, 0, sizeof (struct bgp_dump));

  bgp_dump_obuf = stream_new (BGP_MAX_PACKET_SIZE + BGP_DUMP_MSG_HEADER
                              + BGP_DUMP_HEADER_SIZE);

  install_node (&bgp_dump_node, config_write_bgp_dump);

  install_element (CONFIG_NODE, &dump_bgp_all_cmd);
  install_element (CONFIG_NODE, &dump_bgp_all_interval_cmd);
  install_element (CONFIG_NODE, &no_dump_bgp_all_cmd);
  install_element (CONFIG_NODE, &dump_bgp_updates_cmd);
  install_element (CONFIG_NODE, &dump_bgp_updates_interval_cmd);
  install_element (CONFIG_NODE, &no_dump_bgp_updates_cmd);
  install_element (CONFIG_NODE, &dump_bgp_routes_cmd);
  install_element (CONFIG_NODE, &dump_bgp_routes_interval_cmd);
  install_element (CONFIG_NODE, &no_dump_bgp_routes_cmd);
}

void
bgp_dump_finish (void)
{
  stream_free (bgp_dump_obuf);
  bgp_dump_obuf = NULL;
}

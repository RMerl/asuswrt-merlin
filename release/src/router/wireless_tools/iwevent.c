/*
 *	Wireless Tools
 *
 *		Jean II - HPL 99->04
 *
 * Main code for "iwevent". This listent for wireless events on rtnetlink.
 * You need to link this code against "iwcommon.c" and "-lm".
 *
 * Part of this code is from Alexey Kuznetsov, part is from Casey Carter,
 * I've just put the pieces together...
 * By the way, if you know a way to remove the root restrictions, tell me
 * about it...
 *
 * This file is released under the GPL license.
 *     Copyright (c) 1997-2004 Jean Tourrilhes <jt@hpl.hp.com>
 */

/***************************** INCLUDES *****************************/

#include "iwlib.h"		/* Header */

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <getopt.h>
#include <time.h>
#include <sys/time.h>

/* Ugly backward compatibility :-( */
#ifndef IFLA_WIRELESS
#define IFLA_WIRELESS	(IFLA_MASTER + 1)
#endif /* IFLA_WIRELESS */

/****************************** TYPES ******************************/

/*
 * Static information about wireless interface.
 * We cache this info for performance reason.
 */
typedef struct wireless_iface
{
  /* Linked list */
  struct wireless_iface *	next;

  /* Interface identification */
  int		ifindex;		/* Interface index == black magic */

  /* Interface data */
  char			ifname[IFNAMSIZ + 1];	/* Interface name */
  struct iw_range	range;			/* Wireless static data */
  int			has_range;
} wireless_iface;

/**************************** VARIABLES ****************************/

/* Cache of wireless interfaces */
struct wireless_iface *	interface_cache = NULL;

/************************ RTNETLINK HELPERS ************************/
/*
 * The following code is extracted from :
 * ----------------------------------------------
 * libnetlink.c	RTnetlink service routines.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 * -----------------------------------------------
 */

struct rtnl_handle
{
	int			fd;
	struct sockaddr_nl	local;
	struct sockaddr_nl	peer;
	__u32			seq;
	__u32			dump;
};

static inline void rtnl_close(struct rtnl_handle *rth)
{
	close(rth->fd);
}

static inline int rtnl_open(struct rtnl_handle *rth, unsigned subscriptions)
{
	int addr_len;

	memset(rth, 0, sizeof(rth));

	rth->fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (rth->fd < 0) {
		perror("Cannot open netlink socket");
		return -1;
	}

	memset(&rth->local, 0, sizeof(rth->local));
	rth->local.nl_family = AF_NETLINK;
	rth->local.nl_groups = subscriptions;

	if (bind(rth->fd, (struct sockaddr*)&rth->local, sizeof(rth->local)) < 0) {
		perror("Cannot bind netlink socket");
		return -1;
	}
	addr_len = sizeof(rth->local);
	if (getsockname(rth->fd, (struct sockaddr*)&rth->local,
			(socklen_t *) &addr_len) < 0) {
		perror("Cannot getsockname");
		return -1;
	}
	if (addr_len != sizeof(rth->local)) {
		fprintf(stderr, "Wrong address length %d\n", addr_len);
		return -1;
	}
	if (rth->local.nl_family != AF_NETLINK) {
		fprintf(stderr, "Wrong address family %d\n", rth->local.nl_family);
		return -1;
	}
	rth->seq = time(NULL);
	return 0;
}

/******************* WIRELESS INTERFACE DATABASE *******************/
/*
 * We keep a few information about each wireless interface on the
 * system. This avoid to query this info at each event, therefore
 * reducing overhead.
 *
 * Each interface is indexed by the 'ifindex'. As opposed to interface
 * names, 'ifindex' are never reused (even if you reactivate the same
 * hardware), so the data we cache will never apply to the wrong
 * interface.
 * Because of that, we are pretty lazy when it come to purging the
 * cache...
 */

/*------------------------------------------------------------------*/
/*
 * Get name of interface based on interface index...
 */
static inline int
index2name(int		skfd,
	   int		ifindex,
	   char *	name)
{
  struct ifreq	irq;
  int		ret = 0;

  memset(name, 0, IFNAMSIZ + 1);

  /* Get interface name */
  irq.ifr_ifindex = ifindex;
  if(ioctl(skfd, SIOCGIFNAME, &irq) < 0)
    ret = -1;
  else
    strncpy(name, irq.ifr_name, IFNAMSIZ);

  return(ret);
}

/*------------------------------------------------------------------*/
/*
 * Get interface data from cache or live interface
 */
static struct wireless_iface *
iw_get_interface_data(int	ifindex)
{
  struct wireless_iface *	curr;
  int				skfd = -1;	/* ioctl socket */

  /* Search for it in the database */
  curr = interface_cache;
  while(curr != NULL)
    {
      /* Match ? */
      if(curr->ifindex == ifindex)
	{
	  //printf("Cache : found %d-%s\n", curr->ifindex, curr->ifname);

	  /* Return */
	  return(curr);
	}
      /* Next entry */
      curr = curr->next;
    }

  /* Create a channel to the NET kernel. Doesn't happen too often, so
   * socket creation overhead is minimal... */
  if((skfd = iw_sockets_open()) < 0)
    {
      perror("iw_sockets_open");
      return(NULL);
    }

  /* Create new entry, zero, init */
  curr = calloc(1, sizeof(struct wireless_iface));
  if(!curr)
    {
      fprintf(stderr, "Malloc failed\n");
      return(NULL);
    }
  curr->ifindex = ifindex;

  /* Extract static data */
  if(index2name(skfd, ifindex, curr->ifname) < 0)
    {
      perror("index2name");
      free(curr);
      return(NULL);
    }
  curr->has_range = (iw_get_range_info(skfd, curr->ifname, &curr->range) >= 0);
  //printf("Cache : create %d-%s\n", curr->ifindex, curr->ifname);

  /* Done */
  iw_sockets_close(skfd);

  /* Link it */
  curr->next = interface_cache;
  interface_cache = curr;

  return(curr);
}

/*------------------------------------------------------------------*/
/*
 * Remove interface data from cache (if it exist)
 */
static void
iw_del_interface_data(int	ifindex)
{
  struct wireless_iface *	curr;
  struct wireless_iface *	prev = NULL;
  struct wireless_iface *	next;

  /* Go through the list, find the interface, kills it */
  curr = interface_cache;
  while(curr)
    {
      next = curr->next;

      /* Got a match ? */
      if(curr->ifindex == ifindex)
	{
	  /* Unlink. Root ? */
	  if(!prev)
	    interface_cache = next;
	  else
	    prev->next = next;
	  //printf("Cache : purge %d-%s\n", curr->ifindex, curr->ifname);

	  /* Destroy */
	  free(curr);
	}
      else
	{
	  /* Keep as previous */
	  prev = curr;
	}

      /* Next entry */
      curr = next;
    }
}

/********************* WIRELESS EVENT DECODING *********************/
/*
 * Parse the Wireless Event and print it out
 */

/*------------------------------------------------------------------*/
/*
 * Dump a buffer as a serie of hex
 * Maybe should go in iwlib...
 * Maybe we should have better formatting like iw_print_key...
 */
static char *
iw_hexdump(char *		buf,
	   size_t		buflen,
	   const unsigned char *data,
	   size_t		datalen)
{
  size_t	i;
  char *	pos = buf;

  for(i = 0; i < datalen; i++)
    pos += snprintf(pos, buf + buflen - pos, "%02X", data[i]);
  return buf;
}

/*------------------------------------------------------------------*/
/*
 * Print one element from the scanning results
 */
static inline int
print_event_token(struct iw_event *	event,		/* Extracted token */
		  struct iw_range *	iw_range,	/* Range info */
		  int			has_range)
{
  char		buffer[128];	/* Temporary buffer */
  char		buffer2[30];	/* Temporary buffer */
  char *	prefix = (IW_IS_GET(event->cmd) ? "New" : "Set");

  /* Now, let's decode the event */
  switch(event->cmd)
    {
      /* ----- set events ----- */
      /* Events that result from a "SET XXX" operation by the user */
    case SIOCSIWNWID:
      if(event->u.nwid.disabled)
	printf("Set NWID:off/any\n");
      else
	printf("Set NWID:%X\n", event->u.nwid.value);
      break;
    case SIOCSIWFREQ:
    case SIOCGIWFREQ:
      {
	double		freq;			/* Frequency/channel */
	int		channel = -1;		/* Converted to channel */
	freq = iw_freq2float(&(event->u.freq));
	if(has_range)
	  {
	    if(freq < KILO)
	      /* Convert channel to frequency if possible */
	      channel = iw_channel_to_freq((int) freq, &freq, iw_range);
	    else
	      /* Convert frequency to channel if possible */
	      channel = iw_freq_to_channel(freq, iw_range);
	  }
	iw_print_freq(buffer, sizeof(buffer),
		      freq, channel, event->u.freq.flags);
	printf("%s %s\n", prefix, buffer);
      }
      break;
    case SIOCSIWMODE:
      printf("Set Mode:%s\n",
	     iw_operation_mode[event->u.mode]);
      break;
    case SIOCSIWESSID:
    case SIOCGIWESSID:
      {
	char essid[IW_ESSID_MAX_SIZE+1];
	memset(essid, '\0', sizeof(essid));
	if((event->u.essid.pointer) && (event->u.essid.length))
	  memcpy(essid, event->u.essid.pointer, event->u.essid.length);
	if(event->u.essid.flags)
	  {
	    /* Does it have an ESSID index ? */
	    if((event->u.essid.flags & IW_ENCODE_INDEX) > 1)
	      printf("%s ESSID:\"%s\" [%d]\n", prefix, essid,
		     (event->u.essid.flags & IW_ENCODE_INDEX));
	    else
	      printf("%s ESSID:\"%s\"\n", prefix, essid);
	  }
	else
	  printf("%s ESSID:off/any\n", prefix);
      }
      break;
    case SIOCSIWENCODE:
      {
	unsigned char	key[IW_ENCODING_TOKEN_MAX];
	if(event->u.data.pointer)
	  memcpy(key, event->u.data.pointer, event->u.data.length);
	else
	  event->u.data.flags |= IW_ENCODE_NOKEY;
	printf("Set Encryption key:");
	if(event->u.data.flags & IW_ENCODE_DISABLED)
	  printf("off\n");
	else
	  {
	    /* Display the key */
	    iw_print_key(buffer, sizeof(buffer), key, event->u.data.length,
			 event->u.data.flags);
	    printf("%s", buffer);

	    /* Other info... */
	    if((event->u.data.flags & IW_ENCODE_INDEX) > 1)
	      printf(" [%d]", event->u.data.flags & IW_ENCODE_INDEX);
	    if(event->u.data.flags & IW_ENCODE_RESTRICTED)
	      printf("   Security mode:restricted");
	    if(event->u.data.flags & IW_ENCODE_OPEN)
	      printf("   Security mode:open");
	    printf("\n");
	  }
      }
      break;
      /* ----- driver events ----- */
      /* Events generated by the driver when something important happens */
    case SIOCGIWAP:
      printf("New Access Point/Cell address:%s\n",
	     iw_sawap_ntop(&event->u.ap_addr, buffer));
      break;
    case SIOCGIWSCAN:
      printf("Scan request completed\n");
      break;
    case IWEVTXDROP:
      printf("Tx packet dropped:%s\n",
	     iw_saether_ntop(&event->u.addr, buffer));
      break;
    case IWEVCUSTOM:
      {
	char custom[IW_CUSTOM_MAX+1];
	memset(custom, '\0', sizeof(custom));
	if((event->u.data.pointer) && (event->u.data.length))
	  memcpy(custom, event->u.data.pointer, event->u.data.length);
	printf("Custom driver event:%s\n", custom);
      }
      break;
    case IWEVREGISTERED:
      printf("Registered node:%s\n",
	     iw_saether_ntop(&event->u.addr, buffer));
      break;
    case IWEVEXPIRED:
      printf("Expired node:%s\n",
	     iw_saether_ntop(&event->u.addr, buffer));
      break;
    case SIOCGIWTHRSPY:
      {
	struct iw_thrspy	threshold;
	if((event->u.data.pointer) && (event->u.data.length))
	  {
	    memcpy(&threshold, event->u.data.pointer,
		   sizeof(struct iw_thrspy));
	    printf("Spy threshold crossed on address:%s\n",
		   iw_saether_ntop(&threshold.addr, buffer));
	    iw_print_stats(buffer, sizeof(buffer),
			   &threshold.qual, iw_range, has_range);
	    printf("                            Link %s\n", buffer);
	  }
	else
	  printf("Invalid Spy Threshold event\n");
      }
      break;
      /* ----- driver WPA events ----- */
      /* Events generated by the driver, used for WPA operation */
    case IWEVMICHAELMICFAILURE:
      if(event->u.data.length >= sizeof(struct iw_michaelmicfailure))
	{
	  struct iw_michaelmicfailure mf;
	  memcpy(&mf, event->u.data.pointer, sizeof(mf));
	  printf("Michael MIC failure flags:0x%X src_addr:%s tsc:%s\n",
		 mf.flags,
		 iw_saether_ntop(&mf.src_addr, buffer2),
		 iw_hexdump(buffer, sizeof(buffer),
			    mf.tsc, IW_ENCODE_SEQ_MAX_SIZE));
	}
      break;
    case IWEVASSOCREQIE:
      printf("Association Request IEs:%s\n",
	     iw_hexdump(buffer, sizeof(buffer),
			event->u.data.pointer, event->u.data.length));
      break;
    case IWEVASSOCRESPIE:
      printf("Association Response IEs:%s\n",
	     iw_hexdump(buffer, sizeof(buffer),
			event->u.data.pointer, event->u.data.length));
      break;
    case IWEVPMKIDCAND:
      if(event->u.data.length >= sizeof(struct iw_pmkid_cand))
	{
	  struct iw_pmkid_cand cand;
	  memcpy(&cand, event->u.data.pointer, sizeof(cand));
	  printf("PMKID candidate flags:0x%X index:%d bssid:%s\n",
		 cand.flags, cand.index,
		 iw_saether_ntop(&cand.bssid, buffer));
	}
      break;
      /* ----- junk ----- */
      /* other junk not currently in use */
    case SIOCGIWRATE:
      iw_print_bitrate(buffer, sizeof(buffer), event->u.bitrate.value);
      printf("New Bit Rate:%s\n", buffer);
      break;
    case SIOCGIWNAME:
      printf("Protocol:%-1.16s\n", event->u.name);
      break;
    case IWEVQUAL:
      {
	event->u.qual.updated = 0x0;	/* Not that reliable, disable */
	iw_print_stats(buffer, sizeof(buffer),
		       &event->u.qual, iw_range, has_range);
	printf("Link %s\n", buffer);
	break;
      }
    default:
      printf("(Unknown Wireless event 0x%04X)\n", event->cmd);
    }	/* switch(event->cmd) */

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Print out all Wireless Events part of the RTNetlink message
 * Most often, there will be only one event per message, but
 * just make sure we read everything...
 */
static inline int
print_event_stream(int		ifindex,
		   char *	data,
		   int		len)
{
  struct iw_event	iwe;
  struct stream_descr	stream;
  int			i = 0;
  int			ret;
  char			buffer[64];
  struct timeval	recv_time;
  struct timezone	tz;
  struct wireless_iface *	wireless_data;

  /* Get data from cache */
  wireless_data = iw_get_interface_data(ifindex);
  if(wireless_data == NULL)
    return(-1);

  /* Print received time in readable form */
  gettimeofday(&recv_time, &tz);
  iw_print_timeval(buffer, sizeof(buffer), &recv_time, &tz);

  iw_init_event_stream(&stream, data, len);
  do
    {
      /* Extract an event and print it */
      ret = iw_extract_event_stream(&stream, &iwe,
				    wireless_data->range.we_version_compiled);
      if(ret != 0)
	{
	  if(i++ == 0)
	    printf("%s   %-8.16s ", buffer, wireless_data->ifname);
	  else
	    printf("                           ");
	  if(ret > 0)
	    print_event_token(&iwe,
			      &wireless_data->range, wireless_data->has_range);
	  else
	    printf("(Invalid event)\n");
	  /* Push data out *now*, in case we are redirected to a pipe */
	  fflush(stdout);
	}
    }
  while(ret > 0);

  return(0);
}

/*********************** RTNETLINK EVENT DUMP***********************/
/*
 * Dump the events we receive from rtnetlink
 * This code is mostly from Casey
 */

/*------------------------------------------------------------------*/
/*
 * Respond to a single RTM_NEWLINK event from the rtnetlink socket.
 */
static int
LinkCatcher(struct nlmsghdr *nlh)
{
  struct ifinfomsg* ifi;

#if 0
  fprintf(stderr, "nlmsg_type = %d.\n", nlh->nlmsg_type);
#endif

  ifi = NLMSG_DATA(nlh);

  /* Code is ugly, but sort of works - Jean II */

  /* If interface is getting destoyed */
  if(nlh->nlmsg_type == RTM_DELLINK)
    {
      /* Remove from cache (if in cache) */
      iw_del_interface_data(ifi->ifi_index);
      return 0;
    }

  /* Only keep add/change events */
  if(nlh->nlmsg_type != RTM_NEWLINK)
    return 0;

  /* Check for attributes */
  if (nlh->nlmsg_len > NLMSG_ALIGN(sizeof(struct ifinfomsg)))
    {
      int attrlen = nlh->nlmsg_len - NLMSG_ALIGN(sizeof(struct ifinfomsg));
      struct rtattr *attr = (void *) ((char *) ifi +
				      NLMSG_ALIGN(sizeof(struct ifinfomsg)));

      while (RTA_OK(attr, attrlen))
	{
	  /* Check if the Wireless kind */
	  if(attr->rta_type == IFLA_WIRELESS)
	    {
	      /* Go to display it */
	      print_event_stream(ifi->ifi_index,
				 (char *) attr + RTA_ALIGN(sizeof(struct rtattr)),
				 attr->rta_len - RTA_ALIGN(sizeof(struct rtattr)));
	    }
	  attr = RTA_NEXT(attr, attrlen);
	}
    }

  return 0;
}

/* ---------------------------------------------------------------- */
/*
 * We must watch the rtnelink socket for events.
 * This routine handles those events (i.e., call this when rth.fd
 * is ready to read).
 */
static inline void
handle_netlink_events(struct rtnl_handle *	rth)
{
  while(1)
    {
      struct sockaddr_nl sanl;
      socklen_t sanllen = sizeof(struct sockaddr_nl);

      struct nlmsghdr *h;
      int amt;
      char buf[8192];

      amt = recvfrom(rth->fd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr*)&sanl, &sanllen);
      if(amt < 0)
	{
	  if(errno != EINTR && errno != EAGAIN)
	    {
	      fprintf(stderr, "%s: error reading netlink: %s.\n",
		      __PRETTY_FUNCTION__, strerror(errno));
	    }
	  return;
	}

      if(amt == 0)
	{
	  fprintf(stderr, "%s: EOF on netlink??\n", __PRETTY_FUNCTION__);
	  return;
	}

      h = (struct nlmsghdr*)buf;
      while(amt >= (int)sizeof(*h))
	{
	  int len = h->nlmsg_len;
	  int l = len - sizeof(*h);

	  if(l < 0 || len > amt)
	    {
	      fprintf(stderr, "%s: malformed netlink message: len=%d\n", __PRETTY_FUNCTION__, len);
	      break;
	    }

	  switch(h->nlmsg_type)
	    {
	    case RTM_NEWLINK:
	    case RTM_DELLINK:
	      LinkCatcher(h);
	      break;
	    default:
#if 0
	      fprintf(stderr, "%s: got nlmsg of type %#x.\n", __PRETTY_FUNCTION__, h->nlmsg_type);
#endif
	      break;
	    }

	  len = NLMSG_ALIGN(len);
	  amt -= len;
	  h = (struct nlmsghdr*)((char*)h + len);
	}

      if(amt > 0)
	fprintf(stderr, "%s: remnant of size %d on netlink\n", __PRETTY_FUNCTION__, amt);
    }
}

/**************************** MAIN LOOP ****************************/

/* ---------------------------------------------------------------- */
/*
 * Wait until we get an event
 */
static inline int
wait_for_event(struct rtnl_handle *	rth)
{
#if 0
  struct timeval	tv;	/* Select timeout */
#endif

  /* Forever */
  while(1)
    {
      fd_set		rfds;		/* File descriptors for select */
      int		last_fd;	/* Last fd */
      int		ret;

      /* Guess what ? We must re-generate rfds each time */
      FD_ZERO(&rfds);
      FD_SET(rth->fd, &rfds);
      last_fd = rth->fd;

      /* Wait until something happens */
      ret = select(last_fd + 1, &rfds, NULL, NULL, NULL);

      /* Check if there was an error */
      if(ret < 0)
	{
	  if(errno == EAGAIN || errno == EINTR)
	    continue;
	  fprintf(stderr, "Unhandled signal - exiting...\n");
	  break;
	}

      /* Check if there was a timeout */
      if(ret == 0)
	{
	  continue;
	}

      /* Check for interface discovery events. */
      if(FD_ISSET(rth->fd, &rfds))
	handle_netlink_events(rth);
    }

  return(0);
}

/******************************* MAIN *******************************/

/* ---------------------------------------------------------------- */
/*
 * helper ;-)
 */
static void
iw_usage(int status)
{
  fputs("Usage: iwevent [OPTIONS]\n"
	"   Monitors and displays Wireless Events.\n"
	"   Options are:\n"
	"     -h,--help     Print this message.\n"
	"     -v,--version  Show version of this program.\n",
	status ? stderr : stdout);
  exit(status);
}
/* Command line options */
static const struct option long_opts[] = {
  { "help", no_argument, NULL, 'h' },
  { "version", no_argument, NULL, 'v' },
  { NULL, 0, NULL, 0 }
};

/* ---------------------------------------------------------------- */
/*
 * main body of the program
 */
int
main(int	argc,
     char *	argv[])
{
  struct rtnl_handle	rth;
  int opt;

  /* Check command line options */
  while((opt = getopt_long(argc, argv, "hv", long_opts, NULL)) > 0)
    {
      switch(opt)
	{
	case 'h':
	  iw_usage(0);
	  break;

	case 'v':
	  return(iw_print_version_info("iwevent"));
	  break;

	default:
	  iw_usage(1);
	  break;
	}
    }
  if(optind < argc)
    {
      fputs("Too many arguments.\n", stderr);
      iw_usage(1);
    }

  /* Open netlink channel */
  if(rtnl_open(&rth, RTMGRP_LINK) < 0)
    {
      perror("Can't initialize rtnetlink socket");
      return(1);
    }

  fprintf(stderr, "Waiting for Wireless Events from interfaces...\n");

  /* Do what we have to do */
  wait_for_event(&rth);

  /* Cleanup - only if you are pedantic */
  rtnl_close(&rth);

  return(0);
}

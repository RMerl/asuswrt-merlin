/*
 *	Wireless Tools
 *
 *		Jean II - HPLB '99 - HPL 99->04
 *
 * This tool can manipulate the spy list : add addresses and display stat
 * You need to link this code against "iwlib.c" and "-lm".
 *
 * This file is released under the GPL license.
 *     Copyright (c) 1997-2004 Jean Tourrilhes <jt@hpl.hp.com>
 */

#include "iwlib.h"		/* Header */

/************************* DISPLAY ROUTINES **************************/

/*------------------------------------------------------------------*/
/*
 * Display the spy list of addresses and the associated stats
 */
static int
print_spy_info(int	skfd,
	       char *	ifname,
	       char *	args[],
	       int	count)
{
  struct iwreq		wrq;
  char		buffer[(sizeof(struct iw_quality) +
			sizeof(struct sockaddr)) * IW_MAX_SPY];
  char		temp[128];
  struct sockaddr *	hwa;
  struct iw_quality *	qual;
  iwrange	range;
  int		has_range = 0;
  int		n;
  int		i;

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Collect stats */
  wrq.u.data.pointer = (caddr_t) buffer;
  wrq.u.data.length = IW_MAX_SPY;
  wrq.u.data.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWSPY, &wrq) < 0)
    {
      fprintf(stderr, "%-8.16s  Interface doesn't support wireless statistic collection\n\n", ifname);
      return(-1);
    }

  /* Number of addresses */
  n = wrq.u.data.length;

  /* Check if we have valid mac address type */
  if(iw_check_mac_addr_type(skfd, ifname) < 0)
    {
      fprintf(stderr, "%-8.16s  Interface doesn't support MAC addresses\n\n", ifname);
      return(-2);
    }

  /* Get range info if we can */
  if(iw_get_range_info(skfd, ifname, &(range)) >= 0)
    has_range = 1;

  /* Display it */
  if(n == 0)
    printf("%-8.16s  No statistics to collect\n", ifname);
  else
    printf("%-8.16s  Statistics collected:\n", ifname);
 
  /* The two lists */
  hwa = (struct sockaddr *) buffer;
  qual = (struct iw_quality *) (buffer + (sizeof(struct sockaddr) * n));

  for(i = 0; i < n; i++)
    {
      /* Print stats for each address */
      printf("    %s : ", iw_saether_ntop(&hwa[i], temp));
      iw_print_stats(temp, sizeof(temp), &qual[i], &range, has_range);
      printf("%s\n", temp);
    }

  if((n > 0) && (has_range) && (range.we_version_compiled > 11))
    {
      iwstats	stats;

      /* Get /proc/net/wireless */
      if(iw_get_stats(skfd, ifname, &stats, &range, has_range) >= 0)
	{
	  iw_print_stats(temp, sizeof(temp), &stats.qual, &range, has_range);
	  printf("    Link/Cell/AP      : %s\n", temp);
	  /* Display the static data */
	  iw_print_stats(temp, sizeof(temp),
			 &range.avg_qual, &range, has_range);
	  printf("    Typical/Reference : %s\n", temp);
	}
    }

  printf("\n");
  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Get spy thresholds from the driver and display
 */
static int
get_spy_threshold(int		skfd,		/* The socket */
		  char *	ifname,		/* Dev name */
		  char *	args[],		/* Command line args */
		  int		count)		/* Args count */
{
  struct iwreq		wrq;
  struct iw_thrspy	threshold;
  iwrange	range;
  int			has_range = 0;

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Time to send thresholds to the driver */
  wrq.u.data.pointer = (caddr_t) &threshold;
  wrq.u.data.length = 1;
  wrq.u.data.flags = 0;
  if(iw_set_ext(skfd, ifname, SIOCGIWTHRSPY, &wrq) < 0)
    {
      fprintf(stderr, "Interface doesn't support thresholds...\n");
      fprintf(stderr, "SIOCGIWTHRSPY: %s\n", strerror(errno));
      return(-1);
    }

  /* Get range info if we can */
  if(iw_get_range_info(skfd, ifname, &(range)) >= 0)
    has_range = 1;

  /* Display thresholds */
  if((has_range) && (threshold.low.level))
    {
      /* If the statistics are in dBm */
      if(threshold.low.level > range.max_qual.level)
	{
	  /* Statistics are in dBm (absolute power measurement) */
	  printf("%-8.16s  Low threshold:%d dBm  High threshold:%d dBm\n\n",
		 ifname,
		 threshold.low.level - 0x100, threshold.high.level - 0x100);
	}
      else
	{
	  /* Statistics are relative values (0 -> max) */
	  printf("%-8.16s  Low threshold:%d/%d  High threshold:%d/%d\n\n",
		 ifname,
		 threshold.low.level, range.max_qual.level,
		 threshold.high.level, range.max_qual.level);
	}
    }
  else
    {
      /* We can't read the range, so we don't know... */
      printf("%-8.16s  Low threshold:%d  High threshold:%d\n\n",
	     ifname,
	     threshold.low.level, threshold.high.level);
    }

  return(0);
}

/************************* SETTING ROUTINES **************************/

/*------------------------------------------------------------------*/
/*
 * Set list of addresses specified on command line in the driver.
 */
static int
set_spy_info(int		skfd,		/* The socket */
	     char *		ifname,		/* Dev name */
	     char *		args[],		/* Command line args */
	     int		count)		/* Args count */
{
  struct iwreq		wrq;
  int			i;
  int			nbr;		/* Number of valid addresses */
  struct sockaddr	hw_address[IW_MAX_SPY];

  /* Read command line */
  i = 0;	/* first arg to read */
  nbr = 0;	/* Number of args read so far */

  /* "off" : disable functionality (set 0 addresses) */
  if(!strcmp(args[0], "off"))
    i = 1;	/* skip the "off" */
  else
    {
      /* "+" : add all addresses already in the driver */
      if(!strcmp(args[0], "+"))
	{
	  char	buffer[(sizeof(struct iw_quality) +
			sizeof(struct sockaddr)) * IW_MAX_SPY];

	  /* Check if we have valid mac address type */
	  if(iw_check_mac_addr_type(skfd, ifname) < 0)
	    {
	      fprintf(stderr, "%-8.16s  Interface doesn't support MAC addresses\n", ifname);
	      return(-1);
	    }

	  wrq.u.data.pointer = (caddr_t) buffer;
	  wrq.u.data.length = IW_MAX_SPY;
	  wrq.u.data.flags = 0;
	  if(iw_get_ext(skfd, ifname, SIOCGIWSPY, &wrq) < 0)
	    {
	      fprintf(stderr, "Interface doesn't accept reading addresses...\n");
	      fprintf(stderr, "SIOCGIWSPY: %s\n", strerror(errno));
	      return(-1);
	    }

	  /* Copy old addresses */
	  nbr = wrq.u.data.length;
	  memcpy(hw_address, buffer, nbr * sizeof(struct sockaddr));

	  i = 1;	/* skip the "+" */
	}

      /* Read other args on command line */
      while((i < count) && (nbr < IW_MAX_SPY))
	{
	  /* Get the address and check if the interface supports it */
	  if(iw_in_addr(skfd, ifname, args[i++], &(hw_address[nbr])) < 0)
	    continue;
	  nbr++;
	}

      /* Check the number of addresses */
      if(nbr == 0)
	{
	  fprintf(stderr, "No valid addresses found : exiting...\n");
	  return(-1);
	}
    }

  /* Check if there is some remaining arguments */
  if(i < count)
    {
      fprintf(stderr, "Got only the first %d arguments, remaining discarded\n", i);
    }

  /* Time to do send addresses to the driver */
  wrq.u.data.pointer = (caddr_t) hw_address;
  wrq.u.data.length = nbr;
  wrq.u.data.flags = 0;
  if(iw_set_ext(skfd, ifname, SIOCSIWSPY, &wrq) < 0)
    {
      fprintf(stderr, "Interface doesn't accept addresses...\n");
      fprintf(stderr, "SIOCSIWSPY: %s\n", strerror(errno));
      return(-1);
    }

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Set spy thresholds in the driver from command line
 */
static int
set_spy_threshold(int		skfd,		/* The socket */
		  char *	ifname,		/* Dev name */
		  char *	args[],		/* Command line args */
		  int		count)		/* Args count */
{
  struct iwreq		wrq;
  struct iw_thrspy	threshold;
  int			low_thr;
  int			high_thr;

  /* Init */
  memset(&threshold, '\0', sizeof(threshold));

  /* "off" : disable functionality (set 0 addresses) */
  if(!strcmp(args[0], "off"))
    {
      /* Just send null threshold, will disable it */
    }
  else
    {
      /* Try to get our threshold */
      if(count < 2)
	{
	  fprintf(stderr, "%-8.16s  Need two threshold values\n", ifname);
	  return(-1);
	}
      if((sscanf(args[0], "%i", &low_thr) != 1) ||
	 (sscanf(args[1], "%i", &high_thr) != 1))
	{
	  fprintf(stderr, "%-8.16s  Invalid threshold values\n", ifname);
	  return(-1);
	}
      /* Basic sanity check */
      if(high_thr < low_thr)
	{
	  fprintf(stderr, "%-8.16s  Inverted threshold range\n", ifname);
	  return(-1);
	}
      /* Copy thresholds */
      threshold.low.level = low_thr;
      threshold.low.updated = 0x2;
      threshold.high.level = high_thr;
      threshold.high.updated = 0x2;
    }

  /* Time to send thresholds to the driver */
  wrq.u.data.pointer = (caddr_t) &threshold;
  wrq.u.data.length = 1;
  wrq.u.data.flags = 0;
  if(iw_set_ext(skfd, ifname, SIOCSIWTHRSPY, &wrq) < 0)
    {
      fprintf(stderr, "Interface doesn't accept thresholds...\n");
      fprintf(stderr, "SIOCSIWTHRSPY: %s\n", strerror(errno));
      return(-1);
    }

  return(0);
}

/******************************* MAIN ********************************/

/*------------------------------------------------------------------*/
/*
 * The main !
 */
int
main(int	argc,
     char **	argv)
{
  int skfd;			/* generic raw socket desc.	*/
  int goterr = 0;

  /* Create a channel to the NET kernel. */
  if((skfd = iw_sockets_open()) < 0)
    {
      perror("socket");
      return(-1);
    }

  /* No argument : show the list of all device + info */
  if(argc == 1)
    iw_enum_devices(skfd, &print_spy_info, NULL, 0);
  else
    /* Special cases take one... */
    /* Help */
    if((!strcmp(argv[1], "-h")) || (!strcmp(argv[1], "--help")))
      fprintf(stderr, "Usage: iwspy interface [+] [MAC address] [IP address]\n");
    else
      /* Version */
      if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version"))
	goterr = iw_print_version_info("iwspy");
      else
	/* The device name must be the first argument */
	/* Name only : show spy list for that device only */
	if(argc == 2)
	  print_spy_info(skfd, argv[1], NULL, 0);
	else
	  /* Special commands */
	  if(!strcmp(argv[2], "setthr"))
	    goterr = set_spy_threshold(skfd, argv[1], argv + 3, argc - 3);
	  else
	    if(!strcmp(argv[2], "getthr"))
	      goterr = get_spy_threshold(skfd, argv[1], argv + 3, argc - 3);
	    else
	      /* Otherwise, it's a list of address to set in the spy list */
	      goterr = set_spy_info(skfd, argv[1], argv + 2, argc - 2);

  /* Close the socket. */
  iw_sockets_close(skfd);

  return(goterr);
}

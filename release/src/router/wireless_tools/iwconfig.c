/*
 *	Wireless Tools
 *
 *		Jean II - HPLB 97->99 - HPL 99->07
 *
 * Main code for "iwconfig". This is the generic tool for most
 * manipulations...
 * You need to link this code against "iwlib.c" and "-lm".
 *
 * This file is released under the GPL license.
 *     Copyright (c) 1997-2007 Jean Tourrilhes <jt@hpl.hp.com>
 */

#include "iwlib.h"		/* Header */

/**************************** CONSTANTS ****************************/

/*
 * Error codes defined for setting args
 */
#define IWERR_ARG_NUM		-2
#define IWERR_ARG_TYPE		-3
#define IWERR_ARG_SIZE		-4
#define IWERR_ARG_CONFLICT	-5
#define IWERR_SET_EXT		-6
#define IWERR_GET_EXT		-7

/**************************** VARIABLES ****************************/

/*
 * Ugly, but deal with errors in set_info() efficiently...
 */
static int	errarg;
static int	errmax;

/************************* DISPLAY ROUTINES **************************/

/*------------------------------------------------------------------*/
/*
 * Get wireless informations & config from the device driver
 * We will call all the classical wireless ioctl on the driver through
 * the socket to know what is supported and to get the settings...
 */
static int
get_info(int			skfd,
	 char *			ifname,
	 struct wireless_info *	info)
{
  struct iwreq		wrq;

  memset((char *) info, 0, sizeof(struct wireless_info));

  /* Get basic information */
  if(iw_get_basic_config(skfd, ifname, &(info->b)) < 0)
    {
      /* If no wireless name : no wireless extensions */
      /* But let's check if the interface exists at all */
      struct ifreq ifr;

      strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
      if(ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
	return(-ENODEV);
      else
	return(-ENOTSUP);
    }

  /* Get ranges */
  if(iw_get_range_info(skfd, ifname, &(info->range)) >= 0)
    info->has_range = 1;

  /* Get AP address */
  if(iw_get_ext(skfd, ifname, SIOCGIWAP, &wrq) >= 0)
    {
      info->has_ap_addr = 1;
      memcpy(&(info->ap_addr), &(wrq.u.ap_addr), sizeof (sockaddr));
    }

  /* Get bit rate */
  if(iw_get_ext(skfd, ifname, SIOCGIWRATE, &wrq) >= 0)
    {
      info->has_bitrate = 1;
      memcpy(&(info->bitrate), &(wrq.u.bitrate), sizeof(iwparam));
    }

  /* Get Power Management settings */
  wrq.u.power.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWPOWER, &wrq) >= 0)
    {
      info->has_power = 1;
      memcpy(&(info->power), &(wrq.u.power), sizeof(iwparam));
    }

  /* Get stats */
  if(iw_get_stats(skfd, ifname, &(info->stats),
		  &info->range, info->has_range) >= 0)
    {
      info->has_stats = 1;
    }

#ifndef WE_ESSENTIAL
  /* Get NickName */
  wrq.u.essid.pointer = (caddr_t) info->nickname;
  wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
  wrq.u.essid.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWNICKN, &wrq) >= 0)
    if(wrq.u.data.length > 1)
      info->has_nickname = 1;

  if((info->has_range) && (info->range.we_version_compiled > 9))
    {
      /* Get Transmit Power */
      if(iw_get_ext(skfd, ifname, SIOCGIWTXPOW, &wrq) >= 0)
	{
	  info->has_txpower = 1;
	  memcpy(&(info->txpower), &(wrq.u.txpower), sizeof(iwparam));
	}
    }

  /* Get sensitivity */
  if(iw_get_ext(skfd, ifname, SIOCGIWSENS, &wrq) >= 0)
    {
      info->has_sens = 1;
      memcpy(&(info->sens), &(wrq.u.sens), sizeof(iwparam));
    }

  if((info->has_range) && (info->range.we_version_compiled > 10))
    {
      /* Get retry limit/lifetime */
      if(iw_get_ext(skfd, ifname, SIOCGIWRETRY, &wrq) >= 0)
	{
	  info->has_retry = 1;
	  memcpy(&(info->retry), &(wrq.u.retry), sizeof(iwparam));
	}
    }

  /* Get RTS threshold */
  if(iw_get_ext(skfd, ifname, SIOCGIWRTS, &wrq) >= 0)
    {
      info->has_rts = 1;
      memcpy(&(info->rts), &(wrq.u.rts), sizeof(iwparam));
    }

  /* Get fragmentation threshold */
  if(iw_get_ext(skfd, ifname, SIOCGIWFRAG, &wrq) >= 0)
    {
      info->has_frag = 1;
      memcpy(&(info->frag), &(wrq.u.frag), sizeof(iwparam));
    }
#endif	/* WE_ESSENTIAL */

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Print on the screen in a neat fashion all the info we have collected
 * on a device.
 */
static void
display_info(struct wireless_info *	info,
	     char *			ifname)
{
  char		buffer[128];	/* Temporary buffer */

  /* One token is more of less 5 characters, 14 tokens per line */
  int	tokens = 3;	/* For name */

  /* Display device name and wireless name (name of the protocol used) */
  printf("%-8.16s  %s  ", ifname, info->b.name);

  /* Display ESSID (extended network), if any */
  if(info->b.has_essid)
    {
      if(info->b.essid_on)
	{
	  /* Does it have an ESSID index ? */
	  if((info->b.essid_on & IW_ENCODE_INDEX) > 1)
	    printf("ESSID:\"%s\" [%d]  ", info->b.essid,
		   (info->b.essid_on & IW_ENCODE_INDEX));
	  else
	    printf("ESSID:\"%s\"  ", info->b.essid);
	}
      else
	printf("ESSID:off/any  ");
    }

#ifndef WE_ESSENTIAL
  /* Display NickName (station name), if any */
  if(info->has_nickname)
    printf("Nickname:\"%s\"", info->nickname);
#endif	/* WE_ESSENTIAL */

  /* Formatting */
  if(info->b.has_essid || info->has_nickname)
    {
      printf("\n          ");
      tokens = 0;
    }

#ifndef WE_ESSENTIAL
  /* Display Network ID */
  if(info->b.has_nwid)
    {
      /* Note : should display proper number of digits according to info
       * in range structure */
      if(info->b.nwid.disabled)
	printf("NWID:off/any  ");
      else
	printf("NWID:%X  ", info->b.nwid.value);
      tokens +=2;
    }
#endif	/* WE_ESSENTIAL */

  /* Display the current mode of operation */
  if(info->b.has_mode)
    {
      printf("Mode:%s  ", iw_operation_mode[info->b.mode]);
      tokens +=3;
    }

  /* Display frequency / channel */
  if(info->b.has_freq)
    {
      double		freq = info->b.freq;	/* Frequency/channel */
      int		channel = -1;		/* Converted to channel */
      /* Some drivers insist of returning channel instead of frequency.
       * This fixes them up. Note that, driver should still return
       * frequency, because other tools depend on it. */
      if(info->has_range && (freq < KILO))
	channel = iw_channel_to_freq((int) freq, &freq, &info->range);
      /* Display */
      iw_print_freq(buffer, sizeof(buffer), freq, -1, info->b.freq_flags);
      printf("%s  ", buffer);
      tokens +=4;
    }

  /* Display the address of the current Access Point */
  if(info->has_ap_addr)
    {
      /* A bit of clever formatting */
      if(tokens > 8)
	{
	  printf("\n          ");
	  tokens = 0;
	}
      tokens +=6;

      /* Oups ! No Access Point in Ad-Hoc mode */
      if((info->b.has_mode) && (info->b.mode == IW_MODE_ADHOC))
	printf("Cell:");
      else
	printf("Access Point:");
      printf(" %s   ", iw_sawap_ntop(&info->ap_addr, buffer));
    }

  /* Display the currently used/set bit-rate */
  if(info->has_bitrate)
    {
      /* A bit of clever formatting */
      if(tokens > 11)
	{
	  printf("\n          ");
	  tokens = 0;
	}
      tokens +=3;

      /* Display it */
      iw_print_bitrate(buffer, sizeof(buffer), info->bitrate.value);
      printf("Bit Rate%c%s   ", (info->bitrate.fixed ? '=' : ':'), buffer);
    }

#ifndef WE_ESSENTIAL
  /* Display the Transmit Power */
  if(info->has_txpower)
    {
      /* A bit of clever formatting */
      if(tokens > 11)
	{
	  printf("\n          ");
	  tokens = 0;
	}
      tokens +=3;

      /* Display it */
      iw_print_txpower(buffer, sizeof(buffer), &info->txpower);
      printf("Tx-Power%c%s   ", (info->txpower.fixed ? '=' : ':'), buffer);
    }

  /* Display sensitivity */
  if(info->has_sens)
    {
      /* A bit of clever formatting */
      if(tokens > 10)
	{
	  printf("\n          ");
	  tokens = 0;
	}
      tokens +=4;

      /* Fixed ? */
      printf("Sensitivity%c", info->sens.fixed ? '=' : ':');

      if(info->has_range)
	/* Display in dBm ? */
	if(info->sens.value < 0)
	  printf("%d dBm  ", info->sens.value);
	else
	  printf("%d/%d  ", info->sens.value, info->range.sensitivity);
      else
	printf("%d  ", info->sens.value);
    }
#endif	/* WE_ESSENTIAL */

  printf("\n          ");
  tokens = 0;

#ifndef WE_ESSENTIAL
  /* Display retry limit/lifetime information */
  if(info->has_retry)
    { 
      printf("Retry");
      /* Disabled ? */
      if(info->retry.disabled)
	printf(":off");
      else
	{
	  /* Let's check the value and its type */
	  if(info->retry.flags & IW_RETRY_TYPE)
	    {
	      iw_print_retry_value(buffer, sizeof(buffer),
				   info->retry.value, info->retry.flags,
				   info->range.we_version_compiled);
	      printf("%s", buffer);
	    }

	  /* Let's check if nothing (simply on) */
	  if(info->retry.flags == IW_RETRY_ON)
	    printf(":on");
 	}
      printf("   ");
      tokens += 5;	/* Between 3 and 5, depend on flags */
    }

  /* Display the RTS threshold */
  if(info->has_rts)
    {
      /* Disabled ? */
      if(info->rts.disabled)
	printf("RTS thr:off   ");
      else
	{
	  /* Fixed ? */
	  printf("RTS thr%c%d B   ",
		 info->rts.fixed ? '=' : ':',
		 info->rts.value);
	}
      tokens += 3;
    }

  /* Display the fragmentation threshold */
  if(info->has_frag)
    {
      /* A bit of clever formatting */
      if(tokens > 10)
	{
	  printf("\n          ");
	  tokens = 0;
	}
      tokens +=4;

      /* Disabled ? */
      if(info->frag.disabled)
	printf("Fragment thr:off");
      else
	{
	  /* Fixed ? */
	  printf("Fragment thr%c%d B   ",
		 info->frag.fixed ? '=' : ':',
		 info->frag.value);
	}
    }

  /* Formating */
  if(tokens > 0)
    printf("\n          ");
#endif	/* WE_ESSENTIAL */

  /* Display encryption information */
  /* Note : we display only the "current" key, use iwlist to list all keys */
  if(info->b.has_key)
    {
      printf("Encryption key:");
      if((info->b.key_flags & IW_ENCODE_DISABLED) || (info->b.key_size == 0))
	printf("off");
      else
	{
	  /* Display the key */
	  iw_print_key(buffer, sizeof(buffer),
		       info->b.key, info->b.key_size, info->b.key_flags);
	  printf("%s", buffer);

	  /* Other info... */
	  if((info->b.key_flags & IW_ENCODE_INDEX) > 1)
	    printf(" [%d]", info->b.key_flags & IW_ENCODE_INDEX);
	  if(info->b.key_flags & IW_ENCODE_RESTRICTED)
	    printf("   Security mode:restricted");
	  if(info->b.key_flags & IW_ENCODE_OPEN)
	    printf("   Security mode:open");
 	}
      printf("\n          ");
    }

  /* Display Power Management information */
  /* Note : we display only one parameter, period or timeout. If a device
   * (such as HiperLan) has both, the user need to use iwlist... */
  if(info->has_power)	/* I hope the device has power ;-) */
    { 
      printf("Power Management");
      /* Disabled ? */
      if(info->power.disabled)
	printf(":off");
      else
	{
	  /* Let's check the value and its type */
	  if(info->power.flags & IW_POWER_TYPE)
	    {
	      iw_print_pm_value(buffer, sizeof(buffer),
				info->power.value, info->power.flags,
				info->range.we_version_compiled);
	      printf("%s  ", buffer);
	    }

	  /* Let's check the mode */
	  iw_print_pm_mode(buffer, sizeof(buffer), info->power.flags);
	  printf("%s", buffer);

	  /* Let's check if nothing (simply on) */
	  if(info->power.flags == IW_POWER_ON)
	    printf(":on");
 	}
      printf("\n          ");
    }

  /* Display statistics */
  if(info->has_stats)
    {
      iw_print_stats(buffer, sizeof(buffer),
		     &info->stats.qual, &info->range, info->has_range);
      printf("Link %s\n", buffer);

      if(info->range.we_version_compiled > 11)
	printf("          Rx invalid nwid:%d  Rx invalid crypt:%d  Rx invalid frag:%d\n          Tx excessive retries:%d  Invalid misc:%d   Missed beacon:%d\n",
	       info->stats.discard.nwid,
	       info->stats.discard.code,
	       info->stats.discard.fragment,
	       info->stats.discard.retries,
	       info->stats.discard.misc,
	       info->stats.miss.beacon);
      else
	printf("          Rx invalid nwid:%d  invalid crypt:%d  invalid misc:%d\n",
	       info->stats.discard.nwid,
	       info->stats.discard.code,
	       info->stats.discard.misc);
    }

  printf("\n");
}

/*------------------------------------------------------------------*/
/*
 * Print on the screen in a neat fashion all the info we have collected
 * on a device.
 */
static int
print_info(int		skfd,
	   char *	ifname,
	   char *	args[],
	   int		count)
{
  struct wireless_info	info;
  int			rc;

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  rc = get_info(skfd, ifname, &info);
  switch(rc)
    {
    case 0:	/* Success */
      /* Display it ! */
      display_info(&info, ifname);
      break;

    case -ENOTSUP:
      fprintf(stderr, "%-8.16s  no wireless extensions.\n\n",
	      ifname);
      break;

    default:
      fprintf(stderr, "%-8.16s  %s\n\n", ifname, strerror(-rc));
    }
  return(rc);
}

/****************** COMMAND LINE MODIFIERS PARSING ******************/
/*
 * Factor out the parsing of command line modifiers.
 */

/*------------------------------------------------------------------*/
/*
 * Map command line modifiers to the proper flags...
 */
typedef struct iwconfig_modifier {
  const char *		cmd;		/* Command line shorthand */
  __u16			flag;		/* Flags to add */
  __u16			exclude;	/* Modifiers to exclude */
} iwconfig_modifier;

/*------------------------------------------------------------------*/
/*
 * Modifiers for Power
 */
static const struct iwconfig_modifier	iwmod_power[] = {
  { "min",	IW_POWER_MIN,		IW_POWER_MAX },
  { "max",	IW_POWER_MAX,		IW_POWER_MIN },
  { "period",	IW_POWER_PERIOD,	IW_POWER_TIMEOUT | IW_POWER_SAVING },
  { "timeout",	IW_POWER_TIMEOUT,	IW_POWER_PERIOD | IW_POWER_SAVING },
  { "saving",	IW_POWER_SAVING,	IW_POWER_TIMEOUT | IW_POWER_PERIOD },
};
#define IWMOD_POWER_NUM	(sizeof(iwmod_power)/sizeof(iwmod_power[0]))

/*------------------------------------------------------------------*/
/*
 * Modifiers for Retry
 */
#ifndef WE_ESSENTIAL
static const struct iwconfig_modifier	iwmod_retry[] = {
  { "min",	IW_RETRY_MIN,		IW_RETRY_MAX },
  { "max",	IW_RETRY_MAX,		IW_RETRY_MIN },
  { "short",	IW_RETRY_SHORT,		IW_RETRY_LONG },
  { "long",	IW_RETRY_LONG,		IW_RETRY_SHORT },
  { "limit",	IW_RETRY_LIMIT,		IW_RETRY_LIFETIME },
  { "lifetime",	IW_RETRY_LIFETIME,	IW_RETRY_LIMIT },
};
#define IWMOD_RETRY_NUM	(sizeof(iwmod_retry)/sizeof(iwmod_retry[0]))
#endif	/* WE_ESSENTIAL */

/*------------------------------------------------------------------*/
/*
 * Parse command line modifiers.
 * Return error or number arg parsed.
 * Modifiers must be at the beggining of command line.
 */
static int
parse_modifiers(char *		args[],		/* Command line args */
		int		count,		/* Args count */
		__u16 *		pout,		/* Flags to write */
		const struct iwconfig_modifier	modifier[],
		int		modnum)
{
  int		i = 0;
  int		k = 0;
  __u16		result = 0;	/* Default : no flag set */

  /* Get all modifiers and value types on the command line */
  do
    {
      for(k = 0; k < modnum; k++)
	{
	  /* Check if matches */
	  if(!strcasecmp(args[i], modifier[k].cmd))
	    {
	      /* Check for conflicting flags */
	      if(result & modifier[k].exclude)
		{
		  errarg = i;
		  return(IWERR_ARG_CONFLICT);
		}
	      /* Just add it */
	      result |= modifier[k].flag;
	      ++i;
	      break;
	    }
	}
    }
  /* For as long as current arg matched and not out of args */
  while((i < count) && (k < modnum));

  /* Check there remains one arg for value */
  if(i >= count)
    return(IWERR_ARG_NUM);

  /* Return result */
  *pout = result;
  return(i);
}


/*********************** SETTING SUB-ROUTINES ***********************/
/*
 * The following functions are use to set some wireless parameters and
 * are called by the set dispatcher set_info().
 * They take as arguments the remaining of the command line, with
 * arguments processed already removed.
 * An error is indicated by a negative return value.
 * 0 and positive return values indicate the number of args consumed.
 */

/*------------------------------------------------------------------*/
/*
 * Set ESSID
 */
static int
set_essid_info(int		skfd,
	       char *		ifname,
	       char *		args[],		/* Command line args */
	       int		count)		/* Args count */
{
  struct iwreq		wrq;
  int			i = 1;
  char			essid[IW_ESSID_MAX_SIZE + 1];
  int			we_kernel_version;

  if((!strcasecmp(args[0], "off")) ||
     (!strcasecmp(args[0], "any")))
    {
      wrq.u.essid.flags = 0;
      essid[0] = '\0';
    }
  else
    if(!strcasecmp(args[0], "on"))
      {
	/* Get old essid */
	memset(essid, '\0', sizeof(essid));
	wrq.u.essid.pointer = (caddr_t) essid;
	wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
	wrq.u.essid.flags = 0;
	if(iw_get_ext(skfd, ifname, SIOCGIWESSID, &wrq) < 0)
	  return(IWERR_GET_EXT);
	wrq.u.essid.flags = 1;
      }
    else
      {
	i = 0;

	/* '-' or '--' allow to escape the ESSID string, allowing
	 * to set it to the string "any" or "off".
	 * This is a big ugly, but it will do for now */
	if((!strcmp(args[0], "-")) || (!strcmp(args[0], "--")))
	  {
	    if(++i >= count)
	      return(IWERR_ARG_NUM);
	  }

	/* Check the size of what the user passed us to avoid
	 * buffer overflows */
	if(strlen(args[i]) > IW_ESSID_MAX_SIZE)
	  {
	    errmax = IW_ESSID_MAX_SIZE;
	    return(IWERR_ARG_SIZE);
	  }
	else
	  {
	    int		temp;

	    wrq.u.essid.flags = 1;
	    strcpy(essid, args[i]);	/* Size checked, all clear */
	    i++;

	    /* Check for ESSID index */
	    if((i < count) &&
	       (sscanf(args[i], "[%i]", &temp) == 1) &&
	       (temp > 0) && (temp < IW_ENCODE_INDEX))
	      {
		wrq.u.essid.flags = temp;
		++i;
	      }
	  }
      }

  /* Get version from kernel, device may not have range... */
  we_kernel_version = iw_get_kernel_we_version();

  /* Finally set the ESSID value */
  wrq.u.essid.pointer = (caddr_t) essid;
  wrq.u.essid.length = strlen(essid);
  if(we_kernel_version < 21)
    wrq.u.essid.length++;

  if(iw_set_ext(skfd, ifname, SIOCSIWESSID, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* Var args */
  return(i);
}

/*------------------------------------------------------------------*/
/*
 * Set Mode
 */
static int
set_mode_info(int		skfd,
	      char *		ifname,
	      char *		args[],		/* Command line args */
	      int		count)		/* Args count */
{
  struct iwreq		wrq;
  unsigned int		k;		/* Must be unsigned */

  /* Avoid "Unused parameter" warning */
  count = count;

  /* Check if it is a uint, otherwise get is as a string */
  if(sscanf(args[0], "%i", &k) != 1)
    {
      k = 0;
      while((k < IW_NUM_OPER_MODE) &&
	    strncasecmp(args[0], iw_operation_mode[k], 3))
	k++;
    }
  if(k >= IW_NUM_OPER_MODE)
    {
      errarg = 0;
      return(IWERR_ARG_TYPE);
    }

  wrq.u.mode = k;
  if(iw_set_ext(skfd, ifname, SIOCSIWMODE, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* 1 arg */
  return(1);
}

/*------------------------------------------------------------------*/
/*
 * Set frequency/channel
 */
static int
set_freq_info(int		skfd,
	      char *		ifname,
	      char *		args[],		/* Command line args */
	      int		count)		/* Args count */
{
  struct iwreq		wrq;
  int			i = 1;

  if(!strcasecmp(args[0], "auto"))
    {
      wrq.u.freq.m = -1;
      wrq.u.freq.e = 0;
      wrq.u.freq.flags = 0;
    }
  else
    {
      if(!strcasecmp(args[0], "fixed"))
	{
	  /* Get old frequency */
	  if(iw_get_ext(skfd, ifname, SIOCGIWFREQ, &wrq) < 0)
	    return(IWERR_GET_EXT);
	  wrq.u.freq.flags = IW_FREQ_FIXED;
	}
      else			/* Should be a numeric value */
	{
	  double		freq;
	  char *		unit;

	  freq = strtod(args[0], &unit);
	  if(unit == args[0])
	    {
	      errarg = 0;
	      return(IWERR_ARG_TYPE);
	    }
	  if(unit != NULL)
	    {
	      if(unit[0] == 'G') freq *= GIGA;
	      if(unit[0] == 'M') freq *= MEGA;
	      if(unit[0] == 'k') freq *= KILO;
	    }

	  iw_float2freq(freq, &(wrq.u.freq));

	  wrq.u.freq.flags = IW_FREQ_FIXED;

	  /* Check for an additional argument */
	  if((i < count) && (!strcasecmp(args[i], "auto")))
	    {
	      wrq.u.freq.flags = 0;
	      ++i;
	    }
	  if((i < count) && (!strcasecmp(args[i], "fixed")))
	    {
	      wrq.u.freq.flags = IW_FREQ_FIXED;
	      ++i;
	    }
	}
    }

  if(iw_set_ext(skfd, ifname, SIOCSIWFREQ, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* Var args */
  return(i);
}

/*------------------------------------------------------------------*/
/*
 * Set Bit Rate
 */
static int
set_bitrate_info(int		skfd,
		 char *		ifname,
		 char *		args[],		/* Command line args */
		 int		count)		/* Args count */
{
  struct iwreq		wrq;
  int			i = 1;

  wrq.u.bitrate.flags = 0;
  if(!strcasecmp(args[0], "auto"))
    {
      wrq.u.bitrate.value = -1;
      wrq.u.bitrate.fixed = 0;
    }
  else
    {
      if(!strcasecmp(args[0], "fixed"))
	{
	  /* Get old bitrate */
	  if(iw_get_ext(skfd, ifname, SIOCGIWRATE, &wrq) < 0)
	    return(IWERR_GET_EXT);
	  wrq.u.bitrate.fixed = 1;
	}
      else			/* Should be a numeric value */
	{
	  double		brate;
	  char *		unit;

	  brate = strtod(args[0], &unit);
	  if(unit == args[0])
	    {
	      errarg = 0;
	      return(IWERR_ARG_TYPE);
	    }
	  if(unit != NULL)
	    {
	      if(unit[0] == 'G') brate *= GIGA;
	      if(unit[0] == 'M') brate *= MEGA;
	      if(unit[0] == 'k') brate *= KILO;
	    }
	  wrq.u.bitrate.value = (long) brate;
	  wrq.u.bitrate.fixed = 1;

	  /* Check for an additional argument */
	  if((i < count) && (!strcasecmp(args[i], "auto")))
	    {
	      wrq.u.bitrate.fixed = 0;
	      ++i;
	    }
	  if((i < count) && (!strcasecmp(args[i], "fixed")))
	    {
	      wrq.u.bitrate.fixed = 1;
	      ++i;
	    }
	  if((i < count) && (!strcasecmp(args[i], "unicast")))
	    {
	      wrq.u.bitrate.flags |= IW_BITRATE_UNICAST;
	      ++i;
	    }
	  if((i < count) && (!strcasecmp(args[i], "broadcast")))
	    {
	      wrq.u.bitrate.flags |= IW_BITRATE_BROADCAST;
	      ++i;
	    }
	}
    }

  if(iw_set_ext(skfd, ifname, SIOCSIWRATE, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* Var args */
  return(i);
}

/*------------------------------------------------------------------*/
/*
 * Set encryption
 */
static int
set_enc_info(int		skfd,
	     char *		ifname,
	     char *		args[],		/* Command line args */
	     int		count)		/* Args count */
{
  struct iwreq		wrq;
  int			i = 1;
  unsigned char		key[IW_ENCODING_TOKEN_MAX];

  if(!strcasecmp(args[0], "on"))
    {
      /* Get old encryption information */
      wrq.u.data.pointer = (caddr_t) key;
      wrq.u.data.length = IW_ENCODING_TOKEN_MAX;
      wrq.u.data.flags = 0;
      if(iw_get_ext(skfd, ifname, SIOCGIWENCODE, &wrq) < 0)
	return(IWERR_GET_EXT);
      wrq.u.data.flags &= ~IW_ENCODE_DISABLED;	/* Enable */
    }
  else
    {
      int	gotone = 0;
      int	oldone;
      int	keylen;
      int	temp;

      wrq.u.data.pointer = (caddr_t) NULL;
      wrq.u.data.flags = 0;
      wrq.u.data.length = 0;
      i = 0;

      /* Allow arguments in any order (it's safe) */
      do
	{
	  oldone = gotone;

	  /* -- Check for the key -- */
	  if(i < count)
	    {
	      keylen = iw_in_key_full(skfd, ifname,
				      args[i], key, &wrq.u.data.flags);
	      if(keylen > 0)
		{
		  wrq.u.data.length = keylen;
		  wrq.u.data.pointer = (caddr_t) key;
		  ++i;
		  gotone++;
		}
	    }

	  /* -- Check for token index -- */
	  if((i < count) &&
	     (sscanf(args[i], "[%i]", &temp) == 1) &&
	     (temp > 0) && (temp < IW_ENCODE_INDEX))
	    {
	      wrq.u.encoding.flags |= temp;
	      ++i;
	      gotone++;
	    }

	  /* -- Check the various flags -- */
	  if((i < count) && (!strcasecmp(args[i], "off")))
	    {
	      wrq.u.data.flags |= IW_ENCODE_DISABLED;
	      ++i;
	      gotone++;
	    }
	  if((i < count) && (!strcasecmp(args[i], "open")))
	    {
	      wrq.u.data.flags |= IW_ENCODE_OPEN;
	      ++i;
	      gotone++;
	    }
	  if((i < count) && (!strncasecmp(args[i], "restricted", 5)))
	    {
	      wrq.u.data.flags |= IW_ENCODE_RESTRICTED;
	      ++i;
	      gotone++;
	    }
	  if((i < count) && (!strncasecmp(args[i], "temporary", 4)))
	    {
	      wrq.u.data.flags |= IW_ENCODE_TEMP;
	      ++i;
	      gotone++;
	    }
	}
      while(gotone != oldone);

      /* Pointer is absent in new API */
      if(wrq.u.data.pointer == NULL)
	wrq.u.data.flags |= IW_ENCODE_NOKEY;

      /* Check if we have any invalid argument */
      if(!gotone)
	{
	  errarg = 0;
	  return(IWERR_ARG_TYPE);
	}
    }

  if(iw_set_ext(skfd, ifname, SIOCSIWENCODE, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* Var arg */
  return(i);
}

/*------------------------------------------------------------------*/
/*
 * Set Power Management
 */
static int
set_power_info(int		skfd,
	       char *		ifname,
	       char *		args[],		/* Command line args */
	       int		count)		/* Args count */
{
  struct iwreq		wrq;
  int			i = 1;

  if(!strcasecmp(args[0], "off"))
    wrq.u.power.disabled = 1;	/* i.e. max size */
  else
    if(!strcasecmp(args[0], "on"))
      {
	/* Get old Power info */
	wrq.u.power.flags = 0;
	if(iw_get_ext(skfd, ifname, SIOCGIWPOWER, &wrq) < 0)
	  return(IWERR_GET_EXT);
	wrq.u.power.disabled = 0;
      }
    else
      {
	double		value;
	char *		unit;
	int		gotone = 0;

	/* Parse modifiers */
	i = parse_modifiers(args, count, &wrq.u.power.flags,
			    iwmod_power, IWMOD_POWER_NUM);
	if(i < 0)
	  return(i);

	wrq.u.power.disabled = 0;

	/* Is there any value to grab ? */
	value = strtod(args[0], &unit);
	if(unit != args[0])
	  {
	    struct iw_range	range;
	    int			flags;
	    /* Extract range info to handle properly 'relative' */
	    if(iw_get_range_info(skfd, ifname, &range) < 0)
	      memset(&range, 0, sizeof(range));

	    /* Get the flags to be able to do the proper conversion */
	    switch(wrq.u.power.flags & IW_POWER_TYPE)
	      {
	      case IW_POWER_SAVING:
		flags = range.pms_flags;
		break;
	      case IW_POWER_TIMEOUT:
		flags = range.pmt_flags;
		break;
	      default:
		flags = range.pmp_flags;
		break;
	      }
	    /* Check if time or relative */
	    if(flags & IW_POWER_RELATIVE)
	      {
		if(range.we_version_compiled < 21)
		  value *= MEGA;
		else
		  wrq.u.power.flags |= IW_POWER_RELATIVE;
	      }
	    else
	      {
		value *= MEGA;	/* default = s */
		if(unit[0] == 'u') value /= MEGA;
		if(unit[0] == 'm') value /= KILO;
	      }
	    wrq.u.power.value = (long) value;
	    /* Set some default type if none */
	    if((wrq.u.power.flags & IW_POWER_TYPE) == 0)
	      wrq.u.power.flags |= IW_POWER_PERIOD;
	    ++i;
	    gotone = 1;
	  }

	/* Now, check the mode */
	if(i < count)
	  {
	    if(!strcasecmp(args[i], "all"))
	      wrq.u.power.flags |= IW_POWER_ALL_R;
	    if(!strncasecmp(args[i], "unicast", 4))
	      wrq.u.power.flags |= IW_POWER_UNICAST_R;
	    if(!strncasecmp(args[i], "multicast", 5))
	      wrq.u.power.flags |= IW_POWER_MULTICAST_R;
	    if(!strncasecmp(args[i], "force", 5))
	      wrq.u.power.flags |= IW_POWER_FORCE_S;
	    if(!strcasecmp(args[i], "repeat"))
	      wrq.u.power.flags |= IW_POWER_REPEATER;
	    if(wrq.u.power.flags & IW_POWER_MODE)
	      {
		++i;
		gotone = 1;
	      }
	  }
	if(!gotone)
	  {
	    errarg = i;
	    return(IWERR_ARG_TYPE);
	  }
      }

  if(iw_set_ext(skfd, ifname, SIOCSIWPOWER, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* Var args */
  return(i);
}

#ifndef WE_ESSENTIAL
/*------------------------------------------------------------------*/
/*
 * Set Nickname
 */
static int
set_nick_info(int		skfd,
	      char *		ifname,
	      char *		args[],		/* Command line args */
	      int		count)		/* Args count */
{
  struct iwreq		wrq;
  int			we_kernel_version;

  /* Avoid "Unused parameter" warning */
  count = count;

  if(strlen(args[0]) > IW_ESSID_MAX_SIZE)
    {
      errmax = IW_ESSID_MAX_SIZE;
      return(IWERR_ARG_SIZE);
    }

  we_kernel_version = iw_get_kernel_we_version();

  wrq.u.essid.pointer = (caddr_t) args[0];
  wrq.u.essid.length = strlen(args[0]);
  if(we_kernel_version < 21)
    wrq.u.essid.length++;

  if(iw_set_ext(skfd, ifname, SIOCSIWNICKN, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* 1 args */
  return(1);
}

/*------------------------------------------------------------------*/
/*
 * Set commit
 */
static int
set_nwid_info(int		skfd,
	      char *		ifname,
	      char *		args[],		/* Command line args */
	      int		count)		/* Args count */
{
  struct iwreq		wrq;
  unsigned long		temp;

  /* Avoid "Unused parameter" warning */
  count = count;

  if((!strcasecmp(args[0], "off")) ||
     (!strcasecmp(args[0], "any")))
    wrq.u.nwid.disabled = 1;
  else
    if(!strcasecmp(args[0], "on"))
      {
	/* Get old nwid */
	if(iw_get_ext(skfd, ifname, SIOCGIWNWID, &wrq) < 0)
	  return(IWERR_GET_EXT);
	wrq.u.nwid.disabled = 0;
      }
    else
      if(sscanf(args[0], "%lX", &(temp)) != 1)
	{
	  errarg = 0;
	  return(IWERR_ARG_TYPE);
	}
      else
	{
	  wrq.u.nwid.value = temp;
	  wrq.u.nwid.disabled = 0;
	}

  wrq.u.nwid.fixed = 1;

  /* Set new nwid */
  if(iw_set_ext(skfd, ifname, SIOCSIWNWID, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* 1 arg */
  return(1);
}

/*------------------------------------------------------------------*/
/*
 * Set AP Address
 */
static int
set_apaddr_info(int		skfd,
		char *		ifname,
		char *		args[],		/* Command line args */
		int		count)		/* Args count */
{
  struct iwreq		wrq;

  /* Avoid "Unused parameter" warning */
  count = count;

  if((!strcasecmp(args[0], "auto")) ||
     (!strcasecmp(args[0], "any")))
    {
      /* Send a broadcast address */
      iw_broad_ether(&(wrq.u.ap_addr));
    }
  else
    {
      if(!strcasecmp(args[0], "off"))
	{
	  /* Send a NULL address */
	  iw_null_ether(&(wrq.u.ap_addr));
	}
      else
	{
	  /* Get the address and check if the interface supports it */
	  if(iw_in_addr(skfd, ifname, args[0], &(wrq.u.ap_addr)) < 0)
	    {
	      errarg = 0;
	      return(IWERR_ARG_TYPE);
	    }
	}
    }

  if(iw_set_ext(skfd, ifname, SIOCSIWAP, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* 1 args */
  return(1);
}

/*------------------------------------------------------------------*/
/*
 * Set Tx Power
 */
static int
set_txpower_info(int		skfd,
		char *		ifname,
		char *		args[],		/* Command line args */
		int		count)		/* Args count */
{
  struct iwreq		wrq;
  int			i = 1;

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Prepare the request */
  wrq.u.txpower.value = -1;
  wrq.u.txpower.fixed = 1;
  wrq.u.txpower.disabled = 0;
  wrq.u.txpower.flags = IW_TXPOW_DBM;

  if(!strcasecmp(args[0], "off"))
    wrq.u.txpower.disabled = 1;	/* i.e. turn radio off */
  else
    if(!strcasecmp(args[0], "auto"))
      wrq.u.txpower.fixed = 0;	/* i.e. use power control */
    else
      {
	if(!strcasecmp(args[0], "on"))
	  {
	    /* Get old tx-power */
	    if(iw_get_ext(skfd, ifname, SIOCGIWTXPOW, &wrq) < 0)
	      return(IWERR_GET_EXT);
	    wrq.u.txpower.disabled = 0;
	  }
	else
	  {
	    if(!strcasecmp(args[0], "fixed"))
	      {
		/* Get old tx-power */
		if(iw_get_ext(skfd, ifname, SIOCGIWTXPOW, &wrq) < 0)
		  return(IWERR_GET_EXT);
		wrq.u.txpower.fixed = 1;
		wrq.u.txpower.disabled = 0;
	      }
	    else			/* Should be a numeric value */
	      {
		int		power;
		int		ismwatt = 0;
		struct iw_range	range;

		/* Extract range info to do proper conversion */
		if(iw_get_range_info(skfd, ifname, &range) < 0)
		  memset(&range, 0, sizeof(range));

		/* Get the value */
		if(sscanf(args[0], "%i", &(power)) != 1)
		  {
		    errarg = 0;
		    return(IWERR_ARG_TYPE);
		  }

		/* Check if milliWatt
		 * We authorise a single 'm' as a shorthand for 'mW',
		 * on the other hand a 'd' probably means 'dBm'... */
		ismwatt = ((strchr(args[0], 'm') != NULL)
			   && (strchr(args[0], 'd') == NULL));

		/* We could check 'W' alone... Another time... */

		/* Convert */
		if(range.txpower_capa & IW_TXPOW_RELATIVE)
		  {
		    /* Can't convert */
		    if(ismwatt)
		      {
			errarg = 0;
			return(IWERR_ARG_TYPE);
		      }
		    wrq.u.txpower.flags = IW_TXPOW_RELATIVE;
		  }
		else
		  if(range.txpower_capa & IW_TXPOW_MWATT)
		    {
		      if(!ismwatt)
			power = iw_dbm2mwatt(power);
		      wrq.u.txpower.flags = IW_TXPOW_MWATT;
		    }
		  else
		    {
		      if(ismwatt)
			power = iw_mwatt2dbm(power);
		      wrq.u.txpower.flags = IW_TXPOW_DBM;
		    }
		wrq.u.txpower.value = power;

		/* Check for an additional argument */
		if((i < count) && (!strcasecmp(args[i], "auto")))
		  {
		    wrq.u.txpower.fixed = 0;
		    ++i;
		  }
		if((i < count) && (!strcasecmp(args[i], "fixed")))
		  {
		    wrq.u.txpower.fixed = 1;
		    ++i;
		  }
	      }
	  }
      }

  if(iw_set_ext(skfd, ifname, SIOCSIWTXPOW, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* Var args */
  return(i);
}

/*------------------------------------------------------------------*/
/*
 * Set Sensitivity
 */
static int
set_sens_info(int		skfd,
	      char *		ifname,
	      char *		args[],		/* Command line args */
	      int		count)		/* Args count */
{
  struct iwreq		wrq;
  int			temp;

  /* Avoid "Unused parameter" warning */
  count = count;

  if(sscanf(args[0], "%i", &(temp)) != 1)
    {
      errarg = 0;
      return(IWERR_ARG_TYPE);
    }
  wrq.u.sens.value = temp;

  if(iw_set_ext(skfd, ifname, SIOCSIWSENS, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* 1 arg */
  return(1);
}

/*------------------------------------------------------------------*/
/*
 * Set Retry Limit
 */
static int
set_retry_info(int		skfd,
	       char *		ifname,
	       char *		args[],		/* Command line args */
	       int		count)		/* Args count */
{
  struct iwreq		wrq;
  int			i = 0;
  double		value;
  char *		unit;

  /* Parse modifiers */
  i = parse_modifiers(args, count, &wrq.u.retry.flags,
		      iwmod_retry, IWMOD_RETRY_NUM);
  if(i < 0)
    return(i);

  /* Add default type if none */
  if((wrq.u.retry.flags & IW_RETRY_TYPE) == 0)
    wrq.u.retry.flags |= IW_RETRY_LIMIT;

  wrq.u.retry.disabled = 0;

  /* Is there any value to grab ? */
  value = strtod(args[0], &unit);
  if(unit == args[0])
    {
      errarg = i;
      return(IWERR_ARG_TYPE);
    }

  /* Limit is absolute, on the other hand lifetime is seconds */
  if(wrq.u.retry.flags & IW_RETRY_LIFETIME)
    {
      struct iw_range	range;
      /* Extract range info to handle properly 'relative' */
      if(iw_get_range_info(skfd, ifname, &range) < 0)
	memset(&range, 0, sizeof(range));

      if(range.r_time_flags & IW_RETRY_RELATIVE)
	{
	  if(range.we_version_compiled < 21)
	    value *= MEGA;
	  else
	    wrq.u.retry.flags |= IW_RETRY_RELATIVE;
	}
      else
	{
	  /* Normalise lifetime */
	  value *= MEGA;	/* default = s */
	  if(unit[0] == 'u') value /= MEGA;
	  if(unit[0] == 'm') value /= KILO;
	}
    }
  wrq.u.retry.value = (long) value;
  ++i;

  if(iw_set_ext(skfd, ifname, SIOCSIWRETRY, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* Var args */
  return(i);
}

/*------------------------------------------------------------------*/
/*
 * Set RTS Threshold
 */
static int
set_rts_info(int		skfd,
	     char *		ifname,
	     char *		args[],		/* Command line args */
	     int		count)		/* Args count */
{
  struct iwreq		wrq;

  /* Avoid "Unused parameter" warning */
  count = count;

  wrq.u.rts.value = -1;
  wrq.u.rts.fixed = 1;
  wrq.u.rts.disabled = 0;

  if(!strcasecmp(args[0], "off"))
    wrq.u.rts.disabled = 1;	/* i.e. max size */
  else
    if(!strcasecmp(args[0], "auto"))
      wrq.u.rts.fixed = 0;
    else
      {
	if(!strcasecmp(args[0], "fixed"))
	  {
	    /* Get old RTS threshold */
	    if(iw_get_ext(skfd, ifname, SIOCGIWRTS, &wrq) < 0)
	      return(IWERR_GET_EXT);
	    wrq.u.rts.fixed = 1;
	  }
	else
	  {	/* Should be a numeric value */
	    long	temp;
	    if(sscanf(args[0], "%li", (unsigned long *) &(temp)) != 1)
	      {
		errarg = 0;
		return(IWERR_ARG_TYPE);
	      }
	    wrq.u.rts.value = temp;
	  }
      }

  if(iw_set_ext(skfd, ifname, SIOCSIWRTS, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* 1 arg */
  return(1);
}

/*------------------------------------------------------------------*/
/*
 * Set Fragmentation Threshold
 */
static int
set_frag_info(int		skfd,
	      char *		ifname,
	      char *		args[],		/* Command line args */
	      int		count)		/* Args count */
{
  struct iwreq		wrq;

  /* Avoid "Unused parameter" warning */
  count = count;

  wrq.u.frag.value = -1;
  wrq.u.frag.fixed = 1;
  wrq.u.frag.disabled = 0;

  if(!strcasecmp(args[0], "off"))
    wrq.u.frag.disabled = 1;	/* i.e. max size */
  else
    if(!strcasecmp(args[0], "auto"))
      wrq.u.frag.fixed = 0;
    else
      {
	if(!strcasecmp(args[0], "fixed"))
	  {
	    /* Get old fragmentation threshold */
	    if(iw_get_ext(skfd, ifname, SIOCGIWFRAG, &wrq) < 0)
	      return(IWERR_GET_EXT);
	    wrq.u.frag.fixed = 1;
	  }
	else
	  {	/* Should be a numeric value */
	    long	temp;
	    if(sscanf(args[0], "%li", &(temp))
	       != 1)
	      {
		errarg = 0;
		return(IWERR_ARG_TYPE);
	      }
	    wrq.u.frag.value = temp;
	  }
      }

  if(iw_set_ext(skfd, ifname, SIOCSIWFRAG, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* 1 arg */
  return(1);
}

/*------------------------------------------------------------------*/
/*
 * Set Modulation
 */
static int
set_modulation_info(int		skfd,
		    char *	ifname,
		    char *	args[],		/* Command line args */
		    int		count)		/* Args count */
{
  struct iwreq		wrq;
  int			i = 1;

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  if(!strcasecmp(args[0], "auto"))
    wrq.u.param.fixed = 0;	/* i.e. use any modulation */
  else
    {
      if(!strcasecmp(args[0], "fixed"))
	{
	  /* Get old modulation */
	  if(iw_get_ext(skfd, ifname, SIOCGIWMODUL, &wrq) < 0)
	    return(IWERR_GET_EXT);
	  wrq.u.param.fixed = 1;
	}
      else
	{
	  int		k;

	  /* Allow multiple modulations, combine them together */
	  wrq.u.param.value = 0x0;
	  i = 0;
	  do
	    {
	      for(k = 0; k < IW_SIZE_MODUL_LIST; k++)
		{
		  if(!strcasecmp(args[i], iw_modul_list[k].cmd))
		    {
		      wrq.u.param.value |= iw_modul_list[k].mask;
		      ++i;
		      break;
		    }
		}
	    }
	  /* For as long as current arg matched and not out of args */
	  while((i < count) && (k < IW_SIZE_MODUL_LIST));

	  /* Check we got something */
	  if(i == 0)
	    {
	      errarg = 0;
	      return(IWERR_ARG_TYPE);
	    }

	  /* Check for an additional argument */
	  if((i < count) && (!strcasecmp(args[i], "auto")))
	    {
	      wrq.u.param.fixed = 0;
	      ++i;
	    }
	  if((i < count) && (!strcasecmp(args[i], "fixed")))
	    {
	      wrq.u.param.fixed = 1;
	      ++i;
	    }
	}
    }

  if(iw_set_ext(skfd, ifname, SIOCSIWMODUL, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* Var args */
  return(i);
}
#endif	/* WE_ESSENTIAL */

/*------------------------------------------------------------------*/
/*
 * Set commit
 */
static int
set_commit_info(int		skfd,
		char *		ifname,
		char *		args[],		/* Command line args */
		int		count)		/* Args count */
{
  struct iwreq		wrq;

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  if(iw_set_ext(skfd, ifname, SIOCSIWCOMMIT, &wrq) < 0)
    return(IWERR_SET_EXT);

  /* No args */
  return(0);
}

/************************** SET DISPATCHER **************************/
/*
 * This is a modified version of the dispatcher in iwlist.
 * The main difference is that here we may have multiple commands per
 * line. Also, most commands here do take arguments, and most often
 * a variable number of them.
 * Therefore, the handler *must* return how many args were consumed...
 *
 * Note that the use of multiple commands per line is not advised
 * in scripts, as it makes error management hard. All commands before
 * the error are executed, but commands after the error are not
 * processed.
 * We also try to give as much clue as possible via stderr to the caller
 * on which command did fail, but if there are two time the same command,
 * you don't know which one failed...
 */

/*------------------------------------------------------------------*/
/*
 * Map command line arguments to the proper procedure...
 */
typedef struct iwconfig_entry {
  const char *		cmd;		/* Command line shorthand */
  iw_enum_handler	fn;		/* Subroutine */
  int			min_count;
  int			request;	/* WE numerical ID */
  const char *		name;		/* Human readable string */
  const char *		argsname;	/* Args as human readable string */
} iwconfig_cmd;

static const struct iwconfig_entry iwconfig_cmds[] = {
  { "essid",		set_essid_info,		1,	SIOCSIWESSID,
	"Set ESSID",			"{NNN|any|on|off}" },
  { "mode",		set_mode_info,		1,	SIOCSIWMODE,
	"Set Mode",			"{managed|ad-hoc|master|...}" },
  { "freq",		set_freq_info,		1,	SIOCSIWFREQ,
	"Set Frequency",		"N.NNN[k|M|G]" },
  { "channel",		set_freq_info,		1,	SIOCSIWFREQ,
	"Set Frequency",		"N" },
  { "bit",		set_bitrate_info,	1,	SIOCSIWRATE,
	"Set Bit Rate",			"{N[k|M|G]|auto|fixed}" },
  { "rate",		set_bitrate_info,	1,	SIOCSIWRATE,
	"Set Bit Rate",			"{N[k|M|G]|auto|fixed}" },
  { "enc",		set_enc_info,		1,	SIOCSIWENCODE,
	"Set Encode",			"{NNNN-NNNN|off}" },
  { "key",		set_enc_info,		1,	SIOCSIWENCODE,
	"Set Encode",			"{NNNN-NNNN|off}"  },
  { "power",		set_power_info,		1,	SIOCSIWPOWER,
	"Set Power Management",		"{period N|timeout N|saving N|off}" },
#ifndef WE_ESSENTIAL
  { "nickname",		set_nick_info,		1,	SIOCSIWNICKN,
	"Set Nickname",			"NNN" },
  { "nwid",		set_nwid_info,		1,	SIOCSIWNWID,
	"Set NWID",			"{NN|on|off}" },
  { "ap",		set_apaddr_info,	1,	SIOCSIWAP,
	"Set AP Address",		"{N|off|auto}" },
  { "txpower",		set_txpower_info,	1,	SIOCSIWTXPOW,
	"Set Tx Power",			"{NmW|NdBm|off|auto}" },
  { "sens",		set_sens_info,		1,	SIOCSIWSENS,
	"Set Sensitivity",		"N" },
  { "retry",		set_retry_info,		1,	SIOCSIWRETRY,
	"Set Retry Limit",		"{limit N|lifetime N}" },
  { "rts",		set_rts_info,		1,	SIOCSIWRTS,
	"Set RTS Threshold",		"{N|auto|fixed|off}" },
  { "frag",		set_frag_info,		1,	SIOCSIWFRAG,
	"Set Fragmentation Threshold",	"{N|auto|fixed|off}" },
  { "modulation",	set_modulation_info,	1,	SIOCGIWMODUL,
	"Set Modulation",		"{11g|11a|CCK|OFDMg|...}" },
#endif	/* WE_ESSENTIAL */
  { "commit",		set_commit_info,	0,	SIOCSIWCOMMIT,
	"Commit changes",		"" },
  { NULL, NULL, 0, 0, NULL, NULL },
};

/*------------------------------------------------------------------*/
/*
 * Find the most appropriate command matching the command line
 */
static inline const iwconfig_cmd *
find_command(const char *	cmd)
{
  const iwconfig_cmd *	found = NULL;
  int			ambig = 0;
  unsigned int		len = strlen(cmd);
  int			i;

  /* Go through all commands */
  for(i = 0; iwconfig_cmds[i].cmd != NULL; ++i)
    {
      /* No match -> next one */
      if(strncasecmp(iwconfig_cmds[i].cmd, cmd, len) != 0)
	continue;

      /* Exact match -> perfect */
      if(len == strlen(iwconfig_cmds[i].cmd))
	return &iwconfig_cmds[i];

      /* Partial match */
      if(found == NULL)
	/* First time */
	found = &iwconfig_cmds[i];
      else
	/* Another time */
	if (iwconfig_cmds[i].fn != found->fn)
	  ambig = 1;
    }

  if(found == NULL)
    {
      fprintf(stderr, "iwconfig: unknown command \"%s\"\n", cmd);
      return NULL;
    }

  if(ambig)
    {
      fprintf(stderr, "iwconfig: command \"%s\" is ambiguous\n", cmd);
      return NULL;
    }

  return found;
}

/*------------------------------------------------------------------*/
/*
 * Set the wireless options requested on command line
 * Find the individual commands and call the appropriate subroutine
 */
static int
set_info(int		skfd,		/* The socket */
	 char *		args[],		/* Command line args */
	 int		count,		/* Args count */
	 char *		ifname)		/* Dev name */
{
  const iwconfig_cmd *	iwcmd;
  int			ret;

  /* Loop until we run out of args... */
  while(count > 0)
    {
      /* find the command matching the keyword */
      iwcmd = find_command(args[0]);
      if(iwcmd == NULL)
	{
	  /* Here we have an unrecognised arg... Error already printed out. */
	  return(-1);
	}

      /* One arg is consumed (the command name) */
      args++;
      count--;

      /* Check arg numbers */
      if(count < iwcmd->min_count)
	ret = IWERR_ARG_NUM;
      else
	ret = 0;

      /* Call the command */
      if(!ret)
	ret = (*iwcmd->fn)(skfd, ifname, args, count);

      /* Deal with various errors */
      if(ret < 0)
	{
	  int	request = iwcmd->request;
	  if(ret == IWERR_GET_EXT)
	    request++;	/* Transform the SET into GET */

	  fprintf(stderr, "Error for wireless request \"%s\" (%X) :\n",
		  iwcmd->name, request);
	  switch(ret)
	    {
	    case IWERR_ARG_NUM:
	      fprintf(stderr, "    too few arguments.\n");
	      break;
	    case IWERR_ARG_TYPE:
	      if(errarg < 0)
		errarg = 0;
	      if(errarg >= count)
		errarg = count - 1;
	      fprintf(stderr, "    invalid argument \"%s\".\n", args[errarg]);
	      break;
	    case IWERR_ARG_SIZE:
	      fprintf(stderr, "    argument too big (max %d)\n", errmax);
	      break;
	    case IWERR_ARG_CONFLICT:
	      if(errarg < 0)
		errarg = 0;
	      if(errarg >= count)
		errarg = count - 1;
	      fprintf(stderr, "    conflicting argument \"%s\".\n", args[errarg]);
	      break;
	    case IWERR_SET_EXT:
	      fprintf(stderr, "    SET failed on device %-1.16s ; %s.\n",
		      ifname, strerror(errno));
	      break;
	    case IWERR_GET_EXT:
	      fprintf(stderr, "    GET failed on device %-1.16s ; %s.\n",
		      ifname, strerror(errno));
	      break;
	    }
	  /* Stop processing, we don't know if we are in a consistent state
	   * in reading the command line */
	  return(ret);
	}

      /* Substract consumed args from command line */
      args += ret;
      count -= ret;

      /* Loop back */
    }

  /* Done, all done */
  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Display help
 */
static inline void
iw_usage(void)
{
  int i;

  fprintf(stderr,   "Usage: iwconfig [interface]\n");
  for(i = 0; iwconfig_cmds[i].cmd != NULL; ++i)
    fprintf(stderr, "                interface %s %s\n",
	    iwconfig_cmds[i].cmd, iwconfig_cmds[i].argsname);
  fprintf(stderr,   "       Check man pages for more details.\n");
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
  int skfd;		/* generic raw socket desc.	*/
  int goterr = 0;

  /* Create a channel to the NET kernel. */
  if((skfd = iw_sockets_open()) < 0)
    {
      perror("socket");
      exit(-1);
    }

  /* No argument : show the list of all device + info */
  if(argc == 1)
    iw_enum_devices(skfd, &print_info, NULL, 0);
  else
    /* Special case for help... */
    if((!strcmp(argv[1], "-h")) || (!strcmp(argv[1], "--help")))
      iw_usage();
    else
      /* Special case for version... */
      if(!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version"))
	goterr = iw_print_version_info("iwconfig");
      else
	{
	  /* '--' escape device name */
	  if((argc > 2) && !strcmp(argv[1], "--"))
	    {
	      argv++;
	      argc--;
	    }

	  /* The device name must be the first argument */
	  if(argc == 2)
	    print_info(skfd, argv[1], NULL, 0);
	  else
	    /* The other args on the line specify options to be set... */
	    goterr = set_info(skfd, argv + 2, argc - 2, argv[1]);
	}

  /* Close the socket. */
  iw_sockets_close(skfd);

  return(goterr);
}

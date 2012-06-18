/*
 *	Wireless Tools
 *
 *		Jean II - HPLB 97->99 - HPL 99->07
 *
 * Common subroutines to all the wireless tools...
 *
 * This file is released under the GPL license.
 *     Copyright (c) 1997-2007 Jean Tourrilhes <jt@hpl.hp.com>
 */

/***************************** INCLUDES *****************************/

#include "iwlib.h"		/* Header */

/************************ CONSTANTS & MACROS ************************/

/*
 * Constants fof WE-9->15
 */
#define IW15_MAX_FREQUENCIES	16
#define IW15_MAX_BITRATES	8
#define IW15_MAX_TXPOWER	8
#define IW15_MAX_ENCODING_SIZES	8
#define IW15_MAX_SPY		8
#define IW15_MAX_AP		8

/****************************** TYPES ******************************/

/*
 *	Struct iw_range up to WE-15
 */
struct	iw15_range
{
	__u32		throughput;
	__u32		min_nwid;
	__u32		max_nwid;
	__u16		num_channels;
	__u8		num_frequency;
	struct iw_freq	freq[IW15_MAX_FREQUENCIES];
	__s32		sensitivity;
	struct iw_quality	max_qual;
	__u8		num_bitrates;
	__s32		bitrate[IW15_MAX_BITRATES];
	__s32		min_rts;
	__s32		max_rts;
	__s32		min_frag;
	__s32		max_frag;
	__s32		min_pmp;
	__s32		max_pmp;
	__s32		min_pmt;
	__s32		max_pmt;
	__u16		pmp_flags;
	__u16		pmt_flags;
	__u16		pm_capa;
	__u16		encoding_size[IW15_MAX_ENCODING_SIZES];
	__u8		num_encoding_sizes;
	__u8		max_encoding_tokens;
	__u16		txpower_capa;
	__u8		num_txpower;
	__s32		txpower[IW15_MAX_TXPOWER];
	__u8		we_version_compiled;
	__u8		we_version_source;
	__u16		retry_capa;
	__u16		retry_flags;
	__u16		r_time_flags;
	__s32		min_retry;
	__s32		max_retry;
	__s32		min_r_time;
	__s32		max_r_time;
	struct iw_quality	avg_qual;
};

/*
 * Union for all the versions of iwrange.
 * Fortunately, I mostly only add fields at the end, and big-bang
 * reorganisations are few.
 */
union	iw_range_raw
{
	struct iw15_range	range15;	/* WE 9->15 */
	struct iw_range		range;		/* WE 16->current */
};

/*
 * Offsets in iw_range struct
 */
#define iwr15_off(f)	( ((char *) &(((struct iw15_range *) NULL)->f)) - \
			  (char *) NULL)
#define iwr_off(f)	( ((char *) &(((struct iw_range *) NULL)->f)) - \
			  (char *) NULL)

/**************************** VARIABLES ****************************/

/* Modes as human readable strings */
const char * const iw_operation_mode[] = { "Auto",
					"Ad-Hoc",
					"Managed",
					"Master",
					"Repeater",
					"Secondary",
					"Monitor",
					"Unknown/bug" };

/* Modulations as human readable strings */
const struct iw_modul_descr	iw_modul_list[] = {
  /* Start with aggregate types, so that they display first */
  { IW_MODUL_11AG, "11ag",
    "IEEE 802.11a + 802.11g (2.4 & 5 GHz, up to 54 Mb/s)" },
  { IW_MODUL_11AB, "11ab",
    "IEEE 802.11a + 802.11b (2.4 & 5 GHz, up to 54 Mb/s)" },
  { IW_MODUL_11G, "11g", "IEEE 802.11g (2.4 GHz, up to 54 Mb/s)" },
  { IW_MODUL_11A, "11a", "IEEE 802.11a (5 GHz, up to 54 Mb/s)" },
  { IW_MODUL_11B, "11b", "IEEE 802.11b (2.4 GHz, up to 11 Mb/s)" },

  /* Proprietary aggregates */
  { IW_MODUL_TURBO | IW_MODUL_11A, "turboa",
    "Atheros turbo mode at 5 GHz (up to 108 Mb/s)" },
  { IW_MODUL_TURBO | IW_MODUL_11G, "turbog",
    "Atheros turbo mode at 2.4 GHz (up to 108 Mb/s)" },
  { IW_MODUL_PBCC | IW_MODUL_11B, "11+",
    "TI 802.11+ (2.4 GHz, up to 22 Mb/s)" },

  /* Individual modulations */
  { IW_MODUL_OFDM_G, "OFDMg",
    "802.11g higher rates, OFDM at 2.4 GHz (up to 54 Mb/s)" },
  { IW_MODUL_OFDM_A, "OFDMa", "802.11a, OFDM at 5 GHz (up to 54 Mb/s)" },
  { IW_MODUL_CCK, "CCK", "802.11b higher rates (2.4 GHz, up to 11 Mb/s)" },
  { IW_MODUL_DS, "DS", "802.11 Direct Sequence (2.4 GHz, up to 2 Mb/s)" },
  { IW_MODUL_FH, "FH", "802.11 Frequency Hopping (2,4 GHz, up to 2 Mb/s)" },

  /* Proprietary modulations */
  { IW_MODUL_TURBO, "turbo",
    "Atheros turbo mode, channel bonding (up to 108 Mb/s)" },
  { IW_MODUL_PBCC, "PBCC",
    "TI 802.11+ higher rates (2.4 GHz, up to 22 Mb/s)" },
  { IW_MODUL_CUSTOM, "custom",
    "Driver specific modulation (check driver documentation)" },
};

/* Disable runtime version warning in iw_get_range_info() */
int	iw_ignore_version = 0;

/************************ SOCKET SUBROUTINES *************************/

/*------------------------------------------------------------------*/
/*
 * Open a socket.
 * Depending on the protocol present, open the right socket. The socket
 * will allow us to talk to the driver.
 */
int
iw_sockets_open(void)
{
  static const int families[] = {
    AF_INET, AF_IPX, AF_AX25, AF_APPLETALK
  };
  unsigned int	i;
  int		sock;

  /*
   * Now pick any (exisiting) useful socket family for generic queries
   * Note : don't open all the socket, only returns when one matches,
   * all protocols might not be valid.
   * Workaround by Jim Kaba <jkaba@sarnoff.com>
   * Note : in 99% of the case, we will just open the inet_sock.
   * The remaining 1% case are not fully correct...
   */

  /* Try all families we support */
  for(i = 0; i < sizeof(families)/sizeof(int); ++i)
    {
      /* Try to open the socket, if success returns it */
      sock = socket(families[i], SOCK_DGRAM, 0);
      if(sock >= 0)
	return sock;
  }

  return -1;
}

/*------------------------------------------------------------------*/
/*
 * Extract the interface name out of /proc/net/wireless or /proc/net/dev.
 */
static inline char *
iw_get_ifname(char *	name,	/* Where to store the name */
	      int	nsize,	/* Size of name buffer */
	      char *	buf)	/* Current position in buffer */
{
  char *	end;

  /* Skip leading spaces */
  while(isspace(*buf))
    buf++;

#ifndef IW_RESTRIC_ENUM
  /* Get name up to the last ':'. Aliases may contain ':' in them,
   * but the last one should be the separator */
  end = strrchr(buf, ':');
#else
  /* Get name up to ": "
   * Note : we compare to ": " to make sure to process aliased interfaces
   * properly. Doesn't work on /proc/net/dev, because it doesn't guarantee
   * a ' ' after the ':'*/
  end = strstr(buf, ": ");
#endif

  /* Not found ??? To big ??? */
  if((end == NULL) || (((end - buf) + 1) > nsize))
    return(NULL);

  /* Copy */
  memcpy(name, buf, (end - buf));
  name[end - buf] = '\0';

  /* Return value currently unused, just make sure it's non-NULL */
  return(end);
}

/*------------------------------------------------------------------*/
/*
 * Enumerate devices and call specified routine
 * The new way just use /proc/net/wireless, so get all wireless interfaces,
 * whether configured or not. This is the default if available.
 * The old way use SIOCGIFCONF, so get only configured interfaces (wireless
 * or not).
 */
void
iw_enum_devices(int		skfd,
		iw_enum_handler	fn,
		char *		args[],
		int		count)
{
  char		buff[1024];
  FILE *	fh;
  struct ifconf ifc;
  struct ifreq *ifr;
  int		i;

#ifndef IW_RESTRIC_ENUM
  /* Check if /proc/net/dev is available */
  fh = fopen(PROC_NET_DEV, "r");
#else
  /* Check if /proc/net/wireless is available */
  fh = fopen(PROC_NET_WIRELESS, "r");
#endif

  if(fh != NULL)
    {
      /* Success : use data from /proc/net/wireless */

      /* Eat 2 lines of header */
      fgets(buff, sizeof(buff), fh);
      fgets(buff, sizeof(buff), fh);

      /* Read each device line */
      while(fgets(buff, sizeof(buff), fh))
	{
	  char	name[IFNAMSIZ + 1];
	  char *s;

	  /* Skip empty or almost empty lines. It seems that in some
	   * cases fgets return a line with only a newline. */
	  if((buff[0] == '\0') || (buff[1] == '\0'))
	    continue;

	  /* Extract interface name */
	  s = iw_get_ifname(name, sizeof(name), buff);

	  if(!s)
	    {
	      /* Failed to parse, complain and continue */
#ifndef IW_RESTRIC_ENUM
	      fprintf(stderr, "Cannot parse " PROC_NET_DEV "\n");
#else
	      fprintf(stderr, "Cannot parse " PROC_NET_WIRELESS "\n");
#endif
	    }
	  else
	    /* Got it, print info about this interface */
	    (*fn)(skfd, name, args, count);
	}

      fclose(fh);
    }
  else
    {
      /* Get list of configured devices using "traditional" way */
      ifc.ifc_len = sizeof(buff);
      ifc.ifc_buf = buff;
      if(ioctl(skfd, SIOCGIFCONF, &ifc) < 0)
	{
	  fprintf(stderr, "SIOCGIFCONF: %s\n", strerror(errno));
	  return;
	}
      ifr = ifc.ifc_req;

      /* Print them */
      for(i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++)
	(*fn)(skfd, ifr->ifr_name, args, count);
    }
}

/*********************** WIRELESS SUBROUTINES ************************/

/*------------------------------------------------------------------*/
/*
 * Extract WE version number from /proc/net/wireless
 * In most cases, you really want to get version information from
 * the range info (range->we_version_compiled), see below...
 *
 * If we have WE-16 and later, the WE version is available at the
 * end of the header line of the file.
 * For version prior to that, we can only detect the change from
 * v11 to v12, so we do an approximate job. Fortunately, v12 to v15
 * are highly binary compatible (on the struct level).
 */
int
iw_get_kernel_we_version(void)
{
  char		buff[1024];
  FILE *	fh;
  char *	p;
  int		v;

  /* Check if /proc/net/wireless is available */
  fh = fopen(PROC_NET_WIRELESS, "r");

  if(fh == NULL)
    {
      fprintf(stderr, "Cannot read " PROC_NET_WIRELESS "\n");
      return(-1);
    }

  /* Read the first line of buffer */
  fgets(buff, sizeof(buff), fh);

  if(strstr(buff, "| WE") == NULL)
    {
      /* Prior to WE16, so explicit version not present */

      /* Black magic */
      if(strstr(buff, "| Missed") == NULL)
	v = 11;
      else
	v = 15;
      fclose(fh);
      return(v);
    }

  /* Read the second line of buffer */
  fgets(buff, sizeof(buff), fh);

  /* Get to the last separator, to get the version */
  p = strrchr(buff, '|');
  if((p == NULL) || (sscanf(p + 1, "%d", &v) != 1))
    {
      fprintf(stderr, "Cannot parse " PROC_NET_WIRELESS "\n");
      fclose(fh);
      return(-1);
    }

  fclose(fh);
  return(v);
}

/*------------------------------------------------------------------*/
/*
 * Print the WE versions of the interface.
 */
static int
print_iface_version_info(int	skfd,
			 char *	ifname,
			 char *	args[],		/* Command line args */
			 int	count)		/* Args count */
{
  struct iwreq		wrq;
  char			buffer[sizeof(iwrange) * 2];	/* Large enough */
  struct iw_range *	range;

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* If no wireless name : no wireless extensions.
   * This enable us to treat the SIOCGIWRANGE failure below properly. */
  if(iw_get_ext(skfd, ifname, SIOCGIWNAME, &wrq) < 0)
    return(-1);

  /* Cleanup */
  memset(buffer, 0, sizeof(buffer));

  wrq.u.data.pointer = (caddr_t) buffer;
  wrq.u.data.length = sizeof(buffer);
  wrq.u.data.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWRANGE, &wrq) < 0)
    {
      /* Interface support WE (see above), but not IWRANGE */
      fprintf(stderr, "%-8.16s  Driver has no Wireless Extension version information.\n\n", ifname);
      return(0);
    }

  /* Copy stuff at the right place, ignore extra */
  range = (struct iw_range *) buffer;

  /* For new versions, we can check the version directly, for old versions
   * we use magic. 300 bytes is a also magic number, don't touch... */
  if(wrq.u.data.length >= 300)
    {
      /* Version is always at the same offset, so it's ok */
      printf("%-8.16s  Recommend Wireless Extension v%d or later,\n",
	     ifname, range->we_version_source);
      printf("          Currently compiled with Wireless Extension v%d.\n\n",
	     range->we_version_compiled);
    }
  else
    {
      fprintf(stderr, "%-8.16s  Wireless Extension version too old.\n\n",
		      ifname);
    }


  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Print the WE versions of the tools.
 */
int
iw_print_version_info(const char *	toolname)
{
  int		skfd;			/* generic raw socket desc.	*/
  int		we_kernel_version;

  /* Create a channel to the NET kernel. */
  if((skfd = iw_sockets_open()) < 0)
    {
      perror("socket");
      return -1;
    }

  /* Information about the tools themselves */
  if(toolname != NULL)
    printf("%-8.16s  Wireless-Tools version %d\n", toolname, WT_VERSION);
  printf("          Compatible with Wireless Extension v11 to v%d.\n\n",
	 WE_MAX_VERSION);

  /* Get version from kernel */
  we_kernel_version = iw_get_kernel_we_version();
  /* Only version >= 16 can be verified, other are guessed */
  if(we_kernel_version > 15)
    printf("Kernel    Currently compiled with Wireless Extension v%d.\n\n",
	   we_kernel_version);

  /* Version for each device */
  iw_enum_devices(skfd, &print_iface_version_info, NULL, 0);

  iw_sockets_close(skfd);

  return 0;
}

/*------------------------------------------------------------------*/
/*
 * Get the range information out of the driver
 */
int
iw_get_range_info(int		skfd,
		  const char *	ifname,
		  iwrange *	range)
{
  struct iwreq		wrq;
  char			buffer[sizeof(iwrange) * 2];	/* Large enough */
  union iw_range_raw *	range_raw;

  /* Cleanup */
  bzero(buffer, sizeof(buffer));

  wrq.u.data.pointer = (caddr_t) buffer;
  wrq.u.data.length = sizeof(buffer);
  wrq.u.data.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWRANGE, &wrq) < 0)
    return(-1);

  /* Point to the buffer */
  range_raw = (union iw_range_raw *) buffer;

  /* For new versions, we can check the version directly, for old versions
   * we use magic. 300 bytes is a also magic number, don't touch... */
  if(wrq.u.data.length < 300)
    {
      /* That's v10 or earlier. Ouch ! Let's make a guess...*/
      range_raw->range.we_version_compiled = 9;
    }

  /* Check how it needs to be processed */
  if(range_raw->range.we_version_compiled > 15)
    {
      /* This is our native format, that's easy... */
      /* Copy stuff at the right place, ignore extra */
      memcpy((char *) range, buffer, sizeof(iwrange));
    }
  else
    {
      /* Zero unknown fields */
      bzero((char *) range, sizeof(struct iw_range));

      /* Initial part unmoved */
      memcpy((char *) range,
	     buffer,
	     iwr15_off(num_channels));
      /* Frequencies pushed futher down towards the end */
      memcpy((char *) range + iwr_off(num_channels),
	     buffer + iwr15_off(num_channels),
	     iwr15_off(sensitivity) - iwr15_off(num_channels));
      /* This one moved up */
      memcpy((char *) range + iwr_off(sensitivity),
	     buffer + iwr15_off(sensitivity),
	     iwr15_off(num_bitrates) - iwr15_off(sensitivity));
      /* This one goes after avg_qual */
      memcpy((char *) range + iwr_off(num_bitrates),
	     buffer + iwr15_off(num_bitrates),
	     iwr15_off(min_rts) - iwr15_off(num_bitrates));
      /* Number of bitrates has changed, put it after */
      memcpy((char *) range + iwr_off(min_rts),
	     buffer + iwr15_off(min_rts),
	     iwr15_off(txpower_capa) - iwr15_off(min_rts));
      /* Added encoding_login_index, put it after */
      memcpy((char *) range + iwr_off(txpower_capa),
	     buffer + iwr15_off(txpower_capa),
	     iwr15_off(txpower) - iwr15_off(txpower_capa));
      /* Hum... That's an unexpected glitch. Bummer. */
      memcpy((char *) range + iwr_off(txpower),
	     buffer + iwr15_off(txpower),
	     iwr15_off(avg_qual) - iwr15_off(txpower));
      /* Avg qual moved up next to max_qual */
      memcpy((char *) range + iwr_off(avg_qual),
	     buffer + iwr15_off(avg_qual),
	     sizeof(struct iw_quality));
    }

  /* We are now checking much less than we used to do, because we can
   * accomodate more WE version. But, there are still cases where things
   * will break... */
  if(!iw_ignore_version)
    {
      /* We don't like very old version (unfortunately kernel 2.2.X) */
      if(range->we_version_compiled <= 10)
	{
	  fprintf(stderr, "Warning: Driver for device %s has been compiled with an ancient version\n", ifname);
	  fprintf(stderr, "of Wireless Extension, while this program support version 11 and later.\n");
	  fprintf(stderr, "Some things may be broken...\n\n");
	}

      /* We don't like future versions of WE, because we can't cope with
       * the unknown */
      if(range->we_version_compiled > WE_MAX_VERSION)
	{
	  fprintf(stderr, "Warning: Driver for device %s has been compiled with version %d\n", ifname, range->we_version_compiled);
	  fprintf(stderr, "of Wireless Extension, while this program supports up to version %d.\n", WE_MAX_VERSION);
	  fprintf(stderr, "Some things may be broken...\n\n");
	}

      /* Driver version verification */
      if((range->we_version_compiled > 10) &&
	 (range->we_version_compiled < range->we_version_source))
	{
	  fprintf(stderr, "Warning: Driver for device %s recommend version %d of Wireless Extension,\n", ifname, range->we_version_source);
	  fprintf(stderr, "but has been compiled with version %d, therefore some driver features\n", range->we_version_compiled);
	  fprintf(stderr, "may not be available...\n\n");
	}
      /* Note : we are only trying to catch compile difference, not source.
       * If the driver source has not been updated to the latest, it doesn't
       * matter because the new fields are set to zero */
    }

  /* Don't complain twice.
   * In theory, the test apply to each individual driver, but usually
   * all drivers are compiled from the same kernel. */
  iw_ignore_version = 1;

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Get information about what private ioctls are supported by the driver
 *
 * Note : there is one danger using this function. If it return 0, you
 * still need to free() the buffer. Beware.
 */
int
iw_get_priv_info(int		skfd,
		 const char *	ifname,
		 iwprivargs **	ppriv)
{
  struct iwreq		wrq;
  iwprivargs *		priv = NULL;	/* Not allocated yet */
  int			maxpriv = 16;	/* Minimum for compatibility WE<13 */
  iwprivargs *		newpriv;

  /* Some driver may return a very large number of ioctls. Some
   * others a very small number. We now use a dynamic allocation
   * of the array to satisfy everybody. Of course, as we don't know
   * in advance the size of the array, we try various increasing
   * sizes. Jean II */
  do
    {
      /* (Re)allocate the buffer */
      newpriv = realloc(priv, maxpriv * sizeof(priv[0]));
      if(newpriv == NULL)
	{
	  fprintf(stderr, "%s: Allocation failed\n", __FUNCTION__);
	  break;
	}
      priv = newpriv;

      /* Ask the driver if it's large enough */
      wrq.u.data.pointer = (caddr_t) priv;
      wrq.u.data.length = maxpriv;
      wrq.u.data.flags = 0;
      if(iw_get_ext(skfd, ifname, SIOCGIWPRIV, &wrq) >= 0)
	{
	  /* Success. Pass the buffer by pointer */
	  *ppriv = priv;
	  /* Return the number of ioctls */
	  return(wrq.u.data.length);
	}

      /* Only E2BIG means the buffer was too small, abort on other errors */
      if(errno != E2BIG)
	{
	  /* Most likely "not supported". Don't barf. */
	  break;
	}

      /* Failed. We probably need a bigger buffer. Check if the kernel
       * gave us any hints. */
      if(wrq.u.data.length > maxpriv)
	maxpriv = wrq.u.data.length;
      else
	maxpriv *= 2;
    }
  while(maxpriv < 1000);

  /* Cleanup */
  if(priv)
    free(priv);
  *ppriv = NULL;

  return(-1);
}

/*------------------------------------------------------------------*/
/*
 * Get essential wireless config from the device driver
 * We will call all the classical wireless ioctl on the driver through
 * the socket to know what is supported and to get the settings...
 * Note : compare to the version in iwconfig, we extract only
 * what's *really* needed to configure a device...
 */
int
iw_get_basic_config(int			skfd,
		    const char *	ifname,
		    wireless_config *	info)
{
  struct iwreq		wrq;

  memset((char *) info, 0, sizeof(struct wireless_config));

  /* Get wireless name */
  if(iw_get_ext(skfd, ifname, SIOCGIWNAME, &wrq) < 0)
    /* If no wireless name : no wireless extensions */
    return(-1);
  else
    {
      strncpy(info->name, wrq.u.name, IFNAMSIZ);
      info->name[IFNAMSIZ] = '\0';
    }

  /* Get network ID */
  if(iw_get_ext(skfd, ifname, SIOCGIWNWID, &wrq) >= 0)
    {
      info->has_nwid = 1;
      memcpy(&(info->nwid), &(wrq.u.nwid), sizeof(iwparam));
    }

  /* Get frequency / channel */
  if(iw_get_ext(skfd, ifname, SIOCGIWFREQ, &wrq) >= 0)
    {
      info->has_freq = 1;
      info->freq = iw_freq2float(&(wrq.u.freq));
      info->freq_flags = wrq.u.freq.flags;
    }

  /* Get encryption information */
  wrq.u.data.pointer = (caddr_t) info->key;
  wrq.u.data.length = IW_ENCODING_TOKEN_MAX;
  wrq.u.data.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWENCODE, &wrq) >= 0)
    {
      info->has_key = 1;
      info->key_size = wrq.u.data.length;
      info->key_flags = wrq.u.data.flags;
    }

  /* Get ESSID */
  wrq.u.essid.pointer = (caddr_t) info->essid;
  wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
  wrq.u.essid.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWESSID, &wrq) >= 0)
    {
      info->has_essid = 1;
      info->essid_on = wrq.u.data.flags;
    }

  /* Get operation mode */
  if(iw_get_ext(skfd, ifname, SIOCGIWMODE, &wrq) >= 0)
    {
      info->has_mode = 1;
      /* Note : event->u.mode is unsigned, no need to check <= 0 */
      if(wrq.u.mode < IW_NUM_OPER_MODE)
	info->mode = wrq.u.mode;
      else
	info->mode = IW_NUM_OPER_MODE;	/* Unknown/bug */
    }

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Set essential wireless config in the device driver
 * We will call all the classical wireless ioctl on the driver through
 * the socket to know what is supported and to set the settings...
 * We support only the restricted set as above...
 */
int
iw_set_basic_config(int			skfd,
		    const char *	ifname,
		    wireless_config *	info)
{
  struct iwreq		wrq;
  int			ret = 0;

  /* Get wireless name (check if interface is valid) */
  if(iw_get_ext(skfd, ifname, SIOCGIWNAME, &wrq) < 0)
    /* If no wireless name : no wireless extensions */
    return(-2);

  /* Set the current mode of operation
   * Mode need to be first : some settings apply only in a specific mode
   * (such as frequency).
   */
  if(info->has_mode)
    {
      strncpy(wrq.ifr_name, ifname, IFNAMSIZ);
      wrq.u.mode = info->mode;

      if(iw_get_ext(skfd, ifname, SIOCSIWMODE, &wrq) < 0)
	{
	  fprintf(stderr, "SIOCSIWMODE: %s\n", strerror(errno));
	  ret = -1;
	}
    }

  /* Set frequency / channel */
  if(info->has_freq)
    {
      iw_float2freq(info->freq, &(wrq.u.freq));

      if(iw_set_ext(skfd, ifname, SIOCSIWFREQ, &wrq) < 0)
	{
	  fprintf(stderr, "SIOCSIWFREQ: %s\n", strerror(errno));
	  ret = -1;
	}
    }

  /* Set encryption information */
  if(info->has_key)
    {
      int		flags = info->key_flags;

      /* Check if there is a key index */
      if((flags & IW_ENCODE_INDEX) > 0)
	{
	  /* Set the index */
	  wrq.u.data.pointer = (caddr_t) NULL;
	  wrq.u.data.flags = (flags & (IW_ENCODE_INDEX)) | IW_ENCODE_NOKEY;
	  wrq.u.data.length = 0;

	  if(iw_set_ext(skfd, ifname, SIOCSIWENCODE, &wrq) < 0)
	    {
	      fprintf(stderr, "SIOCSIWENCODE(%d): %s\n",
		      errno, strerror(errno));
	      ret = -1;
	    }
	}

      /* Mask out index to minimise probability of reject when setting key */
      flags = flags & (~IW_ENCODE_INDEX);

      /* Set the key itself (set current key in this case) */
      wrq.u.data.pointer = (caddr_t) info->key;
      wrq.u.data.length = info->key_size;
      wrq.u.data.flags = flags;

      /* Compatibility with WE<13 */
      if(flags & IW_ENCODE_NOKEY)
	wrq.u.data.pointer = NULL;

      if(iw_set_ext(skfd, ifname, SIOCSIWENCODE, &wrq) < 0)
	{
	  fprintf(stderr, "SIOCSIWENCODE(%d): %s\n",
		  errno, strerror(errno));
	  ret = -1;
	}
    }

  /* Set Network ID, if available (this is for non-802.11 cards) */
  if(info->has_nwid)
    {
      memcpy(&(wrq.u.nwid), &(info->nwid), sizeof(iwparam));
      wrq.u.nwid.fixed = 1;	/* Hum... When in Rome... */

      if(iw_set_ext(skfd, ifname, SIOCSIWNWID, &wrq) < 0)
	{
	  fprintf(stderr, "SIOCSIWNWID: %s\n", strerror(errno));
	  ret = -1;
	}
    }

  /* Set ESSID (extended network), if available.
   * ESSID need to be last : most device re-perform the scanning/discovery
   * when this is set, and things like encryption keys are better be
   * defined if we want to discover the right set of APs/nodes.
   */
  if(info->has_essid)
    {
      int		we_kernel_version;
      we_kernel_version = iw_get_kernel_we_version();

      wrq.u.essid.pointer = (caddr_t) info->essid;
      wrq.u.essid.length = strlen(info->essid);
      wrq.u.data.flags = info->essid_on;
      if(we_kernel_version < 21)
	wrq.u.essid.length++;

      if(iw_set_ext(skfd, ifname, SIOCSIWESSID, &wrq) < 0)
	{
	  fprintf(stderr, "SIOCSIWESSID: %s\n", strerror(errno));
	  ret = -1;
	}
    }

  return(ret);
}

/*********************** PROTOCOL SUBROUTINES ***********************/
/*
 * Fun stuff with protocol identifiers (SIOCGIWNAME).
 * We assume that drivers are returning sensible values in there,
 * which is not always the case :-(
 */

/*------------------------------------------------------------------*/
/*
 * Compare protocol identifiers.
 * We don't want to know if the two protocols are the exactly same,
 * but if they interoperate at some level, and also if they accept the
 * same type of config (ESSID vs NWID, freq...).
 * This is supposed to work around the alphabet soup.
 * Return 1 if protocols are compatible, 0 otherwise
 */
int
iw_protocol_compare(const char *	protocol1,
		    const char *	protocol2)
{
  const char *	dot11 = "IEEE 802.11";
  const char *	dot11_ds = "Dbg";
  const char *	dot11_5g = "a";

  /* If the strings are the same -> easy */
  if(!strncmp(protocol1, protocol2, IFNAMSIZ))
    return(1);

  /* Are we dealing with one of the 802.11 variant ? */
  if( (!strncmp(protocol1, dot11, strlen(dot11))) &&
      (!strncmp(protocol2, dot11, strlen(dot11))) )
    {
      const char *	sub1 = protocol1 + strlen(dot11);
      const char *	sub2 = protocol2 + strlen(dot11);
      unsigned int	i;
      int		isds1 = 0;
      int		isds2 = 0;
      int		is5g1 = 0;
      int		is5g2 = 0;

      /* Check if we find the magic letters telling it's DS compatible */
      for(i = 0; i < strlen(dot11_ds); i++)
	{
	  if(strchr(sub1, dot11_ds[i]) != NULL)
	    isds1 = 1;
	  if(strchr(sub2, dot11_ds[i]) != NULL)
	    isds2 = 1;
	}
      if(isds1 && isds2)
	return(1);

      /* Check if we find the magic letters telling it's 5GHz compatible */
      for(i = 0; i < strlen(dot11_5g); i++)
	{
	  if(strchr(sub1, dot11_5g[i]) != NULL)
	    is5g1 = 1;
	  if(strchr(sub2, dot11_5g[i]) != NULL)
	    is5g2 = 1;
	}
      if(is5g1 && is5g2)
	return(1);
    }
  /* Not compatible */
  return(0);
}

/********************** FREQUENCY SUBROUTINES ***********************/
/*
 * Note : the two functions below are the cause of troubles on
 * various embeeded platforms, as they are the reason we require
 * libm (math library).
 * In this case, please use enable BUILD_NOLIBM in the makefile
 *
 * FIXME : check negative mantissa and exponent
 */

/*------------------------------------------------------------------*/
/*
 * Convert a floating point the our internal representation of
 * frequencies.
 * The kernel doesn't want to hear about floating point, so we use
 * this custom format instead.
 */
void
iw_float2freq(double	in,
	      iwfreq *	out)
{
#ifdef WE_NOLIBM
  /* Version without libm : slower */
  out->e = 0;
  while(in > 1e9)
    {
      in /= 10;
      out->e++;
    }
  out->m = (long) in;
#else	/* WE_NOLIBM */
  /* Version with libm : faster */
  out->e = (short) (floor(log10(in)));
  if(out->e > 8)
    {
      out->m = ((long) (floor(in / pow(10,out->e - 6)))) * 100;
      out->e -= 8;
    }
  else
    {
      out->m = (long) in;
      out->e = 0;
    }
#endif	/* WE_NOLIBM */
}

/*------------------------------------------------------------------*/
/*
 * Convert our internal representation of frequencies to a floating point.
 */
double
iw_freq2float(const iwfreq *	in)
{
#ifdef WE_NOLIBM
  /* Version without libm : slower */
  int		i;
  double	res = (double) in->m;
  for(i = 0; i < in->e; i++)
    res *= 10;
  return(res);
#else	/* WE_NOLIBM */
  /* Version with libm : faster */
  return ((double) in->m) * pow(10,in->e);
#endif	/* WE_NOLIBM */
}

/*------------------------------------------------------------------*/
/*
 * Output a frequency with proper scaling
 */
void
iw_print_freq_value(char *	buffer,
		    int		buflen,
		    double	freq)
{
  if(freq < KILO)
    snprintf(buffer, buflen, "%g", freq);
  else
    {
      char	scale;
      int	divisor;

      if(freq >= GIGA)
	{
	  scale = 'G';
	  divisor = GIGA;
	}
      else
	{
	  if(freq >= MEGA)
	    {
	      scale = 'M';
	      divisor = MEGA;
	    }
	  else
	    {
	      scale = 'k';
	      divisor = KILO;
	    }
	}
      snprintf(buffer, buflen, "%g %cHz", freq / divisor, scale);
    }
}

/*------------------------------------------------------------------*/
/*
 * Output a frequency with proper scaling
 */
void
iw_print_freq(char *	buffer,
	      int	buflen,
	      double	freq,
	      int	channel,
	      int	freq_flags)
{
  char	sep = ((freq_flags & IW_FREQ_FIXED) ? '=' : ':');
  char	vbuf[16];

  /* Print the frequency/channel value */
  iw_print_freq_value(vbuf, sizeof(vbuf), freq);

  /* Check if channel only */
  if(freq < KILO)
    snprintf(buffer, buflen, "Channel%c%s", sep, vbuf);
  else
    {
      /* Frequency. Check if we have a channel as well */
      if(channel >= 0)
	snprintf(buffer, buflen, "Frequency%c%s (Channel %d)",
		 sep, vbuf, channel);
      else
	snprintf(buffer, buflen, "Frequency%c%s", sep, vbuf);
    }
}

/*------------------------------------------------------------------*/
/*
 * Convert a frequency to a channel (negative -> error)
 */
int
iw_freq_to_channel(double			freq,
		   const struct iw_range *	range)
{
  double	ref_freq;
  int		k;

  /* Check if it's a frequency or not already a channel */
  if(freq < KILO)
    return(-1);

  /* We compare the frequencies as double to ignore differences
   * in encoding. Slower, but safer... */
  for(k = 0; k < range->num_frequency; k++)
    {
      ref_freq = iw_freq2float(&(range->freq[k]));
      if(freq == ref_freq)
	return(range->freq[k].i);
    }
  /* Not found */
  return(-2);
}

/*------------------------------------------------------------------*/
/*
 * Convert a channel to a frequency (negative -> error)
 * Return the channel on success
 */
int
iw_channel_to_freq(int				channel,
		   double *			pfreq,
		   const struct iw_range *	range)
{
  int		has_freq = 0;
  int		k;

  /* Check if the driver support only channels or if it has frequencies */
  for(k = 0; k < range->num_frequency; k++)
    {
      if((range->freq[k].e != 0) || (range->freq[k].m > (int) KILO))
	has_freq = 1;
    }
  if(!has_freq)
    return(-1);

  /* Find the correct frequency in the list */
  for(k = 0; k < range->num_frequency; k++)
    {
      if(range->freq[k].i == channel)
	{
	  *pfreq = iw_freq2float(&(range->freq[k]));
	  return(channel);
	}
    }
  /* Not found */
  return(-2);
}

/*********************** BITRATE SUBROUTINES ***********************/

/*------------------------------------------------------------------*/
/*
 * Output a bitrate with proper scaling
 */
void
iw_print_bitrate(char *	buffer,
		 int	buflen,
		 int	bitrate)
{
  double	rate = bitrate;
  char		scale;
  int		divisor;

  if(rate >= GIGA)
    {
      scale = 'G';
      divisor = GIGA;
    }
  else
    {
      if(rate >= MEGA)
	{
	  scale = 'M';
	  divisor = MEGA;
	}
      else
	{
	  scale = 'k';
	  divisor = KILO;
	}
    }
  snprintf(buffer, buflen, "%g %cb/s", rate / divisor, scale);
}

/************************ POWER SUBROUTINES *************************/

/*------------------------------------------------------------------*/
/*
 * Convert a value in dBm to a value in milliWatt.
 */
int
iw_dbm2mwatt(int	in)
{
#ifdef WE_NOLIBM
  /* Version without libm : slower */
  int		ip = in / 10;
  int		fp = in % 10;
  int		k;
  double	res = 1.0;

  /* Split integral and floating part to avoid accumulating rounding errors */
  for(k = 0; k < ip; k++)
    res *= 10;
  for(k = 0; k < fp; k++)
    res *= LOG10_MAGIC;
  return((int) res);
#else	/* WE_NOLIBM */
  /* Version with libm : faster */
  return((int) (floor(pow(10.0, (((double) in) / 10.0)))));
#endif	/* WE_NOLIBM */
}

/*------------------------------------------------------------------*/
/*
 * Convert a value in milliWatt to a value in dBm.
 */
int
iw_mwatt2dbm(int	in)
{
#ifdef WE_NOLIBM
  /* Version without libm : slower */
  double	fin = (double) in;
  int		res = 0;

  /* Split integral and floating part to avoid accumulating rounding errors */
  while(fin > 10.0)
    {
      res += 10;
      fin /= 10.0;
    }
  while(fin > 1.000001)	/* Eliminate rounding errors, take ceil */
    {
      res += 1;
      fin /= LOG10_MAGIC;
    }
  return(res);
#else	/* WE_NOLIBM */
  /* Version with libm : faster */
  return((int) (ceil(10.0 * log10((double) in))));
#endif	/* WE_NOLIBM */
}

/*------------------------------------------------------------------*/
/*
 * Output a txpower with proper conversion
 */
void
iw_print_txpower(char *			buffer,
		 int			buflen,
		 struct iw_param *	txpower)
{
  int		dbm;

  /* Check if disabled */
  if(txpower->disabled)
    {
      snprintf(buffer, buflen, "off");
    }
  else
    {
      /* Check for relative values */
      if(txpower->flags & IW_TXPOW_RELATIVE)
	{
	  snprintf(buffer, buflen, "%d", txpower->value);
	}
      else
	{
	  /* Convert everything to dBm */
	  if(txpower->flags & IW_TXPOW_MWATT)
	    dbm = iw_mwatt2dbm(txpower->value);
	  else
	    dbm = txpower->value;

	  /* Display */
	  snprintf(buffer, buflen, "%d dBm", dbm);
	}
    }
}

/********************** STATISTICS SUBROUTINES **********************/

/*------------------------------------------------------------------*/
/*
 * Read /proc/net/wireless to get the latest statistics
 * Note : strtok not thread safe, not used in WE-12 and later.
 */
int
iw_get_stats(int		skfd,
	     const char *	ifname,
	     iwstats *		stats,
	     const iwrange *	range,
	     int		has_range)
{
  /* Fortunately, we can always detect this condition properly */
  if((has_range) && (range->we_version_compiled > 11))
    {
      struct iwreq		wrq;
      wrq.u.data.pointer = (caddr_t) stats;
      wrq.u.data.length = sizeof(struct iw_statistics);
      wrq.u.data.flags = 1;		/* Clear updated flag */
      strncpy(wrq.ifr_name, ifname, IFNAMSIZ);
      if(iw_get_ext(skfd, ifname, SIOCGIWSTATS, &wrq) < 0)
	return(-1);

      /* Format has not changed since WE-12, no conversion */
      return(0);
    }
  else
    {
      FILE *	f = fopen(PROC_NET_WIRELESS, "r");
      char	buf[256];
      char *	bp;
      int	t;

      if(f==NULL)
	return -1;
      /* Loop on all devices */
      while(fgets(buf,255,f))
	{
	  bp=buf;
	  while(*bp&&isspace(*bp))
	    bp++;
	  /* Is it the good device ? */
	  if(strncmp(bp,ifname,strlen(ifname))==0 && bp[strlen(ifname)]==':')
	    {
	      /* Skip ethX: */
	      bp=strchr(bp,':');
	      bp++;
	      /* -- status -- */
	      bp = strtok(bp, " ");
	      sscanf(bp, "%X", &t);
	      stats->status = (unsigned short) t;
	      /* -- link quality -- */
	      bp = strtok(NULL, " ");
	      if(strchr(bp,'.') != NULL)
		stats->qual.updated |= 1;
	      sscanf(bp, "%d", &t);
	      stats->qual.qual = (unsigned char) t;
	      /* -- signal level -- */
	      bp = strtok(NULL, " ");
	      if(strchr(bp,'.') != NULL)
		stats->qual.updated |= 2;
	      sscanf(bp, "%d", &t);
	      stats->qual.level = (unsigned char) t;
	      /* -- noise level -- */
	      bp = strtok(NULL, " ");
	      if(strchr(bp,'.') != NULL)
		stats->qual.updated += 4;
	      sscanf(bp, "%d", &t);
	      stats->qual.noise = (unsigned char) t;
	      /* -- discarded packets -- */
	      bp = strtok(NULL, " ");
	      sscanf(bp, "%d", &stats->discard.nwid);
	      bp = strtok(NULL, " ");
	      sscanf(bp, "%d", &stats->discard.code);
	      bp = strtok(NULL, " ");
	      sscanf(bp, "%d", &stats->discard.misc);
	      fclose(f);
	      /* No conversion needed */
	      return 0;
	    }
	}
      fclose(f);
      return -1;
    }
}

/*------------------------------------------------------------------*/
/*
 * Output the link statistics, taking care of formating
 */
void
iw_print_stats(char *		buffer,
	       int		buflen,
	       const iwqual *	qual,
	       const iwrange *	range,
	       int		has_range)
{
  int		len;

  /* People are very often confused by the 8 bit arithmetic happening
   * here.
   * All the values here are encoded in a 8 bit integer. 8 bit integers
   * are either unsigned [0 ; 255], signed [-128 ; +127] or
   * negative [-255 ; 0].
   * Further, on 8 bits, 0x100 == 256 == 0.
   *
   * Relative/percent values are always encoded unsigned, between 0 and 255.
   * Absolute/dBm values are always encoded between -192 and 63.
   * (Note that up to version 28 of Wireless Tools, dBm used to be
   *  encoded always negative, between -256 and -1).
   *
   * How do we separate relative from absolute values ?
   * The old way is to use the range to do that. As of WE-19, we have
   * an explicit IW_QUAL_DBM flag in updated...
   * The range allow to specify the real min/max of the value. As the
   * range struct only specify one bound of the value, we assume that
   * the other bound is 0 (zero).
   * For relative values, range is [0 ; range->max].
   * For absolute values, range is [range->max ; 63].
   *
   * Let's take two example :
   * 1) value is 75%. qual->value = 75 ; range->max_qual.value = 100
   * 2) value is -54dBm. noise floor of the radio is -104dBm.
   *    qual->value = -54 = 202 ; range->max_qual.value = -104 = 152
   *
   * Jean II
   */

  /* Just do it...
   * The old way to detect dBm require both the range and a non-null
   * level (which confuse the test). The new way can deal with level of 0
   * because it does an explicit test on the flag. */
  if(has_range && ((qual->level != 0)
		   || (qual->updated & (IW_QUAL_DBM | IW_QUAL_RCPI))))
    {
      /* Deal with quality : always a relative value */
      if(!(qual->updated & IW_QUAL_QUAL_INVALID))
	{
	  len = snprintf(buffer, buflen, "Quality%c%d/%d  ",
			 qual->updated & IW_QUAL_QUAL_UPDATED ? '=' : ':',
			 qual->qual, range->max_qual.qual);
	  buffer += len;
	  buflen -= len;
	}

      /* Check if the statistics are in RCPI (IEEE 802.11k) */
      if(qual->updated & IW_QUAL_RCPI)
	{
	  /* Deal with signal level in RCPI */
	  /* RCPI = int{(Power in dBm +110)*2} for 0dbm > Power > -110dBm */
	  if(!(qual->updated & IW_QUAL_LEVEL_INVALID))
	    {
	      double	rcpilevel = (qual->level / 2.0) - 110.0;
	      len = snprintf(buffer, buflen, "Signal level%c%g dBm  ",
			     qual->updated & IW_QUAL_LEVEL_UPDATED ? '=' : ':',
			     rcpilevel);
	      buffer += len;
	      buflen -= len;
	    }

	  /* Deal with noise level in dBm (absolute power measurement) */
	  if(!(qual->updated & IW_QUAL_NOISE_INVALID))
	    {
	      double	rcpinoise = (qual->noise / 2.0) - 110.0;
	      len = snprintf(buffer, buflen, "Noise level%c%g dBm",
			     qual->updated & IW_QUAL_NOISE_UPDATED ? '=' : ':',
			     rcpinoise);
	    }
	}
      else
	{
	  /* Check if the statistics are in dBm */
	  if((qual->updated & IW_QUAL_DBM)
	     || (qual->level > range->max_qual.level))
	    {
	      /* Deal with signal level in dBm  (absolute power measurement) */
	      if(!(qual->updated & IW_QUAL_LEVEL_INVALID))
		{
		  int	dblevel = qual->level;
		  /* Implement a range for dBm [-192; 63] */
		  if(qual->level >= 64)
		    dblevel -= 0x100;
		  len = snprintf(buffer, buflen, "Signal level%c%d dBm  ",
				 qual->updated & IW_QUAL_LEVEL_UPDATED ? '=' : ':',
				 dblevel);
		  buffer += len;
		  buflen -= len;
		}

	      /* Deal with noise level in dBm (absolute power measurement) */
	      if(!(qual->updated & IW_QUAL_NOISE_INVALID))
		{
		  int	dbnoise = qual->noise;
		  /* Implement a range for dBm [-192; 63] */
		  if(qual->noise >= 64)
		    dbnoise -= 0x100;
		  len = snprintf(buffer, buflen, "Noise level%c%d dBm",
				 qual->updated & IW_QUAL_NOISE_UPDATED ? '=' : ':',
				 dbnoise);
		}
	    }
	  else
	    {
	      /* Deal with signal level as relative value (0 -> max) */
	      if(!(qual->updated & IW_QUAL_LEVEL_INVALID))
		{
		  len = snprintf(buffer, buflen, "Signal level%c%d/%d  ",
				 qual->updated & IW_QUAL_LEVEL_UPDATED ? '=' : ':',
				 qual->level, range->max_qual.level);
		  buffer += len;
		  buflen -= len;
		}

	      /* Deal with noise level as relative value (0 -> max) */
	      if(!(qual->updated & IW_QUAL_NOISE_INVALID))
		{
		  len = snprintf(buffer, buflen, "Noise level%c%d/%d",
				 qual->updated & IW_QUAL_NOISE_UPDATED ? '=' : ':',
				 qual->noise, range->max_qual.noise);
		}
	    }
	}
    }
  else
    {
      /* We can't read the range, so we don't know... */
      snprintf(buffer, buflen,
	       "Quality:%d  Signal level:%d  Noise level:%d",
	       qual->qual, qual->level, qual->noise);
    }
}

/*********************** ENCODING SUBROUTINES ***********************/

/*------------------------------------------------------------------*/
/*
 * Output the encoding key, with a nice formating
 */
void
iw_print_key(char *			buffer,
	     int			buflen,
	     const unsigned char *	key,		/* Must be unsigned */
	     int			key_size,
	     int			key_flags)
{
  int	i;

  /* Check buffer size -> 1 bytes => 2 digits + 1/2 separator */
  if((key_size * 3) > buflen)
    {
      snprintf(buffer, buflen, "<too big>");
      return;
    }

  /* Is the key present ??? */
  if(key_flags & IW_ENCODE_NOKEY)
    {
      /* Nope : print on or dummy */
      if(key_size <= 0)
	strcpy(buffer, "on");			/* Size checked */
      else
	{
	  strcpy(buffer, "**");			/* Size checked */
	  buffer +=2;
	  for(i = 1; i < key_size; i++)
	    {
	      if((i & 0x1) == 0)
		strcpy(buffer++, "-");		/* Size checked */
	      strcpy(buffer, "**");		/* Size checked */
	      buffer +=2;
	    }
	}
    }
  else
    {
      /* Yes : print the key */
      sprintf(buffer, "%.2X", key[0]);		/* Size checked */
      buffer +=2;
      for(i = 1; i < key_size; i++)
	{
	  if((i & 0x1) == 0)
	    strcpy(buffer++, "-");		/* Size checked */
	  sprintf(buffer, "%.2X", key[i]);	/* Size checked */
	  buffer +=2;
	}
    }
}

/*------------------------------------------------------------------*/
/*
 * Convert a passphrase into a key
 * ### NOT IMPLEMENTED ###
 * Return size of the key, or 0 (no key) or -1 (error)
 */
static int
iw_pass_key(const char *	input,
	    unsigned char *	key)
{
  input = input; key = key;
  fprintf(stderr, "Error: Passphrase not implemented\n");
  return(-1);
}

/*------------------------------------------------------------------*/
/*
 * Parse a key from the command line.
 * Return size of the key, or 0 (no key) or -1 (error)
 * If the key is too long, it's simply truncated...
 */
int
iw_in_key(const char *		input,
	  unsigned char *	key)
{
  int		keylen = 0;

  /* Check the type of key */
  if(!strncmp(input, "s:", 2))
    {
      /* First case : as an ASCII string (Lucent/Agere cards) */
      keylen = strlen(input + 2);		/* skip "s:" */
      if(keylen > IW_ENCODING_TOKEN_MAX)
	keylen = IW_ENCODING_TOKEN_MAX;
      memcpy(key, input + 2, keylen);
    }
  else
    if(!strncmp(input, "p:", 2))
      {
	/* Second case : as a passphrase (PrismII cards) */
	return(iw_pass_key(input + 2, key));		/* skip "p:" */
      }
    else
      {
	const char *	p;
	int		dlen;	/* Digits sequence length */
	unsigned char	out[IW_ENCODING_TOKEN_MAX];

	/* Third case : as hexadecimal digits */
	p = input;
	dlen = -1;

	/* Loop until we run out of chars in input or overflow the output */
	while(*p != '\0')
	  {
	    int	temph;
	    int	templ;
	    int	count;
	    /* No more chars in this sequence */
	    if(dlen <= 0)
	      {
		/* Skip separator */
		if(dlen == 0)
		  p++;
		/* Calculate num of char to next separator */
		dlen = strcspn(p, "-:;.,");
	      }
	    /* Get each char separatly (and not by two) so that we don't
	     * get confused by 'enc' (=> '0E'+'0C') and similar */
	    count = sscanf(p, "%1X%1X", &temph, &templ);
	    if(count < 1)
	      return(-1);		/* Error -> non-hex char */
	    /* Fixup odd strings such as '123' is '01'+'23' and not '12'+'03'*/
	    if(dlen % 2)
	      count = 1;
	    /* Put back two chars as one byte and output */
	    if(count == 2)
	      templ |= temph << 4;
	    else
	      templ = temph;
	    out[keylen++] = (unsigned char) (templ & 0xFF);
	    /* Check overflow in output */
	    if(keylen >= IW_ENCODING_TOKEN_MAX)
	      break;
	    /* Move on to next chars */
	    p += count;
	    dlen -= count;
	  }
	/* We use a temporary output buffer 'out' so that if there is
	 * an error, we don't overwrite the original key buffer.
	 * Because of the way iwconfig loop on multiple key/enc arguments
	 * until it finds an error in here, this is necessary to avoid
	 * silently corrupting the encryption key... */
	memcpy(key, out, keylen);
      }

#ifdef DEBUG
  {
    char buf[IW_ENCODING_TOKEN_MAX * 3];
    iw_print_key(buf, sizeof(buf), key, keylen, 0);
    printf("Got key : %d [%s]\n", keylen, buf);
  }
#endif

  return(keylen);
}

/*------------------------------------------------------------------*/
/*
 * Parse a key from the command line.
 * Return size of the key, or 0 (no key) or -1 (error)
 */
int
iw_in_key_full(int		skfd,
	       const char *	ifname,
	       const char *	input,
	       unsigned char *	key,
	       __u16 *		flags)
{
  int		keylen = 0;
  char *	p;

  if(!strncmp(input, "l:", 2))
    {
      struct iw_range	range;

      /* Extra case : as a login (user:passwd - Cisco LEAP) */
      keylen = strlen(input + 2) + 1;		/* skip "l:", add '\0' */
      /* Most user/password is 8 char, so 18 char total, < 32 */
      if(keylen > IW_ENCODING_TOKEN_MAX)
	keylen = IW_ENCODING_TOKEN_MAX;
      memcpy(key, input + 2, keylen);

      /* Separate the two strings */
      p = strchr((char *) key, ':');
      if(p == NULL)
	{
	  fprintf(stderr, "Error: Invalid login format\n");
	  return(-1);
	}
      *p = '\0';

      /* Extract range info */
      if(iw_get_range_info(skfd, ifname, &range) < 0)
	/* Hum... Maybe we should return an error ??? */
	memset(&range, 0, sizeof(range));

      if(range.we_version_compiled > 15)
	{

	  printf("flags = %X, index = %X\n",
		 *flags, range.encoding_login_index);
	  if((*flags & IW_ENCODE_INDEX) == 0)
	    {
	      /* Extract range info */
	      if(iw_get_range_info(skfd, ifname, &range) < 0)
		memset(&range, 0, sizeof(range));
	      printf("flags = %X, index = %X\n", *flags, range.encoding_login_index);
	      /* Set the index the driver expects */
	      *flags |= range.encoding_login_index & IW_ENCODE_INDEX;
	    }
	  printf("flags = %X, index = %X\n", *flags, range.encoding_login_index);
	}
    }
  else
    /* Simpler routine above */
    keylen = iw_in_key(input, key);

  return(keylen);
}

/******************* POWER MANAGEMENT SUBROUTINES *******************/

/*------------------------------------------------------------------*/
/*
 * Output a power management value with all attributes...
 */
void
iw_print_pm_value(char *	buffer,
		  int		buflen,
		  int		value,
		  int		flags,
		  int		we_version)
{
  /* Check size */
  if(buflen < 25)
    {
      snprintf(buffer, buflen, "<too big>");
      return;
    }
  buflen -= 25;

  /* Modifiers */
  if(flags & IW_POWER_MIN)
    {
      strcpy(buffer, " min");				/* Size checked */
      buffer += 4;
    }
  if(flags & IW_POWER_MAX)
    {
      strcpy(buffer, " max");				/* Size checked */
      buffer += 4;
    }

  /* Type */
  if(flags & IW_POWER_TIMEOUT)
    {
      strcpy(buffer, " timeout:");			/* Size checked */
      buffer += 9;
    }
  else
    {
      if(flags & IW_POWER_SAVING)
	{
	  strcpy(buffer, " saving:");			/* Size checked */
	  buffer += 8;
	}
      else
	{
	  strcpy(buffer, " period:");			/* Size checked */
	  buffer += 8;
	}
    }

  /* Display value without units */
  if(flags & IW_POWER_RELATIVE)
    {
      if(we_version < 21)
	value /= MEGA;
      snprintf(buffer, buflen, "%d", value);
    }
  else
    {
      /* Display value with units */
      if(value >= (int) MEGA)
	snprintf(buffer, buflen, "%gs", ((double) value) / MEGA);
      else
	if(value >= (int) KILO)
	  snprintf(buffer, buflen, "%gms", ((double) value) / KILO);
	else
	  snprintf(buffer, buflen, "%dus", value);
    }
}

/*------------------------------------------------------------------*/
/*
 * Output a power management mode
 */
void
iw_print_pm_mode(char *	buffer,
		 int	buflen,
		 int	flags)
{
  /* Check size */
  if(buflen < 28)
    {
      snprintf(buffer, buflen, "<too big>");
      return;
    }

  /* Print the proper mode... */
  switch(flags & IW_POWER_MODE)
    {
    case IW_POWER_UNICAST_R:
      strcpy(buffer, "mode:Unicast only received");	/* Size checked */
      break;
    case IW_POWER_MULTICAST_R:
      strcpy(buffer, "mode:Multicast only received");	/* Size checked */
      break;
    case IW_POWER_ALL_R:
      strcpy(buffer, "mode:All packets received");	/* Size checked */
      break;
    case IW_POWER_FORCE_S:
      strcpy(buffer, "mode:Force sending");		/* Size checked */
      break;
    case IW_POWER_REPEATER:
      strcpy(buffer, "mode:Repeat multicasts");		/* Size checked */
      break;
    default:
      strcpy(buffer, "");				/* Size checked */
      break;
    }
}

/***************** RETRY LIMIT/LIFETIME SUBROUTINES *****************/

/*------------------------------------------------------------------*/
/*
 * Output a retry value with all attributes...
 */
void
iw_print_retry_value(char *	buffer,
		     int	buflen,
		     int	value,
		     int	flags,
		     int	we_version)
{
  /* Check buffer size */
  if(buflen < 20)
    {
      snprintf(buffer, buflen, "<too big>");
      return;
    }
  buflen -= 20;

  /* Modifiers */
  if(flags & IW_RETRY_MIN)
    {
      strcpy(buffer, " min");				/* Size checked */
      buffer += 4;
    }
  if(flags & IW_RETRY_MAX)
    {
      strcpy(buffer, " max");				/* Size checked */
      buffer += 4;
    }
  if(flags & IW_RETRY_SHORT)
    {
      strcpy(buffer, " short");				/* Size checked */
      buffer += 6;
    }
  if(flags & IW_RETRY_LONG)
    {
      strcpy(buffer, "  long");				/* Size checked */
      buffer += 6;
    }

  /* Type lifetime of limit */
  if(flags & IW_RETRY_LIFETIME)
    {
      strcpy(buffer, " lifetime:");			/* Size checked */
      buffer += 10;

      /* Display value without units */
      if(flags & IW_RETRY_RELATIVE)
	{
	  if(we_version < 21)
	    value /= MEGA;
	  snprintf(buffer, buflen, "%d", value);
	}
      else
	{
	  /* Display value with units */
	  if(value >= (int) MEGA)
	    snprintf(buffer, buflen, "%gs", ((double) value) / MEGA);
	  else
	    if(value >= (int) KILO)
	      snprintf(buffer, buflen, "%gms", ((double) value) / KILO);
	    else
	      snprintf(buffer, buflen, "%dus", value);
	}
    }
  else
    snprintf(buffer, buflen, " limit:%d", value);
}

/************************* TIME SUBROUTINES *************************/

/*------------------------------------------------------------------*/
/*
 * Print timestamps
 * Inspired from irdadump...
 */
void
iw_print_timeval(char *				buffer,
		 int				buflen,
		 const struct timeval *		timev,
		 const struct timezone *	tz)
{
        int s;

	s = (timev->tv_sec - tz->tz_minuteswest * 60) % 86400;
	snprintf(buffer, buflen, "%02d:%02d:%02d.%06u", 
		s / 3600, (s % 3600) / 60, 
		s % 60, (u_int32_t) timev->tv_usec);
}

/*********************** ADDRESS SUBROUTINES ************************/
/*
 * This section is mostly a cut & past from net-tools-1.2.0
 * (Well... This has evolved over the years)
 * manage address display and input...
 */

/*------------------------------------------------------------------*/
/*
 * Check if interface support the right MAC address type...
 */
int
iw_check_mac_addr_type(int		skfd,
		       const char *	ifname)
{
  struct ifreq		ifr;

  /* Get the type of hardware address */
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  if((ioctl(skfd, SIOCGIFHWADDR, &ifr) < 0) ||
     ((ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
      && (ifr.ifr_hwaddr.sa_family != ARPHRD_IEEE80211)))
    {
      /* Deep trouble... */
      fprintf(stderr, "Interface %s doesn't support MAC addresses\n",
	     ifname);
      return(-1);
    }

#ifdef DEBUG
  {
    char buf[20];
    printf("Hardware : %d - %s\n", ifr.ifr_hwaddr.sa_family,
	   iw_saether_ntop(&ifr.ifr_hwaddr, buf));
  }
#endif

  return(0);
}


/*------------------------------------------------------------------*/
/*
 * Check if interface support the right interface address type...
 */
int
iw_check_if_addr_type(int		skfd,
		      const char *	ifname)
{
  struct ifreq		ifr;

  /* Get the type of interface address */
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  if((ioctl(skfd, SIOCGIFADDR, &ifr) < 0) ||
     (ifr.ifr_addr.sa_family !=  AF_INET))
    {
      /* Deep trouble... */
      fprintf(stderr, "Interface %s doesn't support IP addresses\n", ifname);
      return(-1);
    }

#ifdef DEBUG
  printf("Interface : %d - 0x%lX\n", ifr.ifr_addr.sa_family,
	 *((unsigned long *) ifr.ifr_addr.sa_data));
#endif

  return(0);
}

#if 0
/*------------------------------------------------------------------*/
/*
 * Check if interface support the right address types...
 */
int
iw_check_addr_type(int		skfd,
		   char *	ifname)
{
  /* Check the interface address type */
  if(iw_check_if_addr_type(skfd, ifname) < 0)
    return(-1);

  /* Check the interface address type */
  if(iw_check_mac_addr_type(skfd, ifname) < 0)
    return(-1);

  return(0);
}
#endif

#if 0
/*------------------------------------------------------------------*/
/*
 * Ask the kernel for the MAC address of an interface.
 */
int
iw_get_mac_addr(int			skfd,
		const char *		ifname,
		struct ether_addr *	eth,
		unsigned short *	ptype)
{
  struct ifreq	ifr;
  int		ret;

  /* Prepare request */
  bzero(&ifr, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

  /* Do it */
  ret = ioctl(skfd, SIOCGIFHWADDR, &ifr);

  memcpy(eth->ether_addr_octet, ifr.ifr_hwaddr.sa_data, 6); 
  *ptype = ifr.ifr_hwaddr.sa_family;
  return(ret);
}
#endif

/*------------------------------------------------------------------*/
/*
 * Display an arbitrary length MAC address in readable format.
 */
char *
iw_mac_ntop(const unsigned char *	mac,
	    int				maclen,
	    char *			buf,
	    int				buflen)
{
  int	i;

  /* Overflow check (don't forget '\0') */
  if(buflen < (maclen * 3 - 1 + 1))
    return(NULL);

  /* First byte */
  sprintf(buf, "%02X", mac[0]);

  /* Other bytes */
  for(i = 1; i < maclen; i++)
    sprintf(buf + (i * 3) - 1, ":%02X", mac[i]);
  return(buf);
}

/*------------------------------------------------------------------*/
/*
 * Display an Ethernet address in readable format.
 */
void
iw_ether_ntop(const struct ether_addr *	eth,
	      char *			buf)
{
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
	  eth->ether_addr_octet[0], eth->ether_addr_octet[1],
	  eth->ether_addr_octet[2], eth->ether_addr_octet[3],
	  eth->ether_addr_octet[4], eth->ether_addr_octet[5]);
}

/*------------------------------------------------------------------*/
/*
 * Display an Wireless Access Point Socket Address in readable format.
 * Note : 0x44 is an accident of history, that's what the Orinoco/PrismII
 * chipset report, and the driver doesn't filter it.
 */
char *
iw_sawap_ntop(const struct sockaddr *	sap,
	      char *			buf)
{
  const struct ether_addr ether_zero = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
  const struct ether_addr ether_bcast = {{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }};
  const struct ether_addr ether_hack = {{ 0x44, 0x44, 0x44, 0x44, 0x44, 0x44 }};
  const struct ether_addr * ether_wap = (const struct ether_addr *) sap->sa_data;

  if(!iw_ether_cmp(ether_wap, &ether_zero))
    sprintf(buf, "Not-Associated");
  else
    if(!iw_ether_cmp(ether_wap, &ether_bcast))
      sprintf(buf, "Invalid");
    else
      if(!iw_ether_cmp(ether_wap, &ether_hack))
	sprintf(buf, "None");
      else
	iw_ether_ntop(ether_wap, buf);
  return(buf);
}

/*------------------------------------------------------------------*/
/*
 * Input an arbitrary length MAC address and convert to binary.
 * Return address size.
 */
int
iw_mac_aton(const char *	orig,
	    unsigned char *	mac,
	    int			macmax)
{
  const char *	p = orig;
  int		maclen = 0;

  /* Loop on all bytes of the string */
  while(*p != '\0')
    {
      int	temph;
      int	templ;
      int	count;
      /* Extract one byte as two chars */
      count = sscanf(p, "%1X%1X", &temph, &templ);
      if(count != 2)
	break;			/* Error -> non-hex chars */
      /* Output two chars as one byte */
      templ |= temph << 4;
      mac[maclen++] = (unsigned char) (templ & 0xFF);

      /* Check end of string */
      p += 2;
      if(*p == '\0')
	{
#ifdef DEBUG
	  char buf[20];
	  iw_ether_ntop((const struct ether_addr *) mac, buf);
	  fprintf(stderr, "iw_mac_aton(%s): %s\n", orig, buf);
#endif
	  return(maclen);		/* Normal exit */
	}

      /* Check overflow */
      if(maclen >= macmax)
	{
#ifdef DEBUG
	  fprintf(stderr, "iw_mac_aton(%s): trailing junk!\n", orig);
#endif
	  errno = E2BIG;
	  return(0);			/* Error -> overflow */
	}

      /* Check separator */
      if(*p != ':')
	break;
      p++;
    }

  /* Error... */
#ifdef DEBUG
  fprintf(stderr, "iw_mac_aton(%s): invalid ether address!\n", orig);
#endif
  errno = EINVAL;
  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Input an Ethernet address and convert to binary.
 */
int
iw_ether_aton(const char *orig, struct ether_addr *eth)
{
  int	maclen;
  maclen = iw_mac_aton(orig, (unsigned char *) eth, ETH_ALEN);
  if((maclen > 0) && (maclen < ETH_ALEN))
    {
      errno = EINVAL;
      maclen = 0;
    }
  return(maclen);
}

/*------------------------------------------------------------------*/
/*
 * Input an Internet address and convert to binary.
 */
int
iw_in_inet(char *name, struct sockaddr *sap)
{
  struct hostent *hp;
  struct netent *np;
  struct sockaddr_in *sain = (struct sockaddr_in *) sap;

  /* Grmpf. -FvK */
  sain->sin_family = AF_INET;
  sain->sin_port = 0;

  /* Default is special, meaning 0.0.0.0. */
  if (!strcmp(name, "default")) {
	sain->sin_addr.s_addr = INADDR_ANY;
	return(1);
  }

  /* Try the NETWORKS database to see if this is a known network. */
  if ((np = getnetbyname(name)) != (struct netent *)NULL) {
	sain->sin_addr.s_addr = htonl(np->n_net);
	strcpy(name, np->n_name);
	return(1);
  }

  /* Always use the resolver (DNS name + IP addresses) */
  if ((hp = gethostbyname(name)) == (struct hostent *)NULL) {
	errno = h_errno;
	return(-1);
  }
  memcpy((char *) &sain->sin_addr, (char *) hp->h_addr_list[0], hp->h_length);
  strcpy(name, hp->h_name);
  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Input an address and convert to binary.
 */
int
iw_in_addr(int		skfd,
	   const char *	ifname,
	   char *	bufp,
	   struct sockaddr *sap)
{
  /* Check if it is a hardware or IP address */
  if(strchr(bufp, ':') == NULL)
    {
      struct sockaddr	if_address;
      struct arpreq	arp_query;

      /* Check if we have valid interface address type */
      if(iw_check_if_addr_type(skfd, ifname) < 0)
	{
	  fprintf(stderr, "%-8.16s  Interface doesn't support IP addresses\n", ifname);
	  return(-1);
	}

      /* Read interface address */
      if(iw_in_inet(bufp, &if_address) < 0)
	{
	  fprintf(stderr, "Invalid interface address %s\n", bufp);
	  return(-1);
	}

      /* Translate IP addresses to MAC addresses */
      memcpy((char *) &(arp_query.arp_pa),
	     (char *) &if_address,
	     sizeof(struct sockaddr));
      arp_query.arp_ha.sa_family = 0;
      arp_query.arp_flags = 0;
      /* The following restrict the search to the interface only */
      /* For old kernels which complain, just comment it... */
      strncpy(arp_query.arp_dev, ifname, IFNAMSIZ);
      if((ioctl(skfd, SIOCGARP, &arp_query) < 0) ||
	 !(arp_query.arp_flags & ATF_COM))
	{
	  fprintf(stderr, "Arp failed for %s on %s... (%d)\nTry to ping the address before setting it.\n",
		  bufp, ifname, errno);
	  return(-1);
	}

      /* Store new MAC address */
      memcpy((char *) sap,
	     (char *) &(arp_query.arp_ha),
	     sizeof(struct sockaddr));

#ifdef DEBUG
      {
	char buf[20];
	printf("IP Address %s => Hw Address = %s\n",
	       bufp, iw_saether_ntop(sap, buf));
      }
#endif
    }
  else	/* If it's an hardware address */
    {
      /* Check if we have valid mac address type */
      if(iw_check_mac_addr_type(skfd, ifname) < 0)
	{
	  fprintf(stderr, "%-8.16s  Interface doesn't support MAC addresses\n", ifname);
	  return(-1);
	}

      /* Get the hardware address */
      if(iw_saether_aton(bufp, sap) == 0)
	{
	  fprintf(stderr, "Invalid hardware address %s\n", bufp);
	  return(-1);
	}
    }

#ifdef DEBUG
  {
    char buf[20];
    printf("Hw Address = %s\n", iw_saether_ntop(sap, buf));
  }
#endif

  return(0);
}

/************************* MISC SUBROUTINES **************************/

/* Size (in bytes) of various events */
static const int priv_type_size[] = {
	0,				/* IW_PRIV_TYPE_NONE */
	1,				/* IW_PRIV_TYPE_BYTE */
	1,				/* IW_PRIV_TYPE_CHAR */
	0,				/* Not defined */
	sizeof(__u32),			/* IW_PRIV_TYPE_INT */
	sizeof(struct iw_freq),		/* IW_PRIV_TYPE_FLOAT */
	sizeof(struct sockaddr),	/* IW_PRIV_TYPE_ADDR */
	0,				/* Not defined */
};

/*------------------------------------------------------------------*/
/*
 * Max size in bytes of an private argument.
 */
int
iw_get_priv_size(int	args)
{
  int	num = args & IW_PRIV_SIZE_MASK;
  int	type = (args & IW_PRIV_TYPE_MASK) >> 12;

  return(num * priv_type_size[type]);
}

/************************ EVENT SUBROUTINES ************************/
/*
 * The Wireless Extension API 14 and greater define Wireless Events,
 * that are used for various events and scanning.
 * Those functions help the decoding of events, so are needed only in
 * this case.
 */

/* -------------------------- CONSTANTS -------------------------- */

/* Type of headers we know about (basically union iwreq_data) */
#define IW_HEADER_TYPE_NULL	0	/* Not available */
#define IW_HEADER_TYPE_CHAR	2	/* char [IFNAMSIZ] */
#define IW_HEADER_TYPE_UINT	4	/* __u32 */
#define IW_HEADER_TYPE_FREQ	5	/* struct iw_freq */
#define IW_HEADER_TYPE_ADDR	6	/* struct sockaddr */
#define IW_HEADER_TYPE_POINT	8	/* struct iw_point */
#define IW_HEADER_TYPE_PARAM	9	/* struct iw_param */
#define IW_HEADER_TYPE_QUAL	10	/* struct iw_quality */

/* Handling flags */
/* Most are not implemented. I just use them as a reminder of some
 * cool features we might need one day ;-) */
#define IW_DESCR_FLAG_NONE	0x0000	/* Obvious */
/* Wrapper level flags */
#define IW_DESCR_FLAG_DUMP	0x0001	/* Not part of the dump command */
#define IW_DESCR_FLAG_EVENT	0x0002	/* Generate an event on SET */
#define IW_DESCR_FLAG_RESTRICT	0x0004	/* GET : request is ROOT only */
				/* SET : Omit payload from generated iwevent */
#define IW_DESCR_FLAG_NOMAX	0x0008	/* GET : no limit on request size */
/* Driver level flags */
#define IW_DESCR_FLAG_WAIT	0x0100	/* Wait for driver event */

/* ---------------------------- TYPES ---------------------------- */

/*
 * Describe how a standard IOCTL looks like.
 */
struct iw_ioctl_description
{
	__u8	header_type;		/* NULL, iw_point or other */
	__u8	token_type;		/* Future */
	__u16	token_size;		/* Granularity of payload */
	__u16	min_tokens;		/* Min acceptable token number */
	__u16	max_tokens;		/* Max acceptable token number */
	__u32	flags;			/* Special handling of the request */
};

/* -------------------------- VARIABLES -------------------------- */

/*
 * Meta-data about all the standard Wireless Extension request we
 * know about.
 */
static const struct iw_ioctl_description standard_ioctl_descr[] = {
	[SIOCSIWCOMMIT	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[SIOCGIWNAME	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_CHAR,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[SIOCSIWNWID	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
		.flags		= IW_DESCR_FLAG_EVENT,
	},
	[SIOCGIWNWID	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[SIOCSIWFREQ	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_FREQ,
		.flags		= IW_DESCR_FLAG_EVENT,
	},
	[SIOCGIWFREQ	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_FREQ,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[SIOCSIWMODE	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_UINT,
		.flags		= IW_DESCR_FLAG_EVENT,
	},
	[SIOCGIWMODE	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_UINT,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[SIOCSIWSENS	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCGIWSENS	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCSIWRANGE	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[SIOCGIWRANGE	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= sizeof(struct iw_range),
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[SIOCSIWPRIV	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[SIOCGIWPRIV	- SIOCIWFIRST] = { /* (handled directly by us) */
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[SIOCSIWSTATS	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_NULL,
	},
	[SIOCGIWSTATS	- SIOCIWFIRST] = { /* (handled directly by us) */
		.header_type	= IW_HEADER_TYPE_NULL,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[SIOCSIWSPY	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct sockaddr),
		.max_tokens	= IW_MAX_SPY,
	},
	[SIOCGIWSPY	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct sockaddr) +
				  sizeof(struct iw_quality),
		.max_tokens	= IW_MAX_SPY,
	},
	[SIOCSIWTHRSPY	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct iw_thrspy),
		.min_tokens	= 1,
		.max_tokens	= 1,
	},
	[SIOCGIWTHRSPY	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct iw_thrspy),
		.min_tokens	= 1,
		.max_tokens	= 1,
	},
	[SIOCSIWAP	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_ADDR,
	},
	[SIOCGIWAP	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_ADDR,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[SIOCSIWMLME	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= sizeof(struct iw_mlme),
		.max_tokens	= sizeof(struct iw_mlme),
	},
	[SIOCGIWAPLIST	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= sizeof(struct sockaddr) +
				  sizeof(struct iw_quality),
		.max_tokens	= IW_MAX_AP,
		.flags		= IW_DESCR_FLAG_NOMAX,
	},
	[SIOCSIWSCAN	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= 0,
		.max_tokens	= sizeof(struct iw_scan_req),
	},
	[SIOCGIWSCAN	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_SCAN_MAX_DATA,
		.flags		= IW_DESCR_FLAG_NOMAX,
	},
	[SIOCSIWESSID	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_ESSID_MAX_SIZE + 1,
		.flags		= IW_DESCR_FLAG_EVENT,
	},
	[SIOCGIWESSID	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_ESSID_MAX_SIZE + 1,
		.flags		= IW_DESCR_FLAG_DUMP,
	},
	[SIOCSIWNICKN	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_ESSID_MAX_SIZE + 1,
	},
	[SIOCGIWNICKN	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_ESSID_MAX_SIZE + 1,
	},
	[SIOCSIWRATE	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCGIWRATE	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCSIWRTS	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCGIWRTS	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCSIWFRAG	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCGIWFRAG	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCSIWTXPOW	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCGIWTXPOW	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCSIWRETRY	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCGIWRETRY	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCSIWENCODE	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_ENCODING_TOKEN_MAX,
		.flags		= IW_DESCR_FLAG_EVENT | IW_DESCR_FLAG_RESTRICT,
	},
	[SIOCGIWENCODE	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_ENCODING_TOKEN_MAX,
		.flags		= IW_DESCR_FLAG_DUMP | IW_DESCR_FLAG_RESTRICT,
	},
	[SIOCSIWPOWER	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCGIWPOWER	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCSIWMODUL	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCGIWMODUL	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCSIWGENIE	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_GENERIC_IE_MAX,
	},
	[SIOCGIWGENIE	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_GENERIC_IE_MAX,
	},
	[SIOCSIWAUTH	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCGIWAUTH	- SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_PARAM,
	},
	[SIOCSIWENCODEEXT - SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= sizeof(struct iw_encode_ext),
		.max_tokens	= sizeof(struct iw_encode_ext) +
				  IW_ENCODING_TOKEN_MAX,
	},
	[SIOCGIWENCODEEXT - SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= sizeof(struct iw_encode_ext),
		.max_tokens	= sizeof(struct iw_encode_ext) +
				  IW_ENCODING_TOKEN_MAX,
	},
	[SIOCSIWPMKSA - SIOCIWFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.min_tokens	= sizeof(struct iw_pmksa),
		.max_tokens	= sizeof(struct iw_pmksa),
	},
};
static const unsigned int standard_ioctl_num = (sizeof(standard_ioctl_descr) /
						sizeof(struct iw_ioctl_description));

/*
 * Meta-data about all the additional standard Wireless Extension events
 * we know about.
 */
static const struct iw_ioctl_description standard_event_descr[] = {
	[IWEVTXDROP	- IWEVFIRST] = {
		.header_type	= IW_HEADER_TYPE_ADDR,
	},
	[IWEVQUAL	- IWEVFIRST] = {
		.header_type	= IW_HEADER_TYPE_QUAL,
	},
	[IWEVCUSTOM	- IWEVFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_CUSTOM_MAX,
	},
	[IWEVREGISTERED	- IWEVFIRST] = {
		.header_type	= IW_HEADER_TYPE_ADDR,
	},
	[IWEVEXPIRED	- IWEVFIRST] = {
		.header_type	= IW_HEADER_TYPE_ADDR, 
	},
	[IWEVGENIE	- IWEVFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_GENERIC_IE_MAX,
	},
	[IWEVMICHAELMICFAILURE	- IWEVFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT, 
		.token_size	= 1,
		.max_tokens	= sizeof(struct iw_michaelmicfailure),
	},
	[IWEVASSOCREQIE	- IWEVFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_GENERIC_IE_MAX,
	},
	[IWEVASSOCRESPIE	- IWEVFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= IW_GENERIC_IE_MAX,
	},
	[IWEVPMKIDCAND	- IWEVFIRST] = {
		.header_type	= IW_HEADER_TYPE_POINT,
		.token_size	= 1,
		.max_tokens	= sizeof(struct iw_pmkid_cand),
	},
};
static const unsigned int standard_event_num = (sizeof(standard_event_descr) /
						sizeof(struct iw_ioctl_description));

/* Size (in bytes) of various events */
static const int event_type_size[] = {
	IW_EV_LCP_PK_LEN,	/* IW_HEADER_TYPE_NULL */
	0,
	IW_EV_CHAR_PK_LEN,	/* IW_HEADER_TYPE_CHAR */
	0,
	IW_EV_UINT_PK_LEN,	/* IW_HEADER_TYPE_UINT */
	IW_EV_FREQ_PK_LEN,	/* IW_HEADER_TYPE_FREQ */
	IW_EV_ADDR_PK_LEN,	/* IW_HEADER_TYPE_ADDR */
	0,
	IW_EV_POINT_PK_LEN,	/* Without variable payload */
	IW_EV_PARAM_PK_LEN,	/* IW_HEADER_TYPE_PARAM */
	IW_EV_QUAL_PK_LEN,	/* IW_HEADER_TYPE_QUAL */
};

/*------------------------------------------------------------------*/
/*
 * Initialise the struct stream_descr so that we can extract
 * individual events from the event stream.
 */
void
iw_init_event_stream(struct stream_descr *	stream,	/* Stream of events */
		     char *			data,
		     int			len)
{
  /* Cleanup */
  memset((char *) stream, '\0', sizeof(struct stream_descr));

  /* Set things up */
  stream->current = data;
  stream->end = data + len;
}

/*------------------------------------------------------------------*/
/*
 * Extract the next event from the event stream.
 */
int
iw_extract_event_stream(struct stream_descr *	stream,	/* Stream of events */
			struct iw_event *	iwe,	/* Extracted event */
			int			we_version)
{
  const struct iw_ioctl_description *	descr = NULL;
  int		event_type = 0;
  unsigned int	event_len = 1;		/* Invalid */
  char *	pointer;
  /* Don't "optimise" the following variable, it will crash */
  unsigned	cmd_index;		/* *MUST* be unsigned */

  /* Check for end of stream */
  if((stream->current + IW_EV_LCP_PK_LEN) > stream->end)
    return(0);

#ifdef DEBUG
  printf("DBG - stream->current = %p, stream->value = %p, stream->end = %p\n",
	 stream->current, stream->value, stream->end);
#endif

  /* Extract the event header (to get the event id).
   * Note : the event may be unaligned, therefore copy... */
  memcpy((char *) iwe, stream->current, IW_EV_LCP_PK_LEN);

#ifdef DEBUG
  printf("DBG - iwe->cmd = 0x%X, iwe->len = %d\n",
	 iwe->cmd, iwe->len);
#endif

  /* Check invalid events */
  if(iwe->len <= IW_EV_LCP_PK_LEN)
    return(-1);

  /* Get the type and length of that event */
  if(iwe->cmd <= SIOCIWLAST)
    {
      cmd_index = iwe->cmd - SIOCIWFIRST;
      if(cmd_index < standard_ioctl_num)
	descr = &(standard_ioctl_descr[cmd_index]);
    }
  else
    {
      cmd_index = iwe->cmd - IWEVFIRST;
      if(cmd_index < standard_event_num)
	descr = &(standard_event_descr[cmd_index]);
    }
  if(descr != NULL)
    event_type = descr->header_type;
  /* Unknown events -> event_type=0 => IW_EV_LCP_PK_LEN */
  event_len = event_type_size[event_type];
  /* Fixup for earlier version of WE */
  if((we_version <= 18) && (event_type == IW_HEADER_TYPE_POINT))
    event_len += IW_EV_POINT_OFF;

  /* Check if we know about this event */
  if(event_len <= IW_EV_LCP_PK_LEN)
    {
      /* Skip to next event */
      stream->current += iwe->len;
      return(2);
    }
  event_len -= IW_EV_LCP_PK_LEN;

  /* Set pointer on data */
  if(stream->value != NULL)
    pointer = stream->value;			/* Next value in event */
  else
    pointer = stream->current + IW_EV_LCP_PK_LEN;	/* First value in event */

#ifdef DEBUG
  printf("DBG - event_type = %d, event_len = %d, pointer = %p\n",
	 event_type, event_len, pointer);
#endif

  /* Copy the rest of the event (at least, fixed part) */
  if((pointer + event_len) > stream->end)
    {
      /* Go to next event */
      stream->current += iwe->len;
      return(-2);
    }
  /* Fixup for WE-19 and later : pointer no longer in the stream */
  /* Beware of alignement. Dest has local alignement, not packed */
  if((we_version > 18) && (event_type == IW_HEADER_TYPE_POINT))
    memcpy((char *) iwe + IW_EV_LCP_LEN + IW_EV_POINT_OFF,
	   pointer, event_len);
  else
    memcpy((char *) iwe + IW_EV_LCP_LEN, pointer, event_len);

  /* Skip event in the stream */
  pointer += event_len;

  /* Special processing for iw_point events */
  if(event_type == IW_HEADER_TYPE_POINT)
    {
      /* Check the length of the payload */
      unsigned int	extra_len = iwe->len - (event_len + IW_EV_LCP_PK_LEN);
      if(extra_len > 0)
	{
	  /* Set pointer on variable part (warning : non aligned) */
	  iwe->u.data.pointer = pointer;

	  /* Check that we have a descriptor for the command */
	  if(descr == NULL)
	    /* Can't check payload -> unsafe... */
	    iwe->u.data.pointer = NULL;	/* Discard paylod */
	  else
	    {
	      /* Those checks are actually pretty hard to trigger,
	       * because of the checks done in the kernel... */

	      unsigned int	token_len = iwe->u.data.length * descr->token_size;

	      /* Ugly fixup for alignement issues.
	       * If the kernel is 64 bits and userspace 32 bits,
	       * we have an extra 4+4 bytes.
	       * Fixing that in the kernel would break 64 bits userspace. */
	      if((token_len != extra_len) && (extra_len >= 4))
		{
		  __u16		alt_dlen = *((__u16 *) pointer);
		  unsigned int	alt_token_len = alt_dlen * descr->token_size;
		  if((alt_token_len + 8) == extra_len)
		    {
#ifdef DEBUG
		      printf("DBG - alt_token_len = %d\n", alt_token_len);
#endif
		      /* Ok, let's redo everything */
		      pointer -= event_len;
		      pointer += 4;
		      /* Dest has local alignement, not packed */
		      memcpy((char *) iwe + IW_EV_LCP_LEN + IW_EV_POINT_OFF,
			     pointer, event_len);
		      pointer += event_len + 4;
		      iwe->u.data.pointer = pointer;
		      token_len = alt_token_len;
		    }
		}

	      /* Discard bogus events which advertise more tokens than
	       * what they carry... */
	      if(token_len > extra_len)
		iwe->u.data.pointer = NULL;	/* Discard paylod */
	      /* Check that the advertised token size is not going to
	       * produce buffer overflow to our caller... */
	      if((iwe->u.data.length > descr->max_tokens)
		 && !(descr->flags & IW_DESCR_FLAG_NOMAX))
		iwe->u.data.pointer = NULL;	/* Discard paylod */
	      /* Same for underflows... */
	      if(iwe->u.data.length < descr->min_tokens)
		iwe->u.data.pointer = NULL;	/* Discard paylod */
#ifdef DEBUG
	      printf("DBG - extra_len = %d, token_len = %d, token = %d, max = %d, min = %d\n",
		     extra_len, token_len, iwe->u.data.length, descr->max_tokens, descr->min_tokens);
#endif
	    }
	}
      else
	/* No data */
	iwe->u.data.pointer = NULL;

      /* Go to next event */
      stream->current += iwe->len;
    }
  else
    {
      /* Ugly fixup for alignement issues.
       * If the kernel is 64 bits and userspace 32 bits,
       * we have an extra 4 bytes.
       * Fixing that in the kernel would break 64 bits userspace. */
      if((stream->value == NULL)
	 && ((((iwe->len - IW_EV_LCP_PK_LEN) % event_len) == 4)
	     || ((iwe->len == 12) && ((event_type == IW_HEADER_TYPE_UINT) ||
				      (event_type == IW_HEADER_TYPE_QUAL))) ))
	{
#ifdef DEBUG
	  printf("DBG - alt iwe->len = %d\n", iwe->len - 4);
#endif
	  pointer -= event_len;
	  pointer += 4;
	  /* Beware of alignement. Dest has local alignement, not packed */
	  memcpy((char *) iwe + IW_EV_LCP_LEN, pointer, event_len);
	  pointer += event_len;
	}

      /* Is there more value in the event ? */
      if((pointer + event_len) <= (stream->current + iwe->len))
	/* Go to next value */
	stream->value = pointer;
      else
	{
	  /* Go to next event */
	  stream->value = NULL;
	  stream->current += iwe->len;
	}
    }
  return(1);
}

/*********************** SCANNING SUBROUTINES ***********************/
/*
 * The Wireless Extension API 14 and greater define Wireless Scanning.
 * The normal API is complex, this is an easy API that return
 * a subset of the scanning results. This should be enough for most
 * applications that want to use Scanning.
 * If you want to have use the full/normal API, check iwlist.c...
 *
 * Precaution when using scanning :
 * The scanning operation disable normal network traffic, and therefore
 * you should not abuse of scan.
 * The scan need to check the presence of network on other frequencies.
 * While you are checking those other frequencies, you can *NOT* be on
 * your normal frequency to listen to normal traffic in the cell.
 * You need typically in the order of one second to actively probe all
 * 802.11b channels (do the maths). Some cards may do that in background,
 * to reply to scan commands faster, but they still have to do it.
 * Leaving the cell for such an extended period of time is pretty bad.
 * Any kind of streaming/low latency traffic will be impacted, and the
 * user will perceive it (easily checked with telnet). People trying to
 * send traffic to you will retry packets and waste bandwidth. Some
 * applications may be sensitive to those packet losses in weird ways,
 * and tracing those weird behavior back to scanning may take time.
 * If you are in ad-hoc mode, if two nodes scan approx at the same
 * time, they won't see each other, which may create associations issues.
 * For those reasons, the scanning activity should be limited to
 * what's really needed, and continuous scanning is a bad idea.
 * Jean II
 */

/*------------------------------------------------------------------*/
/*
 * Process/store one element from the scanning results in wireless_scan
 */
static inline struct wireless_scan *
iw_process_scanning_token(struct iw_event *		event,
			  struct wireless_scan *	wscan)
{
  struct wireless_scan *	oldwscan;

  /* Now, let's decode the event */
  switch(event->cmd)
    {
    case SIOCGIWAP:
      /* New cell description. Allocate new cell descriptor, zero it. */
      oldwscan = wscan;
      wscan = (struct wireless_scan *) malloc(sizeof(struct wireless_scan));
      if(wscan == NULL)
	return(wscan);
      /* Link at the end of the list */
      if(oldwscan != NULL)
	oldwscan->next = wscan;

      /* Reset it */
      bzero(wscan, sizeof(struct wireless_scan));

      /* Save cell identifier */
      wscan->has_ap_addr = 1;
      memcpy(&(wscan->ap_addr), &(event->u.ap_addr), sizeof (sockaddr));
      break;
    case SIOCGIWNWID:
      wscan->b.has_nwid = 1;
      memcpy(&(wscan->b.nwid), &(event->u.nwid), sizeof(iwparam));
      break;
    case SIOCGIWFREQ:
      wscan->b.has_freq = 1;
      wscan->b.freq = iw_freq2float(&(event->u.freq));
      wscan->b.freq_flags = event->u.freq.flags;
      break;
    case SIOCGIWMODE:
      wscan->b.mode = event->u.mode;
      if((wscan->b.mode < IW_NUM_OPER_MODE) && (wscan->b.mode >= 0))
	wscan->b.has_mode = 1;
      break;
    case SIOCGIWESSID:
      wscan->b.has_essid = 1;
      wscan->b.essid_on = event->u.data.flags;
      memset(wscan->b.essid, '\0', IW_ESSID_MAX_SIZE+1);
      if((event->u.essid.pointer) && (event->u.essid.length))
	memcpy(wscan->b.essid, event->u.essid.pointer, event->u.essid.length);
      break;
    case SIOCGIWENCODE:
      wscan->b.has_key = 1;
      wscan->b.key_size = event->u.data.length;
      wscan->b.key_flags = event->u.data.flags;
      if(event->u.data.pointer)
	memcpy(wscan->b.key, event->u.essid.pointer, event->u.data.length);
      else
	wscan->b.key_flags |= IW_ENCODE_NOKEY;
      break;
    case IWEVQUAL:
      /* We don't get complete stats, only qual */
      wscan->has_stats = 1;
      memcpy(&wscan->stats.qual, &event->u.qual, sizeof(struct iw_quality));
      break;
    case SIOCGIWRATE:
      /* Scan may return a list of bitrates. As we have space for only
       * a single bitrate, we only keep the largest one. */
      if((!wscan->has_maxbitrate) ||
	 (event->u.bitrate.value > wscan->maxbitrate.value))
	{
	  wscan->has_maxbitrate = 1;
	  memcpy(&(wscan->maxbitrate), &(event->u.bitrate), sizeof(iwparam));
	}
    case IWEVCUSTOM:
      /* How can we deal with those sanely ? Jean II */
    default:
      break;
   }	/* switch(event->cmd) */

  return(wscan);
}

/*------------------------------------------------------------------*/
/*
 * Initiate the scan procedure, and process results.
 * This is a non-blocking procedure and it will return each time
 * it would block, returning the amount of time the caller should wait
 * before calling again.
 * Return -1 for error, delay to wait for (in ms), or 0 for success.
 * Error code is in errno
 */
int
iw_process_scan(int			skfd,
		char *			ifname,
		int			we_version,
		wireless_scan_head *	context)
{
  struct iwreq		wrq;
  unsigned char *	buffer = NULL;		/* Results */
  int			buflen = IW_SCAN_MAX_DATA; /* Min for compat WE<17 */
  unsigned char *	newbuf;

  /* Don't waste too much time on interfaces (150 * 100 = 15s) */
  context->retry++;
  if(context->retry > 150)
    {
      errno = ETIME;
      return(-1);
    }

  /* If we have not yet initiated scanning on the interface */
  if(context->retry == 1)
    {
      /* Initiate Scan */
      wrq.u.data.pointer = NULL;		/* Later */
      wrq.u.data.flags = 0;
      wrq.u.data.length = 0;
      /* Remember that as non-root, we will get an EPERM here */
      if((iw_set_ext(skfd, ifname, SIOCSIWSCAN, &wrq) < 0)
	 && (errno != EPERM))
	return(-1);
      /* Success : now, just wait for event or results */
      return(250);	/* Wait 250 ms */
    }

 realloc:
  /* (Re)allocate the buffer - realloc(NULL, len) == malloc(len) */
  newbuf = realloc(buffer, buflen);
  if(newbuf == NULL)
    {
      /* man says : If realloc() fails the original block is left untouched */
      if(buffer)
	free(buffer);
      errno = ENOMEM;
      return(-1);
    }
  buffer = newbuf;

  /* Try to read the results */
  wrq.u.data.pointer = buffer;
  wrq.u.data.flags = 0;
  wrq.u.data.length = buflen;
  if(iw_get_ext(skfd, ifname, SIOCGIWSCAN, &wrq) < 0)
    {
      /* Check if buffer was too small (WE-17 only) */
      if((errno == E2BIG) && (we_version > 16))
	{
	  /* Some driver may return very large scan results, either
	   * because there are many cells, or because they have many
	   * large elements in cells (like IWEVCUSTOM). Most will
	   * only need the regular sized buffer. We now use a dynamic
	   * allocation of the buffer to satisfy everybody. Of course,
	   * as we don't know in advance the size of the array, we try
	   * various increasing sizes. Jean II */

	  /* Check if the driver gave us any hints. */
	  if(wrq.u.data.length > buflen)
	    buflen = wrq.u.data.length;
	  else
	    buflen *= 2;

	  /* Try again */
	  goto realloc;
	}

      /* Check if results not available yet */
      if(errno == EAGAIN)
	{
	  free(buffer);
	  /* Wait for only 100ms from now on */
	  return(100);	/* Wait 100 ms */
	}

      free(buffer);
      /* Bad error, please don't come back... */
      return(-1);
    }

  /* We have the results, process them */
  if(wrq.u.data.length)
    {
      struct iw_event		iwe;
      struct stream_descr	stream;
      struct wireless_scan *	wscan = NULL;
      int			ret;
#ifdef DEBUG
      /* Debugging code. In theory useless, because it's debugged ;-) */
      int	i;
      printf("Scan result [%02X", buffer[0]);
      for(i = 1; i < wrq.u.data.length; i++)
	printf(":%02X", buffer[i]);
      printf("]\n");
#endif

      /* Init */
      iw_init_event_stream(&stream, (char *) buffer, wrq.u.data.length);
      /* This is dangerous, we may leak user data... */
      context->result = NULL;

      /* Look every token */
      do
	{
	  /* Extract an event and print it */
	  ret = iw_extract_event_stream(&stream, &iwe, we_version);
	  if(ret > 0)
	    {
	      /* Convert to wireless_scan struct */
	      wscan = iw_process_scanning_token(&iwe, wscan);
	      /* Check problems */
	      if(wscan == NULL)
		{
		  free(buffer);
		  errno = ENOMEM;
		  return(-1);
		}
	      /* Save head of list */
	      if(context->result == NULL)
		context->result = wscan;
	    }
	}
      while(ret > 0);
    }

  /* Done with this interface - return success */
  free(buffer);
  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Perform a wireless scan on the specified interface.
 * This is a blocking procedure and it will when the scan is completed
 * or when an error occur.
 *
 * The scan results are given in a linked list of wireless_scan objects.
 * The caller *must* free the result himself (by walking the list).
 * If there is an error, -1 is returned and the error code is available
 * in errno.
 *
 * The parameter we_version can be extracted from the range structure
 * (range.we_version_compiled - see iw_get_range_info()), or using
 * iw_get_kernel_we_version(). For performance reason, you should
 * cache this parameter when possible rather than querying it every time.
 *
 * Return -1 for error and 0 for success.
 */
int
iw_scan(int			skfd,
	char *			ifname,
	int			we_version,
	wireless_scan_head *	context)
{
  int		delay;		/* in ms */

  /* Clean up context. Potential memory leak if(context.result != NULL) */
  context->result = NULL;
  context->retry = 0;

  /* Wait until we get results or error */
  while(1)
    {
      /* Try to get scan results */
      delay = iw_process_scan(skfd, ifname, we_version, context);

      /* Check termination */
      if(delay <= 0)
	break;

      /* Wait a bit */
      usleep(delay * 1000);
    }

  /* End - return -1 or 0 */
  return(delay);
}

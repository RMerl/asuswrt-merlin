/*
 *	Wireless Tools
 *
 *		Jean II - HPL 04 -> 07
 *
 * Main code for "ifrename". This is tool allows to rename network
 * interfaces based on various criteria (not only wireless).
 * You need to link this code against "iwlib.c" and "-lm".
 *
 * This file is released under the GPL license.
 *     Copyright (c) 2007 Jean Tourrilhes <jt@hpl.hp.com>
 */

/* 
 * The changelog for ifrename is in the file CHANGELOG.h ;-)
 *
 * This work is a nearly complete rewrite of 'nameif.c'.
 * Original CopyRight of version of 'nameif' I used is :
 * -------------------------------------------------------
 * Name Interfaces based on MAC address.
 * Writen 2000 by Andi Kleen.
 * Subject to the Gnu Public License, version 2.  
 * TODO: make it support token ring etc.
 * $Id: ifrename.c,v 1.2 2008-07-01 11:52:00 steven Exp $
 * -------------------------------------------------------
 *
 *	It started with a series of patches to nameif which never made
 * into the regular version, and had some architecural 'issues' with
 * those patches, which is the reason of this rewrite.
 *	Difference with standard 'nameif' :
 *	o 'nameif' has only a single selector, the interface MAC address.
 *	o Modular selector architecture, easily add new selectors.
 *	o Wide range of selector, including sysfs and sysfs symlinks...
 *	o hotplug invocation support.
 *	o module loading support.
 *	o MAC address wildcard.
 *	o Interface name wildcard ('eth*' or 'wlan*').
 *	o Non-Ethernet MAC addresses (any size, not just 48 bits)
 */

/***************************** INCLUDES *****************************/

/* This is needed to enable GNU extensions such as getline & FNM_CASEFOLD */
#ifndef _GNU_SOURCE 
#define _GNU_SOURCE
#endif

#include <getopt.h>		/* getopt_long() */
#include <linux/sockios.h>	/* SIOCSIFNAME */
#include <fnmatch.h>		/* fnmatch() */
//#include <sys/syslog.h>

#include "iwlib.h"		/* Wireless Tools library */

// This would be cool, unfortunately...
//#include <linux/ethtool.h>	/* Ethtool stuff -> struct ethtool_drvinfo */

/************************ CONSTANTS & MACROS ************************/

/* Our default configuration file */
const char DEFAULT_CONF[] =		"/etc/iftab"; 

/* Debian stuff */
const char DEBIAN_CONFIG_FILE[] =	"/etc/network/interfaces";

/* Backward compatibility */
#ifndef ifr_newname
#define ifr_newname ifr_ifru.ifru_slave
#endif

/* Types of selector we support. Must match selector_list */
const int SELECT_MAC		= 0;	/* Select by MAC address */
const int SELECT_ETHADDR	= 1;	/* Select by MAC address */
const int SELECT_ARP		= 2;	/* Select by ARP type */
const int SELECT_LINKTYPE	= 3;	/* Select by ARP type */
const int SELECT_DRIVER		= 4;	/* Select by Driver name */
const int SELECT_BUSINFO	= 5;	/* Select by Bus-Info */
const int SELECT_FIRMWARE	= 6;	/* Select by Firmware revision */
const int SELECT_BASEADDR	= 7;	/* Select by HW Base Address */
const int SELECT_IRQ		= 8;	/* Select by HW Irq line */
const int SELECT_INTERRUPT	= 9;	/* Select by HW Irq line */
const int SELECT_IWPROTO	= 10;	/* Select by Wireless Protocol */
const int SELECT_PCMCIASLOT	= 11;	/* Select by Pcmcia Slot */
const int SELECT_SYSFS		= 12;	/* Select by sysfs file */
const int SELECT_PREVNAME	= 13;	/* Select by previous interface name */
#define SELECT_NUM		14

#define HAS_MAC_EXACT	1
#define HAS_MAC_FILTER	2
#define MAX_MAC_LEN	16	/* Maximum lenght of MAC address */

const struct ether_addr	zero_mac = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

const struct option long_opt[] =
{ 
  {"config-file", 1, NULL, 'c' },
  {"debian", 0, NULL, 'd' },
  {"dry-run", 0, NULL, 'D' },
  {"help", 0, NULL, '?' },
  {"interface", 1, NULL, 'i' },
  {"newname", 1, NULL, 'n' },
  {"takeover", 0, NULL, 't' },
  {"udev", 0, NULL, 'u' },
  {"version", 0, NULL, 'v' },
  {"verbose", 0, NULL, 'V' },
  {NULL, 0, NULL, '\0' },
};

/* Pcmcia stab files */
#define PCMCIA_STAB1	"/var/lib/pcmcia/stab"
#define PCMCIA_STAB2	"/var/run/stab"

/* Max number of sysfs file types we support */
#define SYSFS_MAX_FILE	8

/* Userspace headers lag, fix that... */
#ifndef ARPHRD_IEEE1394
#define ARPHRD_IEEE1394 24
#endif
#ifndef ARPHRD_EUI64
#define ARPHRD_EUI64 27
#endif
#ifndef ARPHRD_IRDA
#define ARPHRD_IRDA 783
#endif

/* Length of various non-standard MAC addresses */
const int	weird_mac_len[][2] =
{
  { ARPHRD_IEEE1394, 8 },
  { ARPHRD_EUI64, 8 },
  { ARPHRD_IRDA, 4 },
};
const int weird_mac_len_num = sizeof(weird_mac_len) / sizeof(weird_mac_len[0]);

/****************************** TYPES ******************************/

/* Cut'n'paste from ethtool.h */
#define ETHTOOL_BUSINFO_LEN	32
/* these strings are set to whatever the driver author decides... */
struct ethtool_drvinfo {
	__u32	cmd;
	char	driver[32];	/* driver short name, "tulip", "eepro100" */
	char	version[32];	/* driver version string */
	char	fw_version[32];	/* firmware version string, if applicable */
	char	bus_info[ETHTOOL_BUSINFO_LEN];	/* Bus info for this IF. */
				/* For PCI devices, use pci_dev->slot_name. */
	char	reserved1[32];
	char	reserved2[16];
	__u32	n_stats;	/* number of u64's from ETHTOOL_GSTATS */
	__u32	testinfo_len;
	__u32	eedump_len;	/* Size of data from ETHTOOL_GEEPROM (bytes) */
	__u32	regdump_len;	/* Size of data from ETHTOOL_GREGS (bytes) */
};
#define ETHTOOL_GDRVINFO	0x00000003 /* Get driver info. */

/* Description of an interface mapping */
typedef struct if_mapping
{ 
  /* Linked list */
  struct if_mapping *	next;

  /* Name of this interface */
  char			ifname[IFNAMSIZ+1];
  char *		sysfs_devpath;
  int			sysfs_devplen;

  /* Selectors for this interface */
  int			active[SELECT_NUM];	/* Selectors active */

  /* Selector data */
  unsigned char		mac[MAX_MAC_LEN];	/* Exact MAC address, hex */
  int			mac_len;		/* Length (usually 6) */
  char			mac_filter[16*3 + 1];	/* WildCard, ascii */
  unsigned short	hw_type;		/* Link/ARP type */
  char			driver[32];		/* driver short name */
  char		bus_info[ETHTOOL_BUSINFO_LEN];	/* Bus info for this IF. */
  char			fw_version[32];		/* Firmware revision */
  unsigned short	base_addr;		/* HW Base I/O address */ 
  unsigned char		irq;			/* HW irq line */
  char			iwproto[IFNAMSIZ + 1];	/* Wireless/protocol name */
  int			pcmcia_slot;		/* Pcmcia slot */
  char *		sysfs[SYSFS_MAX_FILE];	/* sysfs selectors */
  char			prevname[IFNAMSIZ+1];	/* previous interface name */
} if_mapping;

/* Extra parsing information when adding a mapping */
typedef struct add_extra
{ 
  char *		modif_pos;		/* Descriptor modifier */
  size_t		modif_len;
} parsing_extra;

/* Prototype for adding a selector to a mapping. Return -1 if invalid value. */
typedef int (*mapping_add)(struct if_mapping *	ifnode,
			   int *		active,
			   char *		pos,
			   size_t		len,
			   struct add_extra *	extra,
			   int			linenum);

/* Prototype for comparing the selector of two mapping. Return 0 if matches. */
typedef int (*mapping_cmp)(struct if_mapping *	ifnode,
			   struct if_mapping *	target);
/* Prototype for extracting selector value from live interface */
typedef int (*mapping_get)(int			skfd,
			   const char *		ifname,
			   struct if_mapping *	target,
			   int			flag);

/* How to handle a selector */
typedef struct mapping_selector
{
  char *	name;
  mapping_add	add_fn;
  mapping_cmp	cmp_fn;
  mapping_get	get_fn;
} mapping_selector;

/* sysfs global data */
typedef struct sysfs_metadata
{
  char *		root;			/* Root of the sysfs */
  int			rlen;			/* Size of it */
  int			filenum;		/* Number of files */
  char *		filename[SYSFS_MAX_FILE];	/* Name of files */
} sysfs_metadata;

/**************************** PROTOTYPES ****************************/

static int
	mapping_addmac(struct if_mapping *	ifnode,
		       int *			active,
		       char *			pos,
		       size_t			len,
		       struct add_extra *	extra,
		       int			linenum);
static int
	mapping_cmpmac(struct if_mapping *	ifnode,
		       struct if_mapping *	target);
static int
	mapping_getmac(int			skfd,
		       const char *		ifname,
		       struct if_mapping *	target,
		       int			flag);
static int
	mapping_addarp(struct if_mapping *	ifnode,
		       int *			active,
		       char *			pos,
		       size_t			len,
		       struct add_extra *	extra,
		       int			linenum);
static int
	mapping_cmparp(struct if_mapping *	ifnode,
		       struct if_mapping *	target);
static int
	mapping_getarp(int			skfd,
		       const char *		ifname,
		       struct if_mapping *	target,
		       int			flag);
static int
	mapping_adddriver(struct if_mapping *	ifnode,
			  int *			active,
			  char *		pos,
			  size_t		len,
			  struct add_extra *	extra,
			  int			linenum);
static int
	mapping_cmpdriver(struct if_mapping *	ifnode,
			  struct if_mapping *	target);
static int
	mapping_addbusinfo(struct if_mapping *	ifnode,
			   int *		active,
			   char *		pos,
			   size_t		len,
			   struct add_extra *	extra,
			   int			linenum);
static int
	mapping_cmpbusinfo(struct if_mapping *	ifnode,
			   struct if_mapping *	target);
static int
	mapping_addfirmware(struct if_mapping *	ifnode,
			    int *		active,
			    char *		pos,
			    size_t		len,
			    struct add_extra *	extra,
			    int			linenum);
static int
	mapping_cmpfirmware(struct if_mapping *	ifnode,
			    struct if_mapping *	target);
static int
	mapping_getdriverbusinfo(int			skfd,
				 const char *		ifname,
				 struct if_mapping *	target,
				 int			flag);
static int
	mapping_addbaseaddr(struct if_mapping *	ifnode,
			    int *		active,
			    char *		pos,
			    size_t		len,
			    struct add_extra *	extra,
			    int			linenum);
static int
	mapping_cmpbaseaddr(struct if_mapping *	ifnode,
			    struct if_mapping *	target);
static int
	mapping_addirq(struct if_mapping *	ifnode,
		       int *			active,
		       char *			pos,
		       size_t			len,
		       struct add_extra *	extra,
		       int			linenum);
static int
	mapping_cmpirq(struct if_mapping *	ifnode,
		       struct if_mapping *	target);
static int
	mapping_getbaseaddrirq(int			skfd,
			       const char *		ifname,
			       struct if_mapping *	target,
			       int			flag);
static int
	mapping_addiwproto(struct if_mapping *	ifnode,
			   int *		active,
			   char *		pos,
			   size_t		len,
			   struct add_extra *	extra,
			   int			linenum);
static int
	mapping_cmpiwproto(struct if_mapping *	ifnode,
			   struct if_mapping *	target);
static int
	mapping_getiwproto(int			skfd,
			   const char *		ifname,
			   struct if_mapping *	target,
			   int			flag);
static int
	mapping_addpcmciaslot(struct if_mapping *	ifnode,
			      int *			active,
			      char *			pos,
			      size_t			len,
			      struct add_extra *	extra,
			      int			linenum);
static int
	mapping_cmppcmciaslot(struct if_mapping *	ifnode,
			   struct if_mapping *		target);
static int
	mapping_getpcmciaslot(int			skfd,
			      const char *		ifname,
			      struct if_mapping *	target,
			      int			flag);
static int
	mapping_addsysfs(struct if_mapping *	ifnode,
			 int *			active,
			 char *			pos,
			 size_t			len,
			 struct add_extra *	extra,
			 int			linenum);
static int
	mapping_cmpsysfs(struct if_mapping *	ifnode,
			 struct if_mapping *	target);
static int
	mapping_getsysfs(int			skfd,
			 const char *		ifname,
			 struct if_mapping *	target,
			 int			flag);
static int
	mapping_addprevname(struct if_mapping *	ifnode,
			   int *		active,
			   char *		pos,
			   size_t		len,
			   struct add_extra *	extra,
			   int			linenum);
static int
	mapping_cmpprevname(struct if_mapping *	ifnode,
			   struct if_mapping *	target);
static int
	mapping_getprevname(int			skfd,
			   const char *		ifname,
			   struct if_mapping *	target,
			   int			flag);

/**************************** VARIABLES ****************************/

/* List of mapping read for config file */
struct if_mapping *	mapping_list = NULL;

/* List of selectors we can handle */
const struct mapping_selector	selector_list[] =
{
  /* MAC address and ARP/Link type from ifconfig */
  { "mac", &mapping_addmac, &mapping_cmpmac, &mapping_getmac },
  { "ethaddr", &mapping_addmac, &mapping_cmpmac, &mapping_getmac },
  { "arp", &mapping_addarp, &mapping_cmparp, &mapping_getarp },
  { "linktype", &mapping_addarp, &mapping_cmparp, &mapping_getarp },
  /* Driver name, Bus-Info and firmware rev from ethtool -i */
  { "driver", &mapping_adddriver, &mapping_cmpdriver,
    &mapping_getdriverbusinfo },
  { "businfo", &mapping_addbusinfo, &mapping_cmpbusinfo,
    &mapping_getdriverbusinfo },
  { "firmware", &mapping_addfirmware, &mapping_cmpfirmware,
    &mapping_getdriverbusinfo },
  /* Base Address and IRQ from ifconfig */
  { "baseaddress", &mapping_addbaseaddr, &mapping_cmpbaseaddr,
    &mapping_getbaseaddrirq },
  { "irq", &mapping_addirq, &mapping_cmpirq, &mapping_getbaseaddrirq },
  { "interrupt", &mapping_addirq, &mapping_cmpirq, &mapping_getbaseaddrirq },
  /* Wireless Protocol from iwconfig */
  { "iwproto", &mapping_addiwproto, &mapping_cmpiwproto, &mapping_getiwproto },
  /* Pcmcia slot from cardmgr */
  { "pcmciaslot", &mapping_addpcmciaslot, &mapping_cmppcmciaslot, &mapping_getpcmciaslot },
  /* sysfs file (udev emulation) */
  { "sysfs", &mapping_addsysfs, &mapping_cmpsysfs, &mapping_getsysfs },
  /* previous interface name */
  { "prevname", &mapping_addprevname, &mapping_cmpprevname, &mapping_getprevname },
  /* The Terminator */
  { NULL, NULL, NULL, NULL },
};
const int selector_num = sizeof(selector_list)/sizeof(selector_list[0]);

/* List of active selectors */
int	selector_active[SELECT_NUM];	/* Selectors active */

/*
 * All the following flags are controlled by the command line switches...
 * It's a bit hackish to have them all as global, so maybe we should pass
 * them in a big struct as function arguments... More complex and
 * probably not worth it ?
 */

/* Invocation type */
int	print_newname = 0;
char *	new_name = NULL;

/* Takeover support */
int	force_takeover = 0;	/* Takeover name from other interface */
int	num_takeover = 0;	/* Number of takeover done */

/* Dry-run support */
int	dry_run = 0;		/* Just print new name, don't rename */

/* Verbose support (i.e. debugging) */
int	verbose = 0;

/* udev output support (print new DEVPATH) */
int	udev_output = 0;

/* sysfs global data */
struct sysfs_metadata	sysfs_global =
{
  NULL, 0,
  0, { NULL, NULL, NULL, NULL, NULL },
};

/******************** INTERFACE NAME MANAGEMENT ********************/
/*
 * Bunch of low level function for managing interface names.
 */

/*------------------------------------------------------------------*/
/*
 * Compare two interface names, with wildcards.
 * We can't use fnmatch() because we don't want expansion of '[...]'
 * expressions, '\' sequences and matching of '.'.
 * We only want to match a single '*' (converted to a %d at that point)
 * to a numerical value (no ascii).
 * Return 0 is matches.
 */
static int
if_match_ifname(const char *	pattern,
		const char *	value)
{
  const char *	p;
  const char *	v;
  int		n;
  int		ret;

  /* Check for a wildcard */
  p = strchr(pattern, '*');

  /* No wildcard, simple comparison */
  if(p == NULL)
    return(strcmp(pattern, value));

  /* Check is prefixes match */
  n = (p - pattern);
  ret = strncmp(pattern, value, n);
  if(ret)
    return(ret);

  /* Check that value has some digits at this point */
  v = value + n;
  if(!isdigit(*v))
    return(-1);

  /* Skip digits to go to value suffix */
  do
    v++;
  while(isdigit(*v));

  /* Pattern suffix */
  p += 1;

  /* Compare suffixes */
  return(strcmp(p, v));
}

/*------------------------------------------------------------------*/
/*
 * Steal interface name from another interface. This enable interface
 * name swapping.
 * This will work :
 *	1) with kernel 2.6.X
 *	2) if other interface is down
 * Because of (2), it won't work with hotplug, but we don't need it
 * with hotplug, only with static ifaces...
 */
static int
if_takeover_name(int			skfd,
		 const char *		victimname)
{
  char		autoname[IFNAMSIZ+1];
  int		len;
  struct ifreq	ifr;
  int		ret;

  /* Compute name for victim interface */
  len = strlen(victimname);
  memcpy(autoname, victimname, len + 1);
  if(len > (IFNAMSIZ - 2))
    len = IFNAMSIZ - 2;		/* Make sure we have at least two char */
  len--;			/* Convert to index */
  while(isdigit(autoname[len]))
    len--;			/* Scrap all trailing digits */
  strcpy(autoname + len + 1, "%d");

  if(verbose)
    fprintf(stderr, "Takeover : moving interface `%s' to `%s'.\n",
	    victimname, autoname);

  /* Prepare request */
  bzero(&ifr, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, victimname, IFNAMSIZ); 
  strncpy(ifr.ifr_newname, autoname, IFNAMSIZ); 

  /* Rename victim interface */
  ret = ioctl(skfd, SIOCSIFNAME, &ifr);

  if(!ret)
    num_takeover++;

  return(ret);
}

/*------------------------------------------------------------------*/
/*
 * Ask the kernel to change the name of an interface.
 * That's what we want to do. All the rest is to make sure we call this
 * appropriately.
 */
static int
if_set_name(int			skfd,
	    const char *	oldname,
	    const char *	newname,
	    char *		retname)
{
  struct ifreq	ifr;
  char *	star;
  int		ret;

  /* The kernel doesn't check is the interface already has the correct
   * name and may return an error, so check ourselves.
   * In the case of wildcard, the result can be weird : if oldname='eth0'
   * and newname='eth*', retname would be 'eth1'.
   * So, if the oldname value matches the newname pattern, just return
   * success. */
  if(!if_match_ifname(newname, oldname))
    {
      if(verbose)
	fprintf(stderr, "Setting : Interface `%s' already matches `%s'.\n",
		oldname, newname);

      strcpy(retname, oldname);
      return(0);
    }

  /* Prepare request */
  bzero(&ifr, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, oldname, IFNAMSIZ); 
  strncpy(ifr.ifr_newname, newname, IFNAMSIZ); 

  /* Check for wildcard interface name, such as 'eth*' or 'wlan*'...
   * This require specific kernel support (2.6.2-rc1 and later).
   * We externally use '*', but the kernel doesn't know about that,
   * so convert it to something it knows about... */
  star = strchr(newname, '*');
  if(star != NULL)
    {
      int	slen = star - newname;
      /* Replace '*' with '%d' in the new buffer */
      star = ifr.ifr_newname + slen;
      /* Size was checked in process_rename() and mapping_create() */
      memmove(star + 2, star + 1, IFNAMSIZ - slen - 2);
      star[0] = '%';
      star[1] = 'd';
    }

  /* Do it */
  ret = ioctl(skfd, SIOCSIFNAME, &ifr);

  /* Takeover support : grab interface name from another interface */
  if(ret && (errno == EEXIST) && force_takeover)
    {
      /* Push things around */
      ret = if_takeover_name(skfd, newname);
      if(!ret)
	/* Second try */
	ret = ioctl(skfd, SIOCSIFNAME, &ifr);
    }

  if(!ret)
    {
      /* Get the real new name (in case newname is a wildcard) */
      strcpy(retname, ifr.ifr_newname);

      if(verbose)
	fprintf(stderr, "Setting : Interface `%s' renamed to `%s'.\n",
		oldname, retname);
    }

  return(ret);
}

/************************ SELECTOR HANDLING ************************/
/*
 * Handle the various selector we support
 */

/*------------------------------------------------------------------*/
/*
 * Add a MAC address selector to a mapping
 */
static int
mapping_addmac(struct if_mapping *	ifnode,
	       int *			active,
	       char *			string,
	       size_t			len,
	       struct add_extra *	extra,
	       int			linenum)
{
  size_t	n;

  /* Avoid "Unused parameter" warning */
  extra = extra;

  /* Verify validity of string */
  if(len >= sizeof(ifnode->mac_filter))
    { 
      fprintf(stderr, "Error : MAC address too long at line %d\n", linenum);  
      return(-1);
    }
  n = strspn(string, "0123456789ABCDEFabcdef:*"); 
  if(n < len)
    {
      fprintf(stderr, "Error: Invalid MAC address `%s' at line %d\n",
	      string, linenum);
      return(-1);
    }

  /* Copy as filter in all cases */
  memcpy(ifnode->mac_filter, string, len + 1); 

  /* Check the type of MAC address */
  if (strchr(ifnode->mac_filter, '*') != NULL)
    {
      /* This is a wilcard. Usual format : "01:23:45:*"
       * Unfortunately, we can't do proper parsing. */
      ifnode->active[SELECT_MAC] = HAS_MAC_FILTER;
      active[SELECT_MAC] = HAS_MAC_FILTER;
    }
  else
    {
      /* Not a wildcard : "01:23:45:67:89:AB" */
      ifnode->mac_len = iw_mac_aton(ifnode->mac_filter,
				    ifnode->mac, MAX_MAC_LEN);
      if(ifnode->mac_len == 0)
	{
	  fprintf(stderr, "Error: Invalid MAC address `%s' at line %d\n",
		  ifnode->mac_filter, linenum);
	  return(-1);
	}

      /* Check that it's not NULL */
      if((ifnode->mac_len == 6) && (!memcmp(&ifnode->mac, &zero_mac, 6)))
	{
	  fprintf(stderr,
		  "Warning: MAC address is null at line %d, this is dangerous...\n",
		  linenum);
	}

      ifnode->active[SELECT_MAC] = HAS_MAC_EXACT;
      if(active[SELECT_MAC] == 0)
	active[SELECT_MAC] = HAS_MAC_EXACT;
    }

  if(verbose)
    fprintf(stderr,
	    "Parsing : Added %s MAC address `%s' from line %d.\n",
	    ifnode->active[SELECT_MAC] == HAS_MAC_FILTER ? "filter" : "exact",
	    ifnode->mac_filter, linenum);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Compare the mac address of two mappings
 */
static int
mapping_cmpmac(struct if_mapping *	ifnode,
	       struct if_mapping *	target)
{
  /* Check for wildcard matching */
  if(ifnode->active[SELECT_MAC] == HAS_MAC_FILTER)
    /* Do wildcard matching, case insensitive */
    return(fnmatch(ifnode->mac_filter, target->mac_filter, FNM_CASEFOLD));
  else
    /* Exact matching, in hex */
    return((ifnode->mac_len != target->mac_len) ||
	   memcmp(ifnode->mac, target->mac, ifnode->mac_len));
}

/*------------------------------------------------------------------*/
/*
 * Extract the MAC address and Link Type of an interface
 */
static int
mapping_getmac(int			skfd,
	       const char *		ifname,
	       struct if_mapping *	target,
	       int			flag)
{
  struct ifreq	ifr;
  int		ret;
  int		i;

  /* Get MAC address */
  bzero(&ifr, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  ret = ioctl(skfd, SIOCGIFHWADDR, &ifr);
  if(ret < 0)
    {
      fprintf(stderr, "Error: Can't read MAC address on interface `%s' : %s\n",
	      ifname, strerror(errno));
      return(-1);
    }

  /* Extract ARP type */
  target->hw_type = ifr.ifr_hwaddr.sa_family;
  /* Calculate address length */
  target->mac_len = 6;
  for(i = 0; i < weird_mac_len_num; i++)
    if(weird_mac_len[i][0] == ifr.ifr_hwaddr.sa_family)
      {
	target->mac_len = weird_mac_len[i][1];
	break;
      }
  /* Extract MAC address bytes */
  memcpy(target->mac, ifr.ifr_hwaddr.sa_data, target->mac_len);

  /* Check the type of comparison */
  if((flag == HAS_MAC_FILTER) || verbose)
    {
      /* Convert to ASCII */
      iw_mac_ntop(target->mac, target->mac_len,
		  target->mac_filter, sizeof(target->mac_filter));
    }

  target->active[SELECT_MAC] = flag;
  target->active[SELECT_ARP] = 1;

  if(verbose)
    fprintf(stderr,
	    "Querying %s : Got MAC address `%s' and ARP/Link Type `%d'.\n",
	    ifname, target->mac_filter, target->hw_type);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Add a ARP/Link type selector to a mapping
 */
static int
mapping_addarp(struct if_mapping *	ifnode,
	       int *			active,
	       char *			string,
	       size_t			len,
	       struct add_extra *	extra,
	       int			linenum)
{
  size_t	n;
  unsigned int	type;

  /* Avoid "Unused parameter" warning */
  extra = extra;

  /* Verify validity of string, convert to int */
  n = strspn(string, "0123456789"); 
  if((n < len) || (sscanf(string, "%d", &type) != 1))
    {
      fprintf(stderr, "Error: Invalid ARP/Link Type `%s' at line %d\n",
	      string, linenum);
      return(-1);
    }

  ifnode->hw_type = (unsigned short) type;
  ifnode->active[SELECT_ARP] = 1;
  active[SELECT_ARP] = 1;

  if(verbose)
    fprintf(stderr, "Parsing : Added ARP/Link Type `%d' from line %d.\n",
	    ifnode->hw_type, linenum);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Compare the ARP/Link type of two mappings
 */
static int
mapping_cmparp(struct if_mapping *	ifnode,
	       struct if_mapping *	target)
{
  return(!(ifnode->hw_type == target->hw_type));
}

/*------------------------------------------------------------------*/
/*
 * Extract the ARP/Link type of an interface
 */
static int
mapping_getarp(int			skfd,
	       const char *		ifname,
	       struct if_mapping *	target,
	       int			flag)
{
  /* We may have already extracted the MAC address */
  if(target->active[SELECT_MAC])
    return(0);

  /* Otherwise just do it */
  return(mapping_getmac(skfd, ifname, target, flag));
}

/*------------------------------------------------------------------*/
/*
 * Add a Driver name selector to a mapping
 */
static int
mapping_adddriver(struct if_mapping *	ifnode,
		  int *			active,
		  char *		string,
		  size_t		len,
		  struct add_extra *	extra,
		  int			linenum)
{
  /* Avoid "Unused parameter" warning */
  extra = extra;

  /* Plain string, minimal verification */
  if(len >= sizeof(ifnode->driver))
    { 
      fprintf(stderr, "Error: Driver name too long at line %d\n", linenum);  
      return(-1);
    }

  /* Copy */
  memcpy(ifnode->driver, string, len + 1); 

  /* Activate */
  ifnode->active[SELECT_DRIVER] = 1;
  active[SELECT_DRIVER] = 1;

  if(verbose)
    fprintf(stderr,
	    "Parsing : Added Driver name `%s' from line %d.\n",
	    ifnode->driver, linenum);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Compare the Driver name of two mappings
 */
static int
mapping_cmpdriver(struct if_mapping *	ifnode,
		  struct if_mapping *	target)
{
  /* Do wildcard matching, case insensitive */
  return(fnmatch(ifnode->driver, target->driver, FNM_CASEFOLD));
}

/*------------------------------------------------------------------*/
/*
 * Add a Bus-Info selector to a mapping
 */
static int
mapping_addbusinfo(struct if_mapping *	ifnode,
		   int *		active,
		   char *		string,
		   size_t		len,
		   struct add_extra *	extra,
		   int			linenum)
{
#if 0
  size_t	n;
#endif

  /* Avoid "Unused parameter" warning */
  extra = extra;

  /* Verify validity of string */
  if(len >= sizeof(ifnode->bus_info))
    { 
      fprintf(stderr, "Bus Info too long at line %d\n", linenum);  
      return(-1);
    }
#if 0
  /* Hum... This doesn's seem true for non-PCI bus-info */
  n = strspn(string, "0123456789ABCDEFabcdef:.*"); 
  if(n < len)
    {
      fprintf(stderr, "Error: Invalid Bus Info `%s' at line %d\n",
	      string, linenum);
      return(-1);
    }
#endif

  /* Copy */
  memcpy(ifnode->bus_info, string, len + 1); 

  /* Activate */
  ifnode->active[SELECT_BUSINFO] = 1;
  active[SELECT_BUSINFO] = 1;

  if(verbose)
    fprintf(stderr,
	    "Parsing : Added Bus Info `%s' from line %d.\n",
	    ifnode->bus_info, linenum);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Compare the Bus-Info of two mappings
 */
static int
mapping_cmpbusinfo(struct if_mapping *	ifnode,
		   struct if_mapping *	target)
{
  /* Do wildcard matching, case insensitive */
  return(fnmatch(ifnode->bus_info, target->bus_info, FNM_CASEFOLD));
}

/*------------------------------------------------------------------*/
/*
 * Add a Firmare revision selector to a mapping
 */
static int
mapping_addfirmware(struct if_mapping *	ifnode,
		    int *		active,
		    char *		string,
		    size_t		len,
		    struct add_extra *	extra,
		    int			linenum)
{
  /* Avoid "Unused parameter" warning */
  extra = extra;

  /* Verify validity of string */
  if(len >= sizeof(ifnode->fw_version))
    { 
      fprintf(stderr, "Firmware revision too long at line %d\n", linenum);  
      return(-1);
    }

  /* Copy */
  memcpy(ifnode->fw_version, string, len + 1); 

  /* Activate */
  ifnode->active[SELECT_FIRMWARE] = 1;
  active[SELECT_FIRMWARE] = 1;

  if(verbose)
    fprintf(stderr,
	    "Parsing : Added Firmware Revision `%s' from line %d.\n",
	    ifnode->fw_version, linenum);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Compare the Bus-Info of two mappings
 */
static int
mapping_cmpfirmware(struct if_mapping *	ifnode,
		    struct if_mapping *	target)
{
  /* Do wildcard matching, case insensitive */
  return(fnmatch(ifnode->fw_version, target->fw_version, FNM_CASEFOLD));
}

/*------------------------------------------------------------------*/
/*
 * Extract the Driver name and Bus-Info from a live interface
 */
static int
mapping_getdriverbusinfo(int			skfd,
			 const char *		ifname,
			 struct if_mapping *	target,
			 int			flag)
{
  struct ifreq	ifr;
  struct ethtool_drvinfo drvinfo;
  int	ret;

  /* Avoid "Unused parameter" warning */
  flag = flag;

  /* We may come here twice or more, so do the job only once */
  if(target->active[SELECT_DRIVER] || target->active[SELECT_BUSINFO]
     || target->active[SELECT_FIRMWARE])
    return(0);

  /* Prepare request */
  bzero(&ifr, sizeof(struct ifreq));
  bzero(&drvinfo, sizeof(struct ethtool_drvinfo));
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  drvinfo.cmd = ETHTOOL_GDRVINFO;
  ifr.ifr_data = (caddr_t) &drvinfo;

  /* Do it */
  ret = ioctl(skfd, SIOCETHTOOL, &ifr);
  if(ret < 0)
    {
      /* Most drivers don't support that, keep quiet for now */
      if(verbose)
	fprintf(stderr,
		"Error: Can't read driver/bus-info on interface `%s' : %s\n",
		ifname, strerror(errno));
      return(-1);
    }

  /* Copy over */
  strcpy(target->driver, drvinfo.driver);
  strcpy(target->bus_info, drvinfo.bus_info);
  strcpy(target->fw_version, drvinfo.fw_version);

  /* Activate */
  target->active[SELECT_DRIVER] = 1;
  target->active[SELECT_BUSINFO] = 1;
  target->active[SELECT_FIRMWARE] = 1;

  if(verbose)
    fprintf(stderr,
	    "Querying %s : Got Driver name `%s', Bus Info `%s' and Firmware `%s'.\n",
	    ifname, target->driver, target->bus_info, target->fw_version);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Add a Base Address selector to a mapping
 */
static int
mapping_addbaseaddr(struct if_mapping *	ifnode,
		    int *		active,
		    char *		string,
		    size_t		len,
		    struct add_extra *	extra,
		    int			linenum)
{
  size_t	n;
  unsigned int	address;

  /* Avoid "Unused parameter" warning */
  extra = extra;

  /* Verify validity of string */
  n = strspn(string, "0123456789ABCDEFabcdefx"); 
  if((n < len) || (sscanf(string, "0x%X", &address) != 1))
    {
      fprintf(stderr, "Error: Invalid Base Address `%s' at line %d\n",
	      string, linenum);
      return(-1);
    }

  /* Copy */
  ifnode->base_addr = (unsigned short) address;

  /* Activate */
  ifnode->active[SELECT_BASEADDR] = 1;
  active[SELECT_BASEADDR] = 1;

  if(verbose)
    fprintf(stderr,
	    "Parsing : Added Base Address `0x%X' from line %d.\n",
	    ifnode->base_addr, linenum);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Compare the Base Address of two mappings
 */
static int
mapping_cmpbaseaddr(struct if_mapping *	ifnode,
		    struct if_mapping *	target)
{
  /* Do wildcard matching, case insensitive */
  return(!(ifnode->base_addr == target->base_addr));
}

/*------------------------------------------------------------------*/
/*
 * Add a IRQ selector to a mapping
 */
static int
mapping_addirq(struct if_mapping *	ifnode,
	       int *			active,
	       char *			string,
	       size_t			len,
	       struct add_extra *	extra,
	       int			linenum)
{
  size_t	n;
  unsigned int	irq;

  /* Avoid "Unused parameter" warning */
  extra = extra;

  /* Verify validity of string */
  n = strspn(string, "0123456789"); 
  if((n < len) || (sscanf(string, "%d", &irq) != 1))
    {
      fprintf(stderr, "Error: Invalid Base Address `%s' at line %d\n",
	      string, linenum);
      return(-1);
    }

  /* Copy */
  ifnode->irq = (unsigned char) irq;

  /* Activate */
  ifnode->active[SELECT_IRQ] = 1;
  active[SELECT_IRQ] = 1;

  if(verbose)
    fprintf(stderr,
	    "Parsing : Added IRQ `%d' from line %d.\n",
	    ifnode->irq, linenum);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Compare the IRQ of two mappings
 */
static int
mapping_cmpirq(struct if_mapping *	ifnode,
	       struct if_mapping *	target)
{
  /* Do wildcard matching, case insensitive */
  return(!(ifnode->irq == target->irq));
}

/*------------------------------------------------------------------*/
/*
 * Extract the Driver name and Bus-Info from a live interface
 */
static int
mapping_getbaseaddrirq(int			skfd,
		       const char *		ifname,
		       struct if_mapping *	target,
		       int			flag)
{
  struct ifreq	ifr;
  struct ifmap	map;		/* hardware setup        */
  int	ret;

  /* Avoid "Unused parameter" warning */
  flag = flag;

  /* We may come here twice, so do the job only once */
  if(target->active[SELECT_BASEADDR] || target->active[SELECT_IRQ])
    return(0);

  /* Prepare request */
  bzero(&ifr, sizeof(struct ifreq));
  bzero(&map, sizeof(struct ifmap));
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

  /* Do it */
  ret = ioctl(skfd, SIOCGIFMAP, &ifr);
  if(ret < 0)
    {
      /* Don't know if every interface has that, so keep quiet... */
      if(verbose)
	fprintf(stderr,
		"Error: Can't read base address/irq on interface `%s' : %s\n",
		ifname, strerror(errno));
      return(-1);
    }

  /* Copy over, activate */
  if(ifr.ifr_map.base_addr >= 0x100)
    {
      target->base_addr = ifr.ifr_map.base_addr;
      target->active[SELECT_BASEADDR] = 1;
    }
  target->irq = ifr.ifr_map.irq;
  target->active[SELECT_IRQ] = 1;

  if(verbose)
    fprintf(stderr,
	    "Querying %s : Got Base Address `0x%X' and IRQ `%d'.\n",
	    ifname, target->base_addr, target->irq);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Add a Wireless Protocol selector to a mapping
 */
static int
mapping_addiwproto(struct if_mapping *	ifnode,
		   int *		active,
		   char *		string,
		   size_t		len,
		   struct add_extra *	extra,
		   int			linenum)
{
  /* Avoid "Unused parameter" warning */
  extra = extra;

  /* Verify validity of string */
  if(len >= sizeof(ifnode->iwproto))
    { 
      fprintf(stderr, "Wireless Protocol too long at line %d\n", linenum);  
      return(-1);
    }

  /* Copy */
  memcpy(ifnode->iwproto, string, len + 1); 

  /* Activate */
  ifnode->active[SELECT_IWPROTO] = 1;
  active[SELECT_IWPROTO] = 1;

  if(verbose)
    fprintf(stderr,
	    "Parsing : Added Wireless Protocol `%s' from line %d.\n",
	    ifnode->iwproto, linenum);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Compare the Wireless Protocol of two mappings
 */
static int
mapping_cmpiwproto(struct if_mapping *	ifnode,
		   struct if_mapping *	target)
{
  /* Do wildcard matching, case insensitive */
  return(fnmatch(ifnode->iwproto, target->iwproto, FNM_CASEFOLD));
}

/*------------------------------------------------------------------*/
/*
 * Extract the Wireless Protocol from a live interface
 */
static int
mapping_getiwproto(int			skfd,
		   const char *		ifname,
		   struct if_mapping *	target,
		   int			flag)
{
  struct iwreq		wrq;

  /* Avoid "Unused parameter" warning */
  flag = flag;

  /* Get wireless name */
  if(iw_get_ext(skfd, ifname, SIOCGIWNAME, &wrq) < 0)
    /* Don't complain about it, Ethernet cards will never support this */
    return(-1);

  strncpy(target->iwproto, wrq.u.name, IFNAMSIZ);
  target->iwproto[IFNAMSIZ] = '\0';

  /* Activate */
  target->active[SELECT_IWPROTO] = 1;

  if(verbose)
    fprintf(stderr,
	    "Querying %s : Got Wireless Protocol `%s'.\n",
	    ifname, target->iwproto);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Add a Pcmcia Slot selector to a mapping
 */
static int
mapping_addpcmciaslot(struct if_mapping *	ifnode,
		      int *			active,
		      char *			string,
		      size_t			len,
		      struct add_extra *	extra,
		      int			linenum)
{
  size_t	n;

  /* Avoid "Unused parameter" warning */
  extra = extra;

  /* Verify validity of string, convert to int */
  n = strspn(string, "0123456789"); 
  if((n < len) || (sscanf(string, "%d", &ifnode->pcmcia_slot) != 1))
    {
      fprintf(stderr, "Error: Invalid Pcmcia Slot `%s' at line %d\n",
	      string, linenum);
      return(-1);
    }

  ifnode->active[SELECT_PCMCIASLOT] = 1;
  active[SELECT_PCMCIASLOT] = 1;

  if(verbose)
    fprintf(stderr, "Parsing : Added Pcmcia Slot `%d' from line %d.\n",
	    ifnode->pcmcia_slot, linenum);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Compare the Pcmcia Slot of two mappings
 */
static int
mapping_cmppcmciaslot(struct if_mapping *	ifnode,
		      struct if_mapping *	target)
{
  return(!(ifnode->pcmcia_slot == target->pcmcia_slot));
}

/*------------------------------------------------------------------*/
/*
 * Extract the Pcmcia Slot of an interface
 * Note that this works only for cards fully managed by cardmgr.
 * With the kernel pcmcia modules, 32 bits cards (CardBus) are not managed
 * by cardmgr, and therefore won't have a valid slot number. For those
 * cards, you should use Bus Info (when the driver exports it).
 * In the long term, 16 bits card as well will no longer be managed by
 * cardmgr. Currently, Bus Info for 16 bit cards don't have any information
 * enabling to locate their physical location on the system, but I hope that
 * this will change.
 * When that happen, we can drop this code...
 */
static int
mapping_getpcmciaslot(int			skfd,
		      const char *		ifname,
		      struct if_mapping *	target,
		      int			flag)
{
  FILE *	stream;
  char *	linebuf = NULL;
  size_t	linelen = 0; 
  int		linenum = 0; 

  /* Avoid "Unused parameter" warning */
  skfd = skfd;
  flag = flag;

  /* Open the stab file for reading */
  stream = fopen(PCMCIA_STAB1, "r");
  if(!stream) 
    {
      /* Try again, alternate location */
      stream = fopen(PCMCIA_STAB2, "r");
      if(!stream) 
	{
	  fprintf(stderr, "Error: Can't open PCMCIA Stab file `%s' or `%s': %s\n",
		  PCMCIA_STAB1, PCMCIA_STAB2, strerror(errno)); 
	  return(-1);
	}
    }

  /* Read each line of file
   * getline is a GNU extension :-( The buffer is recycled and increased
   * as needed by getline. */
  while(getline(&linebuf, &linelen, stream) > 0)
    {
      char *			p;
      size_t			n;
      size_t			k;
      int			pcmcia_slot;
      int			i;

      /* Keep track of line number */
      linenum++;

      /* Get Pcmcia socket number */
      p = linebuf;
      while(isspace(*p))
	++p; 
      if(*p == '\0')
	continue;	/* Line ended */
      n = strcspn(p, " \t\n");
      k = strspn(p, "0123456789"); 
      if((k < n) || (sscanf(p, "%d", &pcmcia_slot) != 1))
	/* Next line */
	continue;

      /* Skip socket number */
      /* Skip socket number ; device class ; driver name ; instance */
      for(i = 0; i < 4; i++)
	{
	  /* Skip item */
	  p += n;
	  /* Skip space */
	  p += strspn(p, " \t\n"); 
	  if(*p == '\0')
	    break;	/* Line ended */
	  /* Next item size */
	  n = strcspn(p, " \t\n");
	}
      if(*p == '\0')
	continue;	/* Line ended */

      /* Terminate dev name */
      p[n] = '\0';

      /* Compare to interface name */
      if(!strcmp(p, ifname))
	{
	  /* Save */
	  target->pcmcia_slot = pcmcia_slot;

	  /* Activate */
	  target->active[SELECT_PCMCIASLOT] = 1;

	  if(verbose)
	    fprintf(stderr,
		    "Querying %s : Got Pcmcia Slot `%d'.\n",
		    ifname, target->pcmcia_slot);
	  /* Exit loop, found it */
	  break;
	}

      /* Finished -> next line */
    }

  /* Cleanup */
  free(linebuf);
  fclose(stream);

  return(target->active[SELECT_PCMCIASLOT] ? 0 : -1);
}

/*------------------------------------------------------------------*/
/*
 * Add a sysfs selector to a mapping
 */
static int
mapping_addsysfs(struct if_mapping *	ifnode,
		 int *			active,
		 char *			string,
		 size_t			len,
		 struct add_extra *	extra,
		 int			linenum)
{
  int		findex;	/* filename index */
  char *	sdup;

  /* Check if we have a modifier */
  if((extra == NULL) || (extra->modif_pos == NULL))
    { 
      fprintf(stderr, "Error: No SYSFS filename at line %d\n", linenum);  
      return(-1);
    }

  /* Search if the filename already exist */
  for(findex = 0; findex < sysfs_global.filenum; findex++)
    {
      if(!strcmp(extra->modif_pos, sysfs_global.filename[findex]))
	break;
    }

  /* If filename does not exist, creates it */
  if(findex == sysfs_global.filenum)
    {
      if(findex == SYSFS_MAX_FILE)
	{
	  fprintf(stderr, "Error: Too many SYSFS filenames at line %d\n", linenum);  
	  return(-1);
	}
      sdup = strndup(extra->modif_pos, extra->modif_len);
      if(sdup == NULL)
	{
	  fprintf(stderr, "Error: Can't allocate SYSFS file\n");  
	  return(-1);
	}
      sysfs_global.filename[findex] = sdup;
      sysfs_global.filenum++;
    }

  /* Store value */
  sdup = strndup(string, len);
  if(sdup == NULL)
    {
      fprintf(stderr, "Error: Can't allocate SYSFS value\n");  
      return(-1);
    }
  ifnode->sysfs[findex] = sdup;

  /* Activate */
  ifnode->active[SELECT_SYSFS] = 1;
  active[SELECT_SYSFS] = 1;

  if(verbose)
    fprintf(stderr,
	    "Parsing : Added SYSFS filename `%s' value `%s' from line %d.\n",
	    sysfs_global.filename[findex], ifnode->sysfs[findex], linenum);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Compare all the sysfs values of two mappings
 */
static int
mapping_cmpsysfs(struct if_mapping *	ifnode,
		 struct if_mapping *	target)
{
  int		findex;	/* filename index */
  int		match = 1;

  /* Loop on all sysfs selector */
  for(findex = 0; findex < sysfs_global.filenum; findex++)
    {
      /* If the mapping defines this sysfs selector.. */
      if(ifnode->sysfs[findex] != NULL)
	/* And if the sysfs values don't match */
	if((target->sysfs[findex] == NULL) ||
	   (fnmatch(ifnode->sysfs[findex], target->sysfs[findex],
		    FNM_CASEFOLD)))
	  /* Then the sysfs selector doesn't match */
	  match = 0;
    }

  return(!match);
}

/*------------------------------------------------------------------*/
/*
 * Extract all the sysfs values of an interface
 */
static int
mapping_getsysfs(int			skfd,
		 const char *		ifname,
		 struct if_mapping *	target,
		 int			flag)
{
  FILE *	stream;
  char *	fname;
  int		fnsize;
  char *	linebuf = NULL;
  size_t	linelen = 0; 
  char *	sdup;
  int		findex;	/* filename index */

  /* Avoid "Unused parameter" warning */
  skfd = skfd;
  flag = flag;

  /* Check if we know the devpath of this device */
  if(target->sysfs_devpath == NULL)
    {
      /* Check if we know the root of the sysfs filesystem */
      if(sysfs_global.root == NULL)
	{
	  /* Open the mount file for reading */
	  stream = fopen("/proc/mounts", "r");
	  if(!stream) 
	    {
	      fprintf(stderr, "Error: Can't open /proc/mounts file: %s\n",
		      strerror(errno)); 
	      return(-1);
	    }

	  /* Read each line of file
	   * getline is a GNU extension :-( The buffer is recycled and
	   * increased as needed by getline. */
	  while(getline(&linebuf, &linelen, stream) > 0)
	    {
	      int		i;
	      char *	p;
	      size_t	n;
	      char *	token[3];
	      size_t	toklen[3];

	      /* The format of /proc/mounts is similar to /etc/fstab (5).
	       * The first argument is the device. For sysfs, there is no
	       * associated device, so this argument is ignored.
	       * The second argument is the mount point.
	       * The third argument is the filesystem type.
	       */

	      /* Extract the first 3 tokens */
	      p = linebuf;
	      for(i = 0; i < 3; i++)
		{
		  while(isspace(*p))
		    ++p; 
		  token[i] = p;
		  n = strcspn(p, " \t\n");
		  toklen[i] = n;
		  p += n;
		}
	      /* Get the filesystem which type is "sysfs" */
	      if((n == 5) && (!strncasecmp(token[2], "sysfs", 5)))
		{
		  /* Get its mount point */
		  n = toklen[1];
		  sdup = strndup(token[1], n);
		  if((n == 0) || (sdup == NULL))
		    {
		      fprintf(stderr,
			      "Error: Can't parse /proc/mounts file: %s\n",
			      strerror(errno)); 
		      return(-1);
		    }
		  /* Store it */
		  sysfs_global.root = sdup;
		  sysfs_global.rlen = n;
		  break;
		}
	      /* Finished -> next line */
	    }

	  /* Cleanup */
	  fclose(stream);

	  /* Check if we found it */
	  if(sysfs_global.root == NULL)
	    {
	      fprintf(stderr,
		      "Error: Can't find sysfs in /proc/mounts file\n");
	      free(linebuf);
	      return(-1);
	    }
	}

      /* Construct devpath for this interface.
       * Reserve enough space to replace name without realloc. */
      fnsize = (sysfs_global.rlen + 11 + IFNAMSIZ + 1);
      fname = malloc(fnsize);
      if(fname == NULL)
	{
	  fprintf(stderr, "Error: Can't allocate SYSFS devpath\n");  
	  return(-1);
	}
      /* Not true devpath for 2.6.20+, but this syslink should work */
      target->sysfs_devplen = sprintf(fname, "%s/class/net/%s",
				      sysfs_global.root, ifname);
      target->sysfs_devpath = fname;
    }

  /* Loop on all sysfs selector */
  for(findex = 0; findex < sysfs_global.filenum; findex++)
    {
      char *	p;
      ssize_t	n;

      /* Construct complete filename for the sysfs selector */
      fnsize = (target->sysfs_devplen + 1 +
		strlen(sysfs_global.filename[findex]) + 1);
      fname = malloc(fnsize);
      if(fname == NULL)
	{
	  fprintf(stderr, "Error: Can't allocate SYSFS filename\n");  
	  free(linebuf);
	  return(-1);
	}
      sprintf(fname, "%s/%s", target->sysfs_devpath,
	      sysfs_global.filename[findex]);

      /* Open the sysfs file for reading */
      stream = fopen(fname, "r");
      if(!stream) 
	{
	  /* Some sysfs attribute may no exist for some interface */
	  if(verbose)
	    fprintf(stderr, "Error: Can't open file `%s': %s\n", fname,
		    strerror(errno)); 
	  /* Next sysfs selector */
	  continue;
	}

      /* Read file. Only one line in file. */
      n = getline(&linebuf, &linelen, stream);
      fclose(stream);
      if(n <= 0)
	{
	  /* Some attributes are just symlinks to another directory.
	   * We can read the attributes in that other directory
	   * just fine, but sometimes the symlink itself gives a lot
	   * of information.
	   * Examples : SYSFS{device} and SYSFS{device/driver}
	   * In such cases, get the name of the directory pointed to...
	   */
	  /*
	   * I must note that the API for readlink() is very bad,
	   * which force us to have this ugly code. Yuck !
	   */
	  int		allocsize = 128;	/* 256 = Good start */
	  int		retry = 16;
	  char *	linkpath = NULL;
	  int		pathlen;

	  /* Try reading the link with increased buffer size */
	  do
	    {
	      allocsize *= 2;
	      linkpath = realloc(linkpath, allocsize);
	      pathlen = readlink(fname, linkpath, allocsize);
	      /* If we did not hit the buffer limit, success */
	      if(pathlen < allocsize)
		break;
	    }
	  while(retry-- > 0);

	  /* Check for error, most likely ENOENT */
	  if(pathlen > 0)
	    /* We have a symlink ;-) Terminate the string. */
	    linkpath[pathlen] = '\0';
	  else
	    {
	      /* Error ! */
	      free(linkpath);

	      /* A lot of information in the sysfs is implicit, given
	       * by the position of a file in the tree. It is therefore
	       * important to be able to read the various components
	       * of a path. For this reason, we resolve '..' to the
	       * real name of the parent directory... */
	      /* We have at least 11 char, see above */
	      if(!strcmp(fname + fnsize - 4, "/.."))
		//if(!strcmp(fname + strlen(fname) - 3, "/.."))
		{
		  /* This procedure to get the realpath is not very
		   * nice, but it's the "best practice". Hmm... */
		  int	cwd_fd = open(".", O_RDONLY);
		  linkpath = NULL;
		  if(cwd_fd > 0)
		    {
		      int	ret = chdir(fname);
		      if(ret == 0)
			/* Using getcwd with NULL is a GNU extension. Nice. */
			linkpath = getcwd(NULL, 0);
		      /* This may fail, but it's not fatal */
		      fchdir(cwd_fd);
		    }
		  /* Check if we suceeded */
		  if(!linkpath)
		    {
		      free(linkpath);
		      if(verbose)
			fprintf(stderr, "Error: Can't read parent directory `%s'\n", fname);
		      /* Next sysfs selector */
		      continue;
		    }
		}
	      else
		{
		  /* Some sysfs attribute are void for some interface,
		   * we may have a real directory, or we may have permission
		   * issues... */
		  if(verbose)
		    fprintf(stderr, "Error: Can't read file `%s'\n", fname);
		  /* Next sysfs selector */
		  continue;
		}
	    }

	  /* Here, we have a link name or a parent directory name */

	  /* Keep only the last component of path name, save it */
	  p = basename(linkpath);
	  sdup = strdup(p);
	  free(linkpath);
	}
      else
	{
	  /* This is a regular file (well, pseudo file) */
	  /* Get content, remove trailing '/n', save it */
	  p = linebuf;
	  if(p[n - 1] == '\n')
	    n--;
	  sdup = strndup(p, n);
	}
      if(sdup == NULL)
	{
	  fprintf(stderr, "Error: Can't allocate SYSFS value\n"); 
	  free(linebuf);
	  return(-1);
	}
      target->sysfs[findex] = sdup;

      /* Activate */
      target->active[SELECT_SYSFS] = 1;

      if(verbose)
	fprintf(stderr,
		"Querying %s : Got SYSFS filename `%s' value `%s'.\n",
		ifname, sysfs_global.filename[findex], target->sysfs[findex]);

      /* Finished : Next sysfs selector */
    }

  /* Cleanup */
  free(linebuf);

  return(target->active[SELECT_SYSFS] ? 0 : -1);
}

/*------------------------------------------------------------------*/
/*
 * Add a Previous Interface Name selector to a mapping
 */
static int
mapping_addprevname(struct if_mapping *	ifnode,
		   int *		active,
		   char *		string,
		   size_t		len,
		   struct add_extra *	extra,
		   int			linenum)
{
  /* Avoid "Unused parameter" warning */
  extra = extra;

  /* Verify validity of string */
  if(len >= sizeof(ifnode->prevname))
    { 
      fprintf(stderr, "Old Interface Name too long at line %d\n", linenum);  
      return(-1);
    }

  /* Copy */
  memcpy(ifnode->prevname, string, len + 1); 

  /* Activate */
  ifnode->active[SELECT_PREVNAME] = 1;
  active[SELECT_PREVNAME] = 1;

  if(verbose)
    fprintf(stderr,
	    "Parsing : Added Old Interface Name `%s' from line %d.\n",
	    ifnode->prevname, linenum);

  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Compare the Previous Interface Name of two mappings
 * Note : this one is special.
 */
static int
mapping_cmpprevname(struct if_mapping *	ifnode,
		   struct if_mapping *	target)
{
  /* Do wildcard matching, case insensitive */
  return(fnmatch(ifnode->prevname, target->ifname, FNM_CASEFOLD));
}

/*------------------------------------------------------------------*/
/*
 * Extract the Previous Interface Name from a live interface
 */
static int
mapping_getprevname(int			skfd,
		   const char *		ifname,
		   struct if_mapping *	target,
		   int			flag)
{
  /* Avoid "Unused parameter" warning */
  skfd = skfd; ifname = ifname; flag = flag;

  /* Don't do anything, it's already in target->ifname ;-) */

  /* Activate */
  target->active[SELECT_PREVNAME] = 1;

  return(0);
}


/*********************** MAPPING MANAGEMENTS ***********************/
/*
 * Manage interface mappings.
 * Each mapping tell us how to identify a specific interface name.
 * It is composed of a bunch of selector values.
 */

/*------------------------------------------------------------------*/
/*
 * Create a new interface mapping and verify its name
 */
static struct if_mapping *
mapping_create(char *	pos,
	       int	len,
	       int	linenum)
{
  struct if_mapping *	ifnode;
  char *		star;

  star = memchr(pos, '*', len);

  /* Check overflow, need one extra char for wildcard */
  if((len + (star != NULL)) > IFNAMSIZ)
    {
      fprintf(stderr, "Error: Interface name `%.*s' too long at line %d\n",
	      (int) len, pos, linenum);  
      return(NULL);
    }

  /* Create mapping, zero it */
  ifnode = calloc(1, sizeof(if_mapping));
  if(!ifnode)
    {
      fprintf(stderr, "Error: Can't allocate interface mapping.\n");  
      return(NULL);
    }

  /* Set the name, terminates it */
  memcpy(ifnode->ifname, pos, len); 
  ifnode->ifname[len] = '\0'; 

  /* Check the interface name and issue various pedantic warnings.
   * We assume people using takeover want to force interfaces to those
   * names and know what they are doing, so don't bother them... */
  if((!force_takeover) &&
     ((!strcmp(ifnode->ifname, "eth0")) || (!strcmp(ifnode->ifname, "wlan0"))))
    fprintf(stderr,
	    "Warning: Interface name is `%s' at line %d, can't be mapped reliably.\n",
	    ifnode->ifname, linenum);
  if(strchr(ifnode->ifname, ':'))
    fprintf(stderr, "Warning: Alias device `%s' at line %d probably can't be mapped.\n",
	    ifnode->ifname, linenum);

  if(verbose)
    fprintf(stderr, "Parsing : Added Mapping `%s' from line %d.\n",
	    ifnode->ifname, linenum);

  /* Done */
  return(ifnode);
}

/*------------------------------------------------------------------*/
/*
 * Find the most appropriate selector matching a given selector name
 */
static inline const struct mapping_selector *
selector_find(const char *	string,
	      size_t		slen,
	      int		linenum)
{
  const struct mapping_selector *	found = NULL;
  int			ambig = 0;
  int			i;

  /* Go through all selectors */
  for(i = 0; selector_list[i].name != NULL; ++i)
    {
      /* No match -> next one */
      if(strncasecmp(selector_list[i].name, string, slen) != 0)
	continue;

      /* Exact match -> perfect */
      if(slen == strlen(selector_list[i].name))
	return &selector_list[i];

      /* Partial match */
      if(found == NULL)
	/* First time */
	found = &selector_list[i];
      else
	/* Another time */
	if (selector_list[i].add_fn != found->add_fn)
	  ambig = 1;
    }

  if(found == NULL)
    {
      fprintf(stderr, "Error: Unknown selector `%.*s' at line %d.\n",
	      (int) slen, string, linenum);
      return NULL;
    }

  if(ambig)
    {
      fprintf(stderr, "Selector `%.*s'at line %d is ambiguous.\n",
	      (int) slen, string, linenum);
      return NULL;
    }

  return found;
}

/*------------------------------------------------------------------*/
/*
 * Read the configuration file and extract all valid mappings and their
 * selectors.
 */
static int
mapping_readfile(const char *	filename)
{
  FILE *		stream;
  char *		linebuf = NULL;
  size_t		linelen = 0; 
  int			linenum = 0; 
  struct add_extra	extrainfo;

  /* Reset the list of filters */
  bzero(selector_active, sizeof(selector_active));

  /* Check filename */
  if(!strcmp(filename, "-"))
    {
      /* Read from stdin */
      stream = stdin;

    }
  else
    {
      /* Open the file for reading */
      stream = fopen(filename, "r");
      if(!stream) 
	{
	  fprintf(stderr, "Error: Can't open configuration file `%s': %s\n",
		  filename, strerror(errno)); 
	  return(-1);
	}
    }

  /* Read each line of file
   * getline is a GNU extension :-( The buffer is recycled and increased
   * as needed by getline. */
  while(getline(&linebuf, &linelen, stream) > 0)
    {
      struct if_mapping *	ifnode;
      char *			p;
      char *			e;
      size_t			n;
      int			ret = -13;	/* Complain if no selectors */

      /* Keep track of line number */
      linenum++;

      /* Every comments terminates parsing */
      if((p = strchr(linebuf,'#')) != NULL)
	*p = '\0';

      /* Get interface name */
      p = linebuf;
      while(isspace(*p))
	++p; 
      if(*p == '\0')
	continue;	/* Line ended */
      n = strcspn(p, " \t\n");

      /* Create mapping */
      ifnode = mapping_create(p, n, linenum);
      if(!ifnode)
	continue;	/* Ignore this line */
      p += n;
      p += strspn(p, " \t\n"); 

      /* Loop on all selectors */
      while(*p != '\0')
	{
	  const struct mapping_selector *	selector = NULL;
	  struct add_extra *			extra = NULL;

	  /* Selector name length - stop at modifier start */
	  n = strcspn(p, " \t\n{");

	  /* Find it */
	  selector = selector_find(p, n, linenum);
	  if(!selector)
	    {
	      ret = -1;
	      break;
	    }
	  p += n;

	  /* Check for modifier */
	  if(*p == '{')
	    {
	      p++;
	      /* Find end of modifier */
	      e = strchr(p, '}');
	      if(e == NULL)
		{
		  fprintf(stderr,
			  "Error: unterminated selector modifier value on line %d\n",
			  linenum);
		  ret = -1;
		  break;	/* Line ended */
		}
	      /* Fill in struct and hook it */
	      extrainfo.modif_pos = p;
	      extrainfo.modif_len = e - p;
	      extra = &extrainfo;
	      /* Terminate modifier value */
	      e[0] = '\0';
	      /* Skip it */
	      p = e + 1;
	    }

	  /* Get to selector value */
	  p += strspn(p, " \t\n"); 
	  if(*p == '\0')
	    {
	      fprintf(stderr, "Error: no value for selector `%s' on line %d\n",
		      selector->name, linenum);
	      ret = -1;
	      break;	/* Line ended */
	    }
	  /* Check for quoted arguments */
	  if(*p == '"')
	    {
	      p++;
	      e = strchr(p, '"');
	      if(e == NULL)
		{
		  fprintf(stderr,
			  "Error: unterminated quoted value on line %d\n",
			  linenum);
		  ret = -1;
		  break;	/* Line ended */
		}
	      n = e - p;
	      e++;
	    }
	  else
	    {
	      /* Just end at next blank */
	      n = strcspn(p, " \t\n");
	      e = p + n;
	    }
	  /* Make 'e' point past the '\0' we are going to add */
	  if(*e != '\0')
	    e++;
	  /* Terminate selector value */
	  p[n] = '\0';

	  /* Add it to the mapping */
	  ret = selector->add_fn(ifnode, selector_active, p, n,
				 extra, linenum);
	  if(ret < 0)
	    break;

	  /* Go to next selector */
	  p = e;
	  p += strspn(p, " \t\n"); 
	}

      /* We add a mapping only if it has at least one selector and if all
       * selectors were parsed properly. */
      if(ret < 0)
	{
	  /* If we have not yet printed an error, now is a good time ;-) */
	  if(ret == -13)
	    fprintf(stderr, "Error: Line %d ignored, no valid selectors\n",
		    linenum);
	  else
	    fprintf(stderr, "Error: Line %d ignored due to prior errors\n",
		    linenum);

	  free(ifnode);
	}
      else
	{
	  /* Link it in the list */
	  ifnode->next = mapping_list;
	  mapping_list = ifnode;
	}
    }

  /* Cleanup */
  free(linebuf);

  /* Finished reading, close the file */
  if(stream != stdin)
    fclose(stream);
  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Extract all the interesting selectors for the interface in consideration
 */
static struct if_mapping *
mapping_extract(int		skfd,
		const char *	ifname)
{
  struct if_mapping *	target;
  int			i;

  /* Create mapping, zero it */
  target = calloc(1, sizeof(if_mapping));
  if(!target)
    {
      fprintf(stderr, "Error: Can't allocate interface mapping.\n");  
      return(NULL);
    }

  /* Set the interface name */
  strcpy(target->ifname, ifname);

  /* Loop on all active selectors */
  for(i = 0; i < SELECT_NUM; i++)
    {
      /* Check if this selector is active */
      if(selector_active[i] != 0)
	{
	  /* Extract selector */
	  selector_list[i].get_fn(skfd, ifname, target, selector_active[i]);

	  /* Ignore errors. Some mapping may not need all selectors */
	}
    }

  return(target);
} 

/*------------------------------------------------------------------*/
/*
 * Find the first mapping in the list matching the one we want.
 */
static struct if_mapping *
mapping_find(struct if_mapping *	target)
{
  struct if_mapping *	ifnode;
  int			i;

  /* Look over all our mappings */
  for(ifnode = mapping_list; ifnode != NULL; ifnode = ifnode->next)
    {
      int		matches = 1;

      /* Look over all our selectors, all must match */
      for(i = 0; i < SELECT_NUM; i++)
	{
	  /* Check if this selector is active */
	  if(ifnode->active[i] != 0)
	    {
	      /* If this selector doesn't match, game over for this mapping */
	      if((target->active[i] == 0) ||
		 (selector_list[i].cmp_fn(ifnode, target) != 0))
		{
		  matches = 0;
		  break;
		}
	    }
	}

      /* Check is this mapping was "the one" */
      if(matches)
	return(ifnode);
    }

  /* Not found */
  return(NULL);
} 

/************************** MODULE SUPPORT **************************/
/*
 * Load all necessary module so that interfaces do exist.
 * This is necessary for system that are fully modular when
 * doing the boot time processing, because we need to run before
 * 'ifup -a'.
 */

/*------------------------------------------------------------------*/
/*
 * Probe interfaces based on our list of mappings.
 * This is the default, but usually not the best way to do it.
 */
static void
probe_mappings(int		skfd)
{
  struct if_mapping *	ifnode;
  struct ifreq		ifr;

  /* Look over all our mappings */
  for(ifnode = mapping_list; ifnode != NULL; ifnode = ifnode->next)
    {
      /* Can't load wildcards interface name :-( */
      if(strchr(ifnode->ifname, '%') != NULL)
	continue;

      if(verbose)
	fprintf(stderr, "Probing : Trying to load interface [%s]\n",
		ifnode->ifname);

      /* Trick the kernel into loading the interface.
       * This allow us to not depend on the exact path and
       * name of the '/sbin/modprobe' command.
       * Obviously, we expect this command to 'fail', as
       * the interface will load with the old/wrong name.
       */
      strncpy(ifr.ifr_name, ifnode->ifname, IFNAMSIZ);
      ioctl(skfd, SIOCGIFHWADDR, &ifr);
    }
}

/*------------------------------------------------------------------*/
/*
 * Probe interfaces based on Debian's config files.
 * This allow to enly load modules for interfaces the user want active,
 * all built-in interfaces that should remain unconfigured won't
 * be probed (and can have mappings).
 */
static void
probe_debian(int		skfd)
{
  FILE *		stream;
  char *		linebuf = NULL;
  size_t		linelen = 0; 
  struct ifreq		ifr;

  /* Open Debian config file */
  stream = fopen(DEBIAN_CONFIG_FILE, "r");
  if(stream == NULL)
    {
      fprintf(stderr, "Error: can't open file [%s]\n", DEBIAN_CONFIG_FILE);
      return;
    }

  /* Read each line of file
   * getline is a GNU extension :-( The buffer is recycled and increased
   * as needed by getline. */
  while(getline(&linebuf, &linelen, stream) > 0)
    {
      char *			p;
      char *			e;
      size_t			n;

      /* Check for auto keyword, ignore when commented out */
      if(!strncasecmp(linebuf, "auto ", 5))
	{
	  /* Skip "auto" keyword */
	  p = linebuf + 5;

	  /* Terminate at first comment */
	  e = strchr(p, '#');
	  if(e != NULL)
	    *e = '\0';

	  /* Loop on all interfaces given */
	  while(*p != '\0')
	    {
	      /* Interface name length */
	      n = strcspn(p, " \t\n");

	      /* Look for end of interface name */
	      e = p + n;
	      /* Make 'e' point past the '\0' we are going to add */
	      if(*e != '\0')
		e++;
	      /* Terminate interface name */
	      p[n] = '\0';

	      if(verbose)
		fprintf(stderr, "Probing : Trying to load interface [%s]\n",
			p);

	      /* Load interface */
	      strncpy(ifr.ifr_name, p, IFNAMSIZ);
	      ioctl(skfd, SIOCGIFHWADDR, &ifr);

	      /* Go to next interface name */
	      p = e;
	      p += strspn(p, " \t\n"); 
	    }
	}
    }

  /* Done */
  fclose(stream);
  return;
}

/**************************** MAIN LOGIC ****************************/

/*------------------------------------------------------------------*/
/*
 * Rename an interface to a specified new name.
 */
static int
process_rename(int	skfd,
	       char *	ifname,
	       char *	newname)
{
  char		retname[IFNAMSIZ+1];
  int		len;
  char *	star;

  len = strlen(newname);
  star = strchr(newname, '*');

  /* Check newname length, need one extra char for wildcard */
  if((len + (star != NULL)) > IFNAMSIZ)
    {
      fprintf(stderr, "Error: Interface name `%s' too long.\n",
	      newname);  
      return(-1);
    }

  /* Change the name of the interface */
  if(if_set_name(skfd, ifname, newname, retname) < 0)
    {
      fprintf(stderr, "Error: cannot change name of %s to %s: %s\n",
	      ifname, newname, strerror(errno)); 
      return(-1);
    }

  /* Always print out the *new* interface name so that
   * the calling script can pick it up and know where its interface
   * has gone. */
  printf("%s\n", retname);

  /* Done */
  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Process a specified interface.
 */
static int
process_ifname(int	skfd,
	       char *	ifname,
	       char *	args[],
	       int	count)
{
  struct if_mapping *		target;
  const struct if_mapping *	mapping;
  char				retname[IFNAMSIZ+1];

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Get description of this interface */
  target = mapping_extract(skfd, ifname);
  if(target == NULL)
    return(-1);

  /* If udev is calling us, get the real devpath. */
  if(udev_output)
    {
      const char *env;
      /* It's passed to us as an environment variable */
      env = getenv("DEVPATH");
      if(env)
	{
	  int	env_len = strlen(env);
	  target->sysfs_devplen = env_len;
	  /* Make enough space for new interface name */
	  target->sysfs_devpath = malloc(env_len + IFNAMSIZ + 1);
	  if(target->sysfs_devpath != NULL)
	    memcpy(target->sysfs_devpath, env, env_len + 1);
	}
      /* We will get a second chance is the user has some sysfs selectors */
    }

  /* Find matching mapping */
  mapping = mapping_find(target);
  if(mapping == NULL)
    return(-1);

  /* If user specified a new name, keep only interfaces that would
   * match the new name... */
  if((new_name != NULL) && (if_match_ifname(mapping->ifname, new_name) != 0))
    return(-1);

  /* Check if user want only dry-run.
   * Note that, in the case of wildcard, we don't resolve the wildcard.
   * That would be tricky to do... */
  if(dry_run)
    {
      strcpy(retname, mapping->ifname);
      fprintf(stderr, "Dry-run : Would rename %s to %s.\n",
	      target->ifname, mapping->ifname);
    }
  else
    {
      /* Change the name of the interface */
      if(if_set_name(skfd, target->ifname, mapping->ifname, retname) < 0)
	{
	  fprintf(stderr, "Error: cannot change name of %s to %s: %s\n",
		  target->ifname, mapping->ifname, strerror(errno)); 
	  return(-1);
	}
    }

  /* Check if called with an explicit interface name */
  if(print_newname)
    {
      if(!udev_output)
	/* Always print out the *new* interface name so that
	 * the calling script can pick it up and know where its interface
	 * has gone. */
	printf("%s\n", retname);
      else
	/* udev likes to call us as an IMPORT action. This means that
	 * we need to return udev the environment variables changed.
	 * Obviously, we don't want to return anything is nothing changed. */
	if(strcmp(target->ifname, retname))
	  {
	    char *	pos;
	    /* Hack */
	    if(!target->sysfs_devpath)
	      mapping_getsysfs(skfd, ifname, target, 0);
	    /* Update devpath. Size is large enough. */
	    pos = strrchr(target->sysfs_devpath, '/');
	    if((pos != NULL) && (!strcmp(target->ifname, pos + 1)))
	      strcpy(pos + 1, retname);
	    /* Return new environment variables */
	    printf("DEVPATH=%s\nINTERFACE=%s\nINTERFACE_OLD=%s\n",
		   target->sysfs_devpath, retname, target->ifname);
	  }
    }

  /* Done */
  return(0);
}

/*------------------------------------------------------------------*/
/*
 * Process all network interface present on the system.
 */
static inline int
process_iflist(int	skfd,
	       char *	args[],
	       int	count)
{
  num_takeover = 0;

  /* Just do it */
  iw_enum_devices(skfd, &process_ifname, args, count);

  /* If we do any takeover, the interface list grabbed with
   * iw_enum_devices() may get out of sync with the real interfaces,
   * and we may miss the victim interface. So, let's go through the
   * list again.
   * On the other hand, we may have ping pong between two interfaces,
   * each claiming the same name, so let's not do it forever...
   * Two time should be enough for most configs...
   * Jean II */
  if(force_takeover && num_takeover)
    /* Play it again, Sam... */
    iw_enum_devices(skfd, &process_ifname, args, count);

  /* Done */
  return(0);
}

/******************************* MAIN *******************************/


/*------------------------------------------------------------------*/
/*
 */
static void
usage(void)
{
  fprintf(stderr, "usage: ifrename [-c configurationfile] [-i ifname] [-p] [-t] [-d] [-D]\n");
  exit(1); 
}

/*------------------------------------------------------------------*/
/*
 * The main !
 */
int
main(int	argc,
     char *	argv[]) 
{
  const char *	conf_file = DEFAULT_CONF;
  char *	ifname = NULL;
  int		use_probe = 0;
  int		is_debian = 0;
  int		skfd;
  int		ret;

  /* Loop over all command line options */
  while(1)
    {
      int c = getopt_long(argc, argv, "c:dDi:n:ptuvV", long_opt, NULL);
      if(c == -1)
	break;

      switch(c)
	{ 
	default:
	case '?':
	  usage(); 
	case 'c':
	  conf_file = optarg;
	  break;
	case 'd':
	  is_debian = 1;
	  break;
	case 'D':
	  dry_run = 1;
	  break;
	case 'i':
	  ifname = optarg;
	  break;
	case 'n':
	  new_name = optarg;
	  break;
	case 'p':
	  use_probe = 1;
	  break;
	case 't':
	  force_takeover = 1;
	  break;
	case 'u':
	  udev_output = 1;
	  break;
	case 'v':
	  printf("%-8.16s  Wireless-Tools version %d\n", "ifrename", WT_VERSION);
	  return(0);
	case 'V':
	  verbose = 1;
	  break;
	}
    }

  /* Read the specified/default config file, or stdin. */
  if(mapping_readfile(conf_file) < 0)
    return(-1);

  /* Create a channel to the NET kernel. */
  if((skfd = iw_sockets_open()) < 0)
    {
      perror("socket");
      return(-1);
    }

  /* Check if interface name was specified with -i. */
  if(ifname != NULL)
    {
      /* Check is target name specified */
      if(new_name != NULL)
	{
	  /* User want to simply rename an interface to a specified name */
	  ret = process_rename(skfd, ifname, new_name);
	}
      else
	{
	  /* Rename only this interface based on mappings
	   * Mostly used for HotPlug processing (from /etc/hotplug/net.agent)
	   * or udev processing (from a udev IMPORT rule).
	   * Process the network interface specified on the command line,
	   * and return the new name on stdout.
	   */
	  print_newname = 1;
	  ret = process_ifname(skfd, ifname, NULL, 0);
	}
    }
  else
    {
      /* Load all the necesary modules */
      if(use_probe)
	{
	  if(is_debian)
	    probe_debian(skfd);
	  else
	    probe_mappings(skfd);
	}

      /* Rename all system interfaces
       * Mostly used for boot time processing (from init scripts).
       */
      ret = process_iflist(skfd, NULL, 0);
    }

  /* Cleanup */
  iw_sockets_close(skfd);
  return(ret);
} 

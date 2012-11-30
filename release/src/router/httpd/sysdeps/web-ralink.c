/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * ASUS Home Gateway Reference Design
 * Web Page Configuration Support Routines
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 */

#ifdef WEBS
#include <webs.h>
#include <uemf.h>
#include <ej.h>
#else /* !WEBS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <httpd.h>
#endif /* WEBS */
#include <typedefs.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <shutils.h>
#include <ralink.h>
#include <iwlib.h>
#include <stapriv.h>
#include <ethutils.h>
#include <semaphore_mfp.h>
#include <shared.h>
#include <sys/mman.h>
#ifndef O_BINARY
#define O_BINARY 	0
#endif
#include <image.h>
#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif

#define wan_prefix(unit, prefix)	snprintf(prefix, sizeof(prefix), "wan%d_", unit)
//static char * rfctime(const time_t *timep);
//static char * reltime(unsigned int seconds);
void reltime(unsigned int seconds, char *buf);

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/klog.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <net/if_arp.h>

#include <dirent.h>

int is_hwnat_loaded()
{
	DIR *dir_to_open = NULL;
        
	dir_to_open = opendir("/sys/module/hw_nat");
	if (dir_to_open)
	{
		closedir(dir_to_open);
		return 1;
	}
		return 0;
}

/* Dump NAT table <tr><td>destination</td><td>MAC</td><td>IP</td><td>expires</td></tr> format */
int
ej_nat_table(int eid, webs_t wp, int argc, char_t **argv)
{
#ifdef REMOVE
    	netconf_nat_t *nat_list = 0;
//	netconf_nat_t **plist, *cur;
#endif
	// value need to init
	int ret = 0;

	if (nvram_match("wan_nat_x", "1"))
	{
#ifndef RTCONFIG_DSL
		ret += websWrite(wp, "Hardware NAT: %s\n", is_hwnat_loaded() ? "Enabled": "Disabled");
#endif		
		ret += websWrite(wp, "Software QoS: %s\n", nvram_match("qos_enable", "1") ? "Enabled": "Disabled");
	}

	ret += websWrite(wp, "Destination     Proto.  Port Range  Redirect to\n");

#ifdef REMOVE
    	netconf_get_nat(NULL, &needlen);

    	if (needlen > 0) 
	{

		nat_list = (netconf_nat_t *) malloc(needlen);
		if (nat_list) {
	    		memset(nat_list, 0, needlen);
	    		listlen = needlen;
	    		if (netconf_get_nat(nat_list, &listlen) == 0 && needlen == listlen) {
				listlen = needlen/sizeof(netconf_nat_t);

				for (i=0;i<listlen;i++)
				{				
				//printf("%d %d %d\n", nat_list[i].target,
				//		nat_list[i].match.ipproto,
				//		nat_list[i].match.dst.ipaddr.s_addr);	
				if (nat_list[i].target==NETCONF_DNAT)
				{
					if (nat_list[i].match.dst.ipaddr.s_addr==0)
					{
						sprintf(line, "%-15s", "all");
					}
					else
					{
						sprintf(line, "%-15s", inet_ntoa(nat_list[i].match.dst.ipaddr));
					}


					if (ntohs(nat_list[i].match.dst.ports[0])==0)	
						sprintf(line, "%s %-7s", line, "ALL");
					else if (nat_list[i].match.ipproto==IPPROTO_TCP)
						sprintf(line, "%s %-7s", line, "TCP");
					else sprintf(line, "%s %-7s", line, "UDP");

					if (nat_list[i].match.dst.ports[0] == nat_list[i].match.dst.ports[1])
					{
						if (ntohs(nat_list[i].match.dst.ports[0])==0)	
						sprintf(line, "%s %-11s", line, "ALL");
						else
						sprintf(line, "%s %-11d", line, ntohs(nat_list[i].match.dst.ports[0]));
					}
					else 
					{
						sprintf(tstr, "%d:%d", ntohs(nat_list[i].match.dst.ports[0]),
						ntohs(nat_list[i].match.dst.ports[1]));
						sprintf(line, "%s %-11s", line, tstr);					
					}	
					sprintf(line, "%s %s\n", line, inet_ntoa(nat_list[i].ipaddr));
					ret += websWrite(wp, line);
				
				}
				}
	    		}
	    		free(nat_list);
		}
    	}
#endif
	return ret;
}


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

/* Disable runtime version warning in ralink_get_range_info() */
int	iw_ignore_version_sp = 0;

/*------------------------------------------------------------------*/
/*
 * Get the range information out of the driver
 */
int
ralink_get_range_info(iwrange *	range, char* buffer, int length)
{
  union iw_range_raw *	range_raw;

  /* Point to the buffer */
  range_raw = (union iw_range_raw *) buffer;

  /* For new versions, we can check the version directly, for old versions
   * we use magic. 300 bytes is a also magic number, don't touch... */
  if (length < 300)
    {
      /* That's v10 or earlier. Ouch ! Let's make a guess...*/
      range_raw->range.we_version_compiled = 9;
    }

  /* Check how it needs to be processed */
  if (range_raw->range.we_version_compiled > 15)
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
  if (!iw_ignore_version_sp)
    {
      /* We don't like very old version (unfortunately kernel 2.2.X) */
      if (range->we_version_compiled <= 10)
	{
	  fprintf(stderr, "Warning: Driver for device %s has been compiled with an ancient version\n", "raxx");
	  fprintf(stderr, "of Wireless Extension, while this program support version 11 and later.\n");
	  fprintf(stderr, "Some things may be broken...\n\n");
	}

      /* We don't like future versions of WE, because we can't cope with
       * the unknown */
      if (range->we_version_compiled > WE_MAX_VERSION)
	{
	  fprintf(stderr, "Warning: Driver for device %s has been compiled with version %d\n", "raxx", range->we_version_compiled);
	  fprintf(stderr, "of Wireless Extension, while this program supports up to version %d.\n", WE_VERSION);
	  fprintf(stderr, "Some things may be broken...\n\n");
	}

      /* Driver version verification */
      if ((range->we_version_compiled > 10) &&
	 (range->we_version_compiled < range->we_version_source))
	{
	  fprintf(stderr, "Warning: Driver for device %s recommend version %d of Wireless Extension,\n", "raxx", range->we_version_source);
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
  iw_ignore_version_sp = 1;

  return (0);
}

#define RTPRIV_IOCTL_SHOW		SIOCIWFIRSTPRIV + 0x11
#define RTPRIV_IOCTL_GET_MAC_TABLE	SIOCIWFIRSTPRIV + 0x0F


char* GetBW(int BW)
{
	switch(BW)
	{
		case BW_10:
			return "10M";

		case BW_20:
			return "20M";

		case BW_40:
			return "40M";

		default:
			return "N/A";
	}
}

char* GetPhyMode(int Mode)
{
	switch(Mode)
	{
		case MODE_CCK:
			return "CCK";

		case MODE_OFDM:
			return "OFDM";
		case MODE_HTMIX:
			return "HTMIX";

		case MODE_HTGREENFIELD:
			return "GREEN";
		default:
			return "N/A";
	}
}

int MCSMappingRateTable[] =
	{2,  4,   11,  22, // CCK
	12, 18,   24,  36, 48, 72, 96, 108, // OFDM
	13, 26,   39,  52,  78, 104, 117, 130, 26,  52,  78, 104, 156, 208, 234, 260, // 20MHz, 800ns GI, MCS: 0 ~ 15
	39, 78,  117, 156, 234, 312, 351, 390,										  // 20MHz, 800ns GI, MCS: 16 ~ 23
	27, 54,   81, 108, 162, 216, 243, 270, 54, 108, 162, 216, 324, 432, 486, 540, // 40MHz, 800ns GI, MCS: 0 ~ 15
	81, 162, 243, 324, 486, 648, 729, 810,										  // 40MHz, 800ns GI, MCS: 16 ~ 23
	14, 29,   43,  57,  87, 115, 130, 144, 29, 59,   87, 115, 173, 230, 260, 288, // 20MHz, 400ns GI, MCS: 0 ~ 15
	43, 87,  130, 173, 260, 317, 390, 433,										  // 20MHz, 400ns GI, MCS: 16 ~ 23
	30, 60,   90, 120, 180, 240, 270, 300, 60, 120, 180, 240, 360, 480, 540, 600, // 40MHz, 400ns GI, MCS: 0 ~ 15
	90, 180, 270, 360, 540, 720, 810, 900};

int
getRate(MACHTTRANSMIT_SETTING HTSetting)
{
	int rate_count = sizeof(MCSMappingRateTable)/sizeof(int);
	int rate_index = 0;  

    if (HTSetting.field.MODE >= MODE_HTMIX)
    {
    	rate_index = 12 + ((unsigned char)HTSetting.field.BW *24) + ((unsigned char)HTSetting.field.ShortGI *48) + ((unsigned char)HTSetting.field.MCS);
    }
    else 
    if (HTSetting.field.MODE == MODE_OFDM)                
    	rate_index = (unsigned char)(HTSetting.field.MCS) + 4;
    else if (HTSetting.field.MODE == MODE_CCK)   
    	rate_index = (unsigned char)(HTSetting.field.MCS);

    if (rate_index < 0)
        rate_index = 0;
    
    if (rate_index > rate_count)
        rate_index = rate_count;

	return (MCSMappingRateTable[rate_index] * 5)/10;
}

int
getRate_2g(MACHTTRANSMIT_SETTING_2G HTSetting)
{
	int rate_count = sizeof(MCSMappingRateTable)/sizeof(int);
	int rate_index = 0;  

    if (HTSetting.field.MODE >= MODE_HTMIX)
    {
    	rate_index = 12 + ((unsigned char)HTSetting.field.BW *24) + ((unsigned char)HTSetting.field.ShortGI *48) + ((unsigned char)HTSetting.field.MCS);
    }
    else 
    if (HTSetting.field.MODE == MODE_OFDM)                
    	rate_index = (unsigned char)(HTSetting.field.MCS) + 4;
    else if (HTSetting.field.MODE == MODE_CCK)   
    	rate_index = (unsigned char)(HTSetting.field.MCS);

    if (rate_index < 0)
        rate_index = 0;
    
    if (rate_index > rate_count)
        rate_index = rate_count;

	return (MCSMappingRateTable[rate_index] * 5)/10;
}

int
ej_wl_status(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int retval = 0;
	int ii = 0;
	char word[256], *next;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		retval += wl_status(eid, wp, argc, argv, ii, word);
		retval += websWrite(wp, "\n");

		ii++;
	}

	return retval;
}

int
ej_wl_status_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	return ej_wl_status(eid, wp, argc, argv, 0);
}

int
wl_status(int eid, webs_t wp, int argc, char_t **argv, int unit, const char *ifname)
{
	int ret = 0;
	int channel;
	struct iw_range	range;
	double freq;
	struct iwreq wrq0;
	struct iwreq wrq1;
	struct iwreq wrq2;
	struct iwreq wrq3;
	unsigned long phy_mode;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	int wl_mode_x;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

#if 0
	if (nvram_match(strcat_r(prefix, "radio", tmp), "0"))
	{
		ret+=websWrite(wp, "Radio is disabled\n");
		return ret;
	}
#else
	if (!get_radio_status(nvram_safe_get(strcat_r(prefix, "ifname", tmp))))
	{
		ret+=websWrite(wp, "Radio is disabled\n");
		return ret;
	}
#endif

	if (wl_ioctl(ifname, SIOCGIWAP, &wrq0) < 0)
	{
		ret+=websWrite(wp, "Radio is disabled\n");
		return ret;
	}

	wrq0.u.ap_addr.sa_family = ARPHRD_ETHER;
	ret+=websWrite(wp, "MAC address	: %02X:%02X:%02X:%02X:%02X:%02X\n",
			(unsigned char)wrq0.u.ap_addr.sa_data[0],
			(unsigned char)wrq0.u.ap_addr.sa_data[1],
			(unsigned char)wrq0.u.ap_addr.sa_data[2],
			(unsigned char)wrq0.u.ap_addr.sa_data[3],
			(unsigned char)wrq0.u.ap_addr.sa_data[4],
			(unsigned char)wrq0.u.ap_addr.sa_data[5]);

	if (wl_ioctl(ifname, SIOCGIWFREQ, &wrq1) < 0)
		return ret;

	char buffer[sizeof(iwrange) * 2];
	bzero(buffer, sizeof(buffer));
	wrq2.u.data.pointer = (caddr_t) buffer;
	wrq2.u.data.length = sizeof(buffer);
	wrq2.u.data.flags = 0;

	if (wl_ioctl(ifname, SIOCGIWRANGE, &wrq2) < 0)
		return ret;

	if (ralink_get_range_info(&range, buffer, wrq2.u.data.length) < 0)
		return ret;

	bzero(buffer, sizeof(unsigned long));
	wrq2.u.data.length = sizeof(unsigned long);
	wrq2.u.data.pointer = (caddr_t) buffer;
	wrq2.u.data.flags = RT_OID_GET_PHY_MODE;

	if (wl_ioctl(ifname, RT_PRIV_IOCTL, &wrq2) < 0)
		return ret;
	else
		phy_mode=wrq2.u.mode;

	freq = iw_freq2float(&(wrq1.u.freq));
	if (freq < KILO)
		channel = (int) freq;
	else
	{
		channel = iw_freq_to_channel(freq, &range);
		if (channel < 0)
			return ret;
	}

	wl_mode_x = nvram_get_int(strcat_r(prefix, "mode_x", tmp));
	if      (wl_mode_x == 1)
		ret+=websWrite(wp, "OP Mode		: WDS Only\n");
	else if (wl_mode_x == 2)
		ret+=websWrite(wp, "OP Mode		: Hybrid\n");
	else
		ret+=websWrite(wp, "OP Mode		: AP\n");

	if (phy_mode==PHY_11BG_MIXED)
		ret+=websWrite(wp, "Phy Mode	: 11b/g\n");
	else if (phy_mode==PHY_11B)
		ret+=websWrite(wp, "Phy Mode	: 11b\n");
	else if (phy_mode==PHY_11A)
		ret+=websWrite(wp, "Phy Mode	: 11a\n");
	else if (phy_mode==PHY_11ABG_MIXED)
		ret+=websWrite(wp, "Phy Mode	: 11a/b/g\n");
	else if (phy_mode==PHY_11G)
		ret+=websWrite(wp, "Phy Mode	: 11g\n");
	else if (phy_mode==PHY_11ABGN_MIXED)
		ret+=websWrite(wp, "Phy Mode	: 11a/b/g/n\n");
	else if (phy_mode==PHY_11N)
		ret+=websWrite(wp, "Phy Mode	: 11n\n");
	else if (phy_mode==PHY_11GN_MIXED)
		ret+=websWrite(wp, "Phy Mode	: 11g/n\n");
	else if (phy_mode==PHY_11AN_MIXED)
		ret+=websWrite(wp, "Phy Mode	: 11a/n\n");
	else if (phy_mode==PHY_11BGN_MIXED)
		ret+=websWrite(wp, "Phy Mode	: 11b/g/n\n");
	else if (phy_mode==PHY_11AGN_MIXED)
		ret+=websWrite(wp, "Phy Mode	: 11a/g/n\n");

	ret+=websWrite(wp, "Channel		: %d\n", channel);

	char data[16384];
	memset(data, 0, sizeof(data));
	wrq3.u.data.pointer = data;
	wrq3.u.data.length = sizeof(data);
	wrq3.u.data.flags = 0;

	if (wl_ioctl(ifname, RTPRIV_IOCTL_GET_MAC_TABLE, &wrq3) < 0)
		return ret;

	RT_802_11_MAC_TABLE* mp=(RT_802_11_MAC_TABLE*)wrq3.u.data.pointer;
	int i;

	ret+=websWrite(wp, "\nStations List			   \n");
	ret+=websWrite(wp, "----------------------------------------\n");
	ret+=websWrite(wp, "%-18s%-4s%-8s%-4s%-4s%-4s%-5s%-5s%-12s\n",
			   "MAC", "PSM", "PhyMode", "BW", "MCS", "SGI", "STBC", "Rate", "Connect Time");

	int hr, min, sec;
	if (!strcmp(ifname, WIF_5G))
	for (i=0;i<mp->Num;i++)
	{
                hr = mp->Entry[i].ConnectedTime/3600;
                min = (mp->Entry[i].ConnectedTime % 3600)/60;
                sec = mp->Entry[i].ConnectedTime - hr*3600 - min*60;

		ret+=websWrite(wp, "%02X:%02X:%02X:%02X:%02X:%02X %s %-7s %s %-03d %s %s  %-03dM %02d:%02d:%02d\n",
				mp->Entry[i].Addr[0], mp->Entry[i].Addr[1],
				mp->Entry[i].Addr[2], mp->Entry[i].Addr[3],
				mp->Entry[i].Addr[4], mp->Entry[i].Addr[5],
				mp->Entry[i].Psm ? "Yes" : "NO ",
				GetPhyMode(mp->Entry[i].TxRate.field.MODE),
				GetBW(mp->Entry[i].TxRate.field.BW),
				mp->Entry[i].TxRate.field.MCS,
				mp->Entry[i].TxRate.field.ShortGI ? "Yes" : "NO ",
				mp->Entry[i].TxRate.field.STBC ? "Yes" : "NO ",
				getRate(mp->Entry[i].TxRate),
				hr, min, sec
		);
	}
	else
	for (i=0;i<mp_2g->Num;i++)
	{
                hr = mp_2g->Entry[i].ConnectedTime/3600;
                min = (mp_2g->Entry[i].ConnectedTime % 3600)/60;
                sec = mp_2g->Entry[i].ConnectedTime - hr*3600 - min*60;

		ret+=websWrite(wp, "%02X:%02X:%02X:%02X:%02X:%02X %s %-7s %s %-03d %s %s  %-03dM %02d:%02d:%02d\n",
				mp_2g->Entry[i].Addr[0], mp_2g->Entry[i].Addr[1],
				mp_2g->Entry[i].Addr[2], mp_2g->Entry[i].Addr[3],
				mp_2g->Entry[i].Addr[4], mp_2g->Entry[i].Addr[5],
				mp_2g->Entry[i].Psm ? "Yes" : "NO ",
				GetPhyMode(mp_2g->Entry[i].TxRate.field.MODE),
				GetBW(mp_2g->Entry[i].TxRate.field.BW),
				mp_2g->Entry[i].TxRate.field.MCS,
				mp_2g->Entry[i].TxRate.field.ShortGI ? "Yes" : "NO ",
				mp_2g->Entry[i].TxRate.field.STBC ? "Yes" : "NO ",
				getRate_2g(mp_2g->Entry[i].TxRate),
				hr, min, sec
		);
	}

	return ret;
}

typedef struct PACKED _WSC_CONFIGURED_VALUE {
    unsigned short WscConfigured;	// 1 un-configured; 2 configured
    unsigned char WscSsid[32 + 1];
    unsigned short WscAuthMode;		// mandatory, 0x01: open, 0x02: wpa-psk, 0x04: shared, 0x08:wpa, 0x10: wpa2, 0x
    unsigned short WscEncrypType;	// 0x01: none, 0x02: wep, 0x04: tkip, 0x08: aes
    unsigned char DefaultKeyIdx;
    unsigned char WscWPAKey[64 + 1];
} WSC_CONFIGURED_VALUE;

void getWPSAuthMode(WSC_CONFIGURED_VALUE *result, char *ret_str)
{
	if (result->WscAuthMode & 0x1)
		strcat(ret_str, "Open System");
	if (result->WscAuthMode & 0x2)
		strcat(ret_str, "WPA-Personal");
	if (result->WscAuthMode & 0x4)
		strcat(ret_str, "Shared Key");
	if (result->WscAuthMode & 0x8)
		strcat(ret_str, "WPA-Enterprise");
	if (result->WscAuthMode & 0x10)
		strcat(ret_str, "WPA2-Enterprise");
	if (result->WscAuthMode & 0x20)
		strcat(ret_str, "WPA2-Personal");
}

void getWPSEncrypType(WSC_CONFIGURED_VALUE *result, char *ret_str)
{
	if (result->WscEncrypType & 0x1)
		strcat(ret_str, "None");
	if (result->WscEncrypType & 0x2)
		strcat(ret_str, "WEP");
	if (result->WscEncrypType & 0x4)
		strcat(ret_str, "TKIP");
	if (result->WscEncrypType & 0x8)
		strcat(ret_str, "AES");
}

/*
 * these definitions are from rt2860v2 driver include/wsc.h 
 */
char *getWscStatusStr(int status)
{
	switch(status) {
	case 0:
		return "Not used";
	case 1:
		return "Idle";
	case 2:
#if 0
		return "WPS Fail(Ignore this if Intel/Marvell registrar used)";
#else
		return "Idle";
#endif
	case 3:
		return "Start WPS Process";
	case 4:
		return "Received EAPOL-Start";
	case 5:
		return "Sending EAP-Req(ID)";
	case 6:
		return "Receive EAP-Rsp(ID)";
	case 7:
		return "Receive EAP-Req with wrong WPS SMI Vendor Id";
	case 8:
		return "Receive EAP-Req with wrong WPS Vendor Type";
	case 9:
		return "Sending EAP-Req(WPS_START)";
	case 10:
		return "Send M1";
	case 11:
		return "Received M1";
	case 12:
		return "Send M2";
	case 13:
		return "Received M2";
	case 14:
		return "Received M2D";
	case 15:
		return "Send M3";
	case 16:
		return "Received M3";
	case 17:
		return "Send M4";
	case 18:
		return "Received M4";
	case 19:
		return "Send M5";
	case 20:
		return "Received M5";
	case 21:
		return "Send M6";
	case 22:
		return "Received M6";
	case 23:
		return "Send M7";
	case 24:
		return "Received M7";
	case 25:
		return "Send M8";
	case 26:
		return "Received M8";
	case 27:
		return "Processing EAP Response (ACK)";
	case 28:
		return "Processing EAP Request (Done)";
	case 29:
		return "Processing EAP Response (Done)";
	case 30:
		return "Sending EAP-Fail";
	case 31:
		return "WPS_ERROR_HASH_FAIL";
	case 32:
		return "WPS_ERROR_HMAC_FAIL";
	case 33:
		return "WPS_ERROR_DEV_PWD_AUTH_FAIL";
	case 34:
//		return "Configured";
		return "Success";
	case 35:
		return "SCAN AP";
	case 36:
		return "EAPOL START SENT";
	case 37:
		return "WPS_EAP_RSP_DONE_SENT";
	case 38:
		return "WAIT PINCODE";
	case 39:
		return "WSC_START_ASSOC";
	case 0x101:
		return "PBC:TOO MANY AP";
	case 0x102:
		return "PBC:NO AP";
	case 0x103:
		return "EAP_FAIL_RECEIVED";
	case 0x104:
		return "EAP_NONCE_MISMATCH";
	case 0x105:
		return "EAP_INVALID_DATA";
	case 0x106:
		return "PASSWORD_MISMATCH";
	case 0x107:
		return "EAP_REQ_WRONG_SMI";
	case 0x108:
		return "EAP_REQ_WRONG_VENDOR_TYPE";
	case 0x109:
		return "PBC_SESSION_OVERLAP";
	default:
		return "Unknown";
	}
}

int getWscStatus(int unit)
{
	int data = 0;
	struct iwreq wrq;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	wrq.u.data.length = sizeof(data);
	wrq.u.data.pointer = (caddr_t) &data;
	wrq.u.data.flags = RT_OID_WSC_QUERY_STATUS;

	if (wl_ioctl(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), RT_PRIV_IOCTL, &wrq) < 0)
		fprintf(stderr, "errors in getting WSC status\n");

	return data;
}

unsigned int getAPPIN(int unit)
{
	unsigned int data = 0;
	struct iwreq wrq;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	wrq.u.data.length = sizeof(data);
	wrq.u.data.pointer = (caddr_t) &data;
	wrq.u.data.flags = RT_OID_WSC_PIN_CODE;

	if (wl_ioctl(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), RT_PRIV_IOCTL, &wrq) < 0)
		fprintf(stderr, "errors in getting AP PIN\n");

	return data;
}

int
wl_wps_info(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int i;
	char tmpstr[128], tmpstr2[256];
	WSC_CONFIGURED_VALUE result;
	int retval=0;
	struct iwreq wrq;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *wps_sta_pin;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	wrq.u.data.length = sizeof(WSC_CONFIGURED_VALUE);
	wrq.u.data.pointer = (caddr_t) &result;
	wrq.u.data.flags = 0;
	strcpy((char *)&result, "get_wsc_profile");

	if (wl_ioctl(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), RTPRIV_IOCTL_WSC_PROFILE, &wrq) < 0)
	{
		fprintf(stderr, "errors in getting WSC profile\n");
		return 0;
	}

	retval += websWrite(wp, "<wps>\n");

	//0. WSC Status
	retval += websWrite(wp, "<wps_info>%s</wps_info>\n", getWscStatusStr(getWscStatus(unit)));

	//1. WPSConfigured
	if (result.WscConfigured==2)
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "Yes");
	else
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "No");
	
	//2. WPSSSID
	memset(tmpstr, 0, sizeof(tmpstr));
	char_to_ascii(tmpstr, result.WscSsid);
	retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);

	//3. WPSAuthMode
	memset(tmpstr, 0, sizeof(tmpstr));
	getWPSAuthMode(&result, tmpstr);
	retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);

	//4. EncrypType
	memset(tmpstr, 0, sizeof(tmpstr));
	getWPSEncrypType(&result, tmpstr);
	retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);

	//5. DefaultKeyIdx
	memset(tmpstr, 0, sizeof(tmpstr));
	sprintf(tmpstr, "%d", result.DefaultKeyIdx);
	retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);

	//6. WPAKey
	memset(tmpstr, 0, sizeof(tmpstr));
	for (i=0; i<64; i++)	// WPA key default length is 64 (defined & hardcode in driver) 
	{
		sprintf(tmpstr, "%s%c", tmpstr, result.WscWPAKey[i]);
	}
	if (!strlen(tmpstr))
		retval += websWrite(wp, "<wps_info>None</wps_info>\n");
	else
	{
		char_to_ascii(tmpstr2, tmpstr);
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr2);
	}

	//7. AP PIN Code
	memset(tmpstr, 0, sizeof(tmpstr));
	sprintf(tmpstr, "%d", getAPPIN(unit));
	retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);

	//8. Saved WPAKey
	if (!strlen(nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp))))
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "None");
	else
	{
		char_to_ascii(tmpstr, nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", tmpstr);
	}

	//9. WPS enable?
	if (!strcmp(nvram_safe_get(strcat_r(prefix, "wps_mode", tmp)), "enabled"))
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "None");
	else
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", nvram_safe_get("wps_enable"));

	//A. WPS mode
	wps_sta_pin = nvram_safe_get("wps_sta_pin");
	if (strlen(wps_sta_pin) && strcmp(wps_sta_pin, "00000000"))
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "1");
	else
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "2");

	//B. current auth mode
	if (!strlen(nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp))))
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", "None");
	else
		retval += websWrite(wp, "<wps_info>%s</wps_info>\n", nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp)));

	//C. WPS band
	retval += websWrite(wp, "<wps_info>%d</wps_info>\n", nvram_get_int("wps_band"));

	retval += websWrite(wp, "</wps>");

	return retval;
}

int
ej_wps_info(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_wps_info(eid, wp, argc, argv, 1);
}

int
ej_wps_info_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_wps_info(eid, wp, argc, argv, 0);
}

// Wireless Client List		 /* Start --Alicia, 08.09.23 */
#define RTPRIV_IOCTL_GET_MAC_TABLE	SIOCIWFIRSTPRIV + 0x0F

int ej_wl_auth_list(int eid, webs_t wp, int argc, char_t **argv)
{
	struct iwreq wrq;
	int i, firstRow;
	char data[16384];
	char mac[ETHER_ADDR_STR_LEN];	
	RT_802_11_MAC_TABLE *mp;
	RT_802_11_MAC_TABLE_2G *mp2;
	char *value;
	
	memset(mac, 0, sizeof(mac));
	
	/* query wl for authenticated sta list */
	memset(data, 0, sizeof(data));
	wrq.u.data.pointer = data;
	wrq.u.data.length = sizeof(data);
	wrq.u.data.flags = 0;	
	if (wl_ioctl(WIF_2G, RTPRIV_IOCTL_GET_MAC_TABLE, &wrq) < 0)
		goto exit;

	/* build wireless sta list */
	firstRow = 1;
	mp = (RT_802_11_MAC_TABLE *)wrq.u.data.pointer;
	for (i=0; i<mp->Num; i++)
	{
		if (firstRow == 1)
			firstRow = 0;
		else
			websWrite(wp, ", ");
		websWrite(wp, "[");

		sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
				mp->Entry[i].Addr[0], mp->Entry[i].Addr[1],
				mp->Entry[i].Addr[2], mp->Entry[i].Addr[3],
				mp->Entry[i].Addr[4], mp->Entry[i].Addr[5]);
		websWrite(wp, "\"%s\"", mac);
		value = "YES";
		websWrite(wp, ", \"%s\"", value);
		value = "";
		websWrite(wp, ", \"%s\"", value);
		websWrite(wp, "]");
	}

	/* query wl for authenticated sta list */
	memset(data, 0, sizeof(data));
	wrq.u.data.pointer = data;
	wrq.u.data.length = sizeof(data);
	wrq.u.data.flags = 0;	
	if (wl_ioctl(WIF_5G, RTPRIV_IOCTL_GET_MAC_TABLE, &wrq) < 0)
		goto exit;

	/* build wireless sta list */
	mp2 = (RT_802_11_MAC_TABLE_2G *)wrq.u.data.pointer;
	for (i = 0; i<mp2->Num; i++)
	{
		if (firstRow == 1)
			firstRow = 0;
		else
			websWrite(wp, ", ");
		websWrite(wp, "[");
				
		sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
				mp2->Entry[i].Addr[0], mp2->Entry[i].Addr[1],
				mp2->Entry[i].Addr[2], mp2->Entry[i].Addr[3],
				mp2->Entry[i].Addr[4], mp2->Entry[i].Addr[5]);
		websWrite(wp, "\"%s\"", mac);
		
		value = "YES";
		websWrite(wp, ", \"%s\"", value);
		
		value = "";
		websWrite(wp, ", \"%s\"", value);
		
		websWrite(wp, "]");
	}

	/* error/exit */
exit:
	return 0;
}

static int wl_scan(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int retval = 0, i = 0, apCount = 0;
	char data[8192];
	char ssid_str[256];
	char header[128];
	struct iwreq wrq;
	SSA *ssap;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	memset(data, 0x00, 255);
	strcpy(data, "SiteSurvey=1"); 
	wrq.u.data.length = strlen(data)+1; 
	wrq.u.data.pointer = data; 
	wrq.u.data.flags = 0; 

	spinlock_lock(SPINLOCK_SiteSurvey);
	if (wl_ioctl(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), RTPRIV_IOCTL_SET, &wrq) < 0)
	{
		spinlock_unlock(0);
		dbg("Site Survey fails\n");
		return 0;
	}
	spinlock_unlock(SPINLOCK_SiteSurvey);
	dbg("Please wait");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".\n\n");
	memset(data, 0, 8192);
	strcpy(data, "");
	wrq.u.data.length = 8192;
	wrq.u.data.pointer = data;
	wrq.u.data.flags = 0;
	if (wl_ioctl(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), RTPRIV_IOCTL_GSITESURVEY, &wrq) < 0)
	{
		dbg("errors in getting site survey result\n");
		return 0;
	}
	memset(header, 0, sizeof(header));
	//sprintf(header, "%-3s%-33s%-18s%-8s%-15s%-9s%-8s%-2s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode", "NT");
	sprintf(header, "%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode");
	dbg("\n%s", header);
	if (wrq.u.data.length > 0)
	{
		ssap=(SSA *)(wrq.u.data.pointer+strlen(header)+1);
		int len = strlen(wrq.u.data.pointer+strlen(header))-1;
		char *sp, *op;
 		op = sp = wrq.u.data.pointer+strlen(header)+1;
		while (*sp && ((len - (sp-op)) >= 0))
		{
			ssap->SiteSurvey[i].channel[3] = '\0';
			ssap->SiteSurvey[i].ssid[32] = '\0';
			ssap->SiteSurvey[i].bssid[17] = '\0';
			ssap->SiteSurvey[i].encryption[8] = '\0';
			ssap->SiteSurvey[i].authmode[15] = '\0';
			ssap->SiteSurvey[i].signal[8] = '\0';
			ssap->SiteSurvey[i].wmode[7] = '\0';
			sp+=strlen(header);
			apCount=++i;
		}
		if (apCount)
		{
			retval += websWrite(wp, "[");
			for (i = 0; i < apCount; i++)
			{
				dbg("%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n",
					ssap->SiteSurvey[i].channel,
					(char*)ssap->SiteSurvey[i].ssid,
					ssap->SiteSurvey[i].bssid,
					ssap->SiteSurvey[i].encryption,
					ssap->SiteSurvey[i].authmode,
					ssap->SiteSurvey[i].signal,
					ssap->SiteSurvey[i].wmode
				);

				memset(ssid_str, 0, sizeof(ssid_str));
				char_to_ascii(ssid_str, trim_r(ssap->SiteSurvey[i].ssid));

				if (!i)
//					retval += websWrite(wp, "\"%s\"", ssap->SiteSurvey[i].bssid);
					retval += websWrite(wp, "[\"%s\", \"%s\"]", ssid_str, ssap->SiteSurvey[i].bssid);
				else
//					retval += websWrite(wp, ", \"%s\"", ssap->SiteSurvey[i].bssid);
					retval += websWrite(wp, ", [\"%s\", \"%s\"]", ssid_str, ssap->SiteSurvey[i].bssid);
			}
			retval += websWrite(wp, "]");
			dbg("\n");
		}
		else
			retval += websWrite(wp, "[]");
	}
	return retval;
}

int
ej_wl_scan(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_scan(eid, wp, argc, argv, 0);
}

int
ej_wl_scan_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_scan(eid, wp, argc, argv, 0);
}

int
ej_wl_scan_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_scan(eid, wp, argc, argv, 1);
}

unsigned char A_BAND_REGION_0_CHANNEL_LIST[]={36, 40, 44, 48, 149, 153, 157, 161, 165};
unsigned char A_BAND_REGION_1_CHANNEL_LIST[]={36, 40, 44, 48};
#ifdef RTCONFIG_LOCALE2012
unsigned char A_BAND_REGION_2_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165};
unsigned char A_BAND_REGION_3_CHANNEL_LIST[]={52, 56, 60, 64, 149, 153, 157, 161, 165};
#else
unsigned char A_BAND_REGION_2_CHANNEL_LIST[]={36, 40, 44, 48};
unsigned char A_BAND_REGION_3_CHANNEL_LIST[]={149, 153, 157, 161};
#endif
unsigned char A_BAND_REGION_4_CHANNEL_LIST[]={149, 153, 157, 161, 165};
unsigned char A_BAND_REGION_5_CHANNEL_LIST[]={149, 153, 157, 161};
#ifdef RTCONFIG_LOCALE2012
unsigned char A_BAND_REGION_6_CHANNEL_LIST[]={36, 40, 44, 48, 132, 136, 140, 149, 153, 157, 161, 165};
#else
unsigned char A_BAND_REGION_6_CHANNEL_LIST[]={36, 40, 44, 48};
#endif
unsigned char A_BAND_REGION_7_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165, 169, 173};
unsigned char A_BAND_REGION_8_CHANNEL_LIST[]={52, 56, 60, 64};
#ifdef RTCONFIG_LOCALE2012
unsigned char A_BAND_REGION_9_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132};
#else
unsigned char A_BAND_REGION_9_CHANNEL_LIST[]={36, 40, 44, 48};
#endif
unsigned char A_BAND_REGION_10_CHANNEL_LIST[]={36, 40, 44, 48, 149, 153, 157, 161, 165};
unsigned char A_BAND_REGION_11_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 149, 153, 157, 161};
unsigned char A_BAND_REGION_12_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140};
unsigned char A_BAND_REGION_13_CHANNEL_LIST[]={52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161};
unsigned char A_BAND_REGION_14_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 136, 140, 149, 153, 157, 161, 165};
unsigned char A_BAND_REGION_15_CHANNEL_LIST[]={149, 153, 157, 161, 165, 169, 173};
unsigned char A_BAND_REGION_16_CHANNEL_LIST[]={52, 56, 60, 64, 149, 153, 157, 161, 165};
unsigned char A_BAND_REGION_17_CHANNEL_LIST[]={36, 40, 44, 48, 149, 153, 157, 161};
unsigned char A_BAND_REGION_18_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140};
unsigned char A_BAND_REGION_19_CHANNEL_LIST[]={56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161};
unsigned char A_BAND_REGION_20_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 149, 153, 157, 161};
unsigned char A_BAND_REGION_21_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161};

unsigned char G_BAND_REGION_0_CHANNEL_LIST[]={1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
unsigned char G_BAND_REGION_1_CHANNEL_LIST[]={1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
unsigned char G_BAND_REGION_5_CHANNEL_LIST[]={1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

#define A_BAND_REGION_0				0
#define A_BAND_REGION_1				1
#define A_BAND_REGION_2				2
#define A_BAND_REGION_3				3
#define A_BAND_REGION_4				4
#define A_BAND_REGION_5				5
#define A_BAND_REGION_6				6
#define A_BAND_REGION_7				7
#define A_BAND_REGION_8				8
#define A_BAND_REGION_9				9
#define A_BAND_REGION_10			10
#define A_BAND_REGION_11			11
#define A_BAND_REGION_12			12
#define A_BAND_REGION_13			13
#define A_BAND_REGION_14			14
#define A_BAND_REGION_15			15
#define A_BAND_REGION_16			16
#define A_BAND_REGION_17			17
#define A_BAND_REGION_18			18
#define A_BAND_REGION_19			19
#define A_BAND_REGION_20			20
#define A_BAND_REGION_21			21

#define G_BAND_REGION_0				0
#define G_BAND_REGION_1				1
#define G_BAND_REGION_2				2
#define G_BAND_REGION_3				3
#define G_BAND_REGION_4				4
#define G_BAND_REGION_5				5
#define G_BAND_REGION_6				6

typedef struct CountryCodeToCountryRegion {
	unsigned char	IsoName[3];
	unsigned char	RegDomainNum11A;
	unsigned char	RegDomainNum11G;
} COUNTRY_CODE_TO_COUNTRY_REGION;

COUNTRY_CODE_TO_COUNTRY_REGION allCountry[] = {
	/* {Country Number, ISO Name, Country Name, Support 11A, 11A Country Region, Support 11G, 11G Country Region} */
	{"DB", A_BAND_REGION_7, G_BAND_REGION_5},
	{"AL", A_BAND_REGION_0, G_BAND_REGION_1},
	{"DZ", A_BAND_REGION_0, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"AR", A_BAND_REGION_0, G_BAND_REGION_1},
	{"AM", A_BAND_REGION_1, G_BAND_REGION_1},
#else	
	{"AR", A_BAND_REGION_3, G_BAND_REGION_1},
	{"AM", A_BAND_REGION_2, G_BAND_REGION_1},
#endif
	{"AU", A_BAND_REGION_0, G_BAND_REGION_1},
	{"AT", A_BAND_REGION_1, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"AZ", A_BAND_REGION_1, G_BAND_REGION_1},
#else
	{"AZ", A_BAND_REGION_2, G_BAND_REGION_1},
#endif
	{"BH", A_BAND_REGION_0, G_BAND_REGION_1},
	{"BY", A_BAND_REGION_0, G_BAND_REGION_1},
	{"BE", A_BAND_REGION_1, G_BAND_REGION_1},
	{"BZ", A_BAND_REGION_4, G_BAND_REGION_1},
	{"BO", A_BAND_REGION_4, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"BR", A_BAND_REGION_4, G_BAND_REGION_1},
#else
	{"BR", A_BAND_REGION_1, G_BAND_REGION_1},
#endif
	{"BN", A_BAND_REGION_4, G_BAND_REGION_1},
	{"BG", A_BAND_REGION_1, G_BAND_REGION_1},
	{"CA", A_BAND_REGION_0, G_BAND_REGION_0},
	{"CL", A_BAND_REGION_0, G_BAND_REGION_1},
	{"CN", A_BAND_REGION_4, G_BAND_REGION_1},
	{"CO", A_BAND_REGION_0, G_BAND_REGION_0},
	{"CR", A_BAND_REGION_0, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"HR", A_BAND_REGION_1, G_BAND_REGION_1},
#else
	{"HR", A_BAND_REGION_2, G_BAND_REGION_1},
#endif
	{"CY", A_BAND_REGION_1, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"CZ", A_BAND_REGION_1, G_BAND_REGION_1},
#else
	{"CZ", A_BAND_REGION_2, G_BAND_REGION_1},
#endif
	{"DK", A_BAND_REGION_1, G_BAND_REGION_1},
	{"DO", A_BAND_REGION_0, G_BAND_REGION_0},
	{"EC", A_BAND_REGION_0, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"EG", A_BAND_REGION_1, G_BAND_REGION_1},
#else
	{"EG", A_BAND_REGION_2, G_BAND_REGION_1},
#endif
	{"SV", A_BAND_REGION_0, G_BAND_REGION_1},
	{"EE", A_BAND_REGION_1, G_BAND_REGION_1},
	{"FI", A_BAND_REGION_1, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"FR", A_BAND_REGION_1, G_BAND_REGION_1},
	{"GE", A_BAND_REGION_1, G_BAND_REGION_1},
#else
	{"FR", A_BAND_REGION_2, G_BAND_REGION_1},
	{"GE", A_BAND_REGION_2, G_BAND_REGION_1},
#endif
	{"DE", A_BAND_REGION_1, G_BAND_REGION_1},
	{"GR", A_BAND_REGION_1, G_BAND_REGION_1},
	{"GT", A_BAND_REGION_0, G_BAND_REGION_0},
	{"HN", A_BAND_REGION_0, G_BAND_REGION_1},
	{"HK", A_BAND_REGION_0, G_BAND_REGION_1},
	{"HU", A_BAND_REGION_1, G_BAND_REGION_1},
	{"IS", A_BAND_REGION_1, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"IN", A_BAND_REGION_2, G_BAND_REGION_1},
#else
	{"IN", A_BAND_REGION_0, G_BAND_REGION_1},
#endif
	{"ID", A_BAND_REGION_4, G_BAND_REGION_1},
	{"IR", A_BAND_REGION_4, G_BAND_REGION_1},
	{"IE", A_BAND_REGION_1, G_BAND_REGION_1},
	{"IL", A_BAND_REGION_0, G_BAND_REGION_1},
	{"IT", A_BAND_REGION_1, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"JP", A_BAND_REGION_1, G_BAND_REGION_1},
#else
	{"JP", A_BAND_REGION_9, G_BAND_REGION_1},
#endif
	{"JO", A_BAND_REGION_0, G_BAND_REGION_1},
	{"KZ", A_BAND_REGION_0, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"KP", A_BAND_REGION_1, G_BAND_REGION_1},
	{"KR", A_BAND_REGION_1, G_BAND_REGION_1},
#else
	{"KP", A_BAND_REGION_5, G_BAND_REGION_1},
	{"KR", A_BAND_REGION_5, G_BAND_REGION_1},
#endif
	{"KW", A_BAND_REGION_0, G_BAND_REGION_1},
	{"LV", A_BAND_REGION_1, G_BAND_REGION_1},
	{"LB", A_BAND_REGION_0, G_BAND_REGION_1},
	{"LI", A_BAND_REGION_1, G_BAND_REGION_1},
	{"LT", A_BAND_REGION_1, G_BAND_REGION_1},
	{"LU", A_BAND_REGION_1, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"MO", A_BAND_REGION_4, G_BAND_REGION_1},
#else
	{"MO", A_BAND_REGION_0, G_BAND_REGION_1},
#endif
	{"MK", A_BAND_REGION_0, G_BAND_REGION_1},
	{"MY", A_BAND_REGION_0, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"MX", A_BAND_REGION_2, G_BAND_REGION_0},
	{"MC", A_BAND_REGION_1, G_BAND_REGION_1},
#else
	{"MX", A_BAND_REGION_0, G_BAND_REGION_0},
	{"MC", A_BAND_REGION_2, G_BAND_REGION_1},
#endif
	{"MA", A_BAND_REGION_0, G_BAND_REGION_1},
	{"NL", A_BAND_REGION_1, G_BAND_REGION_1},
	{"NZ", A_BAND_REGION_0, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"NO", A_BAND_REGION_1, G_BAND_REGION_0},
#else
	{"NO", A_BAND_REGION_0, G_BAND_REGION_0},
#endif
	{"OM", A_BAND_REGION_0, G_BAND_REGION_1},
	{"PK", A_BAND_REGION_0, G_BAND_REGION_1},
	{"PA", A_BAND_REGION_0, G_BAND_REGION_0},
	{"PE", A_BAND_REGION_4, G_BAND_REGION_1},
	{"PH", A_BAND_REGION_4, G_BAND_REGION_1},
	{"PL", A_BAND_REGION_1, G_BAND_REGION_1},
	{"PT", A_BAND_REGION_1, G_BAND_REGION_1},
	{"PR", A_BAND_REGION_0, G_BAND_REGION_0},
	{"QA", A_BAND_REGION_0, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"RO", A_BAND_REGION_1, G_BAND_REGION_1},
	{"RU", A_BAND_REGION_6, G_BAND_REGION_1},
#else
	{"RO", A_BAND_REGION_0, G_BAND_REGION_1},
	{"RU", A_BAND_REGION_0, G_BAND_REGION_1},
#endif
	{"SA", A_BAND_REGION_0, G_BAND_REGION_1},
	{"SG", A_BAND_REGION_0, G_BAND_REGION_1},
	{"SK", A_BAND_REGION_1, G_BAND_REGION_1},
	{"SI", A_BAND_REGION_1, G_BAND_REGION_1},
	{"ZA", A_BAND_REGION_1, G_BAND_REGION_1},
	{"ES", A_BAND_REGION_1, G_BAND_REGION_1},
	{"SE", A_BAND_REGION_1, G_BAND_REGION_1},
	{"CH", A_BAND_REGION_1, G_BAND_REGION_1},
	{"SY", A_BAND_REGION_0, G_BAND_REGION_1},
	{"TW", A_BAND_REGION_3, G_BAND_REGION_0},
	{"TH", A_BAND_REGION_0, G_BAND_REGION_1},
#ifdef RTCONFIG_LOCALE2012
	{"TT", A_BAND_REGION_1, G_BAND_REGION_1},
	{"TN", A_BAND_REGION_1, G_BAND_REGION_1},
	{"TR", A_BAND_REGION_1, G_BAND_REGION_1},
	{"UA", A_BAND_REGION_9, G_BAND_REGION_1},
#else
	{"TT", A_BAND_REGION_2, G_BAND_REGION_1},
	{"TN", A_BAND_REGION_2, G_BAND_REGION_1},
	{"TR", A_BAND_REGION_2, G_BAND_REGION_1},
	{"UA", A_BAND_REGION_0, G_BAND_REGION_1},
#endif
	{"AE", A_BAND_REGION_0, G_BAND_REGION_1},
	{"GB", A_BAND_REGION_1, G_BAND_REGION_1},
	{"US", A_BAND_REGION_0, G_BAND_REGION_0},
#ifdef RTCONFIG_LOCALE2012
	{"UY", A_BAND_REGION_0, G_BAND_REGION_1},
#else
	{"UY", A_BAND_REGION_5, G_BAND_REGION_1},
#endif
	{"UZ", A_BAND_REGION_1, G_BAND_REGION_0},
#ifdef RTCONFIG_LOCALE2012
	{"VE", A_BAND_REGION_4, G_BAND_REGION_1},
#else
	{"VE", A_BAND_REGION_5, G_BAND_REGION_1},
#endif
	{"VN", A_BAND_REGION_0, G_BAND_REGION_1},
	{"YE", A_BAND_REGION_0, G_BAND_REGION_1},
	{"ZW", A_BAND_REGION_0, G_BAND_REGION_1},
	{"",	0,	0}
};

#define NUM_OF_COUNTRIES	(sizeof(allCountry)/sizeof(COUNTRY_CODE_TO_COUNTRY_REGION))

static int ej_wl_channel_list(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int retval = 0;
	char tmp[256], prefix[] = "wlXXXXXXXXXX_";
	int index, num, i;
	unsigned char *pChannelListTemp = NULL;
	char *country_code;
	int band = -1;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	country_code = nvram_get(strcat_r(prefix, "country_code", tmp));
	band = unit;

	if (country_code == NULL || strlen(country_code) != 2) return retval;

	if (band != 0 && band != 1) return retval;

	for (index = 0; index < NUM_OF_COUNTRIES; index++)
	{
		if (strncmp((char *) allCountry[index].IsoName, country_code, 2) == 0)
			break;
	}

	if (index >= NUM_OF_COUNTRIES) return retval;

	if (band == 1)
	switch (allCountry[index].RegDomainNum11A)
	{
		case A_BAND_REGION_0:
			num = sizeof(A_BAND_REGION_0_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_0_CHANNEL_LIST;
			break;
		case A_BAND_REGION_1:
			num = sizeof(A_BAND_REGION_1_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_1_CHANNEL_LIST;
			break;
		case A_BAND_REGION_2:
			num = sizeof(A_BAND_REGION_2_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_2_CHANNEL_LIST;
			break;
		case A_BAND_REGION_3:
			num = sizeof(A_BAND_REGION_3_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_3_CHANNEL_LIST;
			break;
		case A_BAND_REGION_4:
			num = sizeof(A_BAND_REGION_4_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_4_CHANNEL_LIST;
			break;
		case A_BAND_REGION_5:
			num = sizeof(A_BAND_REGION_5_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_5_CHANNEL_LIST;
			break;
		case A_BAND_REGION_6:
			num = sizeof(A_BAND_REGION_6_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_6_CHANNEL_LIST;
			break;
		case A_BAND_REGION_7:
			num = sizeof(A_BAND_REGION_7_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_7_CHANNEL_LIST;
			break;
		case A_BAND_REGION_8:
			num = sizeof(A_BAND_REGION_8_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_8_CHANNEL_LIST;
			break;
		case A_BAND_REGION_9:
			num = sizeof(A_BAND_REGION_9_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_9_CHANNEL_LIST;
			break;
		case A_BAND_REGION_10:
			num = sizeof(A_BAND_REGION_10_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_10_CHANNEL_LIST;
			break;
		case A_BAND_REGION_11:
			num = sizeof(A_BAND_REGION_11_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_11_CHANNEL_LIST;
			break;	
		case A_BAND_REGION_12:
			num = sizeof(A_BAND_REGION_12_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_12_CHANNEL_LIST;
			break;
		case A_BAND_REGION_13:
			num = sizeof(A_BAND_REGION_13_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_13_CHANNEL_LIST;
			break;
		case A_BAND_REGION_14:
			num = sizeof(A_BAND_REGION_14_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_14_CHANNEL_LIST;
			break;
		case A_BAND_REGION_15:
			num = sizeof(A_BAND_REGION_15_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_15_CHANNEL_LIST;
			break;
		case A_BAND_REGION_16:
			num = sizeof(A_BAND_REGION_16_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_16_CHANNEL_LIST;
			break;
		case A_BAND_REGION_17:
			num = sizeof(A_BAND_REGION_17_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_17_CHANNEL_LIST;
			break;
		case A_BAND_REGION_18:
			num = sizeof(A_BAND_REGION_18_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_18_CHANNEL_LIST;
			break;
		case A_BAND_REGION_19:
			num = sizeof(A_BAND_REGION_19_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_19_CHANNEL_LIST;
			break;
		case A_BAND_REGION_20:
			num = sizeof(A_BAND_REGION_20_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_20_CHANNEL_LIST;
			break;
		case A_BAND_REGION_21:
			num = sizeof(A_BAND_REGION_21_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = A_BAND_REGION_21_CHANNEL_LIST;
			break;
		default:	// Error. should never happen
			dbg("countryregionA=%d not support", allCountry[index].RegDomainNum11A);
			break;
	}
	else if (band == 0)
	switch (allCountry[index].RegDomainNum11G)
	{
		case G_BAND_REGION_0:
			num = sizeof(G_BAND_REGION_0_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = G_BAND_REGION_0_CHANNEL_LIST;
			break;
		case G_BAND_REGION_1:
			num = sizeof(G_BAND_REGION_1_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = G_BAND_REGION_1_CHANNEL_LIST;
			break;
		case G_BAND_REGION_5:
			num = sizeof(G_BAND_REGION_5_CHANNEL_LIST)/sizeof(unsigned char);
			pChannelListTemp = G_BAND_REGION_5_CHANNEL_LIST;
			break;
		default:	// Error. should never happen
			dbg("countryregionG=%d not support", allCountry[index].RegDomainNum11G);
			break;
	}

	if (pChannelListTemp != NULL)
	{
		memset(tmp, 0x0, sizeof(tmp));

		for (i = 0; i < num; i++)
		{
#if 0
			if (!strlen(tmp))
				sprintf(tmp, "%d", pChannelListTemp[i]);
			else
				sprintf(tmp, "%s %d", tmp, pChannelListTemp[i]);
#else
			if (i == 0)
				sprintf(tmp, "[\"%d\",", pChannelListTemp[i]);
			else if (i == (num - 1))
				sprintf(tmp,  "%s \"%d\"]", tmp, pChannelListTemp[i]);
			else
				sprintf(tmp,  "%s \"%d\",", tmp, pChannelListTemp[i]);
#endif
		}

		retval += websWrite(wp, "%s", tmp);
	}

	return retval;
}

int
ej_wl_channel_list_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	return ej_wl_channel_list(eid, wp, argc, argv, 0);
}

int
ej_wl_channel_list_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	return ej_wl_channel_list(eid, wp, argc, argv, 1);
}

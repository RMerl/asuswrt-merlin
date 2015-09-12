#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <typedefs.h>
#include <bcmnvram.h>
#include <sys/ioctl.h>
#include <ralink.h>
#include <iwlib.h>
#include "utils.h"
#include "shutils.h"
#include <shared.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <iwlib.h>
#ifndef O_BINARY
#define O_BINARY 	0
#endif
#include <image.h>
#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif
#include <linux_gpio.h>

typedef uint32_t __u32;

#if defined(RTN14U) || defined(RTAC52U) || defined(RTAC51U) || defined(RTN11P) || defined(RTN300) || defined(RTN54U) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTAC54U)
const char WIF_5G[]	= "rai0";
const char WIF_2G[]	= "ra0";
const char WDSIF_5G[]	= "wdsi";
const char APCLI_5G[]	= "apclii0";
const char APCLI_2G[]	= "apcli0";
#else
const char WIF_5G[]	= "ra0";
const char WIF_2G[]	= "rai0";
const char WDSIF_5G[]	= "wds";
const char APCLI_5G[]	= "apcli0";
const char APCLI_2G[]	= "apclii0";
#endif

#if defined(RA_ESW)
/* Read TX/RX byte count information from switch's register. */
#if defined(RTCONFIG_RALINK_MT7620)
int get_mt7620_wan_unit_bytecount(int unit, unsigned long *tx, unsigned long *rx)
#elif defined(RTCONFIG_RALINK_MT7621)
int get_mt7621_wan_unit_bytecount(int unit, unsigned long *tx, unsigned long *rx)
#endif
{
#if defined(RTCONFIG_RALINK_MT7620)
	return __mt7620_wan_bytecount(unit, tx, rx);
#elif defined(RTCONFIG_RALINK_MT7621)
	return __mt7621_wan_bytecount(unit, tx, rx);
#endif
}
#endif
uint32_t gpio_dir(uint32_t gpio, int dir)
{
	return ralink_gpio_init(gpio, dir);
}

uint32_t get_gpio(uint32_t gpio)
{
	return ralink_gpio_read_bit(gpio);
}


uint32_t set_gpio(uint32_t gpio, uint32_t value)
{
	ralink_gpio_write_bit(gpio, value);
	return 0;
}

int get_switch_model(void)
{
	// TODO
	return SWITCH_UNKNOWN;
}

uint32_t get_phy_status(uint32_t portmask)
{
	// TODO
	return 1;
}

uint32_t get_phy_speed(uint32_t portmask)
{
	// TODO
	return 1;
}

uint32_t set_phy_ctrl(uint32_t portmask, int ctrl)
{
	// TODO
	return 1;
}

#define SWAP_LONG(x) \
	((__u32)( \
		(((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
		(((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
		(((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
		(((__u32)(x) & (__u32)0xff000000UL) >> 24) ))

/* 0: it is not a legal image
 * 1: it is legal image
 */
int check_imageheader(char *buf, long *filelen)
{
	uint32_t checksum;
	image_header_t header2;
	image_header_t *hdr, *hdr2;

	hdr  = (image_header_t *) buf;
	hdr2 = &header2;

	/* check header magic */
	if (SWAP_LONG(hdr->ih_magic) != IH_MAGIC) {
		_dprintf ("Bad Magic Number\n");
		return 0;
	}

	/* check header crc */
	memcpy (hdr2, hdr, sizeof(image_header_t));
	hdr2->ih_hcrc = 0;
	checksum = crc_calc(0, (const char *)hdr2, sizeof(image_header_t));
	_dprintf("header crc: %X\n", checksum);
	_dprintf("org header crc: %X\n", SWAP_LONG(hdr->ih_hcrc));
	if (checksum != SWAP_LONG(hdr->ih_hcrc))
	{
		_dprintf("Bad Header Checksum\n");
		return 0;
	}

	{
		if(strcmp(buf+36, nvram_safe_get("productid"))==0) {
			*filelen  = SWAP_LONG(hdr->ih_size);
			*filelen += sizeof(image_header_t);
#ifdef RTCONFIG_DSL
			// DSL product may have modem firmware
			*filelen+=(512*1024);			
#endif		
			_dprintf("image len: %x\n", *filelen);	
			return 1;
		}
	}
	return 0;
}

int
checkcrc(char *fname)
{
	int ifd = -1;
	uint32_t checksum;
	struct stat sbuf;
	unsigned char *ptr = NULL;
	image_header_t *hdr;
	char *imagefile;
	int ret = -1;
	int len;

	imagefile = fname;
//	fprintf(stderr, "img file: %s\n", imagefile);

	ifd = open(imagefile, O_RDONLY|O_BINARY);

	if (ifd < 0) {
		_dprintf("Can't open %s: %s\n",
			imagefile, strerror(errno));
		goto checkcrc_end;
	}

	/* We're a bit of paranoid */
#if defined(_POSIX_SYNCHRONIZED_IO) && !defined(__sun__) && !defined(__FreeBSD__)
	(void) fdatasync (ifd);
#else
	(void) fsync (ifd);
#endif
	if (fstat(ifd, &sbuf) < 0) {
		_dprintf("Can't stat %s: %s\n",
			imagefile, strerror(errno));
		goto checkcrc_fail;
	}

	ptr = (unsigned char *)mmap(0, sbuf.st_size,
				    PROT_READ, MAP_SHARED, ifd, 0);
	if (ptr == (unsigned char *)MAP_FAILED) {
		_dprintf("Can't map %s: %s\n",
			imagefile, strerror(errno));
		goto checkcrc_fail;
	}
	hdr = (image_header_t *)ptr;

	/* check image header */
	if(check_imageheader((char*)hdr, (long*)&len) == 0)
	{
		_dprintf("Check image heaer fail !!!\n");
		goto checkcrc_fail;
	}

	len = SWAP_LONG(hdr->ih_size);
	if (sbuf.st_size < (len + sizeof(image_header_t))) {
		_dprintf("Size mismatch %lx/%lx !!!\n", sbuf.st_size, (len + sizeof(image_header_t)));
		goto checkcrc_fail;
	}

	/* check body crc */
	_dprintf("Verifying Checksum ... ");
	checksum = crc_calc(0, (const char *)ptr + sizeof(image_header_t), len);
	if(checksum != SWAP_LONG(hdr->ih_dcrc))
	{
		_dprintf("Bad Data CRC\n");
		goto checkcrc_fail;
	}
	_dprintf("OK\n");

	ret = 0;

	/* We're a bit of paranoid */
checkcrc_fail:
	if(ptr != NULL)
		munmap(ptr, sbuf.st_size);
#if defined(_POSIX_SYNCHRONIZED_IO) && !defined(__sun__) && !defined(__FreeBSD__)
	(void) fdatasync (ifd);
#else
	(void) fsync (ifd);
#endif
	if (close(ifd)) {
		_dprintf("Read error on %s: %s\n",
			imagefile, strerror(errno));
		ret=-1;
	}
checkcrc_end:
	return ret;
}

/* 
 * 0: illegal image
 * 1: legal image
 *
 * check product id, crc ..
 */

int check_imagefile(char *fname)
{
	if(checkcrc(fname)==0) return 1;
	return 0;
}

int wl_ioctl(const char *ifname, int cmd, struct iwreq *pwrq)
{
	int ret = 0;
 	int s;

	/* open socket to kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return errno;
	}

	/* do it */
	strncpy(pwrq->ifr_name, ifname, IFNAMSIZ);
	if ((ret = ioctl(s, cmd, pwrq)) < 0)
		perror(pwrq->ifr_name);

	/* cleanup */
	close(s);
	return ret;
}

unsigned int get_radio_status(char *ifname)
{
	struct iwreq wrq;
	unsigned int data = 0;

	wrq.u.data.length = sizeof(data);
	wrq.u.data.pointer = (caddr_t) &data;
	wrq.u.data.flags = ASUS_SUBCMD_RADIO_STATUS;
	if (wl_ioctl(ifname, RTPRIV_IOCTL_ASUSCMD, &wrq) < 0)
		printf("ioctl error\n");

	return data;
}

int get_radio(int unit, int subunit)
{
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";

	if (subunit > 0)
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
	else
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	// TODO: handle subunit
	if (subunit > 0)
		return nvram_match(strcat_r(prefix, "radio", tmp), "1");
	else
		return get_radio_status(nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
}

void set_radio(int on, int unit, int subunit)
{
	char /*tmp[100],*/ prefix[] = "wlXXXXXXXXXXXXXX";

	if (subunit > 0)
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
	else
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	//if (nvram_match(strcat_r(prefix, "radio", tmp), "0")) return;
	// TODO: replace hardcoded 
	// TODO: handle subunit
	if(unit==0)
		doSystem("iwpriv %s set RadioOn=%d", WIF_2G, on);
	else doSystem("iwpriv %s set RadioOn=%d", WIF_5G, on);

#if defined(RTAC1200HP) || defined(RTN56UB1) //5G:7612E 2G:7603E
	led_onoff(unit);
#endif	
}

char *wif_to_vif(char *wif)
{
	static char vif[32];
	int unit = 0, subunit = 0;
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXX";

	vif[0] = '\0';

	for (unit = 0; unit < 2; unit++)
		for (subunit = 1; subunit < 4; subunit++)
		{
			snprintf(prefix, sizeof(prefix), "wl%d.%d", unit, subunit);
			
			if (nvram_match(strcat_r(prefix, "_ifname", tmp), wif))
			{
				sprintf(vif, "%s", prefix);
				goto RETURN_VIF;
			}
		}

RETURN_VIF:
	return vif;
}


/* get channel list via currently setting in wifi driver */
int get_channel_list_via_driver(int unit, char *buffer, int len)
{
	struct iwreq wrq;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_", *ifname;

	if(buffer == NULL || len <= 0)
		return -1;

	memset(buffer, 0, len);
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	memset(&wrq, 0, sizeof(wrq));
	wrq.u.data.pointer = buffer;
	wrq.u.data.length  = len;
	wrq.u.data.flags   = ASUS_SUBCMD_CHLIST;
	if (wl_ioctl(ifname, RTPRIV_IOCTL_ASUSCMD, &wrq) < 0)
		return -1;

	return wrq.u.data.length;
}

/* get channel list via value of countryCode */
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

int get_channel_list_via_country(int unit, const char *country_code, char *buffer, int len)
{
	unsigned char *pChannelListTemp = NULL;
	int index, num, i;
	char *p = buffer;
	int band = unit;

	if(buffer == NULL || len <= 0)
		return -1;

	memset(buffer, 0, len);

	if (band != 0 && band != 1) return -1;

	for (index = 0; index < NUM_OF_COUNTRIES; index++)
	{
		if (strncmp((char *) allCountry[index].IsoName, country_code, 2) == 0)
			break;
	}

	if (index >= NUM_OF_COUNTRIES) return 0;

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
		for (i = 0; i < num; i++)
		{
#if 0
			if (i == 0)
				p += sprintf(p, "\"%d\"", pChannelListTemp[i]);
			else
				p += sprintf(p,  ", \"%d\"", pChannelListTemp[i]);
#else
			if (i == 0)
				p += sprintf(p, "%d", pChannelListTemp[i]);
			else
				p += sprintf(p,  ",%d", pChannelListTemp[i]);
#endif
		}
	}

	return (p - buffer);
}


#if defined(RTAC1200HP) || defined(RTN56UB1)
void led_onoff(int unit)
{   
#if defined(RTAC1200HP)
	if(unit==1)
#endif		
		if(get_radio(unit, 0))
			led_control(unit?LED_5G:LED_2G, LED_ON);	
		else
			led_control(unit?LED_5G:LED_2G, LED_OFF);	
}
#endif

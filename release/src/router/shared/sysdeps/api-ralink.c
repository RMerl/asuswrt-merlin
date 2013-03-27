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

uint32_t set_phy_ctrl(uint32_t portmask, uint32_t ctrl)
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
	if(check_imageheader(hdr, &len) == 0)
	{
		_dprintf("Check image heaer fail !!!\n");
		goto checkcrc_fail;
	}

	/* check body crc */
	len = SWAP_LONG(hdr->ih_size);

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
	else doSystem("iwpriv %s set RadioOn=%d", WIF, on);
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


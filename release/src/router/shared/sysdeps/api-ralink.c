#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <typedefs.h>
#include <bcmnvram.h>
#include <ralink.h>
#include "utils.h"
#include "shutils.h"
#include <shared.h>
#include <sys/mman.h>
#ifndef O_BINARY
#define O_BINARY 	0
#endif
#include <image.h>
#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif
#include <linux_gpio.h>

typedef uint32_t __u32;

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
	long *filelenptr, tmp;

	if(buf[0]==0x27&&buf[1]==0x05&&buf[2]==0x19&&buf[3]==0x56)
	{
		if(strcmp(buf+36, nvram_safe_get("productid"))==0) {
			filelenptr=(buf+12);
			tmp=*filelenptr;
			*filelen=SWAP_LONG(tmp);
			*filelen+=64;			
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
	int ifd;
	uint32_t checksum;
	struct stat sbuf;
	unsigned char *ptr;
	image_header_t header2;
	image_header_t *hdr, *hdr2=&header2;
	char *imagefile;
	int ret=0;

	imagefile = fname;
//	fprintf(stderr, "img file: %s\n", imagefile);

	ifd = open(imagefile, O_RDONLY|O_BINARY);

	if (ifd < 0) {
		fprintf (stderr, "Can't open %s: %s\n",
			imagefile, strerror(errno));
		ret=-1;
		goto checkcrc_end;
	}

	memset (hdr2, 0, sizeof(image_header_t));

	/* We're a bit of paranoid */
#if defined(_POSIX_SYNCHRONIZED_IO) && !defined(__sun__) && !defined(__FreeBSD__)
	(void) fdatasync (ifd);
#else
	(void) fsync (ifd);
#endif
	if (fstat(ifd, &sbuf) < 0) {
		fprintf (stderr, "Can't stat %s: %s\n",
			imagefile, strerror(errno));
		ret=-1;
		goto checkcrc_fail;
	}

	ptr = (unsigned char *)mmap(0, sbuf.st_size,
				    PROT_READ, MAP_SHARED, ifd, 0);
	if (ptr == (unsigned char *)MAP_FAILED) {
		fprintf (stderr, "Can't map %s: %s\n",
			imagefile, strerror(errno));
		ret=-1;
		goto checkcrc_fail;
	}
	hdr = (image_header_t *)ptr;
/*
	checksum = crc32_sp (0,
			  (const char *)(ptr + sizeof(image_header_t)),
			  sbuf.st_size - sizeof(image_header_t)
			 );
	fprintf(stderr,"data crc: %X\n", checksum);
	fprintf(stderr,"org data crc: %X\n", SWAP_LONG(hdr->ih_dcrc));
//	if (checksum!=SWAP_LONG(hdr->ih_dcrc))
//		return -1;
*/
	memcpy (hdr2, hdr, sizeof(image_header_t));
	memset(&hdr2->ih_hcrc, 0, sizeof(uint32_t));
	
	checksum = crc_calc(0,(const char *)hdr2,sizeof(image_header_t));

	fprintf(stderr, "header crc: %X\n", checksum);
	fprintf(stderr, "org header crc: %X\n", SWAP_LONG(hdr->ih_hcrc));

	if (checksum!=SWAP_LONG(hdr->ih_hcrc))
	{
		ret=-1;
		goto checkcrc_fail;
	}

	(void) munmap((void *)ptr, sbuf.st_size);

	/* We're a bit of paranoid */
checkcrc_fail:
#if defined(_POSIX_SYNCHRONIZED_IO) && !defined(__sun__) && !defined(__FreeBSD__)
	(void) fdatasync (ifd);
#else
	(void) fsync (ifd);
#endif
	if (close(ifd)) {
		fprintf (stderr, "Read error on %s: %s\n",
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
	else return 1;
}

int get_radio(int unit, int subunit)
{
	uint32 n;

	// TODO
	return 1;
}

void set_radio(int on, int unit, int subunit)
{
	uint32 n;

	// TODO: replace hardcoded 
	// TODO: handle subunit
	if(unit==0)
		doSystem("iwpriv %s set RadioOn=%d", WIF_2G, on);
	else doSystem("iwpriv %s set RadioOn=%d", WIF, on);
}




/*
 * Copyright (c) 2d3D, Inc.
 * Written by Abraham vd Merwe <abraham@2d3d.co.za>
 * All rights reserved.
 *
 * Renamed to flashcp.c to avoid conflicts with fcp from fsh package
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *	  may be used to endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define PROGRAM_NAME "flashcp"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <mtd/mtd-user.h>
#include <getopt.h>

typedef int bool;
#define true 1
#define false 0

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

/* for debugging purposes only */
#ifdef DEBUG
#undef DEBUG
#define DEBUG(fmt,args...) { log_printf (LOG_ERROR,"%d: ",__LINE__); log_printf (LOG_ERROR,fmt,## args); }
#else
#undef DEBUG
#define DEBUG(fmt,args...)
#endif

#define KB(x) ((x) / 1024)
#define PERCENTAGE(x,total) (((x) * 100) / (total))

/* size of read/write buffer */
#define BUFSIZE (10 * 1024)

/* cmd-line flags */
#define FLAG_NONE		0x00
#define FLAG_VERBOSE	0x01
#define FLAG_HELP		0x02
#define FLAG_FILENAME	0x04
#define FLAG_DEVICE		0x08

/* error levels */
#define LOG_NORMAL	1
#define LOG_ERROR	2

static void log_printf (int level,const char *fmt, ...)
{
	FILE *fp = level == LOG_NORMAL ? stdout : stderr;
	va_list ap;
	va_start (ap,fmt);
	vfprintf (fp,fmt,ap);
	va_end (ap);
	fflush (fp);
}

static void showusage(bool error)
{
	int level = error ? LOG_ERROR : LOG_NORMAL;

	log_printf (level,
			"\n"
			"Flash Copy - Written by Abraham van der Merwe <abraham@2d3d.co.za>\n"
			"\n"
			"usage: %1$s [ -v | --verbose ] <filename> <device>\n"
			"       %1$s -h | --help\n"
			"\n"
			"   -h | --help      Show this help message\n"
			"   -v | --verbose   Show progress reports\n"
			"   <filename>       File which you want to copy to flash\n"
			"   <device>         Flash device to write to (e.g. /dev/mtd0, /dev/mtd1, etc.)\n"
			"\n",
			PROGRAM_NAME);

	exit (error ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int safe_open (const char *pathname,int flags)
{
	int fd;

	fd = open (pathname,flags);
	if (fd < 0)
	{
		log_printf (LOG_ERROR,"While trying to open %s",pathname);
		if (flags & O_RDWR)
			log_printf (LOG_ERROR," for read/write access");
		else if (flags & O_RDONLY)
			log_printf (LOG_ERROR," for read access");
		else if (flags & O_WRONLY)
			log_printf (LOG_ERROR," for write access");
		log_printf (LOG_ERROR,": %m\n");
		exit (EXIT_FAILURE);
	}

	return (fd);
}

static void safe_read (int fd,const char *filename,void *buf,size_t count,bool verbose)
{
	ssize_t result;

	result = read (fd,buf,count);
	if (count != result)
	{
		if (verbose) log_printf (LOG_NORMAL,"\n");
		if (result < 0)
		{
			log_printf (LOG_ERROR,"While reading data from %s: %m\n",filename);
			exit (EXIT_FAILURE);
		}
		log_printf (LOG_ERROR,"Short read count returned while reading from %s\n",filename);
		exit (EXIT_FAILURE);
	}
}

static void safe_rewind (int fd,const char *filename)
{
	if (lseek (fd,0L,SEEK_SET) < 0)
	{
		log_printf (LOG_ERROR,"While seeking to start of %s: %m\n",filename);
		exit (EXIT_FAILURE);
	}
}

/******************************************************************************/

static int dev_fd = -1,fil_fd = -1;

static void cleanup (void)
{
	if (dev_fd > 0) close (dev_fd);
	if (fil_fd > 0) close (fil_fd);
}

int main (int argc,char *argv[])
{
	const char *filename = NULL,*device = NULL;
	int i,flags = FLAG_NONE;
	ssize_t result;
	size_t size,written;
	struct mtd_info_user mtd;
	struct erase_info_user erase;
	struct stat filestat;
	unsigned char src[BUFSIZE],dest[BUFSIZE];

	/*********************
	 * parse cmd-line
	 *****************/

	for (;;) {
		int option_index = 0;
		static const char *short_options = "hv";
		static const struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"verbose", no_argument, 0, 'v'},
			{0, 0, 0, 0},
		};

		int c = getopt_long(argc, argv, short_options,
				long_options, &option_index);
		if (c == EOF) {
			break;
		}

		switch (c) {
			case 'h':
				flags |= FLAG_HELP;
				DEBUG("Got FLAG_HELP\n");
				break;
			case 'v':
				flags |= FLAG_VERBOSE;
				DEBUG("Got FLAG_VERBOSE\n");
				break;
			default:
				DEBUG("Unknown parameter: %s\n",argv[option_index]);
				showusage(true);
		}
	}
	if (optind+2 == argc) {
		flags |= FLAG_FILENAME;
		filename = argv[optind];
		DEBUG("Got filename: %s\n",filename);

		flags |= FLAG_DEVICE;
		device = argv[optind+1];
		DEBUG("Got device: %s\n",device);
	}

	if (flags & FLAG_HELP || device == NULL)
		showusage(flags != FLAG_HELP);

	atexit (cleanup);

	/* get some info about the flash device */
	dev_fd = safe_open (device,O_SYNC | O_RDWR);
	if (ioctl (dev_fd,MEMGETINFO,&mtd) < 0)
	{
		DEBUG("ioctl(): %m\n");
		log_printf (LOG_ERROR,"This doesn't seem to be a valid MTD flash device!\n");
		exit (EXIT_FAILURE);
	}

	/* get some info about the file we want to copy */
	fil_fd = safe_open (filename,O_RDONLY);
	if (fstat (fil_fd,&filestat) < 0)
	{
		log_printf (LOG_ERROR,"While trying to get the file status of %s: %m\n",filename);
		exit (EXIT_FAILURE);
	}

	/* does it fit into the device/partition? */
	if (filestat.st_size > mtd.size)
	{
		log_printf (LOG_ERROR,"%s won't fit into %s!\n",filename,device);
		exit (EXIT_FAILURE);
	}

	/*****************************************************
	 * erase enough blocks so that we can write the file *
	 *****************************************************/

#warning "Check for smaller erase regions"

	erase.start = 0;
	erase.length = (filestat.st_size + mtd.erasesize - 1) / mtd.erasesize;
	erase.length *= mtd.erasesize;

	if (flags & FLAG_VERBOSE)
	{
		/* if the user wants verbose output, erase 1 block at a time and show him/her what's going on */
		int blocks = erase.length / mtd.erasesize;
		erase.length = mtd.erasesize;
		log_printf (LOG_NORMAL,"Erasing blocks: 0/%d (0%%)",blocks);
		for (i = 1; i <= blocks; i++)
		{
			log_printf (LOG_NORMAL,"\rErasing blocks: %d/%d (%d%%)",i,blocks,PERCENTAGE (i,blocks));
			if (ioctl (dev_fd,MEMERASE,&erase) < 0)
			{
				log_printf (LOG_NORMAL,"\n");
				log_printf (LOG_ERROR,
						"While erasing blocks 0x%.8x-0x%.8x on %s: %m\n",
						(unsigned int) erase.start,(unsigned int) (erase.start + erase.length),device);
				exit (EXIT_FAILURE);
			}
			erase.start += mtd.erasesize;
		}
		log_printf (LOG_NORMAL,"\rErasing blocks: %d/%d (100%%)\n",blocks,blocks);
	}
	else
	{
		/* if not, erase the whole chunk in one shot */
		if (ioctl (dev_fd,MEMERASE,&erase) < 0)
		{
			log_printf (LOG_ERROR,
					"While erasing blocks from 0x%.8x-0x%.8x on %s: %m\n",
					(unsigned int) erase.start,(unsigned int) (erase.start + erase.length),device);
			exit (EXIT_FAILURE);
		}
	}
	DEBUG("Erased %u / %luk bytes\n",erase.length,filestat.st_size);

	/**********************************
	 * write the entire file to flash *
	 **********************************/

	if (flags & FLAG_VERBOSE) log_printf (LOG_NORMAL,"Writing data: 0k/%luk (0%%)",KB (filestat.st_size));
	size = filestat.st_size;
	i = BUFSIZE;
	written = 0;
	while (size)
	{
		if (size < BUFSIZE) i = size;
		if (flags & FLAG_VERBOSE)
			log_printf (LOG_NORMAL,"\rWriting data: %dk/%luk (%lu%%)",
					KB (written + i),
					KB (filestat.st_size),
					PERCENTAGE (written + i,filestat.st_size));

		/* read from filename */
		safe_read (fil_fd,filename,src,i,flags & FLAG_VERBOSE);

		/* write to device */
		result = write (dev_fd,src,i);
		if (i != result)
		{
			if (flags & FLAG_VERBOSE) log_printf (LOG_NORMAL,"\n");
			if (result < 0)
			{
				log_printf (LOG_ERROR,
						"While writing data to 0x%.8x-0x%.8x on %s: %m\n",
						written,written + i,device);
				exit (EXIT_FAILURE);
			}
			log_printf (LOG_ERROR,
					"Short write count returned while writing to x%.8x-0x%.8x on %s: %d/%lu bytes written to flash\n",
					written,written + i,device,written + result,filestat.st_size);
			exit (EXIT_FAILURE);
		}

		written += i;
		size -= i;
	}
	if (flags & FLAG_VERBOSE)
		log_printf (LOG_NORMAL,
				"\rWriting data: %luk/%luk (100%%)\n",
				KB (filestat.st_size),
				KB (filestat.st_size));
	DEBUG("Wrote %d / %luk bytes\n",written,filestat.st_size);

	/**********************************
	 * verify that flash == file data *
	 **********************************/

	safe_rewind (fil_fd,filename);
	safe_rewind (dev_fd,device);
	size = filestat.st_size;
	i = BUFSIZE;
	written = 0;
	if (flags & FLAG_VERBOSE) log_printf (LOG_NORMAL,"Verifying data: 0k/%luk (0%%)",KB (filestat.st_size));
	while (size)
	{
		if (size < BUFSIZE) i = size;
		if (flags & FLAG_VERBOSE)
			log_printf (LOG_NORMAL,
					"\rVerifying data: %dk/%luk (%lu%%)",
					KB (written + i),
					KB (filestat.st_size),
					PERCENTAGE (written + i,filestat.st_size));

		/* read from filename */
		safe_read (fil_fd,filename,src,i,flags & FLAG_VERBOSE);

		/* read from device */
		safe_read (dev_fd,device,dest,i,flags & FLAG_VERBOSE);

		/* compare buffers */
		if (memcmp (src,dest,i))
		{
			log_printf (LOG_ERROR,
					"File does not seem to match flash data. First mismatch at 0x%.8x-0x%.8x\n",
					written,written + i);
			exit (EXIT_FAILURE);
		}

		written += i;
		size -= i;
	}
	if (flags & FLAG_VERBOSE)
		log_printf (LOG_NORMAL,
				"\rVerifying data: %luk/%luk (100%%)\n",
				KB (filestat.st_size),
				KB (filestat.st_size));
	DEBUG("Verified %d / %luk bytes\n",written,filestat.st_size);

	exit (EXIT_SUCCESS);
}


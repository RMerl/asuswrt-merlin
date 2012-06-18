/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2008 Michele Sanges <michele.sanges@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Usage:
 * - use kernel option 'vga=xxx' or otherwise enable framebuffer device.
 * - put somewhere fbsplash.cfg file and an image in .ppm format.
 * - run applet: $ setsid fbsplash [params] &
 *      -c: hide cursor
 *      -d /dev/fbN: framebuffer device (if not /dev/fb0)
 *      -s path_to_image_file (can be "-" for stdin)
 *      -i path_to_cfg_file
 *      -f path_to_fifo (can be "-" for stdin)
 * - if you want to run it only in presence of a kernel parameter
 *   (for example fbsplash=on), use:
 *   grep -q "fbsplash=on" </proc/cmdline && setsid fbsplash [params]
 * - commands for fifo:
 *   "NN" (ASCII decimal number) - percentage to show on progress bar.
 *   "exit" (or just close fifo) - well you guessed it.
 */

#include "libbb.h"
#include <linux/fb.h>

/* If you want logging messages on /tmp/fbsplash.log... */
#define DEBUG 0

#define BYTES_PER_PIXEL 2

typedef unsigned short DATA;

struct globals {
#if DEBUG
	bool bdebug_messages;	// enable/disable logging
	FILE *logfile_fd;	// log file
#endif
	unsigned char *addr;	// pointer to framebuffer memory
	unsigned ns[7];		// n-parameters
	const char *image_filename;
	struct fb_var_screeninfo scr_var;
	struct fb_fix_screeninfo scr_fix;
};
#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)

#define nbar_width	ns[0]	// progress bar width
#define nbar_height	ns[1]	// progress bar height
#define nbar_posx	ns[2]	// progress bar horizontal position
#define nbar_posy	ns[3]	// progress bar vertical position
#define nbar_colr	ns[4]	// progress bar color red component
#define nbar_colg	ns[5]	// progress bar color green component
#define nbar_colb	ns[6]	// progress bar color blue component

#if DEBUG
#define DEBUG_MESSAGE(strMessage, args...) \
	if (G.bdebug_messages) { \
		fprintf(G.logfile_fd, "[%s][%s] - %s\n", \
		__FILE__, __FUNCTION__, strMessage);	\
	}
#else
#define DEBUG_MESSAGE(...) ((void)0)
#endif


/**
 *	Open and initialize the framebuffer device
 * \param *strfb_device pointer to framebuffer device
 */
static void fb_open(const char *strfb_device)
{
	int fbfd = xopen(strfb_device, O_RDWR);

	// framebuffer properties
	xioctl(fbfd, FBIOGET_VSCREENINFO, &G.scr_var);
	xioctl(fbfd, FBIOGET_FSCREENINFO, &G.scr_fix);

	if (G.scr_var.bits_per_pixel != 16)
		bb_error_msg_and_die("only 16 bpp is supported");

	// map the device in memory
	G.addr = mmap(NULL,
			G.scr_var.xres * G.scr_var.yres
			* BYTES_PER_PIXEL /*(G.scr_var.bits_per_pixel / 8)*/,
			PROT_WRITE, MAP_SHARED, fbfd, 0);
	if (G.addr == MAP_FAILED)
		bb_perror_msg_and_die("mmap");
	close(fbfd);
}


/**
 *	Draw hollow rectangle on framebuffer
 */
static void fb_drawrectangle(void)
{
	int cnt;
	DATA thispix;
	DATA *ptr1, *ptr2;
	unsigned char nred = G.nbar_colr/2;
	unsigned char ngreen =  G.nbar_colg/2;
	unsigned char nblue = G.nbar_colb/2;

	nred   >>= 3;  // 5-bit red
	ngreen >>= 2;  // 6-bit green
	nblue  >>= 3;  // 5-bit blue
	thispix = nblue + (ngreen << 5) + (nred << (5+6));

	// horizontal lines
	ptr1 = (DATA*)(G.addr + (G.nbar_posy * G.scr_var.xres + G.nbar_posx) * BYTES_PER_PIXEL);
	ptr2 = (DATA*)(G.addr + ((G.nbar_posy + G.nbar_height - 1) * G.scr_var.xres + G.nbar_posx) * BYTES_PER_PIXEL);
	cnt = G.nbar_width - 1;
	do {
		*ptr1++ = thispix;
		*ptr2++ = thispix;
	} while (--cnt >= 0);

	// vertical lines
	ptr1 = (DATA*)(G.addr + (G.nbar_posy * G.scr_var.xres + G.nbar_posx) * BYTES_PER_PIXEL);
	ptr2 = (DATA*)(G.addr + (G.nbar_posy * G.scr_var.xres + G.nbar_posx + G.nbar_width - 1) * BYTES_PER_PIXEL);
	cnt = G.nbar_height - 1;
	do {
		*ptr1 = thispix; ptr1 += G.scr_var.xres;
		*ptr2 = thispix; ptr2 += G.scr_var.xres;
	} while (--cnt >= 0);
}


/**
 *	Draw filled rectangle on framebuffer
 * \param nx1pos,ny1pos upper left position
 * \param nx2pos,ny2pos down right position
 * \param nred,ngreen,nblue rgb color
 */
static void fb_drawfullrectangle(int nx1pos, int ny1pos, int nx2pos, int ny2pos,
	unsigned char nred, unsigned char ngreen, unsigned char nblue)
{
	int cnt1, cnt2, nypos;
	DATA thispix;
	DATA *ptr;

	nred   >>= 3;  // 5-bit red
	ngreen >>= 2;  // 6-bit green
	nblue  >>= 3;  // 5-bit blue
	thispix = nblue + (ngreen << 5) + (nred << (5+6));

	cnt1 = ny2pos - ny1pos;
	nypos = ny1pos;
	do {
		ptr = (DATA*)(G.addr + (nypos * G.scr_var.xres + nx1pos) * BYTES_PER_PIXEL);
		cnt2 = nx2pos - nx1pos;
		do {
			*ptr++ = thispix;
		} while (--cnt2 >= 0);

		nypos++;
	} while (--cnt1 >= 0);
}


/**
 *	Draw a progress bar on framebuffer
 * \param percent percentage of loading
 */
static void fb_drawprogressbar(unsigned percent)
{
	int i, left_x, top_y, width, height;

	// outer box
	left_x = G.nbar_posx;
	top_y = G.nbar_posy;
	width = G.nbar_width - 1;
	height = G.nbar_height - 1;
	if ((height | width) < 0)
		return;
	// NB: "width" of 1 actually makes rect with width of 2!
	fb_drawrectangle();

	// inner "empty" rectangle
	left_x++;
	top_y++;
	width -= 2;
	height -= 2;
	if ((height | width) < 0)
		return;
	fb_drawfullrectangle(
			left_x,	top_y,
					left_x + width, top_y + height,
			G.nbar_colr, G.nbar_colg, G.nbar_colb);

	if (percent > 0) {
		// actual progress bar
		width = width * percent / 100;
		i = height;
		if (height == 0)
			height++; // divide by 0 is bad
		while (i >= 0) {
			// draw one-line thick "rectangle"
			// top line will have gray lvl 200, bottom one 100
			unsigned gray_level = 100 + i*100/height;
			fb_drawfullrectangle(
					left_x, top_y, left_x + width, top_y,
					gray_level, gray_level, gray_level);
			top_y++;
			i--;
		}
	}
}


/**
 *	Draw image from PPM file
 */
static void fb_drawimage(void)
{
	FILE *theme_file;
	char *read_ptr;
	unsigned char *pixline;
	unsigned i, j, width, height, line_size;

	if (LONE_DASH(G.image_filename)) {
		theme_file = stdin;
	} else {
		int fd = open_zipped(G.image_filename);
		if (fd < 0)
			bb_simple_perror_msg_and_die(G.image_filename);
		theme_file = xfdopen_for_read(fd);
	}

	/* Parse ppm header:
	 * - Magic: two characters "P6".
	 * - Whitespace (blanks, TABs, CRs, LFs).
	 * - A width, formatted as ASCII characters in decimal.
	 * - Whitespace.
	 * - A height, ASCII decimal.
	 * - Whitespace.
	 * - The maximum color value, ASCII decimal, in 0..65535
	 * - Newline or other single whitespace character.
	 *   (we support newline only)
	 * - A raster of Width * Height pixels in triplets of rgb
	 *   in pure binary by 1 or 2 bytes. (we support only 1 byte)
	 */
#define concat_buf bb_common_bufsiz1
	read_ptr = concat_buf;
	while (1) {
		int w, h, max_color_val;
		int rem = concat_buf + sizeof(concat_buf) - read_ptr;
		if (rem < 2
		 || fgets(read_ptr, rem, theme_file) == NULL
		) {
			bb_error_msg_and_die("bad PPM file '%s'", G.image_filename);
		}
		read_ptr = strchrnul(read_ptr, '#');
		*read_ptr = '\0'; /* ignore #comments */
		if (sscanf(concat_buf, "P6 %u %u %u", &w, &h, &max_color_val) == 3
		 && max_color_val <= 255
		) {
			width = w; /* w is on stack, width may be in register */
			height = h;
			break;
		}
	}

	line_size = width*3;
	pixline = xmalloc(line_size);

	if (width > G.scr_var.xres)
		width = G.scr_var.xres;
	if (height > G.scr_var.yres)
		height = G.scr_var.yres;
	for (j = 0; j < height; j++) {
		unsigned char *pixel;
		DATA *src;

		if (fread(pixline, 1, line_size, theme_file) != line_size)
			bb_error_msg_and_die("bad PPM file '%s'", G.image_filename);
		pixel = pixline;
		src = (DATA *)(G.addr + j * G.scr_fix.line_length);
		for (i = 0; i < width; i++) {
			unsigned thispix;
			thispix = (((unsigned)pixel[0] << 8) & 0xf800)
				| (((unsigned)pixel[1] << 3) & 0x07e0)
				| (((unsigned)pixel[2] >> 3));
			*src++ = thispix;
			pixel += 3;
		}
	}
	free(pixline);
	fclose(theme_file);
}


/**
 *	Parse configuration file
 * \param *cfg_filename name of the configuration file
 */
static void init(const char *cfg_filename)
{
	static const char param_names[] ALIGN1 =
		"BAR_WIDTH\0" "BAR_HEIGHT\0"
		"BAR_LEFT\0" "BAR_TOP\0"
		"BAR_R\0" "BAR_G\0" "BAR_B\0"
#if DEBUG
		"DEBUG\0"
#endif
		;
	char *token[2];
	parser_t *parser = config_open2(cfg_filename, xfopen_stdin);
	while (config_read(parser, token, 2, 2, "#=",
				(PARSE_NORMAL | PARSE_MIN_DIE) & ~(PARSE_TRIM | PARSE_COLLAPSE))) {
		unsigned val = xatoi_u(token[1]);
		int i = index_in_strings(param_names, token[0]);
		if (i < 0)
			bb_error_msg_and_die("syntax error: %s", token[0]);
		if (i >= 0 && i < 7)
			G.ns[i] = val;
#if DEBUG
		if (i == 7) {
			G.bdebug_messages = val;
			if (G.bdebug_messages)
				G.logfile_fd = xfopen_for_write("/tmp/fbsplash.log");
		}
#endif
	}
	config_close(parser);
}


int fbsplash_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int fbsplash_main(int argc UNUSED_PARAM, char **argv)
{
	const char *fb_device, *cfg_filename, *fifo_filename;
	FILE *fp = fp; // for compiler
	char *num_buf;
	unsigned num;
	bool bCursorOff;

	INIT_G();

	// parse command line options
	fb_device = "/dev/fb0";
	cfg_filename = NULL;
	fifo_filename = NULL;
	bCursorOff = 1 & getopt32(argv, "cs:d:i:f:",
			&G.image_filename, &fb_device, &cfg_filename, &fifo_filename);

	// parse configuration file
	if (cfg_filename)
		init(cfg_filename);

	// We must have -s IMG
	if (!G.image_filename)
		bb_show_usage();

	fb_open(fb_device);

	if (fifo_filename && bCursorOff) {
		// hide cursor (BEFORE any fb ops)
		full_write(STDOUT_FILENO, "\033[?25l", 6);
	}

	fb_drawimage();

	if (!fifo_filename)
		return EXIT_SUCCESS;

	fp = xfopen_stdin(fifo_filename);
	if (fp != stdin) {
		// For named pipes, we want to support this:
		//  mkfifo cmd_pipe
		//  fbsplash -f cmd_pipe .... &
		//  ...
		//  echo 33 >cmd_pipe
		//  ...
		//  echo 66 >cmd_pipe
		// This means that we don't want fbsplash to get EOF
		// when last writer closes input end.
		// The simplest way is to open fifo for writing too
		// and become an additional writer :)
		open(fifo_filename, O_WRONLY); // errors are ignored
	}

	fb_drawprogressbar(0);
	// Block on read, waiting for some input.
	// Use of <stdio.h> style I/O allows to correctly
	// handle a case when we have many buffered lines
	// already in the pipe
	while ((num_buf = xmalloc_fgetline(fp)) != NULL) {
		if (strncmp(num_buf, "exit", 4) == 0) {
			DEBUG_MESSAGE("exit");
			break;
		}
		num = atoi(num_buf);
		if (isdigit(num_buf[0]) && (num <= 100)) {
#if DEBUG
			DEBUG_MESSAGE(itoa(num));
#endif
			fb_drawprogressbar(num);
		}
		free(num_buf);
	}

	if (bCursorOff) // restore cursor
		full_write(STDOUT_FILENO, "\033[?25h", 6);

	return EXIT_SUCCESS;
}

/*
 *  dumpjffs2.c
 *
 *  Copyright (C) 2003 Thomas Gleixner (tglx@linutronix.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Overview:
 *   This utility dumps the contents of a binary JFFS2 image
 *
 *
 * Bug/ToDo:
 */

#define PROGRAM_NAME "jffs2dump"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <asm/types.h>
#include <dirent.h>
#include <mtd/jffs2-user.h>
#include <endian.h>
#include <byteswap.h>
#include <getopt.h>
#include <crc32.h>
#include "summary.h"
#include "common.h"

#define PAD(x) (((x)+3)&~3)

/* For outputting a byte-swapped version of the input image. */
#define cnv_e32(x) ((jint32_t){bswap_32(x.v32)})
#define cnv_e16(x) ((jint16_t){bswap_16(x.v16)})

#define t32_backwards(x) ({ uint32_t __b = (x); (target_endian==__BYTE_ORDER)?bswap_32(__b):__b; })
#define cpu_to_e32(x) ((jint32_t){t32_backwards(x)})

// Global variables
long	imglen;		// length of image
char	*data;		// image data

void display_help (void)
{
	printf("Usage: %s [OPTION]... INPUTFILE\n"
	       "Dump the contents of a binary JFFS2 image.\n\n"
	       "     --help                   display this help and exit\n"
	       "     --version                display version information and exit\n"
	       " -b, --bigendian              image is big endian\n"
	       " -l, --littleendian           image is little endian\n"
	       " -c, --content                dump image contents\n"
	       " -e, --endianconvert=FNAME    convert image endianness, output to file fname\n"
	       " -r, --recalccrc              recalc name and data crc on endian conversion\n"
	       " -d, --datsize=LEN            size of data chunks, when oob data in binary image (NAND only)\n"
	       " -o, --oobsize=LEN            size of oob data chunk in binary image (NAND only)\n"
	       " -v, --verbose                verbose output\n",
	       PROGRAM_NAME);
	exit(0);
}

void display_version (void)
{
	printf("%1$s " VERSION "\n"
			"\n"
			"Copyright (C) 2003 Thomas Gleixner \n"
			"\n"
			"%1$s comes with NO WARRANTY\n"
			"to the extent permitted by law.\n"
			"\n"
			"You may redistribute copies of %1$s\n"
			"under the terms of the GNU General Public Licence.\n"
			"See the file `COPYING' for more information.\n",
			PROGRAM_NAME);
	exit(0);
}

// Option variables

int 	verbose;		// verbose output
char 	*img;			// filename of image
int	dumpcontent;		// dump image content
int	target_endian = __BYTE_ORDER;	// image endianess
int	convertendian;		// convert endianness
int	recalccrc;		// recalc name and data crc's on endian conversion
char	cnvfile[256];		// filename for conversion output
int	datsize;		// Size of data chunks, when oob data is inside the binary image
int	oobsize;		// Size of oob chunks, when oob data is inside the binary image

void process_options (int argc, char *argv[])
{
	int error = 0;

	for (;;) {
		int option_index = 0;
		static const char *short_options = "blce:rd:o:v";
		static const struct option long_options[] = {
			{"help", no_argument, 0, 0},
			{"version", no_argument, 0, 0},
			{"bigendian", no_argument, 0, 'b'},
			{"littleendian", no_argument, 0, 'l'},
			{"content", no_argument, 0, 'c'},
			{"endianconvert", required_argument, 0, 'e'},
			{"datsize", required_argument, 0, 'd'},
			{"oobsize", required_argument, 0, 'o'},
			{"recalccrc", required_argument, 0, 'r'},
			{"verbose", no_argument, 0, 'v'},
			{0, 0, 0, 0},
		};

		int c = getopt_long(argc, argv, short_options,
				long_options, &option_index);
		if (c == EOF) {
			break;
		}

		switch (c) {
			case 0:
				switch (option_index) {
					case 0:
						display_help();
						break;
					case 1:
						display_version();
						break;
				}
				break;
			case 'v':
				verbose = 1;
				break;
			case 'b':
				target_endian = __BIG_ENDIAN;
				break;
			case 'l':
				target_endian = __LITTLE_ENDIAN;
				break;
			case 'c':
				dumpcontent = 1;
				break;
			case 'd':
				datsize = atoi(optarg);
				break;
			case 'o':
				oobsize = atoi(optarg);
				break;
			case 'e':
				convertendian = 1;
				strcpy (cnvfile, optarg);
				break;
			case 'r':
				recalccrc = 1;
				break;
			case '?':
				error = 1;
				break;
		}
	}

	if ((argc - optind) != 1 || error)
		display_help ();

	img = argv[optind];
}


/*
 *	Dump image contents
 */
void do_dumpcontent (void)
{
	char			*p = data, *p_free_begin;
	union jffs2_node_union 	*node;
	int			empty = 0, dirty = 0;
	char			name[256];
	uint32_t		crc;
	uint16_t		type;
	int			bitchbitmask = 0;
	int			obsolete;

	p_free_begin = NULL;
	while ( p < (data + imglen)) {
		node = (union jffs2_node_union*) p;

		/* Skip empty space */
		if (!p_free_begin)
			p_free_begin = p;
		if (je16_to_cpu (node->u.magic) == 0xFFFF && je16_to_cpu (node->u.nodetype) == 0xFFFF) {
			p += 4;
			empty += 4;
			continue;
		}

		if (p != p_free_begin)
			printf("Empty space found from 0x%08zx to 0x%08zx\n", p_free_begin-data, p-data);
		p_free_begin = NULL;

		if (je16_to_cpu (node->u.magic) != JFFS2_MAGIC_BITMASK)	{
			if (!bitchbitmask++)
				printf ("Wrong bitmask  at  0x%08zx, 0x%04x\n", p - data, je16_to_cpu (node->u.magic));
			p += 4;
			dirty += 4;
			continue;
		}
		bitchbitmask = 0;

		type = je16_to_cpu(node->u.nodetype);
		if ((type & JFFS2_NODE_ACCURATE) != JFFS2_NODE_ACCURATE) {
			obsolete = 1;
			type |= JFFS2_NODE_ACCURATE;
		} else
			obsolete = 0;
		/* Set accurate for CRC check */
		node->u.nodetype = cpu_to_je16(type);

		crc = mtd_crc32 (0, node, sizeof (struct jffs2_unknown_node) - 4);
		if (crc != je32_to_cpu (node->u.hdr_crc)) {
			printf ("Wrong hdr_crc  at  0x%08zx, 0x%08x instead of 0x%08x\n", p - data, je32_to_cpu (node->u.hdr_crc), crc);
			p += 4;
			dirty += 4;
			continue;
		}

		switch(je16_to_cpu(node->u.nodetype)) {

			case JFFS2_NODETYPE_INODE:
				printf ("%8s Inode      node at 0x%08zx, totlen 0x%08x, #ino  %5d, version %5d, isize %8d, csize %8d, dsize %8d, offset %8d\n",
						obsolete ? "Obsolete" : "",
						p - data, je32_to_cpu (node->i.totlen), je32_to_cpu (node->i.ino),
						je32_to_cpu ( node->i.version), je32_to_cpu (node->i.isize),
						je32_to_cpu (node->i.csize), je32_to_cpu (node->i.dsize), je32_to_cpu (node->i.offset));

				crc = mtd_crc32 (0, node, sizeof (struct jffs2_raw_inode) - 8);
				if (crc != je32_to_cpu (node->i.node_crc)) {
					printf ("Wrong node_crc at  0x%08zx, 0x%08x instead of 0x%08x\n", p - data, je32_to_cpu (node->i.node_crc), crc);
					p += PAD(je32_to_cpu (node->i.totlen));
					dirty += PAD(je32_to_cpu (node->i.totlen));;
					continue;
				}

				crc = mtd_crc32(0, p + sizeof (struct jffs2_raw_inode), je32_to_cpu(node->i.csize));
				if (crc != je32_to_cpu(node->i.data_crc)) {
					printf ("Wrong data_crc at  0x%08zx, 0x%08x instead of 0x%08x\n", p - data, je32_to_cpu (node->i.data_crc), crc);
					p += PAD(je32_to_cpu (node->i.totlen));
					dirty += PAD(je32_to_cpu (node->i.totlen));;
					continue;
				}

				p += PAD(je32_to_cpu (node->i.totlen));
				break;

			case JFFS2_NODETYPE_DIRENT:
				memcpy (name, node->d.name, node->d.nsize);
				name [node->d.nsize] = 0x0;
				printf ("%8s Dirent     node at 0x%08zx, totlen 0x%08x, #pino %5d, version %5d, #ino  %8d, nsize %8d, name %s\n",
						obsolete ? "Obsolete" : "",
						p - data, je32_to_cpu (node->d.totlen), je32_to_cpu (node->d.pino),
						je32_to_cpu ( node->d.version), je32_to_cpu (node->d.ino),
						node->d.nsize, name);

				crc = mtd_crc32 (0, node, sizeof (struct jffs2_raw_dirent) - 8);
				if (crc != je32_to_cpu (node->d.node_crc)) {
					printf ("Wrong node_crc at  0x%08zx, 0x%08x instead of 0x%08x\n", p - data, je32_to_cpu (node->d.node_crc), crc);
					p += PAD(je32_to_cpu (node->d.totlen));
					dirty += PAD(je32_to_cpu (node->d.totlen));;
					continue;
				}

				crc = mtd_crc32(0, p + sizeof (struct jffs2_raw_dirent), node->d.nsize);
				if (crc != je32_to_cpu(node->d.name_crc)) {
					printf ("Wrong name_crc at  0x%08zx, 0x%08x instead of 0x%08x\n", p - data, je32_to_cpu (node->d.name_crc), crc);
					p += PAD(je32_to_cpu (node->d.totlen));
					dirty += PAD(je32_to_cpu (node->d.totlen));;
					continue;
				}

				p += PAD(je32_to_cpu (node->d.totlen));
				break;

			case JFFS2_NODETYPE_SUMMARY: {

											 int i;
											 struct jffs2_sum_marker * sm;

											 printf("%8s Inode Sum  node at 0x%08zx, totlen 0x%08x, sum_num  %5d, cleanmarker size %5d\n",
													 obsolete ? "Obsolete" : "",
													 p - data,
													 je32_to_cpu (node->s.totlen),
													 je32_to_cpu (node->s.sum_num),
													 je32_to_cpu (node->s.cln_mkr));

											 crc = mtd_crc32 (0, node, sizeof (struct jffs2_raw_summary) - 8);
											 if (crc != je32_to_cpu (node->s.node_crc)) {
												 printf ("Wrong node_crc at  0x%08zx, 0x%08x instead of 0x%08x\n", p - data, je32_to_cpu (node->s.node_crc), crc);
												 p += PAD(je32_to_cpu (node->s.totlen));
												 dirty += PAD(je32_to_cpu (node->s.totlen));;
												 continue;
											 }

											 crc = mtd_crc32(0, p + sizeof (struct jffs2_raw_summary),  je32_to_cpu (node->s.totlen) - sizeof(struct jffs2_raw_summary));
											 if (crc != je32_to_cpu(node->s.sum_crc)) {
												 printf ("Wrong data_crc at  0x%08zx, 0x%08x instead of 0x%08x\n", p - data, je32_to_cpu (node->s.sum_crc), crc);
												 p += PAD(je32_to_cpu (node->s.totlen));
												 dirty += PAD(je32_to_cpu (node->s.totlen));;
												 continue;
											 }

											 if (verbose) {
												 void *sp;
												 sp = (p + sizeof(struct jffs2_raw_summary));

												 for(i=0; i<je32_to_cpu(node->s.sum_num); i++) {

													 switch(je16_to_cpu(((struct jffs2_sum_unknown_flash *)sp)->nodetype)) {
														 case JFFS2_NODETYPE_INODE : {

																						 struct jffs2_sum_inode_flash *spi;
																						 spi = sp;

																						 printf ("%14s #ino  %5d,  version %5d, offset 0x%08x, totlen 0x%08x\n",
																								 "",
																								 je32_to_cpu (spi->inode),
																								 je32_to_cpu (spi->version),
																								 je32_to_cpu (spi->offset),
																								 je32_to_cpu (spi->totlen));

																						 sp += JFFS2_SUMMARY_INODE_SIZE;
																						 break;
																					 }

														 case JFFS2_NODETYPE_DIRENT : {

																						  char name[255];
																						  struct jffs2_sum_dirent_flash *spd;
																						  spd = sp;

																						  memcpy(name,spd->name,spd->nsize);
																						  name [spd->nsize] = 0x0;

																						  printf ("%14s dirent offset 0x%08x, totlen 0x%08x, #pino  %5d,  version %5d, #ino  %8d, nsize %8d, name %s \n",
																								  "",
																								  je32_to_cpu (spd->offset),
																								  je32_to_cpu (spd->totlen),
																								  je32_to_cpu (spd->pino),
																								  je32_to_cpu (spd->version),
																								  je32_to_cpu (spd->ino),
																								  spd->nsize,
																								  name);

																						  sp += JFFS2_SUMMARY_DIRENT_SIZE(spd->nsize);
																						  break;
																					  }

														 default :
																					  printf("Unknown summary node!\n");
																					  break;
													 }
												 }

												 sm = (struct jffs2_sum_marker *) ((char *)p + je32_to_cpu(node->s.totlen) - sizeof(struct jffs2_sum_marker));

												 printf("%14s Sum Node Offset  0x%08x, Magic 0x%08x, Padded size 0x%08x\n",
														 "",
														 je32_to_cpu(sm->offset),
														 je32_to_cpu(sm->magic),
														 je32_to_cpu(node->s.padded));
											 }

											 p += PAD(je32_to_cpu (node->s.totlen));
											 break;
										 }

			case JFFS2_NODETYPE_CLEANMARKER:
										 if (verbose) {
											 printf ("%8s Cleanmarker     at 0x%08zx, totlen 0x%08x\n",
													 obsolete ? "Obsolete" : "",
													 p - data, je32_to_cpu (node->u.totlen));
										 }
										 p += PAD(je32_to_cpu (node->u.totlen));
										 break;

			case JFFS2_NODETYPE_PADDING:
										 if (verbose) {
											 printf ("%8s Padding    node at 0x%08zx, totlen 0x%08x\n",
													 obsolete ? "Obsolete" : "",
													 p - data, je32_to_cpu (node->u.totlen));
										 }
										 p += PAD(je32_to_cpu (node->u.totlen));
										 break;

			case 0xffff:
										 p += 4;
										 empty += 4;
										 break;

			default:
										 if (verbose) {
											 printf ("%8s Unknown    node at 0x%08zx, totlen 0x%08x\n",
													 obsolete ? "Obsolete" : "",
													 p - data, je32_to_cpu (node->u.totlen));
										 }
										 p += PAD(je32_to_cpu (node->u.totlen));
										 dirty += PAD(je32_to_cpu (node->u.totlen));

		}
	}

	if (verbose)
		printf ("Empty space: %d, dirty space: %d\n", empty, dirty);
}

/*
 *	Convert endianess
 */
void do_endianconvert (void)
{
	char			*p = data;
	union jffs2_node_union 	*node, newnode;
	int			fd, len;
	jint32_t		mode;
	uint32_t		crc;

	fd = open (cnvfile, O_WRONLY | O_CREAT, 0644);
	if (fd < 0) {
		fprintf (stderr, "Cannot open / create file: %s\n", cnvfile);
		return;
	}

	while ( p < (data + imglen)) {
		node = (union jffs2_node_union*) p;

		/* Skip empty space */
		if (je16_to_cpu (node->u.magic) == 0xFFFF && je16_to_cpu (node->u.nodetype) == 0xFFFF) {
			write (fd, p, 4);
			p += 4;
			continue;
		}

		if (je16_to_cpu (node->u.magic) != JFFS2_MAGIC_BITMASK)	{
			printf ("Wrong bitmask  at  0x%08zx, 0x%04x\n", p - data, je16_to_cpu (node->u.magic));
			newnode.u.magic = cnv_e16 (node->u.magic);
			newnode.u.nodetype = cnv_e16 (node->u.nodetype);
			write (fd, &newnode, 4);
			p += 4;
			continue;
		}

		crc = mtd_crc32 (0, node, sizeof (struct jffs2_unknown_node) - 4);
		if (crc != je32_to_cpu (node->u.hdr_crc)) {
			printf ("Wrong hdr_crc  at  0x%08zx, 0x%08x instead of 0x%08x\n", p - data, je32_to_cpu (node->u.hdr_crc), crc);
		}

		switch(je16_to_cpu(node->u.nodetype)) {

			case JFFS2_NODETYPE_INODE:

				newnode.i.magic = cnv_e16 (node->i.magic);
				newnode.i.nodetype = cnv_e16 (node->i.nodetype);
				newnode.i.totlen = cnv_e32 (node->i.totlen);
				newnode.i.hdr_crc = cpu_to_e32 (mtd_crc32 (0, &newnode, sizeof (struct jffs2_unknown_node) - 4));
				newnode.i.ino = cnv_e32 (node->i.ino);
				newnode.i.version = cnv_e32 (node->i.version);
				mode.v32 = node->i.mode.m;
				mode = cnv_e32 (mode);
				newnode.i.mode.m = mode.v32;
				newnode.i.uid = cnv_e16 (node->i.uid);
				newnode.i.gid = cnv_e16 (node->i.gid);
				newnode.i.isize = cnv_e32 (node->i.isize);
				newnode.i.atime = cnv_e32 (node->i.atime);
				newnode.i.mtime = cnv_e32 (node->i.mtime);
				newnode.i.ctime = cnv_e32 (node->i.ctime);
				newnode.i.offset = cnv_e32 (node->i.offset);
				newnode.i.csize = cnv_e32 (node->i.csize);
				newnode.i.dsize = cnv_e32 (node->i.dsize);
				newnode.i.compr = node->i.compr;
				newnode.i.usercompr = node->i.usercompr;
				newnode.i.flags = cnv_e16 (node->i.flags);
				if (recalccrc) {
					len = je32_to_cpu(node->i.csize);
					newnode.i.data_crc = cpu_to_e32 ( mtd_crc32(0, p + sizeof (struct jffs2_raw_inode), len));
				} else
					newnode.i.data_crc = cnv_e32 (node->i.data_crc);

				newnode.i.node_crc = cpu_to_e32 (mtd_crc32 (0, &newnode, sizeof (struct jffs2_raw_inode) - 8));

				write (fd, &newnode, sizeof (struct jffs2_raw_inode));
				write (fd, p + sizeof (struct jffs2_raw_inode), PAD (je32_to_cpu (node->i.totlen) -  sizeof (struct jffs2_raw_inode)));

				p += PAD(je32_to_cpu (node->i.totlen));
				break;

			case JFFS2_NODETYPE_DIRENT:
				newnode.d.magic = cnv_e16 (node->d.magic);
				newnode.d.nodetype = cnv_e16 (node->d.nodetype);
				newnode.d.totlen = cnv_e32 (node->d.totlen);
				newnode.d.hdr_crc = cpu_to_e32 (mtd_crc32 (0, &newnode, sizeof (struct jffs2_unknown_node) - 4));
				newnode.d.pino = cnv_e32 (node->d.pino);
				newnode.d.version = cnv_e32 (node->d.version);
				newnode.d.ino = cnv_e32 (node->d.ino);
				newnode.d.mctime = cnv_e32 (node->d.mctime);
				newnode.d.nsize = node->d.nsize;
				newnode.d.type = node->d.type;
				newnode.d.unused[0] = node->d.unused[0];
				newnode.d.unused[1] = node->d.unused[1];
				newnode.d.node_crc = cpu_to_e32 (mtd_crc32 (0, &newnode, sizeof (struct jffs2_raw_dirent) - 8));
				if (recalccrc)
					newnode.d.name_crc = cpu_to_e32 ( mtd_crc32(0, p + sizeof (struct jffs2_raw_dirent), node->d.nsize));
				else
					newnode.d.name_crc = cnv_e32 (node->d.name_crc);

				write (fd, &newnode, sizeof (struct jffs2_raw_dirent));
				write (fd, p + sizeof (struct jffs2_raw_dirent), PAD (je32_to_cpu (node->d.totlen) -  sizeof (struct jffs2_raw_dirent)));
				p += PAD(je32_to_cpu (node->d.totlen));
				break;

			case JFFS2_NODETYPE_CLEANMARKER:
			case JFFS2_NODETYPE_PADDING:
				newnode.u.magic = cnv_e16 (node->u.magic);
				newnode.u.nodetype = cnv_e16 (node->u.nodetype);
				newnode.u.totlen = cnv_e32 (node->u.totlen);
				newnode.u.hdr_crc = cpu_to_e32 (mtd_crc32 (0, &newnode, sizeof (struct jffs2_unknown_node) - 4));

				write (fd, &newnode, sizeof (struct jffs2_unknown_node));
				len = PAD(je32_to_cpu (node->u.totlen) - sizeof (struct jffs2_unknown_node));
				if (len > 0)
					write (fd, p + sizeof (struct jffs2_unknown_node), len);

				p += PAD(je32_to_cpu (node->u.totlen));
				break;

			case JFFS2_NODETYPE_SUMMARY : {
											  struct jffs2_sum_marker *sm_ptr;
											  int i,sum_len;
											  int counter = 0;

											  newnode.s.magic = cnv_e16 (node->s.magic);
											  newnode.s.nodetype = cnv_e16 (node->s.nodetype);
											  newnode.s.totlen = cnv_e32 (node->s.totlen);
											  newnode.s.hdr_crc = cpu_to_e32 (mtd_crc32 (0, &newnode, sizeof (struct jffs2_unknown_node) - 4));
											  newnode.s.sum_num = cnv_e32 (node->s.sum_num);
											  newnode.s.cln_mkr = cnv_e32 (node->s.cln_mkr);
											  newnode.s.padded = cnv_e32 (node->s.padded);

											  newnode.s.node_crc = cpu_to_e32 (mtd_crc32 (0, &newnode, sizeof (struct jffs2_raw_summary) - 8));

											  // summary header
											  p += sizeof (struct jffs2_raw_summary);

											  // summary data
											  sum_len = je32_to_cpu (node->s.totlen) - sizeof (struct jffs2_raw_summary) - sizeof (struct jffs2_sum_marker);

											  for (i=0; i<je32_to_cpu (node->s.sum_num); i++) {
												  union jffs2_sum_flash *fl_ptr;

												  fl_ptr = (union jffs2_sum_flash *) p;

												  switch (je16_to_cpu (fl_ptr->u.nodetype)) {
													  case JFFS2_NODETYPE_INODE:

														  fl_ptr->i.nodetype = cnv_e16 (fl_ptr->i.nodetype);
														  fl_ptr->i.inode = cnv_e32 (fl_ptr->i.inode);
														  fl_ptr->i.version = cnv_e32 (fl_ptr->i.version);
														  fl_ptr->i.offset = cnv_e32 (fl_ptr->i.offset);
														  fl_ptr->i.totlen = cnv_e32 (fl_ptr->i.totlen);
														  p += sizeof (struct jffs2_sum_inode_flash);
														  counter += sizeof (struct jffs2_sum_inode_flash);
														  break;

													  case JFFS2_NODETYPE_DIRENT:
														  fl_ptr->d.nodetype = cnv_e16 (fl_ptr->d.nodetype);
														  fl_ptr->d.totlen = cnv_e32 (fl_ptr->d.totlen);
														  fl_ptr->d.offset = cnv_e32 (fl_ptr->d.offset);
														  fl_ptr->d.pino = cnv_e32 (fl_ptr->d.pino);
														  fl_ptr->d.version = cnv_e32 (fl_ptr->d.version);
														  fl_ptr->d.ino = cnv_e32 (fl_ptr->d.ino);
														  p += sizeof (struct jffs2_sum_dirent_flash) + fl_ptr->d.nsize;
														  counter += sizeof (struct jffs2_sum_dirent_flash) + fl_ptr->d.nsize;
														  break;

													  default :
														  printf("Unknown node in summary information!!! nodetype(%x)\n", je16_to_cpu (fl_ptr->u.nodetype));
														  exit(EXIT_FAILURE);
														  break;
												  }

											  }

											  //pad
											  p += sum_len - counter;

											  // summary marker
											  sm_ptr = (struct jffs2_sum_marker *) p;
											  sm_ptr->offset = cnv_e32 (sm_ptr->offset);
											  sm_ptr->magic = cnv_e32 (sm_ptr->magic);
											  p += sizeof (struct jffs2_sum_marker);

											  // generate new crc on sum data
											  newnode.s.sum_crc = cpu_to_e32 ( mtd_crc32(0, ((char *) node) + sizeof (struct jffs2_raw_summary),
														  je32_to_cpu (node->s.totlen) - sizeof (struct jffs2_raw_summary)));

											  // write out new node header
											  write(fd, &newnode, sizeof (struct jffs2_raw_summary));
											  // write out new summary data
											  write(fd, &node->s.sum, sum_len + sizeof (struct jffs2_sum_marker));

											  break;
										  }

			case 0xffff:
										  write (fd, p, 4);
										  p += 4;
										  break;

			default:
										  printf ("Unknown node type: 0x%04x at 0x%08zx, totlen 0x%08x\n", je16_to_cpu (node->u.nodetype), p - data, je32_to_cpu (node->u.totlen));
										  p += PAD(je32_to_cpu (node->u.totlen));

		}
	}

	close (fd);

}

/*
 * Main program
 */
int main(int argc, char **argv)
{
	int fd;

	process_options(argc, argv);

	/* Open the input file */
	if ((fd = open(img, O_RDONLY)) == -1) {
		perror("open input file");
		exit(1);
	}

	// get image length
	imglen = lseek(fd, 0, SEEK_END);
	lseek (fd, 0, SEEK_SET);

	data = malloc (imglen);
	if (!data) {
		perror("out of memory");
		close (fd);
		exit(1);
	}

	if (datsize && oobsize) {
		int  idx = 0;
		long len = imglen;
		uint8_t oob[oobsize];
		printf ("Peeling data out of combined data/oob image\n");
		while (len) {
			// read image data
			read (fd, &data[idx], datsize);
			read (fd, oob, oobsize);
			idx += datsize;
			imglen -= oobsize;
			len -= datsize + oobsize;
		}

	} else {
		// read image data
		read (fd, data, imglen);
	}
	// Close the input file
	close(fd);

	if (dumpcontent)
		do_dumpcontent ();

	if (convertendian)
		do_endianconvert ();

	// free memory
	free (data);

	// Return happy
	exit (0);
}

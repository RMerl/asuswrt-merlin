/**
 * ntfswipe - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2005 Anton Altaparmakov
 * Copyright (c) 2002-2005 Richard Russon
 * Copyright (c) 2004 Yura Pakhuchiy
 *
 * This utility will overwrite unused space on an NTFS volume.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ntfswipe.h"
#include "types.h"
#include "volume.h"
#include "utils.h"
#include "debug.h"
#include "dir.h"
#include "mst.h"
#include "version.h"
#include "logging.h"

static const char *EXEC_NAME = "ntfswipe";
static struct options opts;

/**
 * version - Print version information about the program
 *
 * Print a copyright statement and a brief description of the program.
 *
 * Return:  none
 */
static void version(void)
{
	ntfs_log_info("\n%s v%s (libntfs %s) - Overwrite the unused space on an NTFS "
			"Volume.\n\n", EXEC_NAME, VERSION,
			ntfs_libntfs_version());
	ntfs_log_info("Copyright (c) 2002-2005 Richard Russon\n");
	ntfs_log_info("Copyright (c) 2004 Yura Pakhuchiy\n");
	ntfs_log_info("\n%s\n%s%s\n", ntfs_gpl, ntfs_bugs, ntfs_home);
}

/**
 * usage - Print a list of the parameters to the program
 *
 * Print a list of the parameters and options for the program.
 *
 * Return:  none
 */
static void usage(void)
{
	ntfs_log_info("\nUsage: %s [options] device\n"
		"    -i       --info        Show volume information (default)\n"
		"\n"
		"    -d       --directory   Wipe directory indexes\n"
		"    -l       --logfile     Wipe the logfile (journal)\n"
		"    -m       --mft         Wipe mft space\n"
		"    -p       --pagefile    Wipe pagefile (swap space)\n"
		"    -t       --tails       Wipe file tails\n"
		"    -u       --unused      Wipe unused clusters\n"
		"\n"
		"    -a       --all         Wipe all unused space\n"
		"\n"
		"    -c num   --count num   Number of times to write(default = 1)\n"
		"    -b list  --bytes list  List of values to write(default = 0)\n"
		"\n"
		"    -n       --no-action   Do not write to disk\n"
		"    -f       --force       Use less caution\n"
		"    -q       --quiet       Less output\n"
		"    -v       --verbose     More output\n"
		"    -V       --version     Version information\n"
		"    -h       --help        Print this help\n\n",
		EXEC_NAME);
	ntfs_log_info("%s%s\n", ntfs_bugs, ntfs_home);
}

/**
 * parse_list - Read a comma-separated list of numbers
 * @list:    The comma-separated list of numbers
 * @result:  Store the parsed list here (must be freed by caller)
 *
 * Read a comma-separated list of numbers and allocate an array of ints to store
 * them in.  The numbers can be in decimal, octal or hex.
 *
 * N.B.  The caller must free the memory returned in @result.
 * N.B.  If the function fails, @result is not changed.
 *
 * Return:  0  Error, invalid string
 *	    n  Success, the count of numbers parsed
 */
static int parse_list(char *list, int **result)
{
	char *ptr;
	char *end;
	int i;
	int count;
	int *mem = NULL;

	if (!list || !result)
		return 0;

	for (count = 0, ptr = list; ptr; ptr = strchr(ptr+1, ','))
		count++;

	mem = malloc((count+1) * sizeof(int));
	if (!mem) {
		ntfs_log_error("Couldn't allocate memory in parse_list().\n");
		return 0;
	}

	memset(mem, 0xFF, (count+1) * sizeof(int));

	for (ptr = list, i = 0; i < count; i++) {

		end = NULL;
		mem[i] = strtol(ptr, &end, 0);

		if (!end || (end == ptr) || ((*end != ',') && (*end != 0))) {
			ntfs_log_error("Invalid list '%s'\n", list);
			free(mem);
			return 0;
		}

		if ((mem[i] < 0) || (mem[i] > 255)) {
			ntfs_log_error("Bytes must be in range 0-255.\n");
			free(mem);
			return 0;
		}

		ptr = end + 1;
	}

	ntfs_log_debug("Parsing list '%s' - ", list);
	for (i = 0; i <= count; i++)
		ntfs_log_debug("0x%02x ", mem[i]);
	ntfs_log_debug("\n");

	*result = mem;
	return count;
}

/**
 * parse_options - Read and validate the programs command line
 *
 * Read the command line, verify the syntax and parse the options.
 * This function is very long, but quite simple.
 *
 * Return:  1 Success
 *	    0 Error, one or more problems
 */
static int parse_options(int argc, char *argv[])
{
	static const char *sopt = "-ab:c:dfh?ilmnpqtuvV";
	static struct option lopt[] = {
		{ "all",	no_argument,		NULL, 'a' },
		{ "bytes",	required_argument,	NULL, 'b' },
		{ "count",	required_argument,	NULL, 'c' },
		{ "directory",	no_argument,		NULL, 'd' },
		{ "force",	no_argument,		NULL, 'f' },
		{ "help",	no_argument,		NULL, 'h' },
		{ "info",	no_argument,		NULL, 'i' },
		{ "logfile",	no_argument,		NULL, 'l' },
		{ "mft",	no_argument,		NULL, 'm' },
		{ "no-action",	no_argument,		NULL, 'n' },
		//{ "no-wait",	no_argument,		NULL, 0   },
		{ "pagefile",	no_argument,		NULL, 'p' },
		{ "quiet",	no_argument,		NULL, 'q' },
		{ "tails",	no_argument,		NULL, 't' },
		{ "unused",	no_argument,		NULL, 'u' },
		{ "verbose",	no_argument,		NULL, 'v' },
		{ "version",	no_argument,		NULL, 'V' },
		{ NULL,		0,			NULL, 0   }
	};

	int c = -1;
	char *end;
	int err  = 0;
	int ver  = 0;
	int help = 0;
	int levels = 0;

	opterr = 0; /* We'll handle the errors, thank you. */

	opts.count = 1;

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:	/* A non-option argument */
			if (!opts.device) {
				opts.device = argv[optind-1];
			} else {
				opts.device = NULL;
				err++;
			}
			break;

		case 'i':
			opts.info++;		/* and fall through */
		case 'a':
			opts.directory++;
			opts.logfile++;
			opts.mft++;
			opts.pagefile++;
			opts.tails++;
			opts.unused++;
			break;
		case 'b':
			if (!opts.bytes) {
				if (!parse_list(optarg, &opts.bytes))
					err++;
			} else {
				err++;
			}
			break;
		case 'c':
			if (opts.count == 1) {
				end = NULL;
				opts.count = strtol(optarg, &end, 0);
				if (end && *end)
					err++;
			} else {
				err++;
			}
			break;
		case 'd':
			opts.directory++;
			break;
		case 'f':
			opts.force++;
			break;
		case 'h':
		case '?':
			if (strncmp (argv[optind-1], "--log-", 6) == 0) {
				if (!ntfs_log_parse_option (argv[optind-1]))
					err++;
				break;
			}
			help++;
			break;
		case 'l':
			opts.logfile++;
			break;
		case 'm':
			opts.mft++;
			break;
		case 'n':
			opts.noaction++;
			break;
		case 'p':
			opts.pagefile++;
			break;
		case 'q':
			opts.quiet++;
			ntfs_log_clear_levels(NTFS_LOG_LEVEL_QUIET);
			break;
		case 't':
			opts.tails++;
			break;
		case 'u':
			opts.unused++;
			break;
		case 'v':
			opts.verbose++;
			ntfs_log_set_levels(NTFS_LOG_LEVEL_VERBOSE);
			break;
		case 'V':
			ver++;
			break;
		default:
			if ((optopt == 'b') || (optopt == 'c')) {
				ntfs_log_error("Option '%s' requires an argument.\n", argv[optind-1]);
			} else {
				ntfs_log_error("Unknown option '%s'.\n", argv[optind-1]);
			}
			err++;
			break;
		}
	}

	/* Make sure we're in sync with the log levels */
	levels = ntfs_log_get_levels();
	if (levels & NTFS_LOG_LEVEL_VERBOSE)
		opts.verbose++;
	if (!(levels & NTFS_LOG_LEVEL_QUIET))
		opts.quiet++;

	if (help || ver) {
		opts.quiet = 0;
	} else {
		if (opts.device == NULL) {
			if (argc > 1)
				ntfs_log_error("You must specify exactly one device.\n");
			err++;
		}

		if (opts.quiet && opts.verbose) {
			ntfs_log_error("You may not use --quiet and --verbose at the same time.\n");
			err++;
		}

		/*
		if (opts.info && (opts.unused || opts.tails || opts.mft || opts.directory)) {
			ntfs_log_error("You may not use any other options with --info.\n");
			err++;
		}
		*/

		if ((opts.count < 1) || (opts.count > 100)) {
			ntfs_log_error("The iteration count must be between 1 and 100.\n");
			err++;
		}

		/* Create a default list */
		if (!opts.bytes) {
			opts.bytes = malloc(2 * sizeof(int));
			if (opts.bytes) {
				opts.bytes[0] =  0;
				opts.bytes[1] = -1;
			} else {
				ntfs_log_error("Couldn't allocate memory for byte list.\n");
				err++;
			}
		}

		if (!opts.directory && !opts.logfile && !opts.mft &&
		    !opts.pagefile && !opts.tails && !opts.unused) {
			opts.info = 1;
		}
	}

	if (ver)
		version();
	if (help || err)
		usage();

	return (!err && !help && !ver);
}

/**
 * wipe_unused - Wipe unused clusters
 * @vol:   An ntfs volume obtained from ntfs_mount
 * @byte:  Overwrite with this value
 * @act:   Wipe, test or info
 *
 * Read $Bitmap and wipe any clusters that are marked as not in use.
 *
 * Return: >0  Success, the attribute was wiped
 *          0  Nothing to wipe
 *         -1  Error, something went wrong
 */
static s64 wipe_unused(ntfs_volume *vol, int byte, enum action act)
{
	s64 i;
	s64 total = 0;
	s64 result = 0;
	u8 *buffer = NULL;

	if (!vol || (byte < 0))
		return -1;

	if (act != act_info) {
		buffer = malloc(vol->cluster_size);
		if (!buffer) {
			ntfs_log_error("malloc failed\n");
			return -1;
		}
		memset(buffer, byte, vol->cluster_size);
	}

	for (i = 0; i < vol->nr_clusters; i++) {
		if (utils_cluster_in_use(vol, i)) {
			//ntfs_log_verbose("cluster %lld is in use\n", i);
			continue;
		}

		if (act == act_wipe) {
			//ntfs_log_verbose("cluster %lld is not in use\n", i);
			result = ntfs_pwrite(vol->dev, vol->cluster_size * i, vol->cluster_size, buffer);
			if (result != vol->cluster_size) {
				ntfs_log_error("write failed\n");
				goto free;
			}
		}

		total += vol->cluster_size;
	}

	ntfs_log_quiet("wipe_unused 0x%02x, %lld bytes\n", byte, (long long)total);
free:
	free(buffer);
	return total;
}

/**
 * wipe_compressed_attribute - Wipe compressed $DATA attribute
 * @vol:	An ntfs volume obtained from ntfs_mount
 * @byte:	Overwrite with this value
 * @act:	Wipe, test or info
 * @na:		Opened ntfs attribute
 *
 * Return: >0  Success, the attribute was wiped
 *          0  Nothing to wipe
 *         -1  Error, something went wrong
 */
static s64 wipe_compressed_attribute(ntfs_volume *vol, int byte,
						enum action act, ntfs_attr *na)
{
	unsigned char *buf;
	s64 size, offset, ret, wiped = 0;
	u16 block_size;
	VCN cur_vcn = 0;
	runlist *rlc = na->rl;
	s64 cu_mask = na->compression_block_clusters - 1;

	while (rlc->length) {
		cur_vcn += rlc->length;
		if ((cur_vcn & cu_mask) ||
			(((rlc + 1)->length) && (rlc->lcn != LCN_HOLE))) {
			rlc++;
			continue;
		}

		if (rlc->lcn == LCN_HOLE) {
			runlist *rlt;

			offset = cur_vcn - rlc->length;
			if (offset == (offset & (~cu_mask))) {
				rlc++;
				continue;
			}
			offset = (offset & (~cu_mask))
						<< vol->cluster_size_bits;
			rlt = rlc;
			while ((rlt - 1)->lcn == LCN_HOLE) rlt--;
			while (1) {
				ret = ntfs_rl_pread(vol, na->rl,
						offset, 2, &block_size);
				block_size = le16_to_cpu(block_size);
				if (ret != 2) {
					ntfs_log_verbose("Internal error\n");
					ntfs_log_error("ntfs_rl_pread failed");
					return -1;
				}
				if (block_size == 0) {
					offset += 2;
					break;
				}
				block_size &= 0x0FFF;
				block_size += 3;
				offset += block_size;
				if (offset >= (((rlt->vcn) <<
						vol->cluster_size_bits) - 2))
					goto next;
			}
			size = (rlt->vcn << vol->cluster_size_bits) - offset;
		} else {
			size = na->allocated_size - na->data_size;
			offset = (cur_vcn << vol->cluster_size_bits) - size;
		}

		if (size < 0) {
			ntfs_log_verbose("Internal error\n");
			ntfs_log_error("bug or damaged fs: we want "
				"allocate buffer size %lld bytes", size);
			return -1;
		}

		if ((act == act_info) || (!size)) {
			wiped += size;
			rlc++;
			continue;
		}

		buf = malloc(size);
		if (!buf) {
			ntfs_log_verbose("Not enough memory\n");
			ntfs_log_error("Not enough memory to allocate "
							"%lld bytes", size);
			return -1;
		}
		memset(buf, byte, size);

		ret = ntfs_rl_pwrite(vol, na->rl, offset, size, buf);
		free(buf);
		if (ret != size) {
			ntfs_log_verbose("Internal error\n");
			ntfs_log_error("ntfs_rl_pwrite failed, offset %llu, "
				"size %lld, vcn %lld",	offset, size, rlc->vcn);
			return -1;
		}
		wiped += ret;
next:
		rlc++;
	}

	return wiped;
}

/**
 * wipe_attribute - Wipe not compressed $DATA attribute
 * @vol:	An ntfs volume obtained from ntfs_mount
 * @byte:	Overwrite with this value
 * @act:	Wipe, test or info
 * @na:		Opened ntfs attribute
 *
 * Return: >0  Success, the attribute was wiped
 *          0  Nothing to wipe
 *         -1  Error, something went wrong
 */
static s64 wipe_attribute(ntfs_volume *vol, int byte, enum action act,
								ntfs_attr *na)
{
	unsigned char *buf;
	s64 wiped;
	s64 size;
	u64 offset = na->data_size;

	if (!offset)
		return 0;
	if (NAttrEncrypted(na))
		offset = (((offset - 1) >> 10) + 1) << 10;
	size = (vol->cluster_size - offset) % vol->cluster_size;

	if (act == act_info)
		return size;

	buf = malloc(size);
	if (!buf) {
		ntfs_log_verbose("Not enough memory\n");
		ntfs_log_error("Not enough memory to allocate %lld bytes", size);
		return -1;
	}
	memset(buf, byte, size);

	wiped = ntfs_rl_pwrite(vol, na->rl, offset, size, buf);
	if (wiped == -1) {
		ntfs_log_verbose("Internal error\n");
		ntfs_log_error("Couldn't wipe tail");
	}

	free(buf);
	return wiped;
}

/**
 * wipe_tails - Wipe the file tails
 * @vol:   An ntfs volume obtained from ntfs_mount
 * @byte:  Overwrite with this value
 * @act:   Wipe, test or info
 *
 * Disk space is allocated in clusters.  If a file isn't an exact multiple of
 * the cluster size, there is some slack space at the end.  Wipe this space.
 *
 * Return: >0  Success, the clusters were wiped
 *          0  Nothing to wipe
 *         -1  Error, something went wrong
 */
static s64 wipe_tails(ntfs_volume *vol, int byte, enum action act)
{
	s64 total = 0;
	s64 nr_mft_records, inode_num;
	ntfs_inode *ni;
	ntfs_attr *na;

	if (!vol || (byte < 0))
		return -1;

	nr_mft_records = vol->mft_na->initialized_size >>
			vol->mft_record_size_bits;

	for (inode_num = 16; inode_num < nr_mft_records; inode_num++) {
		s64 wiped;

		ntfs_log_verbose("Inode %lld - ", inode_num);
		ni = ntfs_inode_open(vol, inode_num);
		if (!ni) {
			ntfs_log_verbose("Could not open inode\n");
			continue;
		}

		if (ni->mrec->base_mft_record) {
			ntfs_log_verbose("Not base mft record. Skipping\n");
			goto close_inode;
		}

		na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
		if (!na) {
			ntfs_log_verbose("Couldn't open $DATA attribute\n");
			goto close_inode;
		}

		if (!NAttrNonResident(na)) {
			ntfs_log_verbose("Resident $DATA attribute. Skipping.\n");
			goto close_attr;
		}

		if (ntfs_attr_map_whole_runlist(na)) {
			ntfs_log_verbose("Internal error\n");
			ntfs_log_error("Can't map runlist (inode %lld)\n", inode_num);
			goto close_attr;
		}

		if (NAttrCompressed(na))
			wiped = wipe_compressed_attribute(vol, byte, act, na);
		else
			wiped = wipe_attribute(vol, byte, act, na);

		if (wiped == -1) {
			ntfs_log_error(" (inode %lld)\n", inode_num);
			goto close_attr;
		}

		if (wiped) {
			ntfs_log_verbose("Wiped %llu bytes\n", wiped);
			total += wiped;
		} else
			ntfs_log_verbose("Nothing to wipe\n");
close_attr:
		ntfs_attr_close(na);
close_inode:
		ntfs_inode_close(ni);
	}
	ntfs_log_quiet("wipe_tails 0x%02x, %lld bytes\n", byte, total);
	return total;
}

/**
 * wipe_mft - Wipe the MFT slack space
 * @vol:   An ntfs volume obtained from ntfs_mount
 * @byte:  Overwrite with this value
 * @act:   Wipe, test or info
 *
 * MFT Records are 1024 bytes long, but some of this space isn't used.  Wipe any
 * unused space at the end of the record and wipe any unused records.
 *
 * Return: >0  Success, the clusters were wiped
 *          0  Nothing to wipe
 *         -1  Error, something went wrong
 */
static s64 wipe_mft(ntfs_volume *vol, int byte, enum action act)
{
	// by considering the individual attributes we might be able to
	// wipe a few more bytes at the attr's tail.
	s64 nr_mft_records, i;
	s64 total = 0;
	s64 result = 0;
	int size = 0;
	u8 *buffer = NULL;

	if (!vol || (byte < 0))
		return -1;

	buffer = malloc(vol->mft_record_size);
	if (!buffer) {
		ntfs_log_error("malloc failed\n");
		return -1;
	}

	nr_mft_records = vol->mft_na->initialized_size >>
			vol->mft_record_size_bits;

	for (i = 0; i < nr_mft_records; i++) {
		if (utils_mftrec_in_use(vol, i)) {
			result = ntfs_attr_mst_pread(vol->mft_na, vol->mft_record_size * i,
				1, vol->mft_record_size, buffer);
			if (result != 1) {
				ntfs_log_error("error attr mst read %lld\n",
						(long long)i);
				total = -1;	// XXX just negate result?
				goto free;
			}

			// We know that the end marker will only take 4 bytes
			size = *((u32*) (buffer + 0x18)) - 4;

			if (act == act_info) {
				//ntfs_log_info("mft %d\n", size);
				total += size;
				continue;
			}

			memset(buffer + size, byte, vol->mft_record_size - size);

			result = ntfs_attr_mst_pwrite(vol->mft_na, vol->mft_record_size * i,
				1, vol->mft_record_size, buffer);
			if (result != 1) {
				ntfs_log_error("error attr mst write %lld\n",
						(long long)i);
				total = -1;
				goto free;
			}

			if ((vol->mft_record_size * (i+1)) <= vol->mftmirr_na->allocated_size)
			{
				// We have to reduce the update sequence number, or else...
				u16 offset;
				u16 usa;
				offset = le16_to_cpu(*(buffer + 0x04));
				usa = le16_to_cpu(*(buffer + offset));
				*((u16*) (buffer + offset)) = cpu_to_le16(usa - 1);

				result = ntfs_attr_mst_pwrite(vol->mftmirr_na, vol->mft_record_size * i,
					1, vol->mft_record_size, buffer);
				if (result != 1) {
					ntfs_log_error("error attr mst write %lld\n",
							(long long)i);
					total = -1;
					goto free;
				}
			}

			total += vol->mft_record_size;
		} else {
			if (act == act_info) {
				total += vol->mft_record_size;
				continue;
			}

			// Build the record from scratch
			memset(buffer, 0, vol->mft_record_size);

			// Common values
			*((u32*) (buffer + 0x00)) = magic_FILE;				// Magic
			*((u16*) (buffer + 0x06)) = cpu_to_le16(0x0003);		// USA size
			*((u16*) (buffer + 0x10)) = cpu_to_le16(0x0001);		// Seq num
			*((u32*) (buffer + 0x1C)) = cpu_to_le32(vol->mft_record_size);	// FILE size
			*((u16*) (buffer + 0x28)) = cpu_to_le16(0x0001);		// Attr ID

			if (vol->major_ver == 3) {
				// Only XP and 2K3
				*((u16*) (buffer + 0x04)) = cpu_to_le16(0x0030);	// USA offset
				*((u16*) (buffer + 0x14)) = cpu_to_le16(0x0038);	// Attr offset
				*((u32*) (buffer + 0x18)) = cpu_to_le32(0x00000040);	// FILE usage
				*((u32*) (buffer + 0x38)) = cpu_to_le32(0xFFFFFFFF);	// End marker
			} else {
				// Only NT and 2K
				*((u16*) (buffer + 0x04)) = cpu_to_le16(0x002A);	// USA offset
				*((u16*) (buffer + 0x14)) = cpu_to_le16(0x0030);	// Attr offset
				*((u32*) (buffer + 0x18)) = cpu_to_le32(0x00000038);	// FILE usage
				*((u32*) (buffer + 0x30)) = cpu_to_le32(0xFFFFFFFF);	// End marker
			}

			result = ntfs_attr_mst_pwrite(vol->mft_na, vol->mft_record_size * i,
				1, vol->mft_record_size, buffer);
			if (result != 1) {
				ntfs_log_error("error attr mst write %lld\n",
						(long long)i);
				total = -1;
				goto free;
			}

			total += vol->mft_record_size;
		}
	}

	ntfs_log_quiet("wipe_mft 0x%02x, %lld bytes\n", byte, (long long)total);
free:
	free(buffer);
	return total;
}

/**
 * wipe_index_allocation - Wipe $INDEX_ALLOCATION attribute
 * @vol:		An ntfs volume obtained from ntfs_mount
 * @byte:		Overwrite with this value
 * @act:		Wipe, test or info
 * @naa:		Opened ntfs $INDEX_ALLOCATION attribute
 * @nab:		Opened ntfs $BITMAP attribute
 * @indx_record_size:	Size of INDX record
 *
 * Return: >0  Success, the clusters were wiped
 *          0  Nothing to wipe
 *         -1  Error, something went wrong
 */
static s64 wipe_index_allocation(ntfs_volume *vol, int byte, enum action act
	__attribute__((unused)), ntfs_attr *naa, ntfs_attr *nab,
	u32 indx_record_size)
{
	s64 total = 0;
	s64 wiped = 0;
	s64 offset = 0;
	s64 obyte = 0;
	u64 wipe_offset;
	s64 wipe_size;
	u8 obit = 0;
	u8 mask;
	u8 *bitmap;
	u8 *buf;

	bitmap = malloc(nab->data_size);
	if (!bitmap) {
		ntfs_log_verbose("malloc failed\n");
		ntfs_log_error("Couldn't allocate %lld bytes", nab->data_size);
		return -1;
	}

	if (ntfs_attr_pread(nab, 0, nab->data_size, bitmap)
						!= nab->data_size) {
		ntfs_log_verbose("Internal error\n");
		ntfs_log_error("Couldn't read $BITMAP");
		total = -1;
		goto free_bitmap;
	}

	buf = malloc(indx_record_size);
	if (!buf) {
		ntfs_log_verbose("malloc failed\n");
		ntfs_log_error("Couldn't allocate %u bytes",
				(unsigned int)indx_record_size);
		total = -1;
		goto free_bitmap;
	}

	while (offset < naa->allocated_size) {
		mask = 1 << obit;
		if (bitmap[obyte] & mask) {
			INDEX_ALLOCATION *indx;

			s64 ret = ntfs_rl_pread(vol, naa->rl,
					offset, indx_record_size, buf);
			if (ret != indx_record_size) {
				ntfs_log_verbose("ntfs_rl_pread failed\n");
				ntfs_log_error("Couldn't read INDX record");
				total = -1;
				goto free_buf;
			}

			indx = (INDEX_ALLOCATION *) buf;
			if (ntfs_mst_post_read_fixup((NTFS_RECORD *)buf,
								indx_record_size))
				ntfs_log_error("damaged fs: mst_post_read_fixup failed");

			if ((le32_to_cpu(indx->index.allocated_size) + 0x18) !=
							indx_record_size) {
				ntfs_log_verbose("Internal error\n");
				ntfs_log_error("INDX record should be %u bytes",
						(unsigned int)indx_record_size);
				total = -1;
				goto free_buf;
			}

			wipe_offset = le32_to_cpu(indx->index.index_length) + 0x18;
			wipe_size = indx_record_size - wipe_offset;
			memset(buf + wipe_offset, byte, wipe_size);
			if (ntfs_mst_pre_write_fixup((NTFS_RECORD *)indx,
								indx_record_size))
				ntfs_log_error("damaged fs: mst_pre_write_protect failed");
			if (opts.verbose > 1)
				ntfs_log_verbose("+");
		} else {
			wipe_size = indx_record_size;
			memset(buf, byte, wipe_size);
			if (opts.verbose > 1)
				ntfs_log_verbose("x");
		}

		wiped = ntfs_rl_pwrite(vol, naa->rl, offset, indx_record_size, buf);
		if (wiped != indx_record_size) {
			ntfs_log_verbose("ntfs_rl_pwrite failed\n");
			ntfs_log_error("Couldn't wipe tail of INDX record");
			total = -1;
			goto free_buf;
		}
		total += wipe_size;

		offset += indx_record_size;
		obit++;
		if (obit > 7) {
			obit = 0;
			obyte++;
		}
	}
	if ((opts.verbose > 1) && (wiped != -1))
		ntfs_log_verbose("\n\t");
free_buf:
	free(buf);
free_bitmap:
	free(bitmap);
	return total;
}

/**
 * get_indx_record_size - determine size of INDX record from $INDEX_ROOT
 * @nar:	Opened ntfs $INDEX_ROOT attribute
 *
 * Return: >0  Success, return INDX record size
 *          0  Error, something went wrong
 */
static u32 get_indx_record_size(ntfs_attr *nar)
{
	u32 indx_record_size;

	if (ntfs_attr_pread(nar, 8, 4, &indx_record_size) != 4) {
		ntfs_log_verbose("Couldn't determine size of INDX record\n");
		ntfs_log_error("ntfs_attr_pread failed");
		return 0;
	}

	indx_record_size = le32_to_cpu(indx_record_size);
	if (!indx_record_size) {
		ntfs_log_verbose("Internal error\n");
		ntfs_log_error("INDX record should be 0");
	}
	return indx_record_size;
}

/**
 * wipe_directory - Wipe the directory indexes
 * @vol:	An ntfs volume obtained from ntfs_mount
 * @byte:	Overwrite with this value
 * @act:	Wipe, test or info
 *
 * Directories are kept in sorted B+ Trees.  Index blocks may not be full.  Wipe
 * the unused space at the ends of these blocks.
 *
 * Return: >0  Success, the clusters were wiped
 *          0  Nothing to wipe
 *         -1  Error, something went wrong
 */
static s64 wipe_directory(ntfs_volume *vol, int byte, enum action act)
{
	s64 total = 0;
	s64 nr_mft_records, inode_num;
	ntfs_inode *ni;
	ntfs_attr *naa;
	ntfs_attr *nab;
	ntfs_attr *nar;

	if (!vol || (byte < 0))
		return -1;

	nr_mft_records = vol->mft_na->initialized_size >>
			vol->mft_record_size_bits;

	for (inode_num = 5; inode_num < nr_mft_records; inode_num++) {
		u32 indx_record_size;
		s64 wiped;

		ntfs_log_verbose("Inode %lld - ", inode_num);
		ni = ntfs_inode_open(vol, inode_num);
		if (!ni) {
			if (opts.verbose > 2)
				ntfs_log_verbose("Could not open inode\n");
			else
				ntfs_log_verbose("\r");
			continue;
		}

		if (ni->mrec->base_mft_record) {
			if (opts.verbose > 2)
				ntfs_log_verbose("Not base mft record. Skipping\n");
			else
				ntfs_log_verbose("\r");
			goto close_inode;
		}

		naa = ntfs_attr_open(ni, AT_INDEX_ALLOCATION, NTFS_INDEX_I30, 4);
		if (!naa) {
			if (opts.verbose > 2)
				ntfs_log_verbose("Couldn't open $INDEX_ALLOCATION\n");
			else
				ntfs_log_verbose("\r");
			goto close_inode;
		}

		if (!NAttrNonResident(naa)) {
			ntfs_log_verbose("Resident $INDEX_ALLOCATION\n");
			ntfs_log_error("damaged fs: Resident $INDEX_ALLOCATION "
					"(inode %lld)\n", inode_num);
			goto close_attr_allocation;
		}

		if (ntfs_attr_map_whole_runlist(naa)) {
			ntfs_log_verbose("Internal error\n");
			ntfs_log_error("Can't map runlist for $INDEX_ALLOCATION "
					"(inode %lld)\n", inode_num);
			goto close_attr_allocation;
		}

		nab = ntfs_attr_open(ni, AT_BITMAP, NTFS_INDEX_I30, 4);
		if (!nab) {
			ntfs_log_verbose("Couldn't open $BITMAP\n");
			ntfs_log_error("damaged fs: $INDEX_ALLOCATION is present, "
					"but we can't open $BITMAP with same "
					"name (inode %lld)\n", inode_num);
			goto close_attr_allocation;
		}

		nar = ntfs_attr_open(ni, AT_INDEX_ROOT, NTFS_INDEX_I30, 4);
		if (!nar) {
			ntfs_log_verbose("Couldn't open $INDEX_ROOT\n");
			ntfs_log_error("damaged fs: $INDEX_ALLOCATION is present, but "
					"we can't open $INDEX_ROOT with same name"
					" (inode %lld)\n", inode_num);
			goto close_attr_bitmap;
		}

		if (NAttrNonResident(nar)) {
			ntfs_log_verbose("Not resident $INDEX_ROOT\n");
			ntfs_log_error("damaged fs: Not resident $INDEX_ROOT "
					"(inode %lld)\n", inode_num);
			goto close_attr_root;
		}

		indx_record_size = get_indx_record_size(nar);
		if (!indx_record_size) {
			ntfs_log_error(" (inode %lld)\n", inode_num);
			goto close_attr_root;
		}

		wiped = wipe_index_allocation(vol, byte, act,
						naa, nab, indx_record_size);
		if (wiped == -1) {
			ntfs_log_error(" (inode %lld)\n", inode_num);
			goto close_attr_root;
		}

		if (wiped) {
			ntfs_log_verbose("Wiped %llu bytes\n", wiped);
			total += wiped;
		} else
			ntfs_log_verbose("Nothing to wipe\n");
close_attr_root:
		ntfs_attr_close(nar);
close_attr_bitmap:
		ntfs_attr_close(nab);
close_attr_allocation:
		ntfs_attr_close(naa);
close_inode:
		ntfs_inode_close(ni);
	}

	ntfs_log_quiet("wipe_directory 0x%02x, %lld bytes\n", byte, total);
	return total;
}

/**
 * wipe_logfile - Wipe the logfile (journal)
 * @vol:   An ntfs volume obtained from ntfs_mount
 * @byte:  Overwrite with this value
 * @act:   Wipe, test or info
 *
 * The logfile journals the metadata to give the volume fault-tolerance.  If the
 * volume is in a consistent state, then this information can be erased.
 *
 * Return: >0  Success, the clusters were wiped
 *          0  Nothing to wipe
 *         -1  Error, something went wrong
 */
static s64 wipe_logfile(ntfs_volume *vol, int byte, enum action act
	__attribute__((unused)))
{
	const int NTFS_BUF_SIZE2 = 8192;
	//FIXME(?): We might need to zero the LSN field of every single mft
	//record as well. (But, first try without doing that and see what
	//happens, since chkdsk might pickup the pieces and do it for us...)
	ntfs_inode *ni;
	ntfs_attr *na;
	s64 len, pos, count;
	char buf[NTFS_BUF_SIZE2];
	int eo;

	/* We can wipe logfile only with 0xff. */
	byte = 0xff;

	if (!vol || (byte < 0))
		return -1;

	//ntfs_log_quiet("wipe_logfile(not implemented) 0x%02x\n", byte);

	if ((ni = ntfs_inode_open(vol, FILE_LogFile)) == NULL) {
		ntfs_log_debug("Failed to open inode FILE_LogFile.\n");
		return -1;
	}

	if ((na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0)) == NULL) {
		ntfs_log_debug("Failed to open $FILE_LogFile/$DATA.\n");
		goto error_exit;
	}

	/* The $DATA attribute of the $LogFile has to be non-resident. */
	if (!NAttrNonResident(na)) {
		ntfs_log_debug("$LogFile $DATA attribute is resident!?!\n");
		errno = EIO;
		goto io_error_exit;
	}

	/* Get length of $LogFile contents. */
	len = na->data_size;
	if (!len) {
		ntfs_log_debug("$LogFile has zero length, no disk write "
				"needed.\n");
		return 0;
	}

	/* Read $LogFile until its end. We do this as a check for correct
	   length thus making sure we are decompressing the mapping pairs
	   array correctly and hence writing below is safe as well. */
	pos = 0;
	while ((count = ntfs_attr_pread(na, pos, NTFS_BUF_SIZE2, buf)) > 0)
		pos += count;

	if (count == -1 || pos != len) {
		ntfs_log_debug("Amount of $LogFile data read does not "
			"correspond to expected length!\n");
		if (count != -1)
			errno = EIO;
		goto io_error_exit;
	}

	/* Fill the buffer with @byte's. */
	memset(buf, byte, NTFS_BUF_SIZE2);

	/* Set the $DATA attribute. */
	pos = 0;
	while ((count = len - pos) > 0) {
		if (count > NTFS_BUF_SIZE2)
			count = NTFS_BUF_SIZE2;

		if ((count = ntfs_attr_pwrite(na, pos, count, buf)) <= 0) {
			ntfs_log_debug("Failed to set the $LogFile attribute "
					"value.\n");
			if (count != -1)
				errno = EIO;
			goto io_error_exit;
		}

		pos += count;
	}

	ntfs_attr_close(na);
	ntfs_inode_close(ni);
	ntfs_log_quiet("wipe_logfile 0x%02x, %lld bytes\n", byte, pos);
	return pos;

io_error_exit:
	eo = errno;
	ntfs_attr_close(na);
	errno = eo;
error_exit:
	eo = errno;
	ntfs_inode_close(ni);
	errno = eo;
	return -1;
}

/**
 * wipe_pagefile - Wipe the pagefile (swap space)
 * @vol:   An ntfs volume obtained from ntfs_mount
 * @byte:  Overwrite with this value
 * @act:   Wipe, test or info
 *
 * pagefile.sys is used by Windows as extra virtual memory (swap space).
 * Windows recreates the file at bootup, so it can be wiped without harm.
 *
 * Return: >0  Success, the clusters were wiped
 *          0  Nothing to wipe
 *         -1  Error, something went wrong
 */
static s64 wipe_pagefile(ntfs_volume *vol, int byte, enum action act
	__attribute__((unused)))
{
	// wipe completely, chkdsk doesn't do anything, booting writes header
	const int NTFS_BUF_SIZE2 = 4096;
	ntfs_inode *ni;
	ntfs_attr *na;
	s64 len, pos, count;
	char buf[NTFS_BUF_SIZE2];
	int eo;

	if (!vol || (byte < 0))
		return -1;

	//ntfs_log_quiet("wipe_pagefile(not implemented) 0x%02x\n", byte);

	ni = ntfs_pathname_to_inode(vol, NULL, "pagefile.sys");
	if (!ni) {
		ntfs_log_debug("Failed to open inode of pagefile.sys.\n");
		return 0;
	}

	if ((na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0)) == NULL) {
		ntfs_log_debug("Failed to open pagefile.sys/$DATA.\n");
		goto error_exit;
	}

	/* The $DATA attribute of the pagefile.sys has to be non-resident. */
	if (!NAttrNonResident(na)) {
		ntfs_log_debug("pagefile.sys $DATA attribute is resident!?!\n");
		errno = EIO;
		goto io_error_exit;
	}

	/* Get length of pagefile.sys contents. */
	len = na->data_size;
	if (!len) {
		ntfs_log_debug("pagefile.sys has zero length, no disk write "
				"needed.\n");
		return 0;
	}

	memset(buf, byte, NTFS_BUF_SIZE2);

	/* Set the $DATA attribute. */
	pos = 0;
	while ((count = len - pos) > 0) {
		if (count > NTFS_BUF_SIZE2)
			count = NTFS_BUF_SIZE2;

		if ((count = ntfs_attr_pwrite(na, pos, count, buf)) <= 0) {
			ntfs_log_debug("Failed to set the pagefile.sys "
					"attribute value.\n");
			if (count != -1)
				errno = EIO;
			goto io_error_exit;
		}

		pos += count;
	}

	ntfs_attr_close(na);
	ntfs_inode_close(ni);
	ntfs_log_quiet("wipe_pagefile 0x%02x, %lld bytes\n", byte, pos);
	return pos;

io_error_exit:
	eo = errno;
	ntfs_attr_close(na);
	errno = eo;
error_exit:
	eo = errno;
	ntfs_inode_close(ni);
	errno = eo;
	return -1;
}

/**
 * print_summary - Tell the user what we are about to do
 *
 * List the operations about to be performed.  The output will be silenced by
 * the --quiet option.
 *
 * Return:  none
 */
static void print_summary(void)
{
	int i;

	if (opts.noaction)
		ntfs_log_quiet("%s is in 'no-action' mode, it will NOT write to disk."
			 "\n\n", EXEC_NAME);

	ntfs_log_quiet("%s is about to wipe:\n", EXEC_NAME);
	if (opts.unused)
		ntfs_log_quiet("\tunused disk space\n");
	if (opts.tails)
		ntfs_log_quiet("\tfile tails\n");
	if (opts.mft)
		ntfs_log_quiet("\tunused mft areas\n");
	if (opts.directory)
		ntfs_log_quiet("\tunused directory index space\n");
	if (opts.logfile)
		ntfs_log_quiet("\tthe logfile (journal)\n");
	if (opts.pagefile)
		ntfs_log_quiet("\tthe pagefile (swap space)\n");

	ntfs_log_quiet("\n%s will overwrite these areas with: ", EXEC_NAME);
	if (opts.bytes) {
		for (i = 0; opts.bytes[i] >= 0; i++)
			ntfs_log_quiet("0x%02x ", opts.bytes[i]);
	}
	ntfs_log_quiet("\n");

	if (opts.count > 1)
		ntfs_log_quiet("%s will repeat these operations %d times.\n", EXEC_NAME, opts.count);
	ntfs_log_quiet("\n");
}

/**
 * main - Begin here
 *
 * Start from here.
 *
 * Return:  0  Success, the program worked
 *	    1  Error, something went wrong
 */
int main(int argc, char *argv[])
{
	ntfs_volume *vol;
	int result = 1;
	int flags = 0;
	int i, j;
	enum action act = act_info;

	ntfs_log_set_handler(ntfs_log_handler_outerr);

	if (!parse_options(argc, argv))
		return 1;

	utils_set_locale();

	if (!opts.info)
		print_summary();

	if (opts.info || opts.noaction)
		flags = NTFS_MNT_RDONLY;
	if (opts.force)
		flags |= NTFS_MNT_FORCE;

	vol = utils_mount_volume(opts.device, flags);
	if (!vol)
		goto free;

	if (NVolWasDirty(vol) && !opts.force)
		goto umount;

	if (opts.info) {
		act = act_info;
		opts.count = 1;
	} else if (opts.noaction) {
		act = act_test;
	} else {
		act = act_wipe;
	}

	/* Even if the output it quieted, you still get 5 seconds to abort. */
	if ((act == act_wipe) && !opts.force) {
		ntfs_log_quiet("\n%s will begin in 5 seconds, press CTRL-C to abort.\n", EXEC_NAME);
		sleep(5);
	}

	ntfs_log_info("\n");
	for (i = 0; i < opts.count; i++) {
		int byte;
		s64 total = 0;
		s64 wiped = 0;

		for (j = 0; byte = opts.bytes[j], byte >= 0; j++) {

			if (opts.directory) {
				wiped = wipe_directory(vol, byte, act);
				if (wiped < 0)
					goto umount;
				else
					total += wiped;
			}

			if (opts.tails) {
				wiped = wipe_tails(vol, byte, act);
				if (wiped < 0)
					goto umount;
				else
					total += wiped;
			}

			if (opts.logfile) {
				wiped = wipe_logfile(vol, byte, act);
				if (wiped < 0)
					goto umount;
				else
					total += wiped;
			}

			if (opts.mft) {
				wiped = wipe_mft(vol, byte, act);
				if (wiped < 0)
					goto umount;
				else
					total += wiped;
			}

			if (opts.pagefile) {
				wiped = wipe_pagefile(vol, byte, act);
				if (wiped < 0)
					goto umount;
				else
					total += wiped;
			}

			if (opts.unused) {
				wiped = wipe_unused(vol, byte, act);
				if (wiped < 0)
					goto umount;
				else
					total += wiped;
			}

			if (act == act_info)
				break;
		}

		ntfs_log_info("%lld bytes were wiped\n", (long long)total);
	}
	result = 0;
umount:
	ntfs_umount(vol, FALSE);
free:
	if (opts.bytes)
		free(opts.bytes);
	return result;
}

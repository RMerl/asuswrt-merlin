/**
 * ntfsinfo - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2004 Matthew J. Fanto
 * Copyright (c) 2002-2006 Anton Altaparmakov
 * Copyright (c) 2002-2005 Richard Russon
 * Copyright (c) 2003-2006 Szabolcs Szakacsits
 * Copyright (c) 2004-2005 Yuval Fledel
 * Copyright (c) 2004-2007 Yura Pakhuchiy
 * Copyright (c)      2005 Cristian Klein
 *
 * This utility will dump a file's attributes.
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
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * TODO LIST:
 *	- Better error checking. (focus on ntfs_dump_volume)
 *	- Comment things better.
 *	- More things at verbose mode.
 *	- Dump ACLs when security_id exists (NTFS 3+ only).
 *	- Clean ups.
 *	- Internationalization.
 *	- Add more Indexed Attr Types.
 *	- Make formatting look more like www.flatcap.org/ntfs/info
 *
 *	Still not dumping certain attributes. Need to find the best
 *	way to output some of these attributes.
 *
 *	Still need to do:
 *	    $REPARSE_POINT/$SYMBOLIC_LINK
 *	    $LOGGED_UTILITY_STREAM
 */

#include "config.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "types.h"
#include "mft.h"
#include "attrib.h"
#include "layout.h"
#include "inode.h"
#include "index.h"
#include "utils.h"
#include "security.h"
#include "mst.h"
#include "dir.h"
#include "ntfstime.h"
#include "version.h"
#include "support.h"

static const char *EXEC_NAME = "ntfsinfo";

static struct options {
	const char *device;	/* Device/File to work with */
	const char *filename;	/* Resolve this filename to mft number */
	s64	 inode;		/* Info for this inode */
	int	 debug;		/* Debug output */
	int	 quiet;		/* Less output */
	int	 verbose;	/* Extra output */
	int	 force;		/* Override common sense */
	int	 notime;	/* Don't report timestamps at all */
	int	 mft;		/* Dump information about the volume as well */
} opts;

/**
 * version - Print version information about the program
 *
 * Print a copyright statement and a brief description of the program.
 *
 * Return:  none
 */
static void version(void)
{
	printf("\n%s v%s (libntfs %s) - Display information about an NTFS "
			"Volume.\n\n", EXEC_NAME, VERSION,
			ntfs_libntfs_version());
	printf("Copyright (c)\n");
	printf("    2002-2004 Matthew J. Fanto\n");
	printf("    2002-2006 Anton Altaparmakov\n");
	printf("    2002-2005 Richard Russon\n");
	printf("    2003-2006 Szabolcs Szakacsits\n");
	printf("    2003      Leonard Norrgård\n");
	printf("    2004-2005 Yuval Fledel\n");
	printf("    2004-2007 Yura Pakhuchiy\n");
	printf("\n%s\n%s%s\n", ntfs_gpl, ntfs_bugs, ntfs_home);
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
	printf("\nUsage: %s [options] device\n"
		"    -i, --inode NUM  Display information about this inode\n"
		"    -F, --file FILE  Display information about this file (absolute path)\n"
		"    -m, --mft        Dump information about the volume\n"
		"    -t, --notime     Don't report timestamps\n"
		"\n"
		"    -f, --force      Use less caution\n"
		"    -q, --quiet      Less output\n"
		"    -v, --verbose    More output\n"
		"    -V, --version    Display version information\n"
		"    -h, --help       Display this help\n"
#ifdef DEBUG
                "    -d, --debug            Show debug information\n"
#endif
	        "\n",
		EXEC_NAME);
	printf("%s%s\n", ntfs_bugs, ntfs_home);
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
	static const char *sopt = "-:dfhi:F:mqtTvV";
	static const struct option lopt[] = {
#ifdef DEBUG
                { "debug",      no_argument,            NULL, 'd' },
#endif
		{ "force",	 no_argument,		NULL, 'f' },
		{ "help",	 no_argument,		NULL, 'h' },
		{ "inode",	 required_argument,	NULL, 'i' },
		{ "file",	 required_argument,	NULL, 'F' },
		{ "quiet",	 no_argument,		NULL, 'q' },
		{ "verbose",	 no_argument,		NULL, 'v' },
		{ "version",	 no_argument,		NULL, 'V' },
		{ "notime",	 no_argument,		NULL, 'T' },
		{ "mft",	 no_argument,		NULL, 'm' },
		{ NULL,		 0,			NULL,  0  }
	};

	int c = -1;
	int err  = 0;
	int ver  = 0;
	int help = 0;
	int levels = 0;

	opterr = 0; /* We'll handle the errors, thank you. */

	opts.inode = -1;
	opts.filename = NULL;

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:
			if (!opts.device)
				opts.device = optarg;
			else
				err++;
			break;
		case 'd':
			opts.debug++;
			break;
		case 'i':
			if ((opts.inode != -1) ||
			    (!utils_parse_size(optarg, &opts.inode, FALSE))) {
				err++;
			}
			break;
		case 'F':
			if (opts.filename == NULL) {
				/* The inode can not be resolved here,
				   store the filename */
				opts.filename = argv[optind-1];
			} else {
				/* "-F" can't appear more than once */
				err++;
			}
			break;
		case 'f':
			opts.force++;
			break;
		case 'h':
			help++;
			break;
		case 'q':
			opts.quiet++;
			ntfs_log_clear_levels(NTFS_LOG_LEVEL_QUIET);
			break;
		case 't':
			opts.notime++;
			break;
		case 'T':
			/* 'T' is deprecated, notify */
			ntfs_log_error("Option 'T' is deprecated, it was "
				"replaced by 't'.\n");
			err++;
			break;
		case 'v':
			opts.verbose++;
			ntfs_log_set_levels(NTFS_LOG_LEVEL_VERBOSE);
			break;
		case 'V':
			ver++;
			break;
		case 'm':
			opts.mft++;
			break;
		case '?':
			if (optopt=='?') {
				help++;
				continue;
			}
			if (ntfs_log_parse_option(argv[optind-1]))
				continue;
			ntfs_log_error("Unknown option '%s'.\n",
					argv[optind-1]);
			err++;
			break;
		case ':':
			ntfs_log_error("Option '%s' requires an "
					"argument.\n", argv[optind-1]);
			err++;
			break;
		default:
			ntfs_log_error("Unhandled option case: %d.\n", c);
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
				ntfs_log_error("You must specify exactly one "
					"device.\n");
			err++;
		}

		if ((opts.inode == -1) && (opts.filename == NULL) && !opts.mft) {
			if (argc > 1)
				ntfs_log_error("You must specify an inode to "
					"learn about.\n");
			err++;
		}

		if (opts.quiet && opts.verbose) {
			ntfs_log_error("You may not use --quiet and --verbose "
				"at the same time.\n");
			err++;
		}

		if ((opts.inode != -1) && (opts.filename != NULL)) {
			if (argc > 1)
				ntfs_log_error("You may not specify --inode "
					"and --file together.\n");
			err++;
		}

	}

#ifdef DEBUG
	if (!opts.debug)
		if (!freopen("/dev/null", "w", stderr)) {
			ntfs_log_perror("Failed to freopen stderr to /dev/null");
			exit(1);
		}
#endif

	if (ver)
		version();
	if (help || err)
		usage();

	return (!err && !help && !ver);
}


/* *************** utility functions ******************** */
/**
 * ntfsinfo_time_to_str() -
 * @sle_ntfs_clock:	on disk time format in 100ns units since 1st jan 1601
 *			in little-endian format
 *
 * Return char* in a format 'Thu Jan  1 00:00:00 1970'.
 * No need to free the returned memory.
 *
 * Example of usage:
 *	char *time_str = ntfsinfo_time_to_str(
 *			sle64_to_cpu(standard_attr->creation_time));
 *	printf("\tFile Creation Time:\t %s", time_str);
 */
static char *ntfsinfo_time_to_str(const sle64 sle_ntfs_clock)
{
	time_t unix_clock = ntfs2utc(sle_ntfs_clock);
	return ctime(&unix_clock);
}

/**
 * ntfs_attr_get_name()
 * @attr:	a valid attribute record
 *
 * return multi-byte string containing the attribute name if exist. the user
 *             is then responsible of freeing that memory.
 *        null if no name exists (attr->name_length==0). no memory allocated.
 *        null if cannot convert to multi-byte string. errno would contain the
 *             error id. no memory allocated in that case
 */
static char *ntfs_attr_get_name_mbs(ATTR_RECORD *attr)
{
	ntfschar *ucs_attr_name;
	char *mbs_attr_name = NULL;
	int mbs_attr_name_size;

	/* Get name in unicode. */
	ucs_attr_name = ntfs_attr_get_name(attr);
	/* Convert unicode to printable format. */
	mbs_attr_name_size = ntfs_ucstombs(ucs_attr_name, attr->name_length,
			&mbs_attr_name, 0);
	if (mbs_attr_name_size > 0)
		return mbs_attr_name;
	else
		return NULL;
}


/* *************** functions for dumping global info ******************** */
/**
 * ntfs_dump_volume - dump information about the volume
 */
static void ntfs_dump_volume(ntfs_volume *vol)
{
	printf("Volume Information \n");
	printf("\tName of device: %s\n", vol->dev->d_name);
	printf("\tDevice state: %lu\n", vol->dev->d_state);
	printf("\tVolume Name: %s\n", vol->vol_name);
	printf("\tVolume State: %lu\n", vol->state);
	printf("\tVolume Version: %u.%u\n", vol->major_ver, vol->minor_ver);
	printf("\tSector Size: %hu\n", vol->sector_size);
	printf("\tCluster Size: %u\n", (unsigned int)vol->cluster_size);
	printf("\tVolume Size in Clusters: %lld\n",
			(long long)vol->nr_clusters);

	printf("MFT Information \n");
	printf("\tMFT Record Size: %u\n", (unsigned int)vol->mft_record_size);
	printf("\tMFT Zone Multiplier: %u\n", vol->mft_zone_multiplier);
	printf("\tMFT Data Position: %lld\n", (long long)vol->mft_data_pos);
	printf("\tMFT Zone Start: %lld\n", (long long)vol->mft_zone_start);
	printf("\tMFT Zone End: %lld\n", (long long)vol->mft_zone_end);
	printf("\tMFT Zone Position: %lld\n", (long long)vol->mft_zone_pos);
	printf("\tCurrent Position in First Data Zone: %lld\n",
			(long long)vol->data1_zone_pos);
	printf("\tCurrent Position in Second Data Zone: %lld\n",
			(long long)vol->data2_zone_pos);
	printf("\tLCN of Data Attribute for FILE_MFT: %lld\n",
			(long long)vol->mft_lcn);
	printf("\tFILE_MFTMirr Size: %d\n", vol->mftmirr_size);
	printf("\tLCN of Data Attribute for File_MFTMirr: %lld\n",
			(long long)vol->mftmirr_lcn);
	printf("\tSize of Attribute Definition Table: %d\n",
			(int)vol->attrdef_len);

	printf("FILE_Bitmap Information \n");
	printf("\tFILE_Bitmap MFT Record Number: %llu\n",
			(unsigned long long)vol->lcnbmp_ni->mft_no);
	printf("\tState of FILE_Bitmap Inode: %lu\n", vol->lcnbmp_ni->state);
	printf("\tLength of Attribute List: %u\n",
			(unsigned int)vol->lcnbmp_ni->attr_list_size);
	printf("\tAttribute List: %s\n", vol->lcnbmp_ni->attr_list);
	printf("\tNumber of Attached Extent Inodes: %d\n",
			(int)vol->lcnbmp_ni->nr_extents);
	/* FIXME: need to add code for the union if nr_extens != 0, but
	   i dont know if it will ever != 0 with FILE_Bitmap */

	printf("FILE_Bitmap Data Attribute Information\n");
	printf("\tDecompressed Runlist: not done yet\n");
	printf("\tBase Inode: %llu\n",
			(unsigned long long)vol->lcnbmp_na->ni->mft_no);
	printf("\tAttribute Types: not done yet\n");
	//printf("\tAttribute Name: %s\n", vol->lcnbmp_na->name);
	printf("\tAttribute Name Length: %u\n",
			(unsigned int)vol->lcnbmp_na->name_len);
	printf("\tAttribute State: %lu\n", vol->lcnbmp_na->state);
	printf("\tAttribute Allocated Size: %lld\n",
			(long long)vol->lcnbmp_na->allocated_size);
	printf("\tAttribute Data Size: %lld\n",
			(long long)vol->lcnbmp_na->data_size);
	printf("\tAttribute Initialized Size: %lld\n",
			(long long)vol->lcnbmp_na->initialized_size);
	printf("\tAttribute Compressed Size: %lld\n",
			(long long)vol->lcnbmp_na->compressed_size);
	printf("\tCompression Block Size: %u\n",
			(unsigned int)vol->lcnbmp_na->compression_block_size);
	printf("\tCompression Block Size Bits: %u\n",
			vol->lcnbmp_na->compression_block_size_bits);
	printf("\tCompression Block Clusters: %u\n",
			vol->lcnbmp_na->compression_block_clusters);

	//TODO: Still need to add a few more attributes
}

/**
 * ntfs_dump_flags - Dump flags for STANDARD_INFORMATION and FILE_NAME.
 * @type:	dump flags for this attribute type
 * @flags:	flags for dumping
 */
static void ntfs_dump_flags(const char *indent, ATTR_TYPES type, le32 flags)
{
	printf("%sFile attributes:\t", indent);
	if (flags & FILE_ATTR_READONLY) {
		printf(" READONLY");
		flags &= ~FILE_ATTR_READONLY;
	}
	if (flags & FILE_ATTR_HIDDEN) {
		printf(" HIDDEN");
		flags &= ~FILE_ATTR_HIDDEN;
	}
	if (flags & FILE_ATTR_SYSTEM) {
		printf(" SYSTEM");
		flags &= ~FILE_ATTR_SYSTEM;
	}
	if (flags & FILE_ATTR_DIRECTORY) {
		printf(" DIRECTORY");
		flags &= ~FILE_ATTR_DIRECTORY;
	}
	if (flags & FILE_ATTR_ARCHIVE) {
		printf(" ARCHIVE");
		flags &= ~FILE_ATTR_ARCHIVE;
	}
	if (flags & FILE_ATTR_DEVICE) {
		printf(" DEVICE");
		flags &= ~FILE_ATTR_DEVICE;
	}
	if (flags & FILE_ATTR_NORMAL) {
		printf(" NORMAL");
		flags &= ~FILE_ATTR_NORMAL;
	}
	if (flags & FILE_ATTR_TEMPORARY) {
		printf(" TEMPORARY");
		flags &= ~FILE_ATTR_TEMPORARY;
	}
	if (flags & FILE_ATTR_SPARSE_FILE) {
		printf(" SPARSE_FILE");
		flags &= ~FILE_ATTR_SPARSE_FILE;
	}
	if (flags & FILE_ATTR_REPARSE_POINT) {
		printf(" REPARSE_POINT");
		flags &= ~FILE_ATTR_REPARSE_POINT;
	}
	if (flags & FILE_ATTR_COMPRESSED) {
		printf(" COMPRESSED");
		flags &= ~FILE_ATTR_COMPRESSED;
	}
	if (flags & FILE_ATTR_OFFLINE) {
		printf(" OFFLINE");
		flags &= ~FILE_ATTR_OFFLINE;
	}
	if (flags & FILE_ATTR_NOT_CONTENT_INDEXED) {
		printf(" NOT_CONTENT_INDEXED");
		flags &= ~FILE_ATTR_NOT_CONTENT_INDEXED;
	}
	if (flags & FILE_ATTR_ENCRYPTED) {
		printf(" ENCRYPTED");
		flags &= ~FILE_ATTR_ENCRYPTED;
	}
	/* We know that FILE_ATTR_I30_INDEX_PRESENT only exists on $FILE_NAME,
	   and in case we are wrong, let it appear as UNKNOWN */
	if (type == AT_FILE_NAME) {
		if (flags & FILE_ATTR_I30_INDEX_PRESENT) {
			printf(" I30_INDEX");
			flags &= ~FILE_ATTR_I30_INDEX_PRESENT;
		}
	}
	if (flags & FILE_ATTR_VIEW_INDEX_PRESENT) {
		printf(" VIEW_INDEX");
		flags &= ~FILE_ATTR_VIEW_INDEX_PRESENT;
	}
	if (flags)
		printf(" UNKNOWN: 0x%08x", (unsigned int)le32_to_cpu(flags));
	/* Print all the flags in hex. */
	printf(" (0x%08x)\n", (unsigned)le32_to_cpu(flags));
}

/**
 * ntfs_dump_namespace
 */
static void ntfs_dump_namespace(const char *indent, u8 file_name_type)
{
	const char *mbs_file_type;

	/* name space */
	switch (file_name_type) {
	case FILE_NAME_POSIX:
		mbs_file_type = "POSIX";
		break;
	case FILE_NAME_WIN32:
		mbs_file_type = "Win32";
		break;
	case FILE_NAME_DOS:
		mbs_file_type = "DOS";
		break;
	case FILE_NAME_WIN32_AND_DOS:
		mbs_file_type = "Win32 & DOS";
		break;
	default:
		mbs_file_type = "(unknown)";
	}
	printf("%sNamespace:\t\t %s\n", indent, mbs_file_type);
}

/* *************** functions for dumping attributes ******************** */
/**
 * ntfs_dump_standard_information
 */
static void ntfs_dump_attr_standard_information(ATTR_RECORD *attr)
{
	STANDARD_INFORMATION *standard_attr = NULL;
	u32 value_length;

	standard_attr = (STANDARD_INFORMATION*)((char *)attr +
		le16_to_cpu(attr->value_offset));

	/* time conversion stuff */
	if (!opts.notime) {
		char *ntfs_time_str = NULL;

		ntfs_time_str = ntfsinfo_time_to_str(standard_attr->creation_time);
		printf("\tFile Creation Time:\t %s",ntfs_time_str);

		ntfs_time_str = ntfsinfo_time_to_str(
			standard_attr->last_data_change_time);
		printf("\tFile Altered Time:\t %s",ntfs_time_str);

		ntfs_time_str = ntfsinfo_time_to_str(
			standard_attr->last_mft_change_time);
		printf("\tMFT Changed Time:\t %s",ntfs_time_str);

		ntfs_time_str = ntfsinfo_time_to_str(standard_attr->last_access_time);
		printf("\tLast Accessed Time:\t %s",ntfs_time_str);
	}
	ntfs_dump_flags("\t", attr->type, standard_attr->file_attributes);

	value_length = le32_to_cpu(attr->value_length);
	if (value_length == 48) {
		/* Only 12 reserved bytes here */
	} else if (value_length == 72) {
		printf("\tMaximum versions:\t %u \n", (unsigned int)
				le32_to_cpu(standard_attr->maximum_versions));
		printf("\tVersion number:\t\t %u \n", (unsigned int)
				le32_to_cpu(standard_attr->version_number));
		printf("\tClass ID:\t\t %u \n",
			(unsigned int)le32_to_cpu(standard_attr->class_id));
		printf("\tUser ID:\t\t %u (0x%x)\n",
			(unsigned int)le32_to_cpu(standard_attr->owner_id),
			(unsigned int)le32_to_cpu(standard_attr->owner_id));
		printf("\tSecurity ID:\t\t %u (0x%x)\n",
			(unsigned int)le32_to_cpu(standard_attr->security_id),
			(unsigned int)le32_to_cpu(standard_attr->security_id));
		printf("\tQuota charged:\t\t %llu (0x%llx)\n",
				(unsigned long long)
				le64_to_cpu(standard_attr->quota_charged),
				(unsigned long long)
				le64_to_cpu(standard_attr->quota_charged));
		printf("\tUpdate Sequence Number:\t %llu (0x%llx)\n",
				(unsigned long long)
				le64_to_cpu(standard_attr->usn),
				(unsigned long long)
				le64_to_cpu(standard_attr->usn));
	} else {
		printf("\tSize of STANDARD_INFORMATION is %u (0x%x).  It "
				"should be either 72 or 48, something is "
				"wrong...\n", (unsigned int)value_length,
				(unsigned)value_length);
	}
}

static void ntfs_dump_bytes(u8 *buf, int start, int stop)
{
	int i;

	for (i = start; i < stop; i++) {
		printf("%02x ", buf[i]);
	}
}

/**
 * ntfs_dump_attr_list()
 */
static void ntfs_dump_attr_list(ATTR_RECORD *attr, ntfs_volume *vol)
{
	ATTR_LIST_ENTRY *entry;
	u8 *value;
	s64 l;

	if (!opts.verbose)
		return;

	l = ntfs_get_attribute_value_length(attr);
	if (!l) {
		ntfs_log_perror("ntfs_get_attribute_value_length failed");
		return;
	}
	value = ntfs_malloc(l);
	if (!value)
		return;

	l = ntfs_get_attribute_value(vol, attr, value);
	if (!l) {
		ntfs_log_perror("ntfs_get_attribute_value failed");
		free(value);
		return;
	}
	printf("\tDumping attribute list:");
	entry = (ATTR_LIST_ENTRY *) value;
	for (;(u8 *)entry < (u8 *) value + l; entry = (ATTR_LIST_ENTRY *)
				((u8 *) entry + le16_to_cpu(entry->length))) {
		printf("\n");
		printf("\t\tAttribute type:\t0x%x\n",
				(unsigned int)le32_to_cpu(entry->type));
		printf("\t\tRecord length:\t%u (0x%x)\n",
				(unsigned)le16_to_cpu(entry->length),
				(unsigned)le16_to_cpu(entry->length));
		printf("\t\tName length:\t%u (0x%x)\n",
				(unsigned)entry->name_length,
				(unsigned)entry->name_length);
		printf("\t\tName offset:\t%u (0x%x)\n",
				(unsigned)entry->name_offset,
				(unsigned)entry->name_offset);
		printf("\t\tStarting VCN:\t%lld (0x%llx)\n",
				(long long)sle64_to_cpu(entry->lowest_vcn),
				(unsigned long long)
				sle64_to_cpu(entry->lowest_vcn));
		printf("\t\tMFT reference:\t%lld (0x%llx)\n",
				(unsigned long long)
				MREF_LE(entry->mft_reference),
				(unsigned long long)
				MREF_LE(entry->mft_reference));
		printf("\t\tInstance:\t%u (0x%x)\n",
				(unsigned)le16_to_cpu(entry->instance),
				(unsigned)le16_to_cpu(entry->instance));
		printf("\t\tName:\t\t");
		if (entry->name_length) {
			char *name = NULL;
			int name_size;

			name_size = ntfs_ucstombs(entry->name,
					entry->name_length, &name, 0);

			if (name_size > 0) {
				printf("%s\n", name);
				free(name);
			} else
				ntfs_log_perror("ntfs_ucstombs failed");
		} else
			printf("unnamed\n");
		printf("\t\tPadding:\t");
		ntfs_dump_bytes((u8 *)entry, entry->name_offset +
				sizeof(ntfschar) * entry->name_length,
				le16_to_cpu(entry->length));
		printf("\n");
	}
	free(value);
	printf("\tEnd of attribute list reached.\n");
}

/**
 * ntfs_dump_filename()
 */
static void ntfs_dump_filename(const char *indent,
		FILE_NAME_ATTR *file_name_attr)
{
	printf("%sParent directory:\t %lld (0x%llx)\n", indent,
			(long long)MREF_LE(file_name_attr->parent_directory),
			(long long)MREF_LE(file_name_attr->parent_directory));
	/* time stuff */
	if (!opts.notime) {
		char *ntfs_time_str;

		ntfs_time_str = ntfsinfo_time_to_str(
				file_name_attr->creation_time);
		printf("%sFile Creation Time:\t %s", indent, ntfs_time_str);

		ntfs_time_str = ntfsinfo_time_to_str(
				file_name_attr->last_data_change_time);
		printf("%sFile Altered Time:\t %s", indent, ntfs_time_str);

		ntfs_time_str = ntfsinfo_time_to_str(
				file_name_attr->last_mft_change_time);
		printf("%sMFT Changed Time:\t %s", indent, ntfs_time_str);

		ntfs_time_str = ntfsinfo_time_to_str(
				file_name_attr->last_access_time);
		printf("%sLast Accessed Time:\t %s", indent, ntfs_time_str);
	}
	/* other basic stuff about the file */
	printf("%sAllocated Size:\t\t %lld (0x%llx)\n", indent, (long long)
			sle64_to_cpu(file_name_attr->allocated_size),
			(unsigned long long)
			sle64_to_cpu(file_name_attr->allocated_size));
	printf("%sData Size:\t\t %lld (0x%llx)\n", indent,
			(long long)sle64_to_cpu(file_name_attr->data_size),
			(unsigned long long)
			sle64_to_cpu(file_name_attr->data_size));
	printf("%sFilename Length:\t %d (0x%x)\n", indent,
			(unsigned)file_name_attr->file_name_length,
			(unsigned)file_name_attr->file_name_length);
	ntfs_dump_flags(indent, AT_FILE_NAME, file_name_attr->file_attributes);
	if (file_name_attr->file_attributes & FILE_ATTR_REPARSE_POINT &&
			file_name_attr->reparse_point_tag)
		printf("%sReparse point tag:\t 0x%x\n", indent, (unsigned)
				le32_to_cpu(file_name_attr->reparse_point_tag));
	else if (file_name_attr->reparse_point_tag) {
		printf("%sEA Length:\t\t %d (0x%x)\n", indent, (unsigned)
				le16_to_cpu(file_name_attr->packed_ea_size),
				(unsigned)
				le16_to_cpu(file_name_attr->packed_ea_size));
		if (file_name_attr->reserved)
			printf("%sReserved:\t\t %d (0x%x)\n", indent,
					(unsigned)
					le16_to_cpu(file_name_attr->reserved),
					(unsigned)
					le16_to_cpu(file_name_attr->reserved));
	}
	/* The filename. */
	ntfs_dump_namespace(indent, file_name_attr->file_name_type);
	if (file_name_attr->file_name_length > 0) {
		/* but first we need to convert the little endian unicode string
		   into a printable format */
		char *mbs_file_name = NULL;
		int mbs_file_name_size;

		mbs_file_name_size = ntfs_ucstombs(file_name_attr->file_name,
			file_name_attr->file_name_length,&mbs_file_name,0);

		if (mbs_file_name_size>0) {
			printf("%sFilename:\t\t '%s'\n", indent, mbs_file_name);
			free(mbs_file_name);
		} else {
			/* an error occurred, errno holds the reason - notify the user */
			ntfs_log_perror("ntfsinfo error: could not parse file name");
		}
	} else {
		printf("%sFile Name:\t\t unnamed?!?\n", indent);
	}
}

/**
 * ntfs_dump_attr_file_name()
 */
static void ntfs_dump_attr_file_name(ATTR_RECORD *attr)
{
	ntfs_dump_filename("\t", (FILE_NAME_ATTR*)((u8*)attr +
			le16_to_cpu(attr->value_offset)));
}

/**
 * ntfs_dump_object_id
 *
 * dump the $OBJECT_ID attribute - not present on all systems
 */
static void ntfs_dump_attr_object_id(ATTR_RECORD *attr,ntfs_volume *vol)
{
	OBJECT_ID_ATTR *obj_id_attr = NULL;

	obj_id_attr = (OBJECT_ID_ATTR *)((u8*)attr +
			le16_to_cpu(attr->value_offset));

	if (vol->major_ver >= 3.0) {
		u32 value_length;
		char printable_GUID[37];

		value_length = le32_to_cpu(attr->value_length);

		/* Object ID is mandatory. */
		ntfs_guid_to_mbs(&obj_id_attr->object_id, printable_GUID);
		printf("\tObject ID:\t\t %s\n", printable_GUID);

		/* Dump Birth Volume ID. */
		if ((value_length > sizeof(GUID)) && !ntfs_guid_is_zero(
				&obj_id_attr->birth_volume_id)) {
			ntfs_guid_to_mbs(&obj_id_attr->birth_volume_id,
					printable_GUID);
			printf("\tBirth Volume ID:\t\t %s\n", printable_GUID);
		} else
			printf("\tBirth Volume ID:\t missing\n");

		/* Dumping Birth Object ID */
		if ((value_length > sizeof(GUID)) && !ntfs_guid_is_zero(
				&obj_id_attr->birth_object_id)) {
			ntfs_guid_to_mbs(&obj_id_attr->birth_object_id,
					printable_GUID);
			printf("\tBirth Object ID:\t\t %s\n", printable_GUID);
		} else
			printf("\tBirth Object ID:\t missing\n");

		/* Dumping Domain_id - reserved for now */
		if ((value_length > sizeof(GUID)) && !ntfs_guid_is_zero(
				&obj_id_attr->domain_id)) {
			ntfs_guid_to_mbs(&obj_id_attr->domain_id,
					printable_GUID);
			printf("\tDomain ID:\t\t\t %s\n", printable_GUID);
		} else
			printf("\tDomain ID:\t\t missing\n");
	} else
		printf("\t$OBJECT_ID not present. Only NTFS versions > 3.0\n"
			"\thave $OBJECT_ID. Your version of NTFS is %d.\n",
				vol->major_ver);
}

/**
 * ntfs_dump_acl
 *
 * given an acl, print it in a beautiful & lovely way.
 */
static void ntfs_dump_acl(const char *prefix, ACL *acl)
{
	unsigned int i;
	u16 ace_count;
	ACCESS_ALLOWED_ACE *ace;

	printf("%sRevision\t %u\n", prefix, acl->revision);

	/*
	 * Do not recalculate le16_to_cpu every iteration (minor speedup on
	 * big-endian machines.
	 */
	ace_count = le16_to_cpu(acl->ace_count);

	/* initialize 'ace' to the first ace (if any) */
	ace = (ACCESS_ALLOWED_ACE *)((char *)acl + 8);

	/* iterate through ACE's */
	for (i = 1; i <= ace_count; i++) {
		const char *ace_type;
		char *sid;

		/* set ace_type. */
		switch (ace->type) {
		case ACCESS_ALLOWED_ACE_TYPE:
			ace_type = "allow";
			break;
		case ACCESS_DENIED_ACE_TYPE:
			ace_type = "deny";
			break;
		case SYSTEM_AUDIT_ACE_TYPE:
			ace_type = "audit";
			break;
		default:
			ace_type = "unknown";
			break;
		}

		printf("%sACE:\t\t type:%s  flags:0x%x  access:0x%x\n", prefix,
			ace_type, (unsigned int)ace->flags,
			(unsigned int)le32_to_cpu(ace->mask));
		/* get a SID string */
		sid = ntfs_sid_to_mbs(&ace->sid, NULL, 0);
		printf("%s\t\t SID: %s\n", prefix, sid);
		free(sid);

		/* proceed to next ACE */
		ace = (ACCESS_ALLOWED_ACE *)(((char *)ace) +
				le16_to_cpu(ace->size));
	}
}


static void ntfs_dump_security_descriptor(SECURITY_DESCRIPTOR_ATTR *sec_desc,
					  const char *indent)
{
	char *sid;

	printf("%s\tRevision:\t\t %u\n", indent, sec_desc->revision);

	/* TODO: parse the flags */
	printf("%s\tControl:\t\t 0x%04x\n", indent,
			le16_to_cpu(sec_desc->control));

	if (~sec_desc->control & SE_SELF_RELATIVE) {
		SECURITY_DESCRIPTOR *sd = (SECURITY_DESCRIPTOR *)sec_desc;

		printf("%s\tOwner SID pointer:\t %p\n", indent, sd->owner);
		printf("%s\tGroup SID pointer:\t %p\n", indent, sd->group);
		printf("%s\tSACL pointer:\t\t %p\n", indent, sd->sacl);
		printf("%s\tDACL pointer:\t\t %p\n", indent, sd->dacl);

		return;
	}

	if (sec_desc->owner) {
		sid = ntfs_sid_to_mbs((SID *)((char *)sec_desc +
			le32_to_cpu(sec_desc->owner)), NULL, 0);
		printf("%s\tOwner SID:\t\t %s\n", indent, sid);
		free(sid);
	} else
		printf("%s\tOwner SID:\t\t missing\n", indent);

	if (sec_desc->group) {
		sid = ntfs_sid_to_mbs((SID *)((char *)sec_desc +
			le32_to_cpu(sec_desc->group)), NULL, 0);
		printf("%s\tGroup SID:\t\t %s\n", indent, sid);
		free(sid);
	} else
		printf("%s\tGroup SID:\t\t missing\n", indent);

	printf("%s\tSystem ACL:\t\t ", indent);
	if (sec_desc->control & SE_SACL_PRESENT) {
		if (sec_desc->control & SE_SACL_DEFAULTED) {
			printf("defaulted");
		}
		printf("\n");
		ntfs_dump_acl(indent ? "\t\t\t" : "\t\t",
			      (ACL *)((char *)sec_desc +
				      le32_to_cpu(sec_desc->sacl)));
	} else {
		printf("missing\n");
	}

	printf("%s\tDiscretionary ACL:\t ", indent);
	if (sec_desc->control & SE_DACL_PRESENT) {
		if (sec_desc->control & SE_SACL_DEFAULTED) {
			printf("defaulted");
		}
		printf("\n");
		ntfs_dump_acl(indent ? "\t\t\t" : "\t\t",
			      (ACL *)((char *)sec_desc +
				      le32_to_cpu(sec_desc->dacl)));
	} else {
		printf("missing\n");
	}
}

/**
 * ntfs_dump_security_descriptor()
 *
 * dump the security information about the file
 */
static void ntfs_dump_attr_security_descriptor(ATTR_RECORD *attr, ntfs_volume *vol)
{
	SECURITY_DESCRIPTOR_ATTR *sec_desc_attr;

	if (attr->non_resident) {
		/* FIXME: We don't handle fragmented mapping pairs case. */
		runlist *rl = ntfs_mapping_pairs_decompress(vol, attr, NULL);
		if (rl) {
			s64 data_size, bytes_read;

			data_size = sle64_to_cpu(attr->data_size);
			sec_desc_attr = ntfs_malloc(data_size);
			if (!sec_desc_attr) {
				free(rl);
				return;
			}
			bytes_read = ntfs_rl_pread(vol, rl, 0,
						data_size, sec_desc_attr);
			if (bytes_read != data_size) {
				ntfs_log_error("ntfsinfo error: could not "
						"read security descriptor\n");
				free(rl);
				free(sec_desc_attr);
				return;
			}
			free(rl);
		} else {
			ntfs_log_error("ntfsinfo error: could not "
						"decompress runlist\n");
			return;
		}
	} else {
		sec_desc_attr = (SECURITY_DESCRIPTOR_ATTR *)((u8*)attr +
				le16_to_cpu(attr->value_offset));
	}

	ntfs_dump_security_descriptor(sec_desc_attr, "");

	if (attr->non_resident)
		free(sec_desc_attr);
}

/**
 * ntfs_dump_volume_name()
 *
 * dump the name of the volume the inode belongs to
 */
static void ntfs_dump_attr_volume_name(ATTR_RECORD *attr)
{
	ntfschar *ucs_vol_name = NULL;

	if (le32_to_cpu(attr->value_length) > 0) {
		char *mbs_vol_name = NULL;
		int mbs_vol_name_size;
		/* calculate volume name position */
		ucs_vol_name = (ntfschar*)((u8*)attr +
				le16_to_cpu(attr->value_offset));
		/* convert the name to current locale multibyte sequence */
		mbs_vol_name_size = ntfs_ucstombs(ucs_vol_name,
				le32_to_cpu(attr->value_length) /
				sizeof(ntfschar), &mbs_vol_name, 0);

		if (mbs_vol_name_size>0) {
			/* output the converted name. */
			printf("\tVolume Name:\t\t '%s'\n", mbs_vol_name);
			free(mbs_vol_name);
		} else
			ntfs_log_perror("ntfsinfo error: could not parse "
					"volume name");
	} else
		printf("\tVolume Name:\t\t unnamed\n");
}

/**
 * ntfs_dump_volume_information()
 *
 * dump the information for the volume the inode belongs to
 *
 */
static void ntfs_dump_attr_volume_information(ATTR_RECORD *attr)
{
	VOLUME_INFORMATION *vol_information = NULL;

	vol_information = (VOLUME_INFORMATION*)((char *)attr+
		le16_to_cpu(attr->value_offset));

	printf("\tVolume Version:\t\t %d.%d\n", vol_information->major_ver,
		vol_information->minor_ver);
	printf("\tVolume Flags:\t\t ");
	if (vol_information->flags & VOLUME_IS_DIRTY)
		printf("DIRTY ");
	if (vol_information->flags & VOLUME_RESIZE_LOG_FILE)
		printf("RESIZE_LOG ");
	if (vol_information->flags & VOLUME_UPGRADE_ON_MOUNT)
		printf("UPG_ON_MOUNT ");
	if (vol_information->flags & VOLUME_MOUNTED_ON_NT4)
		printf("MOUNTED_NT4 ");
	if (vol_information->flags & VOLUME_DELETE_USN_UNDERWAY)
		printf("DEL_USN ");
	if (vol_information->flags & VOLUME_REPAIR_OBJECT_ID)
		printf("REPAIR_OBJID ");
	if (vol_information->flags & VOLUME_CHKDSK_UNDERWAY)
		printf("CHKDSK_UNDERWAY ");
	if (vol_information->flags & VOLUME_MODIFIED_BY_CHKDSK)
		printf("MOD_BY_CHKDSK ");
	if (vol_information->flags & VOLUME_FLAGS_MASK) {
		printf("(0x%04x)\n",
				(unsigned)le16_to_cpu(vol_information->flags));
	} else
		printf("none set (0x0000)\n");
	if (vol_information->flags & (~VOLUME_FLAGS_MASK))
		printf("\t\t\t\t Unknown Flags: 0x%04x\n",
				le16_to_cpu(vol_information->flags &
					(~VOLUME_FLAGS_MASK)));
}

static ntfschar NTFS_DATA_SDS[5] = { const_cpu_to_le16('$'),
	const_cpu_to_le16('S'), const_cpu_to_le16('D'),
	const_cpu_to_le16('S'), const_cpu_to_le16('\0') };

static void ntfs_dump_sds_entry(SECURITY_DESCRIPTOR_HEADER *sds)
{
	SECURITY_DESCRIPTOR_RELATIVE *sd;

	ntfs_log_verbose("\n");
	ntfs_log_verbose("\t\tHash:\t\t\t 0x%08x\n",
			(unsigned)le32_to_cpu(sds->hash));
	ntfs_log_verbose("\t\tSecurity id:\t\t %u (0x%x)\n",
			(unsigned)le32_to_cpu(sds->security_id),
			(unsigned)le32_to_cpu(sds->security_id));
	ntfs_log_verbose("\t\tOffset:\t\t\t %llu (0x%llx)\n",
			(unsigned long long)le64_to_cpu(sds->offset),
			(unsigned long long)le64_to_cpu(sds->offset));
	ntfs_log_verbose("\t\tLength:\t\t\t %u (0x%x)\n",
			(unsigned)le32_to_cpu(sds->length),
			(unsigned)le32_to_cpu(sds->length));

	sd = (SECURITY_DESCRIPTOR_RELATIVE *)((char *)sds +
		sizeof(SECURITY_DESCRIPTOR_HEADER));

	ntfs_dump_security_descriptor(sd, "\t");
}

static void ntfs_dump_sds(ATTR_RECORD *attr, ntfs_inode *ni)
{
	SECURITY_DESCRIPTOR_HEADER *sds, *sd;
	ntfschar *name;
	int name_len;
	s64 data_size;
	u64 inode;

	inode = ni->mft_no;
	if (ni->nr_extents < 0)
		inode = ni->base_ni->mft_no;
	if (FILE_Secure != inode)
		return;

	name_len = attr->name_length;
	if (!name_len)
		return;

	name = (ntfschar *)((u8 *)attr + le16_to_cpu(attr->name_offset));
	if (!ntfs_names_are_equal(NTFS_DATA_SDS, sizeof(NTFS_DATA_SDS) / 2 - 1,
				  name, name_len, 0, NULL, 0))
		return;

	sd = sds = ntfs_attr_readall(ni, AT_DATA, name, name_len, &data_size);
	if (!sd) {
		ntfs_log_perror("Failed to read $SDS attribute");
		return;
	}
	/*
	 * FIXME: The right way is based on the indexes, so we couldn't
	 * miss real entries. For now, dump until it makes sense.
	 */
	while (sd->length && sd->hash &&
	       le64_to_cpu(sd->offset) < (u64)data_size &&
	       le32_to_cpu(sd->length) < (u64)data_size &&
	       le64_to_cpu(sd->offset) +
			le32_to_cpu(sd->length) < (u64)data_size) {
		ntfs_dump_sds_entry(sd);
		sd = (SECURITY_DESCRIPTOR_HEADER *)((char*)sd +
				((le32_to_cpu(sd->length) + 15) & ~15));
	}
	free(sds);
}

static const char *get_attribute_type_name(le32 type)
{
	switch (type) {
	case AT_UNUSED:			return "$UNUSED";
	case AT_STANDARD_INFORMATION:   return "$STANDARD_INFORMATION";
	case AT_ATTRIBUTE_LIST:         return "$ATTRIBUTE_LIST";
	case AT_FILE_NAME:              return "$FILE_NAME";
	case AT_OBJECT_ID:              return "$OBJECT_ID";
	case AT_SECURITY_DESCRIPTOR:    return "$SECURITY_DESCRIPTOR";
	case AT_VOLUME_NAME:            return "$VOLUME_NAME";
	case AT_VOLUME_INFORMATION:     return "$VOLUME_INFORMATION";
	case AT_DATA:                   return "$DATA";
	case AT_INDEX_ROOT:             return "$INDEX_ROOT";
	case AT_INDEX_ALLOCATION:       return "$INDEX_ALLOCATION";
	case AT_BITMAP:                 return "$BITMAP";
	case AT_REPARSE_POINT:          return "$REPARSE_POINT";
	case AT_EA_INFORMATION:         return "$EA_INFORMATION";
	case AT_EA:                     return "$EA";
	case AT_PROPERTY_SET:           return "$PROPERTY_SET";
	case AT_LOGGED_UTILITY_STREAM:  return "$LOGGED_UTILITY_STREAM";
	case AT_END:                    return "$END";
	}

	return "$UNKNOWN";
}

static const char * ntfs_dump_lcn(LCN lcn)
{
	switch (lcn) {
		case LCN_HOLE:
			return "<HOLE>\t";
		case LCN_RL_NOT_MAPPED:
			return "<RL_NOT_MAPPED>";
		case LCN_ENOENT:
			return "<ENOENT>\t";
		case LCN_EINVAL:
			return "<EINVAL>\t";
		case LCN_EIO:
			return "<EIO>\t";
		default:
			ntfs_log_error("Invalid LCN value %llx passed to "
					"ntfs_dump_lcn().\n", lcn);
			return "???\t";
	}
}

static void ntfs_dump_attribute_header(ntfs_attr_search_ctx *ctx,
		ntfs_volume *vol)
{
	ATTR_RECORD *a = ctx->attr;

	printf("Dumping attribute %s (0x%x) from mft record %lld (0x%llx)\n",
			get_attribute_type_name(a->type),
			(unsigned)le32_to_cpu(a->type),
			(unsigned long long)ctx->ntfs_ino->mft_no,
			(unsigned long long)ctx->ntfs_ino->mft_no);

	ntfs_log_verbose("\tAttribute length:\t %u (0x%x)\n",
			(unsigned)le32_to_cpu(a->length),
			(unsigned)le32_to_cpu(a->length));
	printf("\tResident: \t\t %s\n", a->non_resident ? "No" : "Yes");
	ntfs_log_verbose("\tName length:\t\t %u (0x%x)\n",
			(unsigned)a->name_length, (unsigned)a->name_length);
	ntfs_log_verbose("\tName offset:\t\t %u (0x%x)\n",
			(unsigned)le16_to_cpu(a->name_offset),
			(unsigned)le16_to_cpu(a->name_offset));

	/* Dump the attribute (stream) name */
	if (a->name_length) {
		char *attribute_name = NULL;

		attribute_name = ntfs_attr_get_name_mbs(a);
		if (attribute_name) {
			printf("\tAttribute name:\t\t '%s'\n", attribute_name);
			free(attribute_name);
		} else
			ntfs_log_perror("Error: couldn't parse attribute name");
	}

	/* TODO: parse the flags */
	printf("\tAttribute flags:\t 0x%04x\n",
			(unsigned)le16_to_cpu(a->flags));
	printf("\tAttribute instance:\t %u (0x%x)\n",
			(unsigned)le16_to_cpu(a->instance),
			(unsigned)le16_to_cpu(a->instance));

	/* Resident attribute */
	if (!a->non_resident) {
		printf("\tData size:\t\t %u (0x%x)\n",
				(unsigned)le32_to_cpu(a->value_length),
				(unsigned)le32_to_cpu(a->value_length));
		ntfs_log_verbose("\tData offset:\t\t %u (0x%x)\n",
				(unsigned)le16_to_cpu(a->value_offset),
				(unsigned)le16_to_cpu(a->value_offset));
		/* TODO: parse the flags */
		printf("\tResident flags:\t\t 0x%02x\n",
				(unsigned)a->resident_flags);
		ntfs_log_verbose("\tReservedR:\t\t %d (0x%x)\n",
				(unsigned)a->reservedR, (unsigned)a->reservedR);
		return;
	}

	/* Non-resident attribute */
	ntfs_log_verbose("\tLowest VCN\t\t %lld (0x%llx)\n",
			(long long)sle64_to_cpu(a->lowest_vcn),
			(unsigned long long)sle64_to_cpu(a->lowest_vcn));
	ntfs_log_verbose("\tHighest VCN:\t\t %lld (0x%llx)\n",
			(long long)sle64_to_cpu(a->highest_vcn),
			(unsigned long long)sle64_to_cpu(a->highest_vcn));
	ntfs_log_verbose("\tMapping pairs offset:\t %u (0x%x)\n",
			(unsigned)le16_to_cpu(a->mapping_pairs_offset),
			(unsigned)le16_to_cpu(a->mapping_pairs_offset));
	printf("\tCompression unit:\t %u (0x%x)\n",
			(unsigned)a->compression_unit,
			(unsigned)a->compression_unit);
	/* TODO: dump the 5 reserved bytes here in verbose mode */

	if (!a->lowest_vcn) {
		printf("\tData size:\t\t %llu (0x%llx)\n",
				(long long)sle64_to_cpu(a->data_size),
				(unsigned long long)sle64_to_cpu(a->data_size));
		printf("\tAllocated size:\t\t %llu (0x%llx)\n",
				(long long)sle64_to_cpu(a->allocated_size),
				(unsigned long long)
				sle64_to_cpu(a->allocated_size));
		printf("\tInitialized size:\t %llu (0x%llx)\n",
				(long long)sle64_to_cpu(a->initialized_size),
				(unsigned long long)
				sle64_to_cpu(a->initialized_size));
		if (a->compression_unit || a->flags & ATTR_IS_COMPRESSED ||
				a->flags & ATTR_IS_SPARSE)
			printf("\tCompressed size:\t %llu (0x%llx)\n",
					(signed long long)
					sle64_to_cpu(a->compressed_size),
					(signed long long)
					sle64_to_cpu(a->compressed_size));
	}

	if (opts.verbose) {
		runlist *rl;

		rl = ntfs_mapping_pairs_decompress(vol, a, NULL);
		if (rl) {
			runlist *rlc = rl;

			// TODO: Switch this to properly aligned hex...
			printf("\tRunlist:\tVCN\t\tLCN\t\tLength\n");
			while (rlc->length) {
				if (rlc->lcn >= 0)
					printf("\t\t\t0x%llx\t\t0x%llx\t\t"
							"0x%llx\n", rlc->vcn,
							rlc->lcn, rlc->length);
				else
					printf("\t\t\t0x%llx\t\t%s\t"
							"0x%llx\n", rlc->vcn,
							ntfs_dump_lcn(rlc->lcn),
							rlc->length);
				rlc++;
			}
			free(rl);
		} else
			ntfs_log_error("Error: couldn't decompress runlist\n");
	}
}

/**
 * ntfs_dump_data_attr()
 *
 * dump some info about the data attribute if it's metadata
 */
static void ntfs_dump_attr_data(ATTR_RECORD *attr, ntfs_inode *ni)
{
	if (opts.verbose)
		ntfs_dump_sds(attr, ni);
}

typedef enum {
	INDEX_ATTR_UNKNOWN,
	INDEX_ATTR_DIRECTORY_I30,
	INDEX_ATTR_SECURE_SII,
	INDEX_ATTR_SECURE_SDH,
	INDEX_ATTR_OBJID_O,
	INDEX_ATTR_REPARSE_R,
	INDEX_ATTR_QUOTA_O,
	INDEX_ATTR_QUOTA_Q,
} INDEX_ATTR_TYPE;

static void ntfs_dump_index_key(INDEX_ENTRY *entry, INDEX_ATTR_TYPE type)
{
	char *sid;
	char printable_GUID[37];

	switch (type) {
	case INDEX_ATTR_SECURE_SII:
		ntfs_log_verbose("\t\tKey security id:\t %u (0x%x)\n",
				(unsigned)
				le32_to_cpu(entry->key.sii.security_id),
				(unsigned)
				le32_to_cpu(entry->key.sii.security_id));
		break;
	case INDEX_ATTR_SECURE_SDH:
		ntfs_log_verbose("\t\tKey hash:\t\t 0x%08x\n",
				(unsigned)le32_to_cpu(entry->key.sdh.hash));
		ntfs_log_verbose("\t\tKey security id:\t %u (0x%x)\n",
				(unsigned)
				le32_to_cpu(entry->key.sdh.security_id),
				(unsigned)
				le32_to_cpu(entry->key.sdh.security_id));
		break;
	case INDEX_ATTR_OBJID_O:
		ntfs_guid_to_mbs(&entry->key.object_id, printable_GUID);
		ntfs_log_verbose("\t\tKey GUID:\t\t %s\n", printable_GUID);
		break;
	case INDEX_ATTR_REPARSE_R:
		ntfs_log_verbose("\t\tKey reparse tag:\t 0x%08x\n", (unsigned)
				le32_to_cpu(entry->key.reparse.reparse_tag));
		ntfs_log_verbose("\t\tKey file id:\t\t %llu (0x%llx)\n",
				(unsigned long long)
				le64_to_cpu(entry->key.reparse.file_id),
				(unsigned long long)
				le64_to_cpu(entry->key.reparse.file_id));
		break;
	case INDEX_ATTR_QUOTA_O:
		sid = ntfs_sid_to_mbs(&entry->key.sid, NULL, 0);
		ntfs_log_verbose("\t\tKey SID:\t\t %s\n", sid);
		free(sid);
		break;
	case INDEX_ATTR_QUOTA_Q:
		ntfs_log_verbose("\t\tKey owner id:\t\t %u (0x%x)\n",
				(unsigned)le32_to_cpu(entry->key.owner_id),
				(unsigned)le32_to_cpu(entry->key.owner_id));
		break;
	default:
		ntfs_log_verbose("\t\tIndex attr type is UNKNOWN: \t 0x%08x\n",
				(unsigned)type);
		break;
	}
}

typedef union {
	SII_INDEX_DATA sii;		/* $SII index data in $Secure */
	SDH_INDEX_DATA sdh;		/* $SDH index data in $Secure */
	QUOTA_O_INDEX_DATA quota_o;	/* $O index data in $Quota    */
	QUOTA_CONTROL_ENTRY quota_q;	/* $Q index data in $Quota    */
} __attribute__((__packed__)) INDEX_ENTRY_DATA;

static void ntfs_dump_index_data(INDEX_ENTRY *entry, INDEX_ATTR_TYPE type)
{
	INDEX_ENTRY_DATA *data;

	data = (INDEX_ENTRY_DATA *)((u8 *)entry +
			le16_to_cpu(entry->data_offset));

	switch (type) {
	case INDEX_ATTR_SECURE_SII:
		ntfs_log_verbose("\t\tHash:\t\t\t 0x%08x\n",
				(unsigned)le32_to_cpu(data->sii.hash));
		ntfs_log_verbose("\t\tSecurity id:\t\t %u (0x%x)\n",
				(unsigned)le32_to_cpu(data->sii.security_id),
				(unsigned)le32_to_cpu(data->sii.security_id));
		ntfs_log_verbose("\t\tOffset in $SDS:\t\t %llu (0x%llx)\n",
				(unsigned long long)
				le64_to_cpu(data->sii.offset),
				(unsigned long long)
				le64_to_cpu(data->sii.offset));
		ntfs_log_verbose("\t\tLength in $SDS:\t\t %u (0x%x)\n",
				(unsigned)le32_to_cpu(data->sii.length),
				(unsigned)le32_to_cpu(data->sii.length));
		break;
	case INDEX_ATTR_SECURE_SDH:
		ntfs_log_verbose("\t\tHash:\t\t\t 0x%08x\n",
				(unsigned)le32_to_cpu(data->sdh.hash));
		ntfs_log_verbose("\t\tSecurity id:\t\t %u (0x%x)\n",
				(unsigned)le32_to_cpu(data->sdh.security_id),
				(unsigned)le32_to_cpu(data->sdh.security_id));
		ntfs_log_verbose("\t\tOffset in $SDS:\t\t %llu (0x%llx)\n",
				(unsigned long long)
				le64_to_cpu(data->sdh.offset),
				(unsigned long long)
				le64_to_cpu(data->sdh.offset));
		ntfs_log_verbose("\t\tLength in $SDS:\t\t %u (0x%x)\n",
				(unsigned)le32_to_cpu(data->sdh.length),
				(unsigned)le32_to_cpu(data->sdh.length));
		ntfs_log_verbose("\t\tUnknown (padding):\t 0x%08x\n",
				(unsigned)le32_to_cpu(data->sdh.reserved_II));
		break;
	case INDEX_ATTR_OBJID_O: {
		OBJ_ID_INDEX_DATA *object_id_data;
		char printable_GUID[37];

		object_id_data = (OBJ_ID_INDEX_DATA*)((u8*)entry +
				le16_to_cpu(entry->data_offset));
		ntfs_log_verbose("\t\tMFT Number:\t\t 0x%llx\n",
				(unsigned long long)
				MREF_LE(object_id_data->mft_reference));
		ntfs_log_verbose("\t\tMFT Sequence Number:\t 0x%x\n",
				(unsigned)
				MSEQNO_LE(object_id_data->mft_reference));
		ntfs_guid_to_mbs(&object_id_data->birth_volume_id,
				printable_GUID);
		ntfs_log_verbose("\t\tBirth volume id GUID:\t %s\n",
				printable_GUID);
		ntfs_guid_to_mbs(&object_id_data->birth_object_id,
				printable_GUID);
		ntfs_log_verbose("\t\tBirth object id GUID:\t %s\n",
				printable_GUID);
		ntfs_guid_to_mbs(&object_id_data->domain_id, printable_GUID);
		ntfs_log_verbose("\t\tDomain id GUID:\t\t %s\n",
				printable_GUID);
		}
		break;
	case INDEX_ATTR_REPARSE_R:
		/* TODO */
		break;
	case INDEX_ATTR_QUOTA_O:
		ntfs_log_verbose("\t\tOwner id:\t\t %u (0x%x)\n",
				(unsigned)le32_to_cpu(data->quota_o.owner_id),
				(unsigned)le32_to_cpu(data->quota_o.owner_id));
		ntfs_log_verbose("\t\tUnknown:\t\t %u (0x%x)\n",
				(unsigned)le32_to_cpu(data->quota_o.unknown),
				(unsigned)le32_to_cpu(data->quota_o.unknown));
		break;
	case INDEX_ATTR_QUOTA_Q:
		ntfs_log_verbose("\t\tVersion:\t\t %u\n",
				(unsigned)le32_to_cpu(data->quota_q.version));
		ntfs_log_verbose("\t\tQuota flags:\t\t 0x%08x\n",
				(unsigned)le32_to_cpu(data->quota_q.flags));
		ntfs_log_verbose("\t\tBytes used:\t\t %llu (0x%llx)\n",
				(unsigned long long)
				le64_to_cpu(data->quota_q.bytes_used),
				(unsigned long long)
				le64_to_cpu(data->quota_q.bytes_used));
		ntfs_log_verbose("\t\tLast changed:\t\t %s",
				ntfsinfo_time_to_str(
				data->quota_q.change_time));
		ntfs_log_verbose("\t\tThreshold:\t\t %lld (0x%llx)\n",
				(unsigned long long)
				sle64_to_cpu(data->quota_q.threshold),
				(unsigned long long)
				sle64_to_cpu(data->quota_q.threshold));
		ntfs_log_verbose("\t\tLimit:\t\t\t %lld (0x%llx)\n",
				(unsigned long long)
				sle64_to_cpu(data->quota_q.limit),
				(unsigned long long)
				sle64_to_cpu(data->quota_q.limit));
		ntfs_log_verbose("\t\tExceeded time:\t\t %lld (0x%llx)\n",
				(unsigned long long)
				sle64_to_cpu(data->quota_q.exceeded_time),
				(unsigned long long)
				sle64_to_cpu(data->quota_q.exceeded_time));
		if (le16_to_cpu(entry->data_length) > 48) {
			char *sid;
			sid = ntfs_sid_to_mbs(&data->quota_q.sid, NULL, 0);
			ntfs_log_verbose("\t\tOwner SID:\t\t %s\n", sid);
			free(sid);
		}
		break;
	default:
		ntfs_log_verbose("\t\tIndex attr type is UNKNOWN: \t 0x%08x\n",
				(unsigned)type);
		break;
	}
}

/**
 * ntfs_dump_index_entries()
 *
 * dump sequence of index_entries and return number of entries dumped.
 */
static int ntfs_dump_index_entries(INDEX_ENTRY *entry, INDEX_ATTR_TYPE type)
{
	int numb_entries = 1;
	while (1) {
		if (!opts.verbose) {
			if (entry->flags & INDEX_ENTRY_END)
				break;
			entry = (INDEX_ENTRY *)((u8 *)entry +
						le16_to_cpu(entry->length));
			numb_entries++;
			continue;
		}
		ntfs_log_verbose("\t\tEntry length:\t\t %u (0x%x)\n",
				(unsigned)le16_to_cpu(entry->length),
				(unsigned)le16_to_cpu(entry->length));
		ntfs_log_verbose("\t\tKey length:\t\t %u (0x%x)\n",
				(unsigned)le16_to_cpu(entry->key_length),
				(unsigned)le16_to_cpu(entry->key_length));
		ntfs_log_verbose("\t\tIndex entry flags:\t 0x%02x\n",
				(unsigned)le16_to_cpu(entry->flags));

		if (entry->flags & INDEX_ENTRY_NODE)
			ntfs_log_verbose("\t\tSubnode VCN:\t\t %lld (0x%llx)\n",
					 ntfs_ie_get_vcn(entry),
					 ntfs_ie_get_vcn(entry));
		if (entry->flags & INDEX_ENTRY_END)
			break;

		switch (type) {
		case INDEX_ATTR_DIRECTORY_I30:
			ntfs_log_verbose("\t\tFILE record number:\t %llu "
					"(0x%llx)\n", (unsigned long long)
					MREF_LE(entry->indexed_file),
					(unsigned long long)
					MREF_LE(entry->indexed_file));
			ntfs_dump_filename("\t\t", &entry->key.file_name);
			break;
		default:
			ntfs_log_verbose("\t\tData offset:\t\t %u (0x%x)\n",
					(unsigned)
					le16_to_cpu(entry->data_offset),
					(unsigned)
					le16_to_cpu(entry->data_offset));
			ntfs_log_verbose("\t\tData length:\t\t %u (0x%x)\n",
					(unsigned)
					le16_to_cpu(entry->data_length),
					(unsigned)
					le16_to_cpu(entry->data_length));
			ntfs_dump_index_key(entry, type);
			ntfs_log_verbose("\t\tKey Data:\n");
			ntfs_dump_index_data(entry, type);
			break;
		}
		if (!entry->length) {
			ntfs_log_verbose("\tWARNING: Corrupt index entry, "
					"skipping the remainder of this index "
					"block.\n");
			break;
		}
		entry = (INDEX_ENTRY*)((u8*)entry + le16_to_cpu(entry->length));
		numb_entries++;
		ntfs_log_verbose("\n");
	}
	ntfs_log_verbose("\tEnd of index block reached\n");
	return numb_entries;
}

#define	COMPARE_INDEX_NAMES(attr, name)					       \
	ntfs_names_are_equal((name), sizeof(name) / 2 - 1,		       \
		(ntfschar*)((char*)(attr) + le16_to_cpu((attr)->name_offset)), \
		(attr)->name_length, 0, NULL, 0)

static INDEX_ATTR_TYPE get_index_attr_type(ntfs_inode *ni, ATTR_RECORD *attr,
					   INDEX_ROOT *index_root)
{
	char file_name[64];

	if (!attr->name_length)
		return INDEX_ATTR_UNKNOWN;

	if (index_root->type) {
		if (index_root->type == AT_FILE_NAME)
			return INDEX_ATTR_DIRECTORY_I30;
		else
			/* weird, this should be illegal */
			ntfs_log_error("Unknown index attribute type: 0x%0X\n",
				       index_root->type);
		return INDEX_ATTR_UNKNOWN;
	}

	if (utils_is_metadata(ni) <= 0)
		return INDEX_ATTR_UNKNOWN;
	if (utils_inode_get_name(ni, file_name, sizeof(file_name)) <= 0)
		return INDEX_ATTR_UNKNOWN;

	if (COMPARE_INDEX_NAMES(attr, NTFS_INDEX_SDH))
		return INDEX_ATTR_SECURE_SDH;
	else if (COMPARE_INDEX_NAMES(attr, NTFS_INDEX_SII))
		return INDEX_ATTR_SECURE_SII;
	else if (COMPARE_INDEX_NAMES(attr, NTFS_INDEX_SII))
		return INDEX_ATTR_SECURE_SII;
	else if (COMPARE_INDEX_NAMES(attr, NTFS_INDEX_Q))
		return INDEX_ATTR_QUOTA_Q;
	else if (COMPARE_INDEX_NAMES(attr, NTFS_INDEX_R))
		return INDEX_ATTR_REPARSE_R;
	else if (COMPARE_INDEX_NAMES(attr, NTFS_INDEX_O)) {
		if (!strcmp(file_name, "/$Extend/$Quota"))
			return INDEX_ATTR_QUOTA_O;
		else if (!strcmp(file_name, "/$Extend/$ObjId"))
			return INDEX_ATTR_OBJID_O;
	}

	return INDEX_ATTR_UNKNOWN;
}

static void ntfs_dump_index_attr_type(INDEX_ATTR_TYPE type)
{
	if (type == INDEX_ATTR_DIRECTORY_I30)
		printf("DIRECTORY_I30");
	else if (type == INDEX_ATTR_SECURE_SDH)
		printf("SECURE_SDH");
	else if (type == INDEX_ATTR_SECURE_SII)
		printf("SECURE_SII");
	else if (type == INDEX_ATTR_OBJID_O)
		printf("OBJID_O");
	else if (type == INDEX_ATTR_QUOTA_O)
		printf("QUOTA_O");
	else if (type == INDEX_ATTR_QUOTA_Q)
		printf("QUOTA_Q");
	else if (type == INDEX_ATTR_REPARSE_R)
		printf("REPARSE_R");
	else
		printf("UNKNOWN");
	printf("\n");
}

static void ntfs_dump_index_header(const char *indent, INDEX_HEADER *idx)
{
	printf("%sEntries Offset:\t\t %u (0x%x)\n", indent,
			(unsigned)le32_to_cpu(idx->entries_offset),
			(unsigned)le32_to_cpu(idx->entries_offset));
	printf("%sIndex Size:\t\t %u (0x%x)\n", indent,
			(unsigned)le32_to_cpu(idx->index_length),
			(unsigned)le32_to_cpu(idx->index_length));
	printf("%sAllocated Size:\t\t %u (0x%x)\n", indent,
			(unsigned)le32_to_cpu(idx->allocated_size),
			(unsigned)le32_to_cpu(idx->allocated_size));
	printf("%sIndex header flags:\t 0x%02x\n", indent, idx->flags);

	/* FIXME: there are 3 reserved bytes here */
}

/**
 * ntfs_dump_attr_index_root()
 *
 * dump the index_root attribute
 */
static void ntfs_dump_attr_index_root(ATTR_RECORD *attr, ntfs_inode *ni)
{
	INDEX_ATTR_TYPE type;
	INDEX_ROOT *index_root = NULL;
	INDEX_ENTRY *entry;

	index_root = (INDEX_ROOT*)((u8*)attr + le16_to_cpu(attr->value_offset));

	/* attr_type dumping */
	type = get_index_attr_type(ni, attr, index_root);
	printf("\tIndexed Attr Type:\t ");
	ntfs_dump_index_attr_type(type);

	/* collation rule dumping */
	printf("\tCollation Rule:\t\t %u (0x%x)\n",
			(unsigned)le32_to_cpu(index_root->collation_rule),
			(unsigned)le32_to_cpu(index_root->collation_rule));
/*	COLLATION_BINARY, COLLATION_FILE_NAME, COLLATION_UNICODE_STRING,
	COLLATION_NTOFS_ULONG, COLLATION_NTOFS_SID,
	COLLATION_NTOFS_SECURITY_HASH, COLLATION_NTOFS_ULONGS */

	printf("\tIndex Block Size:\t %u (0x%x)\n",
			(unsigned)le32_to_cpu(index_root->index_block_size),
			(unsigned)le32_to_cpu(index_root->index_block_size));
	printf("\tClusters Per Block:\t %u (0x%x)\n",
			(unsigned)index_root->clusters_per_index_block,
			(unsigned)index_root->clusters_per_index_block);

	ntfs_dump_index_header("\t", &index_root->index);

	entry = (INDEX_ENTRY*)((u8*)index_root +
			le32_to_cpu(index_root->index.entries_offset) + 0x10);
	ntfs_log_verbose("\tDumping index root:\n");
	printf("\tIndex entries total:\t %d\n",
			ntfs_dump_index_entries(entry, type));
}

static void ntfs_dump_usa_lsn(const char *indent, MFT_RECORD *mrec)
{
	printf("%sUpd. Seq. Array Off.:\t %u (0x%x)\n", indent,
			(unsigned)le16_to_cpu(mrec->usa_ofs),
			(unsigned)le16_to_cpu(mrec->usa_ofs));
	printf("%sUpd. Seq. Array Count:\t %u (0x%x)\n", indent,
			(unsigned)le16_to_cpu(mrec->usa_count),
			(unsigned)le16_to_cpu(mrec->usa_count));
	printf("%sUpd. Seq. Number:\t %u (0x%x)\n", indent,
			(unsigned)le16_to_cpup((u16 *)((u8 *)mrec +
			le16_to_cpu(mrec->usa_ofs))),
			(unsigned)le16_to_cpup((u16 *)((u8 *)mrec +
			le16_to_cpu(mrec->usa_ofs))));
	printf("%sLogFile Seq. Number:\t 0x%llx\n", indent,
			(unsigned long long)sle64_to_cpu(mrec->lsn));
}


static s32 ntfs_dump_index_block(INDEX_BLOCK *ib, INDEX_ATTR_TYPE type,
		u32 ib_size)
{
	INDEX_ENTRY *entry;

	if (ntfs_mst_post_read_fixup((NTFS_RECORD*)ib, ib_size)) {
		ntfs_log_perror("Damaged INDX record");
		return -1;
	}
	ntfs_log_verbose("\tDumping index block:\n");
	if (opts.verbose)
		ntfs_dump_usa_lsn("\t\t", (MFT_RECORD*)ib);

	ntfs_log_verbose("\t\tNode VCN:\t\t %lld (0x%llx)\n",
			(unsigned long long)sle64_to_cpu(ib->index_block_vcn),
			(unsigned long long)sle64_to_cpu(ib->index_block_vcn));

	entry = (INDEX_ENTRY*)((u8*)ib +
				le32_to_cpu(ib->index.entries_offset) + 0x18);

	if (opts.verbose) {
		ntfs_dump_index_header("\t\t", &ib->index);
		printf("\n");
	}

	return ntfs_dump_index_entries(entry, type);
}

/**
 * ntfs_dump_attr_index_allocation()
 *
 * dump context of the index_allocation attribute
 */
static void ntfs_dump_attr_index_allocation(ATTR_RECORD *attr, ntfs_inode *ni)
{
	INDEX_ALLOCATION *allocation, *tmp_alloc;
	INDEX_ROOT *ir;
	INDEX_ATTR_TYPE type;
	int total_entries = 0;
	int total_indx_blocks = 0;
	u8 *bitmap, *byte;
	int bit;
	ntfschar *name;
	u32 name_len;
	s64 data_size;

	ir = ntfs_index_root_get(ni, attr);
	if (!ir) {
		ntfs_log_perror("Failed to read $INDEX_ROOT attribute");
		return;
	}

	type = get_index_attr_type(ni, attr, ir);

	name = (ntfschar *)((u8 *)attr + le16_to_cpu(attr->name_offset));
	name_len = attr->name_length;

	byte = bitmap = ntfs_attr_readall(ni, AT_BITMAP, name, name_len, NULL);
	if (!byte) {
		ntfs_log_perror("Failed to read $BITMAP attribute");
		goto out_index_root;
	}

	tmp_alloc = allocation = ntfs_attr_readall(ni, AT_INDEX_ALLOCATION,
						   name, name_len, &data_size);
	if (!tmp_alloc) {
		ntfs_log_perror("Failed to read $INDEX_ALLOCATION attribute");
		goto out_bitmap;
	}

	bit = 0;
	while ((u8 *)tmp_alloc < (u8 *)allocation + data_size) {
		if (*byte & (1 << bit)) {
			int entries;

			entries = ntfs_dump_index_block(tmp_alloc, type,
							le32_to_cpu(
							ir->index_block_size));
	       		if (entries != -1) {
				total_entries += entries;
				total_indx_blocks++;
				ntfs_log_verbose("\tIndex entries:\t\t %d\n",
						entries);
			}
		}
		tmp_alloc = (INDEX_ALLOCATION *)((u8 *)tmp_alloc +
						le32_to_cpu(
						ir->index_block_size));
		bit++;
		if (bit > 7) {
			bit = 0;
			byte++;
		}
	}

	printf("\tIndex entries total:\t %d\n", total_entries);
	printf("\tINDX blocks total:\t %d\n", total_indx_blocks);

	free(allocation);
out_bitmap:
	free(bitmap);
out_index_root:
	free(ir);
}

/**
 * ntfs_dump_attr_bitmap()
 *
 * dump the bitmap attribute
 */
static void ntfs_dump_attr_bitmap(ATTR_RECORD *attr __attribute__((unused)))
{
	/* TODO */
}

/**
 * ntfs_dump_attr_reparse_point()
 *
 * of ntfs 3.x dumps the reparse_point attribute
 */
static void ntfs_dump_attr_reparse_point(ATTR_RECORD *attr __attribute__((unused)))
{
	/* TODO */
}

/**
 * ntfs_dump_attr_ea_information()
 *
 * dump the ea_information attribute
 */
static void ntfs_dump_attr_ea_information(ATTR_RECORD *attr)
{
	EA_INFORMATION *ea_info;

	ea_info = (EA_INFORMATION*)((u8*)attr +
			le16_to_cpu(attr->value_offset));
	printf("\tPacked EA length:\t %u (0x%x)\n",
			(unsigned)le16_to_cpu(ea_info->ea_length),
			(unsigned)le16_to_cpu(ea_info->ea_length));
	printf("\tNEED_EA count:\t\t %u (0x%x)\n",
			(unsigned)le16_to_cpu(ea_info->need_ea_count),
			(unsigned)le16_to_cpu(ea_info->need_ea_count));
	printf("\tUnpacked EA length:\t %u (0x%x)\n",
			(unsigned)le32_to_cpu(ea_info->ea_query_length),
			(unsigned)le32_to_cpu(ea_info->ea_query_length));
}

/**
 * ntfs_dump_attr_ea()
 *
 * dump the ea attribute
 */
static void ntfs_dump_attr_ea(ATTR_RECORD *attr, ntfs_volume *vol)
{
	EA_ATTR *ea;
	u8 *buf = NULL;
	s64 data_size;

	if (attr->non_resident) {
		runlist *rl;

		data_size = sle64_to_cpu(attr->data_size);
		if (!opts.verbose)
			return;
		/* FIXME: We don't handle fragmented mapping pairs case. */
		rl = ntfs_mapping_pairs_decompress(vol, attr, NULL);
		if (rl) {
			s64 bytes_read;

			buf = ntfs_malloc(data_size);
			if (!buf) {
				free(rl);
				return;
			}
			bytes_read = ntfs_rl_pread(vol, rl, 0, data_size, buf);
			if (bytes_read != data_size) {
				ntfs_log_perror("ntfs_rl_pread failed");
				free(buf);
				free(rl);
				return;
			}
			free(rl);
			ea = (EA_ATTR*)buf;
		} else {
			ntfs_log_perror("ntfs_mapping_pairs_decompress failed");
			return;
		}
	} else {
		data_size = le32_to_cpu(attr->value_length);
		if (!opts.verbose)
			return;
		ea = (EA_ATTR*)((u8*)attr + le16_to_cpu(attr->value_offset));
	}
	while (1) {
		printf("\n\tEA flags:\t\t ");
		if (ea->flags) {
			if (ea->flags == NEED_EA)
				printf("NEED_EA\n");
			else
				printf("Unknown (0x%02x)\n",
						(unsigned)ea->flags);
		} else
			printf("NONE\n");
		printf("\tName length:\t %d (0x%x)\n",
				(unsigned)ea->name_length,
				(unsigned)ea->name_length);
		printf("\tValue length:\t %d (0x%x)\n",
				(unsigned)le16_to_cpu(ea->value_length),
				(unsigned)le16_to_cpu(ea->value_length));
		printf("\tName:\t\t '%s'\n", ea->name);
		printf("\tValue:\t\t ");
		if (ea->name_length == 11 &&
				!strncmp((const char*)"SETFILEBITS",
				(const char*)ea->name, 11))
			printf("0%o\n", (unsigned)le32_to_cpu(*(le32*)
					(ea->value + ea->name_length + 1)));
		else
			printf("'%s'\n", ea->value + ea->name_length + 1);
		if (ea->next_entry_offset)
			ea = (EA_ATTR*)((u8*)ea +
					le32_to_cpu(ea->next_entry_offset));
		else
			break;
		if ((u8*)ea - buf >= data_size)
			break;
	}
	free(buf);
}

/**
 * ntfs_dump_attr_property_set()
 *
 * dump the property_set attribute
 */
static void ntfs_dump_attr_property_set(ATTR_RECORD *attr __attribute__((unused)))
{
	/* TODO */
}

static void ntfs_hex_dump(void *buf,unsigned int length);

/**
 * ntfs_dump_attr_logged_utility_stream()
 *
 * dump the property_set attribute
 */
static void ntfs_dump_attr_logged_utility_stream(ATTR_RECORD *attr,
		ntfs_inode *ni)
{
	char *buf;
	s64 size;

	if (!opts.verbose)
		return;
	buf = ntfs_attr_readall(ni, AT_LOGGED_UTILITY_STREAM,
			ntfs_attr_get_name(attr), attr->name_length, &size);
	if (buf)
		ntfs_hex_dump(buf, size);
	free(buf);
	/* TODO */
}

/**
 * ntfs_hex_dump
 */
static void ntfs_hex_dump(void *buf,unsigned int length)
{
	unsigned int i=0;
	while (i<length) {
		unsigned int j;

		/* line start */
		printf("\t%04X  ",i);

		/* hex content */
		for (j=i;(j<length) && (j<i+16);j++) {
			unsigned char c = *((char *)buf + j);
			printf("%02hhX ",c);
		}

		/* realign */
		for (;j<i+16;j++) {
			printf("   ");
		}

		/* char content */
		for (j=i;(j<length) && (j<i+16);j++) {
			unsigned char c = *((char *)buf + j);
			/* display unprintable chars as '.' */
			if ((c<32) || (c>126)) {
				c = '.';
			}
			printf("%c",c);
		}

		/* end line */
		printf("\n");
		i=j;
	}
}

/**
 * ntfs_dump_attr_unknown
 */
static void ntfs_dump_attr_unknown(ATTR_RECORD *attr)
{
	printf("=====  Please report this unknown attribute type to %s =====\n",
	       NTFS_DEV_LIST);

	if (!attr->non_resident) {
		/* hex dump */
		printf("\tDumping some of the attribute data:\n");
		ntfs_hex_dump((u8*)attr + le16_to_cpu(attr->value_offset),
				(le32_to_cpu(attr->value_length) > 128) ?
				128 : le32_to_cpu(attr->value_length));
	}
}

/**
 * ntfs_dump_inode_general_info
 */
static void ntfs_dump_inode_general_info(ntfs_inode *inode)
{
	MFT_RECORD *mrec = inode->mrec;
	le16 inode_flags  = mrec->flags;

	printf("Dumping Inode %llu (0x%llx)\n",
			(long long)inode->mft_no,
			(unsigned long long)inode->mft_no);

	ntfs_dump_usa_lsn("", mrec);
	printf("MFT Record Seq. Numb.:\t %u (0x%x)\n",
			(unsigned)le16_to_cpu(mrec->sequence_number),
			(unsigned)le16_to_cpu(mrec->sequence_number));
	printf("Number of Hard Links:\t %u (0x%x)\n",
			(unsigned)le16_to_cpu(mrec->link_count),
			(unsigned)le16_to_cpu(mrec->link_count));
	printf("Attribute Offset:\t %u (0x%x)\n",
			(unsigned)le16_to_cpu(mrec->attrs_offset),
			(unsigned)le16_to_cpu(mrec->attrs_offset));

	printf("MFT Record Flags:\t ");
	if (inode_flags) {
		if (MFT_RECORD_IN_USE & inode_flags) {
			printf("IN_USE ");
			inode_flags &= ~MFT_RECORD_IN_USE;
		}
		if (MFT_RECORD_IS_DIRECTORY & inode_flags) {
			printf("DIRECTORY ");
			inode_flags &= ~MFT_RECORD_IS_DIRECTORY;
		}
		/* The meaning of IS_4 is illusive but not its existence. */
		if (MFT_RECORD_IS_4 & inode_flags) {
			printf("IS_4 ");
			inode_flags &= ~MFT_RECORD_IS_4;
		}
		if (MFT_RECORD_IS_VIEW_INDEX & inode_flags) {
			printf("VIEW_INDEX ");
			inode_flags &= ~MFT_RECORD_IS_VIEW_INDEX;
		}
		if (inode_flags)
			printf("UNKNOWN: 0x%04x", (unsigned)le16_to_cpu(
						inode_flags));
	} else {
		printf("none");
	}
	printf("\n");

	printf("Bytes Used:\t\t %u (0x%x) bytes\n",
			(unsigned)le32_to_cpu(mrec->bytes_in_use),
			(unsigned)le32_to_cpu(mrec->bytes_in_use));
	printf("Bytes Allocated:\t %u (0x%x) bytes\n",
			(unsigned)le32_to_cpu(mrec->bytes_allocated),
			(unsigned)le32_to_cpu(mrec->bytes_allocated));

	if (mrec->base_mft_record) {
		printf("Base MFT Record:\t %llu (0x%llx)\n",
				(unsigned long long)
				MREF_LE(mrec->base_mft_record),
				(unsigned long long)
				MREF_LE(mrec->base_mft_record));
	}
	printf("Next Attribute Instance: %u (0x%x)\n",
			(unsigned)le16_to_cpu(mrec->next_attr_instance),
			(unsigned)le16_to_cpu(mrec->next_attr_instance));

	printf("MFT Padding:\t");
	ntfs_dump_bytes((u8 *)mrec, le16_to_cpu(mrec->usa_ofs) +
			2 * le16_to_cpu(mrec->usa_count),
			le16_to_cpu(mrec->attrs_offset));
	printf("\n");
}

/**
 * ntfs_get_file_attributes
 */
static void ntfs_dump_file_attributes(ntfs_inode *inode)
{
	ntfs_attr_search_ctx *ctx = NULL;

	/* then start enumerating attributes
	   see ntfs_attr_lookup documentation for detailed explanation */
	ctx = ntfs_attr_get_search_ctx(inode, NULL);
	while (!ntfs_attr_lookup(AT_UNUSED, NULL, 0, 0, 0, NULL, 0, ctx)) {
		if (ctx->attr->type == AT_END || ctx->attr->type == AT_UNUSED) {
			printf("Weird: %s attribute type was found, please "
					"report this.\n",
					get_attribute_type_name(
					ctx->attr->type));
			continue;
		}

		ntfs_dump_attribute_header(ctx, inode->vol);

		switch (ctx->attr->type) {
		case AT_STANDARD_INFORMATION:
			ntfs_dump_attr_standard_information(ctx->attr);
			break;
		case AT_ATTRIBUTE_LIST:
			ntfs_dump_attr_list(ctx->attr, inode->vol);
			break;
		case AT_FILE_NAME:
			ntfs_dump_attr_file_name(ctx->attr);
			break;
		case AT_OBJECT_ID:
			ntfs_dump_attr_object_id(ctx->attr, inode->vol);
			break;
		case AT_SECURITY_DESCRIPTOR:
			ntfs_dump_attr_security_descriptor(ctx->attr,
					inode->vol);
			break;
		case AT_VOLUME_NAME:
			ntfs_dump_attr_volume_name(ctx->attr);
			break;
		case AT_VOLUME_INFORMATION:
			ntfs_dump_attr_volume_information(ctx->attr);
			break;
		case AT_DATA:
			ntfs_dump_attr_data(ctx->attr, inode);
			break;
		case AT_INDEX_ROOT:
			ntfs_dump_attr_index_root(ctx->attr, inode);
			break;
		case AT_INDEX_ALLOCATION:
			ntfs_dump_attr_index_allocation(ctx->attr, inode);
			break;
		case AT_BITMAP:
			ntfs_dump_attr_bitmap(ctx->attr);
			break;
		case AT_REPARSE_POINT:
			ntfs_dump_attr_reparse_point(ctx->attr);
			break;
		case AT_EA_INFORMATION:
			ntfs_dump_attr_ea_information(ctx->attr);
			break;
		case AT_EA:
			ntfs_dump_attr_ea(ctx->attr, inode->vol);
			break;
		case AT_PROPERTY_SET:
			ntfs_dump_attr_property_set(ctx->attr);
			break;
		case AT_LOGGED_UTILITY_STREAM:
			ntfs_dump_attr_logged_utility_stream(ctx->attr, inode);
			break;
		default:
			ntfs_dump_attr_unknown(ctx->attr);
		}
	}

	/* if we exited the loop before we're done - notify the user */
	if (errno != ENOENT) {
		ntfs_log_perror("ntfsinfo error: stopped before finished "
				"enumerating attributes");
	} else {
		printf("End of inode reached\n");
	}

	/* close all data-structures we used */
	ntfs_attr_put_search_ctx(ctx);
	ntfs_inode_close(inode);
}

/**
 * main() - Begin here
 *
 * Start from here.
 *
 * Return:  0  Success, the program worked
 *	    1  Error, something went wrong
 */
int main(int argc, char **argv)
{
	ntfs_volume *vol;

	setlinebuf(stdout);

	ntfs_log_set_handler(ntfs_log_handler_outerr);

	if (!parse_options(argc, argv)) {
		printf("Failed to parse command line options\n");
		exit(1);
	}

	utils_set_locale();

	vol = utils_mount_volume(opts.device, NTFS_MNT_RDONLY |
			(opts.force ? NTFS_MNT_FORCE : 0));
	if (!vol) {
		printf("Failed to open '%s'.\n", opts.device);
		exit(1);
	}

	/*
	 * if opts.mft is not 0, then we will print out information about
	 * the volume, such as the sector size and whatnot.
	 */
	if (opts.mft)
		ntfs_dump_volume(vol);

	if ((opts.inode != -1) || opts.filename) {
		ntfs_inode *inode;
		/* obtain the inode */
		if (opts.filename) {
			inode = ntfs_pathname_to_inode(vol, NULL,
					opts.filename);
		} else {
			inode = ntfs_inode_open(vol, MK_MREF(opts.inode, 0));
		}

		/* dump the inode information */
		if (inode) {
			/* general info about the inode's mft record */
			ntfs_dump_inode_general_info(inode);
			/* dump attributes */
			ntfs_dump_file_attributes(inode);
		} else {
			/* can't open inode */
			/*
			 * note: when the specified inode does not exist, either
			 * EIO or or ESPIPE is returned, we should notify better
			 * in those cases
			 */
			ntfs_log_perror("Error loading node");
		}
	}

	ntfs_umount(vol, FALSE);
	return 0;
}


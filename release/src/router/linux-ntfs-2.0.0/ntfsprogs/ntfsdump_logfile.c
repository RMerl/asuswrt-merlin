/**
 * ntfsdump_logfile - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2000-2005 Anton Altaparmakov
 *
 * This utility will interpret the contents of the journal ($LogFile) of an
 * NTFS partition and display the results on stdout.  Errors will be output to
 * stderr.
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
 * along with this program (in the main directory of the Linux-NTFS source
 * in the file COPYING); if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/* TODO:
 *	- Remove the need for clipping at 64MiB.
 *	- Add normal command line switchs (use getopt_long()).
 *	- For a volume: allow dumping only uncommitted records.
 *	- For a file:   get an optional command line parameter for the last SN.
 *	- Sanity checks.
 */

#include "config.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "types.h"
#include "endians.h"
#include "volume.h"
#include "inode.h"
#include "attrib.h"
#include "layout.h"
#include "logfile.h"
#include "mst.h"
#include "utils.h"
#include "version.h"
#include "logging.h"

typedef struct {
	BOOL is_volume;
	const char *filename;
	s64 data_size;
	union {
		struct {
			ntfs_volume *vol;
			ntfs_inode *ni;
			ntfs_attr *na;
		};
		struct {
			int fd;
		};
	};
} logfile_file;

/**
 * logfile_close
 */
static int logfile_close(logfile_file *logfile)
{
	if (logfile->is_volume) {
		if (logfile->na)
			ntfs_attr_close(logfile->na);
		if (logfile->ni && ntfs_inode_close(logfile->ni))
			ntfs_log_perror("Warning: Failed to close $LogFile "
					"(inode %i)", FILE_LogFile);
		if (ntfs_umount(logfile->vol, 0))
			ntfs_log_perror("Warning: Failed to umount %s",
				logfile->filename);
	} else {
		if (close(logfile->fd))
			ntfs_log_perror("Warning: Failed to close file %s",
				logfile->filename);
	}
	return 0;
}

/**
 * device_err_exit - put an error message, cleanup and exit.
 * @vol: volume to unmount.
 * @ni:  Inode to free.
 * @na:  Attribute to close.
 *
 * Use when you wish to exit and collate all the cleanups together.
 * if you don't have some parameter to pass, just pass NULL.
 */
__attribute__((noreturn))
__attribute__((format(printf, 4, 5)))
static void device_err_exit(ntfs_volume *vol, ntfs_inode *ni,
		ntfs_attr *na, const char *fmt, ...)
{
	va_list ap;

	if (na)
		ntfs_attr_close(na);
	if (ni && ntfs_inode_close(ni))
		ntfs_log_perror("Warning: Failed to close $LogFile (inode %i)",
			FILE_LogFile);
	if (ntfs_umount(vol, 0))
		ntfs_log_perror("Warning: Failed to umount");

	fprintf(stderr, "ERROR: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	ntfs_log_error("Aborting...\n");
	exit(1);
}

/**
 * log_err_exit -
 */
__attribute__((noreturn))
__attribute__((format(printf, 2, 3)))
static void log_err_exit(u8 *buf, const char *fmt, ...)
{
	va_list ap;

	free(buf);

	fprintf(stderr, "ERROR: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	ntfs_log_error("Aborting...\n");
	exit(1);
}

/**
 * usage -
 */
__attribute__((noreturn))
static void usage(const char *exec_name)
{
	ntfs_log_error("%s v%s (libntfs %s) - Interpret and display information "
			"about the journal\n($LogFile) of an NTFS volume.\n"
			"Copyright (c) 2000-2005 Anton Altaparmakov.\n"
			"%s is free software, released under the GNU General "
			"Public License\nand you are welcome to redistribute "
			"it under certain conditions.\n%s comes with "
			"ABSOLUTELY NO WARRANTY; for details read the GNU\n"
			"General Public License to be found in the file "
			"COPYING in the main Linux-NTFS\ndistribution "
			"directory.\nUsage: %s device\n    e.g. %s /dev/hda6\n"
			"Alternative usage: %s -f file\n    e.g. %s -f "
			"MyCopyOfTheLogFile\n", exec_name, VERSION,
			ntfs_libntfs_version(), exec_name, exec_name,
			exec_name, exec_name, exec_name, exec_name);
	exit(1);
}

/**
 * logfile_open
 */
static int logfile_open(BOOL is_volume, const char *filename,
		logfile_file *logfile)
{
	if (is_volume) {
		ntfs_volume *vol;
		ntfs_inode *ni;
		ntfs_attr *na;

		vol = ntfs_mount(filename, NTFS_MNT_RDONLY | NTFS_MNT_FORENSIC);
		if (!vol)
			log_err_exit(NULL, "Failed to mount %s: %s\n",
					filename, strerror(errno));
		ntfs_log_info("Mounted NTFS volume %s (NTFS v%i.%i) on device %s.\n",
				vol->vol_name ? vol->vol_name : "<NO_NAME>",
				vol->major_ver, vol->minor_ver, filename);
		if (ntfs_version_is_supported(vol))
			device_err_exit(vol, NULL, NULL,
					"Unsupported NTFS version.\n");
		ni = ntfs_inode_open(vol, FILE_LogFile);
		if (!ni)
			device_err_exit(vol, NULL, NULL, "Failed to "
					"open $LogFile (inode %i): %s\n",
					FILE_LogFile, strerror(errno));
		na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
		if (!na)
			device_err_exit(vol, ni, NULL, "Failed to open "
					"$LogFile/$DATA (attribute 0x%x):"
					" %s\n", (unsigned int)
					le32_to_cpu(AT_DATA), strerror(errno));
		if (!na->data_size)
			device_err_exit(vol, ni, na, "$LogFile has zero "
					"length.  Run chkdsk /f to correct "
					"this.\n");
		logfile->data_size = na->data_size;
		logfile->vol = vol;
		logfile->ni = ni;
		logfile->na = na;
	} else {
		struct stat sbuf;
		int fd;

		if (stat(filename, &sbuf) == -1) {
			if (errno == ENOENT)
				log_err_exit(NULL, "The file %s does not "
						"exist.  Did you specify it "
						"correctly?\n", filename);
			log_err_exit(NULL, "Error getting information about "
					"%s: %s\n", filename, strerror(errno));
		}

		fd = open(filename, O_RDONLY);
		if (fd == -1)
			log_err_exit(NULL, "Failed to open file %s: %s\n",
					filename, strerror(errno));
		logfile->data_size = sbuf.st_size;
		logfile->fd = fd;
	}

	logfile->is_volume = is_volume;
	logfile->filename = filename;

	return 0;
}

/**
 * logfile_read
 */
static int logfile_pread(logfile_file *logfile, int ofs, int count, u8 *buf)
{
	int br;

	if (logfile->is_volume) {
		br = (int)ntfs_attr_pread(logfile->na, ofs, count, buf);
	} else {
		if (lseek(logfile->fd, ofs, SEEK_SET)==-1) {
			ntfs_log_error("Could not seek to offset %u\n", ofs);
			return 0;
		}
		br = read(logfile->fd, buf, count);
	}
	if (br != count) {
		ntfs_log_error("Only %d out of %d bytes read starting at %d\n",
			br, count, ofs);
	}
	return br;
}

/**
 * restart_header_sanity()
 */
static void restart_header_sanity(RESTART_PAGE_HEADER *rstr, u8 *buf)
{
	unsigned int usa_end_ofs, page_size;

	/* Only CHKD records are allowed to have chkdsk_lsn set. */
	if (!ntfs_is_chkd_record(rstr->magic) &&
			sle64_to_cpu(rstr->chkdsk_lsn))
		log_err_exit(buf, "$LogFile is corrupt:  Restart page header "
				"magic is not CHKD but a chkdsk LSN is "
				"specified.  Cannot handle this yet.\n");
	/* Both system and log page size must be >= 512 and a power of 2. */
	page_size = le32_to_cpu(rstr->log_page_size);
	if (page_size < 512 || page_size & (page_size - 1))
		log_err_exit(buf, "$LogFile is corrupt:  Restart page header "
				"specifies invalid log page size.  Cannot "
				"handle this yet.\n");
	if (page_size != le32_to_cpu(rstr->system_page_size)) {
		page_size = le32_to_cpu(rstr->system_page_size);
		if (page_size < 512 || page_size & (page_size - 1))
			log_err_exit(buf, "$LogFile is corrupt:  Restart page "
					"header specifies invalid system page "
					"size.  Cannot handle this yet.\n");
	}
	/* Abort if the version number is not 1.1. */
	if (sle16_to_cpu(rstr->major_ver != 1) ||
			sle16_to_cpu(rstr->minor_ver != 1))
		log_err_exit(buf, "Unknown $LogFile version %i.%i.  Only know "
				"how to handle version 1.1.\n",
				sle16_to_cpu(rstr->major_ver),
				sle16_to_cpu(rstr->minor_ver));
	/* Verify the location and size of the update sequence array. */
	usa_end_ofs = le16_to_cpu(rstr->usa_ofs) +
			le16_to_cpu(rstr->usa_count) * sizeof(u16);
	if (page_size / NTFS_BLOCK_SIZE + 1 != le16_to_cpu(rstr->usa_count))
		log_err_exit(buf, "Restart page header in $LogFile is "
				"corrupt:  Update sequence array size is "
				"wrong.  Cannot handle this yet.\n");
	if (le16_to_cpu(rstr->usa_ofs) < sizeof(RESTART_PAGE_HEADER))
		log_err_exit(buf, "Restart page header in $LogFile is "
				"corrupt:  Update sequence array overlaps "
				"restart page header.  Cannot handle this "
				"yet.\n");
	if (usa_end_ofs > NTFS_BLOCK_SIZE - sizeof(u16))
		log_err_exit(buf, "Restart page header in $LogFile is "
				"corrupt:  Update sequence array overlaps or "
				"is behind first protected sequence number.  "
				"Cannot handle this yet.\n");
	if (usa_end_ofs > le16_to_cpu(rstr->restart_area_offset))
		log_err_exit(buf, "Restart page header in $LogFile is "
				"corrupt:  Update sequence array overlaps or "
				"is behind restart area.  Cannot handle this "
				"yet.\n");
	/* Finally, verify the offset of the restart area. */
	if (le16_to_cpu(rstr->restart_area_offset) & 7)
		log_err_exit(buf, "Restart page header in $LogFile is "
				"corrupt:  Restart area offset is not aligned "
				"to 8-byte boundary.  Cannot handle this "
				"yet.\n");
}

/**
 * dump_restart_areas_header
 */
static void dump_restart_areas_header(RESTART_PAGE_HEADER *rstr)
{
	ntfs_log_info("\nRestart page header:\n");
	ntfs_log_info("magic = %s\n", ntfs_is_rstr_record(rstr->magic) ? "RSTR" :
			"CHKD");
	ntfs_log_info("usa_ofs = %u (0x%x)\n", le16_to_cpu(rstr->usa_ofs),
			le16_to_cpu(rstr->usa_ofs));
	ntfs_log_info("usa_count = %u (0x%x)\n", le16_to_cpu(rstr->usa_count),
			le16_to_cpu(rstr->usa_count));
	ntfs_log_info("chkdsk_lsn = %lli (0x%llx)\n",
			(long long)sle64_to_cpu(rstr->chkdsk_lsn),
			(unsigned long long)sle64_to_cpu(rstr->chkdsk_lsn));
	ntfs_log_info("system_page_size = %u (0x%x)\n",
			(unsigned int)le32_to_cpu(rstr->system_page_size),
			(unsigned int)le32_to_cpu(rstr->system_page_size));
	ntfs_log_info("log_page_size = %u (0x%x)\n",
			(unsigned int)le32_to_cpu(rstr->log_page_size),
			(unsigned int)le32_to_cpu(rstr->log_page_size));
	ntfs_log_info("restart_offset = %u (0x%x)\n",
			le16_to_cpu(rstr->restart_area_offset),
			le16_to_cpu(rstr->restart_area_offset));
}

/**
 * dump_restart_areas_area
 */
static void dump_restart_areas_area(RESTART_PAGE_HEADER *rstr)
{
	LOG_CLIENT_RECORD *lcr;
	RESTART_AREA *ra;
	int client;

	ra = (RESTART_AREA*)((u8*)rstr +
			le16_to_cpu(rstr->restart_area_offset));
	ntfs_log_info("current_lsn = %lli (0x%llx)\n",
			(long long)sle64_to_cpu(ra->current_lsn),
			(unsigned long long)sle64_to_cpu(ra->current_lsn));
	ntfs_log_info("log_clients = %u (0x%x)\n", le16_to_cpu(ra->log_clients),
			le16_to_cpu(ra->log_clients));
	ntfs_log_info("client_free_list = %i (0x%x)\n",
			(s16)le16_to_cpu(ra->client_free_list),
			le16_to_cpu(ra->client_free_list));
	ntfs_log_info("client_in_use_list = %i (0x%x)\n",
			(s16)le16_to_cpu(ra->client_in_use_list),
			le16_to_cpu(ra->client_in_use_list));
	ntfs_log_info("flags = 0x%.4x\n", le16_to_cpu(ra->flags));
	ntfs_log_info("seq_number_bits = %u (0x%x)\n",
			(unsigned int)le32_to_cpu(ra->seq_number_bits),
			(unsigned int)le32_to_cpu(ra->seq_number_bits));
	ntfs_log_info("restart_area_length = %u (0x%x)\n",
			le16_to_cpu(ra->restart_area_length),
			le16_to_cpu(ra->restart_area_length));
	ntfs_log_info("client_array_offset = %u (0x%x)\n",
			le16_to_cpu(ra->client_array_offset),
			le16_to_cpu(ra->client_array_offset));
	ntfs_log_info("file_size = %lli (0x%llx)\n",
			(long long)sle64_to_cpu(ra->file_size),
			(unsigned long long)sle64_to_cpu(ra->file_size));
	ntfs_log_info("last_lsn_data_length = %u (0x%x)\n",
			(unsigned int)le32_to_cpu(ra->last_lsn_data_length),
			(unsigned int)le32_to_cpu(ra->last_lsn_data_length));
	ntfs_log_info("log_record_header_length = %u (0x%x)\n",
			le16_to_cpu(ra->log_record_header_length),
			le16_to_cpu(ra->log_record_header_length));
	ntfs_log_info("log_page_data_offset = %u (0x%x)\n",
			le16_to_cpu(ra->log_page_data_offset),
			le16_to_cpu(ra->log_page_data_offset));
	ntfs_log_info("restart_log_open_count = %u (0x%x)\n",
			(unsigned)le32_to_cpu(ra->restart_log_open_count),
			(unsigned)le32_to_cpu(ra->restart_log_open_count));
	lcr = (LOG_CLIENT_RECORD*)((u8*)ra +
			le16_to_cpu(ra->client_array_offset));
	for (client = 0; client < le16_to_cpu(ra->log_clients); client++) {
		char *client_name;

		ntfs_log_info("\nLog client record number %i:\n", client + 1);
		ntfs_log_info("oldest_lsn = %lli (0x%llx)\n",
				(long long)sle64_to_cpu(lcr->oldest_lsn),
				(unsigned long long)
				sle64_to_cpu(lcr->oldest_lsn));
		ntfs_log_info("client_restart_lsn = %lli (0x%llx)\n", (long long)
				sle64_to_cpu(lcr->client_restart_lsn),
				(unsigned long long)
				sle64_to_cpu(lcr->client_restart_lsn));
		ntfs_log_info("prev_client = %i (0x%x)\n",
				(s16)le16_to_cpu(lcr->prev_client),
				le16_to_cpu(lcr->prev_client));
		ntfs_log_info("next_client = %i (0x%x)\n",
				(s16)le16_to_cpu(lcr->next_client),
				le16_to_cpu(lcr->next_client));
		ntfs_log_info("seq_number = %u (0x%x)\n", le16_to_cpu(lcr->seq_number),
				le16_to_cpu(lcr->seq_number));
		ntfs_log_info("client_name_length = %u (0x%x)\n",
				(unsigned int)le32_to_cpu(lcr->client_name_length) / 2,
				(unsigned int)le32_to_cpu(lcr->client_name_length) / 2);
		if (le32_to_cpu(lcr->client_name_length)) {
			client_name = NULL;
			if (ntfs_ucstombs(lcr->client_name,
					le32_to_cpu(lcr->client_name_length) /
					2, &client_name, 0) < 0) {
				ntfs_log_perror("Failed to convert log client name");
				client_name = strdup("<conversion error>");
			}
		} else
			client_name = strdup("<unnamed>");
		ntfs_log_info("client_name = %s\n", client_name);
		free(client_name);
		/*
		 * Log client records are fixed size so we can simply use the
		 * C increment operator to get to the next one.
		 */
		lcr++;
	}
}

/**
 * dump_restart_areas()
 */
static void *dump_restart_areas(RESTART_PAGE_HEADER *rstr, u8 *buf,
		unsigned int page_size)
{
	int pass = 1;

rstr_pass_loc:
	if (ntfs_is_chkd_record(rstr->magic))
		log_err_exit(buf, "The %s restart page header in $LogFile has "
				"been modified by chkdsk.  Do not know how to "
				"handle this yet.  Reboot into Windows to fix "
				"this.\n", (u8*)rstr == buf ? "first" :
				"second");
	if (ntfs_mst_post_read_fixup((NTFS_RECORD*)rstr, page_size) ||
			ntfs_is_baad_record(rstr->magic))
		log_err_exit(buf, "$LogFile incomplete multi sector transfer "
				"detected in restart page header.  Cannot "
				"handle this yet.\n");
	if (pass == 1)
		ntfs_log_info("$LogFile version %i.%i.\n",
				sle16_to_cpu(rstr->major_ver),
				sle16_to_cpu(rstr->minor_ver));
	else /* if (pass == 2) */ {
		RESTART_AREA *ra;

		/*
		 * rstr is now the second restart page so we declare rstr1
		 * as the first restart page as this one has been verified in
		 * the first pass so we can use all its members safely.
		 */
		RESTART_PAGE_HEADER *rstr1 = (RESTART_PAGE_HEADER*)buf;

		/* Exclude the usa from the comparison. */
		ra = (RESTART_AREA*)((u8*)rstr1 +
				le16_to_cpu(rstr1->restart_area_offset));
		if (!memcmp(rstr1, rstr, le16_to_cpu(rstr1->usa_ofs)) &&
				!memcmp((u8*)rstr1 + le16_to_cpu(
				rstr1->restart_area_offset), (u8*)rstr +
				le16_to_cpu(rstr->restart_area_offset),
				le16_to_cpu(ra->restart_area_length))) {
			puts("\nSkipping analysis of second restart page "
					"because it fully matches the first "
					"one.");
			goto skip_rstr_pass;
		}
		/*
		 * The $LogFile versions specified in each of the two restart
		 * page headers must match.
		 */
		if (rstr1->major_ver != rstr->major_ver ||
				rstr1->minor_ver != rstr->minor_ver)
			log_err_exit(buf, "Second restart area specifies "
					"different $LogFile version to first "
					"restart area.  Cannot handle this "
					"yet.\n");
	}
	/* The restart page header is in rstr and it is mst deprotected. */
	ntfs_log_info("\n%s restart page:\n", pass == 1 ? "1st" : "2nd");
	dump_restart_areas_header(rstr);

	ntfs_log_info("\nRestart area:\n");
	dump_restart_areas_area(rstr);

skip_rstr_pass:
	if (pass == 1) {
		rstr = (RESTART_PAGE_HEADER*)((u8*)rstr + page_size);
		++pass;
		goto rstr_pass_loc;
	}

	return rstr;
}

/**
 * dump_log_records()
 */
static void dump_log_record(LOG_RECORD *lr)
{
	unsigned int i;
	ntfs_log_info("this lsn = 0x%llx\n",
			(unsigned long long)le64_to_cpu(lr->this_lsn));
	ntfs_log_info("client previous lsn = 0x%llx\n", (unsigned long long)
			le64_to_cpu(lr->client_previous_lsn));
	ntfs_log_info("client undo next lsn = 0x%llx\n", (unsigned long long)
			le64_to_cpu(lr->client_undo_next_lsn));
	ntfs_log_info("client data length = 0x%x\n",
			(unsigned int)le32_to_cpu(lr->client_data_length));
	ntfs_log_info("client_id.seq_number = 0x%x\n",
			le16_to_cpu(lr->client_id.seq_number));
	ntfs_log_info("client_id.client_index = 0x%x\n",
			le16_to_cpu(lr->client_id.client_index));
	ntfs_log_info("record type = 0x%x\n",
			(unsigned int)le32_to_cpu(lr->record_type));
	ntfs_log_info("transaction_id = 0x%x\n",
			(unsigned int)le32_to_cpu(lr->transaction_id));
	ntfs_log_info("flags = 0x%x:", lr->flags);
	if (!lr->flags)
		ntfs_log_info(" NONE\n");
	else {
		int _b = 0;

		if (lr->flags & LOG_RECORD_MULTI_PAGE) {
			ntfs_log_info(" LOG_RECORD_MULTI_PAGE");
			_b = 1;
		}
		if (lr->flags & ~LOG_RECORD_MULTI_PAGE) {
			if (_b)
				ntfs_log_info(" |");
			ntfs_log_info(" Unknown flags");
		}
		ntfs_log_info("\n");
	}
	ntfs_log_info("redo_operation = 0x%x\n", le16_to_cpu(lr->redo_operation));
	ntfs_log_info("undo_operation = 0x%x\n", le16_to_cpu(lr->undo_operation));
	ntfs_log_info("redo_offset = 0x%x\n", le16_to_cpu(lr->redo_offset));
	ntfs_log_info("redo_length = 0x%x\n", le16_to_cpu(lr->redo_length));
	ntfs_log_info("undo_offset = 0x%x\n", le16_to_cpu(lr->undo_offset));
	ntfs_log_info("undo_length = 0x%x\n", le16_to_cpu(lr->undo_length));
	ntfs_log_info("target_attribute = 0x%x\n", le16_to_cpu(lr->target_attribute));
	ntfs_log_info("lcns_to_follow = 0x%x\n", le16_to_cpu(lr->lcns_to_follow));
	ntfs_log_info("record_offset = 0x%x\n", le16_to_cpu(lr->record_offset));
	ntfs_log_info("attribute_offset = 0x%x\n", le16_to_cpu(lr->attribute_offset));
	ntfs_log_info("target_vcn = 0x%llx\n",
			(unsigned long long)sle64_to_cpu(lr->target_vcn));
	if (le16_to_cpu(lr->lcns_to_follow) > 0)
		ntfs_log_info("Array of lcns:\n");
	for (i = 0; i < le16_to_cpu(lr->lcns_to_follow); i++)
		ntfs_log_info("lcn_list[%u].lcn = 0x%llx\n", i, (unsigned long long)
				sle64_to_cpu(lr->lcn_list[i].lcn));
}

/**
 * dump_log_records()
 */
static void dump_log_records(RECORD_PAGE_HEADER *rcrd, u8 *buf,
		int buf_size, unsigned int page_size)
{
	LOG_RECORD *lr;
	int pass = 0;
	int client;

	/* Reuse pass for log area. */
rcrd_pass_loc:
	rcrd = (RECORD_PAGE_HEADER*)((u8*)rcrd + page_size);
	if ((u8*)rcrd + page_size > buf + buf_size)
		return;
	ntfs_log_info("\nLog record page number %i", pass);
	if (!ntfs_is_rcrd_record(rcrd->magic) &&
			!ntfs_is_chkd_record(rcrd->magic)) {
		unsigned int i;
		for (i = 0; i < page_size; i++)
			if (((u8*)rcrd)[i] != (u8)-1)
				break;
		if (i < page_size)
			puts(" is corrupt (magic is not RCRD or CHKD).");
		else
			puts(" is empty.");
		pass++;
		goto rcrd_pass_loc;
	} else
		puts(":");
	/* Dump log record page */
	ntfs_log_info("magic = %s\n", ntfs_is_rcrd_record(rcrd->magic) ? "RCRD" :
			"CHKD");
// TODO: I am here... (AIA)
	ntfs_log_info("copy.last_lsn/file_offset = 0x%llx\n", (unsigned long long)
			le64_to_cpu(rcrd->copy.last_lsn));
	ntfs_log_info("flags = 0x%x\n", (unsigned int)le32_to_cpu(rcrd->flags));
	ntfs_log_info("page count = %i\n", le16_to_cpu(rcrd->page_count));
	ntfs_log_info("page position = %i\n", le16_to_cpu(rcrd->page_position));
	ntfs_log_info("header.next_record_offset = 0x%llx\n", (unsigned long long)
			le64_to_cpu(rcrd->header.packed.next_record_offset));
	ntfs_log_info("header.last_end_lsn = 0x%llx\n", (unsigned long long)
			le64_to_cpu(rcrd->header.packed.last_end_lsn));
	/*
	 * Where does the 0x40 come from? Is it just usa_offset +
	 * usa_client * 2 + 7 & ~7 or is it derived from somewhere?
	 */
	lr = (LOG_RECORD*)((u8*)rcrd + 0x40);
	client = 0;
	do {
		ntfs_log_info("\nLog record %i:\n", client);
		dump_log_record(lr);
		client++;
		lr = (LOG_RECORD*)((u8*)lr + 0x70);
	} while (((u8*)lr + 0x70 <= (u8*)rcrd +
			le64_to_cpu(rcrd->header.packed.next_record_offset)));

	pass++;
	goto rcrd_pass_loc;
}

/**
 * main -
 */
int main(int argc, char **argv)
{
	RESTART_PAGE_HEADER *rstr;
	RECORD_PAGE_HEADER *rcrd;
	unsigned int page_size;
	int buf_size, br, err;
	logfile_file logfile;
	u8 *buf;

	ntfs_log_set_handler(ntfs_log_handler_outerr);

	ntfs_log_info("\n");
	if (argc < 2 || argc > 3)
		/* print usage and exit */
		usage(argv[0]);
	/*
	 * If one argument, it is a device containing an NTFS volume which we
	 * need to mount and read the $LogFile from so we can dump its
	 * contents.
	 *
	 * If two arguments the first one must be "-f" and the second one is
	 * the path and name of the $LogFile (or copy thereof) which we need to
	 * read and dump the contents of.
	 */

	if (argc == 2) {
		logfile_open(TRUE, argv[1], &logfile);
	} else /* if (argc == 3) */ {
		if (strncmp(argv[1], "-f", strlen("-f")))
			usage(argv[0]);

		logfile_open(FALSE, argv[2], &logfile);
	}

	buf_size = 64 * 1024 * 1024;

	if (logfile.data_size <= buf_size)
		buf_size = logfile.data_size;
	else
		ntfs_log_error("Warning: $LogFile is too big.  "
			"Only analysing the first 64MiB.\n");

	/* For simplicity we read all of $LogFile/$DATA into memory. */
	buf = malloc(buf_size);
	if (!buf) {
		ntfs_log_perror("Failed to allocate buffer for file data");
		logfile_close(&logfile);
		exit(1);
	}

	br = logfile_pread(&logfile, 0, buf_size, buf);
	err = errno;
	logfile_close(&logfile);
	if (br != buf_size) {
		log_err_exit(buf, "Failed to read $LogFile/$DATA: %s\n",
				br < 0 ? strerror(err) : "Partial read.");
	}

	/*
	 * We now have the entirety of the journal ($LogFile/$DATA or argv[2])
	 * in the memory buffer buf and this has a size of buf_size.  Note we
	 * apply a size capping at 64MiB, so if the journal is any bigger we
	 * only have the first 64MiB.  This should not be a problem as I have
	 * never seen such a large $LogFile.  Usually it is only a few MiB in
	 * size.
	 */
	rstr = (RESTART_PAGE_HEADER*)buf;

	/* Check for presence of restart area signature. */
	if (!ntfs_is_rstr_record(rstr->magic) &&
			!ntfs_is_chkd_record(rstr->magic)) {
		s8 *pos = (s8*)buf;
		s8 *end = pos + buf_size;
		while (pos < end && *pos == -1)
			pos++;
		if (pos != end)
			log_err_exit(buf, "$LogFile contents are corrupt "
					"(magic RSTR is missing).  Cannot "
					"handle this yet.\n");
		/* All bytes are -1. */
		free(buf);
		puts("$LogFile is not initialized.");
		return 0;
	}

	/*
	 * First, verify the restart page header for consistency.
	 */
	restart_header_sanity(rstr, buf);
	page_size = le32_to_cpu(rstr->log_page_size);

	/*
	 * Second, verify the restart area itself.
	 */
	// TODO: Implement this.
	ntfs_log_error("Warning:  Sanity checking of restart area not implemented "
		"yet.\n");
	/*
	 * Third and last, verify the array of log client records.
	 */
	// TODO: Implement this.
	ntfs_log_error("Warning:  Sanity checking of array of log client records not "
		"implemented yet.\n");

	/*
	 * Dump the restart headers & areas.
	 */
	rcrd = (RECORD_PAGE_HEADER*)dump_restart_areas(rstr, buf, page_size);
	ntfs_log_info("\n\nFinished with restart pages.  "
		"Beginning with log pages.\n");

	/*
	 * Dump the log areas.
	 */
	dump_log_records(rcrd, buf, buf_size, page_size);

	free(buf);
	return 0;
}


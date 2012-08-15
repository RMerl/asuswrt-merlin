/*
 * ntfsundelete - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002 Richard Russon
 * Copyright (c) 2007 Yura Pakhuchiy
 *
 * This utility will recover deleted files from an NTFS volume.
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

#ifndef _NTFSUNDELETE_H_
#define _NTFSUNDELETE_H_

#include "types.h"
#include "list.h"
#include "runlist.h"

enum optmode {
	MODE_NONE = 0,
	MODE_SCAN,
	MODE_UNDELETE,
	MODE_COPY,
	MODE_ERROR
};

struct options {
	char		*device;	/* Device/File to work with */
	enum optmode	 mode;		/* Scan / Undelete / Copy */
	int		 percent;	/* Minimum recoverability */
	int		 uinode;	/* Undelete this inode */
	char		*dest;		/* Save file to this directory */
	char		*output;	/* With this filename */
	char		*match;		/* Pattern for filename matching */
	int		 match_case;	/* Case sensitive matching */
	int		 truncate;	/* Truncate files to exact size. */
	int		 quiet;		/* Less output */
	int		 verbose;	/* Extra output */
	int		 force;		/* Override common sense */
	int              optimistic;    /* Undelete in-use clusters as well */
	int		 parent;	/* Show parent directory */
	time_t		 since;		/* Since this time */
	s64		 size_begin;	/* Range for file size */
	s64		 size_end;
	s64		 mft_begin;	/* Range for mft copy */
	s64		 mft_end;
	char		 fillbyte;	/* Use for unrecoverable sections */
};

struct filename {
	struct list_head list;		/* Previous/Next links */
	ntfschar	*uname;		/* Filename in unicode */
	int		 uname_len;	/* and its length */
	long long	 size_alloc;	/* Allocated size (multiple of cluster size) */
	long long	 size_data;	/* Actual size of data */
	FILE_ATTR_FLAGS	 flags;
	time_t		 date_c;	/* Time created */
	time_t		 date_a;	/*	altered */
	time_t		 date_m;	/*	mft record changed */
	time_t		 date_r;	/*	read */
	char		*name;		/* Filename in current locale */
	FILE_NAME_TYPE_FLAGS name_space;
	leMFT_REF	 parent_mref;
	char		*parent_name;
};

struct data {
	struct list_head list;		/* Previous/Next links */
	char		*name;		/* Stream name in current locale */
	ntfschar	*uname;		/* Unicode stream name */
	int		 uname_len;	/* and its length */
	int		 resident;	/* Stream is resident */
	int		 compressed;	/* Stream is compressed */
	int		 encrypted;	/* Stream is encrypted */
	long long	 size_alloc;	/* Allocated size (multiple of cluster size) */
	long long	 size_data;	/* Actual size of data */
	long long	 size_init;	/* Initialised size, may be less than data size */
	long long	 size_vcn;	/* Highest VCN in the data runs */
	runlist_element *runlist;	/* Decoded data runs */
	int		 percent;	/* Amount potentially recoverable */
	void		*data;		/* If resident, a pointer to the data */
};

struct ufile {
	long long	 inode;		/* MFT record number */
	time_t		 date;		/* Last modification date/time */
	struct list_head name;		/* A list of filenames */
	struct list_head data;		/* A list of data streams */
	char		*pref_name;	/* Preferred filename */
	char		*pref_pname;	/*	     parent filename */
	long long	 max_size;	/* Largest size we find */
	int		 attr_list;	/* MFT record may be one of many */
	int		 directory;	/* MFT record represents a directory */
	MFT_RECORD	*mft;		/* Raw MFT record */
};

#endif /* _NTFSUNDELETE_H_ */


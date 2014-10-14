/* AFS Volume Location Service client interface
 *
 * Copyright (C) 2002, 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef AFS_VL_H
#define AFS_VL_H

#include "afs.h"

#define AFS_VL_PORT		7003	/* volume location service port */
#define VL_SERVICE		52	/* RxRPC service ID for the Volume Location service */

enum AFSVL_Operations {
	VLGETENTRYBYID		= 503,	/* AFS Get Cache Entry By ID operation ID */
	VLGETENTRYBYNAME	= 504,	/* AFS Get Cache Entry By Name operation ID */
	VLPROBE			= 514,	/* AFS Probe Volume Location Service operation ID */
};

enum AFSVL_Errors {
	AFSVL_IDEXIST 		= 363520,	/* Volume Id entry exists in vl database */
	AFSVL_IO 		= 363521,	/* I/O related error */
	AFSVL_NAMEEXIST 	= 363522,	/* Volume name entry exists in vl database */
	AFSVL_CREATEFAIL 	= 363523,	/* Internal creation failure */
	AFSVL_NOENT 		= 363524,	/* No such entry */
	AFSVL_EMPTY 		= 363525,	/* Vl database is empty */
	AFSVL_ENTDELETED 	= 363526,	/* Entry is deleted (soft delete) */
	AFSVL_BADNAME 		= 363527,	/* Volume name is illegal */
	AFSVL_BADINDEX 		= 363528,	/* Index is out of range */
	AFSVL_BADVOLTYPE 	= 363529,	/* Bad volume type */
	AFSVL_BADSERVER 	= 363530,	/* Illegal server number (out of range) */
	AFSVL_BADPARTITION 	= 363531,	/* Bad partition number */
	AFSVL_REPSFULL 		= 363532,	/* Run out of space for Replication sites */
	AFSVL_NOREPSERVER 	= 363533,	/* No such Replication server site exists */
	AFSVL_DUPREPSERVER 	= 363534,	/* Replication site already exists */
	AFSVL_RWNOTFOUND 	= 363535,	/* Parent R/W entry not found */
	AFSVL_BADREFCOUNT 	= 363536,	/* Illegal Reference Count number */
	AFSVL_SIZEEXCEEDED 	= 363537,	/* Vl size for attributes exceeded */
	AFSVL_BADENTRY 		= 363538,	/* Bad incoming vl entry */
	AFSVL_BADVOLIDBUMP 	= 363539,	/* Illegal max volid increment */
	AFSVL_IDALREADYHASHED 	= 363540,	/* RO/BACK id already hashed */
	AFSVL_ENTRYLOCKED 	= 363541,	/* Vl entry is already locked */
	AFSVL_BADVOLOPER 	= 363542,	/* Bad volume operation code */
	AFSVL_BADRELLOCKTYPE 	= 363543,	/* Bad release lock type */
	AFSVL_RERELEASE 	= 363544,	/* Status report: last release was aborted */
	AFSVL_BADSERVERFLAG 	= 363545,	/* Invalid replication site server °ag */
	AFSVL_PERM 		= 363546,	/* No permission access */
	AFSVL_NOMEM 		= 363547,	/* malloc/realloc failed to alloc enough memory */
};

/*
 * maps to "struct vldbentry" in vvl-spec.pdf
 */
struct afs_vldbentry {
	char		name[65];		/* name of volume (with NUL char) */
	afs_voltype_t	type;			/* volume type */
	unsigned	num_servers;		/* num servers that hold instances of this vol */
	unsigned	clone_id;		/* cloning ID */

	unsigned	flags;
#define AFS_VLF_RWEXISTS	0x1000		/* R/W volume exists */
#define AFS_VLF_ROEXISTS	0x2000		/* R/O volume exists */
#define AFS_VLF_BACKEXISTS	0x4000		/* backup volume exists */

	afs_volid_t	volume_ids[3];		/* volume IDs */

	struct {
		struct in_addr	addr;		/* server address */
		unsigned	partition;	/* partition ID on this server */
		unsigned	flags;		/* server specific flags */
#define AFS_VLSF_NEWREPSITE	0x0001	/* unused */
#define AFS_VLSF_ROVOL		0x0002	/* this server holds a R/O instance of the volume */
#define AFS_VLSF_RWVOL		0x0004	/* this server holds a R/W instance of the volume */
#define AFS_VLSF_BACKVOL	0x0008	/* this server holds a backup instance of the volume */
	} servers[8];
};

#endif /* AFS_VL_H */

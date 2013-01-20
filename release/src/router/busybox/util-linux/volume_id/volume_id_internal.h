/*
 * volume_id - reads filesystem label and uuid
 *
 * Copyright (C) 2005 Kay Sievers <kay.sievers@vrfy.org>
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation; either
 *	version 2.1 of the License, or (at your option) any later version.
 *
 *	This library is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *	Lesser General Public License for more details.
 *
 *	You should have received a copy of the GNU Lesser General Public
 *	License along with this library; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "libbb.h"
#include "volume_id.h"

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

#define dbg(...) ((void)0)
/* #define dbg(...) bb_error_msg(__VA_ARGS__) */

/* volume_id.h */

#define VOLUME_ID_VERSION		48

#define VOLUME_ID_LABEL_SIZE		64
#define VOLUME_ID_UUID_SIZE		36
#define VOLUME_ID_FORMAT_SIZE		32
#define VOLUME_ID_PARTITIONS_MAX	256

enum volume_id_usage {
	VOLUME_ID_UNUSED,
	VOLUME_ID_UNPROBED,
	VOLUME_ID_OTHER,
	VOLUME_ID_FILESYSTEM,
	VOLUME_ID_PARTITIONTABLE,
	VOLUME_ID_RAID,
	VOLUME_ID_DISKLABEL,
	VOLUME_ID_CRYPTO,
};

#ifdef UNUSED_PARTITION_CODE
struct volume_id_partition {
//	const char	*type;
//	const char	*usage;
//	smallint	usage_id;
//	uint8_t		pt_type_raw;
//	uint64_t	pt_off;
//	uint64_t	pt_len;
};
#endif

struct volume_id {
	int		fd;
//	int		fd_close:1;
	int		error;
	size_t		sbbuf_len;
	size_t		seekbuf_len;
	uint8_t		*sbbuf;
	uint8_t		*seekbuf;
	uint64_t	seekbuf_off;
#ifdef UNUSED_PARTITION_CODE
	struct volume_id_partition *partitions;
	size_t		partition_count;
#endif
//	uint8_t		label_raw[VOLUME_ID_LABEL_SIZE];
//	size_t		label_raw_len;
	char		label[VOLUME_ID_LABEL_SIZE+1];
//	uint8_t		uuid_raw[VOLUME_ID_UUID_SIZE];
//	size_t		uuid_raw_len;
	/* uuid is stored in ASCII (not binary) form here: */
	char		uuid[VOLUME_ID_UUID_SIZE+1];
//	char		type_version[VOLUME_ID_FORMAT_SIZE];
//	smallint	usage_id;
//	const char	*usage;
#if ENABLE_FEATURE_BLKID_TYPE
	const char	*type;
#endif
};

struct volume_id* FAST_FUNC volume_id_open_node(int fd);
int FAST_FUNC volume_id_probe_all(struct volume_id *id, /*uint64_t off,*/ uint64_t size);
void FAST_FUNC free_volume_id(struct volume_id *id);

/* util.h */

/* size of superblock buffer, reiserfs block is at 64k */
#define SB_BUFFER_SIZE				0x11000
/* size of seek buffer, FAT cluster is 32k max */
#define SEEK_BUFFER_SIZE			0x10000

#define bswap16(x) (uint16_t)	( \
				(((uint16_t)(x) & 0x00ffu) << 8) | \
				(((uint16_t)(x) & 0xff00u) >> 8))

#define bswap32(x) (uint32_t)	( \
				(((uint32_t)(x) & 0xff000000u) >> 24) | \
				(((uint32_t)(x) & 0x00ff0000u) >>  8) | \
				(((uint32_t)(x) & 0x0000ff00u) <<  8) | \
				(((uint32_t)(x) & 0x000000ffu) << 24))

#define bswap64(x) (uint64_t)	( \
				(((uint64_t)(x) & 0xff00000000000000ull) >> 56) | \
				(((uint64_t)(x) & 0x00ff000000000000ull) >> 40) | \
				(((uint64_t)(x) & 0x0000ff0000000000ull) >> 24) | \
				(((uint64_t)(x) & 0x000000ff00000000ull) >>  8) | \
				(((uint64_t)(x) & 0x00000000ff000000ull) <<  8) | \
				(((uint64_t)(x) & 0x0000000000ff0000ull) << 24) | \
				(((uint64_t)(x) & 0x000000000000ff00ull) << 40) | \
				(((uint64_t)(x) & 0x00000000000000ffull) << 56))

#if BB_LITTLE_ENDIAN
#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le64_to_cpu(x) (x)
#define be16_to_cpu(x) bswap16(x)
#define be32_to_cpu(x) bswap32(x)
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_be32(x) bswap32(x)
#else
#define le16_to_cpu(x) bswap16(x)
#define le32_to_cpu(x) bswap32(x)
#define le64_to_cpu(x) bswap64(x)
#define be16_to_cpu(x) (x)
#define be32_to_cpu(x) (x)
#define cpu_to_le16(x) bswap16(x)
#define cpu_to_le32(x) bswap32(x)
#define cpu_to_be32(x) (x)
#endif

enum uuid_format {
	UUID_DCE_STRING,
	UUID_DCE,
	UUID_DOS,
	UUID_NTFS,
	UUID_HFS,
};

enum endian {
	LE = 0,
	BE = 1
};

void volume_id_set_unicode16(char *str, size_t len, const uint8_t *buf, enum endian endianess, size_t count);
//void volume_id_set_usage(struct volume_id *id, enum volume_id_usage usage_id);
//void volume_id_set_usage_part(struct volume_id_partition *part, enum volume_id_usage usage_id);
//void volume_id_set_label_raw(struct volume_id *id, const uint8_t *buf, size_t count);
void volume_id_set_label_string(struct volume_id *id, const uint8_t *buf, size_t count);
void volume_id_set_label_unicode16(struct volume_id *id, const uint8_t *buf, enum endian endianess, size_t count);
void volume_id_set_uuid(struct volume_id *id, const uint8_t *buf, enum uuid_format format);
void *volume_id_get_buffer(struct volume_id *id, uint64_t off, size_t len);
void volume_id_free_buffer(struct volume_id *id);


/* Probe routines */

/* RAID */

//int FAST_FUNC volume_id_probe_highpoint_37x_raid(struct volume_id *id /*,uint64_t off*/);
//int FAST_FUNC volume_id_probe_highpoint_45x_raid(struct volume_id *id /*,uint64_t off*/, uint64_t size);

//int FAST_FUNC volume_id_probe_intel_software_raid(struct volume_id *id /*,uint64_t off*/, uint64_t size);

int FAST_FUNC volume_id_probe_linux_raid(struct volume_id *id /*,uint64_t off*/, uint64_t size);

//int FAST_FUNC volume_id_probe_lsi_mega_raid(struct volume_id *id /*,uint64_t off*/, uint64_t size);

//int FAST_FUNC volume_id_probe_nvidia_raid(struct volume_id *id /*,uint64_t off*/, uint64_t size);

//int FAST_FUNC volume_id_probe_promise_fasttrack_raid(struct volume_id *id /*,uint64_t off*/, uint64_t size);

//int FAST_FUNC volume_id_probe_silicon_medley_raid(struct volume_id *id /*,uint64_t off*/, uint64_t size);

//int FAST_FUNC volume_id_probe_via_raid(struct volume_id *id /*,uint64_t off*/, uint64_t size);

//int FAST_FUNC volume_id_probe_lvm1(struct volume_id *id /*,uint64_t off*/);
//int FAST_FUNC volume_id_probe_lvm2(struct volume_id *id /*,uint64_t off*/);

/* FS */

int FAST_FUNC volume_id_probe_btrfs(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_cramfs(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_ext(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_vfat(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_hfs_hfsplus(struct volume_id *id /*,uint64_t off*/);

//int FAST_FUNC volume_id_probe_hpfs(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_iso9660(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_jfs(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_linux_swap(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_luks(struct volume_id *id /*,uint64_t off*/);

//int FAST_FUNC volume_id_probe_mac_partition_map(struct volume_id *id /*,uint64_t off*/);

//int FAST_FUNC volume_id_probe_minix(struct volume_id *id /*,uint64_t off*/);

//int FAST_FUNC volume_id_probe_msdos_part_table(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_ntfs(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_ocfs2(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_reiserfs(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_romfs(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_sysv(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_udf(struct volume_id *id /*,uint64_t off*/);

//int FAST_FUNC volume_id_probe_ufs(struct volume_id *id /*,uint64_t off*/);

int FAST_FUNC volume_id_probe_xfs(struct volume_id *id /*,uint64_t off*/);

POP_SAVED_FUNCTION_VISIBILITY

#ifndef UTIL_LINUX_MINIX_H
#define UTIL_LINUX_MINIX_H

#include <stdint.h>

struct minix_inode {
	uint16_t i_mode;
	uint16_t i_uid;
	uint32_t i_size;
	uint32_t i_time;
	uint8_t  i_gid;
	uint8_t  i_nlinks;
	uint16_t i_zone[9];
};

struct minix2_inode {
	uint16_t i_mode;
	uint16_t i_nlinks;
	uint16_t i_uid;
	uint16_t i_gid;
	uint32_t i_size;
	uint32_t i_atime;
	uint32_t i_mtime;
	uint32_t i_ctime;
	uint32_t i_zone[10];
};

struct minix_super_block {
	uint16_t s_ninodes;
	uint16_t s_nzones;
	uint16_t s_imap_blocks;
	uint16_t s_zmap_blocks;
	uint16_t s_firstdatazone;
	uint16_t s_log_zone_size;
	uint32_t s_max_size;
	uint16_t s_magic;
	uint16_t s_state;
	uint32_t s_zones;
};

/* V3 minix super-block data on disk */
struct minix3_super_block {
	uint32_t s_ninodes;
	uint16_t s_pad0;
	uint16_t s_imap_blocks;
	uint16_t s_zmap_blocks;
	uint16_t s_firstdatazone;
	uint16_t s_log_zone_size;
	uint16_t s_pad1;
	uint32_t s_max_size;
	uint32_t s_zones;
	uint16_t s_magic;
	uint16_t s_pad2;
	uint16_t s_blocksize;
	uint8_t  s_disk_version;
};

/*
 * Minix subpartitions are always within primary dos partition.
 */
#define MINIX_MAXPARTITIONS  4

#define MINIX_BLOCK_SIZE_BITS 10
#define MINIX_BLOCK_SIZE     (1 << MINIX_BLOCK_SIZE_BITS)

#define MINIX_NAME_MAX       255             /* # chars in a file name */
#define MINIX_MAX_INODES     65535

#define MINIX_INODES_PER_BLOCK ((MINIX_BLOCK_SIZE)/(sizeof (struct minix_inode)))
#define MINIX2_INODES_PER_BLOCK ((MINIX_BLOCK_SIZE)/(sizeof (struct minix2_inode)))

/* minix_super_block.s_state */
#define MINIX_VALID_FS       0x0001          /* Clean fs. */
#define MINIX_ERROR_FS       0x0002          /* fs has errors. */

#define MINIX_SUPER_MAGIC    0x137F          /* original minix fs */
#define MINIX_SUPER_MAGIC2   0x138F          /* minix fs, 30 char names */
#define MINIX2_SUPER_MAGIC   0x2468	     /* minix V2 fs */
#define MINIX2_SUPER_MAGIC2  0x2478	     /* minix V2 fs, 30 char names */
#define MINIX3_SUPER_MAGIC   0x4d5a          /* minix V3 fs (60 char names) */

#endif /* UTIL_LINUX_MINIX_H */

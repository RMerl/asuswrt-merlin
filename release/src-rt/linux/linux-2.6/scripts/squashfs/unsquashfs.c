/*
 * Unsquash a squashfs filesystem.  This is a highly compressed read only filesystem.
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007
 *  Phillip Lougher <phillip@lougher.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * unsquash.c
 */

#define CONFIG_SQUASHFS_2_0_COMPATIBILITY

#define TRUE 1
#define FALSE 0
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <zlib.h>
#include <sys/mman.h>
#include <utime.h>

#ifndef linux
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#else
#include <endian.h>
#endif

#include "squashfs_fs.h"
#include "read_fs.h"
#include "global.h"
#include "sqlzma.h"
#include "LzmaDec.h"

#include <stdlib.h>

#ifdef SQUASHFS_TRACE
#define TRACE(s, args...)		do { \
						printf("mksquashfs: "s, ## args); \
					} while(0)
#else
#define TRACE(s, args...)
#endif

#define ERROR(s, args...)		do { \
						fprintf(stderr, s, ## args); \
					} while(0)

#define EXIT_UNSQUASH(s, args...)	do { \
						fprintf(stderr, "FATAL ERROR aborting: "s, ## args); \
					} while(0)

struct hash_table_entry {
	int	start;
	int	bytes;
	struct hash_table_entry *next;
};

typedef struct squashfs_operations {
	struct dir	*(*squashfs_opendir)(unsigned int block_start, unsigned int offset);
	char 		*(*read_fragment)(unsigned int fragment);
	void		(*read_fragment_table)();
	int		(*create_inode)(char *pathname, unsigned int start_block, unsigned int offset);
} squashfs_operations;

squashfs_super_block sBlk;
squashfs_operations s_ops;

int bytes = 0, swap, file_count = 0, dir_count = 0, sym_count = 0,
	dev_count = 0, fifo_count = 0;
char *inode_table = NULL, *directory_table = NULL;
struct hash_table_entry *inode_table_hash[65536], *directory_table_hash[65536];
int fd;
squashfs_fragment_entry *fragment_table;
squashfs_fragment_entry_2 *fragment_table_2;
unsigned int *uid_table, *guid_table;
unsigned int cached_frag = SQUASHFS_INVALID_FRAG;
char *fragment_data;
char *file_data;
char *data;
unsigned int block_size;
int lsonly = FALSE, info = FALSE, force = FALSE;
char **created_inode;
int root_process;

#define CALCULATE_HASH(start)	(start & 0xffff)

int add_entry(struct hash_table_entry *hash_table[], int start, int bytes)
{
	int hash = CALCULATE_HASH(start);
	struct hash_table_entry *hash_table_entry;

	if((hash_table_entry = malloc(sizeof(struct hash_table_entry))) == NULL) {
		ERROR("add_hash: out of memory in malloc\n");
		return FALSE;
	}

	hash_table_entry->start = start;
	hash_table_entry->bytes = bytes;
	hash_table_entry->next = hash_table[hash];
	hash_table[hash] = hash_table_entry;

	return TRUE;
}


int lookup_entry(struct hash_table_entry *hash_table[], int start)
{
	int hash = CALCULATE_HASH(start);
	struct hash_table_entry *hash_table_entry;

	for(hash_table_entry = hash_table[hash]; hash_table_entry;
				hash_table_entry = hash_table_entry->next)
		if(hash_table_entry->start == start)
			return hash_table_entry->bytes;

	return -1;
}


int read_bytes(long long byte, int bytes, char *buff)
{
	off_t off = byte;

	TRACE("read_bytes: reading from position 0x%llx, bytes %d\n", byte, bytes);

	if(lseek(fd, off, SEEK_SET) == -1) {
		ERROR("Lseek failed because %s\b", strerror(errno));
		return FALSE;
	}

	if(read(fd, buff, bytes) == -1) {
		ERROR("Read on destination failed because %s\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}


int read_block(long long start, long long *next, char *block)
{
	unsigned short c_byte;
	int offset = 2;
	
	if(swap) {
		if(read_bytes(start, 2, block) == FALSE)
			goto failed;
		((unsigned char *) &c_byte)[1] = block[0];
		((unsigned char *) &c_byte)[0] = block[1]; 
	} else 
		if(read_bytes(start, 2, (char *)&c_byte) == FALSE)
			goto failed;

	TRACE("read_block: block @0x%llx, %d %s bytes\n", start, SQUASHFS_COMPRESSED_SIZE(c_byte), SQUASHFS_COMPRESSED(c_byte) ? "compressed" : "uncompressed");

	if(SQUASHFS_CHECK_DATA(sBlk.flags))
		offset = 3;
	if(SQUASHFS_COMPRESSED(c_byte)) {
		char buffer[SQUASHFS_METADATA_SIZE];
		int res;
		unsigned long bytes = SQUASHFS_METADATA_SIZE;

		c_byte = SQUASHFS_COMPRESSED_SIZE(c_byte);
		if(read_bytes(start + offset, c_byte, buffer) == FALSE)
			goto failed;
		if (!is_lzma(buffer[0])) {
			if((res = uncompress((unsigned char *) block, &bytes,
			(const unsigned char *) buffer, c_byte)) != Z_OK) {
				if(res == Z_MEM_ERROR)
					ERROR("zlib::uncompress failed, not enough memory\n");
				else if(res == Z_BUF_ERROR)
					ERROR("zlib::uncompress failed, not enough room in output buffer\n");
				else
					ERROR("zlib::uncompress failed, unknown error %d\n", res);
				goto failed;
			}
		}
		else {
			if((res = LzmaUncompress(block, &bytes, buffer, c_byte)) != SZ_OK)
				ERROR("LzmaUncompress: error (%d)\n", res);
		}

		if(next)
			*next = start + offset + c_byte;
		return bytes;
	} else {
		c_byte = SQUASHFS_COMPRESSED_SIZE(c_byte);
		if(read_bytes(start + offset, c_byte, block) == FALSE)
			goto failed;
		if(next)
			*next = start + offset + c_byte;
		return c_byte;
	}

failed:
	return FALSE;
}


int read_data_block(long long start, unsigned int size, char *block)
{
	int res;
	unsigned long bytes = block_size;
	int c_byte = SQUASHFS_COMPRESSED_SIZE_BLOCK(size);

	TRACE("read_data_block: block @0x%llx, %d %s bytes\n", start, SQUASHFS_COMPRESSED_SIZE_BLOCK(c_byte), SQUASHFS_COMPRESSED_BLOCK(c_byte) ? "compressed" : "uncompressed");

	if(SQUASHFS_COMPRESSED_BLOCK(size)) {
		if(read_bytes(start, c_byte, data) == FALSE)
			return 0;

		if (!is_lzma(data[0])) {
			if((res = uncompress((unsigned char *) block, &bytes,
			(const unsigned char *) data, c_byte)) != Z_OK) {
				if(res == Z_MEM_ERROR)
					ERROR("zlib::uncompress failed, not enough memory\n");
				else if(res == Z_BUF_ERROR)
					ERROR("zlib::uncompress failed, not enough room in output buffer\n");
				else
					ERROR("zlib::uncompress failed, unknown error %d\n", res);
				return 0;
			}
		}
		/* LZMA */
		else {
			if((res = LzmaUncompress(block, &bytes, data, c_byte)) != SZ_OK)
				ERROR("LzmaUncompress: error (%d)\n", res);
		}
			
		return bytes;
	} else {
		if(read_bytes(start, c_byte, block) == FALSE)
			return 0;

		return c_byte;
	}
}


void uncompress_inode_table(long long start, long long end)
{
	int size = 0, bytes = 0, res;

	while(start < end) {
		if((size - bytes < SQUASHFS_METADATA_SIZE) &&
				((inode_table = realloc(inode_table, size +=
				SQUASHFS_METADATA_SIZE)) == NULL))
			EXIT_UNSQUASH("uncompress_inode_table: out of memory in realloc\n");
		TRACE("uncompress_inode_table: reading block 0x%llx\n", start);
		add_entry(inode_table_hash, start, bytes);
		if((res = read_block(start, &start, inode_table + bytes)) == 0) {
			free(inode_table);
			EXIT_UNSQUASH("uncompress_inode_table: failed to read block\n");
		}
		bytes += res;
	}
}


int set_attributes(char *pathname, unsigned int mode, unsigned int uid,
unsigned int guid, unsigned int mtime, unsigned int set_mode)
{
	struct utimbuf times = { (time_t) mtime, (time_t) mtime };

	if(utime(pathname, &times) == -1) {
		ERROR("set_attributes: failed to set time on %s, because %s\n", pathname, strerror(errno));
		return FALSE;
	}

	if(root_process) {
		uid_t uid_value = (uid_t) uid_table[uid];
		uid_t guid_value = guid == SQUASHFS_GUIDS ? uid_value : (uid_t) guid_table[guid];

		if(chown(pathname, uid_value, guid_value) == -1) {
			ERROR("set_attributes: failed to change uid and gids on %s, because %s\n", pathname, strerror(errno));
			return FALSE;
		}
	} else
		mode &= ~07000;

	if((set_mode || (mode & 07000)) && chmod(pathname, (mode_t) mode) == -1) {
		ERROR("set_attributes: failed to change mode %s, because %s\n", pathname, strerror(errno));
		return FALSE;
	}

	return TRUE;
}


void read_uids_guids()
{
	if((uid_table = malloc((sBlk.no_uids + sBlk.no_guids) * sizeof(unsigned int))) == NULL)
		EXIT_UNSQUASH("read_uids_guids: failed to allocate uid/gid table\n");

	guid_table = uid_table + sBlk.no_uids;

	if(swap) {
		unsigned int suid_table[sBlk.no_uids + sBlk.no_guids];

		if(read_bytes(sBlk.uid_start, (sBlk.no_uids + sBlk.no_guids)
				* sizeof(unsigned int), (char *) suid_table) ==
				FALSE)
			EXIT_UNSQUASH("read_uids_guids: failed to read uid/gid table\n");
		SQUASHFS_SWAP_INTS(uid_table, suid_table, sBlk.no_uids + sBlk.no_guids);
	} else
		if(read_bytes(sBlk.uid_start, (sBlk.no_uids + sBlk.no_guids)
				* sizeof(unsigned int), (char *) uid_table) ==
				FALSE)
			EXIT_UNSQUASH("read_uids_guids: failed to read uid/gid table\n");
}


void read_fragment_table()
{
	int i, indexes = SQUASHFS_FRAGMENT_INDEXES(sBlk.fragments);
	squashfs_fragment_index fragment_table_index[indexes];

	TRACE("read_fragment_table: %d fragments, reading %d fragment indexes from 0x%llx\n", sBlk.fragments, indexes, sBlk.fragment_table_start);

	if(sBlk.fragments == 0)
		return;

	if((fragment_table = (squashfs_fragment_entry *)
			malloc(sBlk.fragments *
			sizeof(squashfs_fragment_entry))) == NULL)
		EXIT_UNSQUASH("read_fragment_table: failed to allocate fragment table\n");

	if(swap) {
		squashfs_fragment_index sfragment_table_index[indexes];

		read_bytes(sBlk.fragment_table_start, SQUASHFS_FRAGMENT_INDEX_BYTES(sBlk.fragments), (char *) sfragment_table_index);
		SQUASHFS_SWAP_FRAGMENT_INDEXES(fragment_table_index, sfragment_table_index, indexes);
	} else
		read_bytes(sBlk.fragment_table_start, SQUASHFS_FRAGMENT_INDEX_BYTES(sBlk.fragments), (char *) fragment_table_index);

	for(i = 0; i < indexes; i++) {
		int length = read_block(fragment_table_index[i], NULL,
		((char *) fragment_table) + (i * SQUASHFS_METADATA_SIZE));
		TRACE("Read fragment table block %d, from 0x%llx, length %d\n", i, fragment_table_index[i], length);
	}

	if(swap) {
		squashfs_fragment_entry sfragment;
		for(i = 0; i < sBlk.fragments; i++) {
			SQUASHFS_SWAP_FRAGMENT_ENTRY((&sfragment), (&fragment_table[i]));
			memcpy((char *) &fragment_table[i], (char *) &sfragment, sizeof(squashfs_fragment_entry));
		}
	}
}


void read_fragment_table_2()
{
	int i, indexes = SQUASHFS_FRAGMENT_INDEXES_2(sBlk.fragments);
	unsigned int fragment_table_index[indexes];

	TRACE("read_fragment_table: %d fragments, reading %d fragment indexes from 0x%llx\n", sBlk.fragments, indexes, sBlk.fragment_table_start);

	if(sBlk.fragments == 0)
		return;

	if((fragment_table_2 = (squashfs_fragment_entry_2 *)
			malloc(sBlk.fragments *
			sizeof(squashfs_fragment_entry))) == NULL)
		EXIT_UNSQUASH("read_fragment_table: failed to allocate fragment table\n");

	if(swap) {
		 unsigned int sfragment_table_index[indexes];

		read_bytes(sBlk.fragment_table_start, SQUASHFS_FRAGMENT_INDEX_BYTES_2(sBlk.fragments), (char *) sfragment_table_index);
		SQUASHFS_SWAP_FRAGMENT_INDEXES_2(fragment_table_index, sfragment_table_index, indexes);
	} else
		read_bytes(sBlk.fragment_table_start, SQUASHFS_FRAGMENT_INDEX_BYTES_2(sBlk.fragments), (char *) fragment_table_index);

	for(i = 0; i < indexes; i++) {
		int length = read_block(fragment_table_index[i], NULL,
		((char *) fragment_table_2) + (i * SQUASHFS_METADATA_SIZE));
		TRACE("Read fragment table block %d, from 0x%llx, length %d\n", i, fragment_table_index[i], length);
	}

	if(swap) {
		squashfs_fragment_entry_2 sfragment;
		for(i = 0; i < sBlk.fragments; i++) {
			SQUASHFS_SWAP_FRAGMENT_ENTRY_2((&sfragment), (&fragment_table_2[i]));
			memcpy((char *) &fragment_table_2[i], (char *) &sfragment, sizeof(squashfs_fragment_entry_2));
		}
	}
}


char *read_fragment(unsigned int fragment)
{
	TRACE("read_fragment: reading fragment %d\n", fragment);

	if(cached_frag == SQUASHFS_INVALID_FRAG || fragment != cached_frag) {
		squashfs_fragment_entry *fragment_entry = &fragment_table[fragment];
		if(read_data_block(fragment_entry->start_block, fragment_entry->size, fragment_data) == 0) {
			ERROR("read_fragment: failed to read fragment %d\n", fragment);
			cached_frag = SQUASHFS_INVALID_FRAG;
			return NULL;
		}
		cached_frag = fragment;
	}

	return fragment_data;
}


char *read_fragment_2(unsigned int fragment)
{
	TRACE("read_fragment: reading fragment %d\n", fragment);

	if(cached_frag == SQUASHFS_INVALID_FRAG || fragment != cached_frag) {
		squashfs_fragment_entry_2 *fragment_entry = &fragment_table_2[fragment];
		if(read_data_block(fragment_entry->start_block, fragment_entry->size, fragment_data) == 0) {
			ERROR("read_fragment: failed to read fragment %d\n", fragment);
			cached_frag = SQUASHFS_INVALID_FRAG;
			return NULL;
		}
		cached_frag = fragment;
	}

	return fragment_data;
}


int write_file(char *pathname, unsigned int fragment, unsigned int frag_bytes,
unsigned int offset, unsigned int blocks, long long start, char *block_ptr,
unsigned int mode)
{
	unsigned int file_fd, bytes, i;
	unsigned int *block_list;

	TRACE("write_file: regular file, blocks %d\n", blocks);

	if((block_list = malloc(blocks * sizeof(unsigned int))) == NULL) {
		ERROR("write_file: unable to malloc block list\n");
		return FALSE;
	}

	if(swap) {
		unsigned int sblock_list[blocks];
		memcpy(sblock_list, block_ptr, blocks * sizeof(unsigned int));
		SQUASHFS_SWAP_INTS(block_list, sblock_list, blocks);
	} else
		memcpy(block_list, block_ptr, blocks * sizeof(unsigned int));

	if((file_fd = open(pathname, O_CREAT | O_WRONLY | (force ? O_TRUNC : 0), (mode_t) mode & 0777)) == -1) {
		ERROR("write_file: failed to create file %s, because %s\n", pathname,
			strerror(errno));
		free(block_list);
		return FALSE;
	}

	for(i = 0; i < blocks; i++) {
		if((bytes = read_data_block(start, block_list[i], file_data)) == 0) {
			ERROR("write_file: failed to read data block 0x%llx\n", start);
			goto failure;
		}

		if(write(file_fd, file_data, bytes) < bytes) {
			ERROR("write_file: failed to write data block 0x%llx\n", start);
			goto failure;
		}

		start += SQUASHFS_COMPRESSED_SIZE_BLOCK(block_list[i]);
	}

	if(frag_bytes != 0) {
		char *fragment_data = s_ops.read_fragment(fragment);

		if(fragment_data == NULL)
			goto failure;

		if(write(file_fd, fragment_data + offset, frag_bytes) < frag_bytes) {
			ERROR("write_file: failed to write fragment %d\n", fragment);
			goto failure;
		}
	}

	close(file_fd);
	return TRUE;

failure:
	close(file_fd);
	free(block_list);
	return FALSE;
}
		

int create_inode(char *pathname, unsigned int start_block, unsigned int offset)
{
	long long start = sBlk.inode_table_start + start_block;
	squashfs_inode_header header;
	char *block_ptr;
	int bytes = lookup_entry(inode_table_hash, start), file_fd;

	TRACE("create_inode: pathname %s, start 0x%llx, offset %d\n", pathname, start, offset);

	if(bytes == -1) {
		ERROR("create_inode: inode block 0x%llx out of range!\n", start);
		return FALSE;
	}
	block_ptr = inode_table + bytes + offset;

	if(swap) {
		squashfs_base_inode_header sinode;
		memcpy(&sinode, block_ptr, sizeof(header.base));
		SQUASHFS_SWAP_BASE_INODE_HEADER(&header.base, &sinode, sizeof(squashfs_base_inode_header));
	} else
		memcpy(&header.base, block_ptr, sizeof(header.base));

	if(created_inode[header.base.inode_number - 1]) {
		TRACE("create_inode: hard link\n");
		if(force)
			unlink(pathname);

		if(link(created_inode[header.base.inode_number - 1], pathname) == -1) {
			ERROR("create_inode: failed to create hardlink, because %s\n", strerror(errno));
			return FALSE;
		}

		return TRUE;
	}

	switch(header.base.inode_type) {
		case SQUASHFS_FILE_TYPE: {
			unsigned int frag_bytes;
			unsigned int blocks;
			unsigned int offset;
			long long start;
			squashfs_reg_inode_header *inode = &header.reg;

			if(swap) {
				squashfs_reg_inode_header sinode;
				memcpy(&sinode, block_ptr, sizeof(sinode));
				SQUASHFS_SWAP_REG_INODE_HEADER(inode, &sinode);
			} else
				memcpy(inode, block_ptr, sizeof(*inode));

			frag_bytes = inode->fragment == SQUASHFS_INVALID_FRAG ?
				0 : inode->file_size % sBlk.block_size;
			offset = inode->offset;
			blocks = inode->fragment == SQUASHFS_INVALID_FRAG ?
				(inode->file_size + sBlk.block_size - 1) >>
				sBlk.block_log : inode->file_size >>
				sBlk.block_log;
			start = inode->start_block;

			TRACE("create_inode: regular file, file_size %lld, blocks %d\n", inode->file_size, blocks);

			if(write_file(pathname, inode->fragment, frag_bytes,
					offset, blocks, start, block_ptr +
					sizeof(*inode), inode->mode)) {
				set_attributes(pathname, inode->mode, inode->uid, inode->guid, inode->mtime, force);
				file_count ++;
			}
			break;
		}	
		case SQUASHFS_LREG_TYPE: {
			unsigned int frag_bytes;
			unsigned int blocks;
			unsigned int offset;
			long long start;
			squashfs_lreg_inode_header *inode = &header.lreg;

			if(swap) {
				squashfs_lreg_inode_header sinode;
				memcpy(&sinode, block_ptr, sizeof(sinode));
				SQUASHFS_SWAP_LREG_INODE_HEADER(inode, &sinode);
			} else
				memcpy(inode, block_ptr, sizeof(*inode));

			frag_bytes = inode->fragment == SQUASHFS_INVALID_FRAG ?
				0 : inode->file_size % sBlk.block_size;
			offset = inode->offset;
			blocks = inode->fragment == SQUASHFS_INVALID_FRAG ?
				(inode->file_size + sBlk.block_size - 1) >>
				sBlk.block_log : inode->file_size >>
				sBlk.block_log;
			start = inode->start_block;

			TRACE("create_inode: regular file, file_size %lld, blocks %d\n", inode->file_size, blocks);

			if(write_file(pathname, inode->fragment, frag_bytes,
					offset, blocks, start, block_ptr +
					sizeof(*inode), inode->mode)) {
				set_attributes(pathname, inode->mode, inode->uid, inode->guid, inode->mtime, force);
				file_count ++;
			}
			break;
		}	
		case SQUASHFS_SYMLINK_TYPE: {
			squashfs_symlink_inode_header *inodep = &header.symlink;
			char name[65536];

			if(swap) {
				squashfs_symlink_inode_header sinodep;
				memcpy(&sinodep, block_ptr, sizeof(sinodep));
				SQUASHFS_SWAP_SYMLINK_INODE_HEADER(inodep, &sinodep);
			} else
				memcpy(inodep, block_ptr, sizeof(*inodep));

			TRACE("create_inode: symlink, symlink_size %d\n", inodep->symlink_size);

			strncpy(name, block_ptr + sizeof(squashfs_symlink_inode_header), inodep->symlink_size);
			name[inodep->symlink_size] = '\0';

			if(force)
				unlink(pathname);

			if(symlink(name, pathname) == -1) {
				ERROR("create_inode: failed to create symlink %s, because %s\n", pathname,
					strerror(errno));
				break;
			}

			if(root_process) {
				uid_t uid_value = (uid_t) uid_table[inodep->uid];
				uid_t guid_value = inodep->guid ==
					SQUASHFS_GUIDS ? uid_value : (uid_t)
					guid_table[inodep->guid];

				if(lchown(pathname, uid_value, guid_value) == -1)
					ERROR("create_inode: failed to change uid and gids on %s, because %s\n", pathname, strerror(errno));
			}

			sym_count ++;
			break;
		}
 		case SQUASHFS_BLKDEV_TYPE:
	 	case SQUASHFS_CHRDEV_TYPE: {
			squashfs_dev_inode_header *inodep = &header.dev;

			if(swap) {
				squashfs_dev_inode_header sinodep;
				memcpy(&sinodep, block_ptr, sizeof(sinodep));
				SQUASHFS_SWAP_DEV_INODE_HEADER(inodep, &sinodep);
			} else
				memcpy(inodep, block_ptr, sizeof(*inodep));

			TRACE("create_inode: dev, rdev 0x%x\n", inodep->rdev);

			if(root_process) {
				if(force)
					unlink(pathname);

				if(mknod(pathname, inodep->inode_type ==
					SQUASHFS_CHRDEV_TYPE ?  S_IFCHR :
					S_IFBLK, makedev((inodep->rdev >> 8)
					& 0xff, inodep->rdev & 0xff)) == -1) {
					ERROR("create_inode: failed to create %s device %s, because %s\n",
						inodep->inode_type == SQUASHFS_CHRDEV_TYPE ? "character" : "block",
						pathname, strerror(errno));
					break;
				}
				set_attributes(pathname, inodep->mode, inodep->uid, inodep->guid, inodep->mtime, TRUE);
				dev_count ++;
			} else
				ERROR("create_inode: could not create %s device %s, because you're not superuser! %s\n",
					inodep->inode_type == SQUASHFS_CHRDEV_TYPE ? "character" : "block",
					pathname, strerror(errno));
			break;
			}
		case SQUASHFS_FIFO_TYPE:
			TRACE("create_inode: fifo\n");

			if(force)
				unlink(pathname);

			if(mknod(pathname, S_IFIFO, 0) == -1) {
				ERROR("create_inode: failed to create fifo %s, because %s\n",
					pathname, strerror(errno));
				break;
			}
			set_attributes(pathname, header.base.mode, header.base.uid, header.base.guid,
				header.base.mtime, TRUE);
			fifo_count ++;
			break;
		case SQUASHFS_SOCKET_TYPE:
			TRACE("create_inode: socket\n");
			ERROR("create_inode: socket %s ignored\n", pathname);
			break;
		default:
			ERROR("Unknown inode type %d in create_inode_table!\n", header.base.inode_type);
			return FALSE;
	}

	created_inode[header.base.inode_number - 1] = strdup(pathname);

	return TRUE;
}


int create_inode_2(char *pathname, unsigned int start_block, unsigned int offset)
{
	long long start = sBlk.inode_table_start + start_block;
	squashfs_inode_header_2 header;
	char *block_ptr;
	int bytes = lookup_entry(inode_table_hash, start), file_fd;

	TRACE("create_inode: pathname %s, start 0x%llx, offset %d\n", pathname, start, offset);

	if(bytes == -1) {
		ERROR("create_inode: inode block 0x%llx out of range!\n", start);
		return FALSE;
	}
	block_ptr = inode_table + bytes + offset;

	if(swap) {
		squashfs_base_inode_header_2 sinode;
		memcpy(&sinode, block_ptr, sizeof(header.base));
		SQUASHFS_SWAP_BASE_INODE_HEADER_2(&header.base, &sinode, sizeof(squashfs_base_inode_header_2));
	} else
		memcpy(&header.base, block_ptr, sizeof(header.base));

	switch(header.base.inode_type) {
		case SQUASHFS_FILE_TYPE: {
			unsigned int frag_bytes;
			unsigned int blocks;
			unsigned int offset;
			long long start;
			squashfs_reg_inode_header_2 *inode = &header.reg;

			if(swap) {
				squashfs_reg_inode_header_2 sinode;
				memcpy(&sinode, block_ptr, sizeof(sinode));
				SQUASHFS_SWAP_REG_INODE_HEADER_2(inode, &sinode);
			} else
				memcpy(inode, block_ptr, sizeof(*inode));

			frag_bytes = inode->fragment == SQUASHFS_INVALID_FRAG ?
				0 : inode->file_size % sBlk.block_size;
			offset = inode->offset;
			blocks = inode->fragment == SQUASHFS_INVALID_FRAG ?
				(inode->file_size + sBlk.block_size - 1) >>
				sBlk.block_log : inode->file_size >>
				sBlk.block_log;
			start = inode->start_block;

			TRACE("create_inode: regular file, file_size %lld, blocks %d\n", inode->file_size, blocks);

			if(write_file(pathname, inode->fragment, frag_bytes,
					offset, blocks, start, block_ptr +
					sizeof(*inode), inode->mode)) {
				set_attributes(pathname, inode->mode, inode->uid, inode->guid, inode->mtime, force);
				file_count ++;
			}
			break;
		}	
		case SQUASHFS_SYMLINK_TYPE: {
			squashfs_symlink_inode_header_2 *inodep = &header.symlink;
			char name[65536];

			if(swap) {
				squashfs_symlink_inode_header_2 sinodep;
				memcpy(&sinodep, block_ptr, sizeof(sinodep));
				SQUASHFS_SWAP_SYMLINK_INODE_HEADER_2(inodep, &sinodep);
			} else
				memcpy(inodep, block_ptr, sizeof(*inodep));

			TRACE("create_inode: symlink, symlink_size %d\n", inodep->symlink_size);

			strncpy(name, block_ptr + sizeof(squashfs_symlink_inode_header_2), inodep->symlink_size);
			name[inodep->symlink_size] = '\0';

			if(force)
				unlink(pathname);

			if(symlink(name, pathname) == -1) {
				ERROR("create_inode: failed to create symlink %s, because %s\n", pathname,
					strerror(errno));
				break;
			}

			if(root_process) {
				uid_t uid_value = (uid_t) uid_table[inodep->uid];
				uid_t guid_value = inodep->guid ==
					SQUASHFS_GUIDS ? uid_value : (uid_t)
					guid_table[inodep->guid];

				if(lchown(pathname, uid_value, guid_value) == -1)
					ERROR("create_inode: failed to change uid and gids on %s, because %s\n", pathname, strerror(errno));
			}

			sym_count ++;
			break;
		}
 		case SQUASHFS_BLKDEV_TYPE:
	 	case SQUASHFS_CHRDEV_TYPE: {
			squashfs_dev_inode_header_2 *inodep = &header.dev;

			if(swap) {
				squashfs_dev_inode_header_2 sinodep;
				memcpy(&sinodep, block_ptr, sizeof(sinodep));
				SQUASHFS_SWAP_DEV_INODE_HEADER_2(inodep, &sinodep);
			} else
				memcpy(inodep, block_ptr, sizeof(*inodep));

			TRACE("create_inode: dev, rdev 0x%x\n", inodep->rdev);

			if(root_process) {
				if(force)
					unlink(pathname);

				if(mknod(pathname, inodep->inode_type ==
					SQUASHFS_CHRDEV_TYPE ?  S_IFCHR :
					S_IFBLK, makedev((inodep->rdev >> 8)
					& 0xff, inodep->rdev & 0xff)) == -1) {
					ERROR("create_inode: failed to create %s device %s, because %s\n",
						inodep->inode_type == SQUASHFS_CHRDEV_TYPE ? "character" : "block",
						pathname, strerror(errno));
					break;
				}
				set_attributes(pathname, inodep->mode, inodep->uid, inodep->guid, sBlk.mkfs_time, TRUE);
				dev_count ++;
			} else
				ERROR("create_inode: could not create %s device %s, because you're not superuser! %s\n",
					inodep->inode_type == SQUASHFS_CHRDEV_TYPE ? "character" : "block",
					pathname, strerror(errno));
			break;
			}
		case SQUASHFS_FIFO_TYPE:
			TRACE("create_inode: fifo\n");

			if(force)
				unlink(pathname);

			if(mknod(pathname, S_IFIFO, 0) == -1) {
				ERROR("create_inode: failed to create fifo %s, because %s\n",
					pathname, strerror(errno));
				break;
			}
			set_attributes(pathname, header.base.mode, header.base.uid, header.base.guid,
				sBlk.mkfs_time, TRUE);
			fifo_count ++;
			break;
		case SQUASHFS_SOCKET_TYPE:
			TRACE("create_inode: socket\n");
			ERROR("create_inode: socket %s ignored\n", pathname);
			break;
		default:
			ERROR("Unknown inode type %d in create_inode_table!\n", header.base.inode_type);
			return FALSE;
	}

	return TRUE;
}


void uncompress_directory_table(long long start, long long end)
{
	int bytes = 0, size = 0, res;

	while(start < end) {
		if(size - bytes < SQUASHFS_METADATA_SIZE && (directory_table =
				realloc(directory_table, size +=
				SQUASHFS_METADATA_SIZE)) == NULL)
			EXIT_UNSQUASH("uncompress_directory_table: out of memory in realloc\n");
		TRACE("uncompress_directory_table: reading block 0x%llx\n", start);
		add_entry(directory_table_hash, start, bytes);
		if((res = read_block(start, &start, directory_table + bytes)) == 0)
			EXIT_UNSQUASH("uncompress_directory_table: failed to read block\n");
		bytes += res;
	}
}


#define DIR_ENT_SIZE	16

struct dir_ent	{
	char		name[SQUASHFS_NAME_LEN + 1];
	unsigned int	start_block;
	unsigned int	offset;
	unsigned int	type;
};

struct dir {
	int		dir_count;
	int 		cur_entry;
	unsigned int	mode;
	unsigned int	uid;
	unsigned int	guid;
	unsigned int	mtime;
	struct dir_ent	*dirs;
};


struct dir *squashfs_opendir(unsigned int block_start, unsigned int offset)
{
	squashfs_dir_header dirh;
	char buffer[sizeof(squashfs_dir_entry) + SQUASHFS_NAME_LEN + 1];
	squashfs_dir_entry *dire = (squashfs_dir_entry *) buffer;
	long long start = sBlk.inode_table_start + block_start;
	char *block_ptr;
	int bytes = lookup_entry(inode_table_hash, start);
	squashfs_inode_header header;
	int dir_count, size;
	struct dir_ent *new_dir;
	struct dir *dir;

	TRACE("squashfs_opendir: inode start block %d, offset %d\n", block_start, offset);

	if(bytes == -1) {
		ERROR("squashfs_opendir: inode block %d not found!\n", block_start);
		return NULL;
	}
	block_ptr = inode_table + bytes + offset;

	if(swap) {
		squashfs_dir_inode_header sinode;
		memcpy(&sinode, block_ptr, sizeof(header.dir));
		SQUASHFS_SWAP_DIR_INODE_HEADER(&header.dir, &sinode);
	} else
		memcpy(&header.dir, block_ptr, sizeof(header.dir));

	switch(header.dir.inode_type) {
		case SQUASHFS_DIR_TYPE:
			block_start = header.dir.start_block;
			offset = header.dir.offset;
			size = header.dir.file_size;
			break;
		case SQUASHFS_LDIR_TYPE:
			if(swap) {
				squashfs_ldir_inode_header sinode;
				memcpy(&sinode, block_ptr, sizeof(header.ldir));
				SQUASHFS_SWAP_LDIR_INODE_HEADER(&header.ldir, &sinode);
			} else
				memcpy(&header.ldir, block_ptr, sizeof(header.ldir));
			block_start = header.ldir.start_block;
			offset = header.ldir.offset;
			size = header.ldir.file_size;
			break;
		default:
			ERROR("squashfs_opendir: inode not a directory\n");
			return NULL;
	}

	start = sBlk.directory_table_start + block_start;
	bytes = lookup_entry(directory_table_hash, start);

	if(bytes == -1) {
		ERROR("squashfs_opendir: directory block %d not found!\n", block_start);
		return NULL;
	}
	bytes += offset;
	size += bytes - 3;

	if((dir = malloc(sizeof(struct dir))) == NULL) {
		ERROR("squashfs_opendir: malloc failed!\n");
		return NULL;
	}

	dir->dir_count = 0;
	dir->cur_entry = 0;
	dir->mode = header.dir.mode;
	dir->uid = header.dir.uid;
	dir->guid = header.dir.guid;
	dir->mtime = header.dir.mtime;
	dir->dirs = NULL;

	while(bytes < size) {			
		if(swap) {
			squashfs_dir_header sdirh;
			memcpy(&sdirh, directory_table + bytes, sizeof(sdirh));
			SQUASHFS_SWAP_DIR_HEADER(&dirh, &sdirh);
		} else
			memcpy(&dirh, directory_table + bytes, sizeof(dirh));
	
		dir_count = dirh.count + 1;
		TRACE("squashfs_opendir: Read directory header @ byte position %d, %d directory entries\n", bytes, dir_count);
		bytes += sizeof(dirh);

		while(dir_count--) {
			if(swap) {
				squashfs_dir_entry sdire;
				memcpy(&sdire, directory_table + bytes, sizeof(sdire));
				SQUASHFS_SWAP_DIR_ENTRY(dire, &sdire);
			} else
				memcpy(dire, directory_table + bytes, sizeof(dire));
			bytes += sizeof(*dire);

			memcpy(dire->name, directory_table + bytes, dire->size + 1);
			dire->name[dire->size + 1] = '\0';
			TRACE("squashfs_opendir: directory entry %s, inode %d:%d, type %d\n", dire->name, dirh.start_block, dire->offset, dire->type);
			if((dir->dir_count % DIR_ENT_SIZE) == 0) {
				if((new_dir = realloc(dir->dirs, (dir->dir_count + DIR_ENT_SIZE) * sizeof(struct dir_ent))) == NULL) {
					ERROR("squashfs_opendir: realloc failed!\n");
					free(dir->dirs);
					free(dir);
					return NULL;
				}
				dir->dirs = new_dir;
			}
			strcpy(dir->dirs[dir->dir_count].name, dire->name);
			dir->dirs[dir->dir_count].start_block = dirh.start_block;
			dir->dirs[dir->dir_count].offset = dire->offset;
			dir->dirs[dir->dir_count].type = dire->type;
			dir->dir_count ++;
			bytes += dire->size + 1;
		}
	}

	return dir;
}


struct dir *squashfs_opendir_2(unsigned int block_start, unsigned int offset)
{
	squashfs_dir_header_2 dirh;
	char buffer[sizeof(squashfs_dir_entry_2) + SQUASHFS_NAME_LEN + 1];
	squashfs_dir_entry_2 *dire = (squashfs_dir_entry_2 *) buffer;
	long long start = sBlk.inode_table_start + block_start;
	char *block_ptr;
	int bytes = lookup_entry(inode_table_hash, start);
	squashfs_inode_header_2 header;
	int dir_count, size;
	struct dir_ent *new_dir;
	struct dir *dir;

	TRACE("squashfs_opendir: inode start block %d, offset %d\n", block_start, offset);

	if(bytes == -1) {
		ERROR("squashfs_opendir: inode block %d not found!\n", block_start);
		return NULL;
	}
	block_ptr = inode_table + bytes + offset;

	if(swap) {
		squashfs_dir_inode_header_2 sinode;
		memcpy(&sinode, block_ptr, sizeof(header.dir));
		SQUASHFS_SWAP_DIR_INODE_HEADER_2(&header.dir, &sinode);
	} else
		memcpy(&header.dir, block_ptr, sizeof(header.dir));

	switch(header.dir.inode_type) {
		case SQUASHFS_DIR_TYPE:
			block_start = header.dir.start_block;
			offset = header.dir.offset;
			size = header.dir.file_size;
			break;
		case SQUASHFS_LDIR_TYPE:
			if(swap) {
				squashfs_ldir_inode_header_2 sinode;
				memcpy(&sinode, block_ptr, sizeof(header.ldir));
				SQUASHFS_SWAP_LDIR_INODE_HEADER_2(&header.ldir, &sinode);
			} else
				memcpy(&header.ldir, block_ptr, sizeof(header.ldir));
			block_start = header.ldir.start_block;
			offset = header.ldir.offset;
			size = header.ldir.file_size;
			break;
		default:
			ERROR("squashfs_opendir: inode not a directory\n");
			return NULL;
	}

	start = sBlk.directory_table_start + block_start;
	bytes = lookup_entry(directory_table_hash, start);

	if(bytes == -1) {
		ERROR("squashfs_opendir: directory block %d not found!\n", block_start);
		return NULL;
	}
	bytes += offset;
	size += bytes;

	if((dir = malloc(sizeof(struct dir))) == NULL) {
		ERROR("squashfs_opendir: malloc failed!\n");
		return NULL;
	}

	dir->dir_count = 0;
	dir->cur_entry = 0;
	dir->mode = header.dir.mode;
	dir->uid = header.dir.uid;
	dir->guid = header.dir.guid;
	dir->mtime = header.dir.mtime;
	dir->dirs = NULL;

	while(bytes < size) {			
		if(swap) {
			squashfs_dir_header_2 sdirh;
			memcpy(&sdirh, directory_table + bytes, sizeof(sdirh));
			SQUASHFS_SWAP_DIR_HEADER_2(&dirh, &sdirh);
		} else
			memcpy(&dirh, directory_table + bytes, sizeof(dirh));
	
		dir_count = dirh.count + 1;
		TRACE("squashfs_opendir: Read directory header @ byte position %d, %d directory entries\n", bytes, dir_count);
		bytes += sizeof(dirh);

		while(dir_count--) {
			if(swap) {
				squashfs_dir_entry_2 sdire;
				memcpy(&sdire, directory_table + bytes, sizeof(sdire));
				SQUASHFS_SWAP_DIR_ENTRY_2(dire, &sdire);
			} else
				memcpy(dire, directory_table + bytes, sizeof(dire));
			bytes += sizeof(*dire);

			memcpy(dire->name, directory_table + bytes, dire->size + 1);
			dire->name[dire->size + 1] = '\0';
			TRACE("squashfs_opendir: directory entry %s, inode %d:%d, type %d\n", dire->name, dirh.start_block, dire->offset, dire->type);
			if((dir->dir_count % DIR_ENT_SIZE) == 0) {
				if((new_dir = realloc(dir->dirs, (dir->dir_count + DIR_ENT_SIZE) * sizeof(struct dir_ent))) == NULL) {
					ERROR("squashfs_opendir: realloc failed!\n");
					free(dir->dirs);
					free(dir);
					return NULL;
				}
				dir->dirs = new_dir;
			}
			strcpy(dir->dirs[dir->dir_count].name, dire->name);
			dir->dirs[dir->dir_count].start_block = dirh.start_block;
			dir->dirs[dir->dir_count].offset = dire->offset;
			dir->dirs[dir->dir_count].type = dire->type;
			dir->dir_count ++;
			bytes += dire->size + 1;
		}
	}

	return dir;
}


int squashfs_readdir(struct dir *dir, char **name, unsigned int *start_block,
unsigned int *offset, unsigned int *type)
{
	if(dir->cur_entry == dir->dir_count)
		return FALSE;

	*name = dir->dirs[dir->cur_entry].name;
	*start_block = dir->dirs[dir->cur_entry].start_block;
	*offset = dir->dirs[dir->cur_entry].offset;
	*type = dir->dirs[dir->cur_entry].type;
	dir->cur_entry ++;

	return TRUE;
}


void squashfs_closedir(struct dir *dir)
{
	free(dir->dirs);
	free(dir);
}


char *get_component(char *target, char *targname)
{
	while(*target == '/')
		*target ++;

	while(*target != '/' && *target!= '\0')
		*targname ++ = *target ++;

	*targname = '\0';

	return target;
}


int matches(char *targname, char *name)
{
	if(*targname == '\0' || strcmp(targname, name) == 0)
		return TRUE;

	return FALSE;
}


int dir_scan(char *parent_name, unsigned int start_block, unsigned int offset, char *target)
{
	struct dir *dir = s_ops.squashfs_opendir(start_block, offset);
	unsigned int type;
	char *name, pathname[1024];
	char targname[1024];

	target = get_component(target, targname);

	if(dir == NULL) {
		ERROR("dir_scan: Failed to read directory %s (%x:%x)\n", parent_name, start_block, offset);
		return FALSE;
	}

	if(!lsonly && mkdir(parent_name, (mode_t) dir->mode) == -1 && (!force || errno != EEXIST)) {
		ERROR("dir_scan: failed to open directory %s, because %s\n", parent_name, strerror(errno));
		return FALSE;
	}

	while(squashfs_readdir(dir, &name, &start_block, &offset, &type)) {
		TRACE("dir_scan: name %s, start_block %d, offset %d, type %d\n", name, start_block, offset, type);


		if(!matches(targname, name))
			continue;

		strcat(strcat(strcpy(pathname, parent_name), "/"), name);

		if(lsonly || info)
			printf("%s\n", pathname);

		if(type == SQUASHFS_DIR_TYPE)
			dir_scan(pathname, start_block, offset, target);
		else
			if(!lsonly)
				s_ops.create_inode(pathname, start_block, offset);
	}

	!lsonly && set_attributes(parent_name, dir->mode, dir->uid, dir->guid, dir->mtime, force);

	squashfs_closedir(dir);
	dir_count ++;

	return TRUE;
}


int read_super(char *source)
{
	squashfs_super_block sblk;

	read_bytes(SQUASHFS_START, sizeof(squashfs_super_block), (char *) &sBlk);

	/* Check it is a SQUASHFS superblock */
	swap = 0;
	switch (sBlk.s_magic) {
	case SQUASHFS_MAGIC:
	case SQUASHFS_MAGIC_LZMA:
		break;
	case SQUASHFS_MAGIC_SWAP:
	case SQUASHFS_MAGIC_LZMA_SWAP:
		ERROR("Reading a different endian SQUASHFS filesystem on %s\n", source);
		SQUASHFS_SWAP_SUPER_BLOCK(&sblk, &sBlk);
		memcpy(&sBlk, &sblk, sizeof(squashfs_super_block));
		swap = 1;
	default:
		ERROR("Can't find a SQUASHFS superblock on %s\n", source);
		goto failed_mount;
	}

	/* Check the MAJOR & MINOR versions */
	if(sBlk.s_major == 2) {
		sBlk.bytes_used = sBlk.bytes_used_2;
		sBlk.uid_start = sBlk.uid_start_2;
		sBlk.guid_start = sBlk.guid_start_2;
		sBlk.inode_table_start = sBlk.inode_table_start_2;
		sBlk.directory_table_start = sBlk.directory_table_start_2;
		sBlk.fragment_table_start = sBlk.fragment_table_start_2;
		
		s_ops.squashfs_opendir = squashfs_opendir_2;
		s_ops.read_fragment = read_fragment_2;
		s_ops.read_fragment_table = read_fragment_table_2;
		s_ops.create_inode = create_inode_2;
	} else if(sBlk.s_major == 3 && sBlk.s_minor == 0) {
		s_ops.squashfs_opendir = squashfs_opendir;
		s_ops.read_fragment = read_fragment;
		s_ops.read_fragment_table = read_fragment_table;
		s_ops.create_inode = create_inode;
	} else {
		ERROR("Major/Minor mismatch, filesystem on %s is (%d:%d)\n",
				source, sBlk.s_major, sBlk.s_minor);
		ERROR("I support Squashfs 2.x and 3.0 filesystems!\n");
		goto failed_mount;
	}

#if __BYTE_ORDER == __BIG_ENDIAN
	TRACE("Found a valid %s endian SQUASHFS %d:%d superblock on %s.\n", swap ? "little" : "big", sBlk.s_major, sBlk.s_minor, source);
#else
	TRACE("Found a valid %s endian SQUASHFS %d:%d superblock on %s.\n", swap ? "big" : "little", sBlk.s_major, sBlk.s_minor, source);
#endif

	TRACE("\tInodes are %scompressed\n", SQUASHFS_UNCOMPRESSED_INODES(sBlk.flags) ? "un" : "");
	TRACE("\tData is %scompressed\n", SQUASHFS_UNCOMPRESSED_DATA(sBlk.flags) ? "un" : "");
	TRACE("\tFragments are %scompressed\n", SQUASHFS_UNCOMPRESSED_FRAGMENTS(sBlk.flags) ? "un" : "");
	TRACE("\tCheck data is %s present in the filesystem\n", SQUASHFS_CHECK_DATA(sBlk.flags) ? "" : "not");
	TRACE("\tFragments are %s present in the filesystem\n", SQUASHFS_NO_FRAGMENTS(sBlk.flags) ? "not" : "");
	TRACE("\tAlways_use_fragments option is %s specified\n", SQUASHFS_ALWAYS_FRAGMENTS(sBlk.flags) ? "" : "not");
	TRACE("\tDuplicates are %s removed\n", SQUASHFS_DUPLICATES(sBlk.flags) ? "" : "not");
	TRACE("\tFilesystem size %.2f Kbytes (%.2f Mbytes)\n", sBlk.bytes_used / 1024.0, sBlk.bytes_used / (1024.0 * 1024.0));
	TRACE("\tBlock size %d\n", sBlk.block_size);
	TRACE("\tNumber of fragments %d\n", sBlk.fragments);
	TRACE("\tNumber of inodes %d\n", sBlk.inodes);
	TRACE("\tNumber of uids %d\n", sBlk.no_uids);
	TRACE("\tNumber of gids %d\n", sBlk.no_guids);
	TRACE("sBlk.inode_table_start 0x%llx\n", sBlk.inode_table_start);
	TRACE("sBlk.directory_table_start 0x%llx\n", sBlk.directory_table_start);
	TRACE("sBlk.uid_start 0x%llx\n", sBlk.uid_start);
	TRACE("sBlk.fragment_table_start 0x%llx\n\n", sBlk.fragment_table_start);

	return TRUE;

failed_mount:
	return FALSE;
}


#define VERSION() \
	printf("unsquashfs version 1.3 (2007/01/02)\n");\
	printf("copyright (C) 2007 Phillip Lougher <phillip@lougher.org.uk>\n\n"); \
    	printf("This program is free software; you can redistribute it and/or\n");\
	printf("modify it under the terms of the GNU General Public License\n");\
	printf("as published by the Free Software Foundation; either version 2,\n");\
	printf("or (at your option) any later version.\n\n");\
	printf("This program is distributed in the hope that it will be useful,\n");\
	printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");\
	printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");\
	printf("GNU General Public License for more details.\n");
int main(int argc, char *argv[])
{
	char *dest = "squashfs-root";
	int i, version = FALSE;
	char *target = "";

	if((root_process = (geteuid() == 0)))
		umask(0);
	
	for(i = 1; i < argc; i++) {
		if(*argv[i] != '-')
			break;
		if(strcmp(argv[i], "-version") == 0 || strcmp(argv[i], "-v") == 0) {
			VERSION();
			version = TRUE;
		} else if(strcmp(argv[i], "-info") == 0 || strcmp(argv[i], "-i") == 0)
			info = TRUE;
		else if(strcmp(argv[i], "-ls") == 0 || strcmp(argv[i], "-l") == 0)
			lsonly = TRUE;
		else if(strcmp(argv[i], "-dest") == 0 || strcmp(argv[i], "-d") == 0) {
			if(++i == argc)
				goto options;
			dest = argv[i];
		} else if(strcmp(argv[i], "-force") == 0 || strcmp(argv[i], "-f") == 0)
			force = TRUE;
	}

	if(i == argc) {
		if(!version) {
options:
			ERROR("SYNTAX: %s [options] filesystem [directory or file to extract]\n", argv[0]);
			ERROR("\t-v[ersion]\t\tprint version, licence and copyright information\n");
			ERROR("\t-i[nfo]\t\t\tprint files as they are unsquashed\n");
			ERROR("\t-l[s]\t\t\tlist filesystem only\n");
			ERROR("\t-d[est] <pathname>\tunsquash to <pathname>, default \"squashfs-root\"\n");
			ERROR("\t-f[orce]\t\tif file already exists then overwrite\n");
		}
		exit(1);
	}

	if((i + 1) < argc)
		target = argv[i + 1];

	if((fd = open(argv[i], O_RDONLY)) == -1) {
		ERROR("Could not open %s, because %s\n", argv[i], strerror(errno));
		exit(1);
	}

	if(read_super(argv[i]) == FALSE)
		exit(1);

	block_size = sBlk.block_size;
	if((fragment_data = malloc(block_size)) == NULL)
		EXIT_UNSQUASH("failed to allocate fragment_data\n");

	if((file_data = malloc(block_size)) == NULL)
		EXIT_UNSQUASH("failed to allocate file_data");

	if((data = malloc(block_size)) == NULL)
		EXIT_UNSQUASH("failed to allocate datan\n");

	if((created_inode = malloc(sBlk.inodes * sizeof(char *))) == NULL)
		EXIT_UNSQUASH("failed to allocate created_inode\n");

	memset(created_inode, 0, sBlk.inodes * sizeof(char *));

	read_uids_guids();
	s_ops.read_fragment_table();
	uncompress_inode_table(sBlk.inode_table_start, sBlk.directory_table_start);
	uncompress_directory_table(sBlk.directory_table_start, sBlk.fragment_table_start);

	dir_scan(dest, SQUASHFS_INODE_BLK(sBlk.root_inode), SQUASHFS_INODE_OFFSET(sBlk.root_inode), target);

	if(!lsonly) {
		printf("\n");
		printf("created %d files\n", file_count);
		printf("created %d directories\n", dir_count);
		printf("created %d symlinks\n", sym_count);
		printf("created %d devices\n", dev_count);
		printf("created %d fifos\n", fifo_count);
	}
	return 0;
}

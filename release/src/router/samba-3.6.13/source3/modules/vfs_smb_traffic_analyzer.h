/*
 * traffic-analyzer VFS module. Measure the smb traffic users create
 * on the net.
 *
 * Copyright (C) Holger Hetterich, 2008
 * Copyright (C) Jeremy Allison, 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * Protocol version 2.0 description
 *
 * The following table shows the exact assembly of the 2.0 protocol.
 *
 * -->Header<--
 * The protocol header is always send first, and contains various
 * information about the data block to come.
 * The header is always of fixed length, and will be send unencrypted.
 *
 * Byte Number/Bytes    Description
 * 00-02                Contains always the string "V2."
 * 03                   This byte contains a possible subrelease number of the
 *                      protocol. This enables the receiver to make a version
 *                      check to ensure the compatibility and allows us to
 *                      release 2.x versions of the protocol with bugfixes or
 *                      enhancements.
 * 04                   This byte is reserved for possible future extensions.
 * 05                   Usually, this byte contains the character '0'. If the
 *                      VFS module is configured for encryption of the data,
 *                      this byte is set to 'E'.
 * 06-09                These bytes contain the character '0' by default, and
 *                      are reserved for possible future extensions. They have
 *                      no function in 2.0.
 * 10-27                17 bytes containing a string representation of the
 *                      number of bytes to come in the following data block.
 *                      It is right aligned and filled from the left with '0'.
 *
 * -->Data Block<--
 * The data block is send immediately after the header was send. It's length
 * is exactly what was given in bytes 11-28 from in the header.
 *
 * The data block may be send encrypted.
 *
 * To make the data block easy for the receiver to read, it is divided into
 * several sub-blocks, each with it's own header of four byte length. In each
 * of the sub-headers, a string representation of the length of this block is
 * to be found.
 *
 * Thus the formal structure is very simple:
 *
 * [HEADER]data[HEADER]data[HEADER]data[END]
 *
 * whereas [END] is exactly at the position given in bytes 11-28 of the
 * header.
 *
 * Some data the VFS module is capturing is of use for any VFS operation.
 * Therefore, there is a "common set" of data, that will be send with any
 * data block. The following provides a list of this data.
 * - the VFS function identifier (see VFS function ifentifier table below).
 * - a timestamp to the millisecond.
 * - the username (as text) who runs the VFS operation.
 * - the SID of the user who run the VFS operation.
 * - the domain under which the VFS operation has happened.
 *
 */

/* Protocol subrelease number */
#define SMBTA_SUBRELEASE '0'

/*
 * Every data block sends a number of blocks sending common data
 * we send the number of "common data blocks" to come very first
 * so that if the receiver is using an older version of the protocol
 * it knows which blocks it can ignore.
 */
#define SMBTA_COMMON_DATA_COUNT "00017"

/*
 * VFS Functions identifier table. In protocol version 2, every vfs
 * function is given a unique id.
 */
enum vfs_id {
        /*
         * care for the order here, required for compatibility
         * with protocol version 1.
         */
        vfs_id_read,
        vfs_id_pread,
        vfs_id_write,
        vfs_id_pwrite,
        /* end of protocol version 1 identifiers. */
        vfs_id_mkdir,
        vfs_id_rmdir,
        vfs_id_rename,
        vfs_id_chdir,
	vfs_id_open,
	vfs_id_close
};



/*
 * Specific data sets for the VFS functions.
 * A compatible receiver has to have the exact same dataset.
 */
struct open_data {
	const char *filename;
	mode_t mode;
	int result;
};

struct close_data {
	const char *filename;
	int result;
};

struct mkdir_data {
        const char *path;
        mode_t mode;
        int result;
};

struct rmdir_data {
        const char *path;
        int result;
};

struct rename_data {
        const char *src;
        const char *dst;
        int result;
};

struct chdir_data {
        const char *path;
        int result;
};

/* rw_data used for read/write/pread/pwrite */
struct rw_data {
        char *filename;
        size_t len;
};



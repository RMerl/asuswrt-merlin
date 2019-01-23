/* 
   Unix SMB/CIFS implementation.
   client file read/write routines
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) James Myers 2003
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/libcli.h"

/****************************************************************************
  Read size bytes at offset offset using SMBreadX.
****************************************************************************/
ssize_t smbcli_read(struct smbcli_tree *tree, int fnum, void *_buf, off_t offset, 
		 size_t size)
{
	uint8_t *buf = (uint8_t *)_buf;
	union smb_read parms;
	int readsize;
	ssize_t total = 0;
	
	if (size == 0) {
		return 0;
	}

	parms.readx.level = RAW_READ_READX;
	parms.readx.in.file.fnum = fnum;

	/*
	 * Set readsize to the maximum size we can handle in one readX,
	 * rounded down to a multiple of 1024.
	 */
	readsize = (tree->session->transport->negotiate.max_xmit - (MIN_SMB_SIZE+32));
	if (readsize > 0xFFFF) readsize = 0xFFFF;

	while (total < size) {
		NTSTATUS status;

		readsize = MIN(readsize, size-total);

		parms.readx.in.offset    = offset;
		parms.readx.in.mincnt    = readsize;
		parms.readx.in.maxcnt    = readsize;
		parms.readx.in.remaining = size - total;
		parms.readx.in.read_for_execute = false;
		parms.readx.out.data     = buf + total;
		
		status = smb_raw_read(tree, &parms);
		
		if (!NT_STATUS_IS_OK(status)) {
			return -1;
		}

		total += parms.readx.out.nread;
		offset += parms.readx.out.nread;

		/* If the server returned less than we asked for we're at EOF */
		if (parms.readx.out.nread < readsize)
			break;
	}

	return total;
}


/****************************************************************************
  write to a file
  write_mode: 0x0001 disallow write cacheing
              0x0002 return bytes remaining
              0x0004 use raw named pipe protocol
              0x0008 start of message mode named pipe protocol
****************************************************************************/
ssize_t smbcli_write(struct smbcli_tree *tree,
		     int fnum, uint16_t write_mode,
		     const void *_buf, off_t offset, size_t size)
{
	const uint8_t *buf = (const uint8_t *)_buf;
	union smb_write parms;
	int block = (tree->session->transport->negotiate.max_xmit - (MIN_SMB_SIZE+32));
	ssize_t total = 0;

	if (size == 0) {
		return 0;
	}

	if (block > 0xFFFF) block = 0xFFFF;


	parms.writex.level = RAW_WRITE_WRITEX;
	parms.writex.in.file.fnum = fnum;
	parms.writex.in.wmode = write_mode;
	parms.writex.in.remaining = 0;
	
	while (total < size) {
		NTSTATUS status;

		block = MIN(block, size - total);

		parms.writex.in.offset = offset;
		parms.writex.in.count = block;
		parms.writex.in.data = buf;

		status = smb_raw_write(tree, &parms);

		if (!NT_STATUS_IS_OK(status)) {
			return -1;
		}

		offset += parms.writex.out.nwritten;
		total += parms.writex.out.nwritten;
		buf += parms.writex.out.nwritten;
	}

	return total;
}

/****************************************************************************
  write to a file using a SMBwrite and not bypassing 0 byte writes
****************************************************************************/
ssize_t smbcli_smbwrite(struct smbcli_tree *tree,
		     int fnum, const void *_buf, off_t offset, size_t size1)
{
	const uint8_t *buf = (const uint8_t *)_buf;
	union smb_write parms;
	ssize_t total = 0;

	parms.write.level = RAW_WRITE_WRITE;
	parms.write.in.remaining = 0;
	
	do {
		size_t size = MIN(size1, tree->session->transport->negotiate.max_xmit - 48);
		if (size > 0xFFFF) size = 0xFFFF;
		
		parms.write.in.file.fnum = fnum;
		parms.write.in.offset = offset;
		parms.write.in.count = size;
		parms.write.in.data = buf + total;

		if (NT_STATUS_IS_ERR(smb_raw_write(tree, &parms)))
			return -1;

		size = parms.write.out.nwritten;
		if (size == 0)
			break;

		size1 -= size;
		total += size;
		offset += size;
	} while (size1);

	return total;
}

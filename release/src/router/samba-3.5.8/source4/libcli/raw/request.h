#ifndef _REQUEST_H
#define _REQUEST_H
/*
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 2003
   Copyright (C) James Myers 2003 <myersjj@samba.org>

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

#include "libcli/raw/signing.h"

#define BUFINFO_FLAG_UNICODE 0x0001
#define BUFINFO_FLAG_SMB2    0x0002

/*
  buffer limit structure used by both SMB and SMB2
 */
struct request_bufinfo {
	TALLOC_CTX *mem_ctx;
	uint32_t flags;
	const uint8_t *align_base;
	const uint8_t *data;
	size_t data_size;	
};

/*
  Shared state structure between client and server, representing the basic packet.
*/

struct smb_request_buffer {
	/* the raw SMB buffer, including the 4 byte length header */
	uint8_t *buffer;
	
	/* the size of the raw buffer, including 4 byte header */
	size_t size;
	
	/* how much has been allocated - on reply the buffer is over-allocated to 
	   prevent too many realloc() calls 
	*/
	size_t allocated;
	
	/* the start of the SMB header - this is always buffer+4 */
	uint8_t *hdr;
	
	/* the command words and command word count. vwv points
	   into the raw buffer */
	uint8_t *vwv;
	uint_t wct;
	
	/* the data buffer and size. data points into the raw buffer */
	uint8_t *data;
	size_t data_size;
	
	/* ptr is used as a moving pointer into the data area
	 * of the packet. The reason its here and not a local
	 * variable in each function is that when a realloc of
	 * a send packet is done we need to move this
	 * pointer */
	uint8_t *ptr;

	/* this is used to range check and align strings and buffers */
	struct request_bufinfo bufinfo;
};

#endif

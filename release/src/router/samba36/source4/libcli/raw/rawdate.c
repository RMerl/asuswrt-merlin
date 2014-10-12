/* 
   Unix SMB/CIFS implementation.

   raw date handling functions

   Copyright (C) Andrew Tridgell 2004
   
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

/*******************************************************************
put a dos date into a buffer (time/date format)
This takes GMT time and puts local time for zone_offset in the buffer
********************************************************************/
void raw_push_dos_date(struct smbcli_transport *transport,
		      uint8_t *buf, int offset, time_t unixdate)
{
	push_dos_date(buf, offset, unixdate, transport->negotiate.server_zone);
}

/*******************************************************************
put a dos date into a buffer (date/time format)
This takes GMT time and puts local time in the buffer
********************************************************************/
void raw_push_dos_date2(struct smbcli_transport *transport,
		       uint8_t *buf, int offset, time_t unixdate)
{
	push_dos_date2(buf, offset, unixdate, transport->negotiate.server_zone);
}

/*******************************************************************
put a dos 32 bit "unix like" date into a buffer. This routine takes
GMT and converts it to LOCAL time in zone_offset before putting it
********************************************************************/
void raw_push_dos_date3(struct smbcli_transport *transport,
		       uint8_t *buf, int offset, time_t unixdate)
{
	push_dos_date3(buf, offset, unixdate, transport->negotiate.server_zone);
}

/*******************************************************************
convert a dos date
********************************************************************/
time_t raw_pull_dos_date(struct smbcli_transport *transport, 
			 const uint8_t *date_ptr)
{
	return pull_dos_date(date_ptr, transport->negotiate.server_zone);
}

/*******************************************************************
like raw_pull_dos_date() but the words are reversed
********************************************************************/
time_t raw_pull_dos_date2(struct smbcli_transport *transport, 
			  const uint8_t *date_ptr)
{
	return pull_dos_date2(date_ptr, transport->negotiate.server_zone);
}

/*******************************************************************
  create a unix GMT date from a dos date in 32 bit "unix like" format
  these arrive in server zone, with corresponding DST
  ******************************************************************/
time_t raw_pull_dos_date3(struct smbcli_transport *transport,
			  const uint8_t *date_ptr)
{
	return pull_dos_date3(date_ptr, transport->negotiate.server_zone);
}

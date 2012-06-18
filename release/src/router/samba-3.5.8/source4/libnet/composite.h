/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Rafal Szczesniak 2005
   
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

/*
 * Monitor structure and message types definitions. Composite function monitoring
 * allows client application to be notified on function progress. This enables
 * eg. gui client to display progress bars, status messages, etc.
 */


#define  mon_SamrCreateUser        (0x00000001)
#define  mon_SamrOpenUser          (0x00000002)
#define  mon_SamrQueryUser         (0x00000003)
#define  mon_SamrCloseUser         (0x00000004)
#define  mon_SamrLookupName        (0x00000005)
#define  mon_SamrDeleteUser        (0x00000006)
#define  mon_SamrSetUser           (0x00000007)
#define  mon_SamrClose             (0x00000008)
#define  mon_SamrConnect           (0x00000009)
#define  mon_SamrLookupDomain      (0x0000000A)
#define  mon_SamrOpenDomain        (0x0000000B)
#define  mon_SamrEnumDomains       (0x0000000C)
#define  mon_LsaOpenPolicy         (0x0000000D)
#define  mon_LsaQueryPolicy        (0x0000000E)
#define  mon_LsaClose              (0x0000000F)
#define  mon_SamrOpenGroup         (0x00000010)
#define  mon_SamrQueryGroup        (0x00000011)

#define  mon_NetLookupDc           (0x00000100)
#define  mon_NetRpcConnect         (0x00000200)

#define  mon_Mask_Rpc              (0x000000FF)
#define  mon_Mask_Net              (0x0000FF00)


struct monitor_msg {
	uint32_t   type;
	void       *data;
	size_t     data_size;
};

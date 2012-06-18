/* 
   Unix SMB/CIFS implementation.
   Kerberos authorization data
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2003
   
   
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

#ifndef _AUTHDATA_H
#define _AUTHDATA_H

#define PAC_TYPE_LOGON_INFO 1
#define PAC_TYPE_SERVER_CHECKSUM 6
#define PAC_TYPE_PRIVSVR_CHECKSUM 7
#define PAC_TYPE_LOGON_NAME 10

#ifndef KRB5_AUTHDATA_WIN2K_PAC
#define KRB5_AUTHDATA_WIN2K_PAC 128
#endif

#ifndef KRB5_AUTHDATA_IF_RELEVANT
#define KRB5_AUTHDATA_IF_RELEVANT 1
#endif

#endif

/* 
   Parameters for Samba's Internal Registry Database
   
   Copyright (C) Michael Adam 2007
   
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

#ifndef _REG_DB_H
#define _REG_DB_H

#define REG_TDB_FLAGS   TDB_SEQNUM

#define REGVER_V1       1       /* first db version with write support */
#define REGVER_V2       2       /* version 2 with normalized keys */

#define REG_VALUE_PREFIX    "SAMBA_REGVAL"
#define REG_SECDESC_PREFIX  "SAMBA_SECDESC"
#define REG_SORTED_SUBKEYS_PREFIX  "SAMBA_SORTED_SUBKEYS"

#endif /* _REG_DB_H */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef   __TYPECONVERT_H__
#define   __TYPECONVERT_H__

#include <sys/types.h>

typedef u_int8_t	BYTE;
typedef u_int8_t	UCHAR;

typedef u_int16_t	USHORT;
typedef u_int16_t	WCHAR;

typedef u_int32_t	LONG;
typedef u_int32_t	ULONG;
typedef u_int32_t	BOOL;
typedef u_int32_t	BOOLEAN;

typedef u_int64_t	LONG64;
typedef u_int64_t	ULONG64;
typedef u_int64_t	LARGE_INTEGER;

typedef void		VOID;
typedef void *		PVOID;

typedef	u_int32_t	NTSTATUS;
typedef	u_int32_t *	PNTSTATUS;

#endif /*  __TYPECONVERT_H__ */


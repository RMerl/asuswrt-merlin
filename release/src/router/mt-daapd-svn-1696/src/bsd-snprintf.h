/*
 * $Id: $
 *
 * Created by Ron Pedde on 2007-07-01.
 * Copyright (C) 2007 Firefly Media Services. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 

#ifndef _BSD_SNPRINTF_H_
#define _BSD_SNPRINTF_H_

extern int io_util_vsnprintf(char *str, size_t count, const char *fmt, va_list args);
extern int io_util_snprintf(char *str,size_t count,const char *fmt,...);

#endif /* _BSD_SNPRINTF_H_ */


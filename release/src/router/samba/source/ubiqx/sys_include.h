#ifndef SYS_INCLUDE_H
#define SYS_INCLUDE_H
/* ========================================================================== **
 *                               sys_include.h
 *
 *  Copyright (C) 1998 by Christopher R. Hertel
 *
 *  Email: crh@ubiqx.mn.org
 * -------------------------------------------------------------------------- **
 *  This header provides system declarations and data types used internally
 *  by the ubiqx modules.
 * -------------------------------------------------------------------------- **
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * -------------------------------------------------------------------------- **
 *
 *  Samba version of sys_include.h
 *
 * ========================================================================== **
 */

#ifndef _INCLUDES_H

/* Block the inclusion of some Samba headers so that ubiqx types won't be
 * used before the headers that define them.  These headers are not needed
 * in the ubiqx modules anyway.
 */
#define _PROTO_H_
#define _NAMESERV_H_
#define _HASH_H_

/* The main Samba system-adaptive header file.
 */
#include "includes.h"

#endif /* _INCLUDES_H */

/* ================================ The End ================================= */
#endif /* SYS_INCLUDE_H */

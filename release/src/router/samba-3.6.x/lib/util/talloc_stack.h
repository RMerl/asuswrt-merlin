/*
   Unix SMB/CIFS implementation.
   Implement a stack of talloc contexts
   Copyright (C) Volker Lendecke 2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * Implement a stack of talloc frames.
 *
 * When a new talloc stackframe is allocated with talloc_stackframe(), then
 * the TALLOC_CTX returned with talloc_tos() is reset to that new
 * frame. Whenever that stack frame is TALLOC_FREE()'ed, then the reverse
 * happens: The previous talloc_tos() is restored.
 *
 * This API is designed to be robust in the sense that if someone forgets to
 * TALLOC_FREE() a stackframe, then the next outer one correctly cleans up and
 * resets the talloc_tos().
 *
 */

#ifndef _TALLOC_STACK_H
#define _TALLOC_STACK_H

#include <talloc.h>

/*
 * Create a new talloc stack frame.
 *
 * When free'd, it frees all stack frames that were created after this one and
 * not explicitly freed.
 */

TALLOC_CTX *talloc_stackframe(void);
TALLOC_CTX *talloc_stackframe_pool(size_t poolsize);

/*
 * Get us the current top of the talloc stack.
 */

TALLOC_CTX *talloc_tos(void);

#endif

/* suspend.h: suspension stuff
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _ASM_SUSPEND_H
#define _ASM_SUSPEND_H

static inline int arch_prepare_suspend(void)
{
	return 0;
}

#endif /* _ASM_SUSPEND_H */

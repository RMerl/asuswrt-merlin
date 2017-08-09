/*
 * Unix SMB/CIFS implementation.
 * Support for OneFS kernel oplocks
 *
 * Copyright (C) Tim Prouty, 2009
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _OPLOCK_ONEFS_H
#define _OPLOCK_ONEFS_H

#if HAVE_ONEFS

#include <sys/isi_oplock.h>

struct deferred_open_record {
	bool delayed_for_oplocks;
	bool failed; /* added for onefs_oplocks */
	struct file_id id;
};

/*
 * OneFS oplock utility functions
 */
const char *onefs_oplock_str(enum oplock_type onefs_oplock_type);
int onefs_oplock_to_samba_oplock(enum oplock_type onefs_oplock);
enum oplock_type onefs_samba_oplock_to_oplock(int samba_oplock_type);

/*
 * OneFS oplock callback tracking
 */
void destroy_onefs_callback_record(uint64 id);
uint64 onefs_oplock_wait_record(uint16 mid);
void onefs_set_oplock_callback(uint64 id, files_struct *fsp);

#endif /* HAVE_ONEFS */

#endif /* _OPLOCK_ONEFS_H */

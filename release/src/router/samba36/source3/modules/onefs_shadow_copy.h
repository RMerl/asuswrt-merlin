/*
 * Unix SMB/CIFS implementation.
 *
 * OneFS shadow copy implementation that utilizes the file system's native
 * snapshot support.
 *
 * Copyright (C) Dave Richards, 2007
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

#ifndef ONEFS_SHADOW_COPY_H
#define	 ONEFS_SHADOW_COPY_H

void *osc_version_opendir(void);
char *osc_version_readdir(void *vp);
void osc_version_closedir(void *vp);
char *osc_canonicalize_path(const char *path, char *snap_component);

#endif /* ONEFS_SHADOW_COPY_H */

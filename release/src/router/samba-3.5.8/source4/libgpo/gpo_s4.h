
/*
   Samba CIFS implementation
   ADS convenience functions for GPO

   Copyright (C) 2008 Jelmer Vernooij, jelmer@samba.org
   Copyright (C) 2008 Wilco Baan Hofman, wilco@baanhofman.nl

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

#ifndef __LIBGPO_GPO_S4_H__
#define __LIBGPO_GPO_S4_H__

#if _SAMBA_BUILD_ == 4
#include "source4/libcli/libcli.h"
#endif

NTSTATUS gpo_copy_file(TALLOC_CTX *mem_ctx,
		       struct smbcli_state *cli,
		       const char *nt_path,
		       const char *unix_path);

NTSTATUS gpo_sync_directories(TALLOC_CTX *mem_ctx,
			      struct smbcli_state *cli,
			      const char *nt_path,
			      const char *local_path);

#endif

/*
   Unix SMB/CIFS implementation.
   Registry interface
   Copyright (C) Jelmer Vernooij					  2007.

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

#include "includes.h"
#include "lib/registry/common/registry.h"
#include "windows/registry.h"

static WERROR wine_open_reg (struct registry_hive *h, struct registry_key **key)
{
	/* FIXME: Open h->location and mmap it */
}

static REG_OPS reg_backend_wine = {
	.name = "wine",
	.open_hive = wine_open_reg,

};

NTSTATUS registry_wine_init(void)
{
	register_backend("registry", &reg_backend_wine);
	return NT_STATUS_OK;
}

WERROR reg_open_wine(struct registry_key **ctx)
{
	/* FIXME: Open ~/.wine/system.reg, etc */
	return WERR_NOT_SUPPORTED;
}

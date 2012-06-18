/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007
   
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
#include "scripting/python/modules.h"
#include <Python.h>

extern void init_ldb(void);
extern void init_security(void);
extern void init_registry(void);
extern void init_param(void);
extern void init_misc(void);
extern void init_ldb(void);
extern void init_auth(void);
extern void init_credentials(void);
extern void init_tdb(void);
extern void init_dcerpc(void);
extern void init_events(void);
extern void inituuid(void);
extern void init_net(void);
extern void initecho(void);
extern void initdfs(void);
extern void initdrsuapi(void);
extern void initwinreg(void);
extern void initepmapper(void);
extern void initinitshutdown(void);
extern void initmgmt(void);
extern void initnet(void);
extern void initatsvc(void);
extern void initsamr(void);
extern void initlsa(void);
extern void initsvcctl(void);
extern void initwkssvc(void);
extern void initunixinfo(void);
extern void init_libcli_nbt(void);
extern void init_libcli_smb(void);

static struct _inittab py_modules[] = { STATIC_LIBPYTHON_MODULES };

void py_load_samba_modules(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(py_modules); i++) {
		PyImport_ExtendInittab(&py_modules[i]);
	}
}

void py_update_path(const char *bindir)
{
	char *newpath;
	asprintf(&newpath, "%s/python:%s/../scripting/python:%s", bindir, bindir, Py_GetPath());
	PySys_SetPath(newpath);
	free(newpath);
}

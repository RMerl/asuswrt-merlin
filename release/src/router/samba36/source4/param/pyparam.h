/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
   
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

#ifndef _PYPARAM_H_
#define _PYPARAM_H_

#include "param/param.h"

_PUBLIC_ struct loadparm_context *lpcfg_from_py_object(TALLOC_CTX *mem_ctx, PyObject *py_obj);
_PUBLIC_ struct loadparm_context *py_default_loadparm_context(TALLOC_CTX *mem_ctx);

#endif /* _PYPARAM_H_ */

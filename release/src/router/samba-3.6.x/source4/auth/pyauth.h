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

#ifndef _PYAUTH_H_
#define _PYAUTH_H_

#include <pytalloc.h>
#include "auth/session.h"

#define PyAuthSession_AsSession(obj) py_talloc_get_type(obj, struct auth_session_info)
#define PyAuthSession_Check(obj) PyObject_TypeCheck(obj, &PyAuthSession)
struct auth_session_info *PyObject_AsSession(PyObject *obj);
PyObject *PyAuthSession_FromSession(struct auth_session_info *session);

#endif /* _PYAUTH_H */

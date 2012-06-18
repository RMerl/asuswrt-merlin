/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2006

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

#include "librpc/gen_ndr/security.h"

enum security_user_level {
	SECURITY_ANONYMOUS,
	SECURITY_USER,
	SECURITY_DOMAIN_CONTROLLER,
	SECURITY_ADMINISTRATOR,
	SECURITY_SYSTEM
};

struct auth_session_info;

struct object_tree {
	uint32_t remaining_access;
	struct GUID guid;
	/* linked list of children */
	struct object_tree * children;
	struct object_tree * prev;
	struct object_tree * next;
};

/* Moved the dom_sid functions to the top level dir with manual proto header */
#include "libcli/security/dom_sid.h"
#include "libcli/security/secace.h"
#include "libcli/security/secacl.h"
#include "libcli/security/proto.h"
#include "libcli/security/security_descriptor.h"

/* 
   Unix SMB/CIFS implementation.

   Winbind daemon for ntdom nss module

   Copyright (C) Tim Potter 2000
   Copyright (C) Jeremy Allison 2001.
   Copyright (C) Gerald (Jerry) Carter 2003.
   Copyright (C) Volker Lendecke 2005
   
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
#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

/* Fill a grent structure from various other information */

bool fill_grent(TALLOC_CTX *mem_ctx, struct winbindd_gr *gr,
		const char *dom_name, const char *gr_name, gid_t unix_gid)
{
	fstring full_group_name;
	char *mapped_name = NULL;
	struct winbindd_domain *domain;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;

	domain = find_domain_from_name_noinit(dom_name);
	if (domain == NULL) {
		DEBUG(0, ("Failed to find domain '%s'. "
			  "Check connection to trusted domains!\n",
			  dom_name));
		return false;
	}

	nt_status = normalize_name_map(mem_ctx, domain, gr_name,
				       &mapped_name);

	/* Basic whitespace replacement */
	if (NT_STATUS_IS_OK(nt_status)) {
		fill_domain_username(full_group_name, dom_name,
				     mapped_name, true);
	}
	/* Mapped to an aliase */
	else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_FILE_RENAMED)) {
		fstrcpy(full_group_name, mapped_name);
	}
	/* no change */
	else {
		fill_domain_username( full_group_name, dom_name,
				      gr_name, True );
	}

	gr->gr_gid = unix_gid;

	/* Group name and password */

	safe_strcpy(gr->gr_name, full_group_name, sizeof(gr->gr_name) - 1);
	safe_strcpy(gr->gr_passwd, "x", sizeof(gr->gr_passwd) - 1);

	return True;
}

struct getgr_countmem {
	int num;
	size_t len;
};

static int getgr_calc_memberlen(DATA_BLOB key, void *data, void *priv)
{
	struct wbint_Principal *m = talloc_get_type_abort(
		data, struct wbint_Principal);
	struct getgr_countmem *buf = (struct getgr_countmem *)priv;

	buf->num += 1;
	buf->len += strlen(m->name) + 1;
	return 0;
}

struct getgr_stringmem {
	size_t ofs;
	char *buf;
};

static int getgr_unparse_members(DATA_BLOB key, void *data, void *priv)
{
	struct wbint_Principal *m = talloc_get_type_abort(
		data, struct wbint_Principal);
	struct getgr_stringmem *buf = (struct getgr_stringmem *)priv;
	int len;

	len = strlen(m->name);

	memcpy(buf->buf + buf->ofs, m->name, len);
	buf->ofs += len;
	buf->buf[buf->ofs] = ',';
	buf->ofs += 1;
	return 0;
}

NTSTATUS winbindd_print_groupmembers(struct talloc_dict *members,
				     TALLOC_CTX *mem_ctx,
				     int *num_members, char **result)
{
	struct getgr_countmem c;
	struct getgr_stringmem m;
	int res;

	c.num = 0;
	c.len = 0;

	res = talloc_dict_traverse(members, getgr_calc_memberlen, &c);
	if (res == -1) {
		DEBUG(5, ("talloc_dict_traverse failed\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	m.ofs = 0;
	m.buf = talloc_array(mem_ctx, char, c.len);
	if (m.buf == NULL) {
		DEBUG(5, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	res = talloc_dict_traverse(members, getgr_unparse_members, &m);
	if (res == -1) {
		DEBUG(5, ("talloc_dict_traverse failed\n"));
		TALLOC_FREE(m.buf);
		return NT_STATUS_INTERNAL_ERROR;
	}
	m.buf[c.len-1] = '\0';

	*num_members = c.num;
	*result = m.buf;
	return NT_STATUS_OK;
}

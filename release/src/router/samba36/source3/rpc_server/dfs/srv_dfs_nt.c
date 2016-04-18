/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines for Dfs
 *  Copyright (C) Shirish Kalele	2000.
 *  Copyright (C) Jeremy Allison	2001-2007.
 *  Copyright (C) Jelmer Vernooij	2005-2006.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* This is the implementation of the dfs pipe. */

#include "includes.h"
#include "ntdomain.h"
#include "../librpc/gen_ndr/srv_dfs.h"
#include "msdfs.h"
#include "smbd/smbd.h"
#include "auth.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_MSDFS

/* This function does not return a WERROR or NTSTATUS code but rather 1 if
   dfs exists, or 0 otherwise. */

void _dfs_GetManagerVersion(struct pipes_struct *p, struct dfs_GetManagerVersion *r)
{
	if (lp_host_msdfs()) {
		*r->out.version = DFS_MANAGER_VERSION_NT4;
	} else {
		*r->out.version = (enum dfs_ManagerVersion)0;
	}
}

WERROR _dfs_Add(struct pipes_struct *p, struct dfs_Add *r)
{
	struct junction_map *jn = NULL;
	struct referral *old_referral_list = NULL;
	bool self_ref = False;
	int consumedcnt = 0;
	char *altpath = NULL;
	NTSTATUS status;
	TALLOC_CTX *ctx = talloc_tos();

	if (p->session_info->utok.uid != sec_initial_uid()) {
		DEBUG(10,("_dfs_add: uid != 0. Access denied.\n"));
		return WERR_ACCESS_DENIED;
	}

	jn = TALLOC_ZERO_P(ctx, struct junction_map);
	if (!jn) {
		return WERR_NOMEM;
	}

	DEBUG(5,("init_reply_dfs_add: Request to add %s -> %s\\%s.\n",
		r->in.path, r->in.server, r->in.share));

	altpath = talloc_asprintf(ctx, "%s\\%s",
			r->in.server,
			r->in.share);
	if (!altpath) {
		return WERR_NOMEM;
	}

	/* The following call can change the cwd. */
	status = get_referred_path(ctx, r->in.path, jn,
			&consumedcnt, &self_ref);
	if(!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	jn->referral_count += 1;
	old_referral_list = jn->referral_list;

	if (jn->referral_count < 1) {
		return WERR_NOMEM;
	}

	jn->referral_list = TALLOC_ARRAY(ctx, struct referral, jn->referral_count);
	if(jn->referral_list == NULL) {
		DEBUG(0,("init_reply_dfs_add: talloc failed for referral list!\n"));
		return WERR_DFS_INTERNAL_ERROR;
	}

	if(old_referral_list && jn->referral_list) {
		memcpy(jn->referral_list, old_referral_list,
				sizeof(struct referral)*jn->referral_count-1);
	}

	jn->referral_list[jn->referral_count-1].proximity = 0;
	jn->referral_list[jn->referral_count-1].ttl = REFERRAL_TTL;
	jn->referral_list[jn->referral_count-1].alternate_path = altpath;

	if(!create_msdfs_link(jn)) {
		return WERR_DFS_CANT_CREATE_JUNCT;
	}

	return WERR_OK;
}

WERROR _dfs_Remove(struct pipes_struct *p, struct dfs_Remove *r)
{
	struct junction_map *jn = NULL;
	bool self_ref = False;
	int consumedcnt = 0;
	bool found = False;
	TALLOC_CTX *ctx = talloc_tos();
	char *altpath = NULL;

	if (p->session_info->utok.uid != sec_initial_uid()) {
		DEBUG(10,("_dfs_remove: uid != 0. Access denied.\n"));
		return WERR_ACCESS_DENIED;
	}

	jn = TALLOC_ZERO_P(ctx, struct junction_map);
	if (!jn) {
		return WERR_NOMEM;
	}

	if (r->in.servername && r->in.sharename) {
		altpath = talloc_asprintf(ctx, "%s\\%s",
			r->in.servername,
			r->in.sharename);
		if (!altpath) {
			return WERR_NOMEM;
		}
		strlower_m(altpath);
		DEBUG(5,("init_reply_dfs_remove: Request to remove %s -> %s\\%s.\n",
			r->in.dfs_entry_path, r->in.servername, r->in.sharename));
	}

	if(!NT_STATUS_IS_OK(get_referred_path(ctx, r->in.dfs_entry_path, jn,
				&consumedcnt, &self_ref))) {
		return WERR_DFS_NO_SUCH_VOL;
	}

	/* if no server-share pair given, remove the msdfs link completely */
	if(!r->in.servername && !r->in.sharename) {
		if(!remove_msdfs_link(jn)) {
			return WERR_DFS_NO_SUCH_VOL;
		}
	} else {
		int i=0;
		/* compare each referral in the list with the one to remove */
		DEBUG(10,("altpath: .%s. refcnt: %d\n", altpath, jn->referral_count));
		for(i=0;i<jn->referral_count;i++) {
			char *refpath = talloc_strdup(ctx,
					jn->referral_list[i].alternate_path);
			if (!refpath) {
				return WERR_NOMEM;
			}
			trim_char(refpath, '\\', '\\');
			DEBUG(10,("_dfs_remove:  refpath: .%s.\n", refpath));
			if(strequal(refpath, altpath)) {
				*(jn->referral_list[i].alternate_path)='\0';
				DEBUG(10,("_dfs_remove: Removal request matches referral %s\n",
					refpath));
				found = True;
			}
		}

		if(!found) {
			return WERR_DFS_NO_SUCH_SHARE;
		}

		/* Only one referral, remove it */
		if(jn->referral_count == 1) {
			if(!remove_msdfs_link(jn)) {
				return WERR_DFS_NO_SUCH_VOL;
			}
		} else {
			if(!create_msdfs_link(jn)) {
				return WERR_DFS_CANT_CREATE_JUNCT;
			}
		}
	}

	return WERR_OK;
}

static bool init_reply_dfs_info_1(TALLOC_CTX *mem_ctx, struct junction_map* j,struct dfs_Info1* dfs1)
{
	dfs1->path = talloc_asprintf(mem_ctx,
				"\\\\%s\\%s\\%s", global_myname(),
				j->service_name, j->volume_name);
	if (dfs1->path == NULL)
		return False;

	DEBUG(5,("init_reply_dfs_info_1: initing entrypath: %s\n",dfs1->path));
	return True;
}

static bool init_reply_dfs_info_2(TALLOC_CTX *mem_ctx, struct junction_map* j, struct dfs_Info2* dfs2)
{
	dfs2->path = talloc_asprintf(mem_ctx,
			"\\\\%s\\%s\\%s", global_myname(), j->service_name, j->volume_name);
	if (dfs2->path == NULL)
		return False;
	dfs2->comment = talloc_strdup(mem_ctx, j->comment);
	dfs2->state = 1; /* set up state of dfs junction as OK */
	dfs2->num_stores = j->referral_count;
	return True;
}

static bool init_reply_dfs_info_3(TALLOC_CTX *mem_ctx, struct junction_map* j, struct dfs_Info3* dfs3)
{
	int ii;
	if (j->volume_name[0] == '\0')
		dfs3->path = talloc_asprintf(mem_ctx, "\\\\%s\\%s",
			global_myname(), j->service_name);
	else
		dfs3->path = talloc_asprintf(mem_ctx, "\\\\%s\\%s\\%s", global_myname(),
			j->service_name, j->volume_name);

	if (dfs3->path == NULL)
		return False;

	dfs3->comment = talloc_strdup(mem_ctx, j->comment);
	dfs3->state = 1;
	dfs3->num_stores = j->referral_count;

	/* also enumerate the stores */
	if (j->referral_count) {
		dfs3->stores = TALLOC_ARRAY(mem_ctx, struct dfs_StorageInfo, j->referral_count);
		if (!dfs3->stores)
			return False;
		memset(dfs3->stores, '\0', j->referral_count * sizeof(struct dfs_StorageInfo));
	} else {
		dfs3->stores = NULL;
	}

	for(ii=0;ii<j->referral_count;ii++) {
		char* p;
		char *path = NULL;
		struct dfs_StorageInfo* stor = &(dfs3->stores[ii]);
		struct referral* ref = &(j->referral_list[ii]);

		path = talloc_strdup(mem_ctx, ref->alternate_path);
		if (!path) {
			return False;
		}
		trim_char(path,'\\','\0');
		p = strrchr_m(path,'\\');
		if(p==NULL) {
			DEBUG(4,("init_reply_dfs_info_3: invalid path: no \\ found in %s\n",path));
			continue;
		}
		*p = '\0';
		DEBUG(5,("storage %d: %s.%s\n",ii,path,p+1));
		stor->state = 2; /* set all stores as ONLINE */
		stor->server = talloc_strdup(mem_ctx, path);
		stor->share = talloc_strdup(mem_ctx, p+1);
	}
	return True;
}

static bool init_reply_dfs_info_100(TALLOC_CTX *mem_ctx, struct junction_map* j, struct dfs_Info100* dfs100)
{
	dfs100->comment = talloc_strdup(mem_ctx, j->comment);
	return True;
}

WERROR _dfs_Enum(struct pipes_struct *p, struct dfs_Enum *r)
{
	struct junction_map *jn = NULL;
	size_t num_jn = 0;
	size_t i;
	TALLOC_CTX *ctx = talloc_tos();

	jn = enum_msdfs_links(ctx, &num_jn);
	if (!jn || num_jn == 0) {
		num_jn = 0;
		jn = NULL;
	}

	DEBUG(5,("_dfs_Enum: %u junctions found in Dfs, doing level %d\n",
				(unsigned int)num_jn, r->in.level));

	*r->out.total = num_jn;

	/* Create the return array */
	switch (r->in.level) {
	case 1:
		if (num_jn) {
			if ((r->out.info->e.info1->s = TALLOC_ARRAY(ctx, struct dfs_Info1, num_jn)) == NULL) {
				return WERR_NOMEM;
			}
		} else {
			r->out.info->e.info1->s = NULL;
		}
		r->out.info->e.info1->count = num_jn;
		break;
	case 2:
		if (num_jn) {
			if ((r->out.info->e.info2->s = TALLOC_ARRAY(ctx, struct dfs_Info2, num_jn)) == NULL) {
				return WERR_NOMEM;
			}
		} else {
			r->out.info->e.info2->s = NULL;
		}
		r->out.info->e.info2->count = num_jn;
		break;
	case 3:
		if (num_jn) {
			if ((r->out.info->e.info3->s = TALLOC_ARRAY(ctx, struct dfs_Info3, num_jn)) == NULL) {
				return WERR_NOMEM;
			}
		} else {
			r->out.info->e.info3->s = NULL;
		}
		r->out.info->e.info3->count = num_jn;
		break;
	default:
		return WERR_INVALID_PARAM;
	}

	for (i = 0; i < num_jn; i++) {
		switch (r->in.level) {
		case 1:
			init_reply_dfs_info_1(ctx, &jn[i], &r->out.info->e.info1->s[i]);
			break;
		case 2:
			init_reply_dfs_info_2(ctx, &jn[i], &r->out.info->e.info2->s[i]);
			break;
		case 3:
			init_reply_dfs_info_3(ctx, &jn[i], &r->out.info->e.info3->s[i]);
			break;
		default:
			return WERR_INVALID_PARAM;
		}
	}

	return WERR_OK;
}

WERROR _dfs_GetInfo(struct pipes_struct *p, struct dfs_GetInfo *r)
{
	int consumedcnt = strlen(r->in.dfs_entry_path);
	struct junction_map *jn = NULL;
	bool self_ref = False;
	TALLOC_CTX *ctx = talloc_tos();
	bool ret;

	jn = TALLOC_ZERO_P(ctx, struct junction_map);
	if (!jn) {
		return WERR_NOMEM;
	}

	if(!create_junction(ctx, r->in.dfs_entry_path, jn)) {
		return WERR_DFS_NO_SUCH_SERVER;
	}

	/* The following call can change the cwd. */
	if(!NT_STATUS_IS_OK(get_referred_path(ctx, r->in.dfs_entry_path,
					jn, &consumedcnt, &self_ref)) ||
			consumedcnt < strlen(r->in.dfs_entry_path)) {
		return WERR_DFS_NO_SUCH_VOL;
	}

	switch (r->in.level) {
		case 1:
			r->out.info->info1 = TALLOC_ZERO_P(ctx,struct dfs_Info1);
			if (!r->out.info->info1) {
				return WERR_NOMEM;
			}
			ret = init_reply_dfs_info_1(ctx, jn, r->out.info->info1);
			break;
		case 2:
			r->out.info->info2 = TALLOC_ZERO_P(ctx,struct dfs_Info2);
			if (!r->out.info->info2) {
				return WERR_NOMEM;
			}
			ret = init_reply_dfs_info_2(ctx, jn, r->out.info->info2);
			break;
		case 3:
			r->out.info->info3 = TALLOC_ZERO_P(ctx,struct dfs_Info3);
			if (!r->out.info->info3) {
				return WERR_NOMEM;
			}
			ret = init_reply_dfs_info_3(ctx, jn, r->out.info->info3);
			break;
		case 100:
			r->out.info->info100 = TALLOC_ZERO_P(ctx,struct dfs_Info100);
			if (!r->out.info->info100) {
				return WERR_NOMEM;
			}
			ret = init_reply_dfs_info_100(ctx, jn, r->out.info->info100);
			break;
		default:
			r->out.info->info1 = NULL;
			return WERR_INVALID_PARAM;
	}

	if (!ret)
		return WERR_INVALID_PARAM;

	return WERR_OK;
}

WERROR _dfs_SetInfo(struct pipes_struct *p, struct dfs_SetInfo *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_Rename(struct pipes_struct *p, struct dfs_Rename *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_Move(struct pipes_struct *p, struct dfs_Move *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_ManagerGetConfigInfo(struct pipes_struct *p, struct dfs_ManagerGetConfigInfo *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_ManagerSendSiteInfo(struct pipes_struct *p, struct dfs_ManagerSendSiteInfo *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_AddFtRoot(struct pipes_struct *p, struct dfs_AddFtRoot *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_RemoveFtRoot(struct pipes_struct *p, struct dfs_RemoveFtRoot *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_AddStdRoot(struct pipes_struct *p, struct dfs_AddStdRoot *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_RemoveStdRoot(struct pipes_struct *p, struct dfs_RemoveStdRoot *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_ManagerInitialize(struct pipes_struct *p, struct dfs_ManagerInitialize *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_AddStdRootForced(struct pipes_struct *p, struct dfs_AddStdRootForced *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_GetDcAddress(struct pipes_struct *p, struct dfs_GetDcAddress *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_SetDcAddress(struct pipes_struct *p, struct dfs_SetDcAddress *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_FlushFtTable(struct pipes_struct *p, struct dfs_FlushFtTable *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_Add2(struct pipes_struct *p, struct dfs_Add2 *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_Remove2(struct pipes_struct *p, struct dfs_Remove2 *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_EnumEx(struct pipes_struct *p, struct dfs_EnumEx *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _dfs_SetInfo2(struct pipes_struct *p, struct dfs_SetInfo2 *r)
{
	/* FIXME: Implement your code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

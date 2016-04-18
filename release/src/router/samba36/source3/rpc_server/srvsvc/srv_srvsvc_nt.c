/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Jeremy Allison               2001.
 *  Copyright (C) Nigel Williams               2001.
 *  Copyright (C) Gerald (Jerry) Carter        2006.
 *  Copyright (C) Guenther Deschner            2008.
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

/* This is the implementation of the srvsvc pipe. */

#include "includes.h"
#include "system/passwd.h"
#include "ntdomain.h"
#include "../librpc/gen_ndr/srv_srvsvc.h"
#include "../libcli/security/security.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "dbwrap.h"
#include "session.h"
#include "../lib/util/util_pw.h"
#include "smbd/smbd.h"
#include "auth.h"
#include "messages.h"

extern const struct generic_mapping file_generic_mapping;

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

#define MAX_SERVER_DISK_ENTRIES 15

/* Use for enumerating connections, pipes, & files */

struct file_enum_count {
	TALLOC_CTX *ctx;
	const char *username;
	struct srvsvc_NetFileCtr3 *ctr3;
};

struct sess_file_count {
	struct server_id pid;
	uid_t uid;
	int count;
};

/* Used to store pipe open records for NetFileEnum() */

struct pipe_open_rec {
	struct server_id pid;
	uid_t uid;
	int pnum;
	fstring name;
};

/****************************************************************************
 Count the entries belonging to a service in the connection db.
****************************************************************************/

static int pipe_enum_fn( struct db_record *rec, void *p)
{
	struct pipe_open_rec prec;
	struct file_enum_count *fenum = (struct file_enum_count *)p;
	struct srvsvc_NetFileInfo3 *f;
	int i = fenum->ctr3->count;
	char *fullpath = NULL;
	const char *username;

	if (rec->value.dsize != sizeof(struct pipe_open_rec))
		return 0;

	memcpy(&prec, rec->value.dptr, sizeof(struct pipe_open_rec));

	if ( !process_exists(prec.pid) ) {
		return 0;
	}

	username = uidtoname(prec.uid);

	if ((fenum->username != NULL)
	    && !strequal(username, fenum->username)) {
		return 0;
	}

	fullpath = talloc_asprintf(fenum->ctx, "\\PIPE\\%s", prec.name );
	if (!fullpath) {
		return 1;
	}

	f = TALLOC_REALLOC_ARRAY(fenum->ctx, fenum->ctr3->array,
				 struct srvsvc_NetFileInfo3, i+1);
	if ( !f ) {
		DEBUG(0,("conn_enum_fn: realloc failed for %d items\n", i+1));
		return 1;
	}
	fenum->ctr3->array = f;

	fenum->ctr3->array[i].fid		=
		(((uint32_t)(procid_to_pid(&prec.pid))<<16) | prec.pnum);
	fenum->ctr3->array[i].permissions	=
		(FILE_READ_DATA|FILE_WRITE_DATA);
	fenum->ctr3->array[i].num_locks		= 0;
	fenum->ctr3->array[i].path		= fullpath;
	fenum->ctr3->array[i].user		= username;

	fenum->ctr3->count++;

	return 0;
}

/*******************************************************************
********************************************************************/

static WERROR net_enum_pipes(TALLOC_CTX *ctx,
			     const char *username,
			     struct srvsvc_NetFileCtr3 **ctr3,
			     uint32_t resume )
{
	struct file_enum_count fenum;

	fenum.ctx = ctx;
	fenum.username = username;
	fenum.ctr3 = *ctr3;

	if (connections_traverse(pipe_enum_fn, &fenum) == -1) {
		DEBUG(0,("net_enum_pipes: traverse of connections.tdb "
			 "failed\n"));
		return WERR_NOMEM;
	}

	*ctr3 = fenum.ctr3;

	return WERR_OK;
}

/*******************************************************************
********************************************************************/

static void enum_file_fn( const struct share_mode_entry *e,
                          const char *sharepath, const char *fname,
			  void *private_data )
{
	struct file_enum_count *fenum =
		(struct file_enum_count *)private_data;

	struct srvsvc_NetFileInfo3 *f;
	int i = fenum->ctr3->count;
	files_struct fsp;
	struct byte_range_lock *brl;
	int num_locks = 0;
	char *fullpath = NULL;
	uint32 permissions;
	const char *username;

	/* If the pid was not found delete the entry from connections.tdb */

	if ( !process_exists(e->pid) ) {
		return;
	}

	username = uidtoname(e->uid);

	if ((fenum->username != NULL)
	    && !strequal(username, fenum->username)) {
		return;
	}

	f = TALLOC_REALLOC_ARRAY(fenum->ctx, fenum->ctr3->array,
				 struct srvsvc_NetFileInfo3, i+1);
	if ( !f ) {
		DEBUG(0,("conn_enum_fn: realloc failed for %d items\n", i+1));
		return;
	}
	fenum->ctr3->array = f;

	/* need to count the number of locks on a file */

	ZERO_STRUCT( fsp );
	fsp.file_id = e->id;

	if ( (brl = brl_get_locks(talloc_tos(), &fsp)) != NULL ) {
		num_locks = brl->num_locks;
		TALLOC_FREE(brl);
	}

	if ( strcmp( fname, "." ) == 0 ) {
		fullpath = talloc_asprintf(fenum->ctx, "C:%s", sharepath );
	} else {
		fullpath = talloc_asprintf(fenum->ctx, "C:%s/%s",
				sharepath, fname );
	}
	if (!fullpath) {
		return;
	}
	string_replace( fullpath, '/', '\\' );

	/* mask out create (what ever that is) */
	permissions = e->access_mask & (FILE_READ_DATA|FILE_WRITE_DATA);

	/* now fill in the srvsvc_NetFileInfo3 struct */

	fenum->ctr3->array[i].fid		=
		(((uint32_t)(procid_to_pid(&e->pid))<<16) | e->share_file_id);
	fenum->ctr3->array[i].permissions	= permissions;
	fenum->ctr3->array[i].num_locks		= num_locks;
	fenum->ctr3->array[i].path		= fullpath;
	fenum->ctr3->array[i].user		= username;

	fenum->ctr3->count++;
}

/*******************************************************************
********************************************************************/

static WERROR net_enum_files(TALLOC_CTX *ctx,
			     const char *username,
			     struct srvsvc_NetFileCtr3 **ctr3,
			     uint32_t resume)
{
	struct file_enum_count f_enum_cnt;

	f_enum_cnt.ctx = ctx;
	f_enum_cnt.username = username;
	f_enum_cnt.ctr3 = *ctr3;

	share_mode_forall( enum_file_fn, (void *)&f_enum_cnt );

	*ctr3 = f_enum_cnt.ctr3;

	return WERR_OK;
}

/*******************************************************************
 Utility function to get the 'type' of a share from an snum.
 ********************************************************************/
static uint32 get_share_type(int snum)
{
	/* work out the share type */
	uint32 type = STYPE_DISKTREE;

	if (lp_print_ok(snum))
		type = STYPE_PRINTQ;
	if (strequal(lp_fstype(snum), "IPC"))
		type = STYPE_IPC;
	if (lp_administrative_share(snum))
		type |= STYPE_HIDDEN;

	return type;
}

/*******************************************************************
 Fill in a share info level 0 structure.
 ********************************************************************/

static void init_srv_share_info_0(struct pipes_struct *p,
				  struct srvsvc_NetShareInfo0 *r, int snum)
{
	r->name		= lp_servicename(snum);
}

/*******************************************************************
 Fill in a share info level 1 structure.
 ********************************************************************/

static void init_srv_share_info_1(struct pipes_struct *p,
				  struct srvsvc_NetShareInfo1 *r,
				  int snum)
{
	char *net_name = lp_servicename(snum);
	char *remark = talloc_strdup(p->mem_ctx, lp_comment(snum));

	if (remark) {
		remark = talloc_sub_advanced(
			p->mem_ctx, lp_servicename(snum),
			get_current_username(), lp_pathname(snum),
			p->session_info->utok.uid, get_current_username(),
			"", remark);
	}

	r->name		= net_name;
	r->type		= get_share_type(snum);
	r->comment	= remark ? remark : "";
}

/*******************************************************************
 Fill in a share info level 2 structure.
 ********************************************************************/

static void init_srv_share_info_2(struct pipes_struct *p,
				  struct srvsvc_NetShareInfo2 *r,
				  int snum)
{
	char *remark = NULL;
	char *path = NULL;
	int max_connections = lp_max_connections(snum);
	uint32_t max_uses = max_connections!=0 ? max_connections : (uint32_t)-1;
	char *net_name = lp_servicename(snum);

	remark = talloc_strdup(p->mem_ctx, lp_comment(snum));
	if (remark) {
		remark = talloc_sub_advanced(
			p->mem_ctx, lp_servicename(snum),
			get_current_username(), lp_pathname(snum),
			p->session_info->utok.uid, get_current_username(),
			"", remark);
	}
	path = talloc_asprintf(p->mem_ctx,
			"C:%s", lp_pathname(snum));

	if (path) {
		/*
		 * Change / to \\ so that win2k will see it as a valid path.
		 * This was added to enable use of browsing in win2k add
		 * share dialog.
		 */

		string_replace(path, '/', '\\');
	}

	r->name			= net_name;
	r->type			= get_share_type(snum);
	r->comment		= remark ? remark : "";
	r->permissions		= 0;
	r->max_users		= max_uses;
	r->current_users	= count_current_connections(net_name, false);
	r->path			= path ? path : "";
	r->password		= "";
}

/*******************************************************************
 Map any generic bits to file specific bits.
********************************************************************/

static void map_generic_share_sd_bits(struct security_descriptor *psd)
{
	int i;
	struct security_acl *ps_dacl = NULL;

	if (!psd)
		return;

	ps_dacl = psd->dacl;
	if (!ps_dacl)
		return;

	for (i = 0; i < ps_dacl->num_aces; i++) {
		struct security_ace *psa = &ps_dacl->aces[i];
		uint32 orig_mask = psa->access_mask;

		se_map_generic(&psa->access_mask, &file_generic_mapping);
		psa->access_mask |= orig_mask;
	}
}

/*******************************************************************
 Fill in a share info level 501 structure.
********************************************************************/

static void init_srv_share_info_501(struct pipes_struct *p,
				    struct srvsvc_NetShareInfo501 *r, int snum)
{
	const char *net_name = lp_servicename(snum);
	char *remark = talloc_strdup(p->mem_ctx, lp_comment(snum));

	if (remark) {
		remark = talloc_sub_advanced(
			p->mem_ctx, lp_servicename(snum),
			get_current_username(), lp_pathname(snum),
			p->session_info->utok.uid, get_current_username(),
			"", remark);
	}

	r->name		= net_name;
	r->type		= get_share_type(snum);
	r->comment	= remark ? remark : "";
	r->csc_policy	= (lp_csc_policy(snum) << 4);
}

/*******************************************************************
 Fill in a share info level 502 structure.
 ********************************************************************/

static void init_srv_share_info_502(struct pipes_struct *p,
				    struct srvsvc_NetShareInfo502 *r, int snum)
{
	const char *net_name = lp_servicename(snum);
	char *path = NULL;
	struct security_descriptor *sd = NULL;
	struct sec_desc_buf *sd_buf = NULL;
	size_t sd_size = 0;
	TALLOC_CTX *ctx = p->mem_ctx;
	char *remark = talloc_strdup(ctx, lp_comment(snum));

	if (remark) {
		remark = talloc_sub_advanced(
			p->mem_ctx, lp_servicename(snum),
			get_current_username(), lp_pathname(snum),
			p->session_info->utok.uid, get_current_username(),
			"", remark);
	}
	path = talloc_asprintf(ctx, "C:%s", lp_pathname(snum));
	if (path) {
		/*
		 * Change / to \\ so that win2k will see it as a valid path.  This was added to
		 * enable use of browsing in win2k add share dialog.
		 */
		string_replace(path, '/', '\\');
	}

	sd = get_share_security(ctx, lp_servicename(snum), &sd_size);

	sd_buf = make_sec_desc_buf(p->mem_ctx, sd_size, sd);

	r->name			= net_name;
	r->type			= get_share_type(snum);
	r->comment		= remark ? remark : "";
	r->permissions		= 0;
	r->max_users		= (uint32_t)-1;
	r->current_users	= 1; /* ??? */
	r->path			= path ? path : "";
	r->password		= "";
	r->sd_buf		= *sd_buf;
}

/***************************************************************************
 Fill in a share info level 1004 structure.
 ***************************************************************************/

static void init_srv_share_info_1004(struct pipes_struct *p,
				     struct srvsvc_NetShareInfo1004 *r,
				     int snum)
{
	char *remark = talloc_strdup(p->mem_ctx, lp_comment(snum));

	if (remark) {
		remark = talloc_sub_advanced(
			p->mem_ctx, lp_servicename(snum),
			get_current_username(), lp_pathname(snum),
			p->session_info->utok.uid, get_current_username(),
			"", remark);
	}

	r->comment	= remark ? remark : "";
}

/***************************************************************************
 Fill in a share info level 1005 structure.
 ***************************************************************************/

static void init_srv_share_info_1005(struct pipes_struct *p,
				     struct srvsvc_NetShareInfo1005 *r,
				     int snum)
{
	uint32_t dfs_flags = 0;

	if (lp_host_msdfs() && lp_msdfs_root(snum)) {
		dfs_flags |= SHARE_1005_IN_DFS | SHARE_1005_DFS_ROOT;
	}

	dfs_flags |= lp_csc_policy(snum) << SHARE_1005_CSC_POLICY_SHIFT;

	r->dfs_flags	= dfs_flags;
}

/***************************************************************************
 Fill in a share info level 1006 structure.
 ***************************************************************************/

static void init_srv_share_info_1006(struct pipes_struct *p,
				     struct srvsvc_NetShareInfo1006 *r,
				     int snum)
{
	r->max_users	= (uint32_t)-1;
}

/***************************************************************************
 Fill in a share info level 1007 structure.
 ***************************************************************************/

static void init_srv_share_info_1007(struct pipes_struct *p,
				     struct srvsvc_NetShareInfo1007 *r,
				     int snum)
{
	r->flags			= 0;
	r->alternate_directory_name	= "";
}

/*******************************************************************
 Fill in a share info level 1501 structure.
 ********************************************************************/

static void init_srv_share_info_1501(struct pipes_struct *p,
				     struct sec_desc_buf **r,
				     int snum)
{
	struct security_descriptor *sd;
	struct sec_desc_buf *sd_buf = NULL;
	size_t sd_size;
	TALLOC_CTX *ctx = p->mem_ctx;

	sd = get_share_security(ctx, lp_servicename(snum), &sd_size);
	if (sd) {
		sd_buf = make_sec_desc_buf(p->mem_ctx, sd_size, sd);
	}

	*r = sd_buf;
}

/*******************************************************************
 True if it ends in '$'.
 ********************************************************************/

static bool is_hidden_share(int snum)
{
	const char *net_name = lp_servicename(snum);

	return (net_name[strlen(net_name) - 1] == '$') ? True : False;
}

/*******************************************************************
 Verify user is allowed to view share, access based enumeration
********************************************************************/
static bool is_enumeration_allowed(struct pipes_struct *p,
                                   int snum)
{
    if (!lp_access_based_share_enum(snum))
        return true;

    return share_access_check(p->session_info->security_token,
			      lp_servicename(snum), FILE_READ_DATA, NULL);
}

/*******************************************************************
 Fill in a share info structure.
 ********************************************************************/

static WERROR init_srv_share_info_ctr(struct pipes_struct *p,
				      struct srvsvc_NetShareInfoCtr *info_ctr,
				      uint32_t *resume_handle_p,
				      uint32_t *total_entries,
				      bool all_shares)
{
	int num_entries = 0;
	int alloc_entries = 0;
	int num_services = 0;
	int snum;
	TALLOC_CTX *ctx = p->mem_ctx;
	int i = 0;
	int valid_share_count = 0;
	bool *allowed = 0;
	union srvsvc_NetShareCtr ctr;
	uint32_t resume_handle = resume_handle_p ? *resume_handle_p : 0;

	DEBUG(5,("init_srv_share_info_ctr\n"));

	/* Ensure all the usershares are loaded. */
	become_root();
	load_usershare_shares();
	load_registry_shares();
	num_services = lp_numservices();
	unbecome_root();

        allowed = TALLOC_ZERO_ARRAY(ctx, bool, num_services);
        W_ERROR_HAVE_NO_MEMORY(allowed);

        /* Count the number of entries. */
        for (snum = 0; snum < num_services; snum++) {
                if (lp_browseable(snum) && lp_snum_ok(snum) &&
                    is_enumeration_allowed(p, snum) &&
                    (all_shares || !is_hidden_share(snum)) ) {
                        DEBUG(10, ("counting service %s\n",
				lp_servicename(snum) ? lp_servicename(snum) : "(null)"));
                        allowed[snum] = true;
                        num_entries++;
                } else {
                        DEBUG(10, ("NOT counting service %s\n",
				lp_servicename(snum) ? lp_servicename(snum) : "(null)"));
                }
        }

	if (!num_entries || (resume_handle >= num_entries)) {
		return WERR_OK;
	}

	/* Calculate alloc entries. */
	alloc_entries = num_entries - resume_handle;
	switch (info_ctr->level) {
	case 0:
		ctr.ctr0 = TALLOC_ZERO_P(ctx, struct srvsvc_NetShareCtr0);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr0);

		ctr.ctr0->count = alloc_entries;
		ctr.ctr0->array = TALLOC_ZERO_ARRAY(ctx, struct srvsvc_NetShareInfo0, alloc_entries);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr0->array);

		for (snum = 0; snum < num_services; snum++) {
			if (allowed[snum] &&
			    (resume_handle <= (i + valid_share_count++)) ) {
				init_srv_share_info_0(p, &ctr.ctr0->array[i++], snum);
			}
		}

		break;

	case 1:
		ctr.ctr1 = TALLOC_ZERO_P(ctx, struct srvsvc_NetShareCtr1);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr1);

		ctr.ctr1->count = alloc_entries;
		ctr.ctr1->array = TALLOC_ZERO_ARRAY(ctx, struct srvsvc_NetShareInfo1, alloc_entries);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr1->array);

		for (snum = 0; snum < num_services; snum++) {
			if (allowed[snum] &&
			    (resume_handle <= (i + valid_share_count++)) ) {
				init_srv_share_info_1(p, &ctr.ctr1->array[i++], snum);
			}
		}

		break;

	case 2:
		ctr.ctr2 = TALLOC_ZERO_P(ctx, struct srvsvc_NetShareCtr2);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr2);

		ctr.ctr2->count = alloc_entries;
		ctr.ctr2->array = TALLOC_ZERO_ARRAY(ctx, struct srvsvc_NetShareInfo2, alloc_entries);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr2->array);

		for (snum = 0; snum < num_services; snum++) {
			if (allowed[snum] &&
			    (resume_handle <= (i + valid_share_count++)) ) {
				init_srv_share_info_2(p, &ctr.ctr2->array[i++], snum);
			}
		}

		break;

	case 501:
		ctr.ctr501 = TALLOC_ZERO_P(ctx, struct srvsvc_NetShareCtr501);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr501);

		ctr.ctr501->count = alloc_entries;
		ctr.ctr501->array = TALLOC_ZERO_ARRAY(ctx, struct srvsvc_NetShareInfo501, alloc_entries);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr501->array);

		for (snum = 0; snum < num_services; snum++) {
			if (allowed[snum] &&
			    (resume_handle <= (i + valid_share_count++)) ) {
				init_srv_share_info_501(p, &ctr.ctr501->array[i++], snum);
			}
		}

		break;

	case 502:
		ctr.ctr502 = TALLOC_ZERO_P(ctx, struct srvsvc_NetShareCtr502);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr502);

		ctr.ctr502->count = alloc_entries;
		ctr.ctr502->array = TALLOC_ZERO_ARRAY(ctx, struct srvsvc_NetShareInfo502, alloc_entries);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr502->array);

		for (snum = 0; snum < num_services; snum++) {
			if (allowed[snum] &&
			    (resume_handle <= (i + valid_share_count++)) ) {
				init_srv_share_info_502(p, &ctr.ctr502->array[i++], snum);
			}
		}

		break;

	case 1004:
		ctr.ctr1004 = TALLOC_ZERO_P(ctx, struct srvsvc_NetShareCtr1004);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr1004);

		ctr.ctr1004->count = alloc_entries;
		ctr.ctr1004->array = TALLOC_ZERO_ARRAY(ctx, struct srvsvc_NetShareInfo1004, alloc_entries);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr1004->array);

		for (snum = 0; snum < num_services; snum++) {
			if (allowed[snum] &&
			    (resume_handle <= (i + valid_share_count++)) ) {
				init_srv_share_info_1004(p, &ctr.ctr1004->array[i++], snum);
			}
		}

		break;

	case 1005:
		ctr.ctr1005 = TALLOC_ZERO_P(ctx, struct srvsvc_NetShareCtr1005);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr1005);

		ctr.ctr1005->count = alloc_entries;
		ctr.ctr1005->array = TALLOC_ZERO_ARRAY(ctx, struct srvsvc_NetShareInfo1005, alloc_entries);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr1005->array);

		for (snum = 0; snum < num_services; snum++) {
			if (allowed[snum] &&
			    (resume_handle <= (i + valid_share_count++)) ) {
				init_srv_share_info_1005(p, &ctr.ctr1005->array[i++], snum);
			}
		}

		break;

	case 1006:
		ctr.ctr1006 = TALLOC_ZERO_P(ctx, struct srvsvc_NetShareCtr1006);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr1006);

		ctr.ctr1006->count = alloc_entries;
		ctr.ctr1006->array = TALLOC_ZERO_ARRAY(ctx, struct srvsvc_NetShareInfo1006, alloc_entries);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr1006->array);

		for (snum = 0; snum < num_services; snum++) {
			if (allowed[snum] &&
			    (resume_handle <= (i + valid_share_count++)) ) {
				init_srv_share_info_1006(p, &ctr.ctr1006->array[i++], snum);
			}
		}

		break;

	case 1007:
		ctr.ctr1007 = TALLOC_ZERO_P(ctx, struct srvsvc_NetShareCtr1007);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr1007);

		ctr.ctr1007->count = alloc_entries;
		ctr.ctr1007->array = TALLOC_ZERO_ARRAY(ctx, struct srvsvc_NetShareInfo1007, alloc_entries);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr1007->array);

		for (snum = 0; snum < num_services; snum++) {
			if (allowed[snum] &&
			    (resume_handle <= (i + valid_share_count++)) ) {
				init_srv_share_info_1007(p, &ctr.ctr1007->array[i++], snum);
			}
		}

		break;

	case 1501:
		ctr.ctr1501 = TALLOC_ZERO_P(ctx, struct srvsvc_NetShareCtr1501);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr1501);

		ctr.ctr1501->count = alloc_entries;
		ctr.ctr1501->array = TALLOC_ZERO_ARRAY(ctx, struct sec_desc_buf, alloc_entries);
		W_ERROR_HAVE_NO_MEMORY(ctr.ctr1501->array);

		for (snum = 0; snum < num_services; snum++) {
			if (allowed[snum] &&
			    (resume_handle <= (i + valid_share_count++)) ) {
				struct sec_desc_buf *sd_buf = NULL;
				init_srv_share_info_1501(p, &sd_buf, snum);
				ctr.ctr1501->array[i++] = *sd_buf;
			}
		}

		break;

	default:
		DEBUG(5,("init_srv_share_info_ctr: unsupported switch value %d\n",
			info_ctr->level));
		return WERR_UNKNOWN_LEVEL;
	}

	*total_entries = alloc_entries;
	if (resume_handle_p) {
		if (all_shares) {
			*resume_handle_p = (num_entries == 0) ? *resume_handle_p : 0;
		} else {
			*resume_handle_p = num_entries;
		}
	}

	info_ctr->ctr = ctr;

	return WERR_OK;
}

/*******************************************************************
 fill in a sess info level 0 structure.
 ********************************************************************/

static WERROR init_srv_sess_info_0(struct pipes_struct *p,
				   struct srvsvc_NetSessCtr0 *ctr0,
				   uint32_t *resume_handle_p,
				   uint32_t *total_entries)
{
	struct sessionid *session_list;
	uint32_t num_entries = 0;
	uint32_t resume_handle = resume_handle_p ? *resume_handle_p : 0;
	*total_entries = list_sessions(p->mem_ctx, &session_list);

	DEBUG(5,("init_srv_sess_info_0\n"));

	if (ctr0 == NULL) {
		if (resume_handle_p) {
			*resume_handle_p = 0;
		}
		return WERR_OK;
	}

	for (; resume_handle < *total_entries; resume_handle++) {

		ctr0->array = TALLOC_REALLOC_ARRAY(p->mem_ctx,
						   ctr0->array,
						   struct srvsvc_NetSessInfo0,
						   num_entries+1);
		W_ERROR_HAVE_NO_MEMORY(ctr0->array);

		ctr0->array[num_entries].client =
			session_list[resume_handle].remote_machine;

		num_entries++;
	}

	ctr0->count = num_entries;

	if (resume_handle_p) {
		if (*resume_handle_p >= *total_entries) {
			*resume_handle_p = 0;
		} else {
			*resume_handle_p = resume_handle;
		}
	}

	return WERR_OK;
}

/*******************************************************************
********************************************************************/

static void sess_file_fn( const struct share_mode_entry *e,
                          const char *sharepath, const char *fname,
			  void *data )
{
	struct sess_file_count *sess = (struct sess_file_count *)data;

	if ( procid_equal(&e->pid, &sess->pid) && (sess->uid == e->uid) ) {
		sess->count++;
	}

	return;
}

/*******************************************************************
********************************************************************/

static int net_count_files( uid_t uid, struct server_id pid )
{
	struct sess_file_count s_file_cnt;

	s_file_cnt.count = 0;
	s_file_cnt.uid = uid;
	s_file_cnt.pid = pid;

	share_mode_forall( sess_file_fn, &s_file_cnt );

	return s_file_cnt.count;
}

/*******************************************************************
 fill in a sess info level 1 structure.
 ********************************************************************/

static WERROR init_srv_sess_info_1(struct pipes_struct *p,
				   struct srvsvc_NetSessCtr1 *ctr1,
				   uint32_t *resume_handle_p,
				   uint32_t *total_entries)
{
	struct sessionid *session_list;
	uint32_t num_entries = 0;
	time_t now = time(NULL);
	uint32_t resume_handle = resume_handle_p ? *resume_handle_p : 0;

	ZERO_STRUCTP(ctr1);

	if (ctr1 == NULL) {
		if (resume_handle_p) {
			*resume_handle_p = 0;
		}
		return WERR_OK;
	}

	*total_entries = list_sessions(p->mem_ctx, &session_list);

	for (; resume_handle < *total_entries; resume_handle++) {
		uint32 num_files;
		uint32 connect_time;
		struct passwd *pw = sys_getpwnam(session_list[resume_handle].username);
		bool guest;

		if ( !pw ) {
			DEBUG(10,("init_srv_sess_info_1: failed to find owner: %s\n",
				session_list[resume_handle].username));
			continue;
		}

		connect_time = (uint32_t)(now - session_list[resume_handle].connect_start);
		num_files = net_count_files(pw->pw_uid, session_list[resume_handle].pid);
		guest = strequal( session_list[resume_handle].username, lp_guestaccount() );

		ctr1->array = TALLOC_REALLOC_ARRAY(p->mem_ctx,
						   ctr1->array,
						   struct srvsvc_NetSessInfo1,
						   num_entries+1);
		W_ERROR_HAVE_NO_MEMORY(ctr1->array);

		ctr1->array[num_entries].client		= session_list[resume_handle].remote_machine;
		ctr1->array[num_entries].user		= session_list[resume_handle].username;
		ctr1->array[num_entries].num_open	= num_files;
		ctr1->array[num_entries].time		= connect_time;
		ctr1->array[num_entries].idle_time	= 0;
		ctr1->array[num_entries].user_flags	= guest;

		num_entries++;
	}

	ctr1->count = num_entries;

	if (resume_handle_p) {
		if (*resume_handle_p >= *total_entries) {
			*resume_handle_p = 0;
		} else {
			*resume_handle_p = resume_handle;
		}
	}

	return WERR_OK;
}

/*******************************************************************
 fill in a conn info level 0 structure.
 ********************************************************************/

static WERROR init_srv_conn_info_0(struct srvsvc_NetConnCtr0 *ctr0,
				   uint32_t *resume_handle_p,
				   uint32_t *total_entries)
{
	uint32_t num_entries = 0;
	uint32_t resume_handle = resume_handle_p ? *resume_handle_p : 0;

	DEBUG(5,("init_srv_conn_info_0\n"));

	if (ctr0 == NULL) {
		if (resume_handle_p) {
			*resume_handle_p = 0;
		}
		return WERR_OK;
	}

	*total_entries = 1;

	ZERO_STRUCTP(ctr0);

	for (; resume_handle < *total_entries; resume_handle++) {

		ctr0->array = TALLOC_REALLOC_ARRAY(talloc_tos(),
						   ctr0->array,
						   struct srvsvc_NetConnInfo0,
						   num_entries+1);
		if (!ctr0->array) {
			return WERR_NOMEM;
		}

		ctr0->array[num_entries].conn_id = *total_entries;

		/* move on to creating next connection */
		num_entries++;
	}

	ctr0->count = num_entries;
	*total_entries = num_entries;

	if (resume_handle_p) {
		if (*resume_handle_p >= *total_entries) {
			*resume_handle_p = 0;
		} else {
			*resume_handle_p = resume_handle;
		}
	}

	return WERR_OK;
}

/*******************************************************************
 fill in a conn info level 1 structure.
 ********************************************************************/

static WERROR init_srv_conn_info_1(struct srvsvc_NetConnCtr1 *ctr1,
				   uint32_t *resume_handle_p,
				   uint32_t *total_entries)
{
	uint32_t num_entries = 0;
	uint32_t resume_handle = resume_handle_p ? *resume_handle_p : 0;

	DEBUG(5,("init_srv_conn_info_1\n"));

	if (ctr1 == NULL) {
		if (resume_handle_p) {
			*resume_handle_p = 0;
		}
		return WERR_OK;
	}

	*total_entries = 1;

	ZERO_STRUCTP(ctr1);

	for (; resume_handle < *total_entries; resume_handle++) {

		ctr1->array = TALLOC_REALLOC_ARRAY(talloc_tos(),
						   ctr1->array,
						   struct srvsvc_NetConnInfo1,
						   num_entries+1);
		if (!ctr1->array) {
			return WERR_NOMEM;
		}

		ctr1->array[num_entries].conn_id	= *total_entries;
		ctr1->array[num_entries].conn_type	= 0x3;
		ctr1->array[num_entries].num_open	= 1;
		ctr1->array[num_entries].num_users	= 1;
		ctr1->array[num_entries].conn_time	= 3;
		ctr1->array[num_entries].user		= "dummy_user";
		ctr1->array[num_entries].share		= "IPC$";

		/* move on to creating next connection */
		num_entries++;
	}

	ctr1->count = num_entries;
	*total_entries = num_entries;

	if (resume_handle_p) {
		if (*resume_handle_p >= *total_entries) {
			*resume_handle_p = 0;
		} else {
			*resume_handle_p = resume_handle;
		}
	}

	return WERR_OK;
}

/*******************************************************************
 _srvsvc_NetFileEnum
*******************************************************************/

WERROR _srvsvc_NetFileEnum(struct pipes_struct *p,
			   struct srvsvc_NetFileEnum *r)
{
	TALLOC_CTX *ctx = NULL;
	struct srvsvc_NetFileCtr3 *ctr3;
	uint32_t resume_hnd = 0;
	WERROR werr;

	switch (r->in.info_ctr->level) {
	case 3:
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	if (!nt_token_check_sid(&global_sid_Builtin_Administrators,
				p->session_info->security_token)) {
		DEBUG(1, ("Enumerating files only allowed for "
			  "administrators\n"));
		return WERR_ACCESS_DENIED;
	}

	ctx = talloc_tos();
	ctr3 = r->in.info_ctr->ctr.ctr3;
	if (!ctr3) {
		werr = WERR_INVALID_PARAM;
		goto done;
	}

	/* TODO -- Windows enumerates
	   (b) active pipes
	   (c) open directories and files */

	werr = net_enum_files(ctx, r->in.user, &ctr3, resume_hnd);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = net_enum_pipes(ctx, r->in.user, &ctr3, resume_hnd);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	*r->out.totalentries = ctr3->count;
	r->out.info_ctr->ctr.ctr3->array = ctr3->array;
	r->out.info_ctr->ctr.ctr3->count = ctr3->count;

	werr = WERR_OK;

 done:
	return werr;
}

/*******************************************************************
 _srvsvc_NetSrvGetInfo
********************************************************************/

WERROR _srvsvc_NetSrvGetInfo(struct pipes_struct *p,
			     struct srvsvc_NetSrvGetInfo *r)
{
	WERROR status = WERR_OK;

	DEBUG(5,("_srvsvc_NetSrvGetInfo: %d\n", __LINE__));

	if (!pipe_access_check(p)) {
		DEBUG(3, ("access denied to _srvsvc_NetSrvGetInfo\n"));
		return WERR_ACCESS_DENIED;
	}

	switch (r->in.level) {

		/* Technically level 102 should only be available to
		   Administrators but there isn't anything super-secret
		   here, as most of it is made up. */

	case 102: {
		struct srvsvc_NetSrvInfo102 *info102;

		info102 = TALLOC_P(p->mem_ctx, struct srvsvc_NetSrvInfo102);
		if (!info102) {
			return WERR_NOMEM;
		}

		info102->platform_id	= PLATFORM_ID_NT;
		info102->server_name	= global_myname();
		info102->version_major	= lp_major_announce_version();
		info102->version_minor	= lp_minor_announce_version();
		info102->server_type	= lp_default_server_announce();
		info102->comment	= string_truncate(lp_serverstring(),
						MAX_SERVER_STRING_LENGTH);
		info102->users		= 0xffffffff;
		info102->disc		= 0xf;
		info102->hidden		= 0;
		info102->announce	= 240;
		info102->anndelta	= 3000;
		info102->licenses	= 100000;
		info102->userpath	= "C:\\";

		r->out.info->info102 = info102;
		break;
	}
	case 101: {
		struct srvsvc_NetSrvInfo101 *info101;

		info101 = TALLOC_P(p->mem_ctx, struct srvsvc_NetSrvInfo101);
		if (!info101) {
			return WERR_NOMEM;
		}

		info101->platform_id	= PLATFORM_ID_NT;
		info101->server_name	= global_myname();
		info101->version_major	= lp_major_announce_version();
		info101->version_minor	= lp_minor_announce_version();
		info101->server_type	= lp_default_server_announce();
		info101->comment	= string_truncate(lp_serverstring(),
						MAX_SERVER_STRING_LENGTH);

		r->out.info->info101 = info101;
		break;
	}
	case 100: {
		struct srvsvc_NetSrvInfo100 *info100;

		info100 = TALLOC_P(p->mem_ctx, struct srvsvc_NetSrvInfo100);
		if (!info100) {
			return WERR_NOMEM;
		}

		info100->platform_id	= PLATFORM_ID_NT;
		info100->server_name	= global_myname();

		r->out.info->info100 = info100;

		break;
	}
	default:
		status = WERR_UNKNOWN_LEVEL;
		break;
	}

	DEBUG(5,("_srvsvc_NetSrvGetInfo: %d\n", __LINE__));

	return status;
}

/*******************************************************************
 _srvsvc_NetSrvSetInfo
********************************************************************/

WERROR _srvsvc_NetSrvSetInfo(struct pipes_struct *p,
			     struct srvsvc_NetSrvSetInfo *r)
{
	WERROR status = WERR_OK;

	DEBUG(5,("_srvsvc_NetSrvSetInfo: %d\n", __LINE__));

	/* Set up the net server set info structure. */

	DEBUG(5,("_srvsvc_NetSrvSetInfo: %d\n", __LINE__));

	return status;
}

/*******************************************************************
 _srvsvc_NetConnEnum
********************************************************************/

WERROR _srvsvc_NetConnEnum(struct pipes_struct *p,
			   struct srvsvc_NetConnEnum *r)
{
	WERROR werr;

	DEBUG(5,("_srvsvc_NetConnEnum: %d\n", __LINE__));

	if (!nt_token_check_sid(&global_sid_Builtin_Administrators,
				p->session_info->security_token)) {
		DEBUG(1, ("Enumerating connections only allowed for "
			  "administrators\n"));
		return WERR_ACCESS_DENIED;
	}

	switch (r->in.info_ctr->level) {
		case 0:
			werr = init_srv_conn_info_0(r->in.info_ctr->ctr.ctr0,
						    r->in.resume_handle,
						    r->out.totalentries);
			break;
		case 1:
			werr = init_srv_conn_info_1(r->in.info_ctr->ctr.ctr1,
						    r->in.resume_handle,
						    r->out.totalentries);
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	DEBUG(5,("_srvsvc_NetConnEnum: %d\n", __LINE__));

	return werr;
}

/*******************************************************************
 _srvsvc_NetSessEnum
********************************************************************/

WERROR _srvsvc_NetSessEnum(struct pipes_struct *p,
			   struct srvsvc_NetSessEnum *r)
{
	WERROR werr;

	DEBUG(5,("_srvsvc_NetSessEnum: %d\n", __LINE__));

	if (!nt_token_check_sid(&global_sid_Builtin_Administrators,
				p->session_info->security_token)) {
		DEBUG(1, ("Enumerating sessions only allowed for "
			  "administrators\n"));
		return WERR_ACCESS_DENIED;
	}

	switch (r->in.info_ctr->level) {
		case 0:
			werr = init_srv_sess_info_0(p,
						    r->in.info_ctr->ctr.ctr0,
						    r->in.resume_handle,
						    r->out.totalentries);
			break;
		case 1:
			werr = init_srv_sess_info_1(p,
						    r->in.info_ctr->ctr.ctr1,
						    r->in.resume_handle,
						    r->out.totalentries);
			break;
		default:
			return WERR_UNKNOWN_LEVEL;
	}

	DEBUG(5,("_srvsvc_NetSessEnum: %d\n", __LINE__));

	return werr;
}

/*******************************************************************
 _srvsvc_NetSessDel
********************************************************************/

WERROR _srvsvc_NetSessDel(struct pipes_struct *p,
			  struct srvsvc_NetSessDel *r)
{
	struct sessionid *session_list;
	int num_sessions, snum;
	const char *username;
	const char *machine;
	bool not_root = False;
	WERROR werr;

	username = r->in.user;
	machine = r->in.client;

	/* strip leading backslashes if any */
	if (machine && machine[0] == '\\' && machine[1] == '\\') {
		machine += 2;
	}

	num_sessions = list_sessions(p->mem_ctx, &session_list);

	DEBUG(5,("_srvsvc_NetSessDel: %d\n", __LINE__));

	werr = WERR_ACCESS_DENIED;

	/* fail out now if you are not root or not a domain admin */

	if ((p->session_info->utok.uid != sec_initial_uid()) &&
		( ! nt_token_check_domain_rid(p->session_info->security_token,
					      DOMAIN_RID_ADMINS))) {

		goto done;
	}

	for (snum = 0; snum < num_sessions; snum++) {

		if ((strequal(session_list[snum].username, username) || username[0] == '\0' ) &&
		    strequal(session_list[snum].remote_machine, machine)) {

			NTSTATUS ntstat;

			if (p->session_info->utok.uid != sec_initial_uid()) {
				not_root = True;
				become_root();
			}

			ntstat = messaging_send(p->msg_ctx,
						session_list[snum].pid,
						MSG_SHUTDOWN, &data_blob_null);

			if (NT_STATUS_IS_OK(ntstat))
				werr = WERR_OK;

			if (not_root)
				unbecome_root();
		}
	}

	DEBUG(5,("_srvsvc_NetSessDel: %d\n", __LINE__));

done:

	return werr;
}

/*******************************************************************
 _srvsvc_NetShareEnumAll
********************************************************************/

WERROR _srvsvc_NetShareEnumAll(struct pipes_struct *p,
			       struct srvsvc_NetShareEnumAll *r)
{
	WERROR werr;

	DEBUG(5,("_srvsvc_NetShareEnumAll: %d\n", __LINE__));

	if (!pipe_access_check(p)) {
		DEBUG(3, ("access denied to _srvsvc_NetShareEnumAll\n"));
		return WERR_ACCESS_DENIED;
	}

	/* Create the list of shares for the response. */
	werr = init_srv_share_info_ctr(p,
				       r->in.info_ctr,
				       r->in.resume_handle,
				       r->out.totalentries,
				       true);

	DEBUG(5,("_srvsvc_NetShareEnumAll: %d\n", __LINE__));

	return werr;
}

/*******************************************************************
 _srvsvc_NetShareEnum
********************************************************************/

WERROR _srvsvc_NetShareEnum(struct pipes_struct *p,
			    struct srvsvc_NetShareEnum *r)
{
	WERROR werr;

	DEBUG(5,("_srvsvc_NetShareEnum: %d\n", __LINE__));

	if (!pipe_access_check(p)) {
		DEBUG(3, ("access denied to _srvsvc_NetShareEnum\n"));
		return WERR_ACCESS_DENIED;
	}

	/* Create the list of shares for the response. */
	werr = init_srv_share_info_ctr(p,
				       r->in.info_ctr,
				       r->in.resume_handle,
				       r->out.totalentries,
				       false);

	DEBUG(5,("_srvsvc_NetShareEnum: %d\n", __LINE__));

	return werr;
}

/*******************************************************************
 _srvsvc_NetShareGetInfo
********************************************************************/

WERROR _srvsvc_NetShareGetInfo(struct pipes_struct *p,
			       struct srvsvc_NetShareGetInfo *r)
{
	WERROR status = WERR_OK;
	char *share_name = NULL;
	int snum;
	union srvsvc_NetShareInfo *info = r->out.info;

	DEBUG(5,("_srvsvc_NetShareGetInfo: %d\n", __LINE__));

	if (!r->in.share_name) {
		return WERR_INVALID_NAME;
	}

	snum = find_service(talloc_tos(), r->in.share_name, &share_name);
	if (!share_name) {
		return WERR_NOMEM;
	}
	if (snum < 0) {
		return WERR_INVALID_NAME;
	}

	switch (r->in.level) {
		case 0:
			info->info0 = TALLOC_P(p->mem_ctx, struct srvsvc_NetShareInfo0);
			W_ERROR_HAVE_NO_MEMORY(info->info0);
			init_srv_share_info_0(p, info->info0, snum);
			break;
		case 1:
			info->info1 = TALLOC_P(p->mem_ctx, struct srvsvc_NetShareInfo1);
			W_ERROR_HAVE_NO_MEMORY(info->info1);
			init_srv_share_info_1(p, info->info1, snum);
			break;
		case 2:
			info->info2 = TALLOC_P(p->mem_ctx, struct srvsvc_NetShareInfo2);
			W_ERROR_HAVE_NO_MEMORY(info->info2);
			init_srv_share_info_2(p, info->info2, snum);
			break;
		case 501:
			info->info501 = TALLOC_P(p->mem_ctx, struct srvsvc_NetShareInfo501);
			W_ERROR_HAVE_NO_MEMORY(info->info501);
			init_srv_share_info_501(p, info->info501, snum);
			break;
		case 502:
			info->info502 = TALLOC_P(p->mem_ctx, struct srvsvc_NetShareInfo502);
			W_ERROR_HAVE_NO_MEMORY(info->info502);
			init_srv_share_info_502(p, info->info502, snum);
			break;
		case 1004:
			info->info1004 = TALLOC_P(p->mem_ctx, struct srvsvc_NetShareInfo1004);
			W_ERROR_HAVE_NO_MEMORY(info->info1004);
			init_srv_share_info_1004(p, info->info1004, snum);
			break;
		case 1005:
			info->info1005 = TALLOC_P(p->mem_ctx, struct srvsvc_NetShareInfo1005);
			W_ERROR_HAVE_NO_MEMORY(info->info1005);
			init_srv_share_info_1005(p, info->info1005, snum);
			break;
		case 1006:
			info->info1006 = TALLOC_P(p->mem_ctx, struct srvsvc_NetShareInfo1006);
			W_ERROR_HAVE_NO_MEMORY(info->info1006);
			init_srv_share_info_1006(p, info->info1006, snum);
			break;
		case 1007:
			info->info1007 = TALLOC_P(p->mem_ctx, struct srvsvc_NetShareInfo1007);
			W_ERROR_HAVE_NO_MEMORY(info->info1007);
			init_srv_share_info_1007(p, info->info1007, snum);
			break;
		case 1501:
			init_srv_share_info_1501(p, &info->info1501, snum);
			break;
		default:
			DEBUG(5,("_srvsvc_NetShareGetInfo: unsupported switch value %d\n",
				r->in.level));
			status = WERR_UNKNOWN_LEVEL;
			break;
	}

	DEBUG(5,("_srvsvc_NetShareGetInfo: %d\n", __LINE__));

	return status;
}

/*******************************************************************
 _srvsvc_NetShareSetInfo. Modify share details.
********************************************************************/

WERROR _srvsvc_NetShareSetInfo(struct pipes_struct *p,
			       struct srvsvc_NetShareSetInfo *r)
{
	char *command = NULL;
	char *share_name = NULL;
	char *comment = NULL;
	const char *pathname = NULL;
	int type;
	int snum;
	int ret;
	char *path = NULL;
	struct security_descriptor *psd = NULL;
	bool is_disk_op = False;
	int max_connections = 0;
	TALLOC_CTX *ctx = p->mem_ctx;
	union srvsvc_NetShareInfo *info = r->in.info;

#ifndef FULL_SRVSVC
	return WERR_ACCESS_DENIED;
#endif

	DEBUG(5,("_srvsvc_NetShareSetInfo: %d\n", __LINE__));

	if (!r->in.share_name) {
		return WERR_INVALID_NAME;
	}

	if (r->out.parm_error) {
		*r->out.parm_error = 0;
	}

	if ( strequal(r->in.share_name,"IPC$")
		|| ( lp_enable_asu_support() && strequal(r->in.share_name,"ADMIN$") )
		|| strequal(r->in.share_name,"global") )
	{
		DEBUG(5,("_srvsvc_NetShareSetInfo: share %s cannot be "
			"modified by a remote user.\n",
			r->in.share_name ));
		return WERR_ACCESS_DENIED;
	}

	snum = find_service(talloc_tos(), r->in.share_name, &share_name);
	if (!share_name) {
		return WERR_NOMEM;
	}

	/* Does this share exist ? */
	if (snum < 0)
		return WERR_NET_NAME_NOT_FOUND;

	/* No change to printer shares. */
	if (lp_print_ok(snum))
		return WERR_ACCESS_DENIED;

	is_disk_op = security_token_has_privilege(p->session_info->security_token, SEC_PRIV_DISK_OPERATOR);

	/* fail out now if you are not root and not a disk op */

	if ( p->session_info->utok.uid != sec_initial_uid() && !is_disk_op ) {
		DEBUG(2,("_srvsvc_NetShareSetInfo: uid %u doesn't have the "
			"SeDiskOperatorPrivilege privilege needed to modify "
			"share %s\n",
			(unsigned int)p->session_info->utok.uid,
			share_name ));
		return WERR_ACCESS_DENIED;
	}

	switch (r->in.level) {
	case 1:
		pathname = talloc_strdup(ctx, lp_pathname(snum));
		comment = talloc_strdup(ctx, info->info1->comment);
		type = info->info1->type;
		psd = NULL;
		break;
	case 2:
		comment = talloc_strdup(ctx, info->info2->comment);
		pathname = info->info2->path;
		type = info->info2->type;
		max_connections = (info->info2->max_users == (uint32_t)-1) ?
			0 : info->info2->max_users;
		psd = NULL;
		break;
#if 0
		/* not supported on set but here for completeness */
	case 501:
		comment = talloc_strdup(ctx, info->info501->comment);
		type = info->info501->type;
		psd = NULL;
		break;
#endif
	case 502:
		comment = talloc_strdup(ctx, info->info502->comment);
		pathname = info->info502->path;
		type = info->info502->type;
		psd = info->info502->sd_buf.sd;
		map_generic_share_sd_bits(psd);
		break;
	case 1004:
		pathname = talloc_strdup(ctx, lp_pathname(snum));
		comment = talloc_strdup(ctx, info->info1004->comment);
		type = STYPE_DISKTREE;
		break;
	case 1005:
                /* XP re-sets the csc policy even if it wasn't changed by the
		   user, so we must compare it to see if it's what is set in
		   smb.conf, so that we can contine other ops like setting
		   ACLs on a share */
		if (((info->info1005->dfs_flags &
		      SHARE_1005_CSC_POLICY_MASK) >>
		     SHARE_1005_CSC_POLICY_SHIFT) == lp_csc_policy(snum))
			return WERR_OK;
		else {
			DEBUG(3, ("_srvsvc_NetShareSetInfo: client is trying to change csc policy from the network; must be done with smb.conf\n"));
			return WERR_ACCESS_DENIED;
		}
	case 1006:
	case 1007:
		return WERR_ACCESS_DENIED;
	case 1501:
		pathname = talloc_strdup(ctx, lp_pathname(snum));
		comment = talloc_strdup(ctx, lp_comment(snum));
		psd = info->info1501->sd;
		map_generic_share_sd_bits(psd);
		type = STYPE_DISKTREE;
		break;
	default:
		DEBUG(5,("_srvsvc_NetShareSetInfo: unsupported switch value %d\n",
			r->in.level));
		return WERR_UNKNOWN_LEVEL;
	}

	/* We can only modify disk shares. */
	if (type != STYPE_DISKTREE) {
		DEBUG(5,("_srvsvc_NetShareSetInfo: share %s is not a "
			"disk share\n",
			share_name ));
		return WERR_ACCESS_DENIED;
	}

	if (comment == NULL) {
		return WERR_NOMEM;
	}

	/* Check if the pathname is valid. */
	if (!(path = valid_share_pathname(p->mem_ctx, pathname ))) {
		DEBUG(5,("_srvsvc_NetShareSetInfo: invalid pathname %s\n",
			pathname ));
		return WERR_OBJECT_PATH_INVALID;
	}

	/* Ensure share name, pathname and comment don't contain '"' characters. */
	string_replace(share_name, '"', ' ');
	string_replace(path, '"', ' ');
	string_replace(comment, '"', ' ');

	DEBUG(10,("_srvsvc_NetShareSetInfo: change share command = %s\n",
		lp_change_share_cmd() ? lp_change_share_cmd() : "NULL" ));

	/* Only call modify function if something changed. */

	if (strcmp(path, lp_pathname(snum)) || strcmp(comment, lp_comment(snum))
			|| (lp_max_connections(snum) != max_connections)) {
		if (!lp_change_share_cmd() || !*lp_change_share_cmd()) {
			DEBUG(10,("_srvsvc_NetShareSetInfo: No change share command\n"));
			return WERR_ACCESS_DENIED;
		}

		command = talloc_asprintf(p->mem_ctx,
				"%s \"%s\" \"%s\" \"%s\" \"%s\" %d",
				lp_change_share_cmd(),
				get_dyn_CONFIGFILE(),
				share_name,
				path,
				comment ? comment : "",
				max_connections);
		if (!command) {
			return WERR_NOMEM;
		}

		DEBUG(10,("_srvsvc_NetShareSetInfo: Running [%s]\n", command ));

		/********* BEGIN SeDiskOperatorPrivilege BLOCK *********/

		if (is_disk_op)
			become_root();

		if ( (ret = smbrun(command, NULL)) == 0 ) {
			/* Tell everyone we updated smb.conf. */
			message_send_all(p->msg_ctx, MSG_SMB_CONF_UPDATED,
					 NULL, 0, NULL);
		}

		if ( is_disk_op )
			unbecome_root();

		/********* END SeDiskOperatorPrivilege BLOCK *********/

		DEBUG(3,("_srvsvc_NetShareSetInfo: Running [%s] returned (%d)\n",
			command, ret ));

		TALLOC_FREE(command);

		if ( ret != 0 )
			return WERR_ACCESS_DENIED;
	} else {
		DEBUG(10,("_srvsvc_NetShareSetInfo: No change to share name (%s)\n",
			share_name ));
	}

	/* Replace SD if changed. */
	if (psd) {
		struct security_descriptor *old_sd;
		size_t sd_size;

		old_sd = get_share_security(p->mem_ctx, lp_servicename(snum), &sd_size);

		if (old_sd && !security_descriptor_equal(old_sd, psd)) {
			if (!set_share_security(share_name, psd))
				DEBUG(0,("_srvsvc_NetShareSetInfo: Failed to change security info in share %s.\n",
					share_name ));
		}
	}

	DEBUG(5,("_srvsvc_NetShareSetInfo: %d\n", __LINE__));

	return WERR_OK;
}

/*******************************************************************
 _srvsvc_NetShareAdd.
 Call 'add_share_command "sharename" "pathname"
 "comment" "max connections = "
********************************************************************/

WERROR _srvsvc_NetShareAdd(struct pipes_struct *p,
			   struct srvsvc_NetShareAdd *r)
{
	char *command = NULL;
	char *share_name_in = NULL;
	char *share_name = NULL;
	char *comment = NULL;
	char *pathname = NULL;
	int type;
	int snum;
	int ret;
	char *path;
	struct security_descriptor *psd = NULL;
	bool is_disk_op;
	int max_connections = 0;
	TALLOC_CTX *ctx = p->mem_ctx;

#ifndef FULL_SRVSVC
	return WERR_ACCESS_DENIED;
#endif

	DEBUG(5,("_srvsvc_NetShareAdd: %d\n", __LINE__));

	if (r->out.parm_error) {
		*r->out.parm_error = 0;
	}

	is_disk_op = security_token_has_privilege(p->session_info->security_token, SEC_PRIV_DISK_OPERATOR);

	if (p->session_info->utok.uid != sec_initial_uid()  && !is_disk_op )
		return WERR_ACCESS_DENIED;

	if (!lp_add_share_cmd() || !*lp_add_share_cmd()) {
		DEBUG(10,("_srvsvc_NetShareAdd: No add share command\n"));
		return WERR_ACCESS_DENIED;
	}

	switch (r->in.level) {
	case 0:
		/* No path. Not enough info in a level 0 to do anything. */
		return WERR_ACCESS_DENIED;
	case 1:
		/* Not enough info in a level 1 to do anything. */
		return WERR_ACCESS_DENIED;
	case 2:
		share_name_in = talloc_strdup(ctx, r->in.info->info2->name);
		comment = talloc_strdup(ctx, r->in.info->info2->comment);
		pathname = talloc_strdup(ctx, r->in.info->info2->path);
		max_connections = (r->in.info->info2->max_users == (uint32_t)-1) ?
			0 : r->in.info->info2->max_users;
		type = r->in.info->info2->type;
		break;
	case 501:
		/* No path. Not enough info in a level 501 to do anything. */
		return WERR_ACCESS_DENIED;
	case 502:
		share_name_in = talloc_strdup(ctx, r->in.info->info502->name);
		comment = talloc_strdup(ctx, r->in.info->info502->comment);
		pathname = talloc_strdup(ctx, r->in.info->info502->path);
		max_connections = (r->in.info->info502->max_users == (uint32_t)-1) ?
			0 : r->in.info->info502->max_users;
		type = r->in.info->info502->type;
		psd = r->in.info->info502->sd_buf.sd;
		map_generic_share_sd_bits(psd);
		break;

		/* none of the following contain share names.  NetShareAdd does not have a separate parameter for the share name */

	case 1004:
	case 1005:
	case 1006:
	case 1007:
		return WERR_ACCESS_DENIED;
	case 1501:
		/* DFS only level. */
		return WERR_ACCESS_DENIED;
	default:
		DEBUG(5,("_srvsvc_NetShareAdd: unsupported switch value %d\n",
			r->in.level));
		return WERR_UNKNOWN_LEVEL;
	}

	/* check for invalid share names */

	if (!share_name_in || !validate_net_name(share_name_in,
				INVALID_SHARENAME_CHARS,
				strlen(share_name_in))) {
		DEBUG(5,("_srvsvc_NetShareAdd: Bad sharename \"%s\"\n",
					share_name_in ? share_name_in : ""));
		return WERR_INVALID_NAME;
	}

	if (strequal(share_name_in,"IPC$") || strequal(share_name_in,"global")
			|| (lp_enable_asu_support() &&
					strequal(share_name_in,"ADMIN$"))) {
		return WERR_ACCESS_DENIED;
	}

	snum = find_service(ctx, share_name_in, &share_name);
	if (!share_name) {
		return WERR_NOMEM;
	}

	/* Share already exists. */
	if (snum >= 0) {
		return WERR_FILE_EXISTS;
	}

	/* We can only add disk shares. */
	if (type != STYPE_DISKTREE) {
		return WERR_ACCESS_DENIED;
	}

	/* Check if the pathname is valid. */
	if (!(path = valid_share_pathname(p->mem_ctx, pathname))) {
		return WERR_OBJECT_PATH_INVALID;
	}

	/* Ensure share name, pathname and comment don't contain '"' characters. */
	string_replace(share_name_in, '"', ' ');
	string_replace(share_name, '"', ' ');
	string_replace(path, '"', ' ');
	if (comment) {
		string_replace(comment, '"', ' ');
	}

	command = talloc_asprintf(ctx,
			"%s \"%s\" \"%s\" \"%s\" \"%s\" %d",
			lp_add_share_cmd(),
			get_dyn_CONFIGFILE(),
			share_name_in,
			path,
			comment ? comment : "",
			max_connections);
	if (!command) {
		return WERR_NOMEM;
	}

	DEBUG(10,("_srvsvc_NetShareAdd: Running [%s]\n", command ));

	/********* BEGIN SeDiskOperatorPrivilege BLOCK *********/

	if ( is_disk_op )
		become_root();

	/* FIXME: use libnetconf here - gd */

	if ( (ret = smbrun(command, NULL)) == 0 ) {
		/* Tell everyone we updated smb.conf. */
		message_send_all(p->msg_ctx, MSG_SMB_CONF_UPDATED, NULL, 0,
				 NULL);
	}

	if ( is_disk_op )
		unbecome_root();

	/********* END SeDiskOperatorPrivilege BLOCK *********/

	DEBUG(3,("_srvsvc_NetShareAdd: Running [%s] returned (%d)\n",
		command, ret ));

	TALLOC_FREE(command);

	if ( ret != 0 )
		return WERR_ACCESS_DENIED;

	if (psd) {
		/* Note we use share_name here, not share_name_in as
		   we need a canonicalized name for setting security. */
		if (!set_share_security(share_name, psd)) {
			DEBUG(0,("_srvsvc_NetShareAdd: Failed to add security info to share %s.\n",
				share_name ));
		}
	}

	/*
	 * We don't call reload_services() here, the message will
	 * cause this to be done before the next packet is read
	 * from the client. JRA.
	 */

	DEBUG(5,("_srvsvc_NetShareAdd: %d\n", __LINE__));

	return WERR_OK;
}

/*******************************************************************
 _srvsvc_NetShareDel
 Call "delete share command" with the share name as
 a parameter.
********************************************************************/

WERROR _srvsvc_NetShareDel(struct pipes_struct *p,
			   struct srvsvc_NetShareDel *r)
{
	char *command = NULL;
	char *share_name = NULL;
	int ret;
	int snum;
	bool is_disk_op;
	struct share_params *params;
	TALLOC_CTX *ctx = p->mem_ctx;

#ifndef FULL_SRVSVC
	return WERR_ACCESS_DENIED;
#endif

	DEBUG(5,("_srvsvc_NetShareDel: %d\n", __LINE__));

	if (!r->in.share_name) {
		return WERR_NET_NAME_NOT_FOUND;
	}

	if ( strequal(r->in.share_name,"IPC$")
		|| ( lp_enable_asu_support() && strequal(r->in.share_name,"ADMIN$") )
		|| strequal(r->in.share_name,"global") )
	{
		return WERR_ACCESS_DENIED;
	}

	snum = find_service(talloc_tos(), r->in.share_name, &share_name);
	if (!share_name) {
		return WERR_NOMEM;
	}

	if (snum < 0) {
		return WERR_NO_SUCH_SHARE;
	}

	if (!(params = get_share_params(p->mem_ctx, share_name))) {
		return WERR_NO_SUCH_SHARE;
	}

	/* No change to printer shares. */
	if (lp_print_ok(snum))
		return WERR_ACCESS_DENIED;

	is_disk_op = security_token_has_privilege(p->session_info->security_token, SEC_PRIV_DISK_OPERATOR);

	if (p->session_info->utok.uid != sec_initial_uid()  && !is_disk_op )
		return WERR_ACCESS_DENIED;

	if (!lp_delete_share_cmd() || !*lp_delete_share_cmd()) {
		DEBUG(10,("_srvsvc_NetShareDel: No delete share command\n"));
		return WERR_ACCESS_DENIED;
	}

	command = talloc_asprintf(ctx,
			"%s \"%s\" \"%s\"",
			lp_delete_share_cmd(),
			get_dyn_CONFIGFILE(),
			lp_servicename(snum));
	if (!command) {
		return WERR_NOMEM;
	}

	DEBUG(10,("_srvsvc_NetShareDel: Running [%s]\n", command ));

	/********* BEGIN SeDiskOperatorPrivilege BLOCK *********/

	if ( is_disk_op )
		become_root();

	if ( (ret = smbrun(command, NULL)) == 0 ) {
		/* Tell everyone we updated smb.conf. */
		message_send_all(p->msg_ctx, MSG_SMB_CONF_UPDATED, NULL, 0,
				 NULL);
	}

	if ( is_disk_op )
		unbecome_root();

	/********* END SeDiskOperatorPrivilege BLOCK *********/

	DEBUG(3,("_srvsvc_NetShareDel: Running [%s] returned (%d)\n", command, ret ));

	if ( ret != 0 )
		return WERR_ACCESS_DENIED;

	/* Delete the SD in the database. */
	delete_share_security(lp_servicename(params->service));

	lp_killservice(params->service);

	return WERR_OK;
}

/*******************************************************************
 _srvsvc_NetShareDelSticky
********************************************************************/

WERROR _srvsvc_NetShareDelSticky(struct pipes_struct *p,
				 struct srvsvc_NetShareDelSticky *r)
{
	struct srvsvc_NetShareDel q;

	DEBUG(5,("_srvsvc_NetShareDelSticky: %d\n", __LINE__));

	q.in.server_unc		= r->in.server_unc;
	q.in.share_name		= r->in.share_name;
	q.in.reserved		= r->in.reserved;

	return _srvsvc_NetShareDel(p, &q);
}

/*******************************************************************
 _srvsvc_NetRemoteTOD
********************************************************************/

WERROR _srvsvc_NetRemoteTOD(struct pipes_struct *p,
			    struct srvsvc_NetRemoteTOD *r)
{
	struct srvsvc_NetRemoteTODInfo *tod;
	struct tm *t;
	time_t unixdate = time(NULL);

	/* We do this call first as if we do it *after* the gmtime call
	   it overwrites the pointed-to values. JRA */

	uint32 zone = get_time_zone(unixdate)/60;

	DEBUG(5,("_srvsvc_NetRemoteTOD: %d\n", __LINE__));

	if ( !(tod = TALLOC_ZERO_P(p->mem_ctx, struct srvsvc_NetRemoteTODInfo)) )
		return WERR_NOMEM;

	*r->out.info = tod;

	DEBUG(5,("_srvsvc_NetRemoteTOD: %d\n", __LINE__));

	t = gmtime(&unixdate);

	/* set up the */
	tod->elapsed	= unixdate;
	tod->msecs	= 0;
	tod->hours	= t->tm_hour;
	tod->mins	= t->tm_min;
	tod->secs	= t->tm_sec;
	tod->hunds	= 0;
	tod->timezone	= zone;
	tod->tinterval	= 10000;
	tod->day	= t->tm_mday;
	tod->month	= t->tm_mon + 1;
	tod->year	= 1900+t->tm_year;
	tod->weekday	= t->tm_wday;

	DEBUG(5,("_srvsvc_NetRemoteTOD: %d\n", __LINE__));

	return WERR_OK;
}

/***********************************************************************************
 _srvsvc_NetGetFileSecurity
 Win9x NT tools get security descriptor.
***********************************************************************************/

WERROR _srvsvc_NetGetFileSecurity(struct pipes_struct *p,
				  struct srvsvc_NetGetFileSecurity *r)
{
	struct smb_filename *smb_fname = NULL;
	struct security_descriptor *psd = NULL;
	size_t sd_size;
	char *servicename = NULL;
	SMB_STRUCT_STAT st;
	NTSTATUS nt_status;
	WERROR werr;
	connection_struct *conn = NULL;
	struct sec_desc_buf *sd_buf = NULL;
	files_struct *fsp = NULL;
	int snum;
	char *oldcwd = NULL;

	ZERO_STRUCT(st);

	if (!r->in.share) {
		werr = WERR_NET_NAME_NOT_FOUND;
		goto error_exit;
	}
	snum = find_service(talloc_tos(), r->in.share, &servicename);
	if (!servicename) {
		werr = WERR_NOMEM;
		goto error_exit;
	}
	if (snum == -1) {
		DEBUG(10, ("Could not find service %s\n", servicename));
		werr = WERR_NET_NAME_NOT_FOUND;
		goto error_exit;
	}

	nt_status = create_conn_struct(talloc_tos(), &conn, snum,
				       lp_pathname(snum), p->session_info,
				       &oldcwd);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(10, ("create_conn_struct failed: %s\n",
			   nt_errstr(nt_status)));
		werr = ntstatus_to_werror(nt_status);
		goto error_exit;
	}

	nt_status = filename_convert(talloc_tos(),
					conn,
					false,
					r->in.file,
					0,
					NULL,
					&smb_fname);
	if (!NT_STATUS_IS_OK(nt_status)) {
		werr = ntstatus_to_werror(nt_status);
		goto error_exit;
	}

	nt_status = SMB_VFS_CREATE_FILE(
		conn,					/* conn */
		NULL,					/* req */
		0,					/* root_dir_fid */
		smb_fname,				/* fname */
		FILE_READ_ATTRIBUTES,			/* access_mask */
		FILE_SHARE_READ|FILE_SHARE_WRITE,	/* share_access */
		FILE_OPEN,				/* create_disposition*/
		0,					/* create_options */
		0,					/* file_attributes */
		INTERNAL_OPEN_ONLY,			/* oplock_request */
		0,					/* allocation_size */
		0,					/* private_flags */
		NULL,					/* sd */
		NULL,					/* ea_list */
		&fsp,					/* result */
		NULL);					/* pinfo */

	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(3,("_srvsvc_NetGetFileSecurity: can't open %s\n",
			 smb_fname_str_dbg(smb_fname)));
		werr = ntstatus_to_werror(nt_status);
		goto error_exit;
	}

	nt_status = SMB_VFS_FGET_NT_ACL(fsp,
				       (SECINFO_OWNER
					|SECINFO_GROUP
					|SECINFO_DACL), &psd);

	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(3,("_srvsvc_NetGetFileSecurity: Unable to get NT ACL "
			"for file %s\n", smb_fname_str_dbg(smb_fname)));
		werr = ntstatus_to_werror(nt_status);
		goto error_exit;
	}

	sd_size = ndr_size_security_descriptor(psd, 0);

	sd_buf = TALLOC_ZERO_P(p->mem_ctx, struct sec_desc_buf);
	if (!sd_buf) {
		werr = WERR_NOMEM;
		goto error_exit;
	}

	sd_buf->sd_size = sd_size;
	sd_buf->sd = psd;

	*r->out.sd_buf = sd_buf;

	psd->dacl->revision = NT4_ACL_REVISION;

	close_file(NULL, fsp, NORMAL_CLOSE);
	vfs_ChDir(conn, oldcwd);
	SMB_VFS_DISCONNECT(conn);
	conn_free(conn);
	werr = WERR_OK;
	goto done;

error_exit:

	if (fsp) {
		close_file(NULL, fsp, NORMAL_CLOSE);
	}

	if (oldcwd) {
		vfs_ChDir(conn, oldcwd);
	}

	if (conn) {
		SMB_VFS_DISCONNECT(conn);
		conn_free(conn);
	}

 done:
	TALLOC_FREE(smb_fname);

	return werr;
}

/***********************************************************************************
 _srvsvc_NetSetFileSecurity
 Win9x NT tools set security descriptor.
***********************************************************************************/

WERROR _srvsvc_NetSetFileSecurity(struct pipes_struct *p,
				  struct srvsvc_NetSetFileSecurity *r)
{
	struct smb_filename *smb_fname = NULL;
	char *servicename = NULL;
	files_struct *fsp = NULL;
	SMB_STRUCT_STAT st;
	NTSTATUS nt_status;
	WERROR werr;
	connection_struct *conn = NULL;
	int snum;
	char *oldcwd = NULL;
	struct security_descriptor *psd = NULL;
	uint32_t security_info_sent = 0;

	ZERO_STRUCT(st);

	if (!r->in.share) {
		werr = WERR_NET_NAME_NOT_FOUND;
		goto error_exit;
	}

	snum = find_service(talloc_tos(), r->in.share, &servicename);
	if (!servicename) {
		werr = WERR_NOMEM;
		goto error_exit;
	}

	if (snum == -1) {
		DEBUG(10, ("Could not find service %s\n", servicename));
		werr = WERR_NET_NAME_NOT_FOUND;
		goto error_exit;
	}

	nt_status = create_conn_struct(talloc_tos(), &conn, snum,
				       lp_pathname(snum), p->session_info,
				       &oldcwd);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(10, ("create_conn_struct failed: %s\n",
			   nt_errstr(nt_status)));
		werr = ntstatus_to_werror(nt_status);
		goto error_exit;
	}

	nt_status = filename_convert(talloc_tos(),
					conn,
					false,
					r->in.file,
					0,
					NULL,
					&smb_fname);
	if (!NT_STATUS_IS_OK(nt_status)) {
		werr = ntstatus_to_werror(nt_status);
		goto error_exit;
	}

	nt_status = SMB_VFS_CREATE_FILE(
		conn,					/* conn */
		NULL,					/* req */
		0,					/* root_dir_fid */
		smb_fname,				/* fname */
		FILE_WRITE_ATTRIBUTES,			/* access_mask */
		FILE_SHARE_READ|FILE_SHARE_WRITE,	/* share_access */
		FILE_OPEN,				/* create_disposition*/
		0,					/* create_options */
		0,					/* file_attributes */
		INTERNAL_OPEN_ONLY,			/* oplock_request */
		0,					/* allocation_size */
		0,					/* private_flags */
		NULL,					/* sd */
		NULL,					/* ea_list */
		&fsp,					/* result */
		NULL);					/* pinfo */

	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(3,("_srvsvc_NetSetFileSecurity: can't open %s\n",
			 smb_fname_str_dbg(smb_fname)));
		werr = ntstatus_to_werror(nt_status);
		goto error_exit;
	}

	psd = r->in.sd_buf->sd;
	security_info_sent = r->in.securityinformation;

	nt_status = set_sd(fsp, psd, security_info_sent);

	if (!NT_STATUS_IS_OK(nt_status) ) {
		DEBUG(3,("_srvsvc_NetSetFileSecurity: Unable to set NT ACL "
			 "on file %s\n", r->in.share));
		werr = WERR_ACCESS_DENIED;
		goto error_exit;
	}

	close_file(NULL, fsp, NORMAL_CLOSE);
	vfs_ChDir(conn, oldcwd);
	SMB_VFS_DISCONNECT(conn);
	conn_free(conn);
	werr = WERR_OK;
	goto done;

error_exit:

	if (fsp) {
		close_file(NULL, fsp, NORMAL_CLOSE);
	}

	if (oldcwd) {
		vfs_ChDir(conn, oldcwd);
	}

	if (conn) {
		SMB_VFS_DISCONNECT(conn);
		conn_free(conn);
	}

 done:
	TALLOC_FREE(smb_fname);

	return werr;
}

/***********************************************************************************
 It may be that we want to limit users to creating shares on certain areas of the UNIX file area.
 We could define areas by mapping Windows style disks to points on the UNIX directory hierarchy.
 These disks would the disks listed by this function.
 Users could then create shares relative to these disks.  Watch out for moving these disks around.
 "Nigel Williams" <nigel@veritas.com>.
***********************************************************************************/

static const char *server_disks[] = {"C:"};

static uint32 get_server_disk_count(void)
{
	return sizeof(server_disks)/sizeof(server_disks[0]);
}

static uint32 init_server_disk_enum(uint32 *resume)
{
	uint32 server_disk_count = get_server_disk_count();

	/*resume can be an offset into the list for now*/

	if(*resume & 0x80000000)
		*resume = 0;

	if(*resume > server_disk_count)
		*resume = server_disk_count;

	return server_disk_count - *resume;
}

static const char *next_server_disk_enum(uint32 *resume)
{
	const char *disk;

	if(init_server_disk_enum(resume) == 0)
		return NULL;

	disk = server_disks[*resume];

	(*resume)++;

	DEBUG(10, ("next_server_disk_enum: reporting disk %s. resume handle %d.\n", disk, *resume));

	return disk;
}

/********************************************************************
 _srvsvc_NetDiskEnum
********************************************************************/

WERROR _srvsvc_NetDiskEnum(struct pipes_struct *p,
			   struct srvsvc_NetDiskEnum *r)
{
	uint32 i;
	const char *disk_name;
	TALLOC_CTX *ctx = p->mem_ctx;
	WERROR werr;
	uint32_t resume = r->in.resume_handle ? *r->in.resume_handle : 0;

	werr = WERR_OK;

	*r->out.totalentries = init_server_disk_enum(&resume);

	r->out.info->disks = TALLOC_ZERO_ARRAY(ctx, struct srvsvc_NetDiskInfo0,
					       MAX_SERVER_DISK_ENTRIES);
	W_ERROR_HAVE_NO_MEMORY(r->out.info->disks);

	/*allow one struct srvsvc_NetDiskInfo0 for null terminator*/

	r->out.info->count = 0;

	for(i = 0; i < MAX_SERVER_DISK_ENTRIES -1 && (disk_name = next_server_disk_enum(&resume)); i++) {

		r->out.info->count++;

		/*copy disk name into a unicode string*/

		r->out.info->disks[i].disk = talloc_strdup(ctx, disk_name);
		W_ERROR_HAVE_NO_MEMORY(r->out.info->disks[i].disk);
	}

	/* add a terminating null string.  Is this there if there is more data to come? */

	r->out.info->count++;

	r->out.info->disks[i].disk = talloc_strdup(ctx, "");
	W_ERROR_HAVE_NO_MEMORY(r->out.info->disks[i].disk);

	if (r->out.resume_handle) {
		*r->out.resume_handle = resume;
	}

	return werr;
}

/********************************************************************
 _srvsvc_NetNameValidate
********************************************************************/

WERROR _srvsvc_NetNameValidate(struct pipes_struct *p,
			       struct srvsvc_NetNameValidate *r)
{
	switch (r->in.name_type) {
	case 0x9:
		if (!validate_net_name(r->in.name, INVALID_SHARENAME_CHARS,
				       strlen_m(r->in.name)))
		{
			DEBUG(5,("_srvsvc_NetNameValidate: Bad sharename \"%s\"\n",
				r->in.name));
			return WERR_INVALID_NAME;
		}
		break;

	default:
		return WERR_UNKNOWN_LEVEL;
	}

	return WERR_OK;
}

/*******************************************************************
********************************************************************/

struct enum_file_close_state {
	struct srvsvc_NetFileClose *r;
	struct messaging_context *msg_ctx;
};

static void enum_file_close_fn( const struct share_mode_entry *e,
                          const char *sharepath, const char *fname,
			  void *private_data )
{
	char msg[MSG_SMB_SHARE_MODE_ENTRY_SIZE];
	struct enum_file_close_state *state =
		(struct enum_file_close_state *)private_data;
	uint32_t fid = (((uint32_t)(procid_to_pid(&e->pid))<<16) | e->share_file_id);

	if (fid != state->r->in.fid) {
		return; /* Not this file. */
	}

	if (!process_exists(e->pid) ) {
		return;
	}

	/* Ok - send the close message. */
	DEBUG(10,("enum_file_close_fn: request to close file %s, %s\n",
		sharepath,
		share_mode_str(talloc_tos(), 0, e) ));

	share_mode_entry_to_message(msg, e);

	state->r->out.result = ntstatus_to_werror(
		messaging_send_buf(state->msg_ctx,
				e->pid, MSG_SMB_CLOSE_FILE,
				(uint8 *)msg,
				MSG_SMB_SHARE_MODE_ENTRY_SIZE));
}

/********************************************************************
 Close a file given a 32-bit file id.
********************************************************************/

WERROR _srvsvc_NetFileClose(struct pipes_struct *p,
			    struct srvsvc_NetFileClose *r)
{
	struct enum_file_close_state state;
	bool is_disk_op;

	DEBUG(5,("_srvsvc_NetFileClose: %d\n", __LINE__));

	is_disk_op = security_token_has_privilege(p->session_info->security_token, SEC_PRIV_DISK_OPERATOR);

	if (p->session_info->utok.uid != sec_initial_uid() && !is_disk_op) {
		return WERR_ACCESS_DENIED;
	}

	/* enum_file_close_fn sends the close message to
	 * the relevent smbd process. */

	r->out.result = WERR_BADFILE;
	state.r = r;
	state.msg_ctx = p->msg_ctx;
	share_mode_forall(enum_file_close_fn, &state);
	return r->out.result;
}

/********************************************************************
********************************************************************/

WERROR _srvsvc_NetCharDevEnum(struct pipes_struct *p,
			      struct srvsvc_NetCharDevEnum *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetCharDevGetInfo(struct pipes_struct *p,
				 struct srvsvc_NetCharDevGetInfo *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetCharDevControl(struct pipes_struct *p,
				 struct srvsvc_NetCharDevControl *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetCharDevQEnum(struct pipes_struct *p,
			       struct srvsvc_NetCharDevQEnum *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetCharDevQGetInfo(struct pipes_struct *p,
				  struct srvsvc_NetCharDevQGetInfo *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetCharDevQSetInfo(struct pipes_struct *p,
				  struct srvsvc_NetCharDevQSetInfo *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetCharDevQPurge(struct pipes_struct *p,
				struct srvsvc_NetCharDevQPurge *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetCharDevQPurgeSelf(struct pipes_struct *p,
				    struct srvsvc_NetCharDevQPurgeSelf *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetFileGetInfo(struct pipes_struct *p,
			      struct srvsvc_NetFileGetInfo *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetShareCheck(struct pipes_struct *p,
			     struct srvsvc_NetShareCheck *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetServerStatisticsGet(struct pipes_struct *p,
				      struct srvsvc_NetServerStatisticsGet *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetTransportAdd(struct pipes_struct *p,
			       struct srvsvc_NetTransportAdd *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetTransportEnum(struct pipes_struct *p,
				struct srvsvc_NetTransportEnum *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetTransportDel(struct pipes_struct *p,
			       struct srvsvc_NetTransportDel *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetSetServiceBits(struct pipes_struct *p,
				 struct srvsvc_NetSetServiceBits *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetPathType(struct pipes_struct *p,
			   struct srvsvc_NetPathType *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetPathCanonicalize(struct pipes_struct *p,
				   struct srvsvc_NetPathCanonicalize *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetPathCompare(struct pipes_struct *p,
			      struct srvsvc_NetPathCompare *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NETRPRNAMECANONICALIZE(struct pipes_struct *p,
				      struct srvsvc_NETRPRNAMECANONICALIZE *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetPRNameCompare(struct pipes_struct *p,
				struct srvsvc_NetPRNameCompare *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetShareDelStart(struct pipes_struct *p,
				struct srvsvc_NetShareDelStart *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetShareDelCommit(struct pipes_struct *p,
				 struct srvsvc_NetShareDelCommit *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetServerTransportAddEx(struct pipes_struct *p,
				       struct srvsvc_NetServerTransportAddEx *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NetServerSetServiceBitsEx(struct pipes_struct *p,
					 struct srvsvc_NetServerSetServiceBitsEx *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NETRDFSGETVERSION(struct pipes_struct *p,
				 struct srvsvc_NETRDFSGETVERSION *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NETRDFSCREATELOCALPARTITION(struct pipes_struct *p,
					   struct srvsvc_NETRDFSCREATELOCALPARTITION *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NETRDFSDELETELOCALPARTITION(struct pipes_struct *p,
					   struct srvsvc_NETRDFSDELETELOCALPARTITION *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NETRDFSSETLOCALVOLUMESTATE(struct pipes_struct *p,
					  struct srvsvc_NETRDFSSETLOCALVOLUMESTATE *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NETRDFSSETSERVERINFO(struct pipes_struct *p,
				    struct srvsvc_NETRDFSSETSERVERINFO *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NETRDFSCREATEEXITPOINT(struct pipes_struct *p,
				      struct srvsvc_NETRDFSCREATEEXITPOINT *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NETRDFSDELETEEXITPOINT(struct pipes_struct *p,
				      struct srvsvc_NETRDFSDELETEEXITPOINT *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NETRDFSMODIFYPREFIX(struct pipes_struct *p,
				   struct srvsvc_NETRDFSMODIFYPREFIX *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NETRDFSFIXLOCALVOLUME(struct pipes_struct *p,
				     struct srvsvc_NETRDFSFIXLOCALVOLUME *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NETRDFSMANAGERREPORTSITEINFO(struct pipes_struct *p,
					    struct srvsvc_NETRDFSMANAGERREPORTSITEINFO *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _srvsvc_NETRSERVERTRANSPORTDELEX(struct pipes_struct *p,
					struct srvsvc_NETRSERVERTRANSPORTDELEX *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

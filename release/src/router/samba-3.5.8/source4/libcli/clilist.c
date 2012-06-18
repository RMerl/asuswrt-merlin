/* 
   Unix SMB/CIFS implementation.
   client directory list routines
   Copyright (C) Andrew Tridgell 1994-2003
   Copyright (C) James Myers 2003 <myersjj@samba.org>
   
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
#include "libcli/libcli.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"

struct search_private {
	struct clilist_file_info *dirlist;
	TALLOC_CTX *mem_ctx;
	int dirlist_len;
	int ff_searchcount;  /* total received in 1 server trip */
	int total_received;  /* total received all together */
	enum smb_search_data_level data_level;
	const char *last_name;     /* used to continue trans2 search */
	struct smb_search_id id;   /* used for old-style search */
};


/****************************************************************************
 Interpret a long filename structure.
****************************************************************************/
static bool interpret_long_filename(enum smb_search_data_level level,
				    const union smb_search_data *info,
				    struct clilist_file_info *finfo)
{
	struct clilist_file_info finfo2;

	if (!finfo) finfo = &finfo2;
	ZERO_STRUCTP(finfo);

	switch (level) {
	case RAW_SEARCH_DATA_STANDARD:
		finfo->size = info->standard.size;
		finfo->mtime = info->standard.write_time;
		finfo->attrib = info->standard.attrib;
		finfo->name = info->standard.name.s;
		finfo->short_name = info->standard.name.s;
		break;

	case RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO:
		finfo->size = info->both_directory_info.size;
		finfo->mtime = nt_time_to_unix(info->both_directory_info.write_time);
		finfo->attrib = info->both_directory_info.attrib;
		finfo->short_name = info->both_directory_info.short_name.s;
		finfo->name = info->both_directory_info.name.s;
		break;

	default:
		DEBUG(0,("Unhandled level %d in interpret_long_filename\n", (int)level));
		return false;
	}

	return true;
}

/* callback function used for trans2 search */
static bool smbcli_list_new_callback(void *private_data, const union smb_search_data *file)
{
	struct search_private *state = (struct search_private*) private_data;
	struct clilist_file_info *tdl;
 
	/* add file info to the dirlist pool */
	tdl = talloc_realloc(state, 
			     state->dirlist,
			     struct clilist_file_info,
			     state->dirlist_len + 1);
	if (!tdl) {
		return false;
	}
	state->dirlist = tdl;
	state->dirlist_len++;

	interpret_long_filename(state->data_level, file, &state->dirlist[state->total_received]);

	state->last_name = state->dirlist[state->total_received].name;
	state->total_received++;
	state->ff_searchcount++;
	
	return true;
}

int smbcli_list_new(struct smbcli_tree *tree, const char *Mask, uint16_t attribute, 
		    enum smb_search_data_level level,
		    void (*fn)(struct clilist_file_info *, const char *, void *), 
		    void *caller_state)
{
	union smb_search_first first_parms;
	union smb_search_next next_parms;
	struct search_private state;  /* for callbacks */
	int received = 0;
	bool first = true;
	int num_received = 0;
	int max_matches = 512;
	char *mask;
	int ff_eos = 0, i, ff_searchcount;
	int ff_dir_handle=0;

	/* initialize state for search */
	state.mem_ctx = talloc_init("smbcli_list_new");
	state.dirlist_len = 0;
	state.total_received = 0;
	
	state.dirlist = talloc_array(state.mem_ctx, 
				     struct clilist_file_info, 0);
	mask = talloc_strdup(state.mem_ctx, Mask);

	if (level == RAW_SEARCH_DATA_GENERIC) {
		if (tree->session->transport->negotiate.capabilities & CAP_NT_SMBS) {
			level = RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO;
		} else {
			level = RAW_SEARCH_DATA_STANDARD;
		}
	}
	state.data_level = level;

	while (1) {
		state.ff_searchcount = 0;
		if (first) {
			NTSTATUS status;

			first_parms.t2ffirst.level = RAW_SEARCH_TRANS2;
			first_parms.t2ffirst.data_level = state.data_level;
			first_parms.t2ffirst.in.max_count = max_matches;
			first_parms.t2ffirst.in.search_attrib = attribute;
			first_parms.t2ffirst.in.pattern = mask;
			first_parms.t2ffirst.in.flags = FLAG_TRANS2_FIND_CLOSE_IF_END;
			first_parms.t2ffirst.in.storage_type = 0;
			
			status = smb_raw_search_first(tree, 
						      state.mem_ctx, &first_parms,
						      (void*)&state, smbcli_list_new_callback);
			if (!NT_STATUS_IS_OK(status)) {
				talloc_free(state.mem_ctx);
				return -1;
			}
		
			ff_dir_handle = first_parms.t2ffirst.out.handle;
			ff_searchcount = first_parms.t2ffirst.out.count;
			ff_eos = first_parms.t2ffirst.out.end_of_search;
			
			received = first_parms.t2ffirst.out.count;
			if (received <= 0) break;
			if (ff_eos) break;
			first = false;
		} else {
			NTSTATUS status;

			next_parms.t2fnext.level = RAW_SEARCH_TRANS2;
			next_parms.t2fnext.data_level = state.data_level;
			next_parms.t2fnext.in.max_count = max_matches;
			next_parms.t2fnext.in.last_name = state.last_name;
			next_parms.t2fnext.in.handle = ff_dir_handle;
			next_parms.t2fnext.in.resume_key = 0;
			next_parms.t2fnext.in.flags = FLAG_TRANS2_FIND_CLOSE_IF_END;
			
			status = smb_raw_search_next(tree, 
						     state.mem_ctx,
						     &next_parms,
						     (void*)&state, 
						     smbcli_list_new_callback);
			
			if (!NT_STATUS_IS_OK(status)) {
				return -1;
			}
			ff_searchcount = next_parms.t2fnext.out.count;
			ff_eos = next_parms.t2fnext.out.end_of_search;
			received = next_parms.t2fnext.out.count;
			if (received <= 0) break;
			if (ff_eos) break;
		}
		
		num_received += received;
	}

	for (i=0;i<state.total_received;i++) {
		fn(&state.dirlist[i], Mask, caller_state);
	}

	talloc_free(state.mem_ctx);

	return state.total_received;
}

/****************************************************************************
 Interpret a short filename structure.
 The length of the structure is returned.
****************************************************************************/
static bool interpret_short_filename(enum smb_search_data_level level,
				     const union smb_search_data *info,
				     struct clilist_file_info *finfo)
{
	struct clilist_file_info finfo2;

	if (!finfo) finfo = &finfo2;
	ZERO_STRUCTP(finfo);

	switch (level) {
	case RAW_SEARCH_DATA_SEARCH:
		finfo->mtime = info->search.write_time;
		finfo->size = info->search.size;
		finfo->attrib = info->search.attrib;
		finfo->name = info->search.name;
		finfo->short_name = info->search.name;
		break;

	default:
		DEBUG(0,("Unhandled level %d in interpret_short_filename\n", (int)level));
		return false;
	}
	
	return true;
}

/* callback function used for smb_search */
static bool smbcli_list_old_callback(void *private_data, const union smb_search_data *file)
{
	struct search_private *state = (struct search_private*) private_data;
	struct clilist_file_info *tdl;
	
	/* add file info to the dirlist pool */
	tdl = talloc_realloc(state,
			     state->dirlist,
			     struct clilist_file_info,
			     state->dirlist_len + 1);

	if (!tdl) {
		return false;
	}
	state->dirlist = tdl;
	state->dirlist_len++;

	interpret_short_filename(state->data_level, file, &state->dirlist[state->total_received]);

	state->total_received++;
	state->ff_searchcount++;
	state->id = file->search.id; /* return resume info */
	
	return true;
}

int smbcli_list_old(struct smbcli_tree *tree, const char *Mask, uint16_t attribute, 
		 void (*fn)(struct clilist_file_info *, const char *, void *), 
		 void *caller_state)
{
	union smb_search_first first_parms;
	union smb_search_next next_parms;
	struct search_private state;  /* for callbacks */
	const int num_asked = 500;
	int received = 0;
	bool first = true;
	int num_received = 0;
	char *mask;
	int i;

	/* initialize state for search */
	state.mem_ctx = talloc_init("smbcli_list_old");
	state.dirlist_len = 0;
	state.total_received = 0;
	state.data_level = RAW_SEARCH_DATA_SEARCH;

	state.dirlist = talloc_array(state.mem_ctx, struct clilist_file_info,
				     0);
	mask = talloc_strdup(state.mem_ctx, Mask);
  
	while (1) {
		state.ff_searchcount = 0;
		if (first) {
			NTSTATUS status;

			first_parms.search_first.level = RAW_SEARCH_SEARCH;
			first_parms.search_first.data_level = RAW_SEARCH_DATA_SEARCH;
			first_parms.search_first.in.max_count = num_asked;
			first_parms.search_first.in.search_attrib = attribute;
			first_parms.search_first.in.pattern = mask;
			
			status = smb_raw_search_first(tree, state.mem_ctx, 
						      &first_parms,
						      (void*)&state, 
						      smbcli_list_old_callback);

			if (!NT_STATUS_IS_OK(status)) {
				talloc_free(state.mem_ctx);
				return -1;
			}
		
			received = first_parms.search_first.out.count;
			if (received <= 0) break;
			first = false;
		} else {
			NTSTATUS status;

			next_parms.search_next.level = RAW_SEARCH_SEARCH;
			next_parms.search_next.data_level = RAW_SEARCH_DATA_SEARCH;
			next_parms.search_next.in.max_count = num_asked;
			next_parms.search_next.in.search_attrib = attribute;
			next_parms.search_next.in.id = state.id;
			
			status = smb_raw_search_next(tree, state.mem_ctx,
						     &next_parms,
						     (void*)&state, 
						     smbcli_list_old_callback);

			if (NT_STATUS_EQUAL(status, STATUS_NO_MORE_FILES)) {
				break;
			}
			if (!NT_STATUS_IS_OK(status)) {
				talloc_free(state.mem_ctx);
				return -1;
			}
			received = next_parms.search_next.out.count;
			if (received <= 0) break;
		}
		
		num_received += received;
	}

	for (i=0;i<state.total_received;i++) {
		fn(&state.dirlist[i], Mask, caller_state);
	}

	talloc_free(state.mem_ctx);

	return state.total_received;
}

/****************************************************************************
 Do a directory listing, calling fn on each file found.
 This auto-switches between old and new style.
****************************************************************************/

int smbcli_list(struct smbcli_tree *tree, const char *Mask,uint16_t attribute, 
		void (*fn)(struct clilist_file_info *, const char *, void *), void *state)
{
	if (tree->session->transport->negotiate.protocol <= PROTOCOL_LANMAN1)
		return smbcli_list_old(tree, Mask, attribute, fn, state);
	return smbcli_list_new(tree, Mask, attribute, RAW_SEARCH_DATA_GENERIC, fn, state);
}

/* 
   Unix SMB/CIFS implementation.
   
   Test LDB attribute functions
   
   Copyright (C) Andrew Bartlet <abartlet@samba.org> 2008-2009
   Copyright (C) Matthieu Patou <mat@matws.net> 2009
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.	If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "lib/events/events.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "ldb_wrap.h"
#include "param/param.h"
#include "lib/cmdline/popt_common.h" 
#include "torture/smbtorture.h"
#include "torture/local/proto.h"
#include <ctype.h>

bool torture_ldap_sort(struct torture_context *torture)
{
	struct ldb_context *ldb;

	bool ret = false;
	const char *host = torture_setting_string(torture, "host", NULL);
	char *url;
	int i;
	codepoint_t j;
	struct ldb_message_element *elem;
	struct ldb_message *msg;

	struct ldb_server_sort_control **control;
	struct ldb_request *req;
	struct ldb_result *ctx;
	struct ldb_val *prev = NULL;
	const char *prev_txt = NULL;
	int prev_len = 0;
	struct ldb_val *cur = NULL;
	const char *cur_txt = NULL;
	int cur_len = 0;
	struct ldb_dn *dn;
		 
		 
	/* TALLOC_CTX* ctx;*/

	url = talloc_asprintf(torture, "ldap://%s/", host);

	ldb = ldb_wrap_connect(torture, torture->ev, torture->lp_ctx, url,
						 NULL,
						 cmdline_credentials,
						 0);
	torture_assert(torture, ldb, "Failed to make LDB connection to target");

	ctx = talloc_zero(ldb, struct ldb_result);

	control = talloc_array(ctx, struct ldb_server_sort_control *, 2);
	control[0] = talloc(control, struct ldb_server_sort_control);
	control[0]->attributeName = talloc_strdup(control, "cn");
	control[0]->orderingRule = NULL;
	control[0]->reverse = 0;
	control[1] = NULL;

	dn = ldb_get_default_basedn(ldb);
	ldb_dn_add_child_fmt(dn, "cn=users");
	ret = ldb_build_search_req(&req, ldb, ctx,
				   dn,
				   LDB_SCOPE_SUBTREE,
				   "(objectClass=*)", NULL,
				   NULL,
				   ctx, ldb_search_default_callback, NULL);
	torture_assert(torture, ret == LDB_SUCCESS, "Failed to build search request");

	ret = ldb_request_add_control(req, LDB_CONTROL_SERVER_SORT_OID, true, control);
	torture_assert(torture, ret == LDB_SUCCESS, "Failed to add control to search request");

	ret = ldb_request(ldb, req);
	torture_assert(torture, ret == LDB_SUCCESS, ldb_errstring(ldb));

	ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	torture_assert(torture, ret == LDB_SUCCESS, ldb_errstring(ldb));

	ret = true;
	if (ctx->count > 1) {
		for (i=0;i<ctx->count;i++) {
			msg = ctx->msgs[i];
			elem = ldb_msg_find_element(msg,"cn");
			cur = elem->values;
			torture_comment(torture, "cn: %s\n",cur->data);
			if (prev != NULL)
			{
				/* Do only the ascii case right now ... */
				cur_txt = (const char *) cur->data;
				cur_len = cur->length;
				prev_txt = (const char *) prev->data;
				prev_len = prev->length;
				/* Remove leading whitespace as the sort function do so ... */
				while ( cur_txt[0] == cur_txt[1] ) { cur_txt++; cur_len--;}
				while ( prev_txt[0] == prev_txt[1] ) { prev_txt++; prev_len--;}
				while( *(cur_txt) && *(prev_txt) && cur_len && prev_len ) {
					j = toupper_m(*(prev_txt))-toupper_m(*(cur_txt));
					if ( j > 0 ) {
						/* Just check that is not due to trailling white space in prev_txt 
						 * That is to say *cur_txt = 0 and prev_txt = 20 */
						/* Remove trailling whitespace */
						while ( *prev_txt == ' ' ) { prev_txt++; prev_len--;}
						while ( *cur_txt == ' ' ) { cur_txt++; cur_len--;}
						/* Now that potential whitespace are removed if we are at the end 
						 * of the cur_txt then it means that in fact strings were identical
						 */
						torture_assert(torture, *cur_txt && *prev_txt, "Data wrongly sorted");
						break;
					}
					else
					{
						if ( j == 0 )
						{
							if ( *(cur_txt) == ' ') {
								while ( cur_txt[0] == cur_txt[1] ) { cur_txt++; cur_len--;}
								while ( prev_txt[0] == prev_txt[1] ) { prev_txt++; prev_len--;}
							}
							cur_txt++;
							prev_txt++;
							prev_len--;
							cur_len--;
						}
						else
						{
							break;
						} 
					}
				}
				if ( ret != 1 ) {
					break;
				}
			}
			prev = cur; 
		}

	}

	return ret;
}

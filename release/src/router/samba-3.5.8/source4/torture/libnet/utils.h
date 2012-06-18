/* 
   Unix SMB/CIFS implementation.
   Test suite for libnet calls.

   Copyright (C) Rafal Szczesniak 2007
   
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


bool test_opendomain(struct torture_context *tctx, 
		     struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
		     struct policy_handle *handle, struct lsa_String *domname,
		     struct dom_sid2 *sid);

bool test_user_create(struct torture_context *tctx, 
		      struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
		      struct policy_handle *handle, const char *name,
		      uint32_t *rid);

bool test_user_cleanup(struct torture_context *tctx, 
		       struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
		       struct policy_handle *domain_handle,
		       const char *name);

bool test_group_create(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
		       struct policy_handle *handle, const char *name,
		       uint32_t *rid);

bool test_group_cleanup(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			struct policy_handle *domain_handle,
			const char *name);

void msg_handler(struct monitor_msg *m);

/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Rafal Szczesniak 2005
   
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

/*
  a composite functions for user management operations (add/del/chg)
*/

#include "includes.h"
#include "libcli/composite/composite.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_samr_c.h"

/*
 * Composite USER ADD functionality
 */

struct useradd_state {
	struct dcerpc_pipe       *pipe;
	struct policy_handle     domain_handle;
	struct samr_CreateUser   createuser;
	struct policy_handle     user_handle;
	uint32_t                 user_rid;

	/* information about the progress */
	void (*monitor_fn)(struct monitor_msg *);
};


static void continue_useradd_create(struct tevent_req *subreq);


/**
 * Stage 1 (and the only one for now): Create user account.
 */
static void continue_useradd_create(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct useradd_state *s;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct useradd_state);

	/* check rpc layer status code */
	c->status = dcerpc_samr_CreateUser_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	/* check create user call status code */
	c->status = s->createuser.out.result;

	/* get created user account data */
	s->user_handle = *s->createuser.out.user_handle;
	s->user_rid    = *s->createuser.out.rid;

	/* issue a monitor message */
	if (s->monitor_fn) {
		struct monitor_msg msg;
		struct msg_rpc_create_user rpc_create;

		rpc_create.rid = *s->createuser.out.rid;

		msg.type      = mon_SamrCreateUser;
		msg.data      = (void*)&rpc_create;
		msg.data_size = sizeof(rpc_create);
		
		s->monitor_fn(&msg);
	}
	
	composite_done(c);
}


/**
 * Sends asynchronous useradd request
 *
 * @param p dce/rpc call pipe 
 * @param io arguments and results of the call
 * @param monitor monitor function for providing information about the progress
 */

struct composite_context *libnet_rpc_useradd_send(struct dcerpc_pipe *p,
						  struct libnet_rpc_useradd *io,
						  void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct useradd_state *s;
	struct tevent_req *subreq;

	if (!p || !io) return NULL;

	/* composite allocation and setup */
	c = composite_create(p, dcerpc_event_context(p));
	if (c == NULL) return NULL;
	
	s = talloc_zero(c, struct useradd_state);
	if (composite_nomem(s, c)) return c;
	
	c->private_data = s;

	/* put passed arguments to the state structure */
	s->domain_handle = io->in.domain_handle;
	s->pipe          = p;
	s->monitor_fn    = monitor;
	
	/* preparing parameters to send rpc request */
	s->createuser.in.domain_handle         = &io->in.domain_handle;

	s->createuser.in.account_name          = talloc_zero(c, struct lsa_String);
	if (composite_nomem(s->createuser.in.account_name, c)) return c;

	s->createuser.in.account_name->string  = talloc_strdup(c, io->in.username);
	if (composite_nomem(s->createuser.in.account_name->string, c)) return c;

	s->createuser.out.user_handle          = &s->user_handle;
	s->createuser.out.rid                  = &s->user_rid;

	/* send the request */
	subreq = dcerpc_samr_CreateUser_r_send(s, c->event_ctx,
					       p->binding_handle,
					       &s->createuser);
	if (composite_nomem(subreq, c)) return c;

	tevent_req_set_callback(subreq, continue_useradd_create, c);
	return c;
}


/**
 * Waits for and receives result of asynchronous useradd call
 * 
 * @param c composite context returned by asynchronous useradd call
 * @param mem_ctx memory context of the call
 * @param io pointer to results (and arguments) of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_rpc_useradd_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
				 struct libnet_rpc_useradd *io)
{
	NTSTATUS status;
	struct useradd_state *s;
	
	status = composite_wait(c);
	
	if (NT_STATUS_IS_OK(status) && io) {
		/* get and return result of the call */
		s = talloc_get_type(c->private_data, struct useradd_state);
		io->out.user_handle = s->user_handle;
	}

	talloc_free(c);
	return status;
}


/**
 * Synchronous version of useradd call
 *
 * @param pipe dce/rpc call pipe
 * @param mem_ctx memory context for the call
 * @param io arguments and results of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_rpc_useradd(struct dcerpc_pipe *p,
			    TALLOC_CTX *mem_ctx,
			    struct libnet_rpc_useradd *io)
{
	struct composite_context *c = libnet_rpc_useradd_send(p, io, NULL);
	return libnet_rpc_useradd_recv(c, mem_ctx, io);
}



/*
 * Composite USER DELETE functionality
 */


struct userdel_state {
	struct dcerpc_pipe        *pipe;
	struct policy_handle      domain_handle;
	struct policy_handle      user_handle;
	struct samr_LookupNames   lookupname;
	struct samr_OpenUser      openuser;
	struct samr_DeleteUser    deleteuser;

	/* information about the progress */
	void (*monitor_fn)(struct monitor_msg *);
};


static void continue_userdel_name_found(struct tevent_req *subreq);
static void continue_userdel_user_opened(struct tevent_req *subreq);
static void continue_userdel_deleted(struct tevent_req *subreq);


/**
 * Stage 1: Lookup the user name and resolve it to rid
 */
static void continue_userdel_name_found(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct userdel_state *s;
	struct monitor_msg msg;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct userdel_state);

	/* receive samr_LookupNames result */
	c->status = dcerpc_samr_LookupNames_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	c->status = s->lookupname.out.result;
	if (!NT_STATUS_IS_OK(c->status)) {
		composite_error(c, c->status);
		return;
	}

	/* what to do when there's no user account to delete
	   and what if there's more than one rid resolved */
	if (s->lookupname.out.rids->count != s->lookupname.in.num_names) {
		composite_error(c, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}
	if (s->lookupname.out.types->count != s->lookupname.in.num_names) {
		composite_error(c, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	/* issue a monitor message */
	if (s->monitor_fn) {
		struct msg_rpc_lookup_name msg_lookup;

		msg_lookup.rid   = s->lookupname.out.rids->ids;
		msg_lookup.count = s->lookupname.out.rids->count;

		msg.type      = mon_SamrLookupName;
		msg.data      = (void*)&msg_lookup;
		msg.data_size = sizeof(msg_lookup);
		s->monitor_fn(&msg);
	}

	/* prepare the arguments for rpc call */
	s->openuser.in.domain_handle = &s->domain_handle;
	s->openuser.in.rid           = s->lookupname.out.rids->ids[0];
	s->openuser.in.access_mask   = SEC_FLAG_MAXIMUM_ALLOWED;
	s->openuser.out.user_handle  = &s->user_handle;

	/* send rpc request */
	subreq = dcerpc_samr_OpenUser_r_send(s, c->event_ctx,
					     s->pipe->binding_handle,
					     &s->openuser);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_userdel_user_opened, c);
}


/**
 * Stage 2: Open user account.
 */
static void continue_userdel_user_opened(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct userdel_state *s;
	struct monitor_msg msg;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct userdel_state);

	/* receive samr_OpenUser result */
	c->status = dcerpc_samr_OpenUser_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	c->status = s->openuser.out.result;
	if (!NT_STATUS_IS_OK(c->status)) {
		composite_error(c, c->status);
		return;
	}
	
	/* issue a monitor message */
	if (s->monitor_fn) {
		struct msg_rpc_open_user msg_open;

		msg_open.rid         = s->openuser.in.rid;
		msg_open.access_mask = s->openuser.in.access_mask;

		msg.type      = mon_SamrOpenUser;
		msg.data      = (void*)&msg_open;
		msg.data_size = sizeof(msg_open);
		s->monitor_fn(&msg);
	}

	/* prepare the final rpc call arguments */
	s->deleteuser.in.user_handle   = &s->user_handle;
	s->deleteuser.out.user_handle  = &s->user_handle;
	
	/* send rpc request */
	subreq = dcerpc_samr_DeleteUser_r_send(s, c->event_ctx,
					       s->pipe->binding_handle,
					       &s->deleteuser);
	if (composite_nomem(subreq, c)) return;

	/* callback handler setup */
	tevent_req_set_callback(subreq, continue_userdel_deleted, c);
}


/**
 * Stage 3: Delete user account
 */
static void continue_userdel_deleted(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct userdel_state *s;
	struct monitor_msg msg;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct userdel_state);

	/* receive samr_DeleteUser result */
	c->status = dcerpc_samr_DeleteUser_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	/* return the actual function call status */
	c->status = s->deleteuser.out.result;
	if (!NT_STATUS_IS_OK(c->status)) {
		composite_error(c, c->status);
		return;
	}
	
	/* issue a monitor message */
	if (s->monitor_fn) {
		msg.type      = mon_SamrDeleteUser;
		msg.data      = NULL;
		msg.data_size = 0;
		s->monitor_fn(&msg);
	}

	composite_done(c);
}


/**
 * Sends asynchronous userdel request
 *
 * @param p dce/rpc call pipe
 * @param io arguments and results of the call
 * @param monitor monitor function for providing information about the progress
 */

struct composite_context *libnet_rpc_userdel_send(struct dcerpc_pipe *p,
						  struct libnet_rpc_userdel *io,
						  void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct userdel_state *s;
	struct tevent_req *subreq;

	/* composite context allocation and setup */
	c = composite_create(p, dcerpc_event_context(p));
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct userdel_state);
	if (composite_nomem(s, c)) return c;

	c->private_data  = s;

	/* store function parameters in the state structure */
	s->pipe          = p;
	s->domain_handle = io->in.domain_handle;
	s->monitor_fn    = monitor;
	
	/* preparing parameters to send rpc request */
	s->lookupname.in.domain_handle = &io->in.domain_handle;
	s->lookupname.in.num_names     = 1;
	s->lookupname.in.names         = talloc_zero(s, struct lsa_String);
	s->lookupname.in.names->string = io->in.username;
	s->lookupname.out.rids         = talloc_zero(s, struct samr_Ids);
	s->lookupname.out.types        = talloc_zero(s, struct samr_Ids);
	if (composite_nomem(s->lookupname.out.rids, c)) return c;
	if (composite_nomem(s->lookupname.out.types, c)) return c;

	/* send the request */
	subreq = dcerpc_samr_LookupNames_r_send(s, c->event_ctx,
						p->binding_handle,
						&s->lookupname);
	if (composite_nomem(subreq, c)) return c;

	/* set the next stage */
	tevent_req_set_callback(subreq, continue_userdel_name_found, c);
	return c;
}


/**
 * Waits for and receives results of asynchronous userdel call
 *
 * @param c composite context returned by asynchronous userdel call
 * @param mem_ctx memory context of the call
 * @param io pointer to results (and arguments) of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_rpc_userdel_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
				 struct libnet_rpc_userdel *io)
{
	NTSTATUS status;
	struct userdel_state *s;
	
	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status) && io) {
		s  = talloc_get_type(c->private_data, struct userdel_state);
		io->out.user_handle = s->user_handle;
	}

	talloc_free(c);
	return status;
}


/**
 * Synchronous version of userdel call
 *
 * @param pipe dce/rpc call pipe
 * @param mem_ctx memory context for the call
 * @param io arguments and results of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_rpc_userdel(struct dcerpc_pipe *p,
			    TALLOC_CTX *mem_ctx,
			    struct libnet_rpc_userdel *io)
{
	struct composite_context *c = libnet_rpc_userdel_send(p, io, NULL);
	return libnet_rpc_userdel_recv(c, mem_ctx, io);
}


/*
 * USER MODIFY functionality
 */

static void continue_usermod_name_found(struct tevent_req *subreq);
static void continue_usermod_user_opened(struct tevent_req *subreq);
static void continue_usermod_user_queried(struct tevent_req *subreq);
static void continue_usermod_user_changed(struct tevent_req *subreq);


struct usermod_state {
	struct dcerpc_pipe         *pipe;
	struct policy_handle       domain_handle;
	struct policy_handle       user_handle;
	struct usermod_change      change;
	union  samr_UserInfo       info;
	struct samr_LookupNames    lookupname;
	struct samr_OpenUser       openuser;
	struct samr_SetUserInfo    setuser;
	struct samr_QueryUserInfo  queryuser;

	/* information about the progress */
	void (*monitor_fn)(struct monitor_msg *);
};


/**
 * Step 1: Lookup user name
 */
static void continue_usermod_name_found(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct usermod_state *s;
	struct monitor_msg msg;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct usermod_state);

	/* receive samr_LookupNames result */
	c->status = dcerpc_samr_LookupNames_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	c->status = s->lookupname.out.result;
	if (!NT_STATUS_IS_OK(c->status)) {
		composite_error(c, c->status);
		return;
	}

	/* what to do when there's no user account to delete
	   and what if there's more than one rid resolved */
	if (s->lookupname.out.rids->count != s->lookupname.in.num_names) {
		composite_error(c, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}
	if (s->lookupname.out.types->count != s->lookupname.in.num_names) {
		composite_error(c, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	/* issue a monitor message */
	if (s->monitor_fn) {
		struct msg_rpc_lookup_name msg_lookup;

		msg_lookup.rid   = s->lookupname.out.rids->ids;
		msg_lookup.count = s->lookupname.out.rids->count;

		msg.type      = mon_SamrLookupName;
		msg.data      = (void*)&msg_lookup;
		msg.data_size = sizeof(msg_lookup);
		s->monitor_fn(&msg);
	}

	/* prepare the next rpc call */
	s->openuser.in.domain_handle = &s->domain_handle;
	s->openuser.in.rid           = s->lookupname.out.rids->ids[0];
	s->openuser.in.access_mask   = SEC_FLAG_MAXIMUM_ALLOWED;
	s->openuser.out.user_handle  = &s->user_handle;

	/* send the rpc request */
	subreq = dcerpc_samr_OpenUser_r_send(s, c->event_ctx,
					     s->pipe->binding_handle,
					     &s->openuser);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_usermod_user_opened, c);
}


/**
 * Choose a proper level of samr_UserInfo structure depending on required
 * change specified by means of flags field. Subsequent calls of this
 * function are made until there's no flags set meaning that all of the
 * changes have been made.
 */
static bool usermod_setfields(struct usermod_state *s, uint16_t *level,
			      union samr_UserInfo *i, bool queried)
{
	if (s->change.fields == 0) return s->change.fields;

	*level = 0;

	if ((s->change.fields & USERMOD_FIELD_ACCOUNT_NAME) &&
	    (*level == 0 || *level == 7)) {
		*level = 7;
		i->info7.account_name.string = s->change.account_name;
		
		s->change.fields ^= USERMOD_FIELD_ACCOUNT_NAME;
	}

	if ((s->change.fields & USERMOD_FIELD_FULL_NAME) &&
	    (*level == 0 || *level == 8)) {
		*level = 8;
		i->info8.full_name.string = s->change.full_name;
		
		s->change.fields ^= USERMOD_FIELD_FULL_NAME;
	}
	
	if ((s->change.fields & USERMOD_FIELD_DESCRIPTION) &&
	    (*level == 0 || *level == 13)) {
		*level = 13;
		i->info13.description.string = s->change.description;
		
		s->change.fields ^= USERMOD_FIELD_DESCRIPTION;		
	}

	if ((s->change.fields & USERMOD_FIELD_COMMENT) &&
	    (*level == 0 || *level == 2)) {
		*level = 2;
		
		if (queried) {
			/* the user info is obtained, so now set the required field */
			i->info2.comment.string = s->change.comment;
			s->change.fields ^= USERMOD_FIELD_COMMENT;
			
		} else {
			/* we need to query the user info before setting one field in it */
			return false;
		}
	}

	if ((s->change.fields & USERMOD_FIELD_LOGON_SCRIPT) &&
	    (*level == 0 || *level == 11)) {
		*level = 11;
		i->info11.logon_script.string = s->change.logon_script;
		
		s->change.fields ^= USERMOD_FIELD_LOGON_SCRIPT;
	}

	if ((s->change.fields & USERMOD_FIELD_PROFILE_PATH) &&
	    (*level == 0 || *level == 12)) {
		*level = 12;
		i->info12.profile_path.string = s->change.profile_path;
		
		s->change.fields ^= USERMOD_FIELD_PROFILE_PATH;
	}

	if ((s->change.fields & USERMOD_FIELD_HOME_DIRECTORY) &&
	    (*level == 0 || *level == 10)) {
		*level = 10;
		
		if (queried) {
			i->info10.home_directory.string = s->change.home_directory;
			s->change.fields ^= USERMOD_FIELD_HOME_DIRECTORY;
		} else {
			return false;
		}
	}

	if ((s->change.fields & USERMOD_FIELD_HOME_DRIVE) &&
	    (*level == 0 || *level == 10)) {
		*level = 10;
		
		if (queried) {
			i->info10.home_drive.string = s->change.home_drive;
			s->change.fields ^= USERMOD_FIELD_HOME_DRIVE;
		} else {
			return false;
		}
	}
	
	if ((s->change.fields & USERMOD_FIELD_ACCT_EXPIRY) &&
	    (*level == 0 || *level == 17)) {
		*level = 17;
		i->info17.acct_expiry = timeval_to_nttime(s->change.acct_expiry);
		
		s->change.fields ^= USERMOD_FIELD_ACCT_EXPIRY;
	}

	if ((s->change.fields & USERMOD_FIELD_ACCT_FLAGS) &&
	    (*level == 0 || *level == 16)) {
		*level = 16;
		i->info16.acct_flags = s->change.acct_flags;
		
		s->change.fields ^= USERMOD_FIELD_ACCT_FLAGS;
	}

	/* We're going to be here back again soon unless all fields have been set */
	return true;
}


static NTSTATUS usermod_change(struct composite_context *c,
			       struct usermod_state *s)
{
	bool do_set;
	union samr_UserInfo *i = &s->info;
	struct tevent_req *subreq;

	/* set the level to invalid value, so that unless setfields routine 
	   gives it a valid value we report the error correctly */
	uint16_t level = 27;

	/* prepare UserInfo level and data based on bitmask field */
	do_set = usermod_setfields(s, &level, i, false);

	if (level < 1 || level > 26) {
		/* apparently there's a field that the setfields routine
		   does not know how to set */
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* If some specific level is used to set user account data and the change
	   itself does not cover all fields then we need to query the user info
	   first, right before changing the data. Otherwise we could set required
	   fields and accidentally reset the others.
	*/
	if (!do_set) {
		s->queryuser.in.user_handle = &s->user_handle;
		s->queryuser.in.level       = level;
		s->queryuser.out.info       = talloc(s, union samr_UserInfo *);
		if (composite_nomem(s->queryuser.out.info, c)) return NT_STATUS_NO_MEMORY;


		/* send query user info request to retrieve complete data of
		   a particular info level */
		subreq = dcerpc_samr_QueryUserInfo_r_send(s, c->event_ctx,
							  s->pipe->binding_handle,
							  &s->queryuser);
		if (composite_nomem(subreq, c)) return NT_STATUS_NO_MEMORY;
		tevent_req_set_callback(subreq, continue_usermod_user_queried, c);

	} else {
		s->setuser.in.user_handle  = &s->user_handle;
		s->setuser.in.level        = level;
		s->setuser.in.info         = i;

		/* send set user info request after making required change */
		subreq = dcerpc_samr_SetUserInfo_r_send(s, c->event_ctx,
							s->pipe->binding_handle,
							&s->setuser);
		if (composite_nomem(subreq, c)) return NT_STATUS_NO_MEMORY;
		tevent_req_set_callback(subreq, continue_usermod_user_changed, c);
	}
	
	return NT_STATUS_OK;
}


/**
 * Stage 2: Open user account
 */
static void continue_usermod_user_opened(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct usermod_state *s;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct usermod_state);

	c->status = dcerpc_samr_OpenUser_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	c->status = s->openuser.out.result;
	if (!NT_STATUS_IS_OK(c->status)) {
		composite_error(c, c->status);
		return;
	}

	c->status = usermod_change(c, s);
}


/**
 * Stage 2a (optional): Query the user information
 */
static void continue_usermod_user_queried(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct usermod_state *s;
	union samr_UserInfo *i;
	uint16_t level;
	
	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct usermod_state);

	i = &s->info;

	/* receive samr_QueryUserInfo result */
	c->status = dcerpc_samr_QueryUserInfo_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	c->status = s->queryuser.out.result;
	if (!NT_STATUS_IS_OK(c->status)) {
		composite_error(c, c->status);
		return;
	}

	/* get returned user data and make a change (potentially one
	   of many) */
	s->info = *(*s->queryuser.out.info);

	usermod_setfields(s, &level, i, true);

	/* prepare rpc call arguments */
	s->setuser.in.user_handle  = &s->user_handle;
	s->setuser.in.level        = level;
	s->setuser.in.info         = i;

	/* send the rpc request */
	subreq = dcerpc_samr_SetUserInfo_r_send(s, c->event_ctx,
						s->pipe->binding_handle,
						&s->setuser);
	if (composite_nomem(subreq, c)) return;
	tevent_req_set_callback(subreq, continue_usermod_user_changed, c);
}


/**
 * Stage 3: Set new user account data
 */
static void continue_usermod_user_changed(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct usermod_state *s;
	
	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct usermod_state);

	/* receive samr_SetUserInfo result */
	c->status = dcerpc_samr_SetUserInfo_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	/* return the actual function call status */
	c->status = s->setuser.out.result;
	if (!NT_STATUS_IS_OK(c->status)) {
		composite_error(c, c->status);
		return;
	}

	if (s->change.fields == 0) {
		/* all fields have been set - we're done */
		composite_done(c);

	} else {
		/* something's still not changed - repeat the procedure */
		c->status = usermod_change(c, s);
	}
}


/**
 * Sends asynchronous usermod request
 *
 * @param p dce/rpc call pipe
 * @param io arguments and results of the call
 * @param monitor monitor function for providing information about the progress
 */

struct composite_context *libnet_rpc_usermod_send(struct dcerpc_pipe *p,
						  struct libnet_rpc_usermod *io,
						  void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct usermod_state *s;
	struct tevent_req *subreq;

	/* composite context allocation and setup */
	c = composite_create(p, dcerpc_event_context(p));
	if (c == NULL) return NULL;
	s = talloc_zero(c, struct usermod_state);
	if (composite_nomem(s, c)) return c;

	c->private_data = s;

	/* store parameters in the call structure */
	s->pipe          = p;
	s->domain_handle = io->in.domain_handle;
	s->change        = io->in.change;
	s->monitor_fn    = monitor;
	
	/* prepare rpc call arguments */
	s->lookupname.in.domain_handle = &io->in.domain_handle;
	s->lookupname.in.num_names     = 1;
	s->lookupname.in.names         = talloc_zero(s, struct lsa_String);
	s->lookupname.in.names->string = io->in.username;
	s->lookupname.out.rids         = talloc_zero(s, struct samr_Ids);
	s->lookupname.out.types        = talloc_zero(s, struct samr_Ids);
	if (composite_nomem(s->lookupname.out.rids, c)) return c;
	if (composite_nomem(s->lookupname.out.types, c)) return c;

	/* send the rpc request */
	subreq = dcerpc_samr_LookupNames_r_send(s, c->event_ctx,
						p->binding_handle,
						&s->lookupname);
	if (composite_nomem(subreq, c)) return c;
	
	/* callback handler setup */
	tevent_req_set_callback(subreq, continue_usermod_name_found, c);
	return c;
}


/**
 * Waits for and receives results of asynchronous usermod call
 *
 * @param c composite context returned by asynchronous usermod call
 * @param mem_ctx memory context of the call
 * @param io pointer to results (and arguments) of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_rpc_usermod_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
				 struct libnet_rpc_usermod *io)
{
	NTSTATUS status;
	
	status = composite_wait(c);

	talloc_free(c);
	return status;
}


/**
 * Synchronous version of usermod call
 *
 * @param pipe dce/rpc call pipe
 * @param mem_ctx memory context for the call
 * @param io arguments and results of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_rpc_usermod(struct dcerpc_pipe *p,
			    TALLOC_CTX *mem_ctx,
			    struct libnet_rpc_usermod *io)
{
	struct composite_context *c = libnet_rpc_usermod_send(p, io, NULL);
	return libnet_rpc_usermod_recv(c, mem_ctx, io);
}

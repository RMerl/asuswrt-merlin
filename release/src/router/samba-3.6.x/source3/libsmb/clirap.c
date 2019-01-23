/*
   Unix SMB/CIFS implementation.
   client RAP calls
   Copyright (C) Andrew Tridgell         1994-1998
   Copyright (C) Gerald (Jerry) Carter   2004
   Copyright (C) James Peach		 2007

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
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/rap.h"
#include "../lib/crypto/arcfour.h"
#include "../lib/util/tevent_ntstatus.h"
#include "async_smb.h"
#include "libsmb/libsmb.h"
#include "libsmb/clirap.h"
#include "trans2.h"

#define PIPE_LANMAN   "\\PIPE\\LANMAN"

/****************************************************************************
 Call a remote api
****************************************************************************/

bool cli_api(struct cli_state *cli,
	     char *param, int prcnt, int mprcnt,
	     char *data, int drcnt, int mdrcnt,
	     char **rparam, unsigned int *rprcnt,
	     char **rdata, unsigned int *rdrcnt)
{
	NTSTATUS status;

	uint8_t *my_rparam, *my_rdata;
	uint32_t num_my_rparam, num_my_rdata;

	status = cli_trans(talloc_tos(), cli, SMBtrans,
			   PIPE_LANMAN, 0, /* name, fid */
			   0, 0,	   /* function, flags */
			   NULL, 0, 0,	   /* setup */
			   (uint8_t *)param, prcnt, mprcnt, /* Params, length, max */
			   (uint8_t *)data, drcnt, mdrcnt,  /* Data, length, max */
			   NULL,		 /* recv_flags2 */
			   NULL, 0, NULL,	 /* rsetup */
			   &my_rparam, 0, &num_my_rparam,
			   &my_rdata, 0, &num_my_rdata);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	/*
	 * I know this memcpy massively hurts, but there are just tons
	 * of callers of cli_api that eventually need changing to
	 * talloc
	 */

	*rparam = (char *)memdup(my_rparam, num_my_rparam);
	if (*rparam == NULL) {
		goto fail;
	}
	*rprcnt = num_my_rparam;
	TALLOC_FREE(my_rparam);

	*rdata = (char *)memdup(my_rdata, num_my_rdata);
	if (*rdata == NULL) {
		goto fail;
	}
	*rdrcnt = num_my_rdata;
	TALLOC_FREE(my_rdata);

	return true;
fail:
	TALLOC_FREE(my_rdata);
	TALLOC_FREE(my_rparam);
	*rparam = NULL;
	*rprcnt = 0;
	*rdata = NULL;
	*rdrcnt = 0;
	return false;
}

/****************************************************************************
 Perform a NetWkstaUserLogon.
****************************************************************************/

bool cli_NetWkstaUserLogon(struct cli_state *cli,char *user, char *workstation)
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	char param[1024];

	memset(param, 0, sizeof(param));

	/* send a SMBtrans command with api NetWkstaUserLogon */
	p = param;
	SSVAL(p,0,132); /* api number */
	p += 2;
	strlcpy(p,"OOWb54WrLh",sizeof(param)-PTR_DIFF(p,param));
	p = skip_string(param,sizeof(param),p);
	strlcpy(p,"WB21BWDWWDDDDDDDzzzD",sizeof(param)-PTR_DIFF(p,param));
	p = skip_string(param,sizeof(param),p);
	SSVAL(p,0,1);
	p += 2;
	strlcpy(p,user,sizeof(param)-PTR_DIFF(p,param));
	strupper_m(p);
	p += 21;
	p++;
	p += 15;
	p++;
	strlcpy(p, workstation,sizeof(param)-PTR_DIFF(p,param));
	strupper_m(p);
	p += 16;
	SSVAL(p, 0, CLI_BUFFER_SIZE);
	p += 2;
	SSVAL(p, 0, CLI_BUFFER_SIZE);
	p += 2;

	if (cli_api(cli,
                    param, PTR_DIFF(p,param),1024,  /* param, length, max */
                    NULL, 0, CLI_BUFFER_SIZE,           /* data, length, max */
                    &rparam, &rprcnt,               /* return params, return size */
                    &rdata, &rdrcnt                 /* return data, return size */
                   )) {
		cli->rap_error = rparam? SVAL(rparam,0) : -1;
		p = rdata;

		if (cli->rap_error == 0) {
			DEBUG(4,("NetWkstaUserLogon success\n"));
			cli->privileges = SVAL(p, 24);
			/* The cli->eff_name field used to be set here
	                   but it wasn't used anywhere else. */
		} else {
			DEBUG(1,("NetwkstaUserLogon gave error %d\n", cli->rap_error));
		}
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);
	return (cli->rap_error == 0);
}

/****************************************************************************
 Call a NetShareEnum - try and browse available connections on a host.
****************************************************************************/

int cli_RNetShareEnum(struct cli_state *cli, void (*fn)(const char *, uint32, const char *, void *), void *state)
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	char param[1024];
	int count = -1;

	/* now send a SMBtrans command with api RNetShareEnum */
	p = param;
	SSVAL(p,0,0); /* api number */
	p += 2;
	strlcpy(p,"WrLeh",sizeof(param)-PTR_DIFF(p,param));
	p = skip_string(param,sizeof(param),p);
	strlcpy(p,"B13BWz",sizeof(param)-PTR_DIFF(p,param));
	p = skip_string(param,sizeof(param),p);
	SSVAL(p,0,1);
	/*
	 * Win2k needs a *smaller* buffer than 0xFFFF here -
	 * it returns "out of server memory" with 0xFFFF !!! JRA.
	 */
	SSVAL(p,2,0xFFE0);
	p += 4;

	if (cli_api(cli,
		    param, PTR_DIFF(p,param), 1024,  /* Param, length, maxlen */
		    NULL, 0, 0xFFE0,            /* data, length, maxlen - Win2k needs a small buffer here too ! */
		    &rparam, &rprcnt,                /* return params, length */
		    &rdata, &rdrcnt))                /* return data, length */
		{
			int res = rparam? SVAL(rparam,0) : -1;

			if (res == 0 || res == ERRmoredata) {
				int converter=SVAL(rparam,2);
				int i;
				char *rdata_end = rdata + rdrcnt;

				count=SVAL(rparam,4);
				p = rdata;

				for (i=0;i<count;i++,p+=20) {
					char *sname;
					int type;
					int comment_offset;
					const char *cmnt;
					const char *p1;
					char *s1, *s2;
					size_t len;
					TALLOC_CTX *frame = talloc_stackframe();

					if (p + 20 > rdata_end) {
						TALLOC_FREE(frame);
						break;
					}

					sname = p;
					type = SVAL(p,14);
					comment_offset = (IVAL(p,16) & 0xFFFF) - converter;
					if (comment_offset < 0 ||
							comment_offset > (int)rdrcnt) {
						TALLOC_FREE(frame);
						break;
					}
					cmnt = comment_offset?(rdata+comment_offset):"";

					/* Work out the comment length. */
					for (p1 = cmnt, len = 0; *p1 &&
							p1 < rdata_end; len++)
						p1++;
					if (!*p1) {
						len++;
					}
					pull_string_talloc(frame,rdata,0,
						&s1,sname,14,STR_ASCII);
					pull_string_talloc(frame,rdata,0,
						&s2,cmnt,len,STR_ASCII);
					if (!s1 || !s2) {
						TALLOC_FREE(frame);
						continue;
					}

					fn(s1, type, s2, state);

					TALLOC_FREE(frame);
				}
			} else {
				DEBUG(4,("NetShareEnum res=%d\n", res));
			}
		} else {
			DEBUG(4,("NetShareEnum failed\n"));
		}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return count;
}

/****************************************************************************
 Call a NetServerEnum for the specified workgroup and servertype mask.  This
 function then calls the specified callback function for each name returned.

 The callback function takes 4 arguments: the machine name, the server type,
 the comment and a state pointer.
****************************************************************************/

bool cli_NetServerEnum(struct cli_state *cli, char *workgroup, uint32 stype,
		       void (*fn)(const char *, uint32, const char *, void *),
		       void *state)
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *rdata_end = NULL;
	unsigned int rdrcnt,rprcnt;
	char *p;
	char param[1024];
	int uLevel = 1;
	size_t len;
	uint32 func = RAP_NetServerEnum2;
	char *last_entry = NULL;
	int total_cnt = 0;
	int return_cnt = 0;
	int res;

	errno = 0; /* reset */

	/*
	 * This may take more than one transaction, so we should loop until
	 * we no longer get a more data to process or we have all of the
	 * items.
	 */
	do {
		/* send a SMBtrans command with api NetServerEnum */
	        p = param;
		SIVAL(p,0,func); /* api number */
	        p += 2;

		if (func == RAP_NetServerEnum3) {
			strlcpy(p,"WrLehDzz", sizeof(param)-PTR_DIFF(p,param));
		} else {
			strlcpy(p,"WrLehDz", sizeof(param)-PTR_DIFF(p,param));
		}

		p = skip_string(param, sizeof(param), p);
		strlcpy(p,"B16BBDz", sizeof(param)-PTR_DIFF(p,param));

		p = skip_string(param, sizeof(param), p);
		SSVAL(p,0,uLevel);
		SSVAL(p,2,CLI_BUFFER_SIZE);
		p += 4;
		SIVAL(p,0,stype);
		p += 4;

		/* If we have more data, tell the server where
		 * to continue from.
		 */
		len = push_ascii(p,
				workgroup,
				sizeof(param) - PTR_DIFF(p,param) - 1,
				STR_TERMINATE|STR_UPPER);

		if (len == 0) {
			SAFE_FREE(last_entry);
			return false;
		}
		p += len;

		if (func == RAP_NetServerEnum3) {
			len = push_ascii(p,
					last_entry ? last_entry : "",
					sizeof(param) - PTR_DIFF(p,param) - 1,
					STR_TERMINATE);

			if (len == 0) {
				SAFE_FREE(last_entry);
				return false;
			}
			p += len;
		}

		/* Next time through we need to use the continue api */
		func = RAP_NetServerEnum3;

		if (!cli_api(cli,
			param, PTR_DIFF(p,param), 8, /* params, length, max */
			NULL, 0, CLI_BUFFER_SIZE, /* data, length, max */
		            &rparam, &rprcnt, /* return params, return size */
		            &rdata, &rdrcnt)) { /* return data, return size */

			/* break out of the loop on error */
		        res = -1;
		        break;
		}

		rdata_end = rdata + rdrcnt;
		res = rparam ? SVAL(rparam,0) : -1;

		if (res == 0 || res == ERRmoredata ||
                    (res != -1 && cli_errno(cli) == 0)) {
			char *sname = NULL;
			int i, count;
			int converter=SVAL(rparam,2);

			/* Get the number of items returned in this buffer */
			count = SVAL(rparam, 4);

			/* The next field contains the number of items left,
			 * including those returned in this buffer. So the
			 * first time through this should contain all of the
			 * entries.
			 */
			if (total_cnt == 0) {
			        total_cnt = SVAL(rparam, 6);
			}

			/* Keep track of how many we have read */
			return_cnt += count;
			p = rdata;

			/* The last name in the previous NetServerEnum reply is
			 * sent back to server in the NetServerEnum3 request
			 * (last_entry). The next reply should repeat this entry
			 * as the first element. We have no proof that this is
			 * always true, but from traces that seems to be the
			 * behavior from Window Servers. So first lets do a lot
			 * of checking, just being paranoid. If the string
			 * matches then we already saw this entry so skip it.
			 *
			 * NOTE: sv1_name field must be null terminated and has
			 * a max size of 16 (NetBIOS Name).
			 */
			if (last_entry && count && p &&
				(strncmp(last_entry, p, 16) == 0)) {
			    count -= 1; /* Skip this entry */
			    return_cnt = -1; /* Not part of total, so don't count. */
			    p = rdata + 26; /* Skip the whole record */
			}

			for (i = 0; i < count; i++, p += 26) {
				int comment_offset;
				const char *cmnt;
				const char *p1;
				char *s1, *s2;
				TALLOC_CTX *frame = talloc_stackframe();
				uint32_t entry_stype;

				if (p + 26 > rdata_end) {
					TALLOC_FREE(frame);
					break;
				}

				sname = p;
				comment_offset = (IVAL(p,22) & 0xFFFF)-converter;
				cmnt = comment_offset?(rdata+comment_offset):"";

				if (comment_offset < 0 || comment_offset >= (int)rdrcnt) {
					TALLOC_FREE(frame);
					continue;
				}

				/* Work out the comment length. */
				for (p1 = cmnt, len = 0; *p1 &&
						p1 < rdata_end; len++)
					p1++;
				if (!*p1) {
					len++;
				}

				entry_stype = IVAL(p,18) & ~SV_TYPE_LOCAL_LIST_ONLY;

				pull_string_talloc(frame,rdata,0,
					&s1,sname,16,STR_ASCII);
				pull_string_talloc(frame,rdata,0,
					&s2,cmnt,len,STR_ASCII);

				if (!s1 || !s2) {
					TALLOC_FREE(frame);
					continue;
				}

				fn(s1, entry_stype, s2, state);
				TALLOC_FREE(frame);
			}

			/* We are done with the old last entry, so now we can free it */
			if (last_entry) {
			        SAFE_FREE(last_entry); /* This will set it to null */
			}

			/* We always make a copy of  the last entry if we have one */
			if (sname) {
			        last_entry = smb_xstrdup(sname);
			}

			/* If we have more data, but no last entry then error out */
			if (!last_entry && (res == ERRmoredata)) {
			        errno = EINVAL;
			        res = 0;
			}

		}

		SAFE_FREE(rparam);
		SAFE_FREE(rdata);
	} while ((res == ERRmoredata) && (total_cnt > return_cnt));

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);
	SAFE_FREE(last_entry);

	if (res == -1) {
		errno = cli_errno(cli);
	} else {
		if (!return_cnt) {
			/* this is a very special case, when the domain master for the
			   work group isn't part of the work group itself, there is something
			   wild going on */
			errno = ENOENT;
		}
	    }

	return(return_cnt > 0);
}

/****************************************************************************
 Send a SamOEMChangePassword command.
****************************************************************************/

bool cli_oem_change_password(struct cli_state *cli, const char *user, const char *new_password,
                             const char *old_password)
{
	char param[1024];
	unsigned char data[532];
	char *p = param;
	unsigned char old_pw_hash[16];
	unsigned char new_pw_hash[16];
	unsigned int data_len;
	unsigned int param_len = 0;
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rprcnt, rdrcnt;

	if (strlen(user) >= sizeof(fstring)-1) {
		DEBUG(0,("cli_oem_change_password: user name %s is too long.\n", user));
		return False;
	}

	SSVAL(p,0,214); /* SamOEMChangePassword command. */
	p += 2;
	strlcpy(p, "zsT", sizeof(param)-PTR_DIFF(p,param));
	p = skip_string(param,sizeof(param),p);
	strlcpy(p, "B516B16", sizeof(param)-PTR_DIFF(p,param));
	p = skip_string(param,sizeof(param),p);
	strlcpy(p,user, sizeof(param)-PTR_DIFF(p,param));
	p = skip_string(param,sizeof(param),p);
	SSVAL(p,0,532);
	p += 2;

	param_len = PTR_DIFF(p,param);

	/*
	 * Get the Lanman hash of the old password, we
	 * use this as the key to make_oem_passwd_hash().
	 */
	E_deshash(old_password, old_pw_hash);

	encode_pw_buffer(data, new_password, STR_ASCII);

#ifdef DEBUG_PASSWORD
	DEBUG(100,("make_oem_passwd_hash\n"));
	dump_data(100, data, 516);
#endif
	arcfour_crypt( (unsigned char *)data, (unsigned char *)old_pw_hash, 516);

	/*
	 * Now place the old password hash in the data.
	 */
	E_deshash(new_password, new_pw_hash);

	E_old_pw_hash( new_pw_hash, old_pw_hash, (uchar *)&data[516]);

	data_len = 532;

	if (!cli_api(cli,
		     param, param_len, 4,		/* param, length, max */
		     (char *)data, data_len, 0,		/* data, length, max */
		     &rparam, &rprcnt,
		     &rdata, &rdrcnt)) {
		DEBUG(0,("cli_oem_change_password: Failed to send password change for user %s\n",
			user ));
		return False;
	}

	if (rparam) {
		cli->rap_error = SVAL(rparam,0);
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return (cli->rap_error == 0);
}

/****************************************************************************
 Send a qpathinfo call.
****************************************************************************/

struct cli_qpathinfo1_state {
	struct cli_state *cli;
	uint32_t num_data;
	uint8_t *data;
};

static void cli_qpathinfo1_done(struct tevent_req *subreq);

struct tevent_req *cli_qpathinfo1_send(TALLOC_CTX *mem_ctx,
				       struct event_context *ev,
				       struct cli_state *cli,
				       const char *fname)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_qpathinfo1_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state, struct cli_qpathinfo1_state);
	if (req == NULL) {
		return NULL;
	}
	state->cli = cli;
	subreq = cli_qpathinfo_send(state, ev, cli, fname, SMB_INFO_STANDARD,
				    22, cli->max_xmit);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_qpathinfo1_done, req);
	return req;
}

static void cli_qpathinfo1_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_qpathinfo1_state *state = tevent_req_data(
		req, struct cli_qpathinfo1_state);
	NTSTATUS status;

	status = cli_qpathinfo_recv(subreq, state, &state->data,
				    &state->num_data);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_qpathinfo1_recv(struct tevent_req *req,
			     time_t *change_time,
			     time_t *access_time,
			     time_t *write_time,
			     SMB_OFF_T *size,
			     uint16 *mode)
{
	struct cli_qpathinfo1_state *state = tevent_req_data(
		req, struct cli_qpathinfo1_state);
	NTSTATUS status;

	time_t (*date_fn)(const void *buf, int serverzone);

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	if (state->cli->win95) {
		date_fn = make_unix_date;
	} else {
		date_fn = make_unix_date2;
	}

	if (change_time) {
		*change_time = date_fn(state->data+0, state->cli->serverzone);
	}
	if (access_time) {
		*access_time = date_fn(state->data+4, state->cli->serverzone);
	}
	if (write_time) {
		*write_time = date_fn(state->data+8, state->cli->serverzone);
	}
	if (size) {
		*size = IVAL(state->data, 12);
	}
	if (mode) {
		*mode = SVAL(state->data, l1_attrFile);
	}
	return NT_STATUS_OK;
}

NTSTATUS cli_qpathinfo1(struct cli_state *cli,
			const char *fname,
			time_t *change_time,
			time_t *access_time,
			time_t *write_time,
			SMB_OFF_T *size,
			uint16 *mode)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_qpathinfo1_send(frame, ev, cli, fname);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_qpathinfo1_recv(req, change_time, access_time,
				     write_time, size, mode);
 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Send a setpathinfo call.
****************************************************************************/

NTSTATUS cli_setpathinfo_basic(struct cli_state *cli, const char *fname,
			       time_t create_time,
			       time_t access_time,
			       time_t write_time,
			       time_t change_time,
			       uint16 mode)
{
	unsigned int data_len = 0;
	char data[40];
	char *p;

        p = data;

        /*
         * Add the create, last access, modification, and status change times
         */
        put_long_date(p, create_time);
        p += 8;

        put_long_date(p, access_time);
        p += 8;

        put_long_date(p, write_time);
        p += 8;

        put_long_date(p, change_time);
        p += 8;

        /* Add attributes */
        SIVAL(p, 0, mode);
        p += 4;

        /* Add padding */
        SIVAL(p, 0, 0);
        p += 4;

        data_len = PTR_DIFF(p, data);

	return cli_setpathinfo(cli, SMB_FILE_BASIC_INFORMATION, fname,
			       (uint8_t *)data, data_len);
}

/****************************************************************************
 Send a qpathinfo call with the SMB_QUERY_FILE_ALL_INFO info level.
****************************************************************************/

struct cli_qpathinfo2_state {
	uint32_t num_data;
	uint8_t *data;
};

static void cli_qpathinfo2_done(struct tevent_req *subreq);

struct tevent_req *cli_qpathinfo2_send(TALLOC_CTX *mem_ctx,
				       struct event_context *ev,
				       struct cli_state *cli,
				       const char *fname)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_qpathinfo2_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state, struct cli_qpathinfo2_state);
	if (req == NULL) {
		return NULL;
	}
	subreq = cli_qpathinfo_send(state, ev, cli, fname,
				    SMB_QUERY_FILE_ALL_INFO,
				    68, cli->max_xmit);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_qpathinfo2_done, req);
	return req;
}

static void cli_qpathinfo2_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_qpathinfo2_state *state = tevent_req_data(
		req, struct cli_qpathinfo2_state);
	NTSTATUS status;

	status = cli_qpathinfo_recv(subreq, state, &state->data,
				    &state->num_data);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_qpathinfo2_recv(struct tevent_req *req,
			     struct timespec *create_time,
			     struct timespec *access_time,
			     struct timespec *write_time,
			     struct timespec *change_time,
			     SMB_OFF_T *size, uint16 *mode,
			     SMB_INO_T *ino)
{
	struct cli_qpathinfo2_state *state = tevent_req_data(
		req, struct cli_qpathinfo2_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	if (create_time) {
                *create_time = interpret_long_date((char *)state->data+0);
	}
	if (access_time) {
		*access_time = interpret_long_date((char *)state->data+8);
	}
	if (write_time) {
		*write_time = interpret_long_date((char *)state->data+16);
	}
	if (change_time) {
		*change_time = interpret_long_date((char *)state->data+24);
	}
	if (mode) {
		*mode = SVAL(state->data, 32);
	}
	if (size) {
                *size = IVAL2_TO_SMB_BIG_UINT(state->data,48);
	}
	if (ino) {
		*ino = IVAL(state->data, 64);
	}
	return NT_STATUS_OK;
}

NTSTATUS cli_qpathinfo2(struct cli_state *cli, const char *fname,
			struct timespec *create_time,
			struct timespec *access_time,
			struct timespec *write_time,
			struct timespec *change_time,
			SMB_OFF_T *size, uint16 *mode,
			SMB_INO_T *ino)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_qpathinfo2_send(frame, ev, cli, fname);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_qpathinfo2_recv(req, create_time, access_time,
				     write_time, change_time, size, mode, ino);
 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Get the stream info
****************************************************************************/

static bool parse_streams_blob(TALLOC_CTX *mem_ctx, const uint8_t *data,
			       size_t data_len,
			       unsigned int *pnum_streams,
			       struct stream_struct **pstreams);

struct cli_qpathinfo_streams_state {
	uint32_t num_data;
	uint8_t *data;
};

static void cli_qpathinfo_streams_done(struct tevent_req *subreq);

struct tevent_req *cli_qpathinfo_streams_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct cli_state *cli,
					      const char *fname)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_qpathinfo_streams_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_qpathinfo_streams_state);
	if (req == NULL) {
		return NULL;
	}
	subreq = cli_qpathinfo_send(state, ev, cli, fname,
				    SMB_FILE_STREAM_INFORMATION,
				    0, cli->max_xmit);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_qpathinfo_streams_done, req);
	return req;
}

static void cli_qpathinfo_streams_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_qpathinfo_streams_state *state = tevent_req_data(
		req, struct cli_qpathinfo_streams_state);
	NTSTATUS status;

	status = cli_qpathinfo_recv(subreq, state, &state->data,
				    &state->num_data);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_qpathinfo_streams_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    unsigned int *pnum_streams,
				    struct stream_struct **pstreams)
{
	struct cli_qpathinfo_streams_state *state = tevent_req_data(
                req, struct cli_qpathinfo_streams_state);
        NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	if (!parse_streams_blob(mem_ctx, state->data, state->num_data,
				pnum_streams, pstreams)) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}
	return NT_STATUS_OK;
}

NTSTATUS cli_qpathinfo_streams(struct cli_state *cli, const char *fname,
			       TALLOC_CTX *mem_ctx,
			       unsigned int *pnum_streams,
			       struct stream_struct **pstreams)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_qpathinfo_streams_send(frame, ev, cli, fname);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_qpathinfo_streams_recv(req, mem_ctx, pnum_streams,
					    pstreams);
 fail:
	TALLOC_FREE(frame);
	return status;
}

static bool parse_streams_blob(TALLOC_CTX *mem_ctx, const uint8_t *rdata,
			       size_t data_len,
			       unsigned int *pnum_streams,
			       struct stream_struct **pstreams)
{
	unsigned int num_streams;
	struct stream_struct *streams;
	unsigned int ofs;

	num_streams = 0;
	streams = NULL;
	ofs = 0;

	while ((data_len > ofs) && (data_len - ofs >= 24)) {
		uint32_t nlen, len;
		size_t size;
		void *vstr;
		struct stream_struct *tmp;
		uint8_t *tmp_buf;

		tmp = TALLOC_REALLOC_ARRAY(mem_ctx, streams,
					   struct stream_struct,
					   num_streams+1);

		if (tmp == NULL) {
			goto fail;
		}
		streams = tmp;

		nlen                      = IVAL(rdata, ofs + 0x04);

		streams[num_streams].size = IVAL_TO_SMB_OFF_T(
			rdata, ofs + 0x08);
		streams[num_streams].alloc_size = IVAL_TO_SMB_OFF_T(
			rdata, ofs + 0x10);

		if (nlen > data_len - (ofs + 24)) {
			goto fail;
		}

		/*
		 * We need to null-terminate src, how do I do this with
		 * convert_string_talloc??
		 */

		tmp_buf = TALLOC_ARRAY(streams, uint8_t, nlen+2);
		if (tmp_buf == NULL) {
			goto fail;
		}

		memcpy(tmp_buf, rdata+ofs+24, nlen);
		tmp_buf[nlen] = 0;
		tmp_buf[nlen+1] = 0;

		if (!convert_string_talloc(streams, CH_UTF16, CH_UNIX, tmp_buf,
					   nlen+2, &vstr, &size, false))
		{
			TALLOC_FREE(tmp_buf);
			goto fail;
		}

		TALLOC_FREE(tmp_buf);
		streams[num_streams].name = (char *)vstr;
		num_streams++;

		len = IVAL(rdata, ofs);
		if (len > data_len - ofs) {
			goto fail;
		}
		if (len == 0) break;
		ofs += len;
	}

	*pnum_streams = num_streams;
	*pstreams = streams;
	return true;

 fail:
	TALLOC_FREE(streams);
	return false;
}

/****************************************************************************
 Send a qfileinfo QUERY_FILE_NAME_INFO call.
****************************************************************************/

NTSTATUS cli_qfilename(struct cli_state *cli, uint16_t fnum, char *name,
		       size_t namelen)
{
	uint8_t *rdata;
	uint32_t num_rdata;
	NTSTATUS status;

	status = cli_qfileinfo(talloc_tos(), cli, fnum,
			       SMB_QUERY_FILE_NAME_INFO,
			       4, cli->max_xmit,
			       &rdata, &num_rdata);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	clistr_pull(cli->inbuf, name, rdata+4, namelen, IVAL(rdata, 0),
		    STR_UNICODE);
	TALLOC_FREE(rdata);
	return NT_STATUS_OK;
}

/****************************************************************************
 Send a qfileinfo call.
****************************************************************************/

NTSTATUS cli_qfileinfo_basic(struct cli_state *cli, uint16_t fnum,
			     uint16 *mode, SMB_OFF_T *size,
			     struct timespec *create_time,
			     struct timespec *access_time,
			     struct timespec *write_time,
			     struct timespec *change_time,
			     SMB_INO_T *ino)
{
	uint8_t *rdata;
	uint32_t num_rdata;
	NTSTATUS status;

	/* if its a win95 server then fail this - win95 totally screws it
	   up */
	if (cli->win95) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	status = cli_qfileinfo(talloc_tos(), cli, fnum,
			       SMB_QUERY_FILE_ALL_INFO,
			       68, MIN(cli->max_xmit, 0xffff),
			       &rdata, &num_rdata);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (create_time) {
		*create_time = interpret_long_date((char *)rdata+0);
	}
	if (access_time) {
		*access_time = interpret_long_date((char *)rdata+8);
	}
	if (write_time) {
		*write_time = interpret_long_date((char *)rdata+16);
	}
	if (change_time) {
		*change_time = interpret_long_date((char *)rdata+24);
	}
	if (mode) {
		*mode = SVAL(rdata, 32);
	}
	if (size) {
                *size = IVAL2_TO_SMB_BIG_UINT(rdata,48);
	}
	if (ino) {
		*ino = IVAL(rdata, 64);
	}

	TALLOC_FREE(rdata);
	return NT_STATUS_OK;
}

/****************************************************************************
 Send a qpathinfo BASIC_INFO call.
****************************************************************************/

struct cli_qpathinfo_basic_state {
	uint32_t num_data;
	uint8_t *data;
};

static void cli_qpathinfo_basic_done(struct tevent_req *subreq);

struct tevent_req *cli_qpathinfo_basic_send(TALLOC_CTX *mem_ctx,
					    struct event_context *ev,
					    struct cli_state *cli,
					    const char *fname)
{
	struct tevent_req *req = NULL, *subreq = NULL;
	struct cli_qpathinfo_basic_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_qpathinfo_basic_state);
	if (req == NULL) {
		return NULL;
	}
	subreq = cli_qpathinfo_send(state, ev, cli, fname,
				    SMB_QUERY_FILE_BASIC_INFO,
				    36, cli->max_xmit);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_qpathinfo_basic_done, req);
	return req;
}

static void cli_qpathinfo_basic_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_qpathinfo_basic_state *state = tevent_req_data(
		req, struct cli_qpathinfo_basic_state);
	NTSTATUS status;

	status = cli_qpathinfo_recv(subreq, state, &state->data,
				    &state->num_data);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_qpathinfo_basic_recv(struct tevent_req *req,
				  SMB_STRUCT_STAT *sbuf, uint32 *attributes)
{
	struct cli_qpathinfo_basic_state *state = tevent_req_data(
		req, struct cli_qpathinfo_basic_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	sbuf->st_ex_atime = interpret_long_date((char *)state->data+8);
	sbuf->st_ex_mtime = interpret_long_date((char *)state->data+16);
	sbuf->st_ex_ctime = interpret_long_date((char *)state->data+24);
	*attributes = IVAL(state->data, 32);
	return NT_STATUS_OK;
}

NTSTATUS cli_qpathinfo_basic(struct cli_state *cli, const char *name,
			     SMB_STRUCT_STAT *sbuf, uint32 *attributes)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}
	ev = event_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = cli_qpathinfo_basic_send(frame, ev, cli, name);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = cli_qpathinfo_basic_recv(req, sbuf, attributes);
 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
 Send a qpathinfo SMB_QUERY_FILE_ALT_NAME_INFO call.
****************************************************************************/

NTSTATUS cli_qpathinfo_alt_name(struct cli_state *cli, const char *fname, fstring alt_name)
{
	uint8_t *rdata;
	uint32_t num_rdata;
	unsigned int len;
	char *converted = NULL;
	size_t converted_size = 0;
	NTSTATUS status;

	status = cli_qpathinfo(talloc_tos(), cli, fname,
			       SMB_QUERY_FILE_ALT_NAME_INFO,
			       4, cli->max_xmit, &rdata, &num_rdata);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	len = IVAL(rdata, 0);

	if (len > num_rdata - 4) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	/* The returned data is a pushed string, not raw data. */
	if (!convert_string_talloc(talloc_tos(),
				   cli_ucs2(cli) ? CH_UTF16LE : CH_DOS,
				   CH_UNIX,
				   rdata + 4,
				   len,
				   &converted,
				   &converted_size,
				   true)) {
		return NT_STATUS_NO_MEMORY;
	}
	fstrcpy(alt_name, converted);

	TALLOC_FREE(converted);
	TALLOC_FREE(rdata);

	return NT_STATUS_OK;
}

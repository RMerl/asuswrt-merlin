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

/****************************************************************************
 Call a remote api
****************************************************************************/

bool cli_api(struct cli_state *cli,
	     char *param, int prcnt, int mprcnt,
	     char *data, int drcnt, int mdrcnt,
	     char **rparam, unsigned int *rprcnt,
	     char **rdata, unsigned int *rdrcnt)
{
	cli_send_trans(cli,SMBtrans,
                 PIPE_LANMAN,             /* Name */
                 0,0,                     /* fid, flags */
                 NULL,0,0,                /* Setup, length, max */
                 param, prcnt, mprcnt,    /* Params, length, max */
                 data, drcnt, mdrcnt      /* Data, length, max */
                );

	return (cli_receive_trans(cli,SMBtrans,
                            rparam, rprcnt,
                            rdata, rdrcnt));
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

		if (len == (size_t)-1) {
			SAFE_FREE(last_entry);
			return false;
		}
		p += len;

		if (func == RAP_NetServerEnum3) {
			len = push_ascii(p,
					last_entry ? last_entry : "",
					sizeof(param) - PTR_DIFF(p,param) - 1,
					STR_TERMINATE);

			if (len == (size_t)-1) {
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

	if (cli_send_trans(cli,SMBtrans,
                    PIPE_LANMAN,                          /* name */
                    0,0,                                  /* fid, flags */
                    NULL,0,0,                             /* setup, length, max */
                    param,param_len,2,                    /* param, length, max */
                    (char *)data,data_len,0                       /* data, length, max */
                   ) == False) {
		DEBUG(0,("cli_oem_change_password: Failed to send password change for user %s\n",
			user ));
		return False;
	}

	if (!cli_receive_trans(cli,SMBtrans,
                       &rparam, &rprcnt,
                       &rdata, &rdrcnt)) {
		DEBUG(0,("cli_oem_change_password: Failed to recieve reply to password change for user %s\n",
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

bool cli_qpathinfo(struct cli_state *cli,
			const char *fname,
			time_t *change_time,
			time_t *access_time,
			time_t *write_time,
			SMB_OFF_T *size,
			uint16 *mode)
{
	unsigned int data_len = 0;
	unsigned int param_len = 0;
	unsigned int rparam_len, rdata_len;
	uint16 setup = TRANSACT2_QPATHINFO;
	char *param;
	char *rparam=NULL, *rdata=NULL;
	int count=8;
	bool ret;
	time_t (*date_fn)(struct cli_state *, const void *);
	char *p;
	size_t nlen = 2*(strlen(fname)+1);

	param = SMB_MALLOC_ARRAY(char, 6+nlen+2);
	if (!param) {
		return false;
	}
	p = param;
	memset(p, '\0', 6);
	SSVAL(p, 0, SMB_INFO_STANDARD);
	p += 6;
	p += clistr_push(cli, p, fname, nlen, STR_TERMINATE);
	param_len = PTR_DIFF(p, param);

	do {
		ret = (cli_send_trans(cli, SMBtrans2,
				      NULL,           /* Name */
				      -1, 0,          /* fid, flags */
				      &setup, 1, 0,   /* setup, length, max */
				      param, param_len, 10, /* param, length, max */
				      NULL, data_len, cli->max_xmit /* data, length, max */
				      ) &&
		       cli_receive_trans(cli, SMBtrans2,
					 &rparam, &rparam_len,
					 &rdata, &rdata_len));
		if (!cli_is_dos_error(cli)) break;
		if (!ret) {
			/* we need to work around a Win95 bug - sometimes
			   it gives ERRSRV/ERRerror temprarily */
			uint8 eclass;
			uint32 ecode;
			cli_dos_error(cli, &eclass, &ecode);
			if (eclass != ERRSRV || ecode != ERRerror) break;
			smb_msleep(100);
		}
	} while (count-- && ret==False);

	SAFE_FREE(param);
	if (!ret || !rdata || rdata_len < 22) {
		return False;
	}

	if (cli->win95) {
		date_fn = cli_make_unix_date;
	} else {
		date_fn = cli_make_unix_date2;
	}

	if (change_time) {
		*change_time = date_fn(cli, rdata+0);
	}
	if (access_time) {
		*access_time = date_fn(cli, rdata+4);
	}
	if (write_time) {
		*write_time = date_fn(cli, rdata+8);
	}
	if (size) {
		*size = IVAL(rdata, 12);
	}
	if (mode) {
		*mode = SVAL(rdata,l1_attrFile);
	}

	SAFE_FREE(rdata);
	SAFE_FREE(rparam);
	return True;
}

/****************************************************************************
 Send a setpathinfo call.
****************************************************************************/

bool cli_setpathinfo(struct cli_state *cli, const char *fname,
                     time_t create_time,
                     time_t access_time,
                     time_t write_time,
                     time_t change_time,
                     uint16 mode)
{
	unsigned int data_len = 0;
	unsigned int param_len = 0;
	unsigned int rparam_len, rdata_len;
	uint16 setup = TRANSACT2_SETPATHINFO;
	char *param;
	char data[40];
	char *rparam=NULL, *rdata=NULL;
	int count=8;
	bool ret;
	char *p;
	size_t nlen = 2*(strlen(fname)+1);

	param = SMB_MALLOC_ARRAY(char, 6+nlen+2);
	if (!param) {
		return false;
	}
	memset(param, '\0', 6);
	memset(data, 0, sizeof(data));

        p = param;

        /* Add the information level */
	SSVAL(p, 0, SMB_FILE_BASIC_INFORMATION);

        /* Skip reserved */
	p += 6;

        /* Add the file name */
	p += clistr_push(cli, p, fname, nlen, STR_TERMINATE);

	param_len = PTR_DIFF(p, param);

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

	do {
		ret = (cli_send_trans(cli, SMBtrans2,
				      NULL,           /* Name */
				      -1, 0,          /* fid, flags */
				      &setup, 1, 0,   /* setup, length, max */
				      param, param_len, 10, /* param, length, max */
				      data, data_len, cli->max_xmit /* data, length, max */
				      ) &&
		       cli_receive_trans(cli, SMBtrans2,
					 &rparam, &rparam_len,
					 &rdata, &rdata_len));
		if (!cli_is_dos_error(cli)) break;
		if (!ret) {
			/* we need to work around a Win95 bug - sometimes
			   it gives ERRSRV/ERRerror temprarily */
			uint8 eclass;
			uint32 ecode;
			cli_dos_error(cli, &eclass, &ecode);
			if (eclass != ERRSRV || ecode != ERRerror) break;
			smb_msleep(100);
		}
	} while (count-- && ret==False);

	SAFE_FREE(param);
	if (!ret) {
		return False;
	}

	SAFE_FREE(rdata);
	SAFE_FREE(rparam);
	return True;
}

/****************************************************************************
 Send a qpathinfo call with the SMB_QUERY_FILE_ALL_INFO info level.
****************************************************************************/

bool cli_qpathinfo2(struct cli_state *cli, const char *fname,
		    struct timespec *create_time,
                    struct timespec *access_time,
                    struct timespec *write_time,
		    struct timespec *change_time,
                    SMB_OFF_T *size, uint16 *mode,
		    SMB_INO_T *ino)
{
	unsigned int data_len = 0;
	unsigned int param_len = 0;
	uint16 setup = TRANSACT2_QPATHINFO;
	char *param;
	char *rparam=NULL, *rdata=NULL;
	char *p;
	size_t nlen = 2*(strlen(fname)+1);

	param = SMB_MALLOC_ARRAY(char, 6+nlen+2);
	if (!param) {
		return false;
	}
	p = param;
	memset(param, '\0', 6);
	SSVAL(p, 0, SMB_QUERY_FILE_ALL_INFO);
	p += 6;
	p += clistr_push(cli, p, fname, nlen, STR_TERMINATE);

	param_len = PTR_DIFF(p, param);

	if (!cli_send_trans(cli, SMBtrans2,
                            NULL,                         /* name */
                            -1, 0,                        /* fid, flags */
                            &setup, 1, 0,                 /* setup, length, max */
                            param, param_len, 10,         /* param, length, max */
                            NULL, data_len, cli->max_xmit /* data, length, max */
                           )) {
		SAFE_FREE(param);
		return False;
	}

	SAFE_FREE(param);
	if (!cli_receive_trans(cli, SMBtrans2,
                               &rparam, &param_len,
                               &rdata, &data_len)) {
		return False;
	}

	if (!rdata || data_len < 22) {
		return False;
	}

	if (create_time) {
                *create_time = interpret_long_date(rdata+0);
	}
	if (access_time) {
		*access_time = interpret_long_date(rdata+8);
	}
	if (write_time) {
		*write_time = interpret_long_date(rdata+16);
	}
	if (change_time) {
		*change_time = interpret_long_date(rdata+24);
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

	SAFE_FREE(rdata);
	SAFE_FREE(rparam);
	return True;
}

/****************************************************************************
 Get the stream info
****************************************************************************/

bool cli_qpathinfo_streams(struct cli_state *cli, const char *fname,
			   TALLOC_CTX *mem_ctx,
			   unsigned int *pnum_streams,
			   struct stream_struct **pstreams)
{
	unsigned int data_len = 0;
	unsigned int param_len = 0;
	uint16 setup = TRANSACT2_QPATHINFO;
	char *param;
	char *rparam=NULL, *rdata=NULL;
	char *p;
	unsigned int num_streams;
	struct stream_struct *streams;
	unsigned int ofs;
	size_t namelen = 2*(strlen(fname)+1);

	param = SMB_MALLOC_ARRAY(char, 6+namelen+2);
	if (param == NULL) {
		return false;
	}
	p = param;
	memset(p, 0, 6);
	SSVAL(p, 0, SMB_FILE_STREAM_INFORMATION);
	p += 6;
	p += clistr_push(cli, p, fname, namelen, STR_TERMINATE);

	param_len = PTR_DIFF(p, param);

	if (!cli_send_trans(cli, SMBtrans2,
                            NULL,                     /* name */
                            -1, 0,                    /* fid, flags */
                            &setup, 1, 0,             /* setup, len, max */
                            param, param_len, 10,     /* param, len, max */
                            NULL, data_len, cli->max_xmit /* data, len, max */
                           )) {
		return false;
	}

	if (!cli_receive_trans(cli, SMBtrans2,
                               &rparam, &param_len,
                               &rdata, &data_len)) {
		return false;
	}

	if (!rdata) {
		SAFE_FREE(rparam);
		return false;
	}

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

	SAFE_FREE(rdata);
	SAFE_FREE(rparam);

	*pnum_streams = num_streams;
	*pstreams = streams;
	return true;

 fail:
	TALLOC_FREE(streams);
	SAFE_FREE(rdata);
	SAFE_FREE(rparam);
	return false;
}

/****************************************************************************
 Send a qfileinfo QUERY_FILE_NAME_INFO call.
****************************************************************************/

bool cli_qfilename(struct cli_state *cli, uint16_t fnum, char *name, size_t namelen)
{
	unsigned int data_len = 0;
	unsigned int param_len = 0;
	uint16 setup = TRANSACT2_QFILEINFO;
	char param[4];
	char *rparam=NULL, *rdata=NULL;

	param_len = 4;
	SSVAL(param, 0, fnum);
	SSVAL(param, 2, SMB_QUERY_FILE_NAME_INFO);

	if (!cli_send_trans(cli, SMBtrans2,
                            NULL,                         /* name */
                            -1, 0,                        /* fid, flags */
                            &setup, 1, 0,                 /* setup, length, max */
                            param, param_len, 2,          /* param, length, max */
                            NULL, data_len, cli->max_xmit /* data, length, max */
                           )) {
		return False;
	}

	if (!cli_receive_trans(cli, SMBtrans2,
                               &rparam, &param_len,
                               &rdata, &data_len)) {
		return False;
	}

	if (!rdata || data_len < 4) {
		SAFE_FREE(rparam);
		SAFE_FREE(rdata);
		return False;
	}

	clistr_pull(cli->inbuf, name, rdata+4, namelen, IVAL(rdata, 0),
		    STR_UNICODE);

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return True;
}

/****************************************************************************
 Send a qfileinfo call.
****************************************************************************/

bool cli_qfileinfo(struct cli_state *cli, uint16_t fnum,
		   uint16 *mode, SMB_OFF_T *size,
		   struct timespec *create_time,
                   struct timespec *access_time,
                   struct timespec *write_time,
		   struct timespec *change_time,
                   SMB_INO_T *ino)
{
	unsigned int data_len = 0;
	unsigned int param_len = 0;
	uint16 setup;
	uint8_t param[4];
	uint8_t *rparam=NULL, *rdata=NULL;
	NTSTATUS status;

	/* if its a win95 server then fail this - win95 totally screws it
	   up */
	if (cli->win95) return False;

	param_len = 4;

	SSVAL(param, 0, fnum);
	SSVAL(param, 2, SMB_QUERY_FILE_ALL_INFO);

	SSVAL(&setup, 0, TRANSACT2_QFILEINFO);

	status = cli_trans(talloc_tos(), cli, SMBtrans2,
			   NULL, -1, 0, 0, /* name, fid, function, flags */
			   &setup, 1, 0,          /* setup, length, max */
			   param, param_len, 2,   /* param, length, max */
			   NULL, 0, MIN(cli->max_xmit, 0xffff), /* data, length, max */
			   NULL, NULL, /* rsetup, length */
			   &rparam, &param_len,	/* rparam, length */
			   &rdata, &data_len);

	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	if (!rdata || data_len < 68) {
		return False;
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
	TALLOC_FREE(rparam);
	return True;
}

/****************************************************************************
 Send a qpathinfo BASIC_INFO call.
****************************************************************************/

bool cli_qpathinfo_basic( struct cli_state *cli, const char *name,
                          SMB_STRUCT_STAT *sbuf, uint32 *attributes )
{
	unsigned int param_len = 0;
	unsigned int data_len = 0;
	uint16 setup = TRANSACT2_QPATHINFO;
	char *param;
	char *rparam=NULL, *rdata=NULL;
	char *p;
	char *path;
	int len;
	size_t nlen;
	TALLOC_CTX *frame = talloc_stackframe();

	path = talloc_strdup(frame, name);
	if (!path) {
		TALLOC_FREE(frame);
		return false;
	}
	/* cleanup */

	len = strlen(path);
	if ( path[len-1] == '\\' || path[len-1] == '/') {
		path[len-1] = '\0';
	}
	nlen = 2*(strlen(path)+1);

	param = TALLOC_ARRAY(frame,char,6+nlen+2);
	if (!param) {
		return false;
	}
	p = param;
	memset(param, '\0', 6);

	SSVAL(p, 0, SMB_QUERY_FILE_BASIC_INFO);
	p += 6;
	p += clistr_push(cli, p, path, nlen, STR_TERMINATE);
	param_len = PTR_DIFF(p, param);


	if (!cli_send_trans(cli, SMBtrans2,
			NULL,                        /* name */
			-1, 0,                       /* fid, flags */
			&setup, 1, 0,                /* setup, length, max */
			param, param_len, 2,         /* param, length, max */
			NULL,  0, cli->max_xmit      /* data, length, max */
			)) {
		TALLOC_FREE(frame);
		return False;
	}

	TALLOC_FREE(frame);

	if (!cli_receive_trans(cli, SMBtrans2,
		&rparam, &param_len,
		&rdata, &data_len)) {
			return False;
	}

	if (data_len < 36) {
		SAFE_FREE(rdata);
		SAFE_FREE(rparam);
		return False;
	}

	sbuf->st_ex_atime = interpret_long_date( rdata+8 ); /* Access time. */
	sbuf->st_ex_mtime = interpret_long_date( rdata+16 ); /* Write time. */
	sbuf->st_ex_ctime = interpret_long_date( rdata+24 ); /* Change time. */

	*attributes = IVAL( rdata, 32 );

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return True;
}

/****************************************************************************
 Send a qfileinfo call.
****************************************************************************/

bool cli_qfileinfo_test(struct cli_state *cli, uint16_t fnum, int level, char **poutdata, uint32 *poutlen)
{
	unsigned int data_len = 0;
	unsigned int param_len = 0;
	uint16 setup = TRANSACT2_QFILEINFO;
	char param[4];
	char *rparam=NULL, *rdata=NULL;

	*poutdata = NULL;
	*poutlen = 0;

	/* if its a win95 server then fail this - win95 totally screws it
	   up */
	if (cli->win95)
		return False;

	param_len = 4;

	SSVAL(param, 0, fnum);
	SSVAL(param, 2, level);

	if (!cli_send_trans(cli, SMBtrans2,
                            NULL,                           /* name */
                            -1, 0,                          /* fid, flags */
                            &setup, 1, 0,                   /* setup, length, max */
                            param, param_len, 2,            /* param, length, max */
                            NULL, data_len, cli->max_xmit   /* data, length, max */
                           )) {
		return False;
	}

	if (!cli_receive_trans(cli, SMBtrans2,
                               &rparam, &param_len,
                               &rdata, &data_len)) {
		return False;
	}

	*poutdata = (char *)memdup(rdata, data_len);
	if (!*poutdata) {
		SAFE_FREE(rdata);
		SAFE_FREE(rparam);
		return False;
	}

	*poutlen = data_len;

	SAFE_FREE(rdata);
	SAFE_FREE(rparam);
	return True;
}

/****************************************************************************
 Send a qpathinfo SMB_QUERY_FILE_ALT_NAME_INFO call.
****************************************************************************/

NTSTATUS cli_qpathinfo_alt_name(struct cli_state *cli, const char *fname, fstring alt_name)
{
	unsigned int data_len = 0;
	unsigned int param_len = 0;
	uint16 setup = TRANSACT2_QPATHINFO;
	char *param;
	char *rparam=NULL, *rdata=NULL;
	int count=8;
	char *p;
	bool ret;
	unsigned int len;
	size_t nlen = 2*(strlen(fname)+1);

	param = SMB_MALLOC_ARRAY(char, 6+nlen+2);
	if (!param) {
		return NT_STATUS_NO_MEMORY;
	}
	p = param;
	memset(param, '\0', 6);
	SSVAL(p, 0, SMB_QUERY_FILE_ALT_NAME_INFO);
	p += 6;
	p += clistr_push(cli, p, fname, nlen, STR_TERMINATE);
	param_len = PTR_DIFF(p, param);

	do {
		ret = (cli_send_trans(cli, SMBtrans2,
				      NULL,           /* Name */
				      -1, 0,          /* fid, flags */
				      &setup, 1, 0,   /* setup, length, max */
				      param, param_len, 10, /* param, length, max */
				      NULL, data_len, cli->max_xmit /* data, length, max */
				      ) &&
		       cli_receive_trans(cli, SMBtrans2,
					 &rparam, &param_len,
					 &rdata, &data_len));
		if (!ret && cli_is_dos_error(cli)) {
			/* we need to work around a Win95 bug - sometimes
			   it gives ERRSRV/ERRerror temprarily */
			uint8 eclass;
			uint32 ecode;
			cli_dos_error(cli, &eclass, &ecode);
			if (eclass != ERRSRV || ecode != ERRerror) break;
			smb_msleep(100);
		}
	} while (count-- && ret==False);

	SAFE_FREE(param);

	if (!ret || !rdata || data_len < 4) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	len = IVAL(rdata, 0);

	if (len > data_len - 4) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	clistr_pull(cli->inbuf, alt_name, rdata+4, sizeof(fstring), len,
		    STR_UNICODE);

	SAFE_FREE(rdata);
	SAFE_FREE(rparam);

	return NT_STATUS_OK;
}

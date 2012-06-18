/*
   Unix SMB/CIFS implementation.
   client directory list routines
   Copyright (C) Andrew Tridgell 1994-1998

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

/****************************************************************************
 Calculate a safe next_entry_offset.
****************************************************************************/

static size_t calc_next_entry_offset(const char *base, const char *pdata_end)
{
	size_t next_entry_offset = (size_t)IVAL(base,0);

	if (next_entry_offset == 0 ||
			base + next_entry_offset < base ||
			base + next_entry_offset > pdata_end) {
		next_entry_offset = pdata_end - base;
	}
	return next_entry_offset;
}

/****************************************************************************
 Interpret a long filename structure - this is mostly guesses at the moment.
 The length of the structure is returned
 The structure of a long filename depends on the info level.
 SMB_FIND_FILE_BOTH_DIRECTORY_INFO is used
 by NT and SMB_FIND_EA_SIZE is used by OS/2
****************************************************************************/

static size_t interpret_long_filename(TALLOC_CTX *ctx,
					struct cli_state *cli,
					int level,
					const char *p,
					const char *pdata_end,
					file_info *finfo,
					uint32 *p_resume_key,
					DATA_BLOB *p_last_name_raw)
{
	int len;
	size_t ret;
	const char *base = p;

	data_blob_free(p_last_name_raw);

	if (p_resume_key) {
		*p_resume_key = 0;
	}
	ZERO_STRUCTP(finfo);
	finfo->cli = cli;

	switch (level) {
		case SMB_FIND_INFO_STANDARD: /* OS/2 understands this */
			/* these dates are converted to GMT by
                           make_unix_date */
			if (pdata_end - base < 27) {
				return pdata_end - base;
			}
			finfo->ctime_ts = convert_time_t_to_timespec(cli_make_unix_date2(cli, p+4));
			finfo->atime_ts = convert_time_t_to_timespec(cli_make_unix_date2(cli, p+8));
			finfo->mtime_ts = convert_time_t_to_timespec(cli_make_unix_date2(cli, p+12));
			finfo->size = IVAL(p,16);
			finfo->mode = CVAL(p,24);
			len = CVAL(p, 26);
			p += 27;
			p += clistr_align_in(cli, p, 0);

			/* We can safely use len here (which is required by OS/2)
			 * and the NAS-BASIC server instead of +2 or +1 as the
			 * STR_TERMINATE flag below is
			 * actually used as the length calculation.
			 * The len is merely an upper bound.
			 * Due to the explicit 2 byte null termination
			 * in cli_receive_trans/cli_receive_nt_trans
			 * we know this is safe. JRA + kukks
			 */

			if (p + len > pdata_end) {
				return pdata_end - base;
			}

			/* the len+2 below looks strange but it is
			   important to cope with the differences
			   between win2000 and win9x for this call
			   (tridge) */
			ret = clistr_pull_talloc(ctx,
						cli->inbuf,
						&finfo->name,
						p,
						len+2,
						STR_TERMINATE);
			if (ret == (size_t)-1) {
				return pdata_end - base;
			}
			p += ret;
			return PTR_DIFF(p, base);

		case SMB_FIND_EA_SIZE: /* this is what OS/2 uses mostly */
			/* these dates are converted to GMT by
                           make_unix_date */
			if (pdata_end - base < 31) {
				return pdata_end - base;
			}
			finfo->ctime_ts = convert_time_t_to_timespec(cli_make_unix_date2(cli, p+4));
			finfo->atime_ts = convert_time_t_to_timespec(cli_make_unix_date2(cli, p+8));
			finfo->mtime_ts = convert_time_t_to_timespec(cli_make_unix_date2(cli, p+12));
			finfo->size = IVAL(p,16);
			finfo->mode = CVAL(p,24);
			len = CVAL(p, 30);
			p += 31;
			/* check for unisys! */
			if (p + len + 1 > pdata_end) {
				return pdata_end - base;
			}
			ret = clistr_pull_talloc(ctx,
						cli->inbuf,
						&finfo->name,
						p,
					 	len,
						STR_NOALIGN);
			if (ret == (size_t)-1) {
				return pdata_end - base;
			}
			p += ret;
			return PTR_DIFF(p, base) + 1;

		case SMB_FIND_FILE_BOTH_DIRECTORY_INFO: /* NT uses this, but also accepts 2 */
		{
			size_t namelen, slen;

			if (pdata_end - base < 94) {
				return pdata_end - base;
			}

			p += 4; /* next entry offset */

			if (p_resume_key) {
				*p_resume_key = IVAL(p,0);
			}
			p += 4; /* fileindex */

			/* Offset zero is "create time", not "change time". */
			p += 8;
			finfo->atime_ts = interpret_long_date(p);
			p += 8;
			finfo->mtime_ts = interpret_long_date(p);
			p += 8;
			finfo->ctime_ts = interpret_long_date(p);
			p += 8;
			finfo->size = IVAL2_TO_SMB_BIG_UINT(p,0);
			p += 8;
			p += 8; /* alloc size */
			finfo->mode = CVAL(p,0);
			p += 4;
			namelen = IVAL(p,0);
			p += 4;
			p += 4; /* EA size */
			slen = SVAL(p, 0);
			if (slen > 24) {
				/* Bad short name length. */
				return pdata_end - base;
			}
			p += 2;
			{
				/* stupid NT bugs. grr */
				int flags = 0;
				if (p[1] == 0 && namelen > 1) flags |= STR_UNICODE;
				clistr_pull(cli->inbuf, finfo->short_name, p,
					    sizeof(finfo->short_name),
					    slen, flags);
			}
			p += 24; /* short name? */
			if (p + namelen < p || p + namelen > pdata_end) {
				return pdata_end - base;
			}
			ret = clistr_pull_talloc(ctx,
						cli->inbuf,
						&finfo->name,
						p,
				    		namelen,
						0);
			if (ret == (size_t)-1) {
				return pdata_end - base;
			}

			/* To be robust in the face of unicode conversion failures
			   we need to copy the raw bytes of the last name seen here.
			   Namelen doesn't include the terminating unicode null, so
			   copy it here. */

			if (p_last_name_raw) {
				*p_last_name_raw = data_blob(NULL, namelen+2);
				memcpy(p_last_name_raw->data, p, namelen);
				SSVAL(p_last_name_raw->data, namelen, 0);
			}
			return calc_next_entry_offset(base, pdata_end);
		}
	}

	DEBUG(1,("Unknown long filename format %d\n",level));
	return calc_next_entry_offset(base, pdata_end);
}

/****************************************************************************
 Do a directory listing, calling fn on each file found.
****************************************************************************/

int cli_list_new(struct cli_state *cli,const char *Mask,uint16 attribute,
		 void (*fn)(const char *, file_info *, const char *, void *), void *state)
{
#if 1
	int max_matches = 1366; /* Match W2k - was 512. */
#else
	int max_matches = 512;
#endif
	int info_level;
	char *p, *p2, *rdata_end;
	char *mask = NULL;
	file_info finfo;
	int i;
	char *dirlist = NULL;
	int dirlist_len = 0;
	int total_received = -1;
	bool First = True;
	int ff_searchcount=0;
	int ff_eos=0;
	int ff_dir_handle=0;
	int loop_count = 0;
	char *rparam=NULL, *rdata=NULL;
	unsigned int param_len, data_len;
	uint16 setup;
	char *param;
	uint32 resume_key = 0;
	TALLOC_CTX *frame = talloc_stackframe();
	DATA_BLOB last_name_raw = data_blob(NULL, 0);

	/* NT uses SMB_FIND_FILE_BOTH_DIRECTORY_INFO,
	   OS/2 uses SMB_FIND_EA_SIZE. Both accept SMB_FIND_INFO_STANDARD. */
	info_level = (cli->capabilities&CAP_NT_SMBS)?
		SMB_FIND_FILE_BOTH_DIRECTORY_INFO : SMB_FIND_INFO_STANDARD;

	mask = SMB_STRDUP(Mask);
	if (!mask) {
		TALLOC_FREE(frame);
		return -1;
	}

	while (ff_eos == 0) {
		size_t nlen = 2*(strlen(mask)+1);

		loop_count++;
		if (loop_count > 200) {
			DEBUG(0,("Error: Looping in FIND_NEXT??\n"));
			break;
		}

		param = SMB_MALLOC_ARRAY(char, 12+nlen+last_name_raw.length+2);
		if (!param) {
			break;
		}

		if (First) {
			setup = TRANSACT2_FINDFIRST;
			SSVAL(param,0,attribute); /* attribute */
			SSVAL(param,2,max_matches); /* max count */
			SSVAL(param,4,(FLAG_TRANS2_FIND_REQUIRE_RESUME|FLAG_TRANS2_FIND_CLOSE_IF_END));	/* resume required + close on end */
			SSVAL(param,6,info_level);
			SIVAL(param,8,0);
			p = param+12;
			p += clistr_push(cli, param+12, mask,
					 nlen, STR_TERMINATE);
		} else {
			setup = TRANSACT2_FINDNEXT;
			SSVAL(param,0,ff_dir_handle);
			SSVAL(param,2,max_matches); /* max count */
			SSVAL(param,4,info_level);
			/* For W2K servers serving out FAT filesystems we *must* set the
			   resume key. If it's not FAT then it's returned as zero. */
			SIVAL(param,6,resume_key); /* ff_resume_key */
			/* NB. *DON'T* use continue here. If you do it seems that W2K and bretheren
			   can miss filenames. Use last filename continue instead. JRA */
			SSVAL(param,10,(FLAG_TRANS2_FIND_REQUIRE_RESUME|FLAG_TRANS2_FIND_CLOSE_IF_END));	/* resume required + close on end */
			p = param+12;
			if (last_name_raw.length) {
				memcpy(p, last_name_raw.data, last_name_raw.length);
				p += last_name_raw.length;
			} else {
				p += clistr_push(cli, param+12, mask,
						nlen, STR_TERMINATE);
			}
		}

		param_len = PTR_DIFF(p, param);

		if (!cli_send_trans(cli, SMBtrans2,
				    NULL,                   /* Name */
				    -1, 0,                  /* fid, flags */
				    &setup, 1, 0,           /* setup, length, max */
				    param, param_len, 10,   /* param, length, max */
				    NULL, 0,
#if 0
				    /* w2k value. */
				    MIN(16384,cli->max_xmit) /* data, length, max. */
#else
				    cli->max_xmit	    /* data, length, max. */
#endif
				    )) {
			SAFE_FREE(param);
			TALLOC_FREE(frame);
			break;
		}

		SAFE_FREE(param);

		if (!cli_receive_trans(cli, SMBtrans2,
				       &rparam, &param_len,
				       &rdata, &data_len) &&
                    cli_is_dos_error(cli)) {
			/* We need to work around a Win95 bug - sometimes
			   it gives ERRSRV/ERRerror temprarily */
			uint8 eclass;
			uint32 ecode;

			SAFE_FREE(rdata);
			SAFE_FREE(rparam);

			cli_dos_error(cli, &eclass, &ecode);

			/*
			 * OS/2 might return "no more files",
			 * which just tells us, that searchcount is zero
			 * in this search.
			 * Guenter Kukkukk <linux@kukkukk.com>
			 */

			if (eclass == ERRDOS && ecode == ERRnofiles) {
				ff_searchcount = 0;
				cli_reset_error(cli);
				break;
			}

			if (eclass != ERRSRV || ecode != ERRerror)
				break;
			smb_msleep(100);
			continue;
		}

                if (cli_is_error(cli) || !rdata || !rparam) {
			SAFE_FREE(rdata);
			SAFE_FREE(rparam);
			break;
		}

		if (total_received == -1)
			total_received = 0;

		/* parse out some important return info */
		p = rparam;
		if (First) {
			ff_dir_handle = SVAL(p,0);
			ff_searchcount = SVAL(p,2);
			ff_eos = SVAL(p,4);
		} else {
			ff_searchcount = SVAL(p,0);
			ff_eos = SVAL(p,2);
		}

		if (ff_searchcount == 0) {
			SAFE_FREE(rdata);
			SAFE_FREE(rparam);
			break;
		}

		/* point to the data bytes */
		p = rdata;
		rdata_end = rdata + data_len;

		/* we might need the lastname for continuations */
		for (p2=p,i=0;i<ff_searchcount && p2 < rdata_end;i++) {
			if ((info_level == SMB_FIND_FILE_BOTH_DIRECTORY_INFO) &&
					(i == ff_searchcount-1)) {
				/* Last entry - fixup the last offset length. */
				SIVAL(p2,0,PTR_DIFF((rdata + data_len),p2));
			}
			p2 += interpret_long_filename(frame,
							cli,
							info_level,
							p2,
							rdata_end,
							&finfo,
							&resume_key,
							&last_name_raw);

			if (!finfo.name) {
				DEBUG(0,("cli_list_new: Error: unable to parse name from info level %d\n",
					info_level));
				ff_eos = 1;
				break;
			}
			if (!First && *mask && strcsequal(finfo.name, mask)) {
				DEBUG(0,("Error: Looping in FIND_NEXT as name %s has already been seen?\n",
					finfo.name));
				ff_eos = 1;
				break;
			}
		}

		SAFE_FREE(mask);
		if (ff_searchcount > 0 && ff_eos == 0 && finfo.name) {
			mask = SMB_STRDUP(finfo.name);
		} else {
			mask = SMB_STRDUP("");
		}
		if (!mask) {
			SAFE_FREE(rdata);
			SAFE_FREE(rparam);
			break;
		}

		/* grab the data for later use */
		/* and add them to the dirlist pool */
		dirlist = (char *)SMB_REALLOC(dirlist,dirlist_len + data_len);

		if (!dirlist) {
			DEBUG(0,("cli_list_new: Failed to expand dirlist\n"));
			SAFE_FREE(rdata);
			SAFE_FREE(rparam);
			break;
		}

		memcpy(dirlist+dirlist_len,p,data_len);
		dirlist_len += data_len;

		total_received += ff_searchcount;

		SAFE_FREE(rdata);
		SAFE_FREE(rparam);

		DEBUG(3,("received %d entries (eos=%d)\n",
			 ff_searchcount,ff_eos));

		if (ff_searchcount > 0)
			loop_count = 0;

		First = False;
	}

        /* see if the server disconnected or the connection otherwise failed */
        if (cli_is_error(cli)) {
                total_received = -1;
        } else {
                /* no connection problem.  let user function add each entry */
		rdata_end = dirlist + dirlist_len;
                for (p=dirlist,i=0;i<total_received;i++) {
                        p += interpret_long_filename(frame,
							cli,
							info_level,
							p,
							rdata_end,
							&finfo,
							NULL,
							NULL);
			if (!finfo.name) {
				DEBUG(0,("cli_list_new: unable to parse name from info level %d\n",
					info_level));
				break;
			}
                        fn(cli->dfs_mountpoint, &finfo, Mask, state);
                }
        }

	/* free up the dirlist buffer and last name raw blob */
	SAFE_FREE(dirlist);
	data_blob_free(&last_name_raw);
	SAFE_FREE(mask);
	TALLOC_FREE(frame);
	return(total_received);
}

/****************************************************************************
 Interpret a short filename structure.
 The length of the structure is returned.
****************************************************************************/

static bool interpret_short_filename(TALLOC_CTX *ctx,
				struct cli_state *cli,
				char *p,
				file_info *finfo)
{
	size_t ret;
	ZERO_STRUCTP(finfo);

	finfo->cli = cli;
	finfo->mode = CVAL(p,21);

	/* this date is converted to GMT by make_unix_date */
	finfo->ctime_ts.tv_sec = cli_make_unix_date(cli, p+22);
	finfo->ctime_ts.tv_nsec = 0;
	finfo->mtime_ts.tv_sec = finfo->atime_ts.tv_sec = finfo->ctime_ts.tv_sec;
	finfo->mtime_ts.tv_nsec = finfo->atime_ts.tv_nsec = 0;
	finfo->size = IVAL(p,26);
	ret = clistr_pull_talloc(ctx,
			cli->inbuf,
			&finfo->name,
			p+30,
			12,
			STR_ASCII);
	if (ret == (size_t)-1) {
		return false;
	}

	if (finfo->name) {
		strlcpy(finfo->short_name,
			finfo->name,
			sizeof(finfo->short_name));
	}
	return true;
	return(DIR_STRUCT_SIZE);
}

/****************************************************************************
 Do a directory listing, calling fn on each file found.
 this uses the old SMBsearch interface. It is needed for testing Samba,
 but should otherwise not be used.
****************************************************************************/

int cli_list_old(struct cli_state *cli,const char *Mask,uint16 attribute,
		 void (*fn)(const char *, file_info *, const char *, void *), void *state)
{
	char *p;
	int received = 0;
	bool first = True;
	char status[21];
	int num_asked = (cli->max_xmit - 100)/DIR_STRUCT_SIZE;
	int num_received = 0;
	int i;
	char *dirlist = NULL;
	char *mask = NULL;
	TALLOC_CTX *frame = NULL;

	ZERO_ARRAY(status);

	mask = SMB_STRDUP(Mask);
	if (!mask) {
		return -1;
	}

	while (1) {
		memset(cli->outbuf,'\0',smb_size);
		memset(cli->inbuf,'\0',smb_size);

		cli_set_message(cli->outbuf,2,0,True);

		SCVAL(cli->outbuf,smb_com,SMBsearch);

		SSVAL(cli->outbuf,smb_tid,cli->cnum);
		cli_setup_packet(cli);

		SSVAL(cli->outbuf,smb_vwv0,num_asked);
		SSVAL(cli->outbuf,smb_vwv1,attribute);

		p = smb_buf(cli->outbuf);
		*p++ = 4;

		p += clistr_push(cli, p, first?mask:"",
				cli->bufsize - PTR_DIFF(p,cli->outbuf),
				STR_TERMINATE);
		*p++ = 5;
		if (first) {
			SSVAL(p,0,0);
			p += 2;
		} else {
			SSVAL(p,0,21);
			p += 2;
			memcpy(p,status,21);
			p += 21;
		}

		cli_setup_bcc(cli, p);
		cli_send_smb(cli);
		if (!cli_receive_smb(cli)) break;

		received = SVAL(cli->inbuf,smb_vwv0);
		if (received <= 0) break;

		/* Ensure we received enough data. */
		if ((cli->inbuf+4+smb_len(cli->inbuf) - (smb_buf(cli->inbuf)+3)) <
				received*DIR_STRUCT_SIZE) {
			break;
		}

		first = False;

		dirlist = (char *)SMB_REALLOC(
			dirlist,(num_received + received)*DIR_STRUCT_SIZE);
		if (!dirlist) {
			DEBUG(0,("cli_list_old: failed to expand dirlist"));
			SAFE_FREE(mask);
			return 0;
		}

		p = smb_buf(cli->inbuf) + 3;

		memcpy(dirlist+num_received*DIR_STRUCT_SIZE,
		       p,received*DIR_STRUCT_SIZE);

		memcpy(status,p + ((received-1)*DIR_STRUCT_SIZE),21);

		num_received += received;

		if (cli_is_error(cli)) break;
	}

	if (!first) {
		memset(cli->outbuf,'\0',smb_size);
		memset(cli->inbuf,'\0',smb_size);

		cli_set_message(cli->outbuf,2,0,True);
		SCVAL(cli->outbuf,smb_com,SMBfclose);
		SSVAL(cli->outbuf,smb_tid,cli->cnum);
		cli_setup_packet(cli);

		SSVAL(cli->outbuf, smb_vwv0, 0); /* find count? */
		SSVAL(cli->outbuf, smb_vwv1, attribute);

		p = smb_buf(cli->outbuf);
		*p++ = 4;
		fstrcpy(p, "");
		p += strlen(p) + 1;
		*p++ = 5;
		SSVAL(p, 0, 21);
		p += 2;
		memcpy(p,status,21);
		p += 21;

		cli_setup_bcc(cli, p);
		cli_send_smb(cli);
		if (!cli_receive_smb(cli)) {
			DEBUG(0,("Error closing search: %s\n",cli_errstr(cli)));
		}
	}

	frame = talloc_stackframe();
	for (p=dirlist,i=0;i<num_received;i++) {
		file_info finfo;
		if (!interpret_short_filename(frame, cli, p, &finfo)) {
			break;
		}
		p += DIR_STRUCT_SIZE;
		fn("\\", &finfo, Mask, state);
	}
	TALLOC_FREE(frame);

	SAFE_FREE(mask);
	SAFE_FREE(dirlist);
	return(num_received);
}

/****************************************************************************
 Do a directory listing, calling fn on each file found.
 This auto-switches between old and new style.
****************************************************************************/

int cli_list(struct cli_state *cli,const char *Mask,uint16 attribute,
	     void (*fn)(const char *, file_info *, const char *, void *), void *state)
{
	if (cli->protocol <= PROTOCOL_LANMAN1)
		return cli_list_old(cli, Mask, attribute, fn, state);
	return cli_list_new(cli, Mask, attribute, fn, state);
}

/* 
   Unix SMB/CIFS implementation.
   Main SMB reply routines
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Andrew Bartlett      2001
   Copyright (C) Jeremy Allison 1992-2007.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
/*
   This file handles most of the reply_ calls that the server
   makes to handle specific protocols
*/

#include "includes.h"

/* look in server.c for some explanation of these variables */
extern enum protocol_types Protocol;
extern int max_send;
extern int max_recv;
unsigned int smb_echo_count = 0;
extern uint32 global_client_caps;

extern struct current_user current_user;
extern BOOL global_encrypted_passwords_negotiated;

/****************************************************************************
 Ensure we check the path in *exactly* the same way as W2K for a findfirst/findnext
 path or anything including wildcards.
 We're assuming here that '/' is not the second byte in any multibyte char
 set (a safe assumption). '\\' *may* be the second byte in a multibyte char
 set.
****************************************************************************/

/* Custom version for processing POSIX paths. */
#define IS_PATH_SEP(c,posix_only) ((c) == '/' || (!(posix_only) && (c) == '\\'))

NTSTATUS check_path_syntax_internal(pstring destname,
				    const pstring srcname,
				    BOOL posix_path,
				    BOOL *p_last_component_contains_wcard)
{
	char *d = destname;
	const char *s = srcname;
	NTSTATUS ret = NT_STATUS_OK;
	BOOL start_of_name_component = True;

	*p_last_component_contains_wcard = False;

	while (*s) {
		if (IS_PATH_SEP(*s,posix_path)) {
			/*
			 * Safe to assume is not the second part of a mb char as this is handled below.
			 */
			/* Eat multiple '/' or '\\' */
			while (IS_PATH_SEP(*s,posix_path)) {
				s++;
			}
			if ((d != destname) && (*s != '\0')) {
				/* We only care about non-leading or trailing '/' or '\\' */
				*d++ = '/';
			}

			start_of_name_component = True;
			/* New component. */
			*p_last_component_contains_wcard = False;
			continue;
		}

		if (start_of_name_component) {
			if ((s[0] == '.') && (s[1] == '.') && (IS_PATH_SEP(s[2],posix_path) || s[2] == '\0')) {
				/* Uh oh - "/../" or "\\..\\"  or "/..\0" or "\\..\0" ! */

				/*
				 * No mb char starts with '.' so we're safe checking the directory separator here.
				 */

				/* If  we just added a '/' - delete it */
				if ((d > destname) && (*(d-1) == '/')) {
					*(d-1) = '\0';
					d--;
				}

				/* Are we at the start ? Can't go back further if so. */
				if (d <= destname) {
					ret = NT_STATUS_OBJECT_PATH_SYNTAX_BAD;
					break;
				}
				/* Go back one level... */
				/* We know this is safe as '/' cannot be part of a mb sequence. */
				/* NOTE - if this assumption is invalid we are not in good shape... */
				/* Decrement d first as d points to the *next* char to write into. */
				for (d--; d > destname; d--) {
					if (*d == '/')
						break;
				}
				s += 2; /* Else go past the .. */
				/* We're still at the start of a name component, just the previous one. */
				continue;

			} else if ((s[0] == '.') && ((s[1] == '\0') || IS_PATH_SEP(s[1],posix_path))) {
				if (posix_path) {
					/* Eat the '.' */
					s++;
					continue;
				}
			}

		}

		if (!(*s & 0x80)) {
			if (!posix_path) {
				if (*s <= 0x1f) {
					return NT_STATUS_OBJECT_NAME_INVALID;
				}
				switch (*s) {
					case '*':
					case '?':
					case '<':
					case '>':
					case '"':
						*p_last_component_contains_wcard = True;
						break;
					default:
						break;
				}
			}
			*d++ = *s++;
		} else {
			size_t siz;
			/* Get the size of the next MB character. */
			next_codepoint(s,&siz);
			switch(siz) {
				case 5:
					*d++ = *s++;
					/*fall through*/
				case 4:
					*d++ = *s++;
					/*fall through*/
				case 3:
					*d++ = *s++;
					/*fall through*/
				case 2:
					*d++ = *s++;
					/*fall through*/
				case 1:
					*d++ = *s++;
					break;
				default:
					DEBUG(0,("check_path_syntax_internal: character length assumptions invalid !\n"));
					*d = '\0';
					return NT_STATUS_INVALID_PARAMETER;
			}
		}
		start_of_name_component = False;
	}

	*d = '\0';
	return ret;
}

/****************************************************************************
 Ensure we check the path in *exactly* the same way as W2K for regular pathnames.
 No wildcards allowed.
****************************************************************************/

NTSTATUS check_path_syntax(pstring destname, const pstring srcname)
{
	BOOL ignore;
	return check_path_syntax_internal(destname, srcname, False, &ignore);
}

/****************************************************************************
 Ensure we check the path in *exactly* the same way as W2K for regular pathnames.
 Wildcards allowed - p_contains_wcard returns true if the last component contained
 a wildcard.
****************************************************************************/

NTSTATUS check_path_syntax_wcard(pstring destname, const pstring srcname, BOOL *p_contains_wcard)
{
	return check_path_syntax_internal(destname, srcname, False, p_contains_wcard);
}

/****************************************************************************
 Check the path for a POSIX client.
 We're assuming here that '/' is not the second byte in any multibyte char
 set (a safe assumption).
****************************************************************************/

NTSTATUS check_path_syntax_posix(pstring destname, const pstring srcname)
{
	BOOL ignore;
	return check_path_syntax_internal(destname, srcname, True, &ignore);
}

/****************************************************************************
 Pull a string and check the path allowing a wilcard - provide for error return.
****************************************************************************/

size_t srvstr_get_path_wcard(char *inbuf, char *dest, const char *src, size_t dest_len, size_t src_len, int flags,
				NTSTATUS *err, BOOL *contains_wcard)
{
	pstring tmppath;
	char *tmppath_ptr = tmppath;
	size_t ret;
#ifdef DEVELOPER
	SMB_ASSERT(dest_len == sizeof(pstring));
#endif

	if (src_len == 0) {
		ret = srvstr_pull_buf( inbuf, tmppath_ptr, src, dest_len, flags);
	} else {
		ret = srvstr_pull( inbuf, tmppath_ptr, src, dest_len, src_len, flags);
	}

	*contains_wcard = False;

	if (SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES) {
		/* 
		 * For a DFS path the function parse_dfs_path()
		 * will do the path processing, just make a copy.
		 */
		pstrcpy(dest, tmppath);
		*err = NT_STATUS_OK;
		return ret;
	}

	if (lp_posix_pathnames()) {
		*err = check_path_syntax_posix(dest, tmppath);
	} else {
		*err = check_path_syntax_wcard(dest, tmppath, contains_wcard);
	}

	return ret;
}

/****************************************************************************
 Pull a string and check the path - provide for error return.
****************************************************************************/

size_t srvstr_get_path(char *inbuf, char *dest, const char *src, size_t dest_len, size_t src_len, int flags, NTSTATUS *err)
{
	pstring tmppath;
	char *tmppath_ptr = tmppath;
	size_t ret;
#ifdef DEVELOPER
	SMB_ASSERT(dest_len == sizeof(pstring));
#endif

	if (src_len == 0) {
		ret = srvstr_pull_buf( inbuf, tmppath_ptr, src, dest_len, flags);
	} else {
		ret = srvstr_pull( inbuf, tmppath_ptr, src, dest_len, src_len, flags);
	}

	if (SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES) {
		/* 
		 * For a DFS path the function parse_dfs_path()
		 * will do the path processing, just make a copy.
		 */
		pstrcpy(dest, tmppath);
		*err = NT_STATUS_OK;
		return ret;
	}

	if (lp_posix_pathnames()) {
		*err = check_path_syntax_posix(dest, tmppath);
	} else {
		*err = check_path_syntax(dest, tmppath);
	}

	return ret;
}

/****************************************************************************
 Reply to a special message.
****************************************************************************/

int reply_special(char *inbuf,char *outbuf)
{
	int outsize = 4;
	int msg_type = CVAL(inbuf,0);
	int msg_flags = CVAL(inbuf,1);
	fstring name1,name2;
	char name_type = 0;
	
	static BOOL already_got_session = False;

	*name1 = *name2 = 0;
	
	memset(outbuf,'\0',smb_size);

	smb_setlen(outbuf,0);
	
	switch (msg_type) {
	case 0x81: /* session request */
		
		if (already_got_session) {
			exit_server_cleanly("multiple session request not permitted");
		}
		
		SCVAL(outbuf,0,0x82);
		SCVAL(outbuf,3,0);
		if (name_len(inbuf+4) > 50 || 
		    name_len(inbuf+4 + name_len(inbuf + 4)) > 50) {
			DEBUG(0,("Invalid name length in session request\n"));
			return(0);
		}
		name_extract(inbuf,4,name1);
		name_type = name_extract(inbuf,4 + name_len(inbuf + 4),name2);
		DEBUG(2,("netbios connect: name1=%s name2=%s\n",
			 name1,name2));      

		set_local_machine_name(name1, True);
		set_remote_machine_name(name2, True);

		DEBUG(2,("netbios connect: local=%s remote=%s, name type = %x\n",
			 get_local_machine_name(), get_remote_machine_name(),
			 name_type));

		if (name_type == 'R') {
			/* We are being asked for a pathworks session --- 
			   no thanks! */
			SCVAL(outbuf, 0,0x83);
			break;
		}

		/* only add the client's machine name to the list
		   of possibly valid usernames if we are operating
		   in share mode security */
		if (lp_security() == SEC_SHARE) {
			add_session_user(get_remote_machine_name());
		}

		reload_services(True);
		reopen_logs();

		already_got_session = True;
		break;
		
	case 0x89: /* session keepalive request 
		      (some old clients produce this?) */
		SCVAL(outbuf,0,SMBkeepalive);
		SCVAL(outbuf,3,0);
		break;
		
	case 0x82: /* positive session response */
	case 0x83: /* negative session response */
	case 0x84: /* retarget session response */
		DEBUG(0,("Unexpected session response\n"));
		break;
		
	case SMBkeepalive: /* session keepalive */
	default:
		return(0);
	}
	
	DEBUG(5,("init msg_type=0x%x msg_flags=0x%x\n",
		    msg_type, msg_flags));
	
	return(outsize);
}

/****************************************************************************
 Reply to a tcon.
 conn POINTER CAN BE NULL HERE !
****************************************************************************/

int reply_tcon(connection_struct *conn,
	       char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	const char *service;
	pstring service_buf;
	pstring password;
	pstring dev;
	int outsize = 0;
	uint16 vuid = SVAL(inbuf,smb_uid);
	int pwlen=0;
	NTSTATUS nt_status;
	char *p;
	DATA_BLOB password_blob;
	
	START_PROFILE(SMBtcon);

	*service_buf = *password = *dev = 0;

	p = smb_buf(inbuf)+1;
	p += srvstr_pull_buf(inbuf, service_buf, p, sizeof(service_buf), STR_TERMINATE) + 1;
	pwlen = srvstr_pull_buf(inbuf, password, p, sizeof(password), STR_TERMINATE) + 1;
	p += pwlen;
	p += srvstr_pull_buf(inbuf, dev, p, sizeof(dev), STR_TERMINATE) + 1;

	p = strrchr_m(service_buf,'\\');
	if (p) {
		service = p+1;
	} else {
		service = service_buf;
	}

	password_blob = data_blob(password, pwlen+1);

	conn = make_connection(service,password_blob,dev,vuid,&nt_status);

	data_blob_clear_free(&password_blob);
  
	if (!conn) {
		END_PROFILE(SMBtcon);
		return ERROR_NT(nt_status);
	}
  
	outsize = set_message(outbuf,2,0,True);
	SSVAL(outbuf,smb_vwv0,max_recv);
	SSVAL(outbuf,smb_vwv1,conn->cnum);
	SSVAL(outbuf,smb_tid,conn->cnum);
  
	DEBUG(3,("tcon service=%s cnum=%d\n", 
		 service, conn->cnum));
  
	END_PROFILE(SMBtcon);
	return(outsize);
}

/****************************************************************************
 Reply to a tcon and X.
 conn POINTER CAN BE NULL HERE !
****************************************************************************/

int reply_tcon_and_X(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
	fstring service;
	DATA_BLOB password;

	/* what the cleint thinks the device is */
	fstring client_devicetype;
	/* what the server tells the client the share represents */
	const char *server_devicetype;
	NTSTATUS nt_status;
	uint16 vuid = SVAL(inbuf,smb_uid);
	int passlen = SVAL(inbuf,smb_vwv3);
	pstring path;
	char *p, *q;
	uint16 tcon_flags = SVAL(inbuf,smb_vwv2);
	
	START_PROFILE(SMBtconX);	

	*service = *client_devicetype = 0;

	/* we might have to close an old one */
	if ((SVAL(inbuf,smb_vwv2) & 0x1) && conn) {
		close_cnum(conn,vuid);
	}

	if (passlen > MAX_PASS_LEN) {
		return ERROR_DOS(ERRDOS,ERRbuftoosmall);
	}
 
	if (global_encrypted_passwords_negotiated) {
		password = data_blob(smb_buf(inbuf),passlen);
		if (lp_security() == SEC_SHARE) {
			/*
			 * Security = share always has a pad byte
			 * after the password.
			 */
			p = smb_buf(inbuf) + passlen + 1;
		} else {
			p = smb_buf(inbuf) + passlen;
		}
	} else {
		password = data_blob(smb_buf(inbuf),passlen+1);
		/* Ensure correct termination */
		password.data[passlen]=0;
		p = smb_buf(inbuf) + passlen + 1;
	}

	p += srvstr_pull_buf(inbuf, path, p, sizeof(path), STR_TERMINATE);

	/*
	 * the service name can be either: \\server\share
	 * or share directly like on the DELL PowerVault 705
	 */
	if (*path=='\\') {	
		q = strchr_m(path+2,'\\');
		if (!q) {
			END_PROFILE(SMBtconX);
			return(ERROR_DOS(ERRDOS,ERRnosuchshare));
		}
		fstrcpy(service,q+1);
	}
	else
		fstrcpy(service,path);
		
	p += srvstr_pull(inbuf, client_devicetype, p, sizeof(client_devicetype), 6, STR_ASCII);

	DEBUG(4,("Client requested device type [%s] for share [%s]\n", client_devicetype, service));

	conn = make_connection(service,password,client_devicetype,vuid,&nt_status);
	
	data_blob_clear_free(&password);

	if (!conn) {
		END_PROFILE(SMBtconX);
		return ERROR_NT(nt_status);
	}

	if ( IS_IPC(conn) )
		server_devicetype = "IPC";
	else if ( IS_PRINT(conn) )
		server_devicetype = "LPT1:";
	else 
		server_devicetype = "A:";

	if (Protocol < PROTOCOL_NT1) {
		set_message(outbuf,2,0,True);
		p = smb_buf(outbuf);
		p += srvstr_push(outbuf, p, server_devicetype, BUFFER_SIZE - (p - outbuf),
				 STR_TERMINATE|STR_ASCII);
		set_message_end(outbuf,p);
	} else {
		/* NT sets the fstype of IPC$ to the null string */
		const char *fstype = IS_IPC(conn) ? "" : lp_fstype(SNUM(conn));
		
		if (tcon_flags & TCONX_FLAG_EXTENDED_RESPONSE) {
			/* Return permissions. */
			uint32 perm1 = 0;
			uint32 perm2 = 0;

			set_message(outbuf,7,0,True);

			if (IS_IPC(conn)) {
				perm1 = FILE_ALL_ACCESS;
				perm2 = FILE_ALL_ACCESS;
			} else {
				perm1 = CAN_WRITE(conn) ?
						SHARE_ALL_ACCESS :
						SHARE_READ_ONLY;
			}

			SIVAL(outbuf, smb_vwv3, perm1);
			SIVAL(outbuf, smb_vwv5, perm2);
		} else {
			set_message(outbuf,3,0,True);
		}

		p = smb_buf(outbuf);
		p += srvstr_push(outbuf, p, server_devicetype, BUFFER_SIZE - (p - outbuf),
				 STR_TERMINATE|STR_ASCII);
		p += srvstr_push(outbuf, p, fstype, BUFFER_SIZE - (p - outbuf),
				 STR_TERMINATE);
		
		set_message_end(outbuf,p);
		
		/* what does setting this bit do? It is set by NT4 and
		   may affect the ability to autorun mounted cdroms */
		SSVAL(outbuf, smb_vwv2, SMB_SUPPORT_SEARCH_BITS|
				(lp_csc_policy(SNUM(conn)) << 2));
		
		init_dfsroot(conn, inbuf, outbuf);
	}

  
	DEBUG(3,("tconX service=%s \n",
		 service));
  
	/* set the incoming and outgoing tid to the just created one */
	SSVAL(inbuf,smb_tid,conn->cnum);
	SSVAL(outbuf,smb_tid,conn->cnum);

	END_PROFILE(SMBtconX);
	return chain_reply(inbuf,outbuf,length,bufsize);
}

/****************************************************************************
 Reply to an unknown type.
****************************************************************************/

int reply_unknown(char *inbuf,char *outbuf)
{
	int type;
	type = CVAL(inbuf,smb_com);
  
	DEBUG(0,("unknown command type (%s): type=%d (0x%X)\n",
		 smb_fn_name(type), type, type));
  
	return(ERROR_DOS(ERRSRV,ERRunknownsmb));
}

/****************************************************************************
 Reply to an ioctl.
 conn POINTER CAN BE NULL HERE !
****************************************************************************/

int reply_ioctl(connection_struct *conn,
		char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	uint16 device     = SVAL(inbuf,smb_vwv1);
	uint16 function   = SVAL(inbuf,smb_vwv2);
	uint32 ioctl_code = (device << 16) + function;
	int replysize, outsize;
	char *p;
	START_PROFILE(SMBioctl);

	DEBUG(4, ("Received IOCTL (code 0x%x)\n", ioctl_code));

	switch (ioctl_code) {
	    case IOCTL_QUERY_JOB_INFO:
		replysize = 32;
		break;
	    default:
		END_PROFILE(SMBioctl);
		return(ERROR_DOS(ERRSRV,ERRnosupport));
	}

	outsize = set_message(outbuf,8,replysize+1,True);
	SSVAL(outbuf,smb_vwv1,replysize); /* Total data bytes returned */
	SSVAL(outbuf,smb_vwv5,replysize); /* Data bytes this buffer */
	SSVAL(outbuf,smb_vwv6,52);        /* Offset to data */
	p = smb_buf(outbuf) + 1;          /* Allow for alignment */

	switch (ioctl_code) {
		case IOCTL_QUERY_JOB_INFO:		    
		{
			files_struct *fsp = file_fsp(inbuf,smb_vwv0);
			if (!fsp) {
				END_PROFILE(SMBioctl);
				return(UNIXERROR(ERRDOS,ERRbadfid));
			}
			SSVAL(p,0,fsp->rap_print_jobid);             /* Job number */
			srvstr_push(outbuf, p+2, global_myname(), 15, STR_TERMINATE|STR_ASCII);
			if (conn) {
				srvstr_push(outbuf, p+18, lp_servicename(SNUM(conn)), 13, STR_TERMINATE|STR_ASCII);
			}
			break;
		}
	}

	END_PROFILE(SMBioctl);
	return outsize;
}

/****************************************************************************
 Strange checkpath NTSTATUS mapping.
****************************************************************************/

static NTSTATUS map_checkpath_error(const char *inbuf, NTSTATUS status)
{
	/* Strange DOS error code semantics only for checkpath... */
	if (!(SVAL(inbuf,smb_flg2) & FLAGS2_32_BIT_ERROR_CODES)) {
		if (NT_STATUS_EQUAL(NT_STATUS_OBJECT_NAME_INVALID,status)) {
			/* We need to map to ERRbadpath */
			return NT_STATUS_OBJECT_PATH_NOT_FOUND;
		}
	}
	return status;
}
	
/****************************************************************************
 Reply to a checkpath.
****************************************************************************/

int reply_checkpath(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int outsize = 0;
	pstring name;
	SMB_STRUCT_STAT sbuf;
	NTSTATUS status;

	START_PROFILE(SMBcheckpath);

	srvstr_get_path(inbuf, name, smb_buf(inbuf) + 1, sizeof(name), 0, STR_TERMINATE, &status);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBcheckpath);
		status = map_checkpath_error(inbuf, status);
		return ERROR_NT(status);
	}

	status = resolve_dfspath(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, name);
	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			END_PROFILE(SMBcheckpath);
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		goto path_err;
	}

	DEBUG(3,("reply_checkpath %s mode=%d\n", name, (int)SVAL(inbuf,smb_vwv0)));

	status = unix_convert(conn, name, False, NULL, &sbuf);
	if (!NT_STATUS_IS_OK(status)) {
		goto path_err;
	}

	status = check_name(conn, name);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3,("reply_checkpath: check_name of %s failed (%s)\n",name,nt_errstr(status)));
		goto path_err;
	}

	if (!VALID_STAT(sbuf) && (SMB_VFS_STAT(conn,name,&sbuf) != 0)) {
		DEBUG(3,("reply_checkpath: stat of %s failed (%s)\n",name,strerror(errno)));
		status = map_nt_error_from_unix(errno);
		goto path_err;
	}

	if (!S_ISDIR(sbuf.st_mode)) {
		END_PROFILE(SMBcheckpath);
		return ERROR_BOTH(NT_STATUS_NOT_A_DIRECTORY,ERRDOS,ERRbadpath);
	}

	outsize = set_message(outbuf,0,0,False);

	END_PROFILE(SMBcheckpath);
	return outsize;

  path_err:

	END_PROFILE(SMBcheckpath);

	/* We special case this - as when a Windows machine
		is parsing a path is steps through the components
		one at a time - if a component fails it expects
		ERRbadpath, not ERRbadfile.
	*/
	status = map_checkpath_error(inbuf, status);
	if(NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
		/*
		 * Windows returns different error codes if
		 * the parent directory is valid but not the
		 * last component - it returns NT_STATUS_OBJECT_NAME_NOT_FOUND
		 * for that case and NT_STATUS_OBJECT_PATH_NOT_FOUND
		 * if the path is invalid.
		 */
		return ERROR_BOTH(NT_STATUS_OBJECT_NAME_NOT_FOUND,ERRDOS,ERRbadpath);
	}

	return ERROR_NT(status);
}

/****************************************************************************
 Reply to a getatr.
****************************************************************************/

int reply_getatr(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	pstring fname;
	int outsize = 0;
	SMB_STRUCT_STAT sbuf;
	int mode=0;
	SMB_OFF_T size=0;
	time_t mtime=0;
	char *p;
	NTSTATUS status;

	START_PROFILE(SMBgetatr);

	p = smb_buf(inbuf) + 1;
	p += srvstr_get_path(inbuf, fname, p, sizeof(fname), 0, STR_TERMINATE, &status);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBgetatr);
		return ERROR_NT(status);
	}

	status = resolve_dfspath(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, fname);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBgetatr);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}
  
	/* dos smetimes asks for a stat of "" - it returns a "hidden directory"
		under WfWg - weird! */
	if (*fname == '\0') {
		mode = aHIDDEN | aDIR;
		if (!CAN_WRITE(conn)) {
			mode |= aRONLY;
		}
		size = 0;
		mtime = 0;
	} else {
		status = unix_convert(conn, fname, False, NULL,&sbuf);
		if (!NT_STATUS_IS_OK(status)) {
			END_PROFILE(SMBgetatr);
			return ERROR_NT(status);
		}
		status = check_name(conn, fname);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(3,("reply_getatr: check_name of %s failed (%s)\n",fname,nt_errstr(status)));
			END_PROFILE(SMBgetatr);
			return ERROR_NT(status);
		}
		if (!VALID_STAT(sbuf) && (SMB_VFS_STAT(conn,fname,&sbuf) != 0)) {
			DEBUG(3,("reply_getatr: stat of %s failed (%s)\n",fname,strerror(errno)));
			return UNIXERROR(ERRDOS,ERRbadfile);
		}

		mode = dos_mode(conn,fname,&sbuf);
		size = sbuf.st_size;
		mtime = sbuf.st_mtime;
		if (mode & aDIR) {
			size = 0;
		}
	}
  
	outsize = set_message(outbuf,10,0,True);

	SSVAL(outbuf,smb_vwv0,mode);
	if(lp_dos_filetime_resolution(SNUM(conn)) ) {
		srv_put_dos_date3(outbuf,smb_vwv1,mtime & ~1);
	} else {
		srv_put_dos_date3(outbuf,smb_vwv1,mtime);
	}
	SIVAL(outbuf,smb_vwv3,(uint32)size);

	if (Protocol >= PROTOCOL_NT1) {
		SSVAL(outbuf,smb_flg2,SVAL(outbuf, smb_flg2) | FLAGS2_IS_LONG_NAME);
	}
  
	DEBUG(3,("reply_getatr: name=%s mode=%d size=%u\n", fname, mode, (unsigned int)size ) );
  
	END_PROFILE(SMBgetatr);
	return(outsize);
}

/****************************************************************************
 Reply to a setatr.
****************************************************************************/

int reply_setatr(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	pstring fname;
	int outsize = 0;
	int mode;
	time_t mtime;
	SMB_STRUCT_STAT sbuf;
	char *p;
	NTSTATUS status;

	START_PROFILE(SMBsetatr);

	p = smb_buf(inbuf) + 1;
	p += srvstr_get_path(inbuf, fname, p, sizeof(fname), 0, STR_TERMINATE, &status);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBsetatr);
		return ERROR_NT(status);
	}

	status = resolve_dfspath(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, fname);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBsetatr);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}
  
	status = unix_convert(conn, fname, False, NULL, &sbuf);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBsetatr);
		return ERROR_NT(status);
	}

	status = check_name(conn, fname);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBsetatr);
		return ERROR_NT(status);
	}

	if (fname[0] == '.' && fname[1] == '\0') {
		/*
		 * Not sure here is the right place to catch this
		 * condition. Might be moved to somewhere else later -- vl
		 */
		END_PROFILE(SMBsetatr);
		return ERROR_NT(NT_STATUS_ACCESS_DENIED);
	}

	mode = SVAL(inbuf,smb_vwv0);
	mtime = srv_make_unix_date3(inbuf+smb_vwv1);
  
	if (mode != FILE_ATTRIBUTE_NORMAL) {
		if (VALID_STAT_OF_DIR(sbuf))
			mode |= aDIR;
		else
			mode &= ~aDIR;

		if (file_set_dosmode(conn,fname,mode,&sbuf,False) != 0) {
			END_PROFILE(SMBsetatr);
			return UNIXERROR(ERRDOS, ERRnoaccess);
		}
	}

	if (!set_filetime(conn,fname,convert_time_t_to_timespec(mtime))) {
		END_PROFILE(SMBsetatr);
		return UNIXERROR(ERRDOS, ERRnoaccess);
	}
 
	outsize = set_message(outbuf,0,0,False);
  
	DEBUG( 3, ( "setatr name=%s mode=%d\n", fname, mode ) );
  
	END_PROFILE(SMBsetatr);
	return(outsize);
}

/****************************************************************************
 Reply to a dskattr.
****************************************************************************/

int reply_dskattr(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int outsize = 0;
	SMB_BIG_UINT dfree,dsize,bsize;
	START_PROFILE(SMBdskattr);

	if (get_dfree_info(conn,".",True,&bsize,&dfree,&dsize) == (SMB_BIG_UINT)-1) {
		END_PROFILE(SMBdskattr);
		return(UNIXERROR(ERRHRD,ERRgeneral));
	}
  
	outsize = set_message(outbuf,5,0,True);
	
	if (Protocol <= PROTOCOL_LANMAN2) {
		double total_space, free_space;
		/* we need to scale this to a number that DOS6 can handle. We
		   use floating point so we can handle large drives on systems
		   that don't have 64 bit integers 

		   we end up displaying a maximum of 2G to DOS systems
		*/
		total_space = dsize * (double)bsize;
		free_space = dfree * (double)bsize;

		dsize = (total_space+63*512) / (64*512);
		dfree = (free_space+63*512) / (64*512);
		
		if (dsize > 0xFFFF) dsize = 0xFFFF;
		if (dfree > 0xFFFF) dfree = 0xFFFF;

		SSVAL(outbuf,smb_vwv0,dsize);
		SSVAL(outbuf,smb_vwv1,64); /* this must be 64 for dos systems */
		SSVAL(outbuf,smb_vwv2,512); /* and this must be 512 */
		SSVAL(outbuf,smb_vwv3,dfree);
	} else {
		SSVAL(outbuf,smb_vwv0,dsize);
		SSVAL(outbuf,smb_vwv1,bsize/512);
		SSVAL(outbuf,smb_vwv2,512);
		SSVAL(outbuf,smb_vwv3,dfree);
	}

	DEBUG(3,("dskattr dfree=%d\n", (unsigned int)dfree));

	END_PROFILE(SMBdskattr);
	return(outsize);
}

/****************************************************************************
 Reply to a search.
 Can be called from SMBsearch, SMBffirst or SMBfunique.
****************************************************************************/

int reply_search(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	pstring mask;
	pstring directory;
	pstring fname;
	SMB_OFF_T size;
	uint32 mode;
	time_t date;
	uint32 dirtype;
	int outsize = 0;
	unsigned int numentries = 0;
	unsigned int maxentries = 0;
	BOOL finished = False;
	char *p;
	int status_len;
	pstring path;
	char status[21];
	int dptr_num= -1;
	BOOL check_descend = False;
	BOOL expect_close = False;
	NTSTATUS nt_status;
	BOOL mask_contains_wcard = False;
	BOOL allow_long_path_components = (SVAL(inbuf,smb_flg2) & FLAGS2_LONG_PATH_COMPONENTS) ? True : False;

	START_PROFILE(SMBsearch);

	if (lp_posix_pathnames()) {
		END_PROFILE(SMBsearch);
		return reply_unknown(inbuf, outbuf);
	}

	*mask = *directory = *fname = 0;

	/* If we were called as SMBffirst then we must expect close. */
	if(CVAL(inbuf,smb_com) == SMBffirst) {
		expect_close = True;
	}
  
	outsize = set_message(outbuf,1,3,True);
	maxentries = SVAL(inbuf,smb_vwv0); 
	dirtype = SVAL(inbuf,smb_vwv1);
	p = smb_buf(inbuf) + 1;
	p += srvstr_get_path_wcard(inbuf, path, p, sizeof(path), 0, STR_TERMINATE, &nt_status, &mask_contains_wcard);
	if (!NT_STATUS_IS_OK(nt_status)) {
		END_PROFILE(SMBsearch);
		return ERROR_NT(nt_status);
	}

	nt_status = resolve_dfspath_wcard(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, path, &mask_contains_wcard);
	if (!NT_STATUS_IS_OK(nt_status)) {
		END_PROFILE(SMBsearch);
		if (NT_STATUS_EQUAL(nt_status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(nt_status);
	}
  
	p++;
	status_len = SVAL(p, 0);
	p += 2;
  
	/* dirtype &= ~aDIR; */

	if (status_len == 0) {
		SMB_STRUCT_STAT sbuf;

		pstrcpy(directory,path);
		nt_status = unix_convert(conn, directory, True, NULL, &sbuf);
		if (!NT_STATUS_IS_OK(nt_status)) {
			END_PROFILE(SMBsearch);
			return ERROR_NT(nt_status);
		}

		nt_status = check_name(conn, directory);
		if (!NT_STATUS_IS_OK(nt_status)) {
			END_PROFILE(SMBsearch);
			return ERROR_NT(nt_status);
		}

		p = strrchr_m(directory,'/');
		if (!p) {
			pstrcpy(mask,directory);
			pstrcpy(directory,".");
		} else {
			*p = 0;
			pstrcpy(mask,p+1);
		}

		if (*directory == '\0') {
			pstrcpy(directory,".");
		}
		memset((char *)status,'\0',21);
		SCVAL(status,0,(dirtype & 0x1F));
	} else {
		int status_dirtype;

		memcpy(status,p,21);
		status_dirtype = CVAL(status,0) & 0x1F;
		if (status_dirtype != (dirtype & 0x1F)) {
			dirtype = status_dirtype;
		}

		conn->dirptr = dptr_fetch(status+12,&dptr_num);      
		if (!conn->dirptr) {
			goto SearchEmpty;
		}
		string_set(&conn->dirpath,dptr_path(dptr_num));
		pstrcpy(mask, dptr_wcard(dptr_num));
		/*
		 * For a 'continue' search we have no string. So
		 * check from the initial saved string.
		 */
		mask_contains_wcard = ms_has_wild(mask);
	}

	p = smb_buf(outbuf) + 3;
     
	if (status_len == 0) {
		nt_status = dptr_create(conn,
					directory,
					True,
					expect_close,
					SVAL(inbuf,smb_pid),
					mask,
					mask_contains_wcard,
					dirtype,
					&conn->dirptr);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return ERROR_NT(nt_status);
		}
		dptr_num = dptr_dnum(conn->dirptr);
	} else {
		dirtype = dptr_attr(dptr_num);
	}

	DEBUG(4,("dptr_num is %d\n",dptr_num));

	if ((dirtype&0x1F) == aVOLID) {	  
		memcpy(p,status,21);
		make_dir_struct(p,"???????????",volume_label(SNUM(conn)),
				0,aVOLID,0,!allow_long_path_components);
		dptr_fill(p+12,dptr_num);
		if (dptr_zero(p+12) && (status_len==0)) {
			numentries = 1;
		} else {
			numentries = 0;
		}
		p += DIR_STRUCT_SIZE;
	} else {
		unsigned int i;
		maxentries = MIN(maxentries, ((BUFFER_SIZE - (p - outbuf))/DIR_STRUCT_SIZE));

		DEBUG(8,("dirpath=<%s> dontdescend=<%s>\n",
			conn->dirpath,lp_dontdescend(SNUM(conn))));
		if (in_list(conn->dirpath, lp_dontdescend(SNUM(conn)),True)) {
			check_descend = True;
		}

		for (i=numentries;(i<maxentries) && !finished;i++) {
			finished = !get_dir_entry(conn,mask,dirtype,fname,&size,&mode,&date,check_descend);
			if (!finished) {
				memcpy(p,status,21);
				make_dir_struct(p,mask,fname,size, mode,date,
						!allow_long_path_components);
				if (!dptr_fill(p+12,dptr_num)) {
					break;
				}
				numentries++;
				p += DIR_STRUCT_SIZE;
			}
		}
	}

  SearchEmpty:

	/* If we were called as SMBffirst with smb_search_id == NULL
		and no entries were found then return error and close dirptr 
		(X/Open spec) */

	if (numentries == 0) {
		dptr_close(&dptr_num);
	} else if(expect_close && status_len == 0) {
		/* Close the dptr - we know it's gone */
		dptr_close(&dptr_num);
	}

	/* If we were called as SMBfunique, then we can close the dirptr now ! */
	if(dptr_num >= 0 && CVAL(inbuf,smb_com) == SMBfunique) {
		dptr_close(&dptr_num);
	}

	if ((numentries == 0) && !mask_contains_wcard) {
		return ERROR_BOTH(STATUS_NO_MORE_FILES,ERRDOS,ERRnofiles);
	}

	SSVAL(outbuf,smb_vwv0,numentries);
	SSVAL(outbuf,smb_vwv1,3 + numentries * DIR_STRUCT_SIZE);
	SCVAL(smb_buf(outbuf),0,5);
	SSVAL(smb_buf(outbuf),1,numentries*DIR_STRUCT_SIZE);

	/* The replies here are never long name. */
	SSVAL(outbuf,smb_flg2,SVAL(outbuf, smb_flg2) & (~FLAGS2_IS_LONG_NAME));
	if (!allow_long_path_components) {
		SSVAL(outbuf,smb_flg2,SVAL(outbuf, smb_flg2) & (~FLAGS2_LONG_PATH_COMPONENTS));
	}

	/* This SMB *always* returns ASCII names. Remove the unicode bit in flags2. */
	SSVAL(outbuf,smb_flg2, (SVAL(outbuf, smb_flg2) & (~FLAGS2_UNICODE_STRINGS)));
	  
	outsize += DIR_STRUCT_SIZE*numentries;
	smb_setlen(outbuf,outsize - 4);
  
	if ((! *directory) && dptr_path(dptr_num))
		slprintf(directory, sizeof(directory)-1, "(%s)",dptr_path(dptr_num));

	DEBUG( 4, ( "%s mask=%s path=%s dtype=%d nument=%u of %u\n",
		smb_fn_name(CVAL(inbuf,smb_com)), 
		mask, directory, dirtype, numentries, maxentries ) );

	END_PROFILE(SMBsearch);
	return(outsize);
}

/****************************************************************************
 Reply to a fclose (stop directory search).
****************************************************************************/

int reply_fclose(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int outsize = 0;
	int status_len;
	pstring path;
	char status[21];
	int dptr_num= -2;
	char *p;
	NTSTATUS err;
	BOOL path_contains_wcard = False;

	START_PROFILE(SMBfclose);

	if (lp_posix_pathnames()) {
		END_PROFILE(SMBfclose);
		return reply_unknown(inbuf, outbuf);
	}

	outsize = set_message(outbuf,1,0,True);
	p = smb_buf(inbuf) + 1;
	p += srvstr_get_path_wcard(inbuf, path, p, sizeof(path), 0, STR_TERMINATE, &err, &path_contains_wcard);
	if (!NT_STATUS_IS_OK(err)) {
		END_PROFILE(SMBfclose);
		return ERROR_NT(err);
	}
	p++;
	status_len = SVAL(p,0);
	p += 2;

	if (status_len == 0) {
		END_PROFILE(SMBfclose);
		return ERROR_DOS(ERRSRV,ERRsrverror);
	}

	memcpy(status,p,21);

	if(dptr_fetch(status+12,&dptr_num)) {
		/*  Close the dptr - we know it's gone */
		dptr_close(&dptr_num);
	}

	SSVAL(outbuf,smb_vwv0,0);

	DEBUG(3,("search close\n"));

	END_PROFILE(SMBfclose);
	return(outsize);
}

/****************************************************************************
 Reply to an open.
****************************************************************************/

int reply_open(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	pstring fname;
	int outsize = 0;
	uint32 fattr=0;
	SMB_OFF_T size = 0;
	time_t mtime=0;
	int info;
	SMB_STRUCT_STAT sbuf;
	files_struct *fsp;
	int oplock_request = CORE_OPLOCK_REQUEST(inbuf);
	int deny_mode;
	uint32 dos_attr = SVAL(inbuf,smb_vwv1);
	uint32 access_mask;
	uint32 share_mode;
	uint32 create_disposition;
	uint32 create_options = 0;
	NTSTATUS status;
	START_PROFILE(SMBopen);
 
	deny_mode = SVAL(inbuf,smb_vwv0);

	srvstr_get_path(inbuf, fname, smb_buf(inbuf)+1, sizeof(fname), 0, STR_TERMINATE, &status);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBopen);
		return ERROR_NT(status);
	}

	status = resolve_dfspath(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, fname);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBopen);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}

	status = unix_convert(conn, fname, False, NULL, &sbuf);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBopen);
		return ERROR_NT(status);
	}
    
	status = check_name(conn, fname);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBopen);
		return ERROR_NT(status);
	}

	if (!map_open_params_to_ntcreate(fname, deny_mode, OPENX_FILE_EXISTS_OPEN,
			&access_mask, &share_mode, &create_disposition, &create_options)) {
		END_PROFILE(SMBopen);
		return ERROR_NT(NT_STATUS_DOS(ERRDOS, ERRbadaccess));
	}

	status = open_file_ntcreate(conn,fname,&sbuf,
			access_mask,
			share_mode,
			create_disposition,
			create_options,
			dos_attr,
			oplock_request,
			&info, &fsp);

	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBopen);
		if (open_was_deferred(SVAL(inbuf,smb_mid))) {
			/* We have re-scheduled this call. */
			return -1;
		}
		return ERROR_OPEN(status);
	}

	size = sbuf.st_size;
	fattr = dos_mode(conn,fname,&sbuf);
	mtime = sbuf.st_mtime;

	if (fattr & aDIR) {
		DEBUG(3,("attempt to open a directory %s\n",fname));
		close_file(fsp,ERROR_CLOSE);
		END_PROFILE(SMBopen);
		return ERROR_DOS(ERRDOS,ERRnoaccess);
	}
  
	outsize = set_message(outbuf,7,0,True);
	SSVAL(outbuf,smb_vwv0,fsp->fnum);
	SSVAL(outbuf,smb_vwv1,fattr);
	if(lp_dos_filetime_resolution(SNUM(conn)) ) {
		srv_put_dos_date3(outbuf,smb_vwv2,mtime & ~1);
	} else {
		srv_put_dos_date3(outbuf,smb_vwv2,mtime);
	}
	SIVAL(outbuf,smb_vwv4,(uint32)size);
	SSVAL(outbuf,smb_vwv6,deny_mode);

	if (oplock_request && lp_fake_oplocks(SNUM(conn))) {
		SCVAL(outbuf,smb_flg,CVAL(outbuf,smb_flg)|CORE_OPLOCK_GRANTED);
	}
    
	if(EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type)) {
		SCVAL(outbuf,smb_flg,CVAL(outbuf,smb_flg)|CORE_OPLOCK_GRANTED);
	}
	END_PROFILE(SMBopen);
	return(outsize);
}

/****************************************************************************
 Reply to an open and X.
****************************************************************************/

int reply_open_and_X(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
	pstring fname;
	uint16 open_flags = SVAL(inbuf,smb_vwv2);
	int deny_mode = SVAL(inbuf,smb_vwv3);
	uint32 smb_attr = SVAL(inbuf,smb_vwv5);
	/* Breakout the oplock request bits so we can set the
		reply bits separately. */
	int ex_oplock_request = EXTENDED_OPLOCK_REQUEST(inbuf);
	int core_oplock_request = CORE_OPLOCK_REQUEST(inbuf);
	int oplock_request = ex_oplock_request | core_oplock_request;
#if 0
	int smb_sattr = SVAL(inbuf,smb_vwv4); 
	uint32 smb_time = make_unix_date3(inbuf+smb_vwv6);
#endif
	int smb_ofun = SVAL(inbuf,smb_vwv8);
	uint32 fattr=0;
	int mtime=0;
	SMB_STRUCT_STAT sbuf;
	int smb_action = 0;
	files_struct *fsp;
	NTSTATUS status;
	SMB_BIG_UINT allocation_size = (SMB_BIG_UINT)IVAL(inbuf,smb_vwv9);
	ssize_t retval = -1;
	uint32 access_mask;
	uint32 share_mode;
	uint32 create_disposition;
	uint32 create_options = 0;

	START_PROFILE(SMBopenX);

	/* If it's an IPC, pass off the pipe handler. */
	if (IS_IPC(conn)) {
		if (lp_nt_pipe_support()) {
			END_PROFILE(SMBopenX);
			return reply_open_pipe_and_X(conn, inbuf,outbuf,length,bufsize);
		} else {
			END_PROFILE(SMBopenX);
			return ERROR_DOS(ERRSRV,ERRaccess);
		}
	}

	/* XXXX we need to handle passed times, sattr and flags */
	srvstr_get_path(inbuf, fname, smb_buf(inbuf), sizeof(fname), 0, STR_TERMINATE, &status);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBopenX);
		return ERROR_NT(status);
	}

	status = resolve_dfspath(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, fname);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBopenX);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}

	status = unix_convert(conn, fname, False, NULL, &sbuf);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBopenX);
		return ERROR_NT(status);
	}

	status = check_name(conn, fname);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBopenX);
		return ERROR_NT(status);
	}

	if (!map_open_params_to_ntcreate(fname, deny_mode, smb_ofun,
				&access_mask,
				&share_mode,
				&create_disposition,
				&create_options)) {
		END_PROFILE(SMBopenX);
		return ERROR_NT(NT_STATUS_DOS(ERRDOS, ERRbadaccess));
	}

	status = open_file_ntcreate(conn,fname,&sbuf,
			access_mask,
			share_mode,
			create_disposition,
			create_options,
			smb_attr,
			oplock_request,
			&smb_action, &fsp);
      
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBopenX);
		if (open_was_deferred(SVAL(inbuf,smb_mid))) {
			/* We have re-scheduled this call. */
			return -1;
		}
		return ERROR_OPEN(status);
	}

	/* Setting the "size" field in vwv9 and vwv10 causes the file to be set to this size,
	   if the file is truncated or created. */
	if (((smb_action == FILE_WAS_CREATED) || (smb_action == FILE_WAS_OVERWRITTEN)) && allocation_size) {
		fsp->initial_allocation_size = smb_roundup(fsp->conn, allocation_size);
		if (vfs_allocate_file_space(fsp, fsp->initial_allocation_size) == -1) {
			close_file(fsp,ERROR_CLOSE);
			END_PROFILE(SMBopenX);
			return ERROR_NT(NT_STATUS_DISK_FULL);
		}
		retval = vfs_set_filelen(fsp, (SMB_OFF_T)allocation_size);
		if (retval < 0) {
			close_file(fsp,ERROR_CLOSE);
			END_PROFILE(SMBopenX);
			return ERROR_NT(NT_STATUS_DISK_FULL);
		}
		sbuf.st_size = get_allocation_size(conn,fsp,&sbuf);
	}

	fattr = dos_mode(conn,fname,&sbuf);
	mtime = sbuf.st_mtime;
	if (fattr & aDIR) {
		close_file(fsp,ERROR_CLOSE);
		END_PROFILE(SMBopenX);
		return ERROR_DOS(ERRDOS,ERRnoaccess);
	}

	/* If the caller set the extended oplock request bit
		and we granted one (by whatever means) - set the
		correct bit for extended oplock reply.
	*/

	if (ex_oplock_request && lp_fake_oplocks(SNUM(conn))) {
		smb_action |= EXTENDED_OPLOCK_GRANTED;
	}

	if(ex_oplock_request && EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type)) {
		smb_action |= EXTENDED_OPLOCK_GRANTED;
	}

	/* If the caller set the core oplock request bit
		and we granted one (by whatever means) - set the
		correct bit for core oplock reply.
	*/

	if (core_oplock_request && lp_fake_oplocks(SNUM(conn))) {
		SCVAL(outbuf,smb_flg,CVAL(outbuf,smb_flg)|CORE_OPLOCK_GRANTED);
	}

	if(core_oplock_request && EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type)) {
		SCVAL(outbuf,smb_flg,CVAL(outbuf,smb_flg)|CORE_OPLOCK_GRANTED);
	}

	if (open_flags & EXTENDED_RESPONSE_REQUIRED) {
		set_message(outbuf,19,0,True);
	} else {
		set_message(outbuf,15,0,True);
	}
	SSVAL(outbuf,smb_vwv2,fsp->fnum);
	SSVAL(outbuf,smb_vwv3,fattr);
	if(lp_dos_filetime_resolution(SNUM(conn)) ) {
		srv_put_dos_date3(outbuf,smb_vwv4,mtime & ~1);
	} else {
		srv_put_dos_date3(outbuf,smb_vwv4,mtime);
	}
	SIVAL(outbuf,smb_vwv6,(uint32)sbuf.st_size);
	SSVAL(outbuf,smb_vwv8,GET_OPENX_MODE(deny_mode));
	SSVAL(outbuf,smb_vwv11,smb_action);

	if (open_flags & EXTENDED_RESPONSE_REQUIRED) {
		SIVAL(outbuf, smb_vwv15, STD_RIGHT_ALL_ACCESS);
	}

	END_PROFILE(SMBopenX);
	return chain_reply(inbuf,outbuf,length,bufsize);
}

/****************************************************************************
 Reply to a SMBulogoffX.
 conn POINTER CAN BE NULL HERE !
****************************************************************************/

int reply_ulogoffX(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
	uint16 vuid = SVAL(inbuf,smb_uid);
	user_struct *vuser = get_valid_user_struct(vuid);
	START_PROFILE(SMBulogoffX);

	if(vuser == 0)
		DEBUG(3,("ulogoff, vuser id %d does not map to user.\n", vuid));

	/* in user level security we are supposed to close any files
		open by this user */
	if ((vuser != 0) && (lp_security() != SEC_SHARE))
		file_close_user(vuid);

	invalidate_vuid(vuid);

	set_message(outbuf,2,0,True);

	DEBUG( 3, ( "ulogoffX vuid=%d\n", vuid ) );

	END_PROFILE(SMBulogoffX);
	return chain_reply(inbuf,outbuf,length,bufsize);
}

/****************************************************************************
 Reply to a mknew or a create.
****************************************************************************/

int reply_mknew(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	pstring fname;
	int com;
	int outsize = 0;
	uint32 fattr = SVAL(inbuf,smb_vwv0);
	struct timespec ts[2];
	files_struct *fsp;
	int oplock_request = CORE_OPLOCK_REQUEST(inbuf);
	SMB_STRUCT_STAT sbuf;
	NTSTATUS status;
	uint32 access_mask = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
	uint32 share_mode = FILE_SHARE_READ|FILE_SHARE_WRITE;
	uint32 create_disposition;
	uint32 create_options = 0;

	START_PROFILE(SMBcreate);
 
	com = SVAL(inbuf,smb_com);

	ts[1] = convert_time_t_to_timespec(srv_make_unix_date3(inbuf + smb_vwv1)); /* mtime. */

	srvstr_get_path(inbuf, fname, smb_buf(inbuf) + 1, sizeof(fname), 0, STR_TERMINATE, &status);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBcreate);
		return ERROR_NT(status);
	}

	status = resolve_dfspath(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, fname);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBcreate);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}

	status = unix_convert(conn, fname, False, NULL, &sbuf);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBcreate);
		return ERROR_NT(status);
	}

	status = check_name(conn, fname);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBcreate);
		return ERROR_NT(status);
	}

	if (fattr & aVOLID) {
		DEBUG(0,("Attempt to create file (%s) with volid set - please report this\n",fname));
	}

	if(com == SMBmknew) {
		/* We should fail if file exists. */
		create_disposition = FILE_CREATE;
	} else {
		/* Create if file doesn't exist, truncate if it does. */
		create_disposition = FILE_OVERWRITE_IF;
	}

	/* Open file using ntcreate. */
	status = open_file_ntcreate(conn,fname,&sbuf,
				access_mask,
				share_mode,
				create_disposition,
				create_options,
				fattr,
				oplock_request,
				NULL, &fsp);
  
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBcreate);
		if (open_was_deferred(SVAL(inbuf,smb_mid))) {
			/* We have re-scheduled this call. */
			return -1;
		}
		return ERROR_OPEN(status);
	}
 
	ts[0] = get_atimespec(&sbuf); /* atime. */
	file_ntimes(conn, fname, ts);

	outsize = set_message(outbuf,1,0,True);
	SSVAL(outbuf,smb_vwv0,fsp->fnum);

	if (oplock_request && lp_fake_oplocks(SNUM(conn))) {
		SCVAL(outbuf,smb_flg,CVAL(outbuf,smb_flg)|CORE_OPLOCK_GRANTED);
	}
 
	if(EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type)) {
		SCVAL(outbuf,smb_flg,CVAL(outbuf,smb_flg)|CORE_OPLOCK_GRANTED);
	}
 
	DEBUG( 2, ( "reply_mknew: file %s\n", fname ) );
	DEBUG( 3, ( "reply_mknew %s fd=%d dmode=0x%x\n", fname, fsp->fh->fd, (unsigned int)fattr ) );

	END_PROFILE(SMBcreate);
	return(outsize);
}

/****************************************************************************
 Reply to a create temporary file.
****************************************************************************/

int reply_ctemp(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	pstring fname;
	int outsize = 0;
	uint32 fattr = SVAL(inbuf,smb_vwv0);
	files_struct *fsp;
	int oplock_request = CORE_OPLOCK_REQUEST(inbuf);
	int tmpfd;
	SMB_STRUCT_STAT sbuf;
	char *p, *s;
	NTSTATUS status;
	unsigned int namelen;

	START_PROFILE(SMBctemp);

	srvstr_get_path(inbuf, fname, smb_buf(inbuf)+1, sizeof(fname), 0, STR_TERMINATE, &status);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBctemp);
		return ERROR_NT(status);
	}
	if (*fname) {
		pstrcat(fname,"/TMXXXXXX");
	} else {
		pstrcat(fname,"TMXXXXXX");
	}

	status = resolve_dfspath(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, fname);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBctemp);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}

	status = unix_convert(conn, fname, False, NULL, &sbuf);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBctemp);
		return ERROR_NT(status);
	}

	status = check_name(conn, fname);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBctemp);
		return ERROR_NT(status);
	}
  
	tmpfd = smb_mkstemp(fname);
	if (tmpfd == -1) {
		END_PROFILE(SMBctemp);
		return(UNIXERROR(ERRDOS,ERRnoaccess));
	}

	SMB_VFS_STAT(conn,fname,&sbuf);

	/* We should fail if file does not exist. */
	status = open_file_ntcreate(conn,fname,&sbuf,
				FILE_GENERIC_READ | FILE_GENERIC_WRITE,
				FILE_SHARE_READ|FILE_SHARE_WRITE,
				FILE_OPEN,
				0,
				fattr,
				oplock_request,
				NULL, &fsp);

	/* close fd from smb_mkstemp() */
	close(tmpfd);

	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBctemp);
		if (open_was_deferred(SVAL(inbuf,smb_mid))) {
			/* We have re-scheduled this call. */
			return -1;
		}
		return ERROR_OPEN(status);
	}

	outsize = set_message(outbuf,1,0,True);
	SSVAL(outbuf,smb_vwv0,fsp->fnum);

	/* the returned filename is relative to the directory */
	s = strrchr_m(fname, '/');
	if (!s) {
		s = fname;
	} else {
		s++;
	}

	p = smb_buf(outbuf);
#if 0
	/* Tested vs W2K3 - this doesn't seem to be here - null terminated filename is the only
	   thing in the byte section. JRA */
	SSVALS(p, 0, -1); /* what is this? not in spec */
#endif
	namelen = srvstr_push(outbuf, p, s, BUFFER_SIZE - (p - outbuf), STR_ASCII|STR_TERMINATE);
	p += namelen;
	outsize = set_message_end(outbuf, p);

	if (oplock_request && lp_fake_oplocks(SNUM(conn))) {
		SCVAL(outbuf,smb_flg,CVAL(outbuf,smb_flg)|CORE_OPLOCK_GRANTED);
	}
  
	if (EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type)) {
		SCVAL(outbuf,smb_flg,CVAL(outbuf,smb_flg)|CORE_OPLOCK_GRANTED);
	}

	DEBUG( 2, ( "reply_ctemp: created temp file %s\n", fname ) );
	DEBUG( 3, ( "reply_ctemp %s fd=%d umode=0%o\n", fname, fsp->fh->fd,
			(unsigned int)sbuf.st_mode ) );

	END_PROFILE(SMBctemp);
	return(outsize);
}

/*******************************************************************
 Check if a user is allowed to rename a file.
********************************************************************/

static NTSTATUS can_rename(connection_struct *conn, char *fname, uint16 dirtype, SMB_STRUCT_STAT *pst, BOOL self_open)
{
	files_struct *fsp;
	uint32 fmode;
	NTSTATUS status;

	if (!CAN_WRITE(conn)) {
		return NT_STATUS_MEDIA_WRITE_PROTECTED;
	}

	fmode = dos_mode(conn,fname,pst);
	if ((fmode & ~dirtype) & (aHIDDEN | aSYSTEM)) {
		return NT_STATUS_NO_SUCH_FILE;
	}

	if (S_ISDIR(pst->st_mode)) {
		return NT_STATUS_OK;
	}

	status = open_file_ntcreate(conn, fname, pst,
				DELETE_ACCESS,
				/* If we're checking our fsp don't deny for delete. */
				self_open ?
					FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE :
					FILE_SHARE_READ|FILE_SHARE_WRITE,
				FILE_OPEN,
				0,
				FILE_ATTRIBUTE_NORMAL,
				0,
				NULL, &fsp);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	close_file(fsp,NORMAL_CLOSE);
	return NT_STATUS_OK;
}

/*******************************************************************
 Check if a user is allowed to delete a file.
********************************************************************/

static NTSTATUS can_delete(connection_struct *conn, char *fname,
			   uint32 dirtype, BOOL can_defer)
{
	SMB_STRUCT_STAT sbuf;
	uint32 fattr;
	files_struct *fsp;
	uint32 dirtype_orig = dirtype;
	NTSTATUS status;

	DEBUG(10,("can_delete: %s, dirtype = %d\n", fname, dirtype ));

	if (!CAN_WRITE(conn)) {
		return NT_STATUS_MEDIA_WRITE_PROTECTED;
	}

	if (SMB_VFS_LSTAT(conn,fname,&sbuf) != 0) {
		return map_nt_error_from_unix(errno);
	}

	fattr = dos_mode(conn,fname,&sbuf);

	if (dirtype & FILE_ATTRIBUTE_NORMAL) {
		dirtype = aDIR|aARCH|aRONLY;
	}

	dirtype &= (aDIR|aARCH|aRONLY|aHIDDEN|aSYSTEM);
	if (!dirtype) {
		return NT_STATUS_NO_SUCH_FILE;
	}

	if (!dir_check_ftype(conn, fattr, dirtype)) {
		if (fattr & aDIR) {
			return NT_STATUS_FILE_IS_A_DIRECTORY;
		}
		return NT_STATUS_NO_SUCH_FILE;
	}

	if (dirtype_orig & 0x8000) {
		/* These will never be set for POSIX. */
		return NT_STATUS_NO_SUCH_FILE;
	}

#if 0
	if ((fattr & dirtype) & FILE_ATTRIBUTE_DIRECTORY) {
                return NT_STATUS_FILE_IS_A_DIRECTORY;
        }

        if ((fattr & ~dirtype) & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)) {
                return NT_STATUS_NO_SUCH_FILE;
        }

	if (dirtype & 0xFF00) {
		/* These will never be set for POSIX. */
		return NT_STATUS_NO_SUCH_FILE;
	}

	dirtype &= 0xFF;
	if (!dirtype) {
		return NT_STATUS_NO_SUCH_FILE;
	}

	/* Can't delete a directory. */
	if (fattr & aDIR) {
		return NT_STATUS_FILE_IS_A_DIRECTORY;
	}
#endif

#if 0 /* JRATEST */
	else if (dirtype & aDIR) /* Asked for a directory and it isn't. */
		return NT_STATUS_OBJECT_NAME_INVALID;
#endif /* JRATEST */

	/* Fix for bug #3035 from SATOH Fumiyasu <fumiyas@miraclelinux.com>

	  On a Windows share, a file with read-only dosmode can be opened with
	  DELETE_ACCESS. But on a Samba share (delete readonly = no), it
	  fails with NT_STATUS_CANNOT_DELETE error.

	  This semantic causes a problem that a user can not
	  rename a file with read-only dosmode on a Samba share
	  from a Windows command prompt (i.e. cmd.exe, but can rename
	  from Windows Explorer).
	*/

	if (!lp_delete_readonly(SNUM(conn))) {
		if (fattr & aRONLY) {
			return NT_STATUS_CANNOT_DELETE;
		}
	}

	/* On open checks the open itself will check the share mode, so
	   don't do it here as we'll get it wrong. */

	status = open_file_ntcreate(conn, fname, &sbuf,
				    DELETE_ACCESS,
				    FILE_SHARE_NONE,
				    FILE_OPEN,
				    0,
				    FILE_ATTRIBUTE_NORMAL,
				    can_defer ? 0 : INTERNAL_OPEN_ONLY,
				    NULL, &fsp);

	if (NT_STATUS_IS_OK(status)) {
		close_file(fsp,NORMAL_CLOSE);
	}
	return status;
}

/****************************************************************************
 The guts of the unlink command, split out so it may be called by the NT SMB
 code.
****************************************************************************/

NTSTATUS unlink_internals(connection_struct *conn, uint32 dirtype,
			  char *name, BOOL has_wild, BOOL can_defer)
{
	pstring directory;
	pstring mask;
	char *p;
	int count=0;
	NTSTATUS status = NT_STATUS_OK;
	SMB_STRUCT_STAT sbuf;
	
	*directory = *mask = 0;
	
	status = unix_convert(conn, name, has_wild, NULL, &sbuf);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	
	p = strrchr_m(name,'/');
	if (!p) {
		pstrcpy(directory,".");
		pstrcpy(mask,name);
	} else {
		*p = 0;
		pstrcpy(directory,name);
		pstrcpy(mask,p+1);
	}
	
	/*
	 * We should only check the mangled cache
	 * here if unix_convert failed. This means
	 * that the path in 'mask' doesn't exist
	 * on the file system and so we need to look
	 * for a possible mangle. This patch from
	 * Tine Smukavec <valentin.smukavec@hermes.si>.
	 */
	
	if (!VALID_STAT(sbuf) && mangle_is_mangled(mask,conn->params))
		mangle_check_cache( mask, sizeof(pstring)-1, conn->params );
	
	if (!has_wild) {
		pstrcat(directory,"/");
		pstrcat(directory,mask);
		if (dirtype == 0) {
			dirtype = FILE_ATTRIBUTE_NORMAL;
		}

		status = check_name(conn, directory);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		status = can_delete(conn,directory,dirtype,can_defer);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		if (SMB_VFS_UNLINK(conn,directory) == 0) {
			count++;
			notify_fname(conn, NOTIFY_ACTION_REMOVED,
				     FILE_NOTIFY_CHANGE_FILE_NAME,
				     directory);
		}
	} else {
		struct smb_Dir *dir_hnd = NULL;
		long offset = 0;
		const char *dname;
		
		if ((dirtype & SAMBA_ATTRIBUTES_MASK) == aDIR) {
			return NT_STATUS_OBJECT_NAME_INVALID;
		}

		if (strequal(mask,"????????.???")) {
			pstrcpy(mask,"*");
		}

		status = check_name(conn, directory);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		dir_hnd = OpenDir(conn, directory, mask, dirtype);
		if (dir_hnd == NULL) {
			return map_nt_error_from_unix(errno);
		}
		
		/* XXXX the CIFS spec says that if bit0 of the flags2 field is set then
		   the pattern matches against the long name, otherwise the short name 
		   We don't implement this yet XXXX
		*/
		
		status = NT_STATUS_NO_SUCH_FILE;

		while ((dname = ReadDirName(dir_hnd, &offset))) {
			SMB_STRUCT_STAT st;
			pstring fname;
			pstrcpy(fname,dname);

			if (!is_visible_file(conn, directory, dname, &st, True)) {
				continue;
			}

			/* Quick check for "." and ".." */
			if (fname[0] == '.') {
				if (!fname[1] || (fname[1] == '.' && !fname[2])) {
					continue;
				}
			}

			if(!mask_match(fname, mask, conn->case_sensitive)) {
				continue;
			}
				
			slprintf(fname,sizeof(fname)-1, "%s/%s",directory,dname);

			status = check_name(conn, fname);
			if (!NT_STATUS_IS_OK(status)) {
				CloseDir(dir_hnd);
				return status;
			}

			status = can_delete(conn, fname, dirtype, can_defer);
			if (!NT_STATUS_IS_OK(status)) {
				continue;
			}
			if (SMB_VFS_UNLINK(conn,fname) == 0) {
				count++;
				DEBUG(3,("unlink_internals: successful unlink "
					 "[%s]\n",fname));
				notify_fname(conn, NOTIFY_ACTION_REMOVED,
					     FILE_NOTIFY_CHANGE_FILE_NAME,
					     fname);
			}
				
		}
		CloseDir(dir_hnd);
	}
	
	if (count == 0 && NT_STATUS_IS_OK(status)) {
		status = map_nt_error_from_unix(errno);
	}

	return status;
}

/****************************************************************************
 Reply to a unlink
****************************************************************************/

int reply_unlink(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, 
		 int dum_buffsize)
{
	int outsize = 0;
	pstring name;
	uint32 dirtype;
	NTSTATUS status;
	BOOL path_contains_wcard = False;

	START_PROFILE(SMBunlink);

	dirtype = SVAL(inbuf,smb_vwv0);
	
	srvstr_get_path_wcard(inbuf, name, smb_buf(inbuf) + 1, sizeof(name), 0, STR_TERMINATE, &status, &path_contains_wcard);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBunlink);
		return ERROR_NT(status);
	}

	status = resolve_dfspath_wcard(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, name, &path_contains_wcard);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBunlink);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}
	
	DEBUG(3,("reply_unlink : %s\n",name));
	
	status = unlink_internals(conn, dirtype, name, path_contains_wcard,
				  True);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBunlink);
		if (open_was_deferred(SVAL(inbuf,smb_mid))) {
			/* We have re-scheduled this call. */
			return -1;
		}
		return ERROR_NT(status);
	}

	outsize = set_message(outbuf,0,0,False);
  
	END_PROFILE(SMBunlink);
	return outsize;
}

/****************************************************************************
 Fail for readbraw.
****************************************************************************/

static void fail_readraw(void)
{
	pstring errstr;
	slprintf(errstr, sizeof(errstr)-1, "FAIL ! reply_readbraw: socket write fail (%s)",
		strerror(errno) );
	exit_server_cleanly(errstr);
}

#if defined(WITH_SENDFILE)
/****************************************************************************
 Fake (read/write) sendfile. Returns -1 on read or write fail.
****************************************************************************/

static ssize_t fake_sendfile(files_struct *fsp, SMB_OFF_T startpos, size_t nread, char *buf, int bufsize)
{
	ssize_t ret=0;

	/* Paranioa check... */
	if (nread > bufsize) {
		fail_readraw();
	}

	if (nread > 0) {
		ret = read_file(fsp,buf,startpos,nread);
		if (ret == -1) {
			return -1;
		}
	}

	/* If we had a short read, fill with zeros. */
	if (ret < nread) {
		memset(buf, '\0', nread - ret);
	}

	if (write_data(smbd_server_fd(),buf,nread) != nread) {
		return -1;
	}	

	return (ssize_t)nread;
}
#endif

/****************************************************************************
 Use sendfile in readbraw.
****************************************************************************/

void send_file_readbraw(connection_struct *conn, files_struct *fsp, SMB_OFF_T startpos, size_t nread,
		ssize_t mincount, char *outbuf, int out_buffsize)
{
	ssize_t ret=0;

#if defined(WITH_SENDFILE)
	/*
	 * We can only use sendfile on a non-chained packet 
	 * but we can use on a non-oplocked file. tridge proved this
	 * on a train in Germany :-). JRA.
	 * reply_readbraw has already checked the length.
	 */

	if ( (chain_size == 0) && (nread > 0) &&
	    (fsp->wcp == NULL) && lp_use_sendfile(SNUM(conn)) ) {
		DATA_BLOB header;

		_smb_setlen(outbuf,nread);
		header.data = (uint8 *)outbuf;
		header.length = 4;
		header.free = NULL;

		if ( SMB_VFS_SENDFILE( smbd_server_fd(), fsp, fsp->fh->fd, &header, startpos, nread) == -1) {
			/* Returning ENOSYS means no data at all was sent.
			   Do this as a normal read. */
			if (errno == ENOSYS) {
				goto normal_readbraw;
			}

			/*
			 * Special hack for broken Linux with no working sendfile. If we
			 * return EINTR we sent the header but not the rest of the data.
			 * Fake this up by doing read/write calls.
			 */
			if (errno == EINTR) {
				/* Ensure we don't do this again. */
				set_use_sendfile(SNUM(conn), False);
				DEBUG(0,("send_file_readbraw: sendfile not available. Faking..\n"));

				if (fake_sendfile(fsp, startpos, nread, outbuf + 4, out_buffsize - 4) == -1) {
					DEBUG(0,("send_file_readbraw: fake_sendfile failed for file %s (%s).\n",
						fsp->fsp_name, strerror(errno) ));
					exit_server_cleanly("send_file_readbraw fake_sendfile failed");
				}
				return;
			}

			DEBUG(0,("send_file_readbraw: sendfile failed for file %s (%s). Terminating\n",
				fsp->fsp_name, strerror(errno) ));
			exit_server_cleanly("send_file_readbraw sendfile failed");
		}

		return;
	}

  normal_readbraw:

#endif

	if (nread > 0) {
		ret = read_file(fsp,outbuf+4,startpos,nread);
#if 0 /* mincount appears to be ignored in a W2K server. JRA. */
		if (ret < mincount)
			ret = 0;
#else
		if (ret < nread)
			ret = 0;
#endif
	}

	_smb_setlen(outbuf,ret);
	if (write_data(smbd_server_fd(),outbuf,4+ret) != 4+ret)
		fail_readraw();
}

/****************************************************************************
 Reply to a readbraw (core+ protocol).
****************************************************************************/

int reply_readbraw(connection_struct *conn, char *inbuf, char *outbuf, int dum_size, int out_buffsize)
{
	ssize_t maxcount,mincount;
	size_t nread = 0;
	SMB_OFF_T startpos;
	char *header = outbuf;
	files_struct *fsp;
	START_PROFILE(SMBreadbraw);

	if (srv_is_signing_active()) {
		exit_server_cleanly("reply_readbraw: SMB signing is active - raw reads/writes are disallowed.");
	}

	/*
	 * Special check if an oplock break has been issued
	 * and the readraw request croses on the wire, we must
	 * return a zero length response here.
	 */

	fsp = file_fsp(inbuf,smb_vwv0);

	if (!FNUM_OK(fsp,conn) || !fsp->can_read) {
		/*
		 * fsp could be NULL here so use the value from the packet. JRA.
		 */
		DEBUG(3,("fnum %d not open in readbraw - cache prime?\n",(int)SVAL(inbuf,smb_vwv0)));
		_smb_setlen(header,0);
		if (write_data(smbd_server_fd(),header,4) != 4)
			fail_readraw();
		END_PROFILE(SMBreadbraw);
		return(-1);
	}

	CHECK_FSP(fsp,conn);

	flush_write_cache(fsp, READRAW_FLUSH);

	startpos = IVAL_TO_SMB_OFF_T(inbuf,smb_vwv1);
	if(CVAL(inbuf,smb_wct) == 10) {
		/*
		 * This is a large offset (64 bit) read.
		 */
#ifdef LARGE_SMB_OFF_T

		startpos |= (((SMB_OFF_T)IVAL(inbuf,smb_vwv8)) << 32);

#else /* !LARGE_SMB_OFF_T */

		/*
		 * Ensure we haven't been sent a >32 bit offset.
		 */

		if(IVAL(inbuf,smb_vwv8) != 0) {
			DEBUG(0,("readbraw - large offset (%x << 32) used and we don't support \
64 bit offsets.\n", (unsigned int)IVAL(inbuf,smb_vwv8) ));
			_smb_setlen(header,0);
			if (write_data(smbd_server_fd(),header,4) != 4)
				fail_readraw();
			END_PROFILE(SMBreadbraw);
			return(-1);
		}

#endif /* LARGE_SMB_OFF_T */

		if(startpos < 0) {
			DEBUG(0,("readbraw - negative 64 bit readraw offset (%.0f) !\n", (double)startpos ));
			_smb_setlen(header,0);
			if (write_data(smbd_server_fd(),header,4) != 4)
				fail_readraw();
			END_PROFILE(SMBreadbraw);
			return(-1);
		}      
	}
	maxcount = (SVAL(inbuf,smb_vwv3) & 0xFFFF);
	mincount = (SVAL(inbuf,smb_vwv4) & 0xFFFF);

	/* ensure we don't overrun the packet size */
	maxcount = MIN(65535,maxcount);

	if (!is_locked(fsp,(uint32)SVAL(inbuf,smb_pid),(SMB_BIG_UINT)maxcount,(SMB_BIG_UINT)startpos, READ_LOCK)) {
		SMB_STRUCT_STAT st;
		SMB_OFF_T size = 0;
  
		if (SMB_VFS_FSTAT(fsp,fsp->fh->fd,&st) == 0) {
			size = st.st_size;
		}

		if (startpos >= size) {
			nread = 0;
		} else {
			nread = MIN(maxcount,(size - startpos));	  
		}
	}

#if 0 /* mincount appears to be ignored in a W2K server. JRA. */
	if (nread < mincount)
		nread = 0;
#endif
  
	DEBUG( 3, ( "readbraw fnum=%d start=%.0f max=%lu min=%lu nread=%lu\n", fsp->fnum, (double)startpos,
				(unsigned long)maxcount, (unsigned long)mincount, (unsigned long)nread ) );
  
	send_file_readbraw(conn, fsp, startpos, nread, mincount, outbuf, out_buffsize);

	DEBUG(5,("readbraw finished\n"));
	END_PROFILE(SMBreadbraw);
	return -1;
}

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_LOCKING

/****************************************************************************
 Reply to a lockread (core+ protocol).
****************************************************************************/

int reply_lockread(connection_struct *conn, char *inbuf,char *outbuf, int length, int dum_buffsiz)
{
	ssize_t nread = -1;
	char *data;
	int outsize = 0;
	SMB_OFF_T startpos;
	size_t numtoread;
	NTSTATUS status;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	struct byte_range_lock *br_lck = NULL;
	START_PROFILE(SMBlockread);

	CHECK_FSP(fsp,conn);
	if (!CHECK_READ(fsp,inbuf)) {
		return(ERROR_DOS(ERRDOS,ERRbadaccess));
	}

	release_level_2_oplocks_on_change(fsp);

	numtoread = SVAL(inbuf,smb_vwv1);
	startpos = IVAL_TO_SMB_OFF_T(inbuf,smb_vwv2);
  
	outsize = set_message(outbuf,5,3,True);
	numtoread = MIN(BUFFER_SIZE-outsize,numtoread);
	data = smb_buf(outbuf) + 3;
	
	/*
	 * NB. Discovered by Menny Hamburger at Mainsoft. This is a core+
	 * protocol request that predates the read/write lock concept. 
	 * Thus instead of asking for a read lock here we need to ask
	 * for a write lock. JRA.
	 * Note that the requested lock size is unaffected by max_recv.
	 */
	
	br_lck = do_lock(fsp,
			(uint32)SVAL(inbuf,smb_pid), 
			(SMB_BIG_UINT)numtoread,
			(SMB_BIG_UINT)startpos,
			WRITE_LOCK,
			WINDOWS_LOCK,
			False, /* Non-blocking lock. */
			&status,
			NULL);
	TALLOC_FREE(br_lck);

	if (NT_STATUS_V(status)) {
		END_PROFILE(SMBlockread);
		return ERROR_NT(status);
	}

	/*
	 * However the requested READ size IS affected by max_recv. Insanity.... JRA.
	 */

	if (numtoread > max_recv) {
		DEBUG(0,("reply_lockread: requested read size (%u) is greater than maximum allowed (%u). \
Returning short read of maximum allowed for compatibility with Windows 2000.\n",
			(unsigned int)numtoread, (unsigned int)max_recv ));
		numtoread = MIN(numtoread,max_recv);
	}
	nread = read_file(fsp,data,startpos,numtoread);

	if (nread < 0) {
		END_PROFILE(SMBlockread);
		return(UNIXERROR(ERRDOS,ERRnoaccess));
	}
	
	outsize += nread;
	SSVAL(outbuf,smb_vwv0,nread);
	SSVAL(outbuf,smb_vwv5,nread+3);
	SSVAL(smb_buf(outbuf),1,nread);
	
	DEBUG(3,("lockread fnum=%d num=%d nread=%d\n",
		 fsp->fnum, (int)numtoread, (int)nread));

	END_PROFILE(SMBlockread);
	return(outsize);
}

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_ALL

/****************************************************************************
 Reply to a read.
****************************************************************************/

int reply_read(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
	size_t numtoread;
	ssize_t nread = 0;
	char *data;
	SMB_OFF_T startpos;
	int outsize = 0;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	START_PROFILE(SMBread);

	CHECK_FSP(fsp,conn);
	if (!CHECK_READ(fsp,inbuf)) {
		return(ERROR_DOS(ERRDOS,ERRbadaccess));
	}

	numtoread = SVAL(inbuf,smb_vwv1);
	startpos = IVAL_TO_SMB_OFF_T(inbuf,smb_vwv2);

	outsize = set_message(outbuf,5,3,True);
	numtoread = MIN(BUFFER_SIZE-outsize,numtoread);
	/*
	 * The requested read size cannot be greater than max_recv. JRA.
	 */
	if (numtoread > max_recv) {
		DEBUG(0,("reply_read: requested read size (%u) is greater than maximum allowed (%u). \
Returning short read of maximum allowed for compatibility with Windows 2000.\n",
			(unsigned int)numtoread, (unsigned int)max_recv ));
		numtoread = MIN(numtoread,max_recv);
	}

	data = smb_buf(outbuf) + 3;
  
	if (is_locked(fsp,(uint32)SVAL(inbuf,smb_pid),(SMB_BIG_UINT)numtoread,(SMB_BIG_UINT)startpos, READ_LOCK)) {
		END_PROFILE(SMBread);
		return ERROR_DOS(ERRDOS,ERRlock);
	}

	if (numtoread > 0)
		nread = read_file(fsp,data,startpos,numtoread);

	if (nread < 0) {
		END_PROFILE(SMBread);
		return(UNIXERROR(ERRDOS,ERRnoaccess));
	}
  
	outsize += nread;
	SSVAL(outbuf,smb_vwv0,nread);
	SSVAL(outbuf,smb_vwv5,nread+3);
	SCVAL(smb_buf(outbuf),0,1);
	SSVAL(smb_buf(outbuf),1,nread);
  
	DEBUG( 3, ( "read fnum=%d num=%d nread=%d\n",
		fsp->fnum, (int)numtoread, (int)nread ) );

	END_PROFILE(SMBread);
	return(outsize);
}

/****************************************************************************
 Reply to a read and X - possibly using sendfile.
****************************************************************************/

int send_file_readX(connection_struct *conn, char *inbuf,char *outbuf,int length, int len_outbuf,
		files_struct *fsp, SMB_OFF_T startpos, size_t smb_maxcnt)
{
	int outsize = 0;
	ssize_t nread = -1;
	char *data = smb_buf(outbuf);

#if defined(WITH_SENDFILE)
	/*
	 * We can only use sendfile on a non-chained packet 
	 * but we can use on a non-oplocked file. tridge proved this
	 * on a train in Germany :-). JRA.
	 */

	if ((chain_size == 0) && (CVAL(inbuf,smb_vwv0) == 0xFF) &&
	    lp_use_sendfile(SNUM(conn)) && (fsp->wcp == NULL) ) {
		SMB_STRUCT_STAT sbuf;
		DATA_BLOB header;

		if(SMB_VFS_FSTAT(fsp,fsp->fh->fd, &sbuf) == -1)
			return(UNIXERROR(ERRDOS,ERRnoaccess));

		if (startpos > sbuf.st_size)
			goto normal_read;

		if (smb_maxcnt > (sbuf.st_size - startpos))
			smb_maxcnt = (sbuf.st_size - startpos);

		if (smb_maxcnt == 0)
			goto normal_read;

		/* 
		 * Set up the packet header before send. We
		 * assume here the sendfile will work (get the
		 * correct amount of data).
		 */

		SSVAL(outbuf,smb_vwv2,0xFFFF); /* Remaining - must be -1. */
		SSVAL(outbuf,smb_vwv5,smb_maxcnt);
		SSVAL(outbuf,smb_vwv6,smb_offset(data,outbuf));
		SSVAL(outbuf,smb_vwv7,((smb_maxcnt >> 16) & 1));
		SSVAL(smb_buf(outbuf),-2,smb_maxcnt);
		SCVAL(outbuf,smb_vwv0,0xFF);
		set_message(outbuf,12,smb_maxcnt,False);
		header.data = (uint8 *)outbuf;
		header.length = data - outbuf;
		header.free = NULL;

		if ((nread = SMB_VFS_SENDFILE( smbd_server_fd(), fsp, fsp->fh->fd, &header, startpos, smb_maxcnt)) == -1) {
			/* Returning ENOSYS means no data at all was sent. Do this as a normal read. */
			if (errno == ENOSYS) {
				goto normal_read;
			}

			/*
			 * Special hack for broken Linux with no working sendfile. If we
			 * return EINTR we sent the header but not the rest of the data.
			 * Fake this up by doing read/write calls.
			 */

			if (errno == EINTR) {
				/* Ensure we don't do this again. */
				set_use_sendfile(SNUM(conn), False);
				DEBUG(0,("send_file_readX: sendfile not available. Faking..\n"));

				if ((nread = fake_sendfile(fsp, startpos, smb_maxcnt, data,
							len_outbuf - (data-outbuf))) == -1) {
					DEBUG(0,("send_file_readX: fake_sendfile failed for file %s (%s).\n",
						fsp->fsp_name, strerror(errno) ));
					exit_server_cleanly("send_file_readX: fake_sendfile failed");
				}
				DEBUG( 3, ( "send_file_readX: fake_sendfile fnum=%d max=%d nread=%d\n",
					fsp->fnum, (int)smb_maxcnt, (int)nread ) );
				/* Returning -1 here means successful sendfile. */
				return -1;
			}

			DEBUG(0,("send_file_readX: sendfile failed for file %s (%s). Terminating\n",
				fsp->fsp_name, strerror(errno) ));
			exit_server_cleanly("send_file_readX sendfile failed");
		}

		DEBUG( 3, ( "send_file_readX: sendfile fnum=%d max=%d nread=%d\n",
			fsp->fnum, (int)smb_maxcnt, (int)nread ) );
		/* Returning -1 here means successful sendfile. */
		return -1;
	}

  normal_read:

#endif

	nread = read_file(fsp,data,startpos,smb_maxcnt);
  
	if (nread < 0) {
		return(UNIXERROR(ERRDOS,ERRnoaccess));
	}

	outsize = set_message(outbuf,12,nread,False);
	SSVAL(outbuf,smb_vwv2,0xFFFF); /* Remaining - must be -1. */
	SSVAL(outbuf,smb_vwv5,nread);
	SSVAL(outbuf,smb_vwv6,smb_offset(data,outbuf));
	SSVAL(outbuf,smb_vwv7,((nread >> 16) & 1));
	SSVAL(smb_buf(outbuf),-2,nread);
  
	DEBUG( 3, ( "send_file_readX fnum=%d max=%d nread=%d\n",
		fsp->fnum, (int)smb_maxcnt, (int)nread ) );

	/* Returning the number of bytes we want to send back - including header. */
	return outsize;
}

/****************************************************************************
 Reply to a read and X.
****************************************************************************/

int reply_read_and_X(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
	files_struct *fsp = file_fsp(inbuf,smb_vwv2);
	SMB_OFF_T startpos = IVAL_TO_SMB_OFF_T(inbuf,smb_vwv3);
	ssize_t nread = -1;
	size_t smb_maxcnt = SVAL(inbuf,smb_vwv5);
#if 0
	size_t smb_mincnt = SVAL(inbuf,smb_vwv6);
#endif

	START_PROFILE(SMBreadX);

	/* If it's an IPC, pass off the pipe handler. */
	if (IS_IPC(conn)) {
		END_PROFILE(SMBreadX);
		return reply_pipe_read_and_X(inbuf,outbuf,length,bufsize);
	}

	CHECK_FSP(fsp,conn);
	if (!CHECK_READ(fsp,inbuf)) {
		return(ERROR_DOS(ERRDOS,ERRbadaccess));
	}

	set_message(outbuf,12,0,True);

	if (global_client_caps & CAP_LARGE_READX) {
		if (SVAL(inbuf,smb_vwv7) == 1) {
			smb_maxcnt |= (1<<16);
		}
		if (smb_maxcnt > BUFFER_SIZE) {
			DEBUG(0,("reply_read_and_X - read too large (%u) for reply buffer %u\n",
				(unsigned int)smb_maxcnt, (unsigned int)BUFFER_SIZE));
			END_PROFILE(SMBreadX);
			return ERROR_NT(NT_STATUS_INVALID_PARAMETER);
		}
	}

	if(CVAL(inbuf,smb_wct) == 12) {
#ifdef LARGE_SMB_OFF_T
		/*
		 * This is a large offset (64 bit) read.
		 */
		startpos |= (((SMB_OFF_T)IVAL(inbuf,smb_vwv10)) << 32);

#else /* !LARGE_SMB_OFF_T */

		/*
		 * Ensure we haven't been sent a >32 bit offset.
		 */

		if(IVAL(inbuf,smb_vwv10) != 0) {
			DEBUG(0,("reply_read_and_X - large offset (%x << 32) used and we don't support \
64 bit offsets.\n", (unsigned int)IVAL(inbuf,smb_vwv10) ));
			END_PROFILE(SMBreadX);
			return ERROR_DOS(ERRDOS,ERRbadaccess);
		}

#endif /* LARGE_SMB_OFF_T */

	}

	if (is_locked(fsp,(uint32)SVAL(inbuf,smb_pid),(SMB_BIG_UINT)smb_maxcnt,(SMB_BIG_UINT)startpos, READ_LOCK)) {
		END_PROFILE(SMBreadX);
		return ERROR_DOS(ERRDOS,ERRlock);
	}

	if (schedule_aio_read_and_X(conn, inbuf, outbuf, length, bufsize, fsp, startpos, smb_maxcnt)) {
		END_PROFILE(SMBreadX);
		return -1;
	}

	nread = send_file_readX(conn, inbuf, outbuf, length, bufsize, fsp, startpos, smb_maxcnt);
	/* Only call chain_reply if not an error. */
	if (nread != -1 && SVAL(outbuf,smb_rcls) == 0) {
		nread = chain_reply(inbuf,outbuf,length,bufsize);
	}

	END_PROFILE(SMBreadX);
	return nread;
}

/****************************************************************************
 Reply to a writebraw (core+ or LANMAN1.0 protocol).
****************************************************************************/

int reply_writebraw(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
	ssize_t nwritten=0;
	ssize_t total_written=0;
	size_t numtowrite=0;
	size_t tcount;
	SMB_OFF_T startpos;
	char *data=NULL;
	BOOL write_through;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	int outsize = 0;
	NTSTATUS status;
	START_PROFILE(SMBwritebraw);

	if (srv_is_signing_active()) {
		exit_server_cleanly("reply_writebraw: SMB signing is active - raw reads/writes are disallowed.");
	}

	CHECK_FSP(fsp,conn);
	if (!CHECK_WRITE(fsp)) {
		return(ERROR_DOS(ERRDOS,ERRbadaccess));
	}
  
	tcount = IVAL(inbuf,smb_vwv1);
	startpos = IVAL_TO_SMB_OFF_T(inbuf,smb_vwv3);
	write_through = BITSETW(inbuf+smb_vwv7,0);

	/* We have to deal with slightly different formats depending
		on whether we are using the core+ or lanman1.0 protocol */

	if(Protocol <= PROTOCOL_COREPLUS) {
		numtowrite = SVAL(smb_buf(inbuf),-2);
		data = smb_buf(inbuf);
	} else {
		numtowrite = SVAL(inbuf,smb_vwv10);
		data = smb_base(inbuf) + SVAL(inbuf, smb_vwv11);
	}

	/* force the error type */
	SCVAL(inbuf,smb_com,SMBwritec);
	SCVAL(outbuf,smb_com,SMBwritec);

	if (is_locked(fsp,(uint32)SVAL(inbuf,smb_pid),(SMB_BIG_UINT)tcount,(SMB_BIG_UINT)startpos, WRITE_LOCK)) {
		END_PROFILE(SMBwritebraw);
		return(ERROR_DOS(ERRDOS,ERRlock));
	}

	if (numtowrite>0)
		nwritten = write_file(fsp,data,startpos,numtowrite);
  
	DEBUG(3,("writebraw1 fnum=%d start=%.0f num=%d wrote=%d sync=%d\n",
		fsp->fnum, (double)startpos, (int)numtowrite, (int)nwritten, (int)write_through));

	if (nwritten < (ssize_t)numtowrite)  {
		END_PROFILE(SMBwritebraw);
		return(UNIXERROR(ERRHRD,ERRdiskfull));
	}

	total_written = nwritten;

	/* Return a message to the redirector to tell it to send more bytes */
	SCVAL(outbuf,smb_com,SMBwritebraw);
	SSVALS(outbuf,smb_vwv0,-1);
	outsize = set_message(outbuf,Protocol>PROTOCOL_COREPLUS?1:0,0,True);
	show_msg(outbuf);
	if (!send_smb(smbd_server_fd(),outbuf))
		exit_server_cleanly("reply_writebraw: send_smb failed.");
  
	/* Now read the raw data into the buffer and write it */
	if (read_smb_length(smbd_server_fd(),inbuf,SMB_SECONDARY_WAIT) == -1) {
		exit_server_cleanly("secondary writebraw failed");
	}
  
	/* Even though this is not an smb message, smb_len returns the generic length of an smb message */
	numtowrite = smb_len(inbuf);

	/* Set up outbuf to return the correct return */
	outsize = set_message(outbuf,1,0,True);
	SCVAL(outbuf,smb_com,SMBwritec);

	if (numtowrite != 0) {

		if (numtowrite > BUFFER_SIZE) {
			DEBUG(0,("reply_writebraw: Oversize secondary write raw requested (%u). Terminating\n",
				(unsigned int)numtowrite ));
			exit_server_cleanly("secondary writebraw failed");
		}

		if (tcount > nwritten+numtowrite) {
			DEBUG(3,("Client overestimated the write %d %d %d\n",
				(int)tcount,(int)nwritten,(int)numtowrite));
		}

		if (read_data( smbd_server_fd(), inbuf+4, numtowrite) != numtowrite ) {
			DEBUG(0,("reply_writebraw: Oversize secondary write raw read failed (%s). Terminating\n",
				strerror(errno) ));
			exit_server_cleanly("secondary writebraw failed");
		}

		nwritten = write_file(fsp,inbuf+4,startpos+nwritten,numtowrite);
		if (nwritten == -1) {
			END_PROFILE(SMBwritebraw);
			return(UNIXERROR(ERRHRD,ERRdiskfull));
		}

		if (nwritten < (ssize_t)numtowrite) {
			SCVAL(outbuf,smb_rcls,ERRHRD);
			SSVAL(outbuf,smb_err,ERRdiskfull);      
		}

		if (nwritten > 0)
			total_written += nwritten;
 	}
 
	SSVAL(outbuf,smb_vwv0,total_written);

	status = sync_file(conn, fsp, write_through);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5,("reply_writebraw: sync_file for %s returned %s\n",
			fsp->fsp_name, nt_errstr(status) ));
		END_PROFILE(SMBwritebraw);
		return ERROR_NT(status);
	}

	DEBUG(3,("writebraw2 fnum=%d start=%.0f num=%d wrote=%d\n",
		fsp->fnum, (double)startpos, (int)numtowrite,(int)total_written));

	/* we won't return a status if write through is not selected - this follows what WfWg does */
	END_PROFILE(SMBwritebraw);
	if (!write_through && total_written==tcount) {

#if RABBIT_PELLET_FIX
		/*
		 * Fix for "rabbit pellet" mode, trigger an early TCP ack by
		 * sending a SMBkeepalive. Thanks to DaveCB at Sun for this. JRA.
		 */
		if (!send_keepalive(smbd_server_fd()))
			exit_server_cleanly("reply_writebraw: send of keepalive failed");
#endif
		return(-1);
	}

	return(outsize);
}

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_LOCKING

/****************************************************************************
 Reply to a writeunlock (core+).
****************************************************************************/

int reply_writeunlock(connection_struct *conn, char *inbuf,char *outbuf, 
		      int size, int dum_buffsize)
{
	ssize_t nwritten = -1;
	size_t numtowrite;
	SMB_OFF_T startpos;
	char *data;
	NTSTATUS status = NT_STATUS_OK;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	int outsize = 0;
	START_PROFILE(SMBwriteunlock);
	
	CHECK_FSP(fsp,conn);
	if (!CHECK_WRITE(fsp)) {
		return(ERROR_DOS(ERRDOS,ERRbadaccess));
	}

	numtowrite = SVAL(inbuf,smb_vwv1);
	startpos = IVAL_TO_SMB_OFF_T(inbuf,smb_vwv2);
	data = smb_buf(inbuf) + 3;
  
	if (numtowrite && is_locked(fsp,(uint32)SVAL(inbuf,smb_pid),(SMB_BIG_UINT)numtowrite,(SMB_BIG_UINT)startpos, WRITE_LOCK)) {
		END_PROFILE(SMBwriteunlock);
		return ERROR_DOS(ERRDOS,ERRlock);
	}

	/* The special X/Open SMB protocol handling of
	   zero length writes is *NOT* done for
	   this call */
	if(numtowrite == 0) {
		nwritten = 0;
	} else {
		nwritten = write_file(fsp,data,startpos,numtowrite);
	}
  
	status = sync_file(conn, fsp, False /* write through */);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBwriteunlock);
		DEBUG(5,("reply_writeunlock: sync_file for %s returned %s\n",
			fsp->fsp_name, nt_errstr(status) ));
		return ERROR_NT(status);
	}

	if(((nwritten == 0) && (numtowrite != 0))||(nwritten < 0)) {
		END_PROFILE(SMBwriteunlock);
		return(UNIXERROR(ERRHRD,ERRdiskfull));
	}

	if (numtowrite) {
		status = do_unlock(fsp,
				(uint32)SVAL(inbuf,smb_pid),
				(SMB_BIG_UINT)numtowrite, 
				(SMB_BIG_UINT)startpos,
				WINDOWS_LOCK);

		if (NT_STATUS_V(status)) {
			END_PROFILE(SMBwriteunlock);
			return ERROR_NT(status);
		}
	}
	
	outsize = set_message(outbuf,1,0,True);
	
	SSVAL(outbuf,smb_vwv0,nwritten);
	
	DEBUG(3,("writeunlock fnum=%d num=%d wrote=%d\n",
		 fsp->fnum, (int)numtowrite, (int)nwritten));
	
	END_PROFILE(SMBwriteunlock);
	return outsize;
}

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_ALL

/****************************************************************************
 Reply to a write.
****************************************************************************/

int reply_write(connection_struct *conn, char *inbuf,char *outbuf,int size,int dum_buffsize)
{
	size_t numtowrite;
	ssize_t nwritten = -1;
	SMB_OFF_T startpos;
	char *data;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	int outsize = 0;
	NTSTATUS status;
	START_PROFILE(SMBwrite);

	/* If it's an IPC, pass off the pipe handler. */
	if (IS_IPC(conn)) {
		END_PROFILE(SMBwrite);
		return reply_pipe_write(inbuf,outbuf,size,dum_buffsize);
	}

	CHECK_FSP(fsp,conn);
	if (!CHECK_WRITE(fsp)) {
		END_PROFILE(SMBwrite);
		return(ERROR_DOS(ERRDOS,ERRbadaccess));
	}

	numtowrite = SVAL(inbuf,smb_vwv1);
	startpos = IVAL_TO_SMB_OFF_T(inbuf,smb_vwv2);
	data = smb_buf(inbuf) + 3;
  
	if (is_locked(fsp,(uint32)SVAL(inbuf,smb_pid),(SMB_BIG_UINT)numtowrite,(SMB_BIG_UINT)startpos, WRITE_LOCK)) {
		END_PROFILE(SMBwrite);
		return ERROR_DOS(ERRDOS,ERRlock);
	}

	/*
	 * X/Open SMB protocol says that if smb_vwv1 is
	 * zero then the file size should be extended or
	 * truncated to the size given in smb_vwv[2-3].
	 */

	if(numtowrite == 0) {
		/*
		 * This is actually an allocate call, and set EOF. JRA.
		 */
		nwritten = vfs_allocate_file_space(fsp, (SMB_OFF_T)startpos);
		if (nwritten < 0) {
			END_PROFILE(SMBwrite);
			return ERROR_NT(NT_STATUS_DISK_FULL);
		}
		nwritten = vfs_set_filelen(fsp, (SMB_OFF_T)startpos);
		if (nwritten < 0) {
			END_PROFILE(SMBwrite);
			return ERROR_NT(NT_STATUS_DISK_FULL);
		}
	} else
		nwritten = write_file(fsp,data,startpos,numtowrite);
  
	status = sync_file(conn, fsp, False);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBwrite);
		DEBUG(5,("reply_write: sync_file for %s returned %s\n",
			fsp->fsp_name, nt_errstr(status) ));
		return ERROR_NT(status);
	}

	if(((nwritten == 0) && (numtowrite != 0))||(nwritten < 0)) {
		END_PROFILE(SMBwrite);
		return(UNIXERROR(ERRHRD,ERRdiskfull));
	}

	outsize = set_message(outbuf,1,0,True);
  
	SSVAL(outbuf,smb_vwv0,nwritten);

	if (nwritten < (ssize_t)numtowrite) {
		SCVAL(outbuf,smb_rcls,ERRHRD);
		SSVAL(outbuf,smb_err,ERRdiskfull);      
	}
  
	DEBUG(3,("write fnum=%d num=%d wrote=%d\n", fsp->fnum, (int)numtowrite, (int)nwritten));

	END_PROFILE(SMBwrite);
	return(outsize);
}

/****************************************************************************
 Reply to a write and X.
****************************************************************************/

int reply_write_and_X(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
	files_struct *fsp = file_fsp(inbuf,smb_vwv2);
	SMB_OFF_T startpos = IVAL_TO_SMB_OFF_T(inbuf,smb_vwv3);
	size_t numtowrite = SVAL(inbuf,smb_vwv10);
	BOOL write_through = BITSETW(inbuf+smb_vwv7,0);
	ssize_t nwritten = -1;
	unsigned int smb_doff = SVAL(inbuf,smb_vwv11);
	unsigned int smblen = smb_len(inbuf);
	char *data;
	BOOL large_writeX = ((CVAL(inbuf,smb_wct) == 14) && (smblen > 0xFFFF));
	NTSTATUS status;
	START_PROFILE(SMBwriteX);

	/* If it's an IPC, pass off the pipe handler. */
	if (IS_IPC(conn)) {
		END_PROFILE(SMBwriteX);
		return reply_pipe_write_and_X(inbuf,outbuf,length,bufsize);
	}

	CHECK_FSP(fsp,conn);
	if (!CHECK_WRITE(fsp)) {
		return(ERROR_DOS(ERRDOS,ERRbadaccess));
	}

	set_message(outbuf,6,0,True);
  
	/* Deal with possible LARGE_WRITEX */
	if (large_writeX) {
		numtowrite |= ((((size_t)SVAL(inbuf,smb_vwv9)) & 1 )<<16);
	}

	if(smb_doff > smblen || (smb_doff + numtowrite > smblen)) {
		END_PROFILE(SMBwriteX);
		return ERROR_DOS(ERRDOS,ERRbadmem);
	}

	data = smb_base(inbuf) + smb_doff;

	if(CVAL(inbuf,smb_wct) == 14) {
#ifdef LARGE_SMB_OFF_T
		/*
		 * This is a large offset (64 bit) write.
		 */
		startpos |= (((SMB_OFF_T)IVAL(inbuf,smb_vwv12)) << 32);

#else /* !LARGE_SMB_OFF_T */

		/*
		 * Ensure we haven't been sent a >32 bit offset.
		 */

		if(IVAL(inbuf,smb_vwv12) != 0) {
			DEBUG(0,("reply_write_and_X - large offset (%x << 32) used and we don't support \
64 bit offsets.\n", (unsigned int)IVAL(inbuf,smb_vwv12) ));
			END_PROFILE(SMBwriteX);
			return ERROR_DOS(ERRDOS,ERRbadaccess);
		}

#endif /* LARGE_SMB_OFF_T */
	}

	if (is_locked(fsp,(uint32)SVAL(inbuf,smb_pid),(SMB_BIG_UINT)numtowrite,(SMB_BIG_UINT)startpos, WRITE_LOCK)) {
		END_PROFILE(SMBwriteX);
		return ERROR_DOS(ERRDOS,ERRlock);
	}

	/* X/Open SMB protocol says that, unlike SMBwrite
	if the length is zero then NO truncation is
	done, just a write of zero. To truncate a file,
	use SMBwrite. */

	if(numtowrite == 0) {
		nwritten = 0;
	} else {

		if (schedule_aio_write_and_X(conn, inbuf, outbuf, length, bufsize,
					fsp,data,startpos,numtowrite)) {
			END_PROFILE(SMBwriteX);
			return -1;
		}

		nwritten = write_file(fsp,data,startpos,numtowrite);
	}
  
	if(((nwritten == 0) && (numtowrite != 0))||(nwritten < 0)) {
		END_PROFILE(SMBwriteX);
		return(UNIXERROR(ERRHRD,ERRdiskfull));
	}

	SSVAL(outbuf,smb_vwv2,nwritten);
	if (large_writeX)
		SSVAL(outbuf,smb_vwv4,(nwritten>>16)&1);

	if (nwritten < (ssize_t)numtowrite) {
		SCVAL(outbuf,smb_rcls,ERRHRD);
		SSVAL(outbuf,smb_err,ERRdiskfull);      
	}

	DEBUG(3,("writeX fnum=%d num=%d wrote=%d\n",
		fsp->fnum, (int)numtowrite, (int)nwritten));

	status = sync_file(conn, fsp, write_through);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBwriteX);
		DEBUG(5,("reply_write_and_X: sync_file for %s returned %s\n",
			fsp->fsp_name, nt_errstr(status) ));
		return ERROR_NT(status);
	}

	END_PROFILE(SMBwriteX);
	return chain_reply(inbuf,outbuf,length,bufsize);
}

/****************************************************************************
 Reply to a lseek.
****************************************************************************/

int reply_lseek(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
	SMB_OFF_T startpos;
	SMB_OFF_T res= -1;
	int mode,umode;
	int outsize = 0;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	START_PROFILE(SMBlseek);

	CHECK_FSP(fsp,conn);

	flush_write_cache(fsp, SEEK_FLUSH);

	mode = SVAL(inbuf,smb_vwv1) & 3;
	/* NB. This doesn't use IVAL_TO_SMB_OFF_T as startpos can be signed in this case. */
	startpos = (SMB_OFF_T)IVALS(inbuf,smb_vwv2);

	switch (mode) {
		case 0:
			umode = SEEK_SET;
			res = startpos;
			break;
		case 1:
			umode = SEEK_CUR;
			res = fsp->fh->pos + startpos;
			break;
		case 2:
			umode = SEEK_END;
			break;
		default:
			umode = SEEK_SET;
			res = startpos;
			break;
	}

	if (umode == SEEK_END) {
		if((res = SMB_VFS_LSEEK(fsp,fsp->fh->fd,startpos,umode)) == -1) {
			if(errno == EINVAL) {
				SMB_OFF_T current_pos = startpos;
				SMB_STRUCT_STAT sbuf;

				if(SMB_VFS_FSTAT(fsp,fsp->fh->fd, &sbuf) == -1) {
					END_PROFILE(SMBlseek);
					return(UNIXERROR(ERRDOS,ERRnoaccess));
				}

				current_pos += sbuf.st_size;
				if(current_pos < 0)
					res = SMB_VFS_LSEEK(fsp,fsp->fh->fd,0,SEEK_SET);
			}
		}

		if(res == -1) {
			END_PROFILE(SMBlseek);
			return(UNIXERROR(ERRDOS,ERRnoaccess));
		}
	}

	fsp->fh->pos = res;
  
	outsize = set_message(outbuf,2,0,True);
	SIVAL(outbuf,smb_vwv0,res);
  
	DEBUG(3,("lseek fnum=%d ofs=%.0f newpos = %.0f mode=%d\n",
		fsp->fnum, (double)startpos, (double)res, mode));

	END_PROFILE(SMBlseek);
	return(outsize);
}

/****************************************************************************
 Reply to a flush.
****************************************************************************/

int reply_flush(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
	int outsize = set_message(outbuf,0,0,False);
	uint16 fnum = SVAL(inbuf,smb_vwv0);
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	START_PROFILE(SMBflush);

	if (fnum != 0xFFFF)
		CHECK_FSP(fsp,conn);
	
	if (!fsp) {
		file_sync_all(conn);
	} else {
		NTSTATUS status = sync_file(conn, fsp, True);
		if (!NT_STATUS_IS_OK(status)) {
			END_PROFILE(SMBflush);
			DEBUG(5,("reply_flush: sync_file for %s returned %s\n",
				fsp->fsp_name, nt_errstr(status) ));
			return ERROR_NT(status);
		}
	}
	
	DEBUG(3,("flush\n"));
	END_PROFILE(SMBflush);
	return(outsize);
}

/****************************************************************************
 Reply to a exit.
 conn POINTER CAN BE NULL HERE !
****************************************************************************/

int reply_exit(connection_struct *conn, 
	       char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int outsize;
	START_PROFILE(SMBexit);

	file_close_pid(SVAL(inbuf,smb_pid),SVAL(inbuf,smb_uid));

	outsize = set_message(outbuf,0,0,False);

	DEBUG(3,("exit\n"));

	END_PROFILE(SMBexit);
	return(outsize);
}

/****************************************************************************
 Reply to a close - has to deal with closing a directory opened by NT SMB's.
****************************************************************************/

int reply_close(connection_struct *conn, char *inbuf,char *outbuf, int size,
                int dum_buffsize)
{
	NTSTATUS status = NT_STATUS_OK;
	int outsize = 0;
	files_struct *fsp = NULL;
	START_PROFILE(SMBclose);

	outsize = set_message(outbuf,0,0,False);

	/* If it's an IPC, pass off to the pipe handler. */
	if (IS_IPC(conn)) {
		END_PROFILE(SMBclose);
		return reply_pipe_close(conn, inbuf,outbuf);
	}

	fsp = file_fsp(inbuf,smb_vwv0);

	/*
	 * We can only use CHECK_FSP if we know it's not a directory.
	 */

	if(!fsp || (fsp->conn != conn) || (fsp->vuid != current_user.vuid)) {
		END_PROFILE(SMBclose);
		return ERROR_DOS(ERRDOS,ERRbadfid);
	}

	if(fsp->is_directory) {
		/*
		 * Special case - close NT SMB directory handle.
		 */
		DEBUG(3,("close directory fnum=%d\n", fsp->fnum));
		status = close_file(fsp,NORMAL_CLOSE);
	} else {
		/*
		 * Close ordinary file.
		 */

		DEBUG(3,("close fd=%d fnum=%d (numopen=%d)\n",
			 fsp->fh->fd, fsp->fnum,
			 conn->num_files_open));
 
		/*
		 * Take care of any time sent in the close.
		 */

		fsp_set_pending_modtime(fsp,
				convert_time_t_to_timespec(srv_make_unix_date3(inbuf+smb_vwv1)));

		/*
		 * close_file() returns the unix errno if an error
		 * was detected on close - normally this is due to
		 * a disk full error. If not then it was probably an I/O error.
		 */
 
		status = close_file(fsp,NORMAL_CLOSE);
	}  

	if(!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBclose);
		return ERROR_NT(status);
	}

	END_PROFILE(SMBclose);
	return(outsize);
}

/****************************************************************************
 Reply to a writeclose (Core+ protocol).
****************************************************************************/

int reply_writeclose(connection_struct *conn,
		     char *inbuf,char *outbuf, int size, int dum_buffsize)
{
	size_t numtowrite;
	ssize_t nwritten = -1;
	int outsize = 0;
	NTSTATUS close_status = NT_STATUS_OK;
	SMB_OFF_T startpos;
	char *data;
	struct timespec mtime;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	START_PROFILE(SMBwriteclose);

	CHECK_FSP(fsp,conn);
	if (!CHECK_WRITE(fsp)) {
		return(ERROR_DOS(ERRDOS,ERRbadaccess));
	}

	numtowrite = SVAL(inbuf,smb_vwv1);
	startpos = IVAL_TO_SMB_OFF_T(inbuf,smb_vwv2);
	mtime = convert_time_t_to_timespec(srv_make_unix_date3(inbuf+smb_vwv4));
	data = smb_buf(inbuf) + 1;
  
	if (numtowrite && is_locked(fsp,(uint32)SVAL(inbuf,smb_pid),(SMB_BIG_UINT)numtowrite,(SMB_BIG_UINT)startpos, WRITE_LOCK)) {
		END_PROFILE(SMBwriteclose);
		return ERROR_DOS(ERRDOS,ERRlock);
	}
  
	nwritten = write_file(fsp,data,startpos,numtowrite);

	set_filetime(conn, fsp->fsp_name, mtime);
  
	/*
	 * More insanity. W2K only closes the file if writelen > 0.
	 * JRA.
	 */

	if (numtowrite) {
		DEBUG(3,("reply_writeclose: zero length write doesn't close file %s\n",
			fsp->fsp_name ));
		close_status = close_file(fsp,NORMAL_CLOSE);
	}

	DEBUG(3,("writeclose fnum=%d num=%d wrote=%d (numopen=%d)\n",
		 fsp->fnum, (int)numtowrite, (int)nwritten,
		 conn->num_files_open));
  
	if(((nwritten == 0) && (numtowrite != 0))||(nwritten < 0)) {
		END_PROFILE(SMBwriteclose);
		return(UNIXERROR(ERRHRD,ERRdiskfull));
	}
 
	if(!NT_STATUS_IS_OK(close_status)) {
		END_PROFILE(SMBwriteclose);
		return ERROR_NT(close_status);
	}
 
	outsize = set_message(outbuf,1,0,True);
  
	SSVAL(outbuf,smb_vwv0,nwritten);
	END_PROFILE(SMBwriteclose);
	return(outsize);
}

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_LOCKING

/****************************************************************************
 Reply to a lock.
****************************************************************************/

int reply_lock(connection_struct *conn,
	       char *inbuf,char *outbuf, int length, int dum_buffsize)
{
	int outsize = set_message(outbuf,0,0,False);
	SMB_BIG_UINT count,offset;
	NTSTATUS status;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	struct byte_range_lock *br_lck = NULL;

	START_PROFILE(SMBlock);

	CHECK_FSP(fsp,conn);

	release_level_2_oplocks_on_change(fsp);

	count = (SMB_BIG_UINT)IVAL(inbuf,smb_vwv1);
	offset = (SMB_BIG_UINT)IVAL(inbuf,smb_vwv3);

	DEBUG(3,("lock fd=%d fnum=%d offset=%.0f count=%.0f\n",
		 fsp->fh->fd, fsp->fnum, (double)offset, (double)count));

	br_lck = do_lock(fsp,
			(uint32)SVAL(inbuf,smb_pid),
			count,
			offset,
			WRITE_LOCK,
			WINDOWS_LOCK,
			False, /* Non-blocking lock. */
			&status,
			NULL);

	TALLOC_FREE(br_lck);

	if (NT_STATUS_V(status)) {
		END_PROFILE(SMBlock);
		return ERROR_NT(status);
	}

	END_PROFILE(SMBlock);
	return(outsize);
}

/****************************************************************************
 Reply to a unlock.
****************************************************************************/

int reply_unlock(connection_struct *conn, char *inbuf,char *outbuf, int size, 
		 int dum_buffsize)
{
	int outsize = set_message(outbuf,0,0,False);
	SMB_BIG_UINT count,offset;
	NTSTATUS status;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	START_PROFILE(SMBunlock);

	CHECK_FSP(fsp,conn);
	
	count = (SMB_BIG_UINT)IVAL(inbuf,smb_vwv1);
	offset = (SMB_BIG_UINT)IVAL(inbuf,smb_vwv3);
	
	status = do_unlock(fsp,
			(uint32)SVAL(inbuf,smb_pid),
			count,
			offset,
			WINDOWS_LOCK);

	if (NT_STATUS_V(status)) {
		END_PROFILE(SMBunlock);
		return ERROR_NT(status);
	}

	DEBUG( 3, ( "unlock fd=%d fnum=%d offset=%.0f count=%.0f\n",
		    fsp->fh->fd, fsp->fnum, (double)offset, (double)count ) );
	
	END_PROFILE(SMBunlock);
	return(outsize);
}

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_ALL

/****************************************************************************
 Reply to a tdis.
 conn POINTER CAN BE NULL HERE !
****************************************************************************/

int reply_tdis(connection_struct *conn, 
	       char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int outsize = set_message(outbuf,0,0,False);
	uint16 vuid;
	START_PROFILE(SMBtdis);

	vuid = SVAL(inbuf,smb_uid);

	if (!conn) {
		DEBUG(4,("Invalid connection in tdis\n"));
		END_PROFILE(SMBtdis);
		return ERROR_DOS(ERRSRV,ERRinvnid);
	}

	conn->used = False;

	close_cnum(conn,vuid);
  
	END_PROFILE(SMBtdis);
	return outsize;
}

/****************************************************************************
 Reply to a echo.
 conn POINTER CAN BE NULL HERE !
****************************************************************************/

int reply_echo(connection_struct *conn,
	       char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int smb_reverb = SVAL(inbuf,smb_vwv0);
	int seq_num;
	unsigned int data_len = smb_buflen(inbuf);
	int outsize = set_message(outbuf,1,data_len,True);
	START_PROFILE(SMBecho);

	if (data_len > BUFFER_SIZE) {
		DEBUG(0,("reply_echo: data_len too large.\n"));
		END_PROFILE(SMBecho);
		return -1;
	}

	/* copy any incoming data back out */
	if (data_len > 0)
		memcpy(smb_buf(outbuf),smb_buf(inbuf),data_len);

	if (smb_reverb > 100) {
		DEBUG(0,("large reverb (%d)?? Setting to 100\n",smb_reverb));
		smb_reverb = 100;
	}

	for (seq_num =1 ; seq_num <= smb_reverb ; seq_num++) {
		SSVAL(outbuf,smb_vwv0,seq_num);

		smb_setlen(outbuf,outsize - 4);

		show_msg(outbuf);
		if (!send_smb(smbd_server_fd(),outbuf))
			exit_server_cleanly("reply_echo: send_smb failed.");
	}

	DEBUG(3,("echo %d times\n", smb_reverb));

	smb_echo_count++;

	END_PROFILE(SMBecho);
	return -1;
}

/****************************************************************************
 Reply to a printopen.
****************************************************************************/

int reply_printopen(connection_struct *conn, 
		    char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int outsize = 0;
	files_struct *fsp;
	NTSTATUS status;
	
	START_PROFILE(SMBsplopen);
#ifdef AVM_NO_PRINTING
	END_PROFILE(SMBsplopen);
	return ERROR_DOS(ERRDOS,ERRnoaccess);
#else
	if (!CAN_PRINT(conn)) {
		END_PROFILE(SMBsplopen);
		return ERROR_DOS(ERRDOS,ERRnoaccess);
	}

	/* Open for exclusive use, write only. */
	status = print_fsp_open(conn, NULL, &fsp);

	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBsplopen);
		return(ERROR_NT(status));
	}

	outsize = set_message(outbuf,1,0,True);
	SSVAL(outbuf,smb_vwv0,fsp->fnum);
  
	DEBUG(3,("openprint fd=%d fnum=%d\n",
		 fsp->fh->fd, fsp->fnum));

	END_PROFILE(SMBsplopen);
	return(outsize);
#endif
}

/****************************************************************************
 Reply to a printclose.
****************************************************************************/

int reply_printclose(connection_struct *conn,
		     char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int outsize = set_message(outbuf,0,0,False);
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	NTSTATUS status;
	START_PROFILE(SMBsplclose);

	CHECK_FSP(fsp,conn);

#ifdef AVM_NO_PRINTING
	END_PROFILE(SMBsplretq);
	return ERROR_DOS(ERRDOS,ERRnoaccess);
#else
	if (!CAN_PRINT(conn)) {
		END_PROFILE(SMBsplclose);
		return ERROR_NT(NT_STATUS_DOS(ERRSRV, ERRerror));
	}
  
	DEBUG(3,("printclose fd=%d fnum=%d\n",
		 fsp->fh->fd,fsp->fnum));
  
	status = close_file(fsp,NORMAL_CLOSE);

	if(!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBsplclose);
		return ERROR_NT(status);
	}

	END_PROFILE(SMBsplclose);
	return(outsize);
#endif
}

/****************************************************************************
 Reply to a printqueue.
****************************************************************************/

int reply_printqueue(connection_struct *conn,
		     char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
#ifdef AVM_NO_PRINTING
	END_PROFILE(SMBsplretq);
	return ERROR_DOS(ERRDOS,ERRnoaccess);
#else
	int outsize = set_message(outbuf,2,3,True);
	int max_count = SVAL(inbuf,smb_vwv0);
	int start_index = SVAL(inbuf,smb_vwv1);
	START_PROFILE(SMBsplretq);

	/* we used to allow the client to get the cnum wrong, but that
	   is really quite gross and only worked when there was only
	   one printer - I think we should now only accept it if they
	   get it right (tridge) */
	if (!CAN_PRINT(conn)) {
		END_PROFILE(SMBsplretq);
		return ERROR_DOS(ERRDOS,ERRnoaccess);
	}

	SSVAL(outbuf,smb_vwv0,0);
	SSVAL(outbuf,smb_vwv1,0);
	SCVAL(smb_buf(outbuf),0,1);
	SSVAL(smb_buf(outbuf),1,0);
  
	DEBUG(3,("printqueue start_index=%d max_count=%d\n",
		 start_index, max_count));

	{
		print_queue_struct *queue = NULL;
		print_status_struct status;
		char *p = smb_buf(outbuf) + 3;
		int count = print_queue_status(SNUM(conn), &queue, &status);
		int num_to_get = ABS(max_count);
		int first = (max_count>0?start_index:start_index+max_count+1);
		int i;

		if (first >= count)
			num_to_get = 0;
		else
			num_to_get = MIN(num_to_get,count-first);
    

		for (i=first;i<first+num_to_get;i++) {
			srv_put_dos_date2(p,0,queue[i].time);
			SCVAL(p,4,(queue[i].status==LPQ_PRINTING?2:3));
			SSVAL(p,5, queue[i].job);
			SIVAL(p,7,queue[i].size);
			SCVAL(p,11,0);
			srvstr_push(outbuf, p+12, queue[i].fs_user, 16, STR_ASCII);
			p += 28;
		}

		if (count > 0) {
			outsize = set_message(outbuf,2,28*count+3,False); 
			SSVAL(outbuf,smb_vwv0,count);
			SSVAL(outbuf,smb_vwv1,(max_count>0?first+count:first-1));
			SCVAL(smb_buf(outbuf),0,1);
			SSVAL(smb_buf(outbuf),1,28*count);
		}

		SAFE_FREE(queue);
	  
		DEBUG(3,("%d entries returned in queue\n",count));
	}
  
	END_PROFILE(SMBsplretq);
	return(outsize);
#endif
}

/****************************************************************************
 Reply to a printwrite.
****************************************************************************/

int reply_printwrite(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int numtowrite;
	int outsize = set_message(outbuf,0,0,False);
	char *data;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);

	START_PROFILE(SMBsplwr);
  
	if (!CAN_PRINT(conn)) {
		END_PROFILE(SMBsplwr);
		return ERROR_DOS(ERRDOS,ERRnoaccess);
	}

	CHECK_FSP(fsp,conn);
	if (!CHECK_WRITE(fsp)) {
		return(ERROR_DOS(ERRDOS,ERRbadaccess));
	}

	numtowrite = SVAL(smb_buf(inbuf),1);
	data = smb_buf(inbuf) + 3;
  
	if (write_file(fsp,data,-1,numtowrite) != numtowrite) {
		END_PROFILE(SMBsplwr);
		return(UNIXERROR(ERRHRD,ERRdiskfull));
	}

	DEBUG( 3, ( "printwrite fnum=%d num=%d\n", fsp->fnum, numtowrite ) );
  
	END_PROFILE(SMBsplwr);
	return(outsize);
}

/****************************************************************************
 Reply to a mkdir.
****************************************************************************/

int reply_mkdir(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	pstring directory;
	int outsize;
	NTSTATUS status;
	SMB_STRUCT_STAT sbuf;

	START_PROFILE(SMBmkdir);
 
	srvstr_get_path(inbuf, directory, smb_buf(inbuf) + 1, sizeof(directory), 0, STR_TERMINATE, &status);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBmkdir);
		return ERROR_NT(status);
	}

	status = resolve_dfspath(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, directory);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBmkdir);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}

	status = unix_convert(conn, directory, False, NULL, &sbuf);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBmkdir);
		return ERROR_NT(status);
	}

	status = check_name(conn, directory);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBmkdir);
		return ERROR_NT(status);
	}
  
	status = create_directory(conn, directory);

	DEBUG(5, ("create_directory returned %s\n", nt_errstr(status)));

	if (!NT_STATUS_IS_OK(status)) {

		if (!use_nt_status()
		    && NT_STATUS_EQUAL(status,
				       NT_STATUS_OBJECT_NAME_COLLISION)) {
			/*
			 * Yes, in the DOS error code case we get a
			 * ERRDOS:ERRnoaccess here. See BASE-SAMBA3ERROR
			 * samba4 torture test.
			 */
			status = NT_STATUS_DOS(ERRDOS, ERRnoaccess);
		}

		END_PROFILE(SMBmkdir);
		return ERROR_NT(status);
	}

	outsize = set_message(outbuf,0,0,False);

	DEBUG( 3, ( "mkdir %s ret=%d\n", directory, outsize ) );

	END_PROFILE(SMBmkdir);
	return(outsize);
}

/****************************************************************************
 Static function used by reply_rmdir to delete an entire directory
 tree recursively. Return True on ok, False on fail.
****************************************************************************/

static BOOL recursive_rmdir(connection_struct *conn, char *directory)
{
	const char *dname = NULL;
	BOOL ret = True;
	long offset = 0;
	struct smb_Dir *dir_hnd = OpenDir(conn, directory, NULL, 0);

	if(dir_hnd == NULL)
		return False;

	while((dname = ReadDirName(dir_hnd, &offset))) {
		pstring fullname;
		SMB_STRUCT_STAT st;

		if((strcmp(dname, ".") == 0) || (strcmp(dname, "..")==0))
			continue;

		if (!is_visible_file(conn, directory, dname, &st, False))
			continue;

		/* Construct the full name. */
		if(strlen(directory) + strlen(dname) + 1 >= sizeof(fullname)) {
			errno = ENOMEM;
			ret = False;
			break;
		}

		pstrcpy(fullname, directory);
		pstrcat(fullname, "/");
		pstrcat(fullname, dname);

		if(SMB_VFS_LSTAT(conn,fullname, &st) != 0) {
			ret = False;
			break;
		}

		if(st.st_mode & S_IFDIR) {
			if(!recursive_rmdir(conn, fullname)) {
				ret = False;
				break;
			}
			if(SMB_VFS_RMDIR(conn,fullname) != 0) {
				ret = False;
				break;
			}
		} else if(SMB_VFS_UNLINK(conn,fullname) != 0) {
			ret = False;
			break;
		}
	}
	CloseDir(dir_hnd);
	return ret;
}

/****************************************************************************
 The internals of the rmdir code - called elsewhere.
****************************************************************************/

NTSTATUS rmdir_internals(connection_struct *conn, const char *directory)
{
	int ret;
	SMB_STRUCT_STAT st;

	/* Might be a symlink. */
	if(SMB_VFS_LSTAT(conn, directory, &st) != 0) {
		return map_nt_error_from_unix(errno);
	}

	if (S_ISLNK(st.st_mode)) {
		/* Is what it points to a directory ? */
		if(SMB_VFS_STAT(conn, directory, &st) != 0) {
			return map_nt_error_from_unix(errno);
		}
		if (!(S_ISDIR(st.st_mode))) {
			return NT_STATUS_NOT_A_DIRECTORY;
		}
		ret = SMB_VFS_UNLINK(conn,directory);
	} else {
		ret = SMB_VFS_RMDIR(conn,directory);
	}
	if (ret == 0) {
		notify_fname(conn, NOTIFY_ACTION_REMOVED,
			     FILE_NOTIFY_CHANGE_DIR_NAME,
			     directory);
		return NT_STATUS_OK;
	}

	if(((errno == ENOTEMPTY)||(errno == EEXIST)) && lp_veto_files(SNUM(conn))) {
		/* 
		 * Check to see if the only thing in this directory are
		 * vetoed files/directories. If so then delete them and
		 * retry. If we fail to delete any of them (and we *don't*
		 * do a recursive delete) then fail the rmdir.
		 */
		const char *dname;
		long dirpos = 0;
		struct smb_Dir *dir_hnd = OpenDir(conn, directory, NULL, 0);

		if(dir_hnd == NULL) {
			errno = ENOTEMPTY;
			goto err;
		}

		while ((dname = ReadDirName(dir_hnd,&dirpos))) {
			if((strcmp(dname, ".") == 0) || (strcmp(dname, "..")==0))
				continue;
			if (!is_visible_file(conn, directory, dname, &st, False))
				continue;
			if(!IS_VETO_PATH(conn, dname)) {
				CloseDir(dir_hnd);
				errno = ENOTEMPTY;
				goto err;
			}
		}

		/* We only have veto files/directories. Recursive delete. */

		RewindDir(dir_hnd,&dirpos);
		while ((dname = ReadDirName(dir_hnd,&dirpos))) {
			pstring fullname;

			if((strcmp(dname, ".") == 0) || (strcmp(dname, "..")==0))
				continue;
			if (!is_visible_file(conn, directory, dname, &st, False))
				continue;

			/* Construct the full name. */
			if(strlen(directory) + strlen(dname) + 1 >= sizeof(fullname)) {
				errno = ENOMEM;
				break;
			}

			pstrcpy(fullname, directory);
			pstrcat(fullname, "/");
			pstrcat(fullname, dname);
                   
			if(SMB_VFS_LSTAT(conn,fullname, &st) != 0)
				break;
			if(st.st_mode & S_IFDIR) {
				if(lp_recursive_veto_delete(SNUM(conn))) {
					if(!recursive_rmdir(conn, fullname))
						break;
				}
				if(SMB_VFS_RMDIR(conn,fullname) != 0)
					break;
			} else if(SMB_VFS_UNLINK(conn,fullname) != 0)
				break;
		}
		CloseDir(dir_hnd);
		/* Retry the rmdir */
		ret = SMB_VFS_RMDIR(conn,directory);
	}

  err:

	if (ret != 0) {
		DEBUG(3,("rmdir_internals: couldn't remove directory %s : "
			 "%s\n", directory,strerror(errno)));
		return map_nt_error_from_unix(errno);
	}

	notify_fname(conn, NOTIFY_ACTION_REMOVED,
		     FILE_NOTIFY_CHANGE_DIR_NAME,
		     directory);

	return NT_STATUS_OK;
}

/****************************************************************************
 Reply to a rmdir.
****************************************************************************/

int reply_rmdir(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	pstring directory;
	int outsize = 0;
	SMB_STRUCT_STAT sbuf;
	NTSTATUS status;
	START_PROFILE(SMBrmdir);

	srvstr_get_path(inbuf, directory, smb_buf(inbuf) + 1, sizeof(directory), 0, STR_TERMINATE, &status);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBrmdir);
		return ERROR_NT(status);
	}

	status = resolve_dfspath(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, directory);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBrmdir);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}

	status = unix_convert(conn, directory, False, NULL, &sbuf);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBrmdir);
		return ERROR_NT(status);
	}
  
	status = check_name(conn, directory);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBrmdir);
		return ERROR_NT(status);
	}

	dptr_closepath(directory,SVAL(inbuf,smb_pid));
	status = rmdir_internals(conn, directory);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBrmdir);
		return ERROR_NT(status);
	}
 
	outsize = set_message(outbuf,0,0,False);
  
	DEBUG( 3, ( "rmdir %s\n", directory ) );
  
	END_PROFILE(SMBrmdir);
	return(outsize);
}

/*******************************************************************
 Resolve wildcards in a filename rename.
 Note that name is in UNIX charset and thus potentially can be more
 than fstring buffer (255 bytes) especially in default UTF-8 case.
 Therefore, we use pstring inside and all calls should ensure that
 name2 is at least pstring-long (they do already)
********************************************************************/

static BOOL resolve_wildcards(const char *name1, char *name2)
{
	pstring root1,root2;
	pstring ext1,ext2;
	char *p,*p2, *pname1, *pname2;
	int available_space, actual_space;
	
	pname1 = strrchr_m(name1,'/');
	pname2 = strrchr_m(name2,'/');

	if (!pname1 || !pname2)
		return(False);
  
	pstrcpy(root1,pname1);
	pstrcpy(root2,pname2);
	p = strrchr_m(root1,'.');
	if (p) {
		*p = 0;
		pstrcpy(ext1,p+1);
	} else {
		pstrcpy(ext1,"");    
	}
	p = strrchr_m(root2,'.');
	if (p) {
		*p = 0;
		pstrcpy(ext2,p+1);
	} else {
		pstrcpy(ext2,"");    
	}

	p = root1;
	p2 = root2;
	while (*p2) {
		if (*p2 == '?') {
			*p2 = *p;
			p2++;
		} else if (*p2 == '*') {
			pstrcpy(p2, p);
			break;
		} else {
			p2++;
		}
		if (*p)
			p++;
	}

	p = ext1;
	p2 = ext2;
	while (*p2) {
		if (*p2 == '?') {
			*p2 = *p;
			p2++;
		} else if (*p2 == '*') {
			pstrcpy(p2, p);
			break;
		} else {
			p2++;
		}
		if (*p)
			p++;
	}

	available_space = sizeof(pstring) - PTR_DIFF(pname2, name2);
	
	if (ext2[0]) {
		actual_space = snprintf(pname2, available_space - 1, "%s.%s", root2, ext2);
		if (actual_space >= available_space - 1) {
			DEBUG(1,("resolve_wildcards: can't fit resolved name into specified buffer (overrun by %d bytes)\n",
				actual_space - available_space));
		}
	} else {
		pstrcpy_base(pname2, root2, name2);
	}

	return(True);
}

/****************************************************************************
 Ensure open files have their names updated. Updated to notify other smbd's
 asynchronously.
****************************************************************************/

static void rename_open_files(connection_struct *conn, struct share_mode_lock *lck,
				SMB_DEV_T dev, SMB_INO_T inode, const char *newname)
{
	files_struct *fsp;
	BOOL did_rename = False;

	for(fsp = file_find_di_first(dev, inode); fsp; fsp = file_find_di_next(fsp)) {
		/* fsp_name is a relative path under the fsp. To change this for other
		   sharepaths we need to manipulate relative paths. */
		/* TODO - create the absolute path and manipulate the newname
		   relative to the sharepath. */
		if (fsp->conn != conn) {
			continue;
		}
		DEBUG(10,("rename_open_files: renaming file fnum %d (dev = %x, inode = %.0f) from %s -> %s\n",
			fsp->fnum, (unsigned int)fsp->dev, (double)fsp->inode,
			fsp->fsp_name, newname ));
		string_set(&fsp->fsp_name, newname);
		did_rename = True;
	}

	if (!did_rename) {
		DEBUG(10,("rename_open_files: no open files on dev %x, inode %.0f for %s\n",
			(unsigned int)dev, (double)inode, newname ));
	}

	/* Send messages to all smbd's (not ourself) that the name has changed. */
	rename_share_filename(lck, conn->connectpath, newname);
}

/****************************************************************************
 We need to check if the source path is a parent directory of the destination
 (ie. a rename of /foo/bar/baz -> /foo/bar/baz/bibble/bobble. If so we must
 refuse the rename with a sharing violation. Under UNIX the above call can
 *succeed* if /foo/bar/baz is a symlink to another area in the share. We
 probably need to check that the client is a Windows one before disallowing
 this as a UNIX client (one with UNIX extensions) can know the source is a
 symlink and make this decision intelligently. Found by an excellent bug
 report from <AndyLiebman@aol.com>.
****************************************************************************/

static BOOL rename_path_prefix_equal(const char *src, const char *dest)
{
	const char *psrc = src;
	const char *pdst = dest;
	size_t slen;

	if (psrc[0] == '.' && psrc[1] == '/') {
		psrc += 2;
	}
	if (pdst[0] == '.' && pdst[1] == '/') {
		pdst += 2;
	}
	if ((slen = strlen(psrc)) > strlen(pdst)) {
		return False;
	}
	return ((memcmp(psrc, pdst, slen) == 0) && pdst[slen] == '/');
}

/****************************************************************************
 Rename an open file - given an fsp.
****************************************************************************/

NTSTATUS rename_internals_fsp(connection_struct *conn, files_struct *fsp, pstring newname, uint32 attrs, BOOL replace_if_exists)
{
	SMB_STRUCT_STAT sbuf;
	pstring newname_last_component;
	NTSTATUS status = NT_STATUS_OK;
	BOOL dest_exists;
	struct share_mode_lock *lck = NULL;

	ZERO_STRUCT(sbuf);

	status = unix_convert(conn, newname, False, newname_last_component, &sbuf);

	/* If an error we expect this to be NT_STATUS_OBJECT_PATH_NOT_FOUND */

	if (!NT_STATUS_IS_OK(status) && !NT_STATUS_EQUAL(NT_STATUS_OBJECT_PATH_NOT_FOUND, status)) {
		return status;
	}

	status = check_name(conn, newname);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
  
	/* Ensure newname contains a '/' */
	if(strrchr_m(newname,'/') == 0) {
		pstring tmpstr;
		
		pstrcpy(tmpstr, "./");
		pstrcat(tmpstr, newname);
		pstrcpy(newname, tmpstr);
	}

	/*
	 * Check for special case with case preserving and not
	 * case sensitive. If the old last component differs from the original
	 * last component only by case, then we should allow
	 * the rename (user is trying to change the case of the
	 * filename).
	 */

	if((conn->case_sensitive == False) && (conn->case_preserve == True) &&
			strequal(newname, fsp->fsp_name)) {
		char *p;
		pstring newname_modified_last_component;

		/*
		 * Get the last component of the modified name.
		 * Note that we guarantee that newname contains a '/'
		 * character above.
		 */
		p = strrchr_m(newname,'/');
		pstrcpy(newname_modified_last_component,p+1);
			
		if(strcsequal(newname_modified_last_component, 
			      newname_last_component) == False) {
			/*
			 * Replace the modified last component with
			 * the original.
			 */
			pstrcpy(p+1, newname_last_component);
		}
	}

	/*
	 * If the src and dest names are identical - including case,
	 * don't do the rename, just return success.
	 */

	if (strcsequal(fsp->fsp_name, newname)) {
		DEBUG(3,("rename_internals_fsp: identical names in rename %s - returning success\n",
			newname));
		return NT_STATUS_OK;
	}

	dest_exists = vfs_object_exist(conn,newname,NULL);

	if(!replace_if_exists && dest_exists) {
		DEBUG(3,("rename_internals_fsp: dest exists doing rename %s -> %s\n",
			fsp->fsp_name,newname));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	/* Ensure we have a valid stat struct for the source. */
	if (fsp->fh->fd != -1) {
		if (SMB_VFS_FSTAT(fsp,fsp->fh->fd,&sbuf) == -1) {
			return map_nt_error_from_unix(errno);
		}
	} else {
		if (SMB_VFS_STAT(conn,fsp->fsp_name,&sbuf) == -1) {
			return map_nt_error_from_unix(errno);
		}
	}

	status = can_rename(conn,fsp->fsp_name,attrs,&sbuf,True);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3,("rename_internals_fsp: Error %s rename %s -> %s\n",
			nt_errstr(status), fsp->fsp_name,newname));
		if (NT_STATUS_EQUAL(status,NT_STATUS_SHARING_VIOLATION))
			status = NT_STATUS_ACCESS_DENIED;
		return status;
	}

	if (rename_path_prefix_equal(fsp->fsp_name, newname)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	lck = get_share_mode_lock(NULL, fsp->dev, fsp->inode, NULL, NULL);

	if(SMB_VFS_RENAME(conn,fsp->fsp_name, newname) == 0) {
		uint32 create_options = fsp->fh->private_options;

		DEBUG(3,("rename_internals_fsp: succeeded doing rename on %s -> %s\n",
			fsp->fsp_name,newname));

		rename_open_files(conn, lck, fsp->dev, fsp->inode, newname);

		/*
		 * A rename acts as a new file create w.r.t. allowing an initial delete
		 * on close, probably because in Windows there is a new handle to the
		 * new file. If initial delete on close was requested but not
		 * originally set, we need to set it here. This is probably not 100% correct,
		 * but will work for the CIFSFS client which in non-posix mode
		 * depends on these semantics. JRA.
		 */

		set_allow_initial_delete_on_close(lck, fsp, True);

		if (create_options & FILE_DELETE_ON_CLOSE) {
			status = can_set_delete_on_close(fsp, True, 0);

			if (NT_STATUS_IS_OK(status)) {
				/* Note that here we set the *inital* delete on close flag,
				 * not the regular one. The magic gets handled in close. */
				fsp->initial_delete_on_close = True;
			}
		}
		TALLOC_FREE(lck);
		return NT_STATUS_OK;	
	}

	TALLOC_FREE(lck);

	if (errno == ENOTDIR || errno == EISDIR) {
		status = NT_STATUS_OBJECT_NAME_COLLISION;
	} else {
		status = map_nt_error_from_unix(errno);
	}
		
	DEBUG(3,("rename_internals_fsp: Error %s rename %s -> %s\n",
		nt_errstr(status), fsp->fsp_name,newname));

	return status;
}

/*
 * Do the notify calls from a rename
 */

static void notify_rename(connection_struct *conn, BOOL is_dir,
			  const char *oldpath, const char *newpath)
{
	char *olddir, *newdir;
	const char *oldname, *newname;
	uint32 mask;

	mask = is_dir ? FILE_NOTIFY_CHANGE_DIR_NAME
		: FILE_NOTIFY_CHANGE_FILE_NAME;

	if (!parent_dirname_talloc(NULL, oldpath, &olddir, &oldname)
	    || !parent_dirname_talloc(NULL, newpath, &newdir, &newname)) {
		TALLOC_FREE(olddir);
		return;
	}

	if (strcmp(olddir, newdir) == 0) {
		notify_fname(conn, NOTIFY_ACTION_OLD_NAME, mask, oldpath);
		notify_fname(conn, NOTIFY_ACTION_NEW_NAME, mask, newpath);
	}
	else {
		notify_fname(conn, NOTIFY_ACTION_REMOVED, mask, oldpath);
		notify_fname(conn, NOTIFY_ACTION_ADDED, mask, newpath);
	}
	TALLOC_FREE(olddir);
	TALLOC_FREE(newdir);

	/* this is a strange one. w2k3 gives an additional event for
	   CHANGE_ATTRIBUTES and CHANGE_CREATION on the new file when renaming
	   files, but not directories */
	if (!is_dir) {
		notify_fname(conn, NOTIFY_ACTION_MODIFIED,
			     FILE_NOTIFY_CHANGE_ATTRIBUTES
			     |FILE_NOTIFY_CHANGE_CREATION,
			     newpath);
	}
}

/****************************************************************************
 The guts of the rename command, split out so it may be called by the NT SMB
 code. 
****************************************************************************/

NTSTATUS rename_internals(connection_struct *conn,
				pstring name,
				pstring newname,
				uint32 attrs,
				BOOL replace_if_exists,
				BOOL src_has_wild,
				BOOL dest_has_wild)
{
	pstring directory;
	pstring mask;
	pstring last_component_src;
	pstring last_component_dest;
	char *p;
	int count=0;
	NTSTATUS status = NT_STATUS_OK;
	SMB_STRUCT_STAT sbuf1, sbuf2;
	struct share_mode_lock *lck = NULL;
	struct smb_Dir *dir_hnd = NULL;
	const char *dname;
	long offset = 0;
	pstring destname;

	*directory = *mask = 0;

	ZERO_STRUCT(sbuf1);
	ZERO_STRUCT(sbuf2);

	status = unix_convert(conn, name, src_has_wild, last_component_src, &sbuf1);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = unix_convert(conn, newname, dest_has_wild, last_component_dest, &sbuf2);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/*
	 * Split the old name into directory and last component
	 * strings. Note that unix_convert may have stripped off a 
	 * leading ./ from both name and newname if the rename is 
	 * at the root of the share. We need to make sure either both
	 * name and newname contain a / character or neither of them do
	 * as this is checked in resolve_wildcards().
	 */

	p = strrchr_m(name,'/');
	if (!p) {
		pstrcpy(directory,".");
		pstrcpy(mask,name);
	} else {
		*p = 0;
		pstrcpy(directory,name);
		pstrcpy(mask,p+1);
		*p = '/'; /* Replace needed for exceptional test below. */
	}

	/*
	 * We should only check the mangled cache
	 * here if unix_convert failed. This means
	 * that the path in 'mask' doesn't exist
	 * on the file system and so we need to look
	 * for a possible mangle. This patch from
	 * Tine Smukavec <valentin.smukavec@hermes.si>.
	 */

	if (!VALID_STAT(sbuf1) && mangle_is_mangled(mask, conn->params)) {
		mangle_check_cache( mask, sizeof(pstring)-1, conn->params );
	}

	if (!src_has_wild) {
		/*
		 * No wildcards - just process the one file.
		 */
		BOOL is_short_name = mangle_is_8_3(name, True, conn->params);

		/* Add a terminating '/' to the directory name. */
		pstrcat(directory,"/");
		pstrcat(directory,mask);
		
		/* Ensure newname contains a '/' also */
		if(strrchr_m(newname,'/') == 0) {
			pstring tmpstr;
			
			pstrcpy(tmpstr, "./");
			pstrcat(tmpstr, newname);
			pstrcpy(newname, tmpstr);
		}
		
		DEBUG(3, ("rename_internals: case_sensitive = %d, "
			  "case_preserve = %d, short case preserve = %d, "
			  "directory = %s, newname = %s, "
			  "last_component_dest = %s, is_8_3 = %d\n", 
			  conn->case_sensitive, conn->case_preserve,
			  conn->short_case_preserve, directory, 
			  newname, last_component_dest, is_short_name));

		/* Ensure the source name is valid for us to access. */
		status = check_name(conn, directory);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		/* The dest name still may have wildcards. */
		if (dest_has_wild) {
			if (!resolve_wildcards(directory,newname)) {
				DEBUG(6, ("rename_internals: resolve_wildcards %s %s failed\n", 
					  directory,newname));
				return NT_STATUS_NO_MEMORY;
			}
		}
				
		/*
		 * Check for special case with case preserving and not
		 * case sensitive, if directory and newname are identical,
		 * and the old last component differs from the original
		 * last component only by case, then we should allow
		 * the rename (user is trying to change the case of the
		 * filename).
		 */
		if((conn->case_sensitive == False) && 
		   (((conn->case_preserve == True) && 
		     (is_short_name == False)) || 
		    ((conn->short_case_preserve == True) && 
		     (is_short_name == True))) &&
		   strcsequal(directory, newname)) {
			pstring modified_last_component;

			/*
			 * Get the last component of the modified name.
			 * Note that we guarantee that newname contains a '/'
			 * character above.
			 */
			p = strrchr_m(newname,'/');
			pstrcpy(modified_last_component,p+1);
			
			if(strcsequal(modified_last_component, 
				      last_component_dest) == False) {
				/*
				 * Replace the modified last component with
				 * the original.
				 */
				pstrcpy(p+1, last_component_dest);
			}
		}
	
		/* Ensure the dest name is valid for us to access. */
		status = check_name(conn, newname);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		/*
		 * The source object must exist.
		 */

		if (!vfs_object_exist(conn, directory, &sbuf1)) {
			DEBUG(3, ("rename_internals: source doesn't exist "
				  "doing rename %s -> %s\n",
				directory,newname));

			if (errno == ENOTDIR || errno == EISDIR
			    || errno == ENOENT) {
				/*
				 * Must return different errors depending on
				 * whether the parent directory existed or
				 * not.
				 */

				p = strrchr_m(directory, '/');
				if (!p)
					return NT_STATUS_OBJECT_NAME_NOT_FOUND;
				*p = '\0';
				if (vfs_object_exist(conn, directory, NULL))
					return NT_STATUS_OBJECT_NAME_NOT_FOUND;
				return NT_STATUS_OBJECT_PATH_NOT_FOUND;
			}
			status = map_nt_error_from_unix(errno);
			DEBUG(3, ("rename_internals: Error %s rename %s -> "
				  "%s\n", nt_errstr(status), directory,
				  newname));

			return status;
		}

		status = can_rename(conn,directory,attrs,&sbuf1,False);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(3,("rename_internals: Error %s rename %s -> "
				 "%s\n", nt_errstr(status), directory,
				 newname));
			return status;
		}

		/*
		 * If the src and dest names are identical - including case,
		 * don't do the rename, just return success.
		 */

		if (strcsequal(directory, newname)) {
			DEBUG(3, ("rename_internals: identical names in "
				  "rename %s - returning success\n",
				  directory));
			return NT_STATUS_OK;
		}

		if(!replace_if_exists && vfs_object_exist(conn,newname,NULL)) {
			DEBUG(3,("rename_internals: dest exists doing "
				 "rename %s -> %s\n", directory, newname));
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}

		if (rename_path_prefix_equal(directory, newname)) {
			return NT_STATUS_SHARING_VIOLATION;
		}

		lck = get_share_mode_lock(NULL, sbuf1.st_dev, sbuf1.st_ino,
					  NULL, NULL);

		if(SMB_VFS_RENAME(conn,directory, newname) == 0) {
			DEBUG(3,("rename_internals: succeeded doing rename "
				 "on %s -> %s\n", directory, newname));
			notify_rename(conn, S_ISDIR(sbuf1.st_mode),
				      directory, newname);
			rename_open_files(conn, lck, sbuf1.st_dev,
					  sbuf1.st_ino, newname);
			TALLOC_FREE(lck);
			return NT_STATUS_OK;	
		}

		TALLOC_FREE(lck);
		if (errno == ENOTDIR || errno == EISDIR) {
			status = NT_STATUS_OBJECT_NAME_COLLISION;
		} else {
			status = map_nt_error_from_unix(errno);
		}
		
		DEBUG(3,("rename_internals: Error %s rename %s -> %s\n",
			nt_errstr(status), directory,newname));

		return status;
	}

	/*
	 * Wildcards - process each file that matches.
	 */
	if (strequal(mask,"????????.???")) {
		pstrcpy(mask,"*");
	}
			
	status = check_name(conn, directory);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	
	dir_hnd = OpenDir(conn, directory, mask, attrs);
	if (dir_hnd == NULL) {
		return map_nt_error_from_unix(errno);
	}
		
	status = NT_STATUS_NO_SUCH_FILE;
	/*
	 * Was status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
	 * - gentest fix. JRA
	 */
			
	while ((dname = ReadDirName(dir_hnd, &offset))) {
		pstring fname;
		BOOL sysdir_entry = False;

		pstrcpy(fname,dname);
				
		/* Quick check for "." and ".." */
		if (fname[0] == '.') {
			if (!fname[1] || (fname[1] == '.' && !fname[2])) {
				if (attrs & aDIR) {
					sysdir_entry = True;
				} else {
					continue;
				}
			}
		}

		if (!is_visible_file(conn, directory, dname, &sbuf1, False)) {
			continue;
		}

		if(!mask_match(fname, mask, conn->case_sensitive)) {
			continue;
		}
				
		if (sysdir_entry) {
			status = NT_STATUS_OBJECT_NAME_INVALID;
			break;
		}

		status = NT_STATUS_ACCESS_DENIED;
		slprintf(fname, sizeof(fname)-1, "%s/%s", directory, dname);

		/* Ensure the source name is valid for us to access. */
		status = check_name(conn, fname);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		if (!vfs_object_exist(conn, fname, &sbuf1)) {
			status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
			DEBUG(6, ("rename %s failed. Error %s\n",
				  fname, nt_errstr(status)));
			continue;
		}
		status = can_rename(conn,fname,attrs,&sbuf1,False);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(6, ("rename %s refused\n", fname));
			continue;
		}
		pstrcpy(destname,newname);
			
		if (!resolve_wildcards(fname,destname)) {
			DEBUG(6, ("resolve_wildcards %s %s failed\n", 
				  fname, destname));
			continue;
		}
				
		/* Ensure the dest name is valid for us to access. */
		status = check_name(conn, destname);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		if (strcsequal(fname,destname)) {
			DEBUG(3,("rename_internals: identical names "
				 "in wildcard rename %s - success\n",
				 fname));
			count++;
			status = NT_STATUS_OK;
			continue;
		}

		if (!replace_if_exists && vfs_file_exist(conn,destname, NULL)) {
			DEBUG(6,("file_exist %s\n", destname));
			status = NT_STATUS_OBJECT_NAME_COLLISION;
			continue;
		}
				
		if (rename_path_prefix_equal(fname, destname)) {
			return NT_STATUS_SHARING_VIOLATION;
		}

		lck = get_share_mode_lock(NULL, sbuf1.st_dev,
					  sbuf1.st_ino, NULL, NULL);

		if (!SMB_VFS_RENAME(conn,fname,destname)) {
			rename_open_files(conn, lck, sbuf1.st_dev,
					  sbuf1.st_ino, newname);
			count++;
			status = NT_STATUS_OK;
		}
		TALLOC_FREE(lck);
		DEBUG(3,("rename_internals: doing rename on %s -> "
			 "%s\n",fname,destname));
	}
	CloseDir(dir_hnd);

	if (count == 0 && NT_STATUS_IS_OK(status)) {
		status = map_nt_error_from_unix(errno);
	}
	
	return status;
}

/****************************************************************************
 Reply to a mv.
****************************************************************************/

int reply_mv(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, 
	     int dum_buffsize)
{
	int outsize = 0;
	pstring name;
	pstring newname;
	char *p;
	uint32 attrs = SVAL(inbuf,smb_vwv0);
	NTSTATUS status;
	BOOL src_has_wcard = False;
	BOOL dest_has_wcard = False;

	START_PROFILE(SMBmv);

	p = smb_buf(inbuf) + 1;
	p += srvstr_get_path_wcard(inbuf, name, p, sizeof(name), 0, STR_TERMINATE, &status, &src_has_wcard);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBmv);
		return ERROR_NT(status);
	}
	p++;
	p += srvstr_get_path_wcard(inbuf, newname, p, sizeof(newname), 0, STR_TERMINATE, &status, &dest_has_wcard);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBmv);
		return ERROR_NT(status);
	}
	
	status = resolve_dfspath_wcard(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, name, &src_has_wcard);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBmv);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}

	status = resolve_dfspath_wcard(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, newname, &dest_has_wcard);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBmv);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}
	
	DEBUG(3,("reply_mv : %s -> %s\n",name,newname));
	
	status = rename_internals(conn, name, newname, attrs, False, src_has_wcard, dest_has_wcard);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBmv);
		if (open_was_deferred(SVAL(inbuf,smb_mid))) {
			/* We have re-scheduled this call. */
			return -1;
		}
		return ERROR_NT(status);
	}

	outsize = set_message(outbuf,0,0,False);
  
	END_PROFILE(SMBmv);
	return(outsize);
}

/*******************************************************************
 Copy a file as part of a reply_copy.
******************************************************************/

/*
 * TODO: check error codes on all callers
 */

NTSTATUS copy_file(connection_struct *conn,
			char *src,
			char *dest1,
			int ofun,
			int count,
			BOOL target_is_directory)
{
	SMB_STRUCT_STAT src_sbuf, sbuf2;
	SMB_OFF_T ret=-1;
	files_struct *fsp1,*fsp2;
	pstring dest;
 	uint32 dosattrs;
	uint32 new_create_disposition;
	NTSTATUS status;
 
	pstrcpy(dest,dest1);
	if (target_is_directory) {
		char *p = strrchr_m(src,'/');
		if (p) {
			p++;
		} else {
			p = src;
		}
		pstrcat(dest,"/");
		pstrcat(dest,p);
	}

	if (!vfs_file_exist(conn,src,&src_sbuf)) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (!target_is_directory && count) {
		new_create_disposition = FILE_OPEN;
	} else {
		if (!map_open_params_to_ntcreate(dest1,0,ofun,
				NULL, NULL, &new_create_disposition, NULL)) {
			return NT_STATUS_INVALID_PARAMETER;
		}
	}

	status = open_file_ntcreate(conn,src,&src_sbuf,
			FILE_GENERIC_READ,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			FILE_OPEN,
			0,
			FILE_ATTRIBUTE_NORMAL,
			INTERNAL_OPEN_ONLY,
			NULL, &fsp1);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	dosattrs = dos_mode(conn, src, &src_sbuf);
	if (SMB_VFS_STAT(conn,dest,&sbuf2) == -1) {
		ZERO_STRUCTP(&sbuf2);
	}

	status = open_file_ntcreate(conn,dest,&sbuf2,
			FILE_GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			new_create_disposition,
			0,
			dosattrs,
			INTERNAL_OPEN_ONLY,
			NULL, &fsp2);

	if (!NT_STATUS_IS_OK(status)) {
		close_file(fsp1,ERROR_CLOSE);
		return status;
	}

	if ((ofun&3) == 1) {
		if(SMB_VFS_LSEEK(fsp2,fsp2->fh->fd,0,SEEK_END) == -1) {
			DEBUG(0,("copy_file: error - vfs lseek returned error %s\n", strerror(errno) ));
			/*
			 * Stop the copy from occurring.
			 */
			ret = -1;
			src_sbuf.st_size = 0;
		}
	}
  
	if (src_sbuf.st_size) {
		ret = vfs_transfer_file(fsp1, fsp2, src_sbuf.st_size);
	}

	close_file(fsp1,NORMAL_CLOSE);

	/* Ensure the modtime is set correctly on the destination file. */
	fsp_set_pending_modtime( fsp2, get_mtimespec(&src_sbuf));

	/*
	 * As we are opening fsp1 read-only we only expect
	 * an error on close on fsp2 if we are out of space.
	 * Thus we don't look at the error return from the
	 * close of fsp1.
	 */
	status = close_file(fsp2,NORMAL_CLOSE);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (ret != (SMB_OFF_T)src_sbuf.st_size) {
		return NT_STATUS_DISK_FULL;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Reply to a file copy.
****************************************************************************/

int reply_copy(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int outsize = 0;
	pstring name;
	pstring directory;
	pstring mask,newname;
	char *p;
	int count=0;
	int error = ERRnoaccess;
	int err = 0;
	int tid2 = SVAL(inbuf,smb_vwv0);
	int ofun = SVAL(inbuf,smb_vwv1);
	int flags = SVAL(inbuf,smb_vwv2);
	BOOL target_is_directory=False;
	BOOL source_has_wild = False;
	BOOL dest_has_wild = False;
	SMB_STRUCT_STAT sbuf1, sbuf2;
	NTSTATUS status;
	START_PROFILE(SMBcopy);

	*directory = *mask = 0;

	p = smb_buf(inbuf);
	p += srvstr_get_path_wcard(inbuf, name, p, sizeof(name), 0, STR_TERMINATE, &status, &source_has_wild);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBcopy);
		return ERROR_NT(status);
	}
	p += srvstr_get_path_wcard(inbuf, newname, p, sizeof(newname), 0, STR_TERMINATE, &status, &dest_has_wild);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBcopy);
		return ERROR_NT(status);
	}
   
	DEBUG(3,("reply_copy : %s -> %s\n",name,newname));
   
	if (tid2 != conn->cnum) {
		/* can't currently handle inter share copies XXXX */
		DEBUG(3,("Rejecting inter-share copy\n"));
		END_PROFILE(SMBcopy);
		return ERROR_DOS(ERRSRV,ERRinvdevice);
	}

	status = resolve_dfspath_wcard(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, name, &source_has_wild);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBcopy);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}

	status = resolve_dfspath_wcard(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, newname, &dest_has_wild);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBcopy);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}

	status = unix_convert(conn, name, source_has_wild, NULL, &sbuf1);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBcopy);
		return ERROR_NT(status);
	}

	status = unix_convert(conn, newname, dest_has_wild, NULL, &sbuf2);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBcopy);
		return ERROR_NT(status);
	}

	target_is_directory = VALID_STAT_OF_DIR(sbuf2);

	if ((flags&1) && target_is_directory) {
		END_PROFILE(SMBcopy);
		return ERROR_DOS(ERRDOS,ERRbadfile);
	}

	if ((flags&2) && !target_is_directory) {
		END_PROFILE(SMBcopy);
		return ERROR_DOS(ERRDOS,ERRbadpath);
	}

	if ((flags&(1<<5)) && VALID_STAT_OF_DIR(sbuf1)) {
		/* wants a tree copy! XXXX */
		DEBUG(3,("Rejecting tree copy\n"));
		END_PROFILE(SMBcopy);
		return ERROR_DOS(ERRSRV,ERRerror);
	}

	p = strrchr_m(name,'/');
	if (!p) {
		pstrcpy(directory,"./");
		pstrcpy(mask,name);
	} else {
		*p = 0;
		pstrcpy(directory,name);
		pstrcpy(mask,p+1);
	}

	/*
	 * We should only check the mangled cache
	 * here if unix_convert failed. This means
	 * that the path in 'mask' doesn't exist
	 * on the file system and so we need to look
	 * for a possible mangle. This patch from
	 * Tine Smukavec <valentin.smukavec@hermes.si>.
	 */

	if (!VALID_STAT(sbuf1) && mangle_is_mangled(mask, conn->params)) {
		mangle_check_cache( mask, sizeof(pstring)-1, conn->params );
	}

	if (!source_has_wild) {
		pstrcat(directory,"/");
		pstrcat(directory,mask);
		if (dest_has_wild) {
			if (!resolve_wildcards(directory,newname)) {
				END_PROFILE(SMBcopy);
				return ERROR_NT(NT_STATUS_NO_MEMORY);
			}
		}

		status = check_name(conn, directory);
		if (!NT_STATUS_IS_OK(status)) {
			return ERROR_NT(status);
		}
		
		status = check_name(conn, newname);
		if (!NT_STATUS_IS_OK(status)) {
			return ERROR_NT(status);
		}
		
		status = copy_file(conn,directory,newname,ofun,
					count,target_is_directory);

		if(!NT_STATUS_IS_OK(status)) {
			END_PROFILE(SMBcopy);
			return ERROR_NT(status);
		} else {
			count++;
		}
	} else {
		struct smb_Dir *dir_hnd = NULL;
		const char *dname;
		long offset = 0;
		pstring destname;

		if (strequal(mask,"????????.???"))
			pstrcpy(mask,"*");

		status = check_name(conn, directory);
		if (!NT_STATUS_IS_OK(status)) {
			return ERROR_NT(status);
		}
		
		dir_hnd = OpenDir(conn, directory, mask, 0);
		if (dir_hnd == NULL) {
			status = map_nt_error_from_unix(errno);
			return ERROR_NT(status);
		}

		error = ERRbadfile;

		while ((dname = ReadDirName(dir_hnd, &offset))) {
			pstring fname;
			pstrcpy(fname,dname);
    
			if (!is_visible_file(conn, directory, dname, &sbuf1, False)) {
				continue;
			}

			if(!mask_match(fname, mask, conn->case_sensitive)) {
				continue;
			}

			error = ERRnoaccess;
			slprintf(fname,sizeof(fname)-1, "%s/%s",directory,dname);
			pstrcpy(destname,newname);
			if (!resolve_wildcards(fname,destname)) {
				continue;
			}

			status = check_name(conn, fname);
			if (!NT_STATUS_IS_OK(status)) {
				CloseDir(dir_hnd);
				return ERROR_NT(status);
			}
		
			status = check_name(conn, destname);
			if (!NT_STATUS_IS_OK(status)) {
				CloseDir(dir_hnd);
				return ERROR_NT(status);
			}
		
			DEBUG(3,("reply_copy : doing copy on %s -> %s\n",fname, destname));

			status = copy_file(conn,fname,destname,ofun,
					count,target_is_directory);
			if (NT_STATUS_IS_OK(status)) {
				count++;
			}
		}
		CloseDir(dir_hnd);
	}
  
	if (count == 0) {
		if(err) {
			/* Error on close... */
			errno = err;
			END_PROFILE(SMBcopy);
			return(UNIXERROR(ERRHRD,ERRgeneral));
		}

		END_PROFILE(SMBcopy);
		return ERROR_DOS(ERRDOS,error);
	}
  
	outsize = set_message(outbuf,1,0,True);
	SSVAL(outbuf,smb_vwv0,count);

	END_PROFILE(SMBcopy);
	return(outsize);
}

/****************************************************************************
 Reply to a setdir.
****************************************************************************/

int reply_setdir(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int snum;
	int outsize = 0;
	pstring newdir;
	NTSTATUS status;

	START_PROFILE(pathworks_setdir);
  
	snum = SNUM(conn);
	if (!CAN_SETDIR(snum)) {
		END_PROFILE(pathworks_setdir);
		return ERROR_DOS(ERRDOS,ERRnoaccess);
	}

	srvstr_get_path(inbuf, newdir, smb_buf(inbuf) + 1, sizeof(newdir), 0, STR_TERMINATE, &status);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(pathworks_setdir);
		return ERROR_NT(status);
	}
  
	status = resolve_dfspath(conn, SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES, newdir);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(pathworks_setdir);
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			return ERROR_BOTH(NT_STATUS_PATH_NOT_COVERED, ERRSRV, ERRbadpath);
		}
		return ERROR_NT(status);
	}

	if (strlen(newdir) != 0) {
		if (!vfs_directory_exist(conn,newdir,NULL)) {
			END_PROFILE(pathworks_setdir);
			return ERROR_DOS(ERRDOS,ERRbadpath);
		}
		set_conn_connectpath(conn,newdir);
	}
  
	outsize = set_message(outbuf,0,0,False);
	SCVAL(outbuf,smb_reh,CVAL(inbuf,smb_reh));
  
	DEBUG(3,("setdir %s\n", newdir));

	END_PROFILE(pathworks_setdir);
	return(outsize);
}

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_LOCKING

/****************************************************************************
 Get a lock pid, dealing with large count requests.
****************************************************************************/

uint32 get_lock_pid( char *data, int data_offset, BOOL large_file_format)
{
	if(!large_file_format)
		return (uint32)SVAL(data,SMB_LPID_OFFSET(data_offset));
	else
		return (uint32)SVAL(data,SMB_LARGE_LPID_OFFSET(data_offset));
}

/****************************************************************************
 Get a lock count, dealing with large count requests.
****************************************************************************/

SMB_BIG_UINT get_lock_count( char *data, int data_offset, BOOL large_file_format)
{
	SMB_BIG_UINT count = 0;

	if(!large_file_format) {
		count = (SMB_BIG_UINT)IVAL(data,SMB_LKLEN_OFFSET(data_offset));
	} else {

#if defined(HAVE_LONGLONG)
		count = (((SMB_BIG_UINT) IVAL(data,SMB_LARGE_LKLEN_OFFSET_HIGH(data_offset))) << 32) |
			((SMB_BIG_UINT) IVAL(data,SMB_LARGE_LKLEN_OFFSET_LOW(data_offset)));
#else /* HAVE_LONGLONG */

		/*
		 * NT4.x seems to be broken in that it sends large file (64 bit)
		 * lockingX calls even if the CAP_LARGE_FILES was *not*
		 * negotiated. For boxes without large unsigned ints truncate the
		 * lock count by dropping the top 32 bits.
		 */

		if(IVAL(data,SMB_LARGE_LKLEN_OFFSET_HIGH(data_offset)) != 0) {
			DEBUG(3,("get_lock_count: truncating lock count (high)0x%x (low)0x%x to just low count.\n",
				(unsigned int)IVAL(data,SMB_LARGE_LKLEN_OFFSET_HIGH(data_offset)),
				(unsigned int)IVAL(data,SMB_LARGE_LKLEN_OFFSET_LOW(data_offset)) ));
				SIVAL(data,SMB_LARGE_LKLEN_OFFSET_HIGH(data_offset),0);
		}

		count = (SMB_BIG_UINT)IVAL(data,SMB_LARGE_LKLEN_OFFSET_LOW(data_offset));
#endif /* HAVE_LONGLONG */
	}

	return count;
}

#if !defined(HAVE_LONGLONG)
/****************************************************************************
 Pathetically try and map a 64 bit lock offset into 31 bits. I hate Windows :-).
****************************************************************************/

static uint32 map_lock_offset(uint32 high, uint32 low)
{
	unsigned int i;
	uint32 mask = 0;
	uint32 highcopy = high;
 
	/*
	 * Try and find out how many significant bits there are in high.
	 */
 
	for(i = 0; highcopy; i++)
		highcopy >>= 1;
 
	/*
	 * We use 31 bits not 32 here as POSIX
	 * lock offsets may not be negative.
	 */
 
	mask = (~0) << (31 - i);
 
	if(low & mask)
		return 0; /* Fail. */
 
	high <<= (31 - i);
 
	return (high|low);
}
#endif /* !defined(HAVE_LONGLONG) */

/****************************************************************************
 Get a lock offset, dealing with large offset requests.
****************************************************************************/

SMB_BIG_UINT get_lock_offset( char *data, int data_offset, BOOL large_file_format, BOOL *err)
{
	SMB_BIG_UINT offset = 0;

	*err = False;

	if(!large_file_format) {
		offset = (SMB_BIG_UINT)IVAL(data,SMB_LKOFF_OFFSET(data_offset));
	} else {

#if defined(HAVE_LONGLONG)
		offset = (((SMB_BIG_UINT) IVAL(data,SMB_LARGE_LKOFF_OFFSET_HIGH(data_offset))) << 32) |
				((SMB_BIG_UINT) IVAL(data,SMB_LARGE_LKOFF_OFFSET_LOW(data_offset)));
#else /* HAVE_LONGLONG */

		/*
		 * NT4.x seems to be broken in that it sends large file (64 bit)
		 * lockingX calls even if the CAP_LARGE_FILES was *not*
		 * negotiated. For boxes without large unsigned ints mangle the
		 * lock offset by mapping the top 32 bits onto the lower 32.
		 */
      
		if(IVAL(data,SMB_LARGE_LKOFF_OFFSET_HIGH(data_offset)) != 0) {
			uint32 low = IVAL(data,SMB_LARGE_LKOFF_OFFSET_LOW(data_offset));
			uint32 high = IVAL(data,SMB_LARGE_LKOFF_OFFSET_HIGH(data_offset));
			uint32 new_low = 0;

			if((new_low = map_lock_offset(high, low)) == 0) {
				*err = True;
				return (SMB_BIG_UINT)-1;
			}

			DEBUG(3,("get_lock_offset: truncating lock offset (high)0x%x (low)0x%x to offset 0x%x.\n",
				(unsigned int)high, (unsigned int)low, (unsigned int)new_low ));
			SIVAL(data,SMB_LARGE_LKOFF_OFFSET_HIGH(data_offset),0);
			SIVAL(data,SMB_LARGE_LKOFF_OFFSET_LOW(data_offset),new_low);
		}

		offset = (SMB_BIG_UINT)IVAL(data,SMB_LARGE_LKOFF_OFFSET_LOW(data_offset));
#endif /* HAVE_LONGLONG */
	}

	return offset;
}

/****************************************************************************
 Reply to a lockingX request.
****************************************************************************/

int reply_lockingX(connection_struct *conn, char *inbuf, char *outbuf,
		   int length, int bufsize)
{
	files_struct *fsp = file_fsp(inbuf,smb_vwv2);
	unsigned char locktype = CVAL(inbuf,smb_vwv3);
	unsigned char oplocklevel = CVAL(inbuf,smb_vwv3+1);
	uint16 num_ulocks = SVAL(inbuf,smb_vwv6);
	uint16 num_locks = SVAL(inbuf,smb_vwv7);
	SMB_BIG_UINT count = 0, offset = 0;
	uint32 lock_pid;
	int32 lock_timeout = IVAL(inbuf,smb_vwv4);
	int i;
	char *data;
	BOOL large_file_format =
		(locktype & LOCKING_ANDX_LARGE_FILES)?True:False;
	BOOL err;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;

	START_PROFILE(SMBlockingX);
	
	CHECK_FSP(fsp,conn);
	
	data = smb_buf(inbuf);

	if (locktype & LOCKING_ANDX_CHANGE_LOCKTYPE) {
		/* we don't support these - and CANCEL_LOCK makes w2k
		   and XP reboot so I don't really want to be
		   compatible! (tridge) */
		return ERROR_NT(NT_STATUS_DOS(ERRDOS, ERRnoatomiclocks));
	}
	
	/* Check if this is an oplock break on a file
	   we have granted an oplock on.
	*/
	if ((locktype & LOCKING_ANDX_OPLOCK_RELEASE)) {
		/* Client can insist on breaking to none. */
		BOOL break_to_none = (oplocklevel == 0);
		BOOL result;

		DEBUG(5,("reply_lockingX: oplock break reply (%u) from client "
			 "for fnum = %d\n", (unsigned int)oplocklevel,
			 fsp->fnum ));

		/*
		 * Make sure we have granted an exclusive or batch oplock on
		 * this file.
		 */
		
		if (fsp->oplock_type == 0) {

			/* The Samba4 nbench simulator doesn't understand
			   the difference between break to level2 and break
			   to none from level2 - it sends oplock break
			   replies in both cases. Don't keep logging an error
			   message here - just ignore it. JRA. */

			DEBUG(5,("reply_lockingX: Error : oplock break from "
				 "client for fnum = %d (oplock=%d) and no "
				 "oplock granted on this file (%s).\n",
				 fsp->fnum, fsp->oplock_type, fsp->fsp_name));

			/* if this is a pure oplock break request then don't
			 * send a reply */
			if (num_locks == 0 && num_ulocks == 0) {
				END_PROFILE(SMBlockingX);
				return -1;
			} else {
				END_PROFILE(SMBlockingX);
				return ERROR_DOS(ERRDOS,ERRlock);
			}
		}

		if ((fsp->sent_oplock_break == BREAK_TO_NONE_SENT) ||
		    (break_to_none)) {
			result = remove_oplock(fsp);
		} else {
			result = downgrade_oplock(fsp);
		}
		
		if (!result) {
			DEBUG(0, ("reply_lockingX: error in removing "
				  "oplock on file %s\n", fsp->fsp_name));
			/* Hmmm. Is this panic justified? */
			smb_panic("internal tdb error");
		}

		reply_to_oplock_break_requests(fsp);

		/* if this is a pure oplock break request then don't send a
		 * reply */
		if (num_locks == 0 && num_ulocks == 0) {
			/* Sanity check - ensure a pure oplock break is not a
			   chained request. */
			if(CVAL(inbuf,smb_vwv0) != 0xff)
				DEBUG(0,("reply_lockingX: Error : pure oplock "
					 "break is a chained %d request !\n",
					 (unsigned int)CVAL(inbuf,smb_vwv0) ));
			END_PROFILE(SMBlockingX);
			return -1;
		}
	}

	/*
	 * We do this check *after* we have checked this is not a oplock break
	 * response message. JRA.
	 */
	
	release_level_2_oplocks_on_change(fsp);
	
	/* Data now points at the beginning of the list
	   of smb_unlkrng structs */
	for(i = 0; i < (int)num_ulocks; i++) {
		lock_pid = get_lock_pid( data, i, large_file_format);
		count = get_lock_count( data, i, large_file_format);
		offset = get_lock_offset( data, i, large_file_format, &err);
		
		/*
		 * There is no error code marked "stupid client bug".... :-).
		 */
		if(err) {
			END_PROFILE(SMBlockingX);
			return ERROR_DOS(ERRDOS,ERRnoaccess);
		}

		DEBUG(10,("reply_lockingX: unlock start=%.0f, len=%.0f for "
			  "pid %u, file %s\n", (double)offset, (double)count,
			  (unsigned int)lock_pid, fsp->fsp_name ));
		
		status = do_unlock(fsp,
				lock_pid,
				count,
				offset,
				WINDOWS_LOCK);

		if (NT_STATUS_V(status)) {
			END_PROFILE(SMBlockingX);
			return ERROR_NT(status);
		}
	}

	/* Setup the timeout in seconds. */

	if (!lp_blocking_locks(SNUM(conn))) {
		lock_timeout = 0;
	}
	
	/* Now do any requested locks */
	data += ((large_file_format ? 20 : 10)*num_ulocks);
	
	/* Data now points at the beginning of the list
	   of smb_lkrng structs */
	
	for(i = 0; i < (int)num_locks; i++) {
		enum brl_type lock_type = ((locktype & LOCKING_ANDX_SHARED_LOCK) ?
				READ_LOCK:WRITE_LOCK);
		lock_pid = get_lock_pid( data, i, large_file_format);
		count = get_lock_count( data, i, large_file_format);
		offset = get_lock_offset( data, i, large_file_format, &err);
		
		/*
		 * There is no error code marked "stupid client bug".... :-).
		 */
		if(err) {
			END_PROFILE(SMBlockingX);
			return ERROR_DOS(ERRDOS,ERRnoaccess);
		}
		
		DEBUG(10,("reply_lockingX: lock start=%.0f, len=%.0f for pid "
			  "%u, file %s timeout = %d\n", (double)offset,
			  (double)count, (unsigned int)lock_pid,
			  fsp->fsp_name, (int)lock_timeout ));
		
		if (locktype & LOCKING_ANDX_CANCEL_LOCK) {
			if (lp_blocking_locks(SNUM(conn))) {

				/* Schedule a message to ourselves to
				   remove the blocking lock record and
				   return the right error. */

				if (!blocking_lock_cancel(fsp,
						lock_pid,
						offset,
						count,
						WINDOWS_LOCK,
						locktype,
						NT_STATUS_FILE_LOCK_CONFLICT)) {
					END_PROFILE(SMBlockingX);
					return ERROR_NT(NT_STATUS_DOS(ERRDOS, ERRcancelviolation));
				}
			}
			/* Remove a matching pending lock. */
			status = do_lock_cancel(fsp,
						lock_pid,
						count,
						offset,
						WINDOWS_LOCK);
		} else {
			BOOL blocking_lock = lock_timeout ? True : False;
			BOOL defer_lock = False;
			struct byte_range_lock *br_lck;
			uint32 block_smbpid;

			br_lck = do_lock(fsp,
					lock_pid,
					count,
					offset, 
					lock_type,
					WINDOWS_LOCK,
					blocking_lock,
					&status,
					&block_smbpid);

			if (br_lck && blocking_lock && ERROR_WAS_LOCK_DENIED(status)) {
				/* Windows internal resolution for blocking locks seems
				   to be about 200ms... Don't wait for less than that. JRA. */
				if (lock_timeout != -1 && lock_timeout < lp_lock_spin_time()) {
					lock_timeout = lp_lock_spin_time();
				}
				defer_lock = True;
			}

			/* This heuristic seems to match W2K3 very well. If a
			   lock sent with timeout of zero would fail with NT_STATUS_FILE_LOCK_CONFLICT
			   it pretends we asked for a timeout of between 150 - 300 milliseconds as
			   far as I can tell. Replacement for do_lock_spin(). JRA. */

			if (br_lck && lp_blocking_locks(SNUM(conn)) && !blocking_lock &&
					NT_STATUS_EQUAL((status), NT_STATUS_FILE_LOCK_CONFLICT)) {
				defer_lock = True;
				lock_timeout = lp_lock_spin_time();
			}

			if (br_lck && defer_lock) {
				/*
				 * A blocking lock was requested. Package up
				 * this smb into a queued request and push it
				 * onto the blocking lock queue.
				 */
				if(push_blocking_lock_request(br_lck,
							inbuf, length,
							fsp,
							lock_timeout,
							i,
							lock_pid,
							lock_type,
							WINDOWS_LOCK,
							offset,
							count,
							block_smbpid)) {
					TALLOC_FREE(br_lck);
					END_PROFILE(SMBlockingX);
					return -1;
				}
			}

			TALLOC_FREE(br_lck);
		}

		if (NT_STATUS_V(status)) {
			END_PROFILE(SMBlockingX);
			return ERROR_NT(status);
		}
	}
	
	/* If any of the above locks failed, then we must unlock
	   all of the previous locks (X/Open spec). */

	if (!(locktype & LOCKING_ANDX_CANCEL_LOCK) &&
			(i != num_locks) &&
			(num_locks != 0)) {
		/*
		 * Ensure we don't do a remove on the lock that just failed,
		 * as under POSIX rules, if we have a lock already there, we
		 * will delete it (and we shouldn't) .....
		 */
		for(i--; i >= 0; i--) {
			lock_pid = get_lock_pid( data, i, large_file_format);
			count = get_lock_count( data, i, large_file_format);
			offset = get_lock_offset( data, i, large_file_format,
						  &err);
			
			/*
			 * There is no error code marked "stupid client
			 * bug".... :-).
			 */
			if(err) {
				END_PROFILE(SMBlockingX);
				return ERROR_DOS(ERRDOS,ERRnoaccess);
			}
			
			do_unlock(fsp,
				lock_pid,
				count,
				offset,
				WINDOWS_LOCK);
		}
		END_PROFILE(SMBlockingX);
		return ERROR_NT(status);
	}

	set_message(outbuf,2,0,True);
	
	DEBUG(3, ("lockingX fnum=%d type=%d num_locks=%d num_ulocks=%d\n",
		  fsp->fnum, (unsigned int)locktype, num_locks, num_ulocks));
	
	END_PROFILE(SMBlockingX);
	return chain_reply(inbuf,outbuf,length,bufsize);
}

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_ALL

/****************************************************************************
 Reply to a SMBreadbmpx (read block multiplex) request.
****************************************************************************/

int reply_readbmpx(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
	ssize_t nread = -1;
	ssize_t total_read;
	char *data;
	SMB_OFF_T startpos;
	int outsize;
	size_t maxcount;
	int max_per_packet;
	size_t tcount;
	int pad;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	START_PROFILE(SMBreadBmpx);

	/* this function doesn't seem to work - disable by default */
	if (!lp_readbmpx()) {
		END_PROFILE(SMBreadBmpx);
		return ERROR_DOS(ERRSRV,ERRuseSTD);
	}

	outsize = set_message(outbuf,8,0,True);

	CHECK_FSP(fsp,conn);
	if (!CHECK_READ(fsp,inbuf)) {
		return(ERROR_DOS(ERRDOS,ERRbadaccess));
	}

	startpos = IVAL_TO_SMB_OFF_T(inbuf,smb_vwv1);
	maxcount = SVAL(inbuf,smb_vwv3);

	data = smb_buf(outbuf);
	pad = ((long)data)%4;
	if (pad)
		pad = 4 - pad;
	data += pad;

	max_per_packet = bufsize-(outsize+pad);
	tcount = maxcount;
	total_read = 0;

	if (is_locked(fsp,(uint32)SVAL(inbuf,smb_pid),(SMB_BIG_UINT)maxcount,(SMB_BIG_UINT)startpos, READ_LOCK)) {
		END_PROFILE(SMBreadBmpx);
		return ERROR_DOS(ERRDOS,ERRlock);
	}

	do {
		size_t N = MIN(max_per_packet,tcount-total_read);
  
		nread = read_file(fsp,data,startpos,N);

		if (nread <= 0)
			nread = 0;

		if (nread < (ssize_t)N)
			tcount = total_read + nread;

		set_message(outbuf,8,nread+pad,False);
		SIVAL(outbuf,smb_vwv0,startpos);
		SSVAL(outbuf,smb_vwv2,tcount);
		SSVAL(outbuf,smb_vwv6,nread);
		SSVAL(outbuf,smb_vwv7,smb_offset(data,outbuf));

		show_msg(outbuf);
		if (!send_smb(smbd_server_fd(),outbuf))
			exit_server_cleanly("reply_readbmpx: send_smb failed.");

		total_read += nread;
		startpos += nread;
	} while (total_read < (ssize_t)tcount);

	END_PROFILE(SMBreadBmpx);
	return(-1);
}

/****************************************************************************
 Reply to a SMBsetattrE.
****************************************************************************/

int reply_setattrE(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
	struct timespec ts[2];
	int outsize = 0;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	START_PROFILE(SMBsetattrE);

	outsize = set_message(outbuf,0,0,False);

	if(!fsp || (fsp->conn != conn)) {
		END_PROFILE(SMBsetattrE);
		return ERROR_DOS(ERRDOS,ERRbadfid);
	}

	/*
	 * Convert the DOS times into unix times. Ignore create
	 * time as UNIX can't set this.
	 */

	ts[0] = convert_time_t_to_timespec(srv_make_unix_date2(inbuf+smb_vwv3)); /* atime. */
	ts[1] = convert_time_t_to_timespec(srv_make_unix_date2(inbuf+smb_vwv5)); /* mtime. */
  
	/* 
	 * Patch from Ray Frush <frush@engr.colostate.edu>
	 * Sometimes times are sent as zero - ignore them.
	 */

	if (null_timespec(ts[0]) && null_timespec(ts[1])) {
		/* Ignore request */
		if( DEBUGLVL( 3 ) ) {
			dbgtext( "reply_setattrE fnum=%d ", fsp->fnum);
			dbgtext( "ignoring zero request - not setting timestamps of 0\n" );
		}
		END_PROFILE(SMBsetattrE);
		return(outsize);
	} else if (!null_timespec(ts[0]) && null_timespec(ts[1])) {
		/* set modify time = to access time if modify time was unset */
		ts[1] = ts[0];
	}

	/* Set the date on this file */
	/* Should we set pending modtime here ? JRA */
	if(file_ntimes(conn, fsp->fsp_name, ts)) {
		END_PROFILE(SMBsetattrE);
		return ERROR_DOS(ERRDOS,ERRnoaccess);
	}
  
	DEBUG( 3, ( "reply_setattrE fnum=%d actime=%u modtime=%u\n",
		fsp->fnum,
		(unsigned int)ts[0].tv_sec,
		(unsigned int)ts[1].tv_sec));

	END_PROFILE(SMBsetattrE);
	return(outsize);
}


/* Back from the dead for OS/2..... JRA. */

/****************************************************************************
 Reply to a SMBwritebmpx (write block multiplex primary) request.
****************************************************************************/

int reply_writebmpx(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
	size_t numtowrite;
	ssize_t nwritten = -1;
	int outsize = 0;
	SMB_OFF_T startpos;
	size_t tcount;
	BOOL write_through;
	int smb_doff;
	char *data;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	NTSTATUS status;
	START_PROFILE(SMBwriteBmpx);

	CHECK_FSP(fsp,conn);
	if (!CHECK_WRITE(fsp)) {
		return(ERROR_DOS(ERRDOS,ERRbadaccess));
	}
	if (HAS_CACHED_ERROR(fsp)) {
		return(CACHED_ERROR(fsp));
	}

	tcount = SVAL(inbuf,smb_vwv1);
	startpos = IVAL_TO_SMB_OFF_T(inbuf,smb_vwv3);
	write_through = BITSETW(inbuf+smb_vwv7,0);
	numtowrite = SVAL(inbuf,smb_vwv10);
	smb_doff = SVAL(inbuf,smb_vwv11);

	data = smb_base(inbuf) + smb_doff;

	/* If this fails we need to send an SMBwriteC response,
		not an SMBwritebmpx - set this up now so we don't forget */
	SCVAL(outbuf,smb_com,SMBwritec);

	if (is_locked(fsp,(uint32)SVAL(inbuf,smb_pid),(SMB_BIG_UINT)tcount,(SMB_BIG_UINT)startpos,WRITE_LOCK)) {
		END_PROFILE(SMBwriteBmpx);
		return(ERROR_DOS(ERRDOS,ERRlock));
	}

	nwritten = write_file(fsp,data,startpos,numtowrite);

	status = sync_file(conn, fsp, write_through);
	if (!NT_STATUS_IS_OK(status)) {
		END_PROFILE(SMBwriteBmpx);
		DEBUG(5,("reply_writebmpx: sync_file for %s returned %s\n",
			fsp->fsp_name, nt_errstr(status) ));
		return ERROR_NT(status);
	}
  
	if(nwritten < (ssize_t)numtowrite) {
		END_PROFILE(SMBwriteBmpx);
		return(UNIXERROR(ERRHRD,ERRdiskfull));
	}

	/* If the maximum to be written to this file
		is greater than what we just wrote then set
		up a secondary struct to be attached to this
		fd, we will use this to cache error messages etc. */

	if((ssize_t)tcount > nwritten) {
		write_bmpx_struct *wbms;
		if(fsp->wbmpx_ptr != NULL)
			wbms = fsp->wbmpx_ptr; /* Use an existing struct */
		else
			wbms = SMB_MALLOC_P(write_bmpx_struct);
		if(!wbms) {
			DEBUG(0,("Out of memory in reply_readmpx\n"));
			END_PROFILE(SMBwriteBmpx);
			return(ERROR_DOS(ERRSRV,ERRnoresource));
		}
		wbms->wr_mode = write_through;
		wbms->wr_discard = False; /* No errors yet */
		wbms->wr_total_written = nwritten;
		wbms->wr_errclass = 0;
		wbms->wr_error = 0;
		fsp->wbmpx_ptr = wbms;
	}

	/* We are returning successfully, set the message type back to
		SMBwritebmpx */
	SCVAL(outbuf,smb_com,SMBwriteBmpx);
  
	outsize = set_message(outbuf,1,0,True);
  
	SSVALS(outbuf,smb_vwv0,-1); /* We don't support smb_remaining */
  
	DEBUG( 3, ( "writebmpx fnum=%d num=%d wrote=%d\n",
			fsp->fnum, (int)numtowrite, (int)nwritten ) );

	if (write_through && tcount==nwritten) {
		/* We need to send both a primary and a secondary response */
		smb_setlen(outbuf,outsize - 4);
		show_msg(outbuf);
		if (!send_smb(smbd_server_fd(),outbuf))
			exit_server_cleanly("reply_writebmpx: send_smb failed.");

		/* Now the secondary */
		outsize = set_message(outbuf,1,0,True);
		SCVAL(outbuf,smb_com,SMBwritec);
		SSVAL(outbuf,smb_vwv0,nwritten);
	}

	END_PROFILE(SMBwriteBmpx);
	return(outsize);
}

/****************************************************************************
 Reply to a SMBwritebs (write block multiplex secondary) request.
****************************************************************************/

int reply_writebs(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	size_t numtowrite;
	ssize_t nwritten = -1;
	int outsize = 0;
	SMB_OFF_T startpos;
	size_t tcount;
	BOOL write_through;
	int smb_doff;
	char *data;
	write_bmpx_struct *wbms;
	BOOL send_response = False; 
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	NTSTATUS status;
	START_PROFILE(SMBwriteBs);

	CHECK_FSP(fsp,conn);
	if (!CHECK_WRITE(fsp)) {
		return(ERROR_DOS(ERRDOS,ERRbadaccess));
	}

	tcount = SVAL(inbuf,smb_vwv1);
	startpos = IVAL_TO_SMB_OFF_T(inbuf,smb_vwv2);
	numtowrite = SVAL(inbuf,smb_vwv6);
	smb_doff = SVAL(inbuf,smb_vwv7);

	data = smb_base(inbuf) + smb_doff;

	/* We need to send an SMBwriteC response, not an SMBwritebs */
	SCVAL(outbuf,smb_com,SMBwritec);

	/* This fd should have an auxiliary struct attached,
		check that it does */
	wbms = fsp->wbmpx_ptr;
	if(!wbms) {
		END_PROFILE(SMBwriteBs);
		return(-1);
	}

	/* If write through is set we can return errors, else we must cache them */
	write_through = wbms->wr_mode;

	/* Check for an earlier error */
	if(wbms->wr_discard) {
		END_PROFILE(SMBwriteBs);
		return -1; /* Just discard the packet */
	}

	nwritten = write_file(fsp,data,startpos,numtowrite);

	status = sync_file(conn, fsp, write_through);
  
	if (nwritten < (ssize_t)numtowrite || !NT_STATUS_IS_OK(status)) {
		if(write_through) {
			/* We are returning an error - we can delete the aux struct */
			if (wbms)
				free((char *)wbms);
			fsp->wbmpx_ptr = NULL;
			END_PROFILE(SMBwriteBs);
			return(ERROR_DOS(ERRHRD,ERRdiskfull));
		}
		wbms->wr_errclass = ERRHRD;
		wbms->wr_error = ERRdiskfull;
		wbms->wr_status = NT_STATUS_DISK_FULL;
		wbms->wr_discard = True;
		END_PROFILE(SMBwriteBs);
		return -1;
	}

	/* Increment the total written, if this matches tcount
		we can discard the auxiliary struct (hurrah !) and return a writeC */
	wbms->wr_total_written += nwritten;
	if(wbms->wr_total_written >= tcount) {
		if (write_through) {
			outsize = set_message(outbuf,1,0,True);
			SSVAL(outbuf,smb_vwv0,wbms->wr_total_written);    
			send_response = True;
		}

		free((char *)wbms);
		fsp->wbmpx_ptr = NULL;
	}

	if(send_response) {
		END_PROFILE(SMBwriteBs);
		return(outsize);
	}

	END_PROFILE(SMBwriteBs);
	return(-1);
}

/****************************************************************************
 Reply to a SMBgetattrE.
****************************************************************************/

int reply_getattrE(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
	SMB_STRUCT_STAT sbuf;
	int outsize = 0;
	int mode;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	START_PROFILE(SMBgetattrE);

	outsize = set_message(outbuf,11,0,True);

	if(!fsp || (fsp->conn != conn)) {
		END_PROFILE(SMBgetattrE);
		return ERROR_DOS(ERRDOS,ERRbadfid);
	}

	/* Do an fstat on this file */
	if(fsp_stat(fsp, &sbuf)) {
		END_PROFILE(SMBgetattrE);
		return(UNIXERROR(ERRDOS,ERRnoaccess));
	}
  
	mode = dos_mode(conn,fsp->fsp_name,&sbuf);
  
	/*
	 * Convert the times into dos times. Set create
	 * date to be last modify date as UNIX doesn't save
	 * this.
	 */

	srv_put_dos_date2(outbuf,smb_vwv0,get_create_time(&sbuf,lp_fake_dir_create_times(SNUM(conn))));
	srv_put_dos_date2(outbuf,smb_vwv2,sbuf.st_atime);
	/* Should we check pending modtime here ? JRA */
	srv_put_dos_date2(outbuf,smb_vwv4,sbuf.st_mtime);

	if (mode & aDIR) {
		SIVAL(outbuf,smb_vwv6,0);
		SIVAL(outbuf,smb_vwv8,0);
	} else {
		uint32 allocation_size = get_allocation_size(conn,fsp, &sbuf);
		SIVAL(outbuf,smb_vwv6,(uint32)sbuf.st_size);
		SIVAL(outbuf,smb_vwv8,allocation_size);
	}
	SSVAL(outbuf,smb_vwv10, mode);
  
	DEBUG( 3, ( "reply_getattrE fnum=%d\n", fsp->fnum));
  
	END_PROFILE(SMBgetattrE);
	return(outsize);
}

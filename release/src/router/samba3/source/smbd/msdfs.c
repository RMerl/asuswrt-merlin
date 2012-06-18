/* 
   Unix SMB/Netbios implementation.
   Version 3.0
   MSDFS services for Samba
   Copyright (C) Shirish Kalele 2000
   Copyright (C) Jeremy Allison 2007

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

#define DBGC_CLASS DBGC_MSDFS
#include "includes.h"

extern uint32 global_client_caps;

/**********************************************************************
 Parse a DFS pathname of the form \hostname\service\reqpath
 into the dfs_path structure.
 If POSIX pathnames is true, the pathname may also be of the
 form /hostname/service/reqpath.
 We cope with either here.

 If conn != NULL then ensure the provided service is
 the one pointed to by the connection.

 Unfortunately, due to broken clients who might set the
 SVAL(inbuf,smb_flg2) & FLAGS2_DFS_PATHNAMES bit and then
 send a local path, we have to cope with that too....

 JRA.
**********************************************************************/

static NTSTATUS parse_dfs_path(connection_struct *conn,
				const char *pathname,
				BOOL allow_wcards,
				struct dfs_path *pdp,
				BOOL *ppath_contains_wcard)
{
	pstring pathname_local;
	char *p,*temp, *servicename;
	NTSTATUS status = NT_STATUS_OK;
	char sepchar;

	ZERO_STRUCTP(pdp);

	pstrcpy(pathname_local,pathname);
	p = temp = pathname_local;

	pdp->posix_path = (lp_posix_pathnames() && *pathname == '/');

	sepchar = pdp->posix_path ? '/' : '\\';

	if (*pathname != sepchar) {
		DEBUG(10,("parse_dfs_path: path %s doesn't start with %c\n",
			pathname, sepchar ));
		/*
		 * Possibly client sent a local path by mistake.
		 * Try and convert to a local path.
		 */

		pdp->hostname[0] = '\0';
		pdp->servicename[0] = '\0';

		/* We've got no info about separators. */
		pdp->posix_path = lp_posix_pathnames();
		p = temp;
		DEBUG(10,("parse_dfs_path: trying to convert %s to a local path\n",
			temp));
		goto local_path;
	}

	trim_char(temp,sepchar,sepchar);

	DEBUG(10,("parse_dfs_path: temp = |%s| after trimming %c's\n",
		temp, sepchar));

	/* Now tokenize. */
	/* Parse out hostname. */
	p = strchr_m(temp,sepchar);
	if(p == NULL) {
		DEBUG(10,("parse_dfs_path: can't parse hostname from path %s\n",
			temp));
		/*
		 * Possibly client sent a local path by mistake.
		 * Try and convert to a local path.
		 */

		pdp->hostname[0] = '\0';
		pdp->servicename[0] = '\0';

		p = temp;
		DEBUG(10,("parse_dfs_path: trying to convert %s to a local path\n",
			temp));
		goto local_path;
	}
	*p = '\0';
	fstrcpy(pdp->hostname,temp);
	DEBUG(10,("parse_dfs_path: hostname: %s\n",pdp->hostname));

	/* Parse out servicename. */
	servicename = p+1;
	p = strchr_m(servicename,sepchar);
	if (p) {
		*p = '\0';
	}

	/* Is this really our servicename ? */
        if (conn && !( strequal(servicename, lp_servicename(SNUM(conn)))
			|| (strequal(servicename, HOMES_NAME)
			&& strequal(lp_servicename(SNUM(conn)),
				get_current_username()) )) ) {

		DEBUG(10,("parse_dfs_path: %s is not our servicename\n",
			servicename));

		/*
		 * Possibly client sent a local path by mistake.
		 * Try and convert to a local path.
		 */

		pdp->hostname[0] = '\0';
		pdp->servicename[0] = '\0';

		/* Repair the path - replace the sepchar's
		   we nulled out */
		servicename--;
		*servicename = sepchar;
		if (p) {
			*p = sepchar;
		}

		p = temp;
		DEBUG(10,("parse_dfs_path: trying to convert %s "
			"to a local path\n",
			temp));
		goto local_path;
	}

	fstrcpy(pdp->servicename,servicename);

	DEBUG(10,("parse_dfs_path: servicename: %s\n",pdp->servicename));

	if(p == NULL) {
		pdp->reqpath[0] = '\0';
		return NT_STATUS_OK;
	}
	p++;

  local_path:

	*ppath_contains_wcard = False;

	/* Rest is reqpath. */
	if (pdp->posix_path) {
		status = check_path_syntax_posix(pdp->reqpath, p);
	} else {
		if (allow_wcards) {
			status = check_path_syntax_wcard(pdp->reqpath, p, ppath_contains_wcard);
		} else {
			status = check_path_syntax(pdp->reqpath, p);
		}
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("parse_dfs_path: '%s' failed with %s\n",
			p, nt_errstr(status) ));
		return status;
	}

	DEBUG(10,("parse_dfs_path: rest of the path: %s\n",pdp->reqpath));
	return NT_STATUS_OK;
}

/********************************************************
 Fake up a connection struct for the VFS layer.
 Note this CHANGES CWD !!!! JRA.
*********************************************************/

static NTSTATUS create_conn_struct(connection_struct *conn, int snum, const char *path)
{
	pstring connpath;

	ZERO_STRUCTP(conn);

	pstrcpy(connpath, path);
	pstring_sub(connpath , "%S", lp_servicename(snum));

	/* needed for smbd_vfs_init() */

	if ((conn->mem_ctx=talloc_init("connection_struct")) == NULL) {
		DEBUG(0,("talloc_init(connection_struct) failed!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if (!(conn->params = TALLOC_ZERO_P(conn->mem_ctx, struct share_params))) {
		DEBUG(0, ("TALLOC failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	conn->params->service = snum;

	set_conn_connectpath(conn, connpath);

	if (!smbd_vfs_init(conn)) {
		NTSTATUS status = map_nt_error_from_unix(errno);
		DEBUG(0,("create_conn_struct: smbd_vfs_init failed.\n"));
		conn_free_internal(conn);
		return status;
	}

	/*
	 * Windows seems to insist on doing trans2getdfsreferral() calls on the IPC$
	 * share as the anonymous user. If we try to chdir as that user we will
	 * fail.... WTF ? JRA.
	 */

	if (vfs_ChDir(conn,conn->connectpath) != 0) {
		NTSTATUS status = map_nt_error_from_unix(errno);
		DEBUG(3,("create_conn_struct: Can't ChDir to new conn path %s. Error was %s\n",
					conn->connectpath, strerror(errno) ));
		conn_free_internal(conn);
		return status;
	}

	return NT_STATUS_OK;
}

/**********************************************************************
 Parse the contents of a symlink to verify if it is an msdfs referral
 A valid referral is of the form:

 msdfs:server1\share1,server2\share2
 msdfs:server1\share1\pathname,server2\share2\pathname
 msdfs:server1/share1,server2/share2
 msdfs:server1/share1/pathname,server2/share2/pathname.

 Note that the alternate paths returned here must be of the canonicalized
 form:

 \server\share or
 \server\share\path\to\file,

 even in posix path mode. This is because we have no knowledge if the
 server we're referring to understands posix paths.
 **********************************************************************/

static BOOL parse_msdfs_symlink(TALLOC_CTX *ctx,
				char *target,
				struct referral **preflist,
				int *refcount)
{
	pstring temp;
	char *prot;
	char *alt_path[MAX_REFERRAL_COUNT];
	int count = 0, i;
	struct referral *reflist;

	pstrcpy(temp,target);
	prot = strtok(temp,":");
	if (!prot) {
		DEBUG(0,("parse_msdfs_symlink: invalid path !\n"));
		return False;
	}

	/* parse out the alternate paths */
	while((count<MAX_REFERRAL_COUNT) &&
	      ((alt_path[count] = strtok(NULL,",")) != NULL)) {
		count++;
	}

	DEBUG(10,("parse_msdfs_symlink: count=%d\n", count));

	if (count) {
		reflist = *preflist = TALLOC_ZERO_ARRAY(ctx, struct referral, count);
		if(reflist == NULL) {
			DEBUG(0,("parse_msdfs_symlink: talloc failed!\n"));
			return False;
		}
	} else {
		reflist = *preflist = NULL;
	}
	
	for(i=0;i<count;i++) {
		char *p;

		/* Canonicalize link target. Replace all /'s in the path by a \ */
		string_replace(alt_path[i], '/', '\\');

		/* Remove leading '\\'s */
		p = alt_path[i];
		while (*p && (*p == '\\')) {
			p++;
		}

		pstrcpy(reflist[i].alternate_path, "\\");
		pstrcat(reflist[i].alternate_path, p);

		reflist[i].proximity = 0;
		reflist[i].ttl = REFERRAL_TTL;
		DEBUG(10, ("parse_msdfs_symlink: Created alt path: %s\n", reflist[i].alternate_path));
		*refcount += 1;
	}

	return True;
}
 
/**********************************************************************
 Returns true if the unix path is a valid msdfs symlink and also
 returns the target string from inside the link.
**********************************************************************/

BOOL is_msdfs_link(connection_struct *conn,
			const char *path,
			pstring link_target,
			SMB_STRUCT_STAT *sbufp)
{
	SMB_STRUCT_STAT st;
	int referral_len = 0;

	if (sbufp == NULL) {
		sbufp = &st;
	}

	if (SMB_VFS_LSTAT(conn, path, sbufp) != 0) {
		DEBUG(5,("is_msdfs_link: %s does not exist.\n",path));
		return False;
	}
  
	if (!S_ISLNK(sbufp->st_mode)) {
		DEBUG(5,("is_msdfs_link: %s is not a link.\n",path));
		return False;
	}

	/* open the link and read it */
	referral_len = SMB_VFS_READLINK(conn, path, link_target, sizeof(pstring)-1);
	if (referral_len == -1) {
		DEBUG(0,("is_msdfs_link: Error reading msdfs link %s: %s\n",
			path, strerror(errno)));
		return False;
	}
	link_target[referral_len] = '\0';

	DEBUG(5,("is_msdfs_link: %s -> %s\n",path, link_target));

	if (!strnequal(link_target, "msdfs:", 6)) {
		return False;
	}
	return True;
}

/*****************************************************************
 Used by other functions to decide if a dfs path is remote,
 and to get the list of referred locations for that remote path.
 
 search_flag: For findfirsts, dfs links themselves are not
 redirected, but paths beyond the links are. For normal smb calls,
 even dfs links need to be redirected.

 consumedcntp: how much of the dfs path is being redirected. the client
 should try the remaining path on the redirected server.

 If this returns NT_STATUS_PATH_NOT_COVERED the contents of the msdfs
 link redirect are in targetpath.
*****************************************************************/

static NTSTATUS dfs_path_lookup(connection_struct *conn,
			const char *dfspath, /* Incoming complete dfs path */
			const struct dfs_path *pdp, /* Parsed out server+share+extrapath. */
			BOOL search_flag, /* Called from a findfirst ? */
			int *consumedcntp,
			pstring targetpath)
{
	char *p = NULL;
	char *q = NULL;
	SMB_STRUCT_STAT sbuf;
	NTSTATUS status;
	pstring localpath;
	pstring canon_dfspath; /* Canonicalized dfs path. (only '/' components). */

	DEBUG(10,("dfs_path_lookup: Conn path = %s reqpath = %s\n",
		conn->connectpath, pdp->reqpath));

	/* 
 	 * Note the unix path conversion here we're doing we can
	 * throw away. We're looking for a symlink for a dfs
	 * resolution, if we don't find it we'll do another
	 * unix_convert later in the codepath.
	 * If we needed to remember what we'd resolved in
	 * dp->reqpath (as the original code did) we'd
	 * pstrcpy(localhost, dp->reqpath) on any code
	 * path below that returns True - but I don't
	 * think this is needed. JRA.
	 */

	pstrcpy(localpath, pdp->reqpath);
	status = unix_convert(conn, localpath, search_flag, NULL, &sbuf);
	if (!NT_STATUS_IS_OK(status) && !NT_STATUS_EQUAL(status,
					NT_STATUS_OBJECT_PATH_NOT_FOUND)) {
		return status;
	}

	/* Optimization - check if we can redirect the whole path. */

	if (is_msdfs_link(conn, localpath, targetpath, NULL)) {
		if (search_flag) {
			DEBUG(6,("dfs_path_lookup (FindFirst) No redirection "
				 "for dfs link %s.\n", dfspath));
			return NT_STATUS_OK;
		}

		DEBUG(6,("dfs_path_lookup: %s resolves to a "
			"valid dfs link %s.\n", dfspath, targetpath));

		if (consumedcntp) {
			*consumedcntp = strlen(dfspath);
		}
		return NT_STATUS_PATH_NOT_COVERED;
	}

	/* Prepare to test only for '/' components in the given path,
	 * so if a Windows path replace all '\\' characters with '/'.
	 * For a POSIX DFS path we know all separators are already '/'. */

	pstrcpy(canon_dfspath, dfspath);
	if (!pdp->posix_path) {
		string_replace(canon_dfspath, '\\', '/');
	}

	/*
	 * localpath comes out of unix_convert, so it has
	 * no trailing backslash. Make sure that canon_dfspath hasn't either.
	 * Fix for bug #4860 from Jan Martin <Jan.Martin@rwedea.com>.
	 */

	trim_char(canon_dfspath,0,'/');

	/*
	 * Redirect if any component in the path is a link.
	 * We do this by walking backwards through the 
	 * local path, chopping off the last component
	 * in both the local path and the canonicalized
	 * DFS path. If we hit a DFS link then we're done.
	 */

	p = strrchr_m(localpath, '/');
	if (consumedcntp) {
		q = strrchr_m(canon_dfspath, '/');
	}

	while (p) {
		*p = '\0';
		if (q) {
			*q = '\0';
		}

		if (is_msdfs_link(conn, localpath, targetpath, NULL)) {
			DEBUG(4, ("dfs_path_lookup: Redirecting %s because "
				"parent %s is dfs link\n", dfspath, localpath));

			if (consumedcntp) {
				*consumedcntp = strlen(canon_dfspath);
				DEBUG(10, ("dfs_path_lookup: Path consumed: %s "
					"(%d)\n", canon_dfspath, *consumedcntp));
			}

			return NT_STATUS_PATH_NOT_COVERED;
		}

		/* Step back on the filesystem. */
		p = strrchr_m(localpath, '/');

		if (consumedcntp) {
			/* And in the canonicalized dfs path. */
			q = strrchr_m(canon_dfspath, '/');
		}
	}

	return NT_STATUS_OK;
}

/*****************************************************************
 Decides if a dfs pathname should be redirected or not.
 If not, the pathname is converted to a tcon-relative local unix path

 search_wcard_flag: this flag performs 2 functions bother related
 to searches.  See resolve_dfs_path() and parse_dfs_path_XX()
 for details.

 This function can return NT_STATUS_OK, meaning use the returned path as-is
 (mapped into a local path).
 or NT_STATUS_NOT_COVERED meaning return a DFS redirect, or
 any other NT_STATUS error which is a genuine error to be
 returned to the client. 
*****************************************************************/

static NTSTATUS dfs_redirect( connection_struct *conn,
			pstring dfs_path,
			BOOL search_wcard_flag,
			BOOL *ppath_contains_wcard)
{
	NTSTATUS status;
	struct dfs_path dp;
	pstring targetpath;
	
	status = parse_dfs_path(conn, dfs_path, search_wcard_flag, &dp, ppath_contains_wcard);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (dp.reqpath[0] == '\0') {
		pstrcpy(dfs_path, dp.reqpath);
		DEBUG(5,("dfs_redirect: self-referral.\n"));
		return NT_STATUS_OK;
	}

	/* If dfs pathname for a non-dfs share, convert to tcon-relative
	   path and return OK */

	if (!lp_msdfs_root(SNUM(conn))) {
		pstrcpy(dfs_path, dp.reqpath);
		return NT_STATUS_OK;
	}

	/* If it looked like a local path (zero hostname/servicename)
	 * just treat as a tcon-relative path. */ 

	if (dp.hostname[0] == '\0' && dp.servicename[0] == '\0') { 
		pstrcpy(dfs_path, dp.reqpath);
		return NT_STATUS_OK;
	}

	status = dfs_path_lookup(conn, dfs_path, &dp,
			search_wcard_flag, NULL, targetpath);
	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status, NT_STATUS_PATH_NOT_COVERED)) {
			DEBUG(3,("dfs_redirect: Redirecting %s\n", dfs_path));
		} else {
			DEBUG(10,("dfs_redirect: dfs_path_lookup failed for %s with %s\n",
				dfs_path, nt_errstr(status) ));
		}
		return status;
	}

	DEBUG(3,("dfs_redirect: Not redirecting %s.\n", dfs_path));

	/* Form non-dfs tcon-relative path */
	pstrcpy(dfs_path, dp.reqpath);

	DEBUG(3,("dfs_redirect: Path converted to non-dfs path %s\n", dfs_path));
	return NT_STATUS_OK;
}

/**********************************************************************
 Return a self referral.
**********************************************************************/

static NTSTATUS self_ref(TALLOC_CTX *ctx,
			const char *dfs_path,
			struct junction_map *jucn,
			int *consumedcntp,
			BOOL *self_referralp)
{
	struct referral *ref;

	*self_referralp = True;

	jucn->referral_count = 1;
	if((ref = TALLOC_ZERO_P(ctx, struct referral)) == NULL) {
		DEBUG(0,("self_ref: talloc failed for referral\n"));
		return NT_STATUS_NO_MEMORY;
	}

	pstrcpy(ref->alternate_path,dfs_path);
	ref->proximity = 0;
	ref->ttl = REFERRAL_TTL;
	jucn->referral_list = ref;
	*consumedcntp = strlen(dfs_path);
	return NT_STATUS_OK;
}

/**********************************************************************
 Gets valid referrals for a dfs path and fills up the
 junction_map structure.
**********************************************************************/

NTSTATUS get_referred_path(TALLOC_CTX *ctx,
			const char *dfs_path,
			struct junction_map *jucn,
			int *consumedcntp,
			BOOL *self_referralp)
{
	struct connection_struct conns;
	struct connection_struct *conn = &conns;
	struct dfs_path dp;
	pstring conn_path;
	pstring targetpath;
	int snum;
	NTSTATUS status = NT_STATUS_NOT_FOUND;
	BOOL dummy;

	ZERO_STRUCT(conns);

	*self_referralp = False;

	status = parse_dfs_path(NULL, dfs_path, False, &dp, &dummy);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	fstrcpy(jucn->service_name, dp.servicename);
	pstrcpy(jucn->volume_name, dp.reqpath);

	/* Verify the share is a dfs root */
	snum = lp_servicenumber(jucn->service_name);
	if(snum < 0) {
		if ((snum = find_service(jucn->service_name)) < 0) {
			return NT_STATUS_NOT_FOUND;
		}
	}

	if (!lp_msdfs_root(snum)) {
		DEBUG(3,("get_referred_path: |%s| in dfs path %s is not a dfs root.\n",
			 dp.servicename, dfs_path));
		return NT_STATUS_NOT_FOUND;
	}

	/*
	 * Self referrals are tested with a anonymous IPC connection and
	 * a GET_DFS_REFERRAL call to \\server\share. (which means dp.reqpath[0] points
	 * to an empty string). create_conn_struct cd's into the directory and will
	 * fail if it cannot (as the anonymous user). Cope with this.
	 */

	if (dp.reqpath[0] == '\0') {
		struct referral *ref;

		if (*lp_msdfs_proxy(snum) == '\0') {
			return self_ref(ctx,
					dfs_path,
					jucn,
					consumedcntp,
					self_referralp);
		}

		/* 
		 * It's an msdfs proxy share. Redirect to
 		 * the configured target share.
 		 */

		jucn->referral_count = 1;
		if ((ref = TALLOC_ZERO_P(ctx, struct referral)) == NULL) {
			DEBUG(0, ("malloc failed for referral\n"));
			return NT_STATUS_NO_MEMORY;
		}

		pstrcpy(ref->alternate_path, lp_msdfs_proxy(snum));
		if (dp.reqpath[0] != '\0') {
			pstrcat(ref->alternate_path, dp.reqpath);
		}
		ref->proximity = 0;
		ref->ttl = REFERRAL_TTL;
		jucn->referral_list = ref;
		*consumedcntp = strlen(dfs_path);
		return NT_STATUS_OK;
	}

	pstrcpy(conn_path, lp_pathname(snum));
	status = create_conn_struct(conn, snum, conn_path);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* If this is a DFS path dfs_lookup should return
	 * NT_STATUS_PATH_NOT_COVERED. */

	status = dfs_path_lookup(conn, dfs_path, &dp,
			False, consumedcntp, targetpath);

	if (!NT_STATUS_EQUAL(status, NT_STATUS_PATH_NOT_COVERED)) {
		DEBUG(3,("get_referred_path: No valid referrals for path %s\n",
			dfs_path));
		conn_free_internal(conn);
		return status;
	}

	/* We know this is a valid dfs link. Parse the targetpath. */
	if (!parse_msdfs_symlink(ctx, targetpath,
				&jucn->referral_list,
				&jucn->referral_count)) {
		DEBUG(3,("get_referred_path: failed to parse symlink "
			"target %s\n", targetpath ));
		conn_free_internal(conn);
		return NT_STATUS_NOT_FOUND;
	}

	conn_free_internal(conn);
	return NT_STATUS_OK;
}

static int setup_ver2_dfs_referral(const char *pathname,
				char **ppdata, 
				struct junction_map *junction,
				BOOL self_referral)
{
	char* pdata = *ppdata;

	unsigned char uni_requestedpath[1024];
	int uni_reqpathoffset1,uni_reqpathoffset2;
	int uni_curroffset;
	int requestedpathlen=0;
	int offset;
	int reply_size = 0;
	int i=0;

	DEBUG(10,("Setting up version2 referral\nRequested path:\n"));

	requestedpathlen = rpcstr_push(uni_requestedpath, pathname, sizeof(pstring),
				       STR_TERMINATE);

	if (DEBUGLVL(10)) {
	    dump_data(0, (const char *) uni_requestedpath,requestedpathlen);
	}

	DEBUG(10,("ref count = %u\n",junction->referral_count));

	uni_reqpathoffset1 = REFERRAL_HEADER_SIZE + 
			VERSION2_REFERRAL_SIZE * junction->referral_count;

	uni_reqpathoffset2 = uni_reqpathoffset1 + requestedpathlen;

	uni_curroffset = uni_reqpathoffset2 + requestedpathlen;

	reply_size = REFERRAL_HEADER_SIZE + VERSION2_REFERRAL_SIZE*junction->referral_count +
					2 * requestedpathlen;
	DEBUG(10,("reply_size: %u\n",reply_size));

	/* add up the unicode lengths of all the referral paths */
	for(i=0;i<junction->referral_count;i++) {
		DEBUG(10,("referral %u : %s\n",i,junction->referral_list[i].alternate_path));
		reply_size += (strlen(junction->referral_list[i].alternate_path)+1)*2;
	}

	DEBUG(10,("reply_size = %u\n",reply_size));
	/* add the unexplained 0x16 bytes */
	reply_size += 0x16;

	pdata = (char *)SMB_REALLOC(pdata,reply_size);
	if(pdata == NULL) {
		DEBUG(0,("Realloc failed!\n"));
		return -1;
	}
	*ppdata = pdata;

	/* copy in the dfs requested paths.. required for offset calculations */
	memcpy(pdata+uni_reqpathoffset1,uni_requestedpath,requestedpathlen);
	memcpy(pdata+uni_reqpathoffset2,uni_requestedpath,requestedpathlen);

	/* create the header */
	SSVAL(pdata,0,requestedpathlen - 2); /* UCS2 of path consumed minus
						2 byte null */
	SSVAL(pdata,2,junction->referral_count); /* number of referral in this pkt */
	if(self_referral) {
		SIVAL(pdata,4,DFSREF_REFERRAL_SERVER | DFSREF_STORAGE_SERVER); 
	} else {
		SIVAL(pdata,4,DFSREF_STORAGE_SERVER);
	}

	offset = 8;
	/* add the referral elements */
	for(i=0;i<junction->referral_count;i++) {
		struct referral* ref = &junction->referral_list[i];
		int unilen;

		SSVAL(pdata,offset,2); /* version 2 */
		SSVAL(pdata,offset+2,VERSION2_REFERRAL_SIZE);
		if(self_referral) {
			SSVAL(pdata,offset+4,1);
		} else {
			SSVAL(pdata,offset+4,0);
		}
		SSVAL(pdata,offset+6,0); /* ref_flags :use path_consumed bytes? */
		SIVAL(pdata,offset+8,ref->proximity);
		SIVAL(pdata,offset+12,ref->ttl);

		SSVAL(pdata,offset+16,uni_reqpathoffset1-offset);
		SSVAL(pdata,offset+18,uni_reqpathoffset2-offset);
		/* copy referred path into current offset */
		unilen = rpcstr_push(pdata+uni_curroffset, ref->alternate_path,
				     sizeof(pstring), STR_UNICODE);

		SSVAL(pdata,offset+20,uni_curroffset-offset);

		uni_curroffset += unilen;
		offset += VERSION2_REFERRAL_SIZE;
	}
	/* add in the unexplained 22 (0x16) bytes at the end */
	memset(pdata+uni_curroffset,'\0',0x16);
	return reply_size;
}

static int setup_ver3_dfs_referral(const char *pathname,
				char **ppdata, 
				struct junction_map *junction,
				BOOL self_referral)
{
	char* pdata = *ppdata;

	unsigned char uni_reqpath[1024];
	int uni_reqpathoffset1, uni_reqpathoffset2;
	int uni_curroffset;
	int reply_size = 0;

	int reqpathlen = 0;
	int offset,i=0;
	
	DEBUG(10,("setting up version3 referral\n"));

	reqpathlen = rpcstr_push(uni_reqpath, pathname, sizeof(pstring), STR_TERMINATE);
	
	if (DEBUGLVL(10)) {
	    dump_data(0, (char *) uni_reqpath,reqpathlen);
	}

	uni_reqpathoffset1 = REFERRAL_HEADER_SIZE + VERSION3_REFERRAL_SIZE * junction->referral_count;
	uni_reqpathoffset2 = uni_reqpathoffset1 + reqpathlen;
	reply_size = uni_curroffset = uni_reqpathoffset2 + reqpathlen;

	for(i=0;i<junction->referral_count;i++) {
		DEBUG(10,("referral %u : %s\n",i,junction->referral_list[i].alternate_path));
		reply_size += (strlen(junction->referral_list[i].alternate_path)+1)*2;
	}

	pdata = (char *)SMB_REALLOC(pdata,reply_size);
	if(pdata == NULL) {
		DEBUG(0,("version3 referral setup: malloc failed for Realloc!\n"));
		return -1;
	}
	*ppdata = pdata;

	/* create the header */
	SSVAL(pdata,0,reqpathlen - 2); /* UCS2 of path consumed minus
					  2 byte null */
	SSVAL(pdata,2,junction->referral_count); /* number of referral */
	if(self_referral) {
		SIVAL(pdata,4,DFSREF_REFERRAL_SERVER | DFSREF_STORAGE_SERVER); 
	} else {
		SIVAL(pdata,4,DFSREF_STORAGE_SERVER);
	}
	
	/* copy in the reqpaths */
	memcpy(pdata+uni_reqpathoffset1,uni_reqpath,reqpathlen);
	memcpy(pdata+uni_reqpathoffset2,uni_reqpath,reqpathlen);
	
	offset = 8;
	for(i=0;i<junction->referral_count;i++) {
		struct referral* ref = &(junction->referral_list[i]);
		int unilen;

		SSVAL(pdata,offset,3); /* version 3 */
		SSVAL(pdata,offset+2,VERSION3_REFERRAL_SIZE);
		if(self_referral) {
			SSVAL(pdata,offset+4,1);
		} else {
			SSVAL(pdata,offset+4,0);
		}

		SSVAL(pdata,offset+6,0); /* ref_flags :use path_consumed bytes? */
		SIVAL(pdata,offset+8,ref->ttl);
	    
		SSVAL(pdata,offset+12,uni_reqpathoffset1-offset);
		SSVAL(pdata,offset+14,uni_reqpathoffset2-offset);
		/* copy referred path into current offset */
		unilen = rpcstr_push(pdata+uni_curroffset,ref->alternate_path,
				     sizeof(pstring), STR_UNICODE | STR_TERMINATE);
		SSVAL(pdata,offset+16,uni_curroffset-offset);
		/* copy 0x10 bytes of 00's in the ServiceSite GUID */
		memset(pdata+offset+18,'\0',16);

		uni_curroffset += unilen;
		offset += VERSION3_REFERRAL_SIZE;
	}
	return reply_size;
}

/******************************************************************
 Set up the DFS referral for the dfs pathname. This call returns
 the amount of the path covered by this server, and where the
 client should be redirected to. This is the meat of the
 TRANS2_GET_DFS_REFERRAL call.
******************************************************************/

int setup_dfs_referral(connection_struct *orig_conn,
			const char *dfs_path,
			int max_referral_level,
			char **ppdata, NTSTATUS *pstatus)
{
	struct junction_map junction;
	int consumedcnt = 0;
	BOOL self_referral = False;
	int reply_size = 0;
	char *pathnamep = NULL;
	pstring local_dfs_path;
	TALLOC_CTX *ctx;

	if (!(ctx=talloc_init("setup_dfs_referral"))) {
		*pstatus = NT_STATUS_NO_MEMORY;
		return -1;
	}

	ZERO_STRUCT(junction);

	/* get the junction entry */
	if (!dfs_path) {
		talloc_destroy(ctx);
		*pstatus = NT_STATUS_NOT_FOUND;
		return -1;
	}

	/* 
	 * Trim pathname sent by client so it begins with only one backslash.
	 * Two backslashes confuse some dfs clients
	 */

	pstrcpy(local_dfs_path, dfs_path);
	pathnamep = local_dfs_path;
	while (IS_DIRECTORY_SEP(pathnamep[0]) && IS_DIRECTORY_SEP(pathnamep[1])) {
		pathnamep++;
	}

	/* The following call can change cwd. */
	*pstatus = get_referred_path(ctx, pathnamep, &junction, &consumedcnt, &self_referral);
	if (!NT_STATUS_IS_OK(*pstatus)) {
		vfs_ChDir(orig_conn,orig_conn->connectpath);
		talloc_destroy(ctx);
		return -1;
	}
	vfs_ChDir(orig_conn,orig_conn->connectpath);
	
	if (!self_referral) {
		pathnamep[consumedcnt] = '\0';

		if( DEBUGLVL( 3 ) ) {
			int i=0;
			dbgtext("setup_dfs_referral: Path %s to alternate path(s):",pathnamep);
			for(i=0;i<junction.referral_count;i++)
				dbgtext(" %s",junction.referral_list[i].alternate_path);
			dbgtext(".\n");
		}
	}

	/* create the referral depeding on version */
	DEBUG(10,("max_referral_level :%d\n",max_referral_level));

	if (max_referral_level < 2) {
		max_referral_level = 2;
	}
	if (max_referral_level > 3) {
		max_referral_level = 3;
	}

	switch(max_referral_level) {
	case 2:
		reply_size = setup_ver2_dfs_referral(pathnamep, ppdata, &junction, 
						     self_referral);
		break;
	case 3:
		reply_size = setup_ver3_dfs_referral(pathnamep, ppdata, &junction, 
						     self_referral);
		break;
	default:
		DEBUG(0,("setup_dfs_referral: Invalid dfs referral version: %d\n", max_referral_level));
		talloc_destroy(ctx);
		*pstatus = NT_STATUS_INVALID_LEVEL;
		return -1;
	}
      
	if (DEBUGLVL(10)) {
		DEBUGADD(0,("DFS Referral pdata:\n"));
		dump_data(0,*ppdata,reply_size);
	}

	talloc_destroy(ctx);
	*pstatus = NT_STATUS_OK;
	return reply_size;
}

/**********************************************************************
 The following functions are called by the NETDFS RPC pipe functions
 **********************************************************************/

/*********************************************************************
 Creates a junction structure from a DFS pathname
**********************************************************************/

BOOL create_junction(const char *dfs_path, struct junction_map *jucn)
{
	int snum;
	BOOL dummy;
	struct dfs_path dp;
 
	NTSTATUS status = parse_dfs_path(NULL, dfs_path, False, &dp, &dummy);

	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}

	/* check if path is dfs : validate first token */
	if (!is_myname_or_ipaddr(dp.hostname)) {
		DEBUG(4,("create_junction: Invalid hostname %s in dfs path %s\n",
			dp.hostname, dfs_path));
		return False;
	}

	/* Check for a non-DFS share */
	snum = lp_servicenumber(dp.servicename);

	if(snum < 0 || !lp_msdfs_root(snum)) {
		DEBUG(4,("create_junction: %s is not an msdfs root.\n",
			dp.servicename));
		return False;
	}

	fstrcpy(jucn->service_name,dp.servicename);
	pstrcpy(jucn->volume_name,dp.reqpath);
	pstrcpy(jucn->comment, lp_comment(snum));
	return True;
}

/**********************************************************************
 Forms a valid Unix pathname from the junction 
 **********************************************************************/

static BOOL junction_to_local_path(struct junction_map *jucn,
				char *path,
				int max_pathlen,
				connection_struct *conn_out)
{
	int snum;
	pstring conn_path;

	snum = lp_servicenumber(jucn->service_name);
	if(snum < 0) {
		return False;
	}

	safe_strcpy(path, lp_pathname(snum), max_pathlen-1);
	safe_strcat(path, "/", max_pathlen-1);
	safe_strcat(path, jucn->volume_name, max_pathlen-1);

	pstrcpy(conn_path, lp_pathname(snum));
	if (!NT_STATUS_IS_OK(create_conn_struct(conn_out, snum, conn_path))) {
		return False;
	}

	return True;
}

BOOL create_msdfs_link(struct junction_map *jucn, BOOL exists)
{
	pstring path;
	pstring msdfs_link;
	connection_struct conns;
 	connection_struct *conn = &conns;
	int i=0;
	BOOL insert_comma = False;
	BOOL ret = False;

	ZERO_STRUCT(conns);

	if(!junction_to_local_path(jucn, path, sizeof(path), conn)) {
		return False;
	}
  
	/* Form the msdfs_link contents */
	pstrcpy(msdfs_link, "msdfs:");
	for(i=0; i<jucn->referral_count; i++) {
		char* refpath = jucn->referral_list[i].alternate_path;
      
		/* Alternate paths always use Windows separators. */
		trim_char(refpath, '\\', '\\');
		if(*refpath == '\0') {
			if (i == 0) {
				insert_comma = False;
			}
			continue;
		}
		if (i > 0 && insert_comma) {
			pstrcat(msdfs_link, ",");
		}

		pstrcat(msdfs_link, refpath);
		if (!insert_comma) {
			insert_comma = True;
		}
	}

	DEBUG(5,("create_msdfs_link: Creating new msdfs link: %s -> %s\n",
		path, msdfs_link));

	if(exists) {
		if(SMB_VFS_UNLINK(conn,path)!=0) {
			goto out;
		}
	}

	if(SMB_VFS_SYMLINK(conn, msdfs_link, path) < 0) {
		DEBUG(1,("create_msdfs_link: symlink failed %s -> %s\nError: %s\n", 
				path, msdfs_link, strerror(errno)));
		goto out;
	}
	
	
	ret = True;

out:

	conn_free_internal(conn);
	return ret;
}

BOOL remove_msdfs_link(struct junction_map *jucn)
{
	pstring path;
	connection_struct conns;
 	connection_struct *conn = &conns;
	BOOL ret = False;

	ZERO_STRUCT(conns);

	if( junction_to_local_path(jucn, path, sizeof(path), conn) ) {
		if( SMB_VFS_UNLINK(conn, path) == 0 ) {
			ret = True;
		}
		talloc_destroy( conn->mem_ctx );
	}

	conn_free_internal(conn);
	return ret;
}

static int form_junctions(TALLOC_CTX *ctx,
				int snum,
				struct junction_map *jucn,
				int jn_remain)
{
	int cnt = 0;
	SMB_STRUCT_DIR *dirp;
	char *dname;
	pstring connect_path;
	char *service_name = lp_servicename(snum);
	connection_struct conn;
	struct referral *ref = NULL;
 
	ZERO_STRUCT(conn);

	if (jn_remain <= 0) {
		return 0;
	}

	pstrcpy(connect_path,lp_pathname(snum));

	if(*connect_path == '\0') {
		return 0;
	}

	/*
	 * Fake up a connection struct for the VFS layer.
	 */

	if (!NT_STATUS_IS_OK(create_conn_struct(&conn, snum, connect_path))) {
		return 0;
	}

	/* form a junction for the msdfs root - convention 
	   DO NOT REMOVE THIS: NT clients will not work with us
	   if this is not present
	*/ 
	fstrcpy(jucn[cnt].service_name, service_name);
	jucn[cnt].volume_name[0] = '\0';
	jucn[cnt].referral_count = 1;

	ref = jucn[cnt].referral_list = TALLOC_ZERO_P(ctx, struct referral);
	if (jucn[cnt].referral_list == NULL) {
		DEBUG(0, ("talloc failed!\n"));
		goto out;
	}

	ref->proximity = 0;
	ref->ttl = REFERRAL_TTL;
	if (*lp_msdfs_proxy(snum) != '\0') {
		pstrcpy(ref->alternate_path, lp_msdfs_proxy(snum));
		cnt++;
		goto out;
	}

	pstr_sprintf(ref->alternate_path, "\\\\%s\\%s",
			get_local_machine_name(),
			service_name);
	cnt++;

	/* Now enumerate all dfs links */
	dirp = SMB_VFS_OPENDIR(&conn, ".", NULL, 0);
	if(!dirp) {
		goto out;
	}

	while ((dname = vfs_readdirname(&conn, dirp)) != NULL) {
		pstring link_target;
		if (cnt >= jn_remain) {
			SMB_VFS_CLOSEDIR(&conn,dirp);
			DEBUG(2, ("ran out of MSDFS junction slots"));
			goto out;
		}
		if (is_msdfs_link(&conn, dname, link_target, NULL)) {
			if (parse_msdfs_symlink(ctx,
					link_target,
					&jucn[cnt].referral_list,
					&jucn[cnt].referral_count)) {

				fstrcpy(jucn[cnt].service_name, service_name);
				pstrcpy(jucn[cnt].volume_name, dname);
				cnt++;
			}
		}
	}
	
	SMB_VFS_CLOSEDIR(&conn,dirp);

out:

	conn_free_internal(&conn);
	return cnt;
}

int enum_msdfs_links(TALLOC_CTX *ctx, struct junction_map *jucn, int jn_max)
{
	int i=0;
	int sharecount = 0;
	int jn_count = 0;

	if(!lp_host_msdfs()) {
		return 0;
	}

	/* Ensure all the usershares are loaded. */
	become_root();
	sharecount = load_usershare_shares();
	unbecome_root();

	for(i=0;i < sharecount && (jn_max - jn_count) > 0;i++) {
		if(lp_msdfs_root(i)) {
			jn_count += form_junctions(ctx, i,jucn,jn_max - jn_count);
		}
	}
	return jn_count;
}

/******************************************************************************
 Core function to resolve a dfs pathname.
******************************************************************************/

NTSTATUS resolve_dfspath(connection_struct *conn, BOOL dfs_pathnames, pstring name)
{
	NTSTATUS status = NT_STATUS_OK;
	BOOL dummy;
	if (dfs_pathnames) {
		status = dfs_redirect(conn, name, False, &dummy);
	}
	return status;
}

/******************************************************************************
 Core function to resolve a dfs pathname possibly containing a wildcard.
 This function is identical to the above except for the BOOL param to
 dfs_redirect but I need this to be separate so it's really clear when
 we're allowing wildcards and when we're not. JRA.
******************************************************************************/

NTSTATUS resolve_dfspath_wcard(connection_struct *conn, BOOL dfs_pathnames, pstring name, BOOL *ppath_contains_wcard)
{
	NTSTATUS status = NT_STATUS_OK;
	if (dfs_pathnames) {
		status = dfs_redirect(conn, name, True, ppath_contains_wcard);
	}
	return status;
}

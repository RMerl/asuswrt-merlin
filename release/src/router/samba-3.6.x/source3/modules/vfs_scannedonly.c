/*
 * scannedonly VFS module for Samba 3.5
 *
 * Copyright 2007,2008,2009,2010 (C) Olivier Sessink
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * ABOUT SCANNEDONLY
 *
 * scannedonly implements a 'filter' like vfs module that talks over a
 * unix domain socket or over UDP to a anti-virus engine.
 *
 * files that are clean have a corresponding .scanned:{filename} file
 * in the same directory. So why the .scanned: files? They take up
 * only an inode, because they are 0 bytes. To test if the file is
 * scanned only a stat() call on the filesystem is needed which is
 * very quick compared to a database lookup. All modern filesystems
 * use database technology such as balanced trees for lookups anyway.
 * The number of inodes in modern filesystems is also not limiting
 * anymore. The .scanned: files are also easy scriptable. You can
 * remove them with a simple find command or create them with a
 * simple touch command. Extended filesystem attributes have similar
 * properties, but are not supported on all filesystems, so that
 * would limit the usage of the module (and attributes are not as
 * easily scriptable)
 *
 * files that are not clean are sent to the AV-engine. Only the
 * filename is sent over the socket. The protocol is very simple:
 * a newline separated list of filenames inside each datagram.
 *
 * a file AV-scan may be requested multiple times, the AV-engine
 * should also check if the file has been scanned already. Requests
 * can also be dropped by the AV-engine (and we thus don't need the
 * reliability of TCP).
 *
 */

#include "includes.h"
#include "smbd/smbd.h"
#include "system/filesys.h"

#include "config.h"

#define SENDBUFFERSIZE 1450

#ifndef       SUN_LEN
#define       SUN_LEN(sunp)   ((size_t)((struct sockaddr_un *)0)->sun_path \
				+ strlen((sunp)->sun_path))
#endif


struct Tscannedonly {
	int socket;
	int domain_socket;
	int portnum;
	int scanning_message_len;
	int recheck_time_open;
	int recheck_tries_open;
	int recheck_size_open;
	int recheck_time_readdir;
	int recheck_tries_readdir;
	bool show_special_files;
	bool rm_hidden_files_on_rmdir;
	bool hide_nonscanned_files;
	bool allow_nonscanned_files;
	char *socketname;
	char *scanhost;
	char *scanning_message;
	char *p_scanned; /* prefix for scanned files */
	char *p_virus; /* prefix for virus containing files */
	char *p_failed; /* prefix for failed to scan files */
	char gsendbuffer[SENDBUFFERSIZE + 1];
};

#define STRUCTSCANO(var) ((struct Tscannedonly *)var)

struct scannedonly_DIR {
	char *base;
	int notify_loop_done;
	SMB_STRUCT_DIR *DIR;
};
#define SCANNEDONLY_DEBUG 9
/*********************/
/* utility functions */
/*********************/

static char *real_path_from_notify_path(TALLOC_CTX *ctx,
					struct Tscannedonly *so,
					const char *path)
{
	char *name;
	int len, pathlen;

	name = strrchr(path, '/');
	if (!name) {
		return NULL;
	}
	pathlen = name - path;
	name++;
	len = strlen(name);
	if (len <= so->scanning_message_len) {
		return NULL;
	}

	if (strcmp(name + (len - so->scanning_message_len),
		   so->scanning_message) != 0) {
		return NULL;
	}

	return talloc_strndup(ctx,path,
			      pathlen + len - so->scanning_message_len);
}

static char *cachefile_name(TALLOC_CTX *ctx,
			    const char *shortname,
			    const char *base,
			    const char *p_scanned)
{
	return talloc_asprintf(ctx, "%s%s%s", base, p_scanned, shortname);
}

static char *name_w_ending_slash(TALLOC_CTX *ctx, const char *name)
{
	int len = strlen(name);
	if (name[len - 1] == '/') {
		return talloc_strdup(ctx,name);
	} else {
		return talloc_asprintf(ctx, "%s/", name);
	}
}

static char *cachefile_name_f_fullpath(TALLOC_CTX *ctx,
				       const char *fullpath,
				       const char *p_scanned)
{
	const char *base;
	char *tmp, *cachefile, *shortname;
	tmp = strrchr(fullpath, '/');
	if (tmp) {
		base = talloc_strndup(ctx, fullpath, (tmp - fullpath) + 1);
		shortname = tmp + 1;
	} else {
		base = "";
		shortname = (char *)fullpath;
	}
	cachefile = cachefile_name(ctx, shortname, base, p_scanned);
	DEBUG(SCANNEDONLY_DEBUG,
	      ("cachefile_name_f_fullpath cachefile=%s\n", cachefile));
	return cachefile;
}

static char *construct_full_path(TALLOC_CTX *ctx, vfs_handle_struct * handle,
				 const char *somepath, bool ending_slash)
{
	char *tmp;

	if (!somepath) {
		return NULL;
	}
	if (somepath[0] == '/') {
		if (ending_slash) {
			return name_w_ending_slash(ctx,somepath);
		}
		return talloc_strdup(ctx,somepath);
	}
	tmp=(char *)somepath;
	if (tmp[0]=='.'&&tmp[1]=='/') {
		tmp+=2;
	}
	/* vfs_GetWd() seems to return a path with a slash */
	if (ending_slash) {
		return talloc_asprintf(ctx, "%s/%s/",
				       vfs_GetWd(ctx, handle->conn),tmp);
	}
	return talloc_asprintf(ctx, "%s/%s",
			       vfs_GetWd(ctx, handle->conn),tmp);
}

static int connect_to_scanner(vfs_handle_struct * handle)
{
	struct Tscannedonly *so = (struct Tscannedonly *)handle->data;

	if (so->domain_socket) {
		struct sockaddr_un saun;
		DEBUG(SCANNEDONLY_DEBUG, ("socket=%s\n", so->socketname));
		if ((so->socket = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
			DEBUG(2, ("failed to create socket %s\n",
				  so->socketname));
			return -1;
		}
		saun.sun_family = AF_UNIX;
		strncpy(saun.sun_path, so->socketname,
			sizeof(saun.sun_path) - 1);
		if (connect(so->socket, (struct sockaddr *)(void *)&saun,
			    SUN_LEN(&saun)) < 0) {
			DEBUG(2, ("failed to connect to socket %s\n",
				  so->socketname));
			return -1;
		}
		DEBUG(SCANNEDONLY_DEBUG,("bound %s to socket %d\n",
					 saun.sun_path, so->socket));

	} else {
		so->socket = open_udp_socket(so->scanhost, so->portnum);
		if (so->socket < 0) {
			DEBUG(2,("failed to open UDP socket to %s:%d\n",
				 so->scanhost,so->portnum));
			return -1;
		}
	}

	{/* increasing the socket buffer is done because we have large bursts
	    of UDP packets or DGRAM's on a domain socket whenever we hit a
	    large directory with lots of unscanned files. */
		int sndsize;
		socklen_t size = sizeof(int);
		getsockopt(so->socket, SOL_SOCKET, SO_RCVBUF,
			   (char *)&sndsize, &size);
		DEBUG(SCANNEDONLY_DEBUG, ("current socket buffer size=%d\n",
					  sndsize));
		sndsize = 262144;
		if (setsockopt(so->socket, SOL_SOCKET, SO_RCVBUF,
			       (char *)&sndsize,
			       (int)sizeof(sndsize)) != 0) {
			DEBUG(SCANNEDONLY_DEBUG,
			      ("error setting socket buffer %s (%d)\n",
			       strerror(errno), errno));
		}
	}
	set_blocking(so->socket, false);
	return 0;
}

static void flush_sendbuffer(vfs_handle_struct * handle)
{
	struct Tscannedonly *so = (struct Tscannedonly *)handle->data;
	int ret, len, loop = 10;
	if (so->gsendbuffer[0] == '\0') {
		return;
	}

	do {
		loop--;
		len = strlen(so->gsendbuffer);
		ret = send(so->socket, so->gsendbuffer, len, 0);
		if (ret == len) {
			so->gsendbuffer[0] = '\0';
			break;
		}
		if (ret == -1) {
			DEBUG(3,("scannedonly flush_sendbuffer: "
				 "error sending on socket %d to scanner:"
				 " %s (%d)\n",
				 so->socket, strerror(errno), errno));
			if (errno == ECONNREFUSED || errno == ENOTCONN
			    || errno == ECONNRESET) {
				if (connect_to_scanner(handle) == -1)
					break;	/* connecting fails, abort */
				/* try again */
			} else if (errno != EINTR) {
				/* on EINTR we just try again, all remaining
				   other errors we log the error
				   and try again ONCE */
				loop = 1;
				DEBUG(3,("scannedonly flush_sendbuffer: "
					 "error sending data to scanner: %s "
					 "(%d)\n", strerror(errno), errno));
			}
		} else {
			/* --> partial write: Resend all filenames that were
			   not or not completely written. a partial filename
			   written means the filename will not arrive correctly,
			   so resend it completely */
			int pos = 0;
			while (pos < len) {
				char *tmp = strchr(so->gsendbuffer+pos, '\n');
				if (tmp && tmp - so->gsendbuffer < ret)
					pos = tmp - so->gsendbuffer + 1;
				else
					break;
			}
			memmove(so->gsendbuffer, so->gsendbuffer + pos,
				SENDBUFFERSIZE - ret);
			/* now try again */
		}
	} while (loop > 0);

	if (so->gsendbuffer[0] != '\0') {
		DEBUG(2,
		      ("scannedonly flush_sendbuffer: "
		       "failed to send files to AV scanner, "
		       "discarding files."));
		so->gsendbuffer[0] = '\0';
	}
}

static void notify_scanner(vfs_handle_struct * handle, const char *scanfile)
{
	char *tmp;
	int tmplen, gsendlen;
	struct Tscannedonly *so = (struct Tscannedonly *)handle->data;
	TALLOC_CTX *ctx=talloc_tos();
	if (scanfile[0] != '/') {
		tmp = construct_full_path(ctx,handle, scanfile, false);
	} else {
		tmp = (char *)scanfile;
	}
	tmplen = strlen(tmp);
	gsendlen = strlen(so->gsendbuffer);
	DEBUG(SCANNEDONLY_DEBUG,
	      ("scannedonly notify_scanner: tmp=%s, tmplen=%d, gsendlen=%d\n",
	       tmp, tmplen, gsendlen));
	if (gsendlen + tmplen >= SENDBUFFERSIZE) {
		flush_sendbuffer(handle);
	}
	strlcat(so->gsendbuffer, tmp, SENDBUFFERSIZE + 1);
	strlcat(so->gsendbuffer, "\n", SENDBUFFERSIZE + 1);
}

static bool is_scannedonly_file(struct Tscannedonly *so, const char *shortname)
{
	if (shortname[0]!='.') {
		return false;
	}
	if (strncmp(shortname, so->p_scanned, strlen(so->p_scanned)) == 0) {
		return true;
	}
	if (strncmp(shortname, so->p_virus, strlen(so->p_virus)) == 0) {
		return true;
	}
	if (strncmp(shortname, so->p_failed, strlen(so->p_failed)) == 0) {
		return true;
	}
	return false;
}

static bool timespec_is_newer(struct timespec *base, struct timespec *test)
{
	return timespec_compare(base,test) < 0;
}

/*
vfs_handle_struct *handle the scannedonly handle
scannedonly_DIR * sDIR the scannedonly struct if called from _readdir()
or NULL
fullpath is a full path starting from / or a relative path to the
current working directory
shortname is the filename without directory components
basename, is the directory without file name component
allow_nonexistant return TRUE if stat() on the requested file fails
recheck_time, the time in milliseconds to wait for the daemon to
create a .scanned file
recheck_tries, the number of tries to wait
recheck_size, size in Kb of files that should not be waited for
loop : boolean if we should try to loop over all files in the directory
and send a notify to the scanner for all files that need scanning
*/
static bool scannedonly_allow_access(vfs_handle_struct * handle,
				     struct scannedonly_DIR *sDIR,
				     struct smb_filename *smb_fname,
				     const char *shortname,
				     const char *base_name,
				     int allow_nonexistant,
				     int recheck_time, int recheck_tries,
				     int recheck_size, int loop)
{
	struct smb_filename *cache_smb_fname;
	TALLOC_CTX *ctx=talloc_tos();
	char *cachefile;
	int retval = -1;
	int didloop;
	DEBUG(SCANNEDONLY_DEBUG,
	      ("smb_fname->base_name=%s, shortname=%s, base_name=%s\n"
	       ,smb_fname->base_name,shortname,base_name));

	if (ISDOT(shortname) || ISDOTDOT(shortname)) {
		return true;
	}
	if (is_scannedonly_file(STRUCTSCANO(handle->data), shortname)) {
		DEBUG(SCANNEDONLY_DEBUG,
		      ("scannedonly_allow_access, %s is a scannedonly file, "
		       "return 0\n", shortname));
		return false;
	}

	if (!VALID_STAT(smb_fname->st)) {
		DEBUG(SCANNEDONLY_DEBUG,("stat %s\n",smb_fname->base_name));
		retval = SMB_VFS_NEXT_STAT(handle, smb_fname);
		if (retval != 0) {
			/* failed to stat this file?!? --> hide it */
			DEBUG(SCANNEDONLY_DEBUG,("no valid stat, return"
						 " allow_nonexistant=%d\n",
						 allow_nonexistant));
			return allow_nonexistant;
		}
	}
	if (!S_ISREG(smb_fname->st.st_ex_mode)) {
		DEBUG(SCANNEDONLY_DEBUG,
		      ("%s is not a regular file, ISDIR=%d\n",
		       smb_fname->base_name,
		       S_ISDIR(smb_fname->st.st_ex_mode)));
		return (STRUCTSCANO(handle->data)->
			show_special_files ||
			S_ISDIR(smb_fname->st.st_ex_mode));
	}
	if (smb_fname->st.st_ex_size == 0) {
		DEBUG(SCANNEDONLY_DEBUG,("empty file, return 1\n"));
		return true;	/* empty files cannot contain viruses ! */
	}
	cachefile = cachefile_name(ctx,
				   shortname,
				   base_name,
				   STRUCTSCANO(handle->data)->p_scanned);
	create_synthetic_smb_fname(ctx, cachefile,NULL,NULL,&cache_smb_fname);
	if (!VALID_STAT(cache_smb_fname->st)) {
		retval = SMB_VFS_NEXT_STAT(handle, cache_smb_fname);
	}
	if (retval == 0 && VALID_STAT(cache_smb_fname->st)) {
		if (timespec_is_newer(&smb_fname->st.st_ex_ctime,
				      &cache_smb_fname->st.st_ex_ctime)) {
			talloc_free(cache_smb_fname);
			return true;
		}
		/* no cachefile or too old */
		SMB_VFS_NEXT_UNLINK(handle, cache_smb_fname);
		retval = -1;
	}

	notify_scanner(handle, smb_fname->base_name);

	didloop = 0;
	if (loop && sDIR && !sDIR->notify_loop_done) {
		/* check the rest of the directory and notify the
		   scanner if some file needs scanning */
		long offset;
		SMB_STRUCT_DIRENT *dire;

		offset = SMB_VFS_NEXT_TELLDIR(handle, sDIR->DIR);
		dire = SMB_VFS_NEXT_READDIR(handle, sDIR->DIR, NULL);
		while (dire) {
			char *fpath2;
			struct smb_filename *smb_fname2;
			fpath2 = talloc_asprintf(ctx, "%s%s", base_name,dire->d_name);
			DEBUG(SCANNEDONLY_DEBUG,
			      ("scannedonly_allow_access in loop, "
			       "found %s\n", fpath2));
			create_synthetic_smb_fname(ctx, fpath2,NULL,NULL,
						   &smb_fname2);
			scannedonly_allow_access(handle, NULL,
						 smb_fname2,
						 dire->d_name,
						 base_name, 0, 0, 0, 0, 0);
			talloc_free(fpath2);
			talloc_free(smb_fname2);
			dire = SMB_VFS_NEXT_READDIR(handle, sDIR->DIR,NULL);
		}
		sDIR->notify_loop_done = 1;
		didloop = 1;
		SMB_VFS_NEXT_SEEKDIR(handle, sDIR->DIR, offset);
	}
	if (recheck_time > 0
	    && ((recheck_size > 0
		 && smb_fname->st.st_ex_size < (1024 * recheck_size))
		|| didloop)) {
		int i = 0;
		flush_sendbuffer(handle);
		while (retval != 0	/*&& errno == ENOENT */
		       && i < recheck_tries) {
			DEBUG(SCANNEDONLY_DEBUG,
			      ("scannedonly_allow_access, wait (try=%d "
			       "(max %d), %d ms) for %s\n",
			       i, recheck_tries,
			       recheck_time, cache_smb_fname->base_name));
			smb_msleep(recheck_time);
			retval = SMB_VFS_NEXT_STAT(handle, cache_smb_fname);
			i++;
		}
	}
	/* still no cachefile, or still too old, return 0 */
	if (retval != 0
	    || !timespec_is_newer(&smb_fname->st.st_ex_ctime,
				  &cache_smb_fname->st.st_ex_ctime)) {
		DEBUG(SCANNEDONLY_DEBUG,
		      ("retval=%d, return 0\n",retval));
		return false;
	}
	return true;
}

/*********************/
/* VFS functions     */
/*********************/

static SMB_STRUCT_DIR *scannedonly_opendir(vfs_handle_struct * handle,
					   const char *fname,
					   const char *mask, uint32 attr)
{
	SMB_STRUCT_DIR *DIRp;
	struct scannedonly_DIR *sDIR;

	DIRp = SMB_VFS_NEXT_OPENDIR(handle, fname, mask, attr);
	if (!DIRp) {
		return NULL;
	}

	sDIR = TALLOC_P(NULL, struct scannedonly_DIR);
	if (fname[0] != '/') {
		sDIR->base = construct_full_path(sDIR,handle, fname, true);
	} else {
		sDIR->base = name_w_ending_slash(sDIR, fname);
	}
	DEBUG(SCANNEDONLY_DEBUG,
			("scannedonly_opendir, fname=%s, base=%s\n",fname,sDIR->base));
	sDIR->DIR = DIRp;
	sDIR->notify_loop_done = 0;
	return (SMB_STRUCT_DIR *) sDIR;
}

static SMB_STRUCT_DIR *scannedonly_fdopendir(vfs_handle_struct * handle,
					   files_struct *fsp,
					   const char *mask, uint32 attr)
{
	SMB_STRUCT_DIR *DIRp;
	struct scannedonly_DIR *sDIR;
	const char *fname;

	DIRp = SMB_VFS_NEXT_FDOPENDIR(handle, fsp, mask, attr);
	if (!DIRp) {
		return NULL;
	}

	fname = (const char *)fsp->fsp_name->base_name;

	sDIR = TALLOC_P(NULL, struct scannedonly_DIR);
	if (fname[0] != '/') {
		sDIR->base = construct_full_path(sDIR,handle, fname, true);
	} else {
		sDIR->base = name_w_ending_slash(sDIR, fname);
	}
	DEBUG(SCANNEDONLY_DEBUG,
			("scannedonly_fdopendir, fname=%s, base=%s\n",fname,sDIR->base));
	sDIR->DIR = DIRp;
	sDIR->notify_loop_done = 0;
	return (SMB_STRUCT_DIR *) sDIR;
}


static SMB_STRUCT_DIRENT *scannedonly_readdir(vfs_handle_struct *handle,
					      SMB_STRUCT_DIR * dirp,
					      SMB_STRUCT_STAT *sbuf)
{
	SMB_STRUCT_DIRENT *result;
	int allowed = 0;
	char *tmp;
	struct smb_filename *smb_fname;
	char *notify_name;
	int namelen;
	SMB_STRUCT_DIRENT *newdirent;
	TALLOC_CTX *ctx=talloc_tos();

	struct scannedonly_DIR *sDIR = (struct scannedonly_DIR *)dirp;
	if (!dirp) {
		return NULL;
	}

	result = SMB_VFS_NEXT_READDIR(handle, sDIR->DIR, sbuf);

	if (!result)
		return NULL;

	if (is_scannedonly_file(STRUCTSCANO(handle->data), result->d_name)) {
		DEBUG(SCANNEDONLY_DEBUG,
		      ("scannedonly_readdir, %s is a scannedonly file, "
		       "skip to next entry\n", result->d_name));
		return scannedonly_readdir(handle, dirp, NULL);
	}
	tmp = talloc_asprintf(ctx, "%s%s", sDIR->base, result->d_name);
	DEBUG(SCANNEDONLY_DEBUG,
	      ("scannedonly_readdir, check access to %s (sbuf=%p)\n",
	       tmp,sbuf));

	/* even if we don't hide nonscanned files or we allow non scanned
	   files we call allow_access because it will notify the daemon to
	   scan these files */
	create_synthetic_smb_fname(ctx, tmp,NULL,
				   sbuf?VALID_STAT(*sbuf)?sbuf:NULL:NULL,
				   &smb_fname);
	allowed = scannedonly_allow_access(
		handle, sDIR, smb_fname,
		result->d_name,
		sDIR->base, 0,
		STRUCTSCANO(handle->data)->hide_nonscanned_files
		? STRUCTSCANO(handle->data)->recheck_time_readdir
		: 0,
		STRUCTSCANO(handle->data)->recheck_tries_readdir,
		-1,
		1);
	DEBUG(SCANNEDONLY_DEBUG,
	      ("scannedonly_readdir access to %s (%s) = %d\n", tmp,
	       result->d_name, allowed));
	if (allowed) {
		return result;
	}
	DEBUG(SCANNEDONLY_DEBUG,
	      ("hide_nonscanned_files=%d, allow_nonscanned_files=%d\n",
	       STRUCTSCANO(handle->data)->hide_nonscanned_files,
	       STRUCTSCANO(handle->data)->allow_nonscanned_files
		      ));

	if (!STRUCTSCANO(handle->data)->hide_nonscanned_files
	    || STRUCTSCANO(handle->data)->allow_nonscanned_files) {
		return result;
	}

	DEBUG(SCANNEDONLY_DEBUG,
	      ("scannedonly_readdir, readdir listing for %s not "
	       "allowed, notify user\n", result->d_name));
	notify_name = talloc_asprintf(
		ctx,"%s %s",result->d_name,
		STRUCTSCANO(handle->data)->scanning_message);
	namelen = strlen(notify_name);
	newdirent = (SMB_STRUCT_DIRENT *)TALLOC_ARRAY(
		ctx, char, sizeof(SMB_STRUCT_DIRENT) + namelen + 1);
	if (!newdirent) {
		return NULL;
	}
	memcpy(newdirent, result, sizeof(SMB_STRUCT_DIRENT));
	memcpy(&newdirent->d_name, notify_name, namelen + 1);
	DEBUG(SCANNEDONLY_DEBUG,
	      ("scannedonly_readdir, return newdirent at %p with "
	       "notification %s\n", newdirent, newdirent->d_name));
	return newdirent;
}

static void scannedonly_seekdir(struct vfs_handle_struct *handle,
				SMB_STRUCT_DIR * dirp, long offset)
{
	struct scannedonly_DIR *sDIR = (struct scannedonly_DIR *)dirp;
	SMB_VFS_NEXT_SEEKDIR(handle, sDIR->DIR, offset);
}

static long scannedonly_telldir(struct vfs_handle_struct *handle,
				SMB_STRUCT_DIR * dirp)
{
	struct scannedonly_DIR *sDIR = (struct scannedonly_DIR *)dirp;
	return SMB_VFS_NEXT_TELLDIR(handle, sDIR->DIR);
}

static void scannedonly_rewinddir(struct vfs_handle_struct *handle,
				  SMB_STRUCT_DIR * dirp)
{
	struct scannedonly_DIR *sDIR = (struct scannedonly_DIR *)dirp;
	SMB_VFS_NEXT_REWINDDIR(handle, sDIR->DIR);
}

static int scannedonly_closedir(vfs_handle_struct * handle,
				SMB_STRUCT_DIR * dirp)
{
	int retval;
	struct scannedonly_DIR *sDIR = (struct scannedonly_DIR *)dirp;
	flush_sendbuffer(handle);
	retval = SMB_VFS_NEXT_CLOSEDIR(handle, sDIR->DIR);
	TALLOC_FREE(sDIR);
	return retval;
}

static int scannedonly_stat(vfs_handle_struct * handle,
			    struct smb_filename *smb_fname)
{
	int ret;
	ret = SMB_VFS_NEXT_STAT(handle, smb_fname);
	DEBUG(SCANNEDONLY_DEBUG, ("scannedonly_stat: %s returned %d\n",
				  smb_fname->base_name, ret));
	if (ret != 0 && errno == ENOENT) {
		TALLOC_CTX *ctx=talloc_tos();
		char *test_base_name, *tmp_base_name = smb_fname->base_name;
		/* possibly this was a fake name (file is being scanned for
		   viruses.txt): check for that and create the real name and
		   stat the real name */
		test_base_name = real_path_from_notify_path(
			ctx,
			STRUCTSCANO(handle->data),
			smb_fname->base_name);
		if (test_base_name) {
			smb_fname->base_name = test_base_name;
			ret = SMB_VFS_NEXT_STAT(handle, smb_fname);
			DEBUG(5, ("_stat: %s returned %d\n",
				  test_base_name, ret));
			smb_fname->base_name = tmp_base_name;
		}
	}
	return ret;
}

static int scannedonly_lstat(vfs_handle_struct * handle,
			     struct smb_filename *smb_fname)
{
	int ret;
	ret = SMB_VFS_NEXT_LSTAT(handle, smb_fname);
	DEBUG(SCANNEDONLY_DEBUG, ("scannedonly_lstat: %s returned %d\n",
				  smb_fname->base_name, ret));
	if (ret != 0 && errno == ENOENT) {
		TALLOC_CTX *ctx=talloc_tos();
		char *test_base_name, *tmp_base_name = smb_fname->base_name;
		/* possibly this was a fake name (file is being scanned for
		   viruses.txt): check for that and create the real name and
		   stat the real name */
		test_base_name = real_path_from_notify_path(
			ctx, STRUCTSCANO(handle->data), smb_fname->base_name);
		if (test_base_name) {
			smb_fname->base_name = test_base_name;
			ret = SMB_VFS_NEXT_LSTAT(handle, smb_fname);
			DEBUG(5, ("_lstat: %s returned %d\n",
				  test_base_name, ret));
			smb_fname->base_name = tmp_base_name;
		}
	}
	return ret;
}

static int scannedonly_open(vfs_handle_struct * handle,
			    struct smb_filename *smb_fname,
			    files_struct * fsp, int flags, mode_t mode)
{
	const char *base;
	char *tmp, *shortname;
	int allowed, write_access = 0;
	TALLOC_CTX *ctx=talloc_tos();
	/* if open for writing ignore it */
	if ((flags & O_ACCMODE) == O_WRONLY) {
		return SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);
	}
	if ((flags & O_ACCMODE) == O_RDWR) {
		write_access = 1;
	}
	/* check if this file is scanned already */
	tmp = strrchr(smb_fname->base_name, '/');
	if (tmp) {
		base = talloc_strndup(ctx,smb_fname->base_name,
				      (tmp - smb_fname->base_name) + 1);
		shortname = tmp + 1;
	} else {
		base = "";
		shortname = (char *)smb_fname->base_name;
	}
	allowed = scannedonly_allow_access(
		handle, NULL, smb_fname, shortname,
		base,
		write_access,
		STRUCTSCANO(handle->data)->recheck_time_open,
		STRUCTSCANO(handle->data)->recheck_tries_open,
		STRUCTSCANO(handle->data)->recheck_size_open,
		0);
	flush_sendbuffer(handle);
	DEBUG(SCANNEDONLY_DEBUG, ("scannedonly_open: allow=%d for %s\n",
				  allowed, smb_fname->base_name));
	if (allowed
	    || STRUCTSCANO(handle->data)->allow_nonscanned_files) {
		return SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);
	}
	errno = EACCES;
	return -1;
}

static int scannedonly_close(vfs_handle_struct * handle, files_struct * fsp)
{
	/* we only have to notify the scanner
	   for files that were open readwrite or writable. */
	if (fsp->can_write) {
		TALLOC_CTX *ctx = talloc_tos();
		notify_scanner(handle, construct_full_path(
				       ctx,handle,
				       fsp->fsp_name->base_name,false));
		flush_sendbuffer(handle);
	}
	return SMB_VFS_NEXT_CLOSE(handle, fsp);
}

static int scannedonly_rename(vfs_handle_struct * handle,
			      const struct smb_filename *smb_fname_src,
			      const struct smb_filename *smb_fname_dst)
{
	/* rename the cache file before we pass the actual rename on */
	struct smb_filename *smb_fname_src_tmp = NULL;
	struct smb_filename *smb_fname_dst_tmp = NULL;
	char *cachefile_src, *cachefile_dst;
	bool needscandst=false;
	int ret;
	TALLOC_CTX *ctx = talloc_tos();

	/* Setup temporary smb_filename structs. */
	cachefile_src = cachefile_name_f_fullpath(
		ctx,
		smb_fname_src->base_name,
		STRUCTSCANO(handle->data)->p_scanned);
	cachefile_dst =	cachefile_name_f_fullpath(
		ctx,
		smb_fname_dst->base_name,
		STRUCTSCANO(handle->data)->p_scanned);
	create_synthetic_smb_fname(ctx, cachefile_src,NULL,NULL,
				   &smb_fname_src_tmp);
	create_synthetic_smb_fname(ctx, cachefile_dst,NULL,NULL,
				   &smb_fname_dst_tmp);

	ret = SMB_VFS_NEXT_RENAME(handle, smb_fname_src_tmp, smb_fname_dst_tmp);
	if (ret == ENOENT) {
		needscandst=true;
	} else if (ret != 0) {
		DEBUG(SCANNEDONLY_DEBUG,
		      ("failed to rename %s into %s error %d: %s\n", cachefile_src,
		       cachefile_dst, ret, strerror(ret)));
		needscandst=true;
	}
	ret = SMB_VFS_NEXT_RENAME(handle, smb_fname_src, smb_fname_dst);
	if (ret == 0 && needscandst) {
		notify_scanner(handle, smb_fname_dst->base_name);
		flush_sendbuffer(handle);
	}
	return ret;
}

static int scannedonly_unlink(vfs_handle_struct * handle,
			      const struct smb_filename *smb_fname)
{
	/* unlink the 'scanned' file too */
	struct smb_filename *smb_fname_cache = NULL;
	char * cachefile;
	TALLOC_CTX *ctx = talloc_tos();

	cachefile = cachefile_name_f_fullpath(
		ctx,
		smb_fname->base_name,
		STRUCTSCANO(handle->data)->p_scanned);
	create_synthetic_smb_fname(ctx, cachefile,NULL,NULL,
				   &smb_fname_cache);
	if (SMB_VFS_NEXT_UNLINK(handle, smb_fname_cache) != 0) {
		DEBUG(SCANNEDONLY_DEBUG, ("_unlink: failed to unlink %s\n",
					  smb_fname_cache->base_name));
	}
	return SMB_VFS_NEXT_UNLINK(handle, smb_fname);
}

static int scannedonly_rmdir(vfs_handle_struct * handle, const char *path)
{
	/* if there are only .scanned: .virus: or .failed: files, we delete
	   those, because the client cannot see them */
	DIR *dirp;
	SMB_STRUCT_DIRENT *dire;
	TALLOC_CTX *ctx = talloc_tos();
	bool only_deletable_files = true, have_files = false;
	char *path_w_slash;

	if (!STRUCTSCANO(handle->data)->rm_hidden_files_on_rmdir)
		return SMB_VFS_NEXT_RMDIR(handle, path);

	path_w_slash = name_w_ending_slash(ctx,path);
	dirp = SMB_VFS_NEXT_OPENDIR(handle, path, NULL, 0);
	if (!dirp) {
		return -1;
	}
	while ((dire = SMB_VFS_NEXT_READDIR(handle, dirp, NULL)) != NULL) {
		if (ISDOT(dire->d_name) || ISDOTDOT(dire->d_name)) {
			continue;
		}
		have_files = true;
		if (!is_scannedonly_file(STRUCTSCANO(handle->data),
					 dire->d_name)) {
			struct smb_filename *smb_fname = NULL;
			char *fullpath;
			int retval;

			if (STRUCTSCANO(handle->data)->show_special_files) {
				only_deletable_files = false;
				break;
			}
			/* stat the file and see if it is a
			   special file */
			fullpath = talloc_asprintf(ctx, "%s%s", path_w_slash,
						  dire->d_name);
			create_synthetic_smb_fname(ctx, fullpath,NULL,NULL,
						   &smb_fname);
			retval = SMB_VFS_NEXT_STAT(handle, smb_fname);
			if (retval == 0
			    && S_ISREG(smb_fname->st.st_ex_mode)) {
				only_deletable_files = false;
			}
			TALLOC_FREE(fullpath);
			TALLOC_FREE(smb_fname);
			break;
		}
	}
	DEBUG(SCANNEDONLY_DEBUG,
	      ("path=%s, have_files=%d, only_deletable_files=%d\n",
	       path, have_files, only_deletable_files));
	if (have_files && only_deletable_files) {
		DEBUG(SCANNEDONLY_DEBUG,
		      ("scannedonly_rmdir, remove leftover scannedonly "
		       "files from %s\n", path_w_slash));
		SMB_VFS_NEXT_REWINDDIR(handle, dirp);
		while ((dire = SMB_VFS_NEXT_READDIR(handle, dirp, NULL))
		       != NULL) {
			char *fullpath;
			struct smb_filename *smb_fname = NULL;
			if (ISDOT(dire->d_name) || ISDOTDOT(dire->d_name)) {
				continue;
			}
			fullpath = talloc_asprintf(ctx, "%s%s", path_w_slash,
						  dire->d_name);
			create_synthetic_smb_fname(ctx, fullpath,NULL,NULL,
						   &smb_fname);
			DEBUG(SCANNEDONLY_DEBUG, ("unlink %s\n", fullpath));
			SMB_VFS_NEXT_UNLINK(handle, smb_fname);
			TALLOC_FREE(fullpath);
			TALLOC_FREE(smb_fname);
		}
	}
	SMB_VFS_NEXT_CLOSEDIR(handle, dirp);
	return SMB_VFS_NEXT_RMDIR(handle, path);
}

static void free_scannedonly_data(void **data)
{
	SAFE_FREE(*data);
}

static int scannedonly_connect(struct vfs_handle_struct *handle,
			       const char *service, const char *user)
{

	struct Tscannedonly *so;

	so = SMB_MALLOC_P(struct Tscannedonly);
	handle->data = (void *)so;
	handle->free_data = free_scannedonly_data;
	so->gsendbuffer[0]='\0';
	so->domain_socket =
		lp_parm_bool(SNUM(handle->conn), "scannedonly",
			     "domain_socket", True);
	so->socketname =
		(char *)lp_parm_const_string(SNUM(handle->conn),
					     "scannedonly", "socketname",
					     "/var/lib/scannedonly/scan");
	so->portnum =
		lp_parm_int(SNUM(handle->conn), "scannedonly", "portnum",
			    2020);
	so->scanhost =
		(char *)lp_parm_const_string(SNUM(handle->conn),
					     "scannedonly", "scanhost",
					     "localhost");

	so->show_special_files =
		lp_parm_bool(SNUM(handle->conn), "scannedonly",
			     "show_special_files", True);
	so->rm_hidden_files_on_rmdir =
		lp_parm_bool(SNUM(handle->conn), "scannedonly",
			     "rm_hidden_files_on_rmdir", True);
	so->hide_nonscanned_files =
		lp_parm_bool(SNUM(handle->conn), "scannedonly",
			     "hide_nonscanned_files", False);
	so->allow_nonscanned_files =
		lp_parm_bool(SNUM(handle->conn), "scannedonly",
			     "allow_nonscanned_files", False);
	so->scanning_message =
		(char *)lp_parm_const_string(SNUM(handle->conn),
					     "scannedonly",
					     "scanning_message",
					     "is being scanned for viruses");
	so->scanning_message_len = strlen(so->scanning_message);
	so->recheck_time_open =
		lp_parm_int(SNUM(handle->conn), "scannedonly",
			    "recheck_time_open", 50);
	so->recheck_tries_open =
		lp_parm_int(SNUM(handle->conn), "scannedonly",
			    "recheck_tries_open", 100);
	so->recheck_size_open =
		lp_parm_int(SNUM(handle->conn), "scannedonly",
			    "recheck_size_open", 100);
	so->recheck_time_readdir =
		lp_parm_int(SNUM(handle->conn), "scannedonly",
			    "recheck_time_readdir", 50);
	so->recheck_tries_readdir =
		lp_parm_int(SNUM(handle->conn), "scannedonly",
			    "recheck_tries_readdir", 20);

	so->p_scanned =
		(char *)lp_parm_const_string(SNUM(handle->conn),
					     "scannedonly",
					     "pref_scanned",
					     ".scanned:");
	so->p_virus =
		(char *)lp_parm_const_string(SNUM(handle->conn),
					     "scannedonly",
					     "pref_virus",
					     ".virus:");
	so->p_failed =
		(char *)lp_parm_const_string(SNUM(handle->conn),
					     "scannedonly",
					     "pref_failed",
					     ".failed:");
	connect_to_scanner(handle);

	return SMB_VFS_NEXT_CONNECT(handle, service, user);
}

/* VFS operations structure */
static struct vfs_fn_pointers vfs_scannedonly_fns = {
	.opendir = scannedonly_opendir,
	.fdopendir = scannedonly_fdopendir,
	.readdir = scannedonly_readdir,
	.seekdir = scannedonly_seekdir,
	.telldir = scannedonly_telldir,
	.rewind_dir = scannedonly_rewinddir,
	.closedir = scannedonly_closedir,
	.rmdir = scannedonly_rmdir,
	.stat = scannedonly_stat,
	.lstat = scannedonly_lstat,
	.open_fn = scannedonly_open,
	.close_fn = scannedonly_close,
	.rename = scannedonly_rename,
	.unlink = scannedonly_unlink,
	.connect_fn = scannedonly_connect
};

NTSTATUS vfs_scannedonly_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "scannedonly",
				&vfs_scannedonly_fns);
}

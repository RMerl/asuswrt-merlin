/* 
   Unix SMB/CIFS implementation.
   FAKE FILE suppport, for faking up special files windows want access to
   Copyright (C) Stefan (metze) Metzmacher	2003
   
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

#ifndef _FAKE_FILE_H
#define _FAKE_FILE_H

enum FAKE_FILE_TYPE {
	FAKE_FILE_TYPE_NONE = 0,
	FAKE_FILE_TYPE_QUOTA,
	FAKE_FILE_TYPE_NAMED_PIPE,
	FAKE_FILE_TYPE_NAMED_PIPE_PROXY
};

/*
we now get the unix name --metze
*/
#define FAKE_FILE_NAME_QUOTA_WIN32	"\\$Extend\\$Quota:$Q:$INDEX_ALLOCATION"
#define FAKE_FILE_NAME_QUOTA_UNIX	"$Extend/$Quota:$Q:$INDEX_ALLOCATION"

struct fake_file_handle {
	enum FAKE_FILE_TYPE type;
	void *private_data;
};

enum FAKE_FILE_TYPE is_fake_file_path(const char *path);
enum FAKE_FILE_TYPE is_fake_file(const struct smb_filename *smb_fname);
NTSTATUS open_fake_file(struct smb_request *req, connection_struct *conn,
				uint16_t current_vuid,
				enum FAKE_FILE_TYPE fake_file_type,
				const struct smb_filename *smb_fname,
				uint32 access_mask,
				files_struct **result);
NTSTATUS close_fake_file(struct smb_request *req, files_struct *fsp);

#endif /* _FAKE_FILE_H */

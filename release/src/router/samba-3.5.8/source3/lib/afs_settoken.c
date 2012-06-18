/* 
 *  Unix SMB/CIFS implementation.
 *  Generate AFS tickets
 *  Copyright (C) Volker Lendecke 2004
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

#ifdef WITH_FAKE_KASERVER

#define NO_ASN1_TYPEDEFS 1

#include <afs/param.h>
#include <afs/stds.h>
#include <afs/afs.h>
#include <afs/auth.h>
#include <afs/venus.h>
#include <asm/unistd.h>
#include <openssl/des.h>
#include <sys/syscall.h>

int afs_syscall( int subcall,
	  char * path,
	  int cmd,
	  char * cmarg,
	  int follow)
{
/*
	return( syscall( SYS_afs_syscall, subcall, path, cmd, cmarg, follow));
*/
	int errcode;
	struct afsprocdata afs_syscall_data;
	afs_syscall_data.syscall = subcall;
	afs_syscall_data.param1 = (long)path;
	afs_syscall_data.param2 = cmd;
	afs_syscall_data.param3 = (long)cmarg;
	afs_syscall_data.param4 = follow;
	int proc_afs_file = open(PROC_SYSCALL_FNAME, O_RDWR);
	if (proc_afs_file < 0)
		proc_afs_file = open(PROC_SYSCALL_ARLA_FNAME, O_RDWR);
	if (proc_afs_file < 0)
		return -1;
	errcode = ioctl(proc_afs_file, VIOC_SYSCALL, &afs_syscall_data);
	close(proc_afs_file);
	return errcode;
}

struct ClearToken {
	uint32 AuthHandle;
	char HandShakeKey[8];
	uint32 ViceId;
	uint32 BeginTimestamp;
	uint32 EndTimestamp;
};

static bool afs_decode_token(const char *string, char **cell,
			     DATA_BLOB *ticket, struct ClearToken *ct)
{
	DATA_BLOB blob;
	struct ClearToken result_ct;
	char *saveptr;

	char *s = SMB_STRDUP(string);

	char *t;

	if ((t = strtok_r(s, "\n", &saveptr)) == NULL) {
		DEBUG(10, ("strtok_r failed\n"));
		return False;
	}

	*cell = SMB_STRDUP(t);

	if ((t = strtok_r(NULL, "\n", &saveptr)) == NULL) {
		DEBUG(10, ("strtok_r failed\n"));
		return False;
	}

	if (sscanf(t, "%u", &result_ct.AuthHandle) != 1) {
		DEBUG(10, ("sscanf AuthHandle failed\n"));
		return False;
	}
		
	if ((t = strtok_r(NULL, "\n", &saveptr)) == NULL) {
		DEBUG(10, ("strtok_r failed\n"));
		return False;
	}

	blob = base64_decode_data_blob(t);

	if ( (blob.data == NULL) ||
	     (blob.length != sizeof(result_ct.HandShakeKey) )) {
		DEBUG(10, ("invalid key: %x/%d\n", (uint32)blob.data,
			   blob.length));
		return False;
	}

	memcpy(result_ct.HandShakeKey, blob.data, blob.length);

	data_blob_free(&blob);

	if ((t = strtok_r(NULL, "\n", &saveptr)) == NULL) {
		DEBUG(10, ("strtok_r failed\n"));
		return False;
	}

	if (sscanf(t, "%u", &result_ct.ViceId) != 1) {
		DEBUG(10, ("sscanf ViceId failed\n"));
		return False;
	}
		
	if ((t = strtok_r(NULL, "\n", &saveptr)) == NULL) {
		DEBUG(10, ("strtok_r failed\n"));
		return False;
	}

	if (sscanf(t, "%u", &result_ct.BeginTimestamp) != 1) {
		DEBUG(10, ("sscanf BeginTimestamp failed\n"));
		return False;
	}
		
	if ((t = strtok_r(NULL, "\n", &saveptr)) == NULL) {
		DEBUG(10, ("strtok_r failed\n"));
		return False;
	}

	if (sscanf(t, "%u", &result_ct.EndTimestamp) != 1) {
		DEBUG(10, ("sscanf EndTimestamp failed\n"));
		return False;
	}
		
	if ((t = strtok_r(NULL, "\n", &saveptr)) == NULL) {
		DEBUG(10, ("strtok_r failed\n"));
		return False;
	}

	blob = base64_decode_data_blob(t);

	if (blob.data == NULL) {
		DEBUG(10, ("Could not get ticket\n"));
		return False;
	}

	*ticket = blob;
	*ct = result_ct;

	return True;
}

/*
  Put an AFS token into the Kernel so that it can authenticate against
  the AFS server. This assumes correct local uid settings.

  This is currently highly Linux and OpenAFS-specific. The correct API
  call for this would be ktc_SetToken. But to do that we would have to
  import a REALLY big bunch of libraries which I would currently like
  to avoid. 
*/

static bool afs_settoken(const char *cell,
			 const struct ClearToken *ctok,
			 DATA_BLOB ticket)
{
	int ret;
	struct {
		char *in, *out;
		uint16 in_size, out_size;
	} iob;

	char buf[1024];
	char *p = buf;
	int tmp;

	memcpy(p, &ticket.length, sizeof(uint32));
	p += sizeof(uint32);
	memcpy(p, ticket.data, ticket.length);
	p += ticket.length;

	tmp = sizeof(struct ClearToken);
	memcpy(p, &tmp, sizeof(uint32));
	p += sizeof(uint32);
	memcpy(p, ctok, tmp);
	p += tmp;

	tmp = 0;

	memcpy(p, &tmp, sizeof(uint32));
	p += sizeof(uint32);

	tmp = strlen(cell);
	if (tmp >= MAXKTCREALMLEN) {
		DEBUG(1, ("Realm too long\n"));
		return False;
	}

	strncpy(p, cell, tmp);
	p += tmp;
	*p = 0;
	p +=1;

	iob.in = buf;
	iob.in_size = PTR_DIFF(p,buf);
	iob.out = buf;
	iob.out_size = sizeof(buf);

#if 0
	file_save("/tmp/ioctlbuf", iob.in, iob.in_size);
#endif

	ret = afs_syscall(AFSCALL_PIOCTL, 0, VIOCSETTOK, (char *)&iob, 0);

	DEBUG(10, ("afs VIOCSETTOK returned %d\n", ret));
	return (ret == 0);
}

bool afs_settoken_str(const char *token_string)
{
	DATA_BLOB ticket;
	struct ClearToken ct;
	bool result;
	char *cell;

	if (!afs_decode_token(token_string, &cell, &ticket, &ct))
		return False;

	if (geteuid() != 0)
		ct.ViceId = getuid();

	result = afs_settoken(cell, &ct, ticket);

	SAFE_FREE(cell);
	data_blob_free(&ticket);

	return result;
}

#else

bool afs_settoken_str(const char *token_string)
{
	return False;
}

#endif

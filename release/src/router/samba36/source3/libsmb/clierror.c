/* 
   Unix SMB/CIFS implementation.
   client error handling routines
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Jelmer Vernooij 2003
   Copyright (C) Jeremy Allison 2006
   
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
#include "libsmb/libsmb.h"

/*****************************************************
 RAP error codes - a small start but will be extended.

 XXX: Perhaps these should move into a common function because they're
 duplicated in clirap2.c

*******************************************************/

static const struct {
	int err;
	const char *message;
} rap_errmap[] = {
	{5,    "RAP5: User has insufficient privilege" },
	{50,   "RAP50: Not supported by server" },
	{65,   "RAP65: Access denied" },
	{86,   "RAP86: The specified password is invalid" },
	{2220, "RAP2220: Group does not exist" },
	{2221, "RAP2221: User does not exist" },
	{2226, "RAP2226: Operation only permitted on a Primary Domain Controller"  },
	{2237, "RAP2237: User is not in group" },
	{2242, "RAP2242: The password of this user has expired." },
	{2243, "RAP2243: The password of this user cannot change." },
	{2244, "RAP2244: This password cannot be used now (password history conflict)." },
	{2245, "RAP2245: The password is shorter than required." },
	{2246, "RAP2246: The password of this user is too recent to change."},

	/* these really shouldn't be here ... */
	{0x80, "Not listening on called name"},
	{0x81, "Not listening for calling name"},
	{0x82, "Called name not present"},
	{0x83, "Called name present, but insufficient resources"},

	{0, NULL}
};  

/****************************************************************************
 Return a description of an SMB error.
****************************************************************************/

static const char *cli_smb_errstr(struct cli_state *cli)
{
	return smb_dos_errstr(cli->inbuf);
}

/****************************************************************************
 Convert a socket error into an NTSTATUS.
****************************************************************************/

static NTSTATUS cli_smb_rw_error_to_ntstatus(struct cli_state *cli)
{
	switch(cli->smb_rw_error) {
		case SMB_READ_TIMEOUT:
			return NT_STATUS_IO_TIMEOUT;
		case SMB_READ_EOF:
			return NT_STATUS_END_OF_FILE;
		/* What we shoud really do for read/write errors is convert from errno. */
		/* FIXME. JRA. */
		case SMB_READ_ERROR:
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		case SMB_WRITE_ERROR:
			return NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	        case SMB_READ_BAD_SIG:
			return NT_STATUS_INVALID_PARAMETER;
		case SMB_NO_MEMORY:
			return NT_STATUS_NO_MEMORY;
	        default:
			break;
	}
	return NT_STATUS_UNSUCCESSFUL;
}

/***************************************************************************
 Return an error message - either an NT error, SMB error or a RAP error.
 Note some of the NT errors are actually warnings or "informational" errors
 in which case they can be safely ignored.
****************************************************************************/

const char *cli_errstr(struct cli_state *cli)
{   
	fstring cli_error_message;
	uint32 flgs2 = SVAL(cli->inbuf,smb_flg2), errnum;
	uint8 errclass;
	int i;
	char *result;

	if (!cli->initialised) {
		fstrcpy(cli_error_message, "[Programmer's error] cli_errstr called on unitialized cli_stat struct!\n");
		goto done;
	}

	/* Was it server socket error ? */
	if (cli->fd == -1 && cli->smb_rw_error) {
		switch(cli->smb_rw_error) {
			case SMB_READ_TIMEOUT:
				slprintf(cli_error_message, sizeof(cli_error_message) - 1,
					"Call timed out: server did not respond after %d milliseconds",
					cli->timeout);
				break;
			case SMB_READ_EOF:
				slprintf(cli_error_message, sizeof(cli_error_message) - 1,
					"Call returned zero bytes (EOF)" );
				break;
			case SMB_READ_ERROR:
				slprintf(cli_error_message, sizeof(cli_error_message) - 1,
					"Read error: %s", strerror(errno) );
				break;
			case SMB_WRITE_ERROR:
				slprintf(cli_error_message, sizeof(cli_error_message) - 1,
					"Write error: %s", strerror(errno) );
				break;
		        case SMB_READ_BAD_SIG:
				slprintf(cli_error_message, sizeof(cli_error_message) - 1,
					"Server packet had invalid SMB signature!");
				break;
		        case SMB_NO_MEMORY:
				slprintf(cli_error_message, sizeof(cli_error_message) - 1,
					"Out of memory");
				break;
		        default:
				slprintf(cli_error_message, sizeof(cli_error_message) - 1,
					"Unknown error code %d\n", cli->smb_rw_error );
				break;
		}
		goto done;
	}

	/* Case #1: RAP error */
	if (cli->rap_error) {
		for (i = 0; rap_errmap[i].message != NULL; i++) {
			if (rap_errmap[i].err == cli->rap_error) {
				return rap_errmap[i].message;
			}
		}

		slprintf(cli_error_message, sizeof(cli_error_message) - 1, "RAP code %d",
			cli->rap_error);

		goto done;
	}

	/* Case #2: 32-bit NT errors */
	if (flgs2 & FLAGS2_32_BIT_ERROR_CODES) {
		NTSTATUS status = NT_STATUS(IVAL(cli->inbuf,smb_rcls));

		return nt_errstr(status);
        }

	cli_dos_error(cli, &errclass, &errnum);

	/* Case #3: SMB error */

	return cli_smb_errstr(cli);

 done:
	result = talloc_strdup(talloc_tos(), cli_error_message);
	SMB_ASSERT(result);
	return result;
}


/****************************************************************************
 Return the 32-bit NT status code from the last packet.
****************************************************************************/

NTSTATUS cli_nt_error(struct cli_state *cli)
{
        int flgs2 = SVAL(cli->inbuf,smb_flg2);

	/* Deal with socket errors first. */
	if (cli->fd == -1 && cli->smb_rw_error) {
		return cli_smb_rw_error_to_ntstatus(cli);
	}

	if (!(flgs2 & FLAGS2_32_BIT_ERROR_CODES)) {
		int e_class  = CVAL(cli->inbuf,smb_rcls);
		int code  = SVAL(cli->inbuf,smb_err);
		return dos_to_ntstatus(e_class, code);
        }

        return NT_STATUS(IVAL(cli->inbuf,smb_rcls));
}


/****************************************************************************
 Return the DOS error from the last packet - an error class and an error
 code.
****************************************************************************/

void cli_dos_error(struct cli_state *cli, uint8 *eclass, uint32 *ecode)
{
	int  flgs2;

	if(!cli->initialised) {
		return;
	}

	/* Deal with socket errors first. */
	if (cli->fd == -1 && cli->smb_rw_error) {
		NTSTATUS status = cli_smb_rw_error_to_ntstatus(cli);
		ntstatus_to_dos( status, eclass, ecode);
		return;
	}

	flgs2 = SVAL(cli->inbuf,smb_flg2);

	if (flgs2 & FLAGS2_32_BIT_ERROR_CODES) {
		NTSTATUS ntstatus = NT_STATUS(IVAL(cli->inbuf, smb_rcls));
		ntstatus_to_dos(ntstatus, eclass, ecode);
                return;
        }

	*eclass  = CVAL(cli->inbuf,smb_rcls);
	*ecode  = SVAL(cli->inbuf,smb_err);
}


/* Return a UNIX errno appropriate for the error received in the last
   packet. */

int cli_errno(struct cli_state *cli)
{
	NTSTATUS status;

	if (cli_is_nt_error(cli)) {
		status = cli_nt_error(cli);
		return map_errno_from_nt_status(status);
	}

        if (cli_is_dos_error(cli)) {
                uint8 eclass;
                uint32 ecode;

                cli_dos_error(cli, &eclass, &ecode);
		status = dos_to_ntstatus(eclass, ecode);
		return map_errno_from_nt_status(status);
        }

        /*
         * Yuck!  A special case for this Vista error.  Since its high-order
         * byte isn't 0xc0, it doesn't match cli_is_nt_error() above.
         */
        status = cli_nt_error(cli);
        if (NT_STATUS_V(status) == NT_STATUS_V(NT_STATUS_INACCESSIBLE_SYSTEM_SHORTCUT)) {
            return EACCES;
        }

	/* for other cases */
	return EINVAL;
}

/* Return true if the last packet was in error */

bool cli_is_error(struct cli_state *cli)
{
	uint32 flgs2 = SVAL(cli->inbuf,smb_flg2), rcls = 0;

	/* A socket error is always an error. */
	if (cli->fd == -1 && cli->smb_rw_error != 0) {
		return True;
	}

        if (flgs2 & FLAGS2_32_BIT_ERROR_CODES) {
                /* Return error is error bits are set */
                rcls = IVAL(cli->inbuf, smb_rcls);
                return (rcls & 0xF0000000) == 0xC0000000;
        }
                
        /* Return error if error class in non-zero */

        rcls = CVAL(cli->inbuf, smb_rcls);
        return rcls != 0;
}

/* Return true if the last error was an NT error */

bool cli_is_nt_error(struct cli_state *cli)
{
	uint32 flgs2 = SVAL(cli->inbuf,smb_flg2);

	/* A socket error is always an NT error. */
	if (cli->fd == -1 && cli->smb_rw_error != 0) {
		return True;
	}

        return cli_is_error(cli) && (flgs2 & FLAGS2_32_BIT_ERROR_CODES);
}

/* Return true if the last error was a DOS error */

bool cli_is_dos_error(struct cli_state *cli)
{
	uint32 flgs2 = SVAL(cli->inbuf,smb_flg2);

	/* A socket error is always a DOS error. */
	if (cli->fd == -1 && cli->smb_rw_error != 0) {
		return True;
	}

        return cli_is_error(cli) && !(flgs2 & FLAGS2_32_BIT_ERROR_CODES);
}

/* Return the last error always as an NTSTATUS. */

NTSTATUS cli_get_nt_error(struct cli_state *cli)
{
	if (cli_is_nt_error(cli)) {
		return cli_nt_error(cli);
	} else if (cli_is_dos_error(cli)) {
		uint32 ecode;
		uint8 eclass;
		cli_dos_error(cli, &eclass, &ecode);
		return dos_to_ntstatus(eclass, ecode);
	} else {
		/* Something went wrong, we don't know what. */
		return NT_STATUS_UNSUCCESSFUL;
	}
}

/* Push an error code into the inbuf to be returned on the next
 * query. */

void cli_set_nt_error(struct cli_state *cli, NTSTATUS status)
{
	SSVAL(cli->inbuf,smb_flg2, SVAL(cli->inbuf,smb_flg2)|FLAGS2_32_BIT_ERROR_CODES);
	SIVAL(cli->inbuf, smb_rcls, NT_STATUS_V(status));
}

/* Reset an error. */

void cli_reset_error(struct cli_state *cli)
{
        if (SVAL(cli->inbuf,smb_flg2) & FLAGS2_32_BIT_ERROR_CODES) {
		SIVAL(cli->inbuf, smb_rcls, NT_STATUS_V(NT_STATUS_OK));
	} else {
		SCVAL(cli->inbuf,smb_rcls,0);
		SSVAL(cli->inbuf,smb_err,0);
	}
}

bool cli_state_is_connected(struct cli_state *cli)
{
	if (cli == NULL) {
		return false;
	}

	if (!cli->initialised) {
		return false;
	}

	if (cli->fd == -1) {
		return false;
	}

	return true;
}


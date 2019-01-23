/*
   Samba Unix/Linux SMB client library
   More client RAP (SMB Remote Procedure Calls) functions
   Copyright (C) 2001 Steve French (sfrench@us.ibm.com)
   Copyright (C) 2001 Jim McDonough (jmcd@us.ibm.com)
   Copyright (C) 2007 Jeremy Allison. jra@samba.org

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

/*****************************************************/
/*                                                   */
/*   Additional RAP functionality                    */
/*                                                   */
/*   RAP is the original SMB RPC, documented         */
/*   by Microsoft and X/Open in the 1990s and        */
/*   supported by most SMB/CIFS servers although     */
/*   it is unlikely that any one implementation      */
/*   supports all RAP command codes since some       */
/*   are quite obsolete and a few are specific       */
/*   to a particular network operating system        */
/*                                                   */
/*   Although it has largely been replaced           */
/*   for complex remote admistration and management  */
/*   (of servers) by the relatively newer            */
/*   DCE/RPC based remote API (which better handles  */
/*   large >64K data structures), there are many     */
/*   important administrative and resource location  */
/*   tasks and user tasks (e.g. password change)     */
/*   that are performed via RAP.                     */
/*                                                   */
/*   Although a few of the RAP calls are implemented */
/*   in the Samba client library already (clirap.c)  */
/*   the new ones are in clirap2.c for easy patching */
/*   and integration and a corresponding header      */
/*   file, rap.h, has been created.                  */
/*                                                   */
/*   This is based on data from the CIFS spec        */
/*   and the LAN Server and LAN Manager              */
/*   Programming Reference books and published       */
/*   RAP document and CIFS forum postings and        */
/*   lots of trial and error                         */
/*                                                   */
/*   Function names changed from API_ (as they are   */
/*   in the CIFS specification) to RAP_ in order     */
/*   to avoid confusion with other API calls         */
/*   sent via DCE RPC                                */
/*                                                   */
/*****************************************************/

/*****************************************************/
/*                                                   */
/* cifsrap.c already includes support for:           */
/*                                                   */
/* WshareEnum ( API number 0, level 1)               */
/* NetServerEnum2 (API num 104, level 1)             */
/* WWkstaUserLogon (132)                             */
/* SamOEMchgPasswordUser2_P (214)                    */
/*                                                   */
/* cifsprint.c already includes support for:         */
/*                                                   */
/* WPrintJobEnum (API num 76, level 2)               */
/* WPrintJobDel  (API num 81)                        */
/*                                                   */
/*****************************************************/

#include "includes.h"
#include "libsmb/libsmb.h"
#include "../librpc/gen_ndr/rap.h"
#include "../librpc/gen_ndr/svcctl.h"
#include "libsmb/clirap.h"

#define WORDSIZE 2
#define DWORDSIZE 4

#define PUTBYTE(p,b) do {SCVAL(p,0,b); p++;} while(0)

#define GETBYTE(p,b,endp) \
	do {\
		if (p+1 < endp) {\
			b = CVAL(p,0);\
		}\
		p++;\
	} while(0)

#define PUTWORD(p,w) do {SSVAL(p,0,w); p += WORDSIZE;} while(0)

#define GETWORD(p,w,endp) \
	do {\
		if (p+WORDSIZE < endp) {\
			w = SVAL(p,0);\
		}\
		p += WORDSIZE;\
	} while(0)

#define PUTDWORD(p,d) do {SIVAL(p,0,d); p += DWORDSIZE;} while(0)

#define GETDWORD(p,d,endp) \
	do {\
		if (p+DWORDSIZE < endp) {\
			d = IVAL(p,0);\
		}\
		p += DWORDSIZE;\
	} while(0)

#define GETRES(p,endp) ((p && p+2 < endp) ? SVAL(p,0) : -1)

/* put string s at p with max len n and increment p past string */
#define PUTSTRING(p,s,n) \
	do {\
		push_ascii(p,s?s:"",n?n:256,STR_TERMINATE);\
		p = push_skip_string(p);\
	} while(0)

/* put string s and p, using fixed len l, and increment p by l */
#define PUTSTRINGF(p,s,l) \
	do {\
		push_ascii(p,s?s:"",l,STR_TERMINATE);\
		p += l;\
	} while (0)

/* put string pointer at p, supplying offset o from rdata r, store   */
/* dword offset at p, increment p by 4 and o by length of s.  This   */
/* means on the first call, you must calc the offset yourself!       */

#define PUTSTRINGP(p,s,r,o) \
	do {\
		if (s) {\
			push_ascii(r+o,s,strlen(s)+1,STR_TERMINATE);\
			PUTDWORD(p,o);\
			o += strlen(s) + 1;\
		} else {\
			PUTDWORD(p,0);\
		}\
	}while(0);

/* get asciiz string dest from src, return increment past string */

static size_t rap_getstring(TALLOC_CTX *ctx, char *src, char **dest, const char *endp)
{
	char *p1;
	size_t len;

	*dest = NULL;
	for (p1 = src, len = 0; *p1 && p1 < endp; len++)
		p1++;
	if (!*p1) {
		len++;
	}
	pull_string_talloc(ctx,src,0,dest,src,len,STR_ASCII);
	return len;
}

/* get fixed length l string dest from src, return increment for src */

static size_t rap_getstringf(char *src, char *dest, size_t l, size_t dlen, char *endp)
{
	char *p1;
	size_t len;

	if (dlen) {
		dest[0] = '\0';
	}
	for (p1 = src, len = 0; *p1 && p1 < endp; len++) {
		p1++;
	}
	if (!*p1) {
		len++;
	}
	if (len > l) {
		len = l;
	}
	if (len) {
	 	pull_ascii(dest,src,len,len,STR_ASCII);
	}
	return l;
}

/* get string dest from offset (obtained at p) from rdata r - converter c */
static size_t rap_getstringp(TALLOC_CTX *ctx, char *p, char **dest, char *r, uint16_t c, char *endp)
{
	uint32_t off = 0;
	const char *src;
	size_t len=0;

	*dest = NULL;
	if (p+4 < endp) {
		GETDWORD(p,off,endp);
		off &= 0x0000FFFF; /* mask the obsolete segment number from the offset */
		off -= c;
	}
	if (r+off > endp || r+off < r) {
		src="";
		len=1;
	} else {
		const char *p1;
		src=r+off;
		for (p1 = src, len = 0; *p1 && p1 < endp; len++) {
			p1++;
		}
		if (!*p1) {
			len++;
		}
	}
	pull_string_talloc(ctx,src,0,dest,src,len,STR_ASCII);
	return 4;
}

static char *make_header(char *param, uint16 apinum, const char *reqfmt, const char *datafmt)
{
	PUTWORD(param,apinum);
	if (reqfmt)
		PUTSTRING(param,reqfmt,0);
	else
		*param++ = (char) 0;

	if (datafmt)
		PUTSTRING(param,datafmt,0);
	else
		*param++ = (char) 0;

	return param;
}

/****************************************************************************
 call a NetGroupDelete - delete user group from remote server
****************************************************************************/

int cli_NetGroupDelete(struct cli_state *cli, const char *group_name)
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	int res = -1;
	char param[WORDSIZE                    /* api number    */
	    +sizeof(RAP_NetGroupDel_REQ) /* parm string   */
	    +1                           /* no ret string */
	    +RAP_GROUPNAME_LEN           /* group to del  */
	    +WORDSIZE];                  /* reserved word */

	/* now send a SMBtrans command with api GroupDel */
	p = make_header(param, RAP_WGroupDel, RAP_NetGroupDel_REQ, NULL);
	PUTSTRING(p, group_name, RAP_GROUPNAME_LEN);
	PUTWORD(p,0);  /* reserved word MBZ on input */

	if (cli_api(cli,
	      param, PTR_DIFF(p,param), 1024, /* Param, length, maxlen */
	      NULL, 0, 200,       /* data, length, maxlen */
	      &rparam, &rprcnt,   /* return params, length */
	      &rdata, &rdrcnt))   /* return data, length */
	{
		char *endp = rparam + rprcnt;
		res = GETRES(rparam,endp);

		if (res == 0) {
			/* nothing to do */
		} else if ((res == 5) || (res == 65)) {
			DEBUG(1, ("Access Denied\n"));
		} else if (res == 2220) {
			DEBUG (1, ("Group does not exist\n"));
		} else {
			DEBUG(4,("NetGroupDelete res=%d\n", res));
		}
	} else {
		res = -1;
		DEBUG(4,("NetGroupDelete failed\n"));
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/****************************************************************************
 call a NetGroupAdd - add user group to remote server
****************************************************************************/

int cli_NetGroupAdd(struct cli_state *cli, struct rap_group_info_1 *grinfo)
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	int res = -1;
	char param[WORDSIZE                    /* api number    */
	    +sizeof(RAP_NetGroupAdd_REQ) /* req string    */
	    +sizeof(RAP_GROUP_INFO_L1)   /* return string */
	    +WORDSIZE                    /* info level    */
	    +WORDSIZE];                  /* reserved word */

	/* offset into data of free format strings.  Will be updated */
	/* by PUTSTRINGP macro and end up with total data length.    */
	int soffset = RAP_GROUPNAME_LEN + 1 + DWORDSIZE;
	char *data;
	size_t data_size;

	/* Allocate data. */
	data_size = MAX(soffset + strlen(grinfo->comment) + 1, 1024);

	data = SMB_MALLOC_ARRAY(char, data_size);
	if (!data) {
		DEBUG (1, ("Malloc fail\n"));
		return -1;
	}

	/* now send a SMBtrans command with api WGroupAdd */

	p = make_header(param, RAP_WGroupAdd,
			RAP_NetGroupAdd_REQ, RAP_GROUP_INFO_L1);
	PUTWORD(p, 1); /* info level */
	PUTWORD(p, 0); /* reserved word 0 */

	p = data;
	PUTSTRINGF(p, (const char *)grinfo->group_name, RAP_GROUPNAME_LEN);
	PUTBYTE(p, 0); /* pad byte 0 */
	PUTSTRINGP(p, grinfo->comment, data, soffset);

	if (cli_api(cli,
	      param, sizeof(param), 1024, /* Param, length, maxlen */
	      data, soffset, sizeof(data), /* data, length, maxlen */
	      &rparam, &rprcnt,   /* return params, length */
	      &rdata, &rdrcnt))   /* return data, length */
	{
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);

		if (res == 0) {
			/* nothing to do */
		} else if ((res == 5) || (res == 65)) {
			DEBUG(1, ("Access Denied\n"));
		} else if (res == 2223) {
			DEBUG (1, ("Group already exists\n"));
		} else {
			DEBUG(4,("NetGroupAdd res=%d\n", res));
		}
	} else {
		res = -1;
		DEBUG(4,("NetGroupAdd failed\n"));
	}

	SAFE_FREE(data);
	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/****************************************************************************
 Call a NetGroupEnum - try and list user groups on a different host.
****************************************************************************/

int cli_RNetGroupEnum(struct cli_state *cli, void (*fn)(const char *, const char *, void *), void *state)
{
	char param[WORDSIZE                     /* api number    */
	    +sizeof(RAP_NetGroupEnum_REQ) /* parm string   */
	    +sizeof(RAP_GROUP_INFO_L1)    /* return string */
	    +WORDSIZE                     /* info level    */
	    +WORDSIZE];                   /* buffer size   */
	char *p;
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rprcnt, rdrcnt;
	int res = -1;

	memset(param, '\0', sizeof(param));
	p = make_header(param, RAP_WGroupEnum,
		  RAP_NetGroupEnum_REQ, RAP_GROUP_INFO_L1);
	PUTWORD(p,1); /* Info level 1 */  /* add level 0 */
	PUTWORD(p,0xFFE0); /* Return buffer size */

	if (cli_api(cli,
	      param, PTR_DIFF(p,param),8,
	      NULL, 0, 0xFFE0 /* data area size */,
	      &rparam, &rprcnt,
	      &rdata, &rdrcnt)) {
		char *endp = rparam + rdrcnt;

		res = GETRES(rparam, endp);
		cli->rap_error = res;
		if(cli->rap_error == 234) {
			DEBUG(1,("Not all group names were returned (such as those longer than 21 characters)\n"));
		} else if (cli->rap_error != 0) {
			DEBUG(1,("NetGroupEnum gave error %d\n", cli->rap_error));
		}
	}

	if (!rdata) {
		DEBUG(4,("NetGroupEnum no data returned\n"));
		goto out;
	}

	if (res == 0 || res == ERRmoredata) {
		char *endp = rparam + rprcnt;
		int i, converter = 0, count = 0;
		TALLOC_CTX *frame = talloc_stackframe();

		p = rparam + WORDSIZE; /* skip result */
		GETWORD(p, converter, endp);
		GETWORD(p, count, endp);

		endp = rdata + rdrcnt;
		for (i=0,p=rdata; i<count && p < endp;i++) {
			char *comment = NULL;
			char groupname[RAP_GROUPNAME_LEN];

			p += rap_getstringf(p,
					groupname,
					RAP_GROUPNAME_LEN,
					RAP_GROUPNAME_LEN,
					endp);
			p++; /* pad byte */
			p += rap_getstringp(frame,
					p,
					&comment,
					rdata,
					converter,
					endp);

			if (!comment || !groupname[0]) {
				break;
			}

			fn(groupname, comment, cli);
		}
		TALLOC_FREE(frame);
	} else {
		DEBUG(4,("NetGroupEnum res=%d\n", res));
	}

  out:

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

int cli_RNetGroupEnum0(struct cli_state *cli,
		       void (*fn)(const char *, void *),
		       void *state)
{
	char param[WORDSIZE                     /* api number    */
		+sizeof(RAP_NetGroupEnum_REQ) /* parm string   */
		+sizeof(RAP_GROUP_INFO_L0)    /* return string */
		+WORDSIZE                     /* info level    */
		+WORDSIZE];                   /* buffer size   */
	char *p;
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rprcnt, rdrcnt;
	int res = -1;

	memset(param, '\0', sizeof(param));
	p = make_header(param, RAP_WGroupEnum,
		RAP_NetGroupEnum_REQ, RAP_GROUP_INFO_L0);
	PUTWORD(p,0); /* Info level 0 */ /* Hmmm. I *very* much suspect this
				      is the resume count, at least
				      that's what smbd believes... */
	PUTWORD(p,0xFFE0); /* Return buffer size */

	if (cli_api(cli,
			param, PTR_DIFF(p,param),8,
			NULL, 0, 0xFFE0 /* data area size */,
			&rparam, &rprcnt,
			&rdata, &rdrcnt)) {
		char *endp = rparam+rprcnt;
		res = GETRES(rparam,endp);
		cli->rap_error = res;
		if(cli->rap_error == 234) {
			DEBUG(1,("Not all group names were returned (such as those longer than 21 characters)\n"));
		} else if (cli->rap_error != 0) {
			DEBUG(1,("NetGroupEnum gave error %d\n", cli->rap_error));
		}
	}

	if (!rdata) {
		DEBUG(4,("NetGroupEnum no data returned\n"));
		goto out;
	}

	if (res == 0 || res == ERRmoredata) {
		char *endp = rparam + rprcnt;
		int i, count = 0;

		p = rparam + WORDSIZE + WORDSIZE; /* skip result and converter */
		GETWORD(p, count, endp);

		endp = rdata + rdrcnt;
		for (i=0,p=rdata; i<count && p < endp;i++) {
			char groupname[RAP_GROUPNAME_LEN];

			p += rap_getstringf(p,
					groupname,
					RAP_GROUPNAME_LEN,
					RAP_GROUPNAME_LEN,
					endp);
			if (groupname[0]) {
				fn(groupname, cli);
			}
		}
	} else {
		DEBUG(4,("NetGroupEnum res=%d\n", res));
	}

  out:

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

int cli_NetGroupDelUser(struct cli_state * cli, const char *group_name, const char *user_name)
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	int res = -1;
	char param[WORDSIZE                        /* api number    */
		+sizeof(RAP_NetGroupDelUser_REQ) /* parm string   */
		+1                               /* no ret string */
		+RAP_GROUPNAME_LEN               /* group name    */
		+RAP_USERNAME_LEN];              /* user to del   */

	/* now send a SMBtrans command with api GroupMemberAdd */
	p = make_header(param, RAP_WGroupDelUser, RAP_NetGroupDelUser_REQ, NULL);
	PUTSTRING(p,group_name,RAP_GROUPNAME_LEN);
	PUTSTRING(p,user_name,RAP_USERNAME_LEN);

	if (cli_api(cli,
			param, PTR_DIFF(p,param), 1024, /* Param, length, maxlen */
			NULL, 0, 200,       /* data, length, maxlen */
			&rparam, &rprcnt,   /* return params, length */
			&rdata, &rdrcnt))   /* return data, length */
	{
		char *endp = rparam + rprcnt;
		res = GETRES(rparam,endp);

		switch(res) {
		case 0:
			break;
		case 5:
		case 65:
			DEBUG(1, ("Access Denied\n"));
			break;
		case 50:
			DEBUG(1, ("Not supported by server\n"));
			break;
		case 2220:
			DEBUG(1, ("Group does not exist\n"));
			break;
		case 2221:
			DEBUG(1, ("User does not exist\n"));
			break;
		case 2237:
			DEBUG(1, ("User is not in group\n"));
			break;
		default:
			DEBUG(4,("NetGroupDelUser res=%d\n", res));
		}
	} else {
		res = -1;
		DEBUG(4,("NetGroupDelUser failed\n"));
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

int cli_NetGroupAddUser(struct cli_state * cli, const char *group_name, const char *user_name)
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	int res = -1;
	char param[WORDSIZE                        /* api number    */
		+sizeof(RAP_NetGroupAddUser_REQ) /* parm string   */
		+1                               /* no ret string */
		+RAP_GROUPNAME_LEN               /* group name    */
		+RAP_USERNAME_LEN];              /* user to add   */

	/* now send a SMBtrans command with api GroupMemberAdd */
	p = make_header(param, RAP_WGroupAddUser, RAP_NetGroupAddUser_REQ, NULL);
	PUTSTRING(p,group_name,RAP_GROUPNAME_LEN);
	PUTSTRING(p,user_name,RAP_USERNAME_LEN);

	if (cli_api(cli,
			param, PTR_DIFF(p,param), 1024, /* Param, length, maxlen */
			NULL, 0, 200,       /* data, length, maxlen */
			&rparam, &rprcnt,   /* return params, length */
			&rdata, &rdrcnt))   /* return data, length */
	{
		char *endp = rparam + rprcnt;
		res = GETRES(rparam,endp);

		switch(res) {
		case 0:
			break;
		case 5:
		case 65:
			DEBUG(1, ("Access Denied\n"));
			break;
		case 50:
			DEBUG(1, ("Not supported by server\n"));
			break;
		case 2220:
			DEBUG(1, ("Group does not exist\n"));
			break;
		case 2221:
			DEBUG(1, ("User does not exist\n"));
			break;
		default:
			DEBUG(4,("NetGroupAddUser res=%d\n", res));
		}
	} else {
		res = -1;
		DEBUG(4,("NetGroupAddUser failed\n"));
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}


int cli_NetGroupGetUsers(struct cli_state * cli, const char *group_name, void (*fn)(const char *, void *), void *state )
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	int res = -1;
	char param[WORDSIZE                        /* api number    */
		+sizeof(RAP_NetGroupGetUsers_REQ)/* parm string   */
		+sizeof(RAP_GROUP_USERS_INFO_0)  /* return string */
		+RAP_GROUPNAME_LEN               /* group name    */
		+WORDSIZE                        /* info level    */
		+WORDSIZE];                      /* buffer size   */

	/* now send a SMBtrans command with api GroupGetUsers */
	p = make_header(param, RAP_WGroupGetUsers,
		RAP_NetGroupGetUsers_REQ, RAP_GROUP_USERS_INFO_0);
	PUTSTRING(p,group_name,RAP_GROUPNAME_LEN-1);
	PUTWORD(p,0); /* info level 0 */
	PUTWORD(p,0xFFE0); /* return buffer size */

	if (cli_api(cli,
			param, PTR_DIFF(p,param),PTR_DIFF(p,param),
			NULL, 0, CLI_BUFFER_SIZE,
			&rparam, &rprcnt,
			&rdata, &rdrcnt)) {
		char *endp = rparam + rprcnt;
		res = GETRES(rparam,endp);
		cli->rap_error = res;
		if (res != 0) {
			DEBUG(1,("NetGroupGetUsers gave error %d\n", res));
		}
	}

	if (!rdata) {
		DEBUG(4,("NetGroupGetUsers no data returned\n"));
		goto out;
	}

	if (res == 0 || res == ERRmoredata) {
		char *endp = rparam + rprcnt;
		int i, count = 0;
		char username[RAP_USERNAME_LEN];

		p = rparam + WORDSIZE + WORDSIZE;
		GETWORD(p, count, endp);

		endp = rdata + rdrcnt;
		for (i=0,p=rdata; i<count && p < endp; i++) {
			p += rap_getstringf(p,
					username,
					RAP_USERNAME_LEN,
					RAP_USERNAME_LEN,
					endp);
			if (username[0]) {
				fn(username, state);
			}
		}
	} else {
		DEBUG(4,("NetGroupGetUsers res=%d\n", res));
	}

  out:

	SAFE_FREE(rdata);
	SAFE_FREE(rparam);
	return res;
}

int cli_NetUserGetGroups(struct cli_state * cli, const char *user_name, void (*fn)(const char *, void *), void *state )
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	int res = -1;
	char param[WORDSIZE                        /* api number    */
		+sizeof(RAP_NetUserGetGroups_REQ)/* parm string   */
		+sizeof(RAP_GROUP_USERS_INFO_0)  /* return string */
		+RAP_USERNAME_LEN               /* user name    */
		+WORDSIZE                        /* info level    */
		+WORDSIZE];                      /* buffer size   */

	/* now send a SMBtrans command with api GroupGetUsers */
	p = make_header(param, RAP_WUserGetGroups,
		RAP_NetUserGetGroups_REQ, RAP_GROUP_USERS_INFO_0);
	PUTSTRING(p,user_name,RAP_USERNAME_LEN-1);
	PUTWORD(p,0); /* info level 0 */
	PUTWORD(p,0xFFE0); /* return buffer size */

	if (cli_api(cli,
			param, PTR_DIFF(p,param),PTR_DIFF(p,param),
			NULL, 0, CLI_BUFFER_SIZE,
			&rparam, &rprcnt,
			&rdata, &rdrcnt)) {
		char *endp = rparam + rprcnt;
		res = GETRES(rparam,endp);
		cli->rap_error = res;
		if (res != 0) {
			DEBUG(1,("NetUserGetGroups gave error %d\n", res));
		}
	}

	if (!rdata) {
		DEBUG(4,("NetUserGetGroups no data returned\n"));
		goto out;
	}

	if (res == 0 || res == ERRmoredata) {
		char *endp = rparam + rprcnt;
		int i, count = 0;
		char groupname[RAP_GROUPNAME_LEN];

		p = rparam + WORDSIZE + WORDSIZE;
		GETWORD(p, count, endp);

		endp = rdata + rdrcnt;
		for (i=0,p=rdata; i<count && p < endp; i++) {
			p += rap_getstringf(p,
					groupname,
					RAP_GROUPNAME_LEN,
					RAP_GROUPNAME_LEN,
					endp);
			if (groupname[0]) {
				fn(groupname, state);
			}
		}
	} else {
		DEBUG(4,("NetUserGetGroups res=%d\n", res));
	}

  out:

	SAFE_FREE(rdata);
	SAFE_FREE(rparam);
	return res;
}

/****************************************************************************
 Call a NetUserDelete - delete user from remote server.
****************************************************************************/

int cli_NetUserDelete(struct cli_state *cli, const char * user_name )
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	int res = -1;
	char param[WORDSIZE                    /* api number    */
		+sizeof(RAP_NetGroupDel_REQ) /* parm string   */
		+1                           /* no ret string */
		+RAP_USERNAME_LEN            /* user to del   */
		+WORDSIZE];                  /* reserved word */

	/* now send a SMBtrans command with api UserDel */
	p = make_header(param, RAP_WUserDel, RAP_NetGroupDel_REQ, NULL);
	PUTSTRING(p, user_name, RAP_USERNAME_LEN);
	PUTWORD(p,0);  /* reserved word MBZ on input */

	if (cli_api(cli,
			param, PTR_DIFF(p,param), 1024, /* Param, length, maxlen */
			NULL, 0, 200,       /* data, length, maxlen */
			&rparam, &rprcnt,   /* return params, length */
			&rdata, &rdrcnt))   /* return data, length */
	{
		char *endp = rparam + rprcnt;
		res = GETRES(rparam,endp);

		if (res == 0) {
			/* nothing to do */
		} else if ((res == 5) || (res == 65)) {
			DEBUG(1, ("Access Denied\n"));
		} else if (res == 2221) {
			DEBUG (1, ("User does not exist\n"));
		} else {
			DEBUG(4,("NetUserDelete res=%d\n", res));
		}
	} else {
		res = -1;
		DEBUG(4,("NetUserDelete failed\n"));
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/****************************************************************************
 Call a NetUserAdd - add user to remote server.
****************************************************************************/

int cli_NetUserAdd(struct cli_state *cli, struct rap_user_info_1 * userinfo )
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	int res = -1;
	char param[WORDSIZE                    /* api number    */
		+sizeof(RAP_NetUserAdd2_REQ) /* req string    */
		+sizeof(RAP_USER_INFO_L1)    /* data string   */
		+WORDSIZE                    /* info level    */
		+WORDSIZE                    /* buffer length */
		+WORDSIZE];                  /* reserved      */

	char data[1024];
		/* offset into data of free format strings.  Will be updated */
		/* by PUTSTRINGP macro and end up with total data length.    */
	int soffset=RAP_USERNAME_LEN+1 /* user name + pad */
		+ RAP_UPASSWD_LEN            /* password        */
		+ DWORDSIZE                  /* password age    */
		+ WORDSIZE                   /* privilege       */
		+ DWORDSIZE                  /* home dir ptr    */
		+ DWORDSIZE                  /* comment ptr     */
		+ WORDSIZE                   /* flags           */
		+ DWORDSIZE;                 /* login script ptr*/

	/* now send a SMBtrans command with api NetUserAdd */
	p = make_header(param, RAP_WUserAdd2,
		RAP_NetUserAdd2_REQ, RAP_USER_INFO_L1);

	PUTWORD(p, 1); /* info level */
	PUTWORD(p, 0); /* pwencrypt */
	if(userinfo->passwrd)
		PUTWORD(p,MIN(strlen((const char *)userinfo->passwrd), RAP_UPASSWD_LEN));
	else
		PUTWORD(p, 0); /* password length */

	p = data;
	memset(data, '\0', soffset);

	PUTSTRINGF(p, (const char *)userinfo->user_name, RAP_USERNAME_LEN);
	PUTBYTE(p, 0); /* pad byte 0 */
	PUTSTRINGF(p, (const char *)userinfo->passwrd, RAP_UPASSWD_LEN);
	PUTDWORD(p, 0); /* pw age - n.a. on user add */
	PUTWORD(p, userinfo->priv);
	PUTSTRINGP(p, userinfo->home_dir, data, soffset);
	PUTSTRINGP(p, userinfo->comment, data, soffset);
	PUTWORD(p, userinfo->userflags);
	PUTSTRINGP(p, userinfo->logon_script, data, soffset);

	if (cli_api(cli,
		      param, sizeof(param), 1024, /* Param, length, maxlen */
		      data, soffset, sizeof(data), /* data, length, maxlen */
		      &rparam, &rprcnt,   /* return params, length */
		      &rdata, &rdrcnt))   /* return data, length */
	{
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);

		if (res == 0) {
			/* nothing to do */
		} else if ((res == 5) || (res == 65)) {
			DEBUG(1, ("Access Denied\n"));
		} else if (res == 2224) {
			DEBUG (1, ("User already exists\n"));
		} else {
			DEBUG(4,("NetUserAdd res=%d\n", res));
		}
	} else {
		res = -1;
		DEBUG(4,("NetUserAdd failed\n"));
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/****************************************************************************
call a NetUserEnum - try and list users on a different host
****************************************************************************/

int cli_RNetUserEnum(struct cli_state *cli, void (*fn)(const char *, const char *, const char *, const char *, void *), void *state)
{
	char param[WORDSIZE                 /* api number    */
		+sizeof(RAP_NetUserEnum_REQ) /* parm string   */
		+sizeof(RAP_USER_INFO_L1)    /* return string */
		+WORDSIZE                 /* info level    */
		+WORDSIZE];               /* buffer size   */
	char *p;
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rprcnt, rdrcnt;
	int res = -1;

	memset(param, '\0', sizeof(param));
	p = make_header(param, RAP_WUserEnum,
		RAP_NetUserEnum_REQ, RAP_USER_INFO_L1);
	PUTWORD(p,1); /* Info level 1 */
	PUTWORD(p,0xFF00); /* Return buffer size */

	/* BB Fix handling of large numbers of users to be returned */
	if (cli_api(cli,
			param, PTR_DIFF(p,param),8,
			NULL, 0, CLI_BUFFER_SIZE,
			&rparam, &rprcnt,
			&rdata, &rdrcnt)) {
		char *endp = rparam + rprcnt;
		res = GETRES(rparam,endp);
		cli->rap_error = res;
		if (cli->rap_error != 0) {
			DEBUG(1,("NetUserEnum gave error %d\n", cli->rap_error));
		}
	}

	if (!rdata) {
		DEBUG(4,("NetUserEnum no data returned\n"));
		goto out;
	}

	if (res == 0 || res == ERRmoredata) {
		int i, converter = 0, count = 0;
		char username[RAP_USERNAME_LEN];
		char userpw[RAP_UPASSWD_LEN];
		char *endp = rparam + rprcnt;
		char *comment, *homedir, *logonscript;
		TALLOC_CTX *frame = talloc_stackframe();

		p = rparam + WORDSIZE; /* skip result */
		GETWORD(p, converter, endp);
		GETWORD(p, count, endp);

		endp = rdata + rdrcnt;
		for (i=0,p=rdata;i<count && p < endp;i++) {
			p += rap_getstringf(p,
					username,
					RAP_USERNAME_LEN,
					RAP_USERNAME_LEN,
					endp);
			p++; /* pad byte */
			p += rap_getstringf(p,
					userpw,
					RAP_UPASSWD_LEN,
					RAP_UPASSWD_LEN,
					endp);
			p += DWORDSIZE; /* skip password age */
			p += WORDSIZE;  /* skip priv: 0=guest, 1=user, 2=admin */
			p += rap_getstringp(frame,
					p,
					&homedir,
					rdata,
					converter,
					endp);
			p += rap_getstringp(frame,
					p,
					&comment,
					rdata,
					converter,
					endp);
			p += WORDSIZE;  /* skip flags */
			p += rap_getstringp(frame,
					p,
					&logonscript,
					rdata,
					converter,
					endp);
			if (username[0] && comment &&
					homedir && logonscript) {
				fn(username,
					comment,
					homedir,
					logonscript,
					cli);
			}
		}
		TALLOC_FREE(frame);
	} else {
		DEBUG(4,("NetUserEnum res=%d\n", res));
	}

  out:

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

int cli_RNetUserEnum0(struct cli_state *cli,
		      void (*fn)(const char *, void *),
		      void *state)
{
	char param[WORDSIZE                 /* api number    */
		+sizeof(RAP_NetUserEnum_REQ) /* parm string   */
		+sizeof(RAP_USER_INFO_L0)    /* return string */
		+WORDSIZE                 /* info level    */
		+WORDSIZE];               /* buffer size   */
	char *p;
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rprcnt, rdrcnt;
	int res = -1;

	memset(param, '\0', sizeof(param));
	p = make_header(param, RAP_WUserEnum,
		RAP_NetUserEnum_REQ, RAP_USER_INFO_L0);
	PUTWORD(p,0); /* Info level 1 */
	PUTWORD(p,0xFF00); /* Return buffer size */

	/* BB Fix handling of large numbers of users to be returned */
	if (cli_api(cli,
			param, PTR_DIFF(p,param),8,
			NULL, 0, CLI_BUFFER_SIZE,
			&rparam, &rprcnt,
			&rdata, &rdrcnt)) {
		char *endp = rparam + rprcnt;
		res = GETRES(rparam,endp);
		cli->rap_error = res;
		if (cli->rap_error != 0) {
			DEBUG(1,("NetUserEnum gave error %d\n", cli->rap_error));
		}
	}

	if (!rdata) {
		DEBUG(4,("NetUserEnum no data returned\n"));
		goto out;
	}

	if (res == 0 || res == ERRmoredata) {
		int i, count = 0;
		char *endp = rparam + rprcnt;
		char username[RAP_USERNAME_LEN];

		p = rparam + WORDSIZE + WORDSIZE; /* skip result and converter */
		GETWORD(p, count, endp);

		endp = rdata + rdrcnt;
		for (i=0,p=rdata;i<count && p < endp;i++) {
			p += rap_getstringf(p,
					username,
					RAP_USERNAME_LEN,
					RAP_USERNAME_LEN,
					endp);
			if (username[0]) {
				fn(username, cli);
			}
		}
	} else {
		DEBUG(4,("NetUserEnum res=%d\n", res));
	}

  out:

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/****************************************************************************
 Call a NetFileClose2 - close open file on another session to server.
****************************************************************************/

int cli_NetFileClose(struct cli_state *cli, uint32 file_id )
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	char param[WORDSIZE                    /* api number    */
		+sizeof(RAP_WFileClose2_REQ) /* req string    */
		+1                           /* no ret string */
		+DWORDSIZE];                 /* file ID          */
	int res = -1;

	/* now send a SMBtrans command with api RNetShareEnum */
	p = make_header(param, RAP_WFileClose2, RAP_WFileClose2_REQ, NULL);
	PUTDWORD(p, file_id);

	if (cli_api(cli,
			param, PTR_DIFF(p,param), 1024, /* Param, length, maxlen */
			NULL, 0, 200,       /* data, length, maxlen */
			&rparam, &rprcnt,   /* return params, length */
			&rdata, &rdrcnt))   /* return data, length */
	{
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);

		if (res == 0) {
			/* nothing to do */
		} else if (res == 2314){
			DEBUG(1, ("NetFileClose2 - attempt to close non-existant file open instance\n"));
		} else {
			DEBUG(4,("NetFileClose2 res=%d\n", res));
		}
	} else {
		res = -1;
		DEBUG(4,("NetFileClose2 failed\n"));
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/****************************************************************************
 Call a NetFileGetInfo - get information about server file opened from other
 workstation.
****************************************************************************/

int cli_NetFileGetInfo(struct cli_state *cli, uint32 file_id, void (*fn)(const char *, const char *, uint16, uint16, uint32))
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	int res = -1;
	char param[WORDSIZE                      /* api number      */
		+sizeof(RAP_WFileGetInfo2_REQ) /* req string      */
		+sizeof(RAP_FILE_INFO_L3)      /* return string   */
		+DWORDSIZE                     /* file ID          */
		+WORDSIZE                      /* info level      */
		+WORDSIZE];                    /* buffer size     */

	/* now send a SMBtrans command with api RNetShareEnum */
	p = make_header(param, RAP_WFileGetInfo2,
		RAP_WFileGetInfo2_REQ, RAP_FILE_INFO_L3);
	PUTDWORD(p, file_id);
	PUTWORD(p, 3);  /* info level */
	PUTWORD(p, 0x1000);   /* buffer size */
	if (cli_api(cli,
			param, PTR_DIFF(p,param), 1024, /* Param, length, maxlen */
			NULL, 0, 0x1000,  /* data, length, maxlen */
			&rparam, &rprcnt,               /* return params, length */
			&rdata, &rdrcnt))               /* return data, length */
	{
		char *endp = rparam + rprcnt;
		res = GETRES(rparam,endp);
		if (res == 0 || res == ERRmoredata) {
			TALLOC_CTX *frame = talloc_stackframe();
			int converter = 0,id = 0, perms = 0, locks = 0;
			char *fpath, *fuser;

			p = rparam + WORDSIZE; /* skip result */
			GETWORD(p, converter, endp);

			p = rdata;
			endp = rdata + rdrcnt;

			GETDWORD(p, id, endp);
			GETWORD(p, perms, endp);
			GETWORD(p, locks, endp);

			p += rap_getstringp(frame,
					p,
					&fpath,
					rdata,
					converter,
					endp);
			p += rap_getstringp(frame,
					p,
					&fuser,
					rdata,
					converter,
					endp);

			if (fpath && fuser) {
				fn(fpath, fuser, perms, locks, id);
			}

			TALLOC_FREE(frame);
		} else {
			DEBUG(4,("NetFileGetInfo2 res=%d\n", res));
		}
	} else {
		res = -1;
		DEBUG(4,("NetFileGetInfo2 failed\n"));
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/****************************************************************************
* Call a NetFileEnum2 - list open files on an SMB server
*
* PURPOSE:  Remotes a NetFileEnum API call to the current server or target
*           server listing the files open via the network (and their
*           corresponding open instance ids)
*
* Dependencies: none
*
* Parameters:
*             cli    - pointer to cli_state structure
*             user   - if present, return only files opened by this remote user
*             base_path - if present, return only files opened below this
*                         base path
*             fn     - display function to invoke for each entry in the result
*
*
* Returns:
*             True      - success
*             False     - failure
*
****************************************************************************/

int cli_NetFileEnum(struct cli_state *cli, const char * user,
		    const char * base_path,
		    void (*fn)(const char *, const char *, uint16, uint16,
			       uint32))
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	char param[WORDSIZE                   /* api number      */
		+sizeof(RAP_WFileEnum2_REQ) /* req string      */
		+sizeof(RAP_FILE_INFO_L3)   /* return string   */
		+1024                        /* base path (opt) */
		+RAP_USERNAME_LEN           /* user name (opt) */
		+WORDSIZE                   /* info level      */
		+WORDSIZE                   /* buffer size     */
		+DWORDSIZE                  /* resume key ?    */
		+DWORDSIZE];                /* resume key ?    */
	int count = -1;
	int res = -1;

	/* now send a SMBtrans command with api RNetShareEnum */
	p = make_header(param, RAP_WFileEnum2,
		RAP_WFileEnum2_REQ, RAP_FILE_INFO_L3);

	PUTSTRING(p, base_path, 1024);
	PUTSTRING(p, user, RAP_USERNAME_LEN);
	PUTWORD(p, 3); /* info level */
	PUTWORD(p, 0xFF00);  /* buffer size */
	PUTDWORD(p, 0);  /* zero out the resume key */
	PUTDWORD(p, 0);  /* or is this one the resume key? */

	if (cli_api(cli,
			param, PTR_DIFF(p,param), 1024, /* Param, length, maxlen */
			NULL, 0, 0xFF00,  /* data, length, maxlen */
			&rparam, &rprcnt,               /* return params, length */
			&rdata, &rdrcnt))               /* return data, length */
	{
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);

		if (res == 0 || res == ERRmoredata) {
			TALLOC_CTX *frame = talloc_stackframe();
			int converter = 0, i;

			p = rparam + WORDSIZE; /* skip result */
			GETWORD(p, converter, endp);
			GETWORD(p, count, endp);

			p = rdata;
			endp = rdata + rdrcnt;
			for (i=0; i<count && p < endp; i++) {
				int id = 0, perms = 0, locks = 0;
				char *fpath, *fuser;

				GETDWORD(p, id, endp);
				GETWORD(p, perms, endp);
				GETWORD(p, locks, endp);
				p += rap_getstringp(frame,
					p,
					&fpath,
					rdata,
					converter,
					endp);
				p += rap_getstringp(frame,
					p,
					&fuser,
					rdata,
					converter,
					endp);

				if (fpath && fuser) {
					fn(fpath, fuser, perms, locks, id);
				}
			}  /* BB fix ERRmoredata case to send resume request */
			TALLOC_FREE(frame);
		} else {
			DEBUG(4,("NetFileEnum2 res=%d\n", res));
		}
	} else {
		DEBUG(4,("NetFileEnum2 failed\n"));
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return count;
}

/****************************************************************************
 Call a NetShareAdd - share/export directory on remote server.
****************************************************************************/

int cli_NetShareAdd(struct cli_state *cli, struct rap_share_info_2 * sinfo )
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	int res = -1;
	char param[WORDSIZE                  /* api number    */
		+sizeof(RAP_WShareAdd_REQ) /* req string    */
		+sizeof(RAP_SHARE_INFO_L2) /* return string */
		+WORDSIZE                  /* info level    */
		+WORDSIZE];                /* reserved word */
	char data[1024];
	/* offset to free format string section following fixed length data.  */
	/* will be updated by PUTSTRINGP macro and will end up with total len */
	int soffset = RAP_SHARENAME_LEN + 1 /* share name + pad   */
		+ WORDSIZE                        /* share type    */
		+ DWORDSIZE                       /* comment pointer */
		+ WORDSIZE                        /* permissions */
		+ WORDSIZE                        /* max users */
		+ WORDSIZE                        /* active users */
		+ DWORDSIZE                       /* share path */
		+ RAP_SPASSWD_LEN + 1;            /* share password + pad */

	memset(param,'\0',sizeof(param));
	/* now send a SMBtrans command with api RNetShareAdd */
	p = make_header(param, RAP_WshareAdd,
		RAP_WShareAdd_REQ, RAP_SHARE_INFO_L2);
	PUTWORD(p, 2); /* info level */
	PUTWORD(p, 0); /* reserved word 0 */

	p = data;
	PUTSTRINGF(p, (const char *)sinfo->share_name, RAP_SHARENAME_LEN);
	PUTBYTE(p, 0); /* pad byte 0 */

	PUTWORD(p, sinfo->share_type);
	PUTSTRINGP(p, sinfo->comment, data, soffset);
	PUTWORD(p, sinfo->perms);
	PUTWORD(p, sinfo->maximum_users);
	PUTWORD(p, sinfo->active_users);
	PUTSTRINGP(p, sinfo->path, data, soffset);
	PUTSTRINGF(p, (const char *)sinfo->password, RAP_SPASSWD_LEN);
	SCVAL(p,-1,0x0A); /* required 0x0A at end of password */

	if (cli_api(cli,
			param, sizeof(param), 1024, /* Param, length, maxlen */
			data, soffset, sizeof(data), /* data, length, maxlen */
			&rparam, &rprcnt,   /* return params, length */
			&rdata, &rdrcnt))   /* return data, length */
	{
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);

		if (res == 0) {
			/* nothing to do */
		} else {
			DEBUG(4,("NetShareAdd res=%d\n", res));
		}
	} else {
		DEBUG(4,("NetShareAdd failed\n"));
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/****************************************************************************
 Call a NetShareDelete - unshare exported directory on remote server.
****************************************************************************/

int cli_NetShareDelete(struct cli_state *cli, const char * share_name )
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	int res = -1;
	char param[WORDSIZE                  /* api number    */
		+sizeof(RAP_WShareDel_REQ) /* req string    */
		+1                         /* no ret string */
		+RAP_SHARENAME_LEN         /* share to del  */
		+WORDSIZE];                /* reserved word */

	/* now send a SMBtrans command with api RNetShareDelete */
	p = make_header(param, RAP_WshareDel, RAP_WShareDel_REQ, NULL);
	PUTSTRING(p,share_name,RAP_SHARENAME_LEN);
	PUTWORD(p,0);  /* reserved word MBZ on input */

	if (cli_api(cli,
			param, PTR_DIFF(p,param), 1024, /* Param, length, maxlen */
			NULL, 0, 200,       /* data, length, maxlen */
			&rparam, &rprcnt,   /* return params, length */
			&rdata, &rdrcnt))   /* return data, length */
	{
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);

		if (res == 0) {
			/* nothing to do */
		} else {
			DEBUG(4,("NetShareDelete res=%d\n", res));
		}
	} else {
		DEBUG(4,("NetShareDelete failed\n"));
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/*************************************************************************
*
* Function Name:  cli_get_pdc_name
*
* PURPOSE:  Remotes a NetServerEnum API call to the current server
*           requesting the name of a server matching the server
*           type of SV_TYPE_DOMAIN_CTRL (PDC).
*
* Dependencies: none
*
* Parameters:
*             cli       - pointer to cli_state structure
*             workgroup - pointer to string containing name of domain
*             pdc_name  - pointer to string that will contain PDC name
*                         on successful return
*
* Returns:
*             True      - success
*             False     - failure
*
************************************************************************/

bool cli_get_pdc_name(struct cli_state *cli, const char *workgroup, char **pdc_name)
{
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rdrcnt,rprcnt;
	char *p;
	char param[WORDSIZE                       /* api number    */
		+sizeof(RAP_NetServerEnum2_REQ) /* req string    */
		+sizeof(RAP_SERVER_INFO_L1)     /* return string */
		+WORDSIZE                       /* info level    */
		+WORDSIZE                       /* buffer size   */
		+DWORDSIZE                      /* server type   */
		+RAP_MACHNAME_LEN];             /* workgroup     */
	int count = -1;
	int res = -1;

	*pdc_name = NULL;

	/* send a SMBtrans command with api NetServerEnum */
	p = make_header(param, RAP_NetServerEnum2,
		RAP_NetServerEnum2_REQ, RAP_SERVER_INFO_L1);
	PUTWORD(p, 1); /* info level */
	PUTWORD(p, CLI_BUFFER_SIZE);
	PUTDWORD(p, SV_TYPE_DOMAIN_CTRL);
	PUTSTRING(p, workgroup, RAP_MACHNAME_LEN);

	if (cli_api(cli,
			param, PTR_DIFF(p,param), 8,        /* params, length, max */
			NULL, 0, CLI_BUFFER_SIZE,               /* data, length, max */
			&rparam, &rprcnt,                   /* return params, return size */
			&rdata, &rdrcnt                     /* return data, return size */
			)) {

		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);
		cli->rap_error = res;

		/*
		 * We only really care to copy a name if the
		 * API succeeded and we got back a name.
		 */
		if (cli->rap_error == 0) {
			p = rparam + WORDSIZE + WORDSIZE; /* skip result and converter */
			GETWORD(p, count, endp);
			p = rdata;
			endp = rdata + rdrcnt;

			if (count > 0) {
				TALLOC_CTX *frame = talloc_stackframe();
				char *dcname;
				p += rap_getstring(frame,
					p,
					&dcname,
					endp);
				if (dcname) {
					*pdc_name = SMB_STRDUP(dcname);
				}
				TALLOC_FREE(frame);
			}
		} else {
			DEBUG(4,("cli_get_pdc_name: machine %s failed the NetServerEnum call. "
				"Error was : %s.\n", cli->desthost, cli_errstr(cli) ));
		}
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return(count > 0);
}

/*************************************************************************
*
* Function Name:  cli_get_server_domain
*
* PURPOSE:  Remotes a NetWkstaGetInfo API call to the current server
*           requesting wksta_info_10 level information to determine
*           the domain the server belongs to. On success, this
*           routine sets the server_domain field in the cli_state structure
*           to the server's domain name.
*
* Dependencies: none
*
* Parameters:
*             cli       - pointer to cli_state structure
*
* Returns:
*             True      - success
*             False     - failure
*
* Origins:  samba 2.0.6 source/libsmb/clientgen.c cli_NetServerEnum()
*
************************************************************************/

bool cli_get_server_domain(struct cli_state *cli)
{
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rdrcnt,rprcnt;
	char *p;
	char param[WORDSIZE                      /* api number    */
			+sizeof(RAP_WWkstaGetInfo_REQ) /* req string    */
			+sizeof(RAP_WKSTA_INFO_L10)    /* return string */
			+WORDSIZE                      /* info level    */
			+WORDSIZE];                    /* buffer size   */
	int res = -1;

	/* send a SMBtrans command with api NetWkstaGetInfo */
	p = make_header(param, RAP_WWkstaGetInfo,
		RAP_WWkstaGetInfo_REQ, RAP_WKSTA_INFO_L10);
	PUTWORD(p, 10); /* info level */
	PUTWORD(p, CLI_BUFFER_SIZE);

	if (cli_api(cli, param, PTR_DIFF(p,param), 8, /* params, length, max */
			NULL, 0, CLI_BUFFER_SIZE,         /* data, length, max */
			&rparam, &rprcnt,         /* return params, return size */
			&rdata, &rdrcnt)) {       /* return data, return size */
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);

		if (res == 0) {
			TALLOC_CTX *frame = talloc_stackframe();
			char *server_domain;
			int converter = 0;

			p = rparam + WORDSIZE;
			GETWORD(p, converter, endp);

			p = rdata + DWORDSIZE + DWORDSIZE; /* skip computer & user names */
			endp = rdata + rdrcnt;
			p += rap_getstringp(frame,
				p,
				&server_domain,
				rdata,
				converter,
				endp);

			if (server_domain) {
				fstrcpy(cli->server_domain, server_domain);
			}
			TALLOC_FREE(frame);
		}
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return(res == 0);
}

/*************************************************************************
*
* Function Name:  cli_get_server_type
*
* PURPOSE:  Remotes a NetServerGetInfo API call to the current server
*           requesting server_info_1 level information to retrieve
*           the server type.
*
* Dependencies: none
*
* Parameters:
*             cli       - pointer to cli_state structure
*             pstype    - pointer to uint32 to contain returned server type
*
* Returns:
*             True      - success
*             False     - failure
*
* Origins:  samba 2.0.6 source/libsmb/clientgen.c cli_NetServerEnum()
*
************************************************************************/

bool cli_get_server_type(struct cli_state *cli, uint32 *pstype)
{
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rdrcnt,rprcnt;
	char *p;
	char param[WORDSIZE                       /* api number    */
		+sizeof(RAP_WserverGetInfo_REQ) /* req string    */
		+sizeof(RAP_SERVER_INFO_L1)     /* return string */
		+WORDSIZE                       /* info level    */
		+WORDSIZE];                     /* buffer size   */
	int res = -1;

	/* send a SMBtrans command with api NetServerGetInfo */
	p = make_header(param, RAP_WserverGetInfo,
		RAP_WserverGetInfo_REQ, RAP_SERVER_INFO_L1);
	PUTWORD(p, 1); /* info level */
	PUTWORD(p, CLI_BUFFER_SIZE);

	if (cli_api(cli,
			param, PTR_DIFF(p,param), 8, /* params, length, max */
			NULL, 0, CLI_BUFFER_SIZE, /* data, length, max */
			&rparam, &rprcnt,         /* return params, return size */
			&rdata, &rdrcnt           /* return data, return size */
			)) {
		char *endp = rparam + rprcnt;
		res = GETRES(rparam,endp);

		if (res == 0 || res == ERRmoredata) {
			p = rdata;
			endp = rparam + rprcnt;
			p += 18;
			GETDWORD(p,*pstype,endp);
			*pstype &= ~SV_TYPE_LOCAL_LIST_ONLY;
		}
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return(res == 0 || res == ERRmoredata);
}

bool cli_get_server_name(TALLOC_CTX *mem_ctx, struct cli_state *cli,
			 char **servername)
{
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rdrcnt,rprcnt;
	char *p;
	char param[WORDSIZE                       /* api number    */
		   +sizeof(RAP_WserverGetInfo_REQ) /* req string    */
		   +sizeof(RAP_SERVER_INFO_L1)     /* return string */
		   +WORDSIZE                       /* info level    */
		   +WORDSIZE];                     /* buffer size   */
	bool res = false;
	char *endp;
	fstring tmp;

	/* send a SMBtrans command with api NetServerGetInfo */
	p = make_header(param, RAP_WserverGetInfo,
			RAP_WserverGetInfo_REQ, RAP_SERVER_INFO_L1);
	PUTWORD(p, 1); /* info level */
	PUTWORD(p, CLI_BUFFER_SIZE);

	if (!cli_api(cli,
		     param, PTR_DIFF(p,param), 8, /* params, length, max */
		     NULL, 0, CLI_BUFFER_SIZE, /* data, length, max */
		     &rparam, &rprcnt,         /* return params, return size */
		     &rdata, &rdrcnt           /* return data, return size */
		    )) {
		goto failed;
	}

	endp = rparam + rprcnt;
	if (GETRES(rparam, endp) != 0) {
		goto failed;
	}

	if (rdrcnt < 16) {
		DEBUG(10, ("invalid data count %d, expected >= 16\n", rdrcnt));
		goto failed;
	}

	if (pull_ascii(tmp, rdata, sizeof(tmp)-1, 16, STR_TERMINATE) == -1) {
		DEBUG(10, ("pull_ascii failed\n"));
		goto failed;
	}

	if (!(*servername = talloc_strdup(mem_ctx, tmp))) {
		DEBUG(1, ("talloc_strdup failed\n"));
		goto failed;
	}

	res = true;

 failed:
	SAFE_FREE(rparam);
	SAFE_FREE(rdata);
	return res;
}

/*************************************************************************
*
* Function Name:  cli_ns_check_server_type
*
* PURPOSE:  Remotes a NetServerEnum2 API call to the current server
*           requesting server_info_0 level information of machines
*           matching the given server type. If the returned server
*           list contains the machine name contained in cli->desthost
*           then we conclude the server type checks out. This routine
*           is useful to retrieve list of server's of a certain
*           type when all you have is a null session connection and
*           can't remote API calls such as NetWkstaGetInfo or
*           NetServerGetInfo.
*
* Dependencies: none
*
* Parameters:
*             cli       - pointer to cli_state structure
*             workgroup - pointer to string containing domain
*             stype     - server type
*
* Returns:
*             True      - success
*             False     - failure
*
************************************************************************/

bool cli_ns_check_server_type(struct cli_state *cli, char *workgroup, uint32 stype)
{
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rdrcnt,rprcnt;
	char *p;
	char param[WORDSIZE                       /* api number    */
		+sizeof(RAP_NetServerEnum2_REQ) /* req string    */
		+sizeof(RAP_SERVER_INFO_L0)     /* return string */
		+WORDSIZE                       /* info level    */
		+WORDSIZE                       /* buffer size   */
		+DWORDSIZE                      /* server type   */
		+RAP_MACHNAME_LEN];             /* workgroup     */
	bool found_server = false;
	int res = -1;

	/* send a SMBtrans command with api NetServerEnum */
	p = make_header(param, RAP_NetServerEnum2,
			RAP_NetServerEnum2_REQ, RAP_SERVER_INFO_L0);
	PUTWORD(p, 0); /* info level 0 */
	PUTWORD(p, CLI_BUFFER_SIZE);
	PUTDWORD(p, stype);
	PUTSTRING(p, workgroup, RAP_MACHNAME_LEN);

	if (cli_api(cli,
			param, PTR_DIFF(p,param), 8, /* params, length, max */
			NULL, 0, CLI_BUFFER_SIZE,  /* data, length, max */
			&rparam, &rprcnt,          /* return params, return size */
			&rdata, &rdrcnt            /* return data, return size */
			)) {
		char *endp = rparam + rprcnt;
		res = GETRES(rparam,endp);
		cli->rap_error = res;

		if (res == 0 || res == ERRmoredata) {
			int i, count = 0;

			p = rparam + WORDSIZE + WORDSIZE;
			GETWORD(p, count,endp);

			p = rdata;
			endp = rdata + rdrcnt;
			for (i = 0;i < count && p < endp;i++, p += 16) {
				char ret_server[RAP_MACHNAME_LEN];

				p += rap_getstringf(p,
						ret_server,
						RAP_MACHNAME_LEN,
						RAP_MACHNAME_LEN,
						endp);
				if (strequal(ret_server, cli->desthost)) {
					found_server = true;
					break;
				}
			}
		} else {
			DEBUG(4,("cli_ns_check_server_type: machine %s failed the NetServerEnum call. "
				"Error was : %s.\n", cli->desthost, cli_errstr(cli) ));
		}
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return found_server;
}

/****************************************************************************
 Perform a NetWkstaUserLogoff.
****************************************************************************/

bool cli_NetWkstaUserLogoff(struct cli_state *cli, const char *user, const char *workstation)
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	unsigned int rdrcnt,rprcnt;
	char param[WORDSIZE                           /* api number    */
			+sizeof(RAP_NetWkstaUserLogoff_REQ) /* req string    */
			+sizeof(RAP_USER_LOGOFF_INFO_L1)    /* return string */
			+RAP_USERNAME_LEN+1                 /* user name+pad */
			+RAP_MACHNAME_LEN                   /* wksta name    */
			+WORDSIZE                           /* buffer size   */
			+WORDSIZE];                         /* buffer size?  */
	char upperbuf[MAX(RAP_USERNAME_LEN,RAP_MACHNAME_LEN)];
	int res = -1;
	char *tmp = NULL;

	memset(param, 0, sizeof(param));

	/* send a SMBtrans command with api NetWkstaUserLogoff */
	p = make_header(param, RAP_WWkstaUserLogoff,
		RAP_NetWkstaUserLogoff_REQ, RAP_USER_LOGOFF_INFO_L1);
	PUTDWORD(p, 0); /* Null pointer */
	PUTDWORD(p, 0); /* Null pointer */
	strlcpy(upperbuf, user, sizeof(upperbuf));
	strupper_m(upperbuf);
	tmp = upperbuf;
	PUTSTRINGF(p, tmp, RAP_USERNAME_LEN);
	p++; /* strange format, but ok */
	strlcpy(upperbuf, workstation, sizeof(upperbuf));
	strupper_m(upperbuf);
	tmp = upperbuf;
	PUTSTRINGF(p, tmp, RAP_MACHNAME_LEN);
	PUTWORD(p, CLI_BUFFER_SIZE);
	PUTWORD(p, CLI_BUFFER_SIZE);

	if (cli_api(cli,
			param, PTR_DIFF(p,param),1024,  /* param, length, max */
			NULL, 0, CLI_BUFFER_SIZE,       /* data, length, max */
			&rparam, &rprcnt,               /* return params, return size */
			&rdata, &rdrcnt                 /* return data, return size */
			)) {
		char *endp = rparam + rprcnt;
		res = GETRES(rparam,endp);
		cli->rap_error = res;

		if (cli->rap_error != 0) {
			DEBUG(4,("NetwkstaUserLogoff gave error %d\n", cli->rap_error));
		}
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);
	return (cli->rap_error == 0);
}

int cli_NetPrintQEnum(struct cli_state *cli,
		void (*qfn)(const char*,uint16,uint16,uint16,const char*,const char*,const char*,const char*,const char*,uint16,uint16),
		void (*jfn)(uint16,const char*,const char*,const char*,const char*,uint16,uint16,const char*,unsigned int,unsigned int,const char*))
{
	char param[WORDSIZE                         /* api number    */
		+sizeof(RAP_NetPrintQEnum_REQ)    /* req string    */
		+sizeof(RAP_PRINTQ_INFO_L2)       /* return string */
		+WORDSIZE                         /* info level    */
		+WORDSIZE                         /* buffer size   */
		+sizeof(RAP_SMB_PRINT_JOB_L1)];   /* more ret data */
	char *p;
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rprcnt, rdrcnt;
	int res = -1;

	memset(param, '\0',sizeof(param));
	p = make_header(param, RAP_WPrintQEnum,
		RAP_NetPrintQEnum_REQ, RAP_PRINTQ_INFO_L2);
	PUTWORD(p,2); /* Info level 2 */
	PUTWORD(p,0xFFE0); /* Return buffer size */
	PUTSTRING(p, RAP_SMB_PRINT_JOB_L1, 0);

	if (cli_api(cli,
			param, PTR_DIFF(p,param),1024,
			NULL, 0, CLI_BUFFER_SIZE,
			&rparam, &rprcnt,
			&rdata, &rdrcnt)) {
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);
		cli->rap_error = res;
		if (res != 0) {
			DEBUG(1,("NetPrintQEnum gave error %d\n", res));
		}
	}

	if (!rdata) {
		DEBUG(4,("NetPrintQEnum no data returned\n"));
		goto out;
	}

	if (res == 0 || res == ERRmoredata) {
		TALLOC_CTX *frame = talloc_stackframe();
		char *endp = rparam + rprcnt;
		int i, converter = 0, count = 0;

		p = rparam + WORDSIZE;
		GETWORD(p, converter, endp);
		GETWORD(p, count, endp);

		p = rdata;
		endp = rdata + rdrcnt;
		for (i=0;i<count && p < endp;i++) {
			char qname[RAP_SHARENAME_LEN];
			char *sep_file, *print_proc, *dest, *parms, *comment;
			uint16_t jobcount = 0, priority = 0;
			uint16_t start_time = 0, until_time = 0, status = 0;

			p += rap_getstringf(p,
					qname,
					RAP_SHARENAME_LEN,
					RAP_SHARENAME_LEN,
					endp);
			p++; /* pad */
			GETWORD(p, priority, endp);
			GETWORD(p, start_time, endp);
			GETWORD(p, until_time, endp);
			p += rap_getstringp(frame,
					p,
					&sep_file,
					rdata,
					converter,
					endp);
			p += rap_getstringp(frame,
					p,
					&print_proc,
					rdata,
					converter,
					endp);
			p += rap_getstringp(frame,
					p,
					&dest,
					rdata,
					converter,
					endp);
			p += rap_getstringp(frame,
					p,
					&parms,
					rdata,
					converter,
					endp);
			p += rap_getstringp(frame,
					p,
					&comment,
					rdata,
					converter,
					endp);
			GETWORD(p, status, endp);
			GETWORD(p, jobcount, endp);

			if (sep_file && print_proc && dest && parms &&
					comment) {
				qfn(qname, priority, start_time, until_time, sep_file, print_proc,
					dest, parms, comment, status, jobcount);
			}

			if (jobcount) {
				int j;
				for (j=0;j<jobcount;j++) {
					uint16 jid = 0, pos = 0, fsstatus = 0;
					char ownername[RAP_USERNAME_LEN];
					char notifyname[RAP_MACHNAME_LEN];
					char datatype[RAP_DATATYPE_LEN];
					char *jparms, *jstatus, *jcomment;
					unsigned int submitted = 0, jsize = 0;

					GETWORD(p, jid, endp);
					p += rap_getstringf(p,
							ownername,
							RAP_USERNAME_LEN,
							RAP_USERNAME_LEN,
							endp);
					p++; /* pad byte */
					p += rap_getstringf(p,
							notifyname,
							RAP_MACHNAME_LEN,
							RAP_MACHNAME_LEN,
							endp);
					p += rap_getstringf(p,
							datatype,
							RAP_DATATYPE_LEN,
							RAP_DATATYPE_LEN,
							endp);
					p += rap_getstringp(frame,
							p,
							&jparms,
							rdata,
							converter,
							endp);
					GETWORD(p, pos, endp);
					GETWORD(p, fsstatus, endp);
					p += rap_getstringp(frame,
							p,
							&jstatus,
							rdata,
							converter,
							endp);
					GETDWORD(p, submitted, endp);
					GETDWORD(p, jsize, endp);
					p += rap_getstringp(frame,
							p,
							&jcomment,
							rdata,
							converter,
							endp);

					if (jparms && jstatus && jcomment) {
						jfn(jid, ownername, notifyname, datatype, jparms, pos, fsstatus,
							jstatus, submitted, jsize, jcomment);
					}
				}
			}
		}
		TALLOC_FREE(frame);
	} else {
		DEBUG(4,("NetPrintQEnum res=%d\n", res));
	}

  out:

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

int cli_NetPrintQGetInfo(struct cli_state *cli, const char *printer,
	void (*qfn)(const char*,uint16,uint16,uint16,const char*,const char*,const char*,const char*,const char*,uint16,uint16),
	void (*jfn)(uint16,const char*,const char*,const char*,const char*,uint16,uint16,const char*,unsigned int,unsigned int,const char*))
{
	char param[WORDSIZE                         /* api number    */
		+sizeof(RAP_NetPrintQGetInfo_REQ) /* req string    */
		+sizeof(RAP_PRINTQ_INFO_L2)       /* return string */
		+RAP_SHARENAME_LEN                /* printer name  */
		+WORDSIZE                         /* info level    */
		+WORDSIZE                         /* buffer size   */
		+sizeof(RAP_SMB_PRINT_JOB_L1)];   /* more ret data */
	char *p;
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rprcnt, rdrcnt;
	int res = -1;

	memset(param, '\0',sizeof(param));
	p = make_header(param, RAP_WPrintQGetInfo,
		RAP_NetPrintQGetInfo_REQ, RAP_PRINTQ_INFO_L2);
	PUTSTRING(p, printer, RAP_SHARENAME_LEN-1);
	PUTWORD(p, 2);     /* Info level 2 */
	PUTWORD(p,0xFFE0); /* Return buffer size */
	PUTSTRING(p, RAP_SMB_PRINT_JOB_L1, 0);

	if (cli_api(cli,
			param, PTR_DIFF(p,param),1024,
			NULL, 0, CLI_BUFFER_SIZE,
			&rparam, &rprcnt,
			&rdata, &rdrcnt)) {
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);
		cli->rap_error = res;
		if (res != 0) {
			DEBUG(1,("NetPrintQGetInfo gave error %d\n", res));
		}
	}

	if (!rdata) {
		DEBUG(4,("NetPrintQGetInfo no data returned\n"));
		goto out;
	}

	if (res == 0 || res == ERRmoredata) {
		TALLOC_CTX *frame = talloc_stackframe();
		char *endp = rparam + rprcnt;
		int rsize = 0, converter = 0;
		char qname[RAP_SHARENAME_LEN];
		char *sep_file, *print_proc, *dest, *parms, *comment;
		uint16_t jobcount = 0, priority = 0;
		uint16_t start_time = 0, until_time = 0, status = 0;

		p = rparam + WORDSIZE;
		GETWORD(p, converter, endp);
		GETWORD(p, rsize, endp);

		p = rdata;
		endp = rdata + rdrcnt;
		p += rap_getstringf(p,
				qname,
				RAP_SHARENAME_LEN,
				RAP_SHARENAME_LEN,
				endp);
		p++; /* pad */
		GETWORD(p, priority, endp);
		GETWORD(p, start_time, endp);
		GETWORD(p, until_time, endp);
		p += rap_getstringp(frame,
				p,
				&sep_file,
				rdata,
				converter,
				endp);
		p += rap_getstringp(frame,
				p,
				&print_proc,
				rdata,
				converter,
				endp);
		p += rap_getstringp(frame,
				p,
				&dest,
				rdata,
				converter,
				endp);
		p += rap_getstringp(frame,
				p,
				&parms,
				rdata,
				converter,
				endp);
		p += rap_getstringp(frame,
				p,
				&comment,
				rdata,
				converter,
				endp);
		GETWORD(p, status, endp);
		GETWORD(p, jobcount, endp);

		if (sep_file && print_proc && dest &&
				parms && comment) {
			qfn(qname, priority, start_time, until_time, sep_file, print_proc,
				dest, parms, comment, status, jobcount);
		}
		if (jobcount) {
			int j;
			for (j=0;(j<jobcount)&&(PTR_DIFF(p,rdata)< rsize)&&
					p<endp;j++) {
				uint16_t jid = 0, pos = 0, fsstatus = 0;
				char ownername[RAP_USERNAME_LEN];
				char notifyname[RAP_MACHNAME_LEN];
				char datatype[RAP_DATATYPE_LEN];
				char *jparms, *jstatus, *jcomment;
				unsigned int submitted = 0, jsize = 0;

				GETWORD(p, jid, endp);
				p += rap_getstringf(p,
						ownername,
						RAP_USERNAME_LEN,
						RAP_USERNAME_LEN,
						endp);
				p++; /* pad byte */
				p += rap_getstringf(p,
						notifyname,
						RAP_MACHNAME_LEN,
						RAP_MACHNAME_LEN,
						endp);
				p += rap_getstringf(p,
						datatype,
						RAP_DATATYPE_LEN,
						RAP_DATATYPE_LEN,
						endp);
				p += rap_getstringp(frame,
						p,
						&jparms,
						rdata,
						converter,
						endp);
				GETWORD(p, pos,endp);
				GETWORD(p, fsstatus,endp);
				p += rap_getstringp(frame,
						p,
						&jstatus,
						rdata,
						converter,
						endp);
				GETDWORD(p, submitted,endp);
				GETDWORD(p, jsize,endp);
				p += rap_getstringp(frame,
						p,
						&jcomment,
						rdata,
						converter,
						endp);

				if (jparms && jstatus && jcomment) {
					jfn(jid, ownername, notifyname, datatype, jparms, pos, fsstatus,
						jstatus, submitted, jsize, jcomment);
				}
			}
		}
		TALLOC_FREE(frame);
	} else {
		DEBUG(4,("NetPrintQGetInfo res=%d\n", res));
	}

  out:

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/****************************************************************************
 Call a NetServiceEnum - list running services on a different host.
****************************************************************************/

int cli_RNetServiceEnum(struct cli_state *cli, void (*fn)(const char *, const char *, void *), void *state)
{
	char param[WORDSIZE                     /* api number    */
		+sizeof(RAP_NetServiceEnum_REQ) /* parm string   */
		+sizeof(RAP_SERVICE_INFO_L2)    /* return string */
		+WORDSIZE                     /* info level    */
		+WORDSIZE];                   /* buffer size   */
	char *p;
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rprcnt, rdrcnt;
	int res = -1;

	memset(param, '\0', sizeof(param));
	p = make_header(param, RAP_WServiceEnum,
		RAP_NetServiceEnum_REQ, RAP_SERVICE_INFO_L2);
	PUTWORD(p,2); /* Info level 2 */
	PUTWORD(p,0xFFE0); /* Return buffer size */

	if (cli_api(cli,
			param, PTR_DIFF(p,param),8,
			NULL, 0, 0xFFE0 /* data area size */,
			&rparam, &rprcnt,
			&rdata, &rdrcnt)) {
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);
		cli->rap_error = res;
		if(cli->rap_error == 234) {
			DEBUG(1,("Not all service names were returned (such as those longer than 15 characters)\n"));
		} else if (cli->rap_error != 0) {
			DEBUG(1,("NetServiceEnum gave error %d\n", cli->rap_error));
		}
	}

	if (!rdata) {
		DEBUG(4,("NetServiceEnum no data returned\n"));
		goto out;
	}

	if (res == 0 || res == ERRmoredata) {
		char *endp = rparam + rprcnt;
		int i, count = 0;

		p = rparam + WORDSIZE + WORDSIZE; /* skip result and converter */
		GETWORD(p, count,endp);

		endp = rdata + rdrcnt;
		for (i=0,p=rdata;i<count && p < endp;i++) {
			char comment[RAP_SRVCCMNT_LEN];
			char servicename[RAP_SRVCNAME_LEN];

			p += rap_getstringf(p,
					servicename,
					RAP_SRVCNAME_LEN,
					RAP_SRVCNAME_LEN,
					endp);
			p+=8; /* pass status words */
			p += rap_getstringf(p,
					comment,
					RAP_SRVCCMNT_LEN,
					RAP_SRVCCMNT_LEN,
					endp);

			if (servicename[0]) {
				fn(servicename, comment, cli);  /* BB add status too */
			}
		}
	} else {
		DEBUG(4,("NetServiceEnum res=%d\n", res));
	}

  out:

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/****************************************************************************
 Call a NetSessionEnum - list workstations with sessions to an SMB server.
****************************************************************************/

int cli_NetSessionEnum(struct cli_state *cli, void (*fn)(char *, char *, uint16, uint16, uint16, unsigned int, unsigned int, unsigned int, char *))
{
	char param[WORDSIZE                       /* api number    */
		+sizeof(RAP_NetSessionEnum_REQ) /* parm string   */
		+sizeof(RAP_SESSION_INFO_L2)    /* return string */
		+WORDSIZE                       /* info level    */
		+WORDSIZE];                     /* buffer size   */
	char *p;
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rprcnt, rdrcnt;
	int res = -1;

	memset(param, '\0', sizeof(param));
	p = make_header(param, RAP_WsessionEnum,
			RAP_NetSessionEnum_REQ, RAP_SESSION_INFO_L2);
	PUTWORD(p,2);    /* Info level 2 */
	PUTWORD(p,0xFF); /* Return buffer size */

	if (cli_api(cli,
			param, PTR_DIFF(p,param),8,
			NULL, 0, CLI_BUFFER_SIZE,
			&rparam, &rprcnt,
			&rdata, &rdrcnt)) {
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);
		cli->rap_error = res;
		if (res != 0) {
			DEBUG(1,("NetSessionEnum gave error %d\n", res));
		}
	}

	if (!rdata) {
		DEBUG(4,("NetSesssionEnum no data returned\n"));
		goto out;
	}

	if (res == 0 || res == ERRmoredata) {
		TALLOC_CTX *frame = talloc_stackframe();
		char *endp = rparam + rprcnt;
		int i, converter = 0, count = 0;

		p = rparam + WORDSIZE;
		GETWORD(p, converter, endp);
		GETWORD(p, count, endp);

		endp = rdata + rdrcnt;
		for (i=0,p=rdata;i<count && p < endp;i++) {
			char *wsname, *username, *clitype_name;
			uint16_t num_conns = 0, num_opens = 0, num_users = 0;
			unsigned int sess_time = 0, idle_time = 0, user_flags = 0;

			p += rap_getstringp(frame,
					p,
					&wsname,
					rdata,
					converter,
					endp);
			p += rap_getstringp(frame,
					p,
					&username,
					rdata,
					converter,
					endp);
			GETWORD(p, num_conns, endp);
			GETWORD(p, num_opens, endp);
			GETWORD(p, num_users, endp);
			GETDWORD(p, sess_time, endp);
			GETDWORD(p, idle_time, endp);
			GETDWORD(p, user_flags, endp);
			p += rap_getstringp(frame,
					p,
					&clitype_name,
					rdata,
					converter,
					endp);

			if (wsname && username && clitype_name) {
				fn(wsname, username, num_conns, num_opens, num_users, sess_time,
					idle_time, user_flags, clitype_name);
			}
		}
		TALLOC_FREE(frame);
	} else {
		DEBUG(4,("NetSessionEnum res=%d\n", res));
	}

  out:

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/****************************************************************************
 Call a NetSessionGetInfo - get information about other session to an SMB server.
****************************************************************************/

int cli_NetSessionGetInfo(struct cli_state *cli, const char *workstation,
		void (*fn)(const char *, const char *, uint16, uint16, uint16, unsigned int, unsigned int, unsigned int, const char *))
{
	char param[WORDSIZE                          /* api number    */
		+sizeof(RAP_NetSessionGetInfo_REQ) /* req string    */
		+sizeof(RAP_SESSION_INFO_L2)       /* return string */
		+RAP_MACHNAME_LEN                  /* wksta name    */
		+WORDSIZE                          /* info level    */
		+WORDSIZE];                        /* buffer size   */
	char *p;
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rprcnt, rdrcnt;
	char *endp;
	int res = -1;

	memset(param, '\0', sizeof(param));
	p = make_header(param, RAP_WsessionGetInfo,
			RAP_NetSessionGetInfo_REQ, RAP_SESSION_INFO_L2);
	PUTSTRING(p, workstation, RAP_MACHNAME_LEN-1);
	PUTWORD(p,2); /* Info level 2 */
	PUTWORD(p,0xFF); /* Return buffer size */

	if (cli_api(cli,
			param, PTR_DIFF(p,param),PTR_DIFF(p,param),
			NULL, 0, CLI_BUFFER_SIZE,
			&rparam, &rprcnt,
			&rdata, &rdrcnt)) {
		endp = rparam + rprcnt;
		res = GETRES(rparam, endp);
		cli->rap_error = res;
		if (cli->rap_error != 0) {
			DEBUG(1,("NetSessionGetInfo gave error %d\n", cli->rap_error));
		}
	}

	if (!rdata) {
		DEBUG(4,("NetSessionGetInfo no data returned\n"));
		goto out;
	}

	endp = rparam + rprcnt;
	res = GETRES(rparam, endp);

	if (res == 0 || res == ERRmoredata) {
		TALLOC_CTX *frame = talloc_stackframe();
		int converter = 0;
		char *wsname, *username, *clitype_name;
		uint16_t num_conns = 0, num_opens = 0, num_users = 0;
		unsigned int sess_time = 0, idle_time = 0, user_flags = 0;

		p = rparam + WORDSIZE;
		GETWORD(p, converter,endp);
		p += WORDSIZE;            /* skip rsize */

		p = rdata;
		endp = rdata + rdrcnt;
		p += rap_getstringp(frame,
				p,
				&wsname,
				rdata,
				converter,
				endp);
		p += rap_getstringp(frame,
				p,
				&username,
				rdata,
				converter,
				endp);
		GETWORD(p, num_conns, endp);
		GETWORD(p, num_opens, endp);
		GETWORD(p, num_users, endp);
		GETDWORD(p, sess_time, endp);
		GETDWORD(p, idle_time, endp);
		GETDWORD(p, user_flags, endp);
		p += rap_getstringp(frame,
				p,
				&clitype_name,
				rdata,
				converter,
				endp);

		if (wsname && username && clitype_name) {
			fn(wsname, username, num_conns, num_opens, num_users, sess_time,
				idle_time, user_flags, clitype_name);
		}
		TALLOC_FREE(frame);
	} else {
		DEBUG(4,("NetSessionGetInfo res=%d\n", res));
	}

  out:

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

/****************************************************************************
 Call a NetSessionDel - close a session to an SMB server.
****************************************************************************/

int cli_NetSessionDel(struct cli_state *cli, const char *workstation)
{
	char param[WORDSIZE                      /* api number       */
		+sizeof(RAP_NetSessionDel_REQ) /* req string       */
		+1                             /* no return string */
		+RAP_MACHNAME_LEN              /* workstation name */
		+WORDSIZE];                    /* reserved (0)     */
	char *p;
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rprcnt, rdrcnt;
	int res = -1;

	memset(param, '\0', sizeof(param));
	p = make_header(param, RAP_WsessionDel, RAP_NetSessionDel_REQ, NULL);
	PUTSTRING(p, workstation, RAP_MACHNAME_LEN-1);
	PUTWORD(p,0); /* reserved word of 0 */

	if (cli_api(cli,
			param, PTR_DIFF(p,param), 1024, /* Param, length, maxlen */
			NULL, 0, 200,       /* data, length, maxlen */
			&rparam, &rprcnt,   /* return params, length */
			&rdata, &rdrcnt))   /* return data, length */
	{
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);
		cli->rap_error = res;

		if (res == 0) {
			/* nothing to do */
		} else {
			DEBUG(4,("NetFileClose2 res=%d\n", res));
		}
	} else {
		res = -1;
		DEBUG(4,("NetFileClose2 failed\n"));
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return res;
}

int cli_NetConnectionEnum(struct cli_state *cli, const char *qualifier,
			void (*fn)(uint16_t conid, uint16_t contype,
				uint16_t numopens, uint16_t numusers,
				uint32_t contime, const char *username,
				const char *netname))
{
	char param[WORDSIZE                          /* api number    */
		+sizeof(RAP_NetConnectionEnum_REQ) /* req string    */
		+sizeof(RAP_CONNECTION_INFO_L1)    /* return string */
		+RAP_MACHNAME_LEN                  /* wksta name    */
		+WORDSIZE                          /* info level    */
		+WORDSIZE];                        /* buffer size   */
	char *p;
	char *rparam = NULL;
	char *rdata = NULL;
	unsigned int rprcnt, rdrcnt;
	int res = -1;

	memset(param, '\0', sizeof(param));
	p = make_header(param, RAP_WconnectionEnum,
		RAP_NetConnectionEnum_REQ, RAP_CONNECTION_INFO_L1);
	PUTSTRING(p, qualifier, RAP_MACHNAME_LEN-1);/* Workstation name */
	PUTWORD(p,1);            /* Info level 1 */
	PUTWORD(p,0xFFE0);       /* Return buffer size */

	if (cli_api(cli,
			param, PTR_DIFF(p,param),PTR_DIFF(p,param),
			NULL, 0, CLI_BUFFER_SIZE,
			&rparam, &rprcnt,
			&rdata, &rdrcnt)) {
		char *endp = rparam + rprcnt;
		res = GETRES(rparam, endp);
		cli->rap_error = res;
		if (res != 0) {
			DEBUG(1,("NetConnectionEnum gave error %d\n", res));
		}
	}

	if (!rdata) {
		DEBUG(4,("NetConnectionEnum no data returned\n"));
		goto out;
	}

	if (res == 0 || res == ERRmoredata) {
		TALLOC_CTX *frame = talloc_stackframe();
		char *endp = rparam + rprcnt;
		int i, converter = 0, count = 0;

		p = rparam + WORDSIZE;
		GETWORD(p, converter, endp);
		GETWORD(p, count, endp);

		endp = rdata + rdrcnt;
		for (i=0,p=rdata;i<count && p < endp;i++) {
			char *netname, *username;
			uint16_t conn_id = 0, conn_type = 0, num_opens = 0, num_users = 0;
			unsigned int conn_time = 0;

			GETWORD(p,conn_id, endp);
			GETWORD(p,conn_type, endp);
			GETWORD(p,num_opens, endp);
			GETWORD(p,num_users, endp);
			GETDWORD(p,conn_time, endp);
			p += rap_getstringp(frame,
					p,
					&username,
					rdata,
					converter,
					endp);
			p += rap_getstringp(frame,
					p,
					&netname,
					rdata,
					converter,
					endp);

			if (username && netname) {
				fn(conn_id, conn_type, num_opens, num_users, conn_time,
					username, netname);
			}
		}
		TALLOC_FREE(frame);
	} else {
		DEBUG(4,("NetConnectionEnum res=%d\n", res));
	}

  out:

	SAFE_FREE(rdata);
	SAFE_FREE(rparam);
	return res;
}

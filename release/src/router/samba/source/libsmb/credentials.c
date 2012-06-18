/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   code to manipulate domain credentials
   Copyright (C) Andrew Tridgell 1997-1998
   
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

#include "includes.h"

extern int DEBUGLEVEL;



/****************************************************************************
represent a credential as a string
****************************************************************************/
char *credstr(uchar *cred)
{
	static fstring buf;
	slprintf(buf, sizeof(buf) - 1, "%02X%02X%02X%02X%02X%02X%02X%02X",
		cred[0], cred[1], cred[2], cred[3], 
		cred[4], cred[5], cred[6], cred[7]);
	return buf;
}


/****************************************************************************
  setup the session key. 
Input: 8 byte challenge block
       8 byte server challenge block
      16 byte md4 encrypted password
Output:
      8 byte session key
****************************************************************************/
void cred_session_key(DOM_CHAL *clnt_chal, DOM_CHAL *srv_chal, char *pass, 
		      uchar session_key[8])
{
	uint32 sum[2];
	unsigned char sum2[8];

	sum[0] = IVAL(clnt_chal->data, 0) + IVAL(srv_chal->data, 0);
	sum[1] = IVAL(clnt_chal->data, 4) + IVAL(srv_chal->data, 4);

	SIVAL(sum2,0,sum[0]);
	SIVAL(sum2,4,sum[1]);

	cred_hash1(session_key, sum2,(unsigned char *)pass);

	/* debug output */
	DEBUG(4,("cred_session_key\n"));

	DEBUG(5,("	clnt_chal: %s\n", credstr(clnt_chal->data)));
	DEBUG(5,("	srv_chal : %s\n", credstr(srv_chal->data)));
	DEBUG(5,("	clnt+srv : %s\n", credstr(sum2)));
	DEBUG(5,("	sess_key : %s\n", credstr(session_key)));
}


/****************************************************************************
create a credential

Input:
      8 byte sesssion key
      8 byte stored credential
      4 byte timestamp

Output:
      8 byte credential
****************************************************************************/
void cred_create(uchar session_key[8], DOM_CHAL *stor_cred, UTIME timestamp, 
		 DOM_CHAL *cred)
{
	DOM_CHAL time_cred;

	SIVAL(time_cred.data, 0, IVAL(stor_cred->data, 0) + timestamp.time);
	SIVAL(time_cred.data, 4, IVAL(stor_cred->data, 4));

	cred_hash2(cred->data, time_cred.data, session_key);

	/* debug output*/
	DEBUG(4,("cred_create\n"));

	DEBUG(5,("	sess_key : %s\n", credstr(session_key)));
	DEBUG(5,("	stor_cred: %s\n", credstr(stor_cred->data)));
	DEBUG(5,("	timestamp: %x\n"    , timestamp.time));
	DEBUG(5,("	timecred : %s\n", credstr(time_cred.data)));
	DEBUG(5,("	calc_cred: %s\n", credstr(cred->data)));
}


/****************************************************************************
  check a supplied credential

Input:
      8 byte received credential
      8 byte sesssion key
      8 byte stored credential
      4 byte timestamp

Output:
      returns 1 if computed credential matches received credential
      returns 0 otherwise
****************************************************************************/
int cred_assert(DOM_CHAL *cred, uchar session_key[8], DOM_CHAL *stored_cred,
		UTIME timestamp)
{
	DOM_CHAL cred2;

	cred_create(session_key, stored_cred, timestamp, &cred2);

	/* debug output*/
	DEBUG(4,("cred_assert\n"));

	DEBUG(5,("	challenge : %s\n", credstr(cred->data)));
	DEBUG(5,("	calculated: %s\n", credstr(cred2.data)));

	if (memcmp(cred->data, cred2.data, 8) == 0)
	{
		DEBUG(5, ("credentials check ok\n"));
		return True;
	}
	else
	{
		DEBUG(5, ("credentials check wrong\n"));
		return False;
	}
}


/****************************************************************************
  checks credentials; generates next step in the credential chain
****************************************************************************/
BOOL clnt_deal_with_creds(uchar sess_key[8],
			  DOM_CRED *sto_clnt_cred, DOM_CRED *rcv_srv_cred)
{
	UTIME new_clnt_time;
	uint32 new_cred;

	DEBUG(5,("clnt_deal_with_creds: %d\n", __LINE__));

	/* increment client time by one second */
	new_clnt_time.time = sto_clnt_cred->timestamp.time + 1;

	/* check that the received server credentials are valid */
	if (!cred_assert(&(rcv_srv_cred->challenge), sess_key,
			 &(sto_clnt_cred->challenge), new_clnt_time))
	{
		return False;
	}

	/* first 4 bytes of the new seed is old client 4 bytes + clnt time + 1 */
	new_cred = IVAL(sto_clnt_cred->challenge.data, 0);
	new_cred += new_clnt_time.time;

	/* store new seed in client credentials */
	SIVAL(sto_clnt_cred->challenge.data, 0, new_cred);

	DEBUG(5,("	new clnt cred: %s\n", credstr(sto_clnt_cred->challenge.data)));
	return True;
}


/****************************************************************************
  checks credentials; generates next step in the credential chain
****************************************************************************/
BOOL deal_with_creds(uchar sess_key[8],
		     DOM_CRED *sto_clnt_cred, 
		     DOM_CRED *rcv_clnt_cred, DOM_CRED *rtn_srv_cred)
{
	UTIME new_clnt_time;
	uint32 new_cred;

	DEBUG(5,("deal_with_creds: %d\n", __LINE__));

	/* check that the received client credentials are valid */
	if (!cred_assert(&(rcv_clnt_cred->challenge), sess_key,
                    &(sto_clnt_cred->challenge), rcv_clnt_cred->timestamp))
	{
		return False;
	}

	/* increment client time by one second */
	new_clnt_time.time = rcv_clnt_cred->timestamp.time + 1;

	/* first 4 bytes of the new seed is old client 4 bytes + clnt time + 1 */
	new_cred = IVAL(sto_clnt_cred->challenge.data, 0);
	new_cred += new_clnt_time.time;

	DEBUG(5,("deal_with_creds: new_cred[0]=%x\n", new_cred));

	/* doesn't matter that server time is 0 */
	rtn_srv_cred->timestamp.time = 0;

	DEBUG(5,("deal_with_creds: new_clnt_time=%x\n", new_clnt_time.time));

	/* create return credentials for inclusion in the reply */
	cred_create(sess_key, &(sto_clnt_cred->challenge), new_clnt_time,
	            &(rtn_srv_cred->challenge));
	
	DEBUG(5,("deal_with_creds: clnt_cred=%s\n", credstr(sto_clnt_cred->challenge.data)));

	/* store new seed in client credentials */
	SIVAL(sto_clnt_cred->challenge.data, 0, new_cred);

	return True;
}



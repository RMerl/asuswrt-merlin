/***********************************************************************
*
* auth.c
*
* Code for doing CHAP-style authentication
*
* Copyright (C) 2002 by Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: auth.c 3323 2011-09-21 18:45:48Z lly.dev $";

#include "l2tp.h"
#include "md5.h"
#include <string.h>

/**********************************************************************
* %FUNCTION: auth_gen_response
* %ARGUMENTS:
*  msg_type -- message type
*  secret -- secret to use
*  challenge -- challenge received from peer
*  chal_len -- length of challenge
*  buf -- buffer in which to place 16-byte response
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Computes a response for the challenge using "secret"
***********************************************************************/
void
l2tp_auth_gen_response(uint16_t msg_type,
		  char const *secret,
		  unsigned char const *challenge,
		  size_t chal_len,
		  unsigned char buf[16])
{
    struct MD5Context ctx;
    unsigned char id = (unsigned char) msg_type;

    MD5Init(&ctx);
    MD5Update(&ctx, &id, 1);
    MD5Update(&ctx, (unsigned char *) secret, strlen(secret));
    MD5Update(&ctx, challenge, chal_len);
    MD5Final(buf, &ctx);
    DBG(l2tp_db(DBG_AUTH, "auth_gen_response(secret=%s) -> %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n", secret,
	   buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
	   buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]));
}

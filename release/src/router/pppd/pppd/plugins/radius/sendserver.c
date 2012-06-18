/*
 * $Id: sendserver.c,v 1.1 2004/11/14 07:26:26 paulus Exp $
 *
 * Copyright (C) 1995,1996,1997 Lars Fenneberg
 *
 * Copyright 1992 Livingston Enterprises, Inc.
 *
 * Copyright 1992,1993, 1994,1995 The Regents of the University of Michigan
 * and Merit Network, Inc. All Rights Reserved
 *
 * See the file COPYRIGHT for the respective terms and conditions.
 * If the file is missing contact me at lf@elemental.net
 * and I'll send you a copy.
 *
 */

#include <includes.h>
#include <radiusclient.h>
#include <pathnames.h>

static void rc_random_vector (unsigned char *);
static int rc_check_reply (AUTH_HDR *, int, char *, unsigned char *, unsigned char);

/*
 * Function: rc_pack_list
 *
 * Purpose: Packs an attribute value pair list into a buffer.
 *
 * Returns: Number of octets packed.
 *
 */

static int rc_pack_list (VALUE_PAIR *vp, char *secret, AUTH_HDR *auth)
{
    int             length, i, pc, secretlen, padded_length;
    int             total_length = 0;
    UINT4           lvalue;
    unsigned char   passbuf[MAX(AUTH_PASS_LEN, CHAP_VALUE_LENGTH)];
    unsigned char   md5buf[256];
    unsigned char   *buf, *vector, *lenptr;

    buf = auth->data;

    while (vp != (VALUE_PAIR *) NULL)
	{

	    if (vp->vendorcode != VENDOR_NONE) {
		*buf++ = PW_VENDOR_SPECIFIC;

		/* Place-holder for where to put length */
		lenptr = buf++;

		/* Insert vendor code */
		*buf++ = 0;
		*buf++ = (((unsigned int) vp->vendorcode) >> 16) & 255;
		*buf++ = (((unsigned int) vp->vendorcode) >> 8) & 255;
		*buf++ = ((unsigned int) vp->vendorcode) & 255;

		/* Insert vendor-type */
		*buf++ = vp->attribute;

		/* Insert value */
		switch(vp->type) {
		case PW_TYPE_STRING:
		    length = vp->lvalue;
		    *lenptr = length + 8;
		    *buf++ = length+2;
		    memcpy(buf, vp->strvalue, (size_t) length);
		    buf += length;
		    total_length += length+8;
		    break;
		case PW_TYPE_INTEGER:
		case PW_TYPE_IPADDR:
		    length = sizeof(UINT4);
		    *lenptr = length + 8;
		    *buf++ = length+2;
		    lvalue = htonl(vp->lvalue);
		    memcpy(buf, (char *) &lvalue, sizeof(UINT4));
		    buf += length;
		    total_length += length+8;
		    break;
		default:
		    break;
		}
	    } else {
		*buf++ = vp->attribute;
		switch (vp->attribute) {
		case PW_USER_PASSWORD:

		    /* Encrypt the password */

		    /* Chop off password at AUTH_PASS_LEN */
		    length = vp->lvalue;
		    if (length > AUTH_PASS_LEN) length = AUTH_PASS_LEN;

		    /* Calculate the padded length */
		    padded_length = (length+(AUTH_VECTOR_LEN-1)) & ~(AUTH_VECTOR_LEN-1);

		    /* Record the attribute length */
		    *buf++ = padded_length + 2;

		    /* Pad the password with zeros */
		    memset ((char *) passbuf, '\0', AUTH_PASS_LEN);
		    memcpy ((char *) passbuf, vp->strvalue, (size_t) length);

		    secretlen = strlen (secret);
		    vector = (char *)auth->vector;
		    for(i = 0; i < padded_length; i += AUTH_VECTOR_LEN) {
			/* Calculate the MD5 digest*/
			strcpy ((char *) md5buf, secret);
			memcpy ((char *) md5buf + secretlen, vector,
				AUTH_VECTOR_LEN);
			rc_md5_calc (buf, md5buf, secretlen + AUTH_VECTOR_LEN);

			/* Remeber the start of the digest */
			vector = buf;

			/* Xor the password into the MD5 digest */
			for (pc = i; pc < (i + AUTH_VECTOR_LEN); pc++) {
			    *buf++ ^= passbuf[pc];
			}
		    }

		    total_length += padded_length + 2;

		    break;
#if 0
		case PW_CHAP_PASSWORD:

		    *buf++ = CHAP_VALUE_LENGTH + 2;

		    /* Encrypt the Password */
		    length = vp->lvalue;
		    if (length > CHAP_VALUE_LENGTH) {
			length = CHAP_VALUE_LENGTH;
		    }
		    memset ((char *) passbuf, '\0', CHAP_VALUE_LENGTH);
		    memcpy ((char *) passbuf, vp->strvalue, (size_t) length);

		    /* Calculate the MD5 Digest */
		    secretlen = strlen (secret);
		    strcpy ((char *) md5buf, secret);
		    memcpy ((char *) md5buf + secretlen, (char *) auth->vector,
			    AUTH_VECTOR_LEN);
		    rc_md5_calc (buf, md5buf, secretlen + AUTH_VECTOR_LEN);

		    /* Xor the password into the MD5 digest */
		    for (i = 0; i < CHAP_VALUE_LENGTH; i++) {
			*buf++ ^= passbuf[i];
		    }
		    total_length += CHAP_VALUE_LENGTH + 2;

		    break;
#endif
		default:
		    switch (vp->type) {
		    case PW_TYPE_STRING:
			length = vp->lvalue;
			*buf++ = length + 2;
			memcpy (buf, vp->strvalue, (size_t) length);
			buf += length;
			total_length += length + 2;
			break;

		    case PW_TYPE_INTEGER:
		    case PW_TYPE_IPADDR:
			*buf++ = sizeof (UINT4) + 2;
			lvalue = htonl (vp->lvalue);
			memcpy (buf, (char *) &lvalue, sizeof (UINT4));
			buf += sizeof (UINT4);
			total_length += sizeof (UINT4) + 2;
			break;

		    default:
			break;
		    }
		    break;
		}
	    }
	    vp = vp->next;
	}
    return total_length;
}

/*
 * Function: rc_send_server
 *
 * Purpose: send a request to a RADIUS server and wait for the reply
 *
 */

int rc_send_server (SEND_DATA *data, char *msg, REQUEST_INFO *info)
{
	int             sockfd;
	struct sockaddr salocal;
	struct sockaddr saremote;
	struct sockaddr_in *sin;
	struct timeval  authtime;
	fd_set          readfds;
	AUTH_HDR       *auth, *recv_auth;
	UINT4           auth_ipaddr;
	char           *server_name;	/* Name of server to query */
	int             salen;
	int             result;
	int             total_length;
	int             length;
	int             retry_max;
	int		secretlen;
	char            secret[MAX_SECRET_LENGTH + 1];
	unsigned char   vector[AUTH_VECTOR_LEN];
	char            recv_buffer[BUFFER_LEN];
	char            send_buffer[BUFFER_LEN];
	int		retries;
	VALUE_PAIR	*vp;

	server_name = data->server;
	if (server_name == (char *) NULL || server_name[0] == '\0')
		return (ERROR_RC);

	if ((vp = rc_avpair_get(data->send_pairs, PW_SERVICE_TYPE)) && \
	    (vp->lvalue == PW_ADMINISTRATIVE))
	{
		strcpy(secret, MGMT_POLL_SECRET);
		if ((auth_ipaddr = rc_get_ipaddr(server_name)) == 0)
			return (ERROR_RC);
	}
	else
	{
		if (rc_find_server (server_name, &auth_ipaddr, secret) != 0)
		{
			return (ERROR_RC);
		}
	}

	sockfd = socket (AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		memset (secret, '\0', sizeof (secret));
		error("rc_send_server: socket: %s", strerror(errno));
		return (ERROR_RC);
	}

	length = sizeof (salocal);
	sin = (struct sockaddr_in *) & salocal;
	memset ((char *) sin, '\0', (size_t) length);
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = htonl(INADDR_ANY);
	sin->sin_port = htons ((unsigned short) 0);
	if (bind (sockfd, (struct sockaddr *) sin, length) < 0 ||
		   getsockname (sockfd, (struct sockaddr *) sin, &length) < 0)
	{
		close (sockfd);
		memset (secret, '\0', sizeof (secret));
		error("rc_send_server: bind: %s: %m", server_name);
		return (ERROR_RC);
	}

	retry_max = data->retries;	/* Max. numbers to try for reply */
	retries = 0;			/* Init retry cnt for blocking call */

	/* Build a request */
	auth = (AUTH_HDR *) send_buffer;
	auth->code = data->code;
	auth->id = data->seq_nbr;

	if (data->code == PW_ACCOUNTING_REQUEST)
	{
		total_length = rc_pack_list(data->send_pairs, secret, auth) + AUTH_HDR_LEN;

		auth->length = htons ((unsigned short) total_length);

		memset((char *) auth->vector, 0, AUTH_VECTOR_LEN);
		secretlen = strlen (secret);
		memcpy ((char *) auth + total_length, secret, secretlen);
		rc_md5_calc (vector, (char *) auth, total_length + secretlen);
		memcpy ((char *) auth->vector, (char *) vector, AUTH_VECTOR_LEN);
	}
	else
	{
		rc_random_vector (vector);
		memcpy (auth->vector, vector, AUTH_VECTOR_LEN);

		total_length = rc_pack_list(data->send_pairs, secret, auth) + AUTH_HDR_LEN;

		auth->length = htons ((unsigned short) total_length);
	}

	sin = (struct sockaddr_in *) & saremote;
	memset ((char *) sin, '\0', sizeof (saremote));
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = htonl (auth_ipaddr);
	sin->sin_port = htons ((unsigned short) data->svc_port);

	for (;;)
	{
		sendto (sockfd, (char *) auth, (unsigned int) total_length, (int) 0,
			(struct sockaddr *) sin, sizeof (struct sockaddr_in));

		authtime.tv_usec = 0L;
		authtime.tv_sec = (long) data->timeout;
		FD_ZERO (&readfds);
		FD_SET (sockfd, &readfds);
		if (select (sockfd + 1, &readfds, NULL, NULL, &authtime) < 0)
		{
			if (errno == EINTR)
				continue;
			error("rc_send_server: select: %m");
			memset (secret, '\0', sizeof (secret));
			close (sockfd);
			return (ERROR_RC);
		}
		if (FD_ISSET (sockfd, &readfds))
			break;

		/*
		 * Timed out waiting for response.  Retry "retry_max" times
		 * before giving up.  If retry_max = 0, don't retry at all.
		 */
		if (++retries >= retry_max)
		{
			error("rc_send_server: no reply from RADIUS server %s:%u",
			      rc_ip_hostname (auth_ipaddr), data->svc_port);
			close (sockfd);
			memset (secret, '\0', sizeof (secret));
			return (TIMEOUT_RC);
		}
	}
	salen = sizeof (saremote);
	length = recvfrom (sockfd, (char *) recv_buffer,
			   (int) sizeof (recv_buffer),
			   (int) 0, &saremote, &salen);

	if (length <= 0)
	{
		error("rc_send_server: recvfrom: %s:%d: %m", server_name,\
		      data->svc_port);
		close (sockfd);
		memset (secret, '\0', sizeof (secret));
		return (ERROR_RC);
	}

	recv_auth = (AUTH_HDR *)recv_buffer;

	result = rc_check_reply (recv_auth, BUFFER_LEN, secret, vector, data->seq_nbr);

	data->receive_pairs = rc_avpair_gen(recv_auth);

	close (sockfd);
	if (info)
	{
		memcpy(info->secret, secret, sizeof(info->secret));
		memcpy(info->request_vector, vector,
		       sizeof(info->request_vector));
	}
	memset (secret, '\0', sizeof (secret));

	if (result != OK_RC) return (result);

	*msg = '\0';
	vp = data->receive_pairs;
	while (vp)
	{
		if ((vp = rc_avpair_get(vp, PW_REPLY_MESSAGE)))
		{
			strcat(msg, vp->strvalue);
			strcat(msg, "\n");
			vp = vp->next;
		}
	}

	if ((recv_auth->code == PW_ACCESS_ACCEPT) ||
		(recv_auth->code == PW_PASSWORD_ACK) ||
		(recv_auth->code == PW_ACCOUNTING_RESPONSE))
	{
		result = OK_RC;
	}
	else
	{
		result = BADRESP_RC;
	}

	return (result);
}

/*
 * Function: rc_check_reply
 *
 * Purpose: verify items in returned packet.
 *
 * Returns:	OK_RC       -- upon success,
 *		BADRESP_RC  -- if anything looks funny.
 *
 */

static int rc_check_reply (AUTH_HDR *auth, int bufferlen, char *secret,
			   unsigned char *vector, unsigned char seq_nbr)
{
	int             secretlen;
	int             totallen;
	unsigned char   calc_digest[AUTH_VECTOR_LEN];
	unsigned char   reply_digest[AUTH_VECTOR_LEN];

	totallen = ntohs (auth->length);

	secretlen = strlen (secret);

	/* Do sanity checks on packet length */
	if ((totallen < 20) || (totallen > 4096))
	{
		error("rc_check_reply: received RADIUS server response with invalid length");
		return (BADRESP_RC);
	}

	/* Verify buffer space, should never trigger with current buffer size and check above */
	if ((totallen + secretlen) > bufferlen)
	{
		error("rc_check_reply: not enough buffer space to verify RADIUS server response");
		return (BADRESP_RC);
	}
	/* Verify that id (seq. number) matches what we sent */
	if (auth->id != seq_nbr)
	{
		error("rc_check_reply: received non-matching id in RADIUS server response");
		return (BADRESP_RC);
	}

	/* Verify the reply digest */
	memcpy ((char *) reply_digest, (char *) auth->vector, AUTH_VECTOR_LEN);
	memcpy ((char *) auth->vector, (char *) vector, AUTH_VECTOR_LEN);
	memcpy ((char *) auth + totallen, secret, secretlen);
	rc_md5_calc (calc_digest, (char *) auth, totallen + secretlen);

#ifdef DIGEST_DEBUG
	{
		int i;

		fputs("reply_digest: ", stderr);
		for (i = 0; i < AUTH_VECTOR_LEN; i++)
		{
			fprintf(stderr,"%.2x ", (int) reply_digest[i]);
		}
		fputs("\ncalc_digest:  ", stderr);
		for (i = 0; i < AUTH_VECTOR_LEN; i++)
		{
			fprintf(stderr,"%.2x ", (int) calc_digest[i]);
		}
		fputs("\n", stderr);
	}
#endif

	if (memcmp ((char *) reply_digest, (char *) calc_digest,
		    AUTH_VECTOR_LEN) != 0)
	{
#ifdef RADIUS_116
		/* the original Livingston radiusd v1.16 seems to have
		   a bug in digest calculation with accounting requests,
		   authentication request are ok. i looked at the code
		   but couldn't find any bugs. any help to get this
		   kludge out are welcome. preferably i want to
		   reproduce the calculation bug here to be compatible
		   to stock Livingston radiusd v1.16.	-lf, 03/14/96
		 */
		if (auth->code == PW_ACCOUNTING_RESPONSE)
			return (OK_RC);
#endif
		error("rc_check_reply: received invalid reply digest from RADIUS server");
		return (BADRESP_RC);
	}

	return (OK_RC);

}

/*
 * Function: rc_random_vector
 *
 * Purpose: generates a random vector of AUTH_VECTOR_LEN octets.
 *
 * Returns: the vector (call by reference)
 *
 */

static void rc_random_vector (unsigned char *vector)
{
	int             randno;
	int             i;
	int		fd;

/* well, I added this to increase the security for user passwords.
   we use /dev/urandom here, as /dev/random might block and we don't
   need that much randomness. BTW, great idea, Ted!     -lf, 03/18/95	*/

	if ((fd = open(_PATH_DEV_URANDOM, O_RDONLY)) >= 0)
	{
		unsigned char *pos;
		int readcount;

		i = AUTH_VECTOR_LEN;
		pos = vector;
		while (i > 0)
		{
			readcount = read(fd, (char *)pos, i);
			pos += readcount;
			i -= readcount;
		}

		close(fd);
		return;
	} /* else fall through */

	for (i = 0; i < AUTH_VECTOR_LEN;)
	{
		randno = magic();
		memcpy ((char *) vector, (char *) &randno, sizeof (int));
		vector += sizeof (int);
		i += sizeof (int);
	}

	return;
}

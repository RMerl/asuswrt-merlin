/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: user_auth.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/

/*
 * This code is, sadly,  a whimpy excuse for the dynamically loadable
 * modules.  The idea is that you can put your user code in here and it
 * will get included in various files.
 * 
 * Supported Sections:
 *   User Authentication
 * 
 *   DEFINES      FILE WHERE INCLUDED PURPOSE
 *   USER_RECEIVE  lpd_secure.c       define the user authentication
 *                                    This is an entry in a table
 *   USER_SEND     sendauth.c         define the user authentication
 *                                    This is an entry in a table
 *   RECEIVE       lpd_secure.c       define the user authentication
 *                            This is the code referenced in USER_RECEIVE
 *   SENDING       sendauth.c       define the user authentication
 *                            This is the code referenced in USER_SEND
 * 
 */

#include "lp.h"
#include "user_auth.h"
#include "krb5_auth.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "linksupport.h"
#include "child.h"
#include "getqueue.h"
#include "lpd_secure.h"
#include "lpd_dispatch.h"
#include "permission.h"

#ifdef SSL_ENABLE
# include "ssl_auth.h"
#endif


/**************************************************************
 * Secure Protocol
 *
 * the following is sent on *sock:  
 * \REQ_SECUREprintername C/F user authtype \n        - receive a command
 *             0           1   2   3
 * \REQ_SECUREprintername C/F user authtype jobsize\n - receive a job
 *             0           1   2   3        4
 *          Printer_DYN    |   |   |        + jobsize
 *                         |   |   authtype 
 *                         |  user
 *                        from_server=1 if F, 0 if C
 *                         
 * The authtype is used to look up the security information.  This
 * controls the dispatch and the lookup of information from the
 * configuration and printcap entry for the specified printer
 *
 * The info line_list has the information, stripped of the leading
 * xxxx_ of the authtype name.
 * For example:
 *
 * forward_id=test      <- forward_id from configuration/printcap
 * id=test              <- id from configuration/printcap
 * 
 * If there are no problems with this information, a single 0 byte
 * should be written back at this point, or a nonzero byte with an
 * error message.  The 0 will cause the corresponding transfer
 * to be started.
 * 
 * The handshake and with the remote end should be done now.
 *
 * The client will send a string with the following format:
 * destination=test\n     <- destination ID (URL encoded)
 *                       (destination ID from above)
 * server=test\n          <- if originating from server, the server key (URL encoded)
 *                       (originator ID from above)
 * client=papowell\n      <- client id
 *                       (client ID from above)
 * input=%04t1\n          <- input or command
 * This information will be extracted by the server.
 * The 'Do_secure_work' routine can now be called,  and it will do the work.
 * 
 * ERROR MESSAGES:
 *  If you generate an error,  then you should log it.  If you want
 *  return status to be returned to the remote end,  then you have
 *  to take suitable precautions.
 * 1. If the error is detected BEFORE you send the 0 ACK,  then you
 *    can send an error back directly.
 * 2. If the error is discovered as the result of a problem with
 *    the encryption method,  it is strongly recommended that you
 *    simply send a string error message back.  This should be
 *    detected by the remote end,  which will then decide that this
 *    is an error message and not status.
 *
 **************************************************************/

/*
  Test_connect: send the validation information  
    expect to get back NULL or error message
 */

int Test_connect( struct job *job, int *sock,
	int transfer_timeout,
	char *errmsg, int errlen,
	struct security *security, struct line_list *info )
{
	char *cmd, *secure = "TEST\n";
	int status = 0, ack = 0;
	
	if(DEBUGL1)Dump_line_list("Test_connect: info", info );
	DEBUG3("Test_connect: sending '%s'", secure);
	status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
		secure, safestrlen(secure), &ack );
	DEBUG3("Test_connect: status '%s'", Link_err_str(status) );
	if( status ){
		SNPRINTF(errmsg, errlen)
			"Test_connect: error '%s'", Link_err_str(status) );
		status = JFAIL;
	}
	if( ack ){
		SNPRINTF(errmsg, errlen)
			"Test_connect: ack '%d'", ack );
		status = JFAIL;
	}
	return( status );
}

int Test_accept( int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	char *errmsg, int errlen,
	struct line_list *info, struct line_list *header_info,
	struct security *security )
{
	int status, n, len;
	char input[SMALLBUFFER];
	char *value;

	DEBUGFC(DRECV1)Dump_line_list("Test_accept: info", info );
	DEBUGFC(DRECV1)Dump_line_list("Test_accept: header_info", header_info );

	len = sizeof(input)-1;
	status = Link_line_read(ShortRemote_FQDN,sock,
		Send_job_rw_timeout_DYN,input,&len);
	if( len >= 0 ) input[len] = 0;
	if( status ){
		SNPRINTF(errmsg,errlen)
			"error '%s' READ from %s@%s",
			Link_err_str(status), RemotePrinter_DYN, RemoteHost_DYN );
		goto error;
	}
	DEBUG1( "Test_accept: read status %d, len %d, '%s'",
		status, len, input );
	if( (status = Link_send( RemoteHost_DYN, sock, Send_query_rw_timeout_DYN,
		"", 1, 0 )) ){
		SNPRINTF(errmsg,errlen)
			"error '%s' ACK to %s@%s",
			Link_err_str(status), RemotePrinter_DYN, RemoteHost_DYN );
		goto error;
	}
	DEBUG1( "Test_accept: ACK sent");

 error:
	return( status );
}

/**************************************************************
 *Test_send:
 *A simple implementation for testing user supplied authentication
 *
 * Simply send the authentication information to the remote end
 * destination=test     <- destination ID (URL encoded)
 *                       (destination ID from above)
 * server=test          <- if originating from server, the server key (URL encoded)
 *                       (originator ID from above)
 * client=papowell      <- client id
 *                       (client ID from above)
 * input=%04t1          <- input that is 
 *
 **************************************************************/

int Test_send( int *sock,
	int transfer_timeout,
	char *tempfile,
	char *errmsg, int errlen,
	struct security *security, struct line_list *info )
{
	char buffer[LARGEBUFFER];
	struct stat statb;
	int tempfd, len;
	int status = 0;

	if(DEBUGL1)Dump_line_list("Test_send: info", info );
	DEBUG1("Test_send: sending on socket %d", *sock );
	if( (tempfd = Checkread(tempfile,&statb)) < 0){
		SNPRINTF(errmsg, errlen)
			"Test_send: open '%s' for read failed - %s",
			tempfile, Errormsg(errno) );
		status = JABORT;
		goto error;
	}
	DEBUG1("Test_send: starting read");
	while( (len = read( tempfd, buffer, sizeof(buffer)-1 )) > 0 ){
		buffer[len] = 0;
		DEBUG4("Test_send: file information '%s'", buffer );
		if( write( *sock, buffer, len) != len ){
			SNPRINTF(errmsg, errlen)
				"Test_send: write to socket failed - %s", Errormsg(errno) );
			status = JABORT;
			goto error;
		}
	}
	if( len < 0 ){
		SNPRINTF(errmsg, errlen)
			"Test_send: read from '%s' failed - %s", tempfile, Errormsg(errno) );
		status = JABORT;
		goto error;
	}
	close(tempfd); tempfd = -1;
	/* we close the writing side */
	shutdown( *sock, 1 );

	DEBUG1("Test_send: sent file" );

	if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
		SNPRINTF(errmsg, errlen)
			"Test_send: open '%s' for write failed - %s",
			tempfile, Errormsg(errno) );
		status = JABORT;
		goto error;
	}
	DEBUG1("Test_send: starting read");

	while( (len = read(*sock,buffer,sizeof(buffer)-1)) > 0 ){
		buffer[len] = 0;
		DEBUG4("Test_send: socket information '%s'", buffer);
		if( write(tempfd,buffer,len) != len ){
			SNPRINTF(errmsg, errlen)
				"Test_send: write to '%s' failed - %s", tempfile, Errormsg(errno) );
			status = JABORT;
			goto error;
		}
	}
	close( tempfd ); tempfd = -1;

 error:
	return(status);
}

int Test_receive( int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile )
{
	int tempfd, status, n;
	char buffer[LARGEBUFFER];
	struct stat statb;

#if 0
	/* this shows how to create a temporary file for private use
	 * it gets unlinked safely.
	 */
	char *pgpfile
    pgpfile = safestrdup2(tempfile,".pgp",__FILE__,__LINE__); 
    Check_max(&Tempfiles,1);
    Tempfiles.list[Tempfiles.count++] = pgpfile;
#endif

	tempfd = -1;

	DEBUGFC(DRECV1)Dump_line_list("Test_receive: info", info );
	DEBUGFC(DRECV1)Dump_line_list("Test_receive: header_info", header_info );
	/* do validation and then write 0 */
	if( Write_fd_len( *sock, "", 1 ) < 0 ){
		status = JABORT;
		SNPRINTF( errmsg, errlen) "Test_receive: ACK 0 write error - %s",
			Errormsg(errno) );
		goto error;
	}

	/* open a file for the output */
	if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO) "Test_receive: reopen of '%s' for write failed",
			tempfile );
	}

	DEBUGF(DRECV1)("Test_receive: starting read from socket %d", *sock );
	while( (n = read(*sock, buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUGF(DRECV4)("Test_receive: remote read '%d' '%s'", n, buffer );
		if( write( tempfd,buffer,n ) != n ){
			DEBUGF(DRECV1)( "Test_receive: bad write to '%s' - '%s'",
				tempfile, Errormsg(errno) );
			status = JFAIL;
			goto error;
		}
	}
	if( n < 0 ){
		DEBUGF(DRECV1)("Test_receive: bad read '%d' getting command", n );
		status = JFAIL;
		goto error;
	}
	close(tempfd); tempfd = -1;
	DEBUGF(DRECV4)("Test_receive: end read" );

	/*** at this point you can check the format of the received file, etc.
     *** if you have an error message at this point, you should write it
	 *** to the socket,  and arrange protocol can handle this.
	 ***/

	status = Do_secure_work( jobsize, from_server, tempfile, header_info );

	/*** if an error message is returned, you should write this
	 *** message to the tempfile and the proceed to send the contents
	 ***/
	DEBUGF(DRECV1)("Test_receive: doing reply" );
	if( (tempfd = Checkread(tempfile,&statb)) < 0 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO) "Test_receive: reopen of '%s' for write failed",
			tempfile );
	}

	while( (n = read(tempfd, buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUGF(DRECV4)("Test_receive: sending '%d' '%s'", n, buffer );
		if( write( *sock,buffer,n ) != n ){
			DEBUGF(DRECV1)( "Test_receive: bad write to socket - '%s'",
				Errormsg(errno) );
			status = JFAIL;
			goto error;
		}
	}
	if( n < 0 ){
		DEBUGF(DRECV1)("Test_receive: bad read '%d' getting status", n );
		status = JFAIL;
		goto error;
	}
	DEBUGF(DRECV1)("Test_receive: reply done" );

 error:
	if( tempfd>=0) close(tempfd); tempfd = -1;
	return(status);
}

/* 
 * md5 authentication
 
 
 Connection Level: md5_connect and md5_verify
 
 The client and server do a handshake, exchanging salts and values as follows:
 Client connects to server
      Server sends 'salt'
 Client gets user and user secret 
        gets server id and server secret 
        makes 'userid serverid'
        hashs the whole mess and gets md5 hash (see code)
        sends 'userid serverid XXXXXXX' where XXXXX is md5 hash
 Server checks userid and server, generates the same hash
        if values match, all is happy
 
 File Transfer: md5_send and md5 receive
 
 Client connects to server
     Server sends 'salt'
 Client
        get user and user secret
        gets server id and server secret 
        hashes the mess and the file
        Sends the hash followed by the file
 Server reads the file
        gets the hash
        dumps the rest of stuff into a file
        get user and user secret
        gets server id and server secret 
        hashes the mess and the file
        checks to see that all is well
        performs action
  Client keyfile:
    xxx=yyy key   where xxx is md5_id=xxx value from printcap
        yyy should be sent to server as the 'from' value
  Server keyfile:
    yyy=key       where yyy is the 'from' id value

*/


#include "md5.h"

/* Set the name of the file we'll be getting our key from.
   The keyfile should only be readable by the owner and
   by whatever group the user programs run as.
   This way, nobody can write their own authentication module */

/* key length (for MD5, it's 16) */
#define KEY_LENGTH 16
 char *hexstr( char *str, int len, char *outbuf, int outlen );
 void MDString (char *inString, char *outstring, int inlen, int outlen);
 void MDFile( int fd, char *outstring, int outlen );
 int md5key( const char *keyfile, char *name, char *key, int keysize, char *errmsg, int errlen );

/* The md5 hashing function, which does the real work */
 void MDString (char *inString, char *outstring, int inlen, int outlen)
{
	MD5_CONTEXT mdContext;

	MD5Init (&mdContext);
	MD5Update(&mdContext, inString, inlen);
	MD5Final(&mdContext);
	memcpy( outstring, mdContext.digest, outlen );
}

 void MDFile( int fd, char *outstring, int outlen )
{
	MD5_CONTEXT mdContext;
	char buffer[LARGEBUFFER];
	int n;

	MD5Init (&mdContext);
	while( (n = read( fd, buffer, sizeof(buffer))) > 0 ){
		MD5Update(&mdContext, buffer, n);
	}
	MD5Final(&mdContext);
	memcpy( outstring, mdContext.digest, outlen );
}

 char *hexstr( char *str, int len, char *outbuf, int outlen )
{
	int i, j;
	for( i = 0; i < len && 2*(i+1) < outlen ; ++i ){
		j = ((unsigned char *)str)[i];
		SNPRINTF(&outbuf[2*i],4)"%02x",j);
	}
	if( outlen > 0 ) outbuf[2*i] = 0;
	return( outbuf );
}

 int md5key( const char *keyfile, char *name, char *key, int keysize, char *errmsg, int errlen )
{
	const char *keyvalue;
	int i,  keylength = -1;
	struct line_list keys;

	Init_line_list( &keys );
	memset(key,0,keysize);
	/*
		void Read_file_list( int required, struct line_list *model, char *str,
			const char *linesep, int sort, const char *keysep, int uniq, int trim,
			int marker, int doinclude, int nocomment, int depth, int maxdepth )
	*/
	Read_file_list( /*required*/0, /*model*/&keys, /*str*/(char *)keyfile,
		/*linesep*/Line_ends,/*sort*/1, /*keysep*/Value_sep,/*uniq*/1, /*trim*/1,
		/*marker*/0, /*doinclude*/0, /*nocomment*/1,/*depth*/0,/*maxdepth*/4 );
	/* read in the key from the key file */
	keyvalue = Find_exists_value( &keys, name, Value_sep );
	if( keyvalue == 0 ){
		SNPRINTF(errmsg, errlen)
		"md5key: no key for '%s' in '%s'", name, keyfile );
		goto error;
	}
	DEBUG1("md5key: key '%s'", keyvalue );

	/* copy to string */
	for(i = 0; keyvalue[i] && i < keysize; ++i ){
		key[i] = keyvalue[i];
	}
	keylength = i;

 error:
	Free_line_list( &keys );
	return( keylength );
}

/**************************************************************
 *md5_send:
 *A simple implementation for testing user supplied authentication
 * The info line_list has the following fields
 * client=papowell      <- client id, can be forwarded
 * destination=test     <- destination ID - use for remote key
 * forward_id=test      <- forward_id from configuration/printcap
 * from=papowell        <- originator ID  - use for local key
 * id=test              <- id from configuration/printcap
 *
 * The start of the actual file has:
 * destination=test     <- destination ID (URL encoded)
 *                       (destination ID from above)
 * server=test          <- if originating from server, the server key (URL encoded)
 *                       (originator ID from above)
 * client=papowell      <- client id
 *                       (client ID from above)
 * input=%04t1          <- input that is 
 *   If you need to set a 'secure id' then you set the 'FROM'
 *
 **************************************************************/

int md5_send( int *sock, int transfer_timeout, char *tempfile,
	char *errmsg, int errlen,
	struct security *security, struct line_list *info )
{
	char destkey[KEY_LENGTH+1];
	char challenge[KEY_LENGTH+1];
	char response[KEY_LENGTH+1];
	char filehash[KEY_LENGTH+1];
	int destkeylength, i, n;
	char smallbuffer[SMALLBUFFER];
	char keybuffer[SMALLBUFFER];
	char *s, *dest;
	const char *keyfile;
	char buffer[LARGEBUFFER];
	struct stat statb;
	int tempfd = -1, len, ack;
	int status = 0;

	errmsg[0] = 0;
	if(DEBUGL1)Dump_line_list("md5_send: info", info );
	if( !Is_server ){
		/* we get the value of the MD5KEYFILE variable */
		keyfile = getenv("MD5KEYFILE");
		if( keyfile == 0 ){
			SNPRINTF(errmsg, errlen)
				"md5_send: no MD5KEYFILE environment variable" );
			goto error;
		}
	} else {
		keyfile = Find_exists_value( info, "server_keyfile", Value_sep );
		if( keyfile == 0 ){
			SNPRINTF(errmsg, errlen)
				"md5_send: no md5_server_keyfile entry" );
			goto error;
		}
	}

	dest = Find_str_value( info, DESTINATION, Value_sep );
	if( dest == 0 ){
		SNPRINTF(errmsg, errlen)
			"md5_send: no '%s' value in info", DESTINATION );
		goto error;
	}
	if( (destkeylength = md5key( keyfile, dest, keybuffer,
		sizeof(keybuffer), errmsg, errlen ) ) <= 0 ){
		goto error;
	}
	if( (s = strpbrk(keybuffer,Whitespace)) ){
		*s++ = 0;
		while( (isspace(cval(s))) ) ++s;
		if( *s == 0 ){
			SNPRINTF(errmsg, errlen)
				"md5_send: no '%s' value in keyfile", dest );
			goto error;
		}
		dest = keybuffer;
	} else {
		s = keybuffer;
		dest = Find_str_value( info, FROM, Value_sep );
		if( !dest ){
			SNPRINTF(errmsg,errlen)
				"md5_send: no '%s' value in info", FROM );
			goto error;
		}
	}
	destkeylength = safestrlen(s);
	if( destkeylength > KEY_LENGTH ) destkeylength = KEY_LENGTH;
	memcpy( destkey, s, destkeylength );

	DEBUG1("md5_send: sending on socket %d", *sock );
	/* Read the challenge dest server */
	len = sizeof(buffer);
	if( (n = Link_line_read(ShortRemote_FQDN,sock,
		Send_query_rw_timeout_DYN,buffer,&len)) ){
		SNPRINTF(errmsg, errlen)
		"md5_send: error reading challenge - '%s'", Link_err_str(n) );
		goto error;
	} else if( len == 0 ){
		SNPRINTF(errmsg, errlen)
		"md5_send: zero length challenge");
		goto error;
	}
	DEBUG1("md5_send: challenge '%s'", buffer );
	n = safestrlen(buffer);
	if( n == 0 || n % 2 || n/2 > KEY_LENGTH ){
		SNPRINTF(errmsg, errlen)
		"md5_send: bad challenge length '%d'", safestrlen(buffer) );
		goto error;
	}
	memset(challenge, 0, sizeof(challenge));
	smallbuffer[2] = 0;
	for(i = 0; buffer[2*i] && i < KEY_LENGTH; ++i ){
		memcpy(smallbuffer,&buffer[2*i],2);
		challenge[i] = strtol(smallbuffer,0,16);
	}

	DEBUG1("md5_send: decoded challenge '%s'",
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));
	DEBUG1("md5_send: destkey '%s'", 
		hexstr( destkey, KEY_LENGTH, buffer, sizeof(buffer) ));
	/* xor the challenge with the dest key */
	n = 0;
	len = destkeylength;
	for(i = 0; i < KEY_LENGTH; i++){
		challenge[i] = (challenge[i]^destkey[n]);
		n=(n+1)%len;
	}
	DEBUG1("md5_send: challenge^destkey '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));

	DEBUG1("md5_send: challenge^destkey^idkey '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));

	DEBUG1("md5_send: opening tempfile '%s'", tempfile );

	if( (tempfd = Checkread(tempfile,&statb)) < 0){
		SNPRINTF(errmsg, errlen)
			"md5_send: open '%s' for read failed - %s",
			tempfile, Errormsg(errno) );
		status = JABORT;
		goto error;
	}
	DEBUG1("md5_send: doing md5 of file");
	MDFile( tempfd, filehash, KEY_LENGTH);
	DEBUG1("md5_send: filehash '%s'", 
		hexstr( filehash, KEY_LENGTH, buffer, sizeof(buffer) ));

	/* xor the challenge with the file key */
	n = 0;
	len = KEY_LENGTH;
	for(i = 0; i < KEY_LENGTH; i++){
		challenge[i] = (challenge[i]^filehash[n]);
		n=(n+1)%len;
	}

	DEBUG1("md5_send: challenge^destkey^idkey^filehash '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));

	SNPRINTF( smallbuffer, sizeof(smallbuffer)) "%s", dest );

	/* now we xor the buffer with the key */
	len = safestrlen(smallbuffer);
	DEBUG1("md5_send: idstuff len %d '%s'", len, smallbuffer );
	n = 0;
	for(i = 0; i < len; i++){
		challenge[n] = (challenge[n]^smallbuffer[i]);
		n=(n+1)%KEY_LENGTH;
	}
	DEBUG1("md5_send: result challenge^destkey^idkey^filehash^idstuff '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));

	/* now, MD5 hash the string */
	MDString(challenge, response, KEY_LENGTH, KEY_LENGTH);

	/* return the response to the server */
	hexstr( response, KEY_LENGTH, buffer, sizeof(buffer) );
	n = safestrlen(smallbuffer);
	SNPRINTF(smallbuffer+n, sizeof(smallbuffer)-n-1) " %s", buffer );
	DEBUG1("md5_send: sending response '%s'", smallbuffer );
	safestrncat(smallbuffer,"\n");
	ack = 0;
	if( (n =  Link_send( RemoteHost_DYN, sock, Send_query_rw_timeout_DYN,
		smallbuffer, safestrlen(smallbuffer), &ack )) || ack ){
		/* keep the other end dest trying to read */
		if( (s = strchr(smallbuffer,'\n')) ) *s = 0;
		SNPRINTF(errmsg,errlen)
			"error '%s'\n sending str '%s' to %s@%s",
			Link_err_str(n), smallbuffer,
			RemotePrinter_DYN, RemoteHost_DYN );
		goto error;
	}

	if( lseek( tempfd, 0, SEEK_SET ) == -1 ){
		SNPRINTF(errmsg,errlen)
			"md5_send: seek failed - '%s'", Errormsg(errno) );
		goto error;
	}

	DEBUG1("md5_send: starting transfer of file");
	while( (len = read( tempfd, buffer, sizeof(buffer)-1 )) > 0 ){
		buffer[len] = 0;
		DEBUG4("md5_send: file information '%s'", buffer );
		if( write( *sock, buffer, len) != len ){
			SNPRINTF(errmsg, errlen)
				"md5_send: write to socket failed - %s", Errormsg(errno) );
			status = JABORT;
			goto error;
		}
	}
	if( len < 0 ){
		SNPRINTF(errmsg, errlen)
			"md5_send: read dest '%s' failed - %s", tempfile, Errormsg(errno) );
		status = JABORT;
		goto error;
	}
	close(tempfd); tempfd = -1;
	/* we close the writing side */
	shutdown( *sock, 1 );

	DEBUG1("md5_send: sent file" );

	if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
		SNPRINTF(errmsg, errlen)
			"md5_send: open '%s' for write failed - %s",
			tempfile, Errormsg(errno) );
		status = JABORT;
		goto error;
	}
	DEBUG1("md5_send: starting read of response");

	if( (len = read(*sock,buffer,1)) > 0 ){
		n = cval(buffer);
		DEBUG4("md5_send: response byte '%d'", n);
		status = n;
		if( isprint(n) && write(tempfd,buffer,1) != 1 ){
			SNPRINTF(errmsg, errlen)
				"md5_send: write to '%s' failed - %s", tempfile, Errormsg(errno) );
			status = JABORT;
			goto error;
		}
	}
	while( (len = read(*sock,buffer,sizeof(buffer)-1)) > 0 ){
		buffer[len] = 0;
		DEBUG4("md5_send: socket information '%s'", buffer);
		if( write(tempfd,buffer,len) != len ){
			SNPRINTF(errmsg, errlen)
				"md5_send: write to '%s' failed - %s", tempfile, Errormsg(errno) );
			status = JABORT;
			goto error;
		}
	}
	close( tempfd ); tempfd = -1;

 error:
	if( tempfd >= 0 ) close(tempfd); tempfd = -1;
	return(status);
}


int md5_receive( int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile )
{
	char input[SMALLBUFFER];
	char buffer[LARGEBUFFER];
	char keybuffer[LARGEBUFFER];
	int destkeylength, i, n, len, tempfd = -1;
	char *s, *dest, *hash;
	const char *keyfile;
	char destkey[KEY_LENGTH+1];
	char challenge[KEY_LENGTH+1];
	char response[KEY_LENGTH+1];
	char filehash[KEY_LENGTH+1];
	struct stat statb;
	int status_error = 0;
	double size;


	if(DEBUGL1)Dump_line_list("md5_receive: info", info );
	if(DEBUGL1)Dump_line_list("md5_receive: header_info", header_info );
	/* do validation and then write 0 */

	if( !Is_server ){
		SNPRINTF(errmsg, errlen)
			"md5_receive: not server" );
		goto error;
	} else {
		keyfile = Find_exists_value( info, "server_keyfile", Value_sep );
		if( keyfile == 0 ){
			SNPRINTF(errmsg, errlen)
				"md5_receive: no md5_server_keyfile entry" );
			goto error;
		}
	}

	DEBUGF(DRECV1)("md5_receive: sending ACK" );
	if( (n = Link_send( RemoteHost_DYN, sock, Send_query_rw_timeout_DYN,
		"", 1, 0 )) ){
		SNPRINTF(errmsg,errlen)
			"error '%s' ACK to %s@%s",
			Link_err_str(n), RemotePrinter_DYN, RemoteHost_DYN );
		goto error;
	}
	/* First, seed the random number generator */
	srand(time(NULL));

	/* Now, fill the challenge with 16 random values */
	for(i = 0; i < KEY_LENGTH; i++){
		challenge[i] = rand() >> 8;
	}
	hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) );
	DEBUGF(DRECV1)("md5_receive: sending challenge '%s'", buffer );
	safestrncat(buffer,"\n");

	/* Send the challenge to the client */

	if( (n = Link_send( RemoteHost_DYN, sock, Send_query_rw_timeout_DYN,
		buffer, safestrlen(buffer), 0 )) ){
		/* keep the other end dest trying to read */
		if( (s = strchr(buffer,'\n')) ) *s = 0;
		SNPRINTF(errmsg,errlen)
			"error '%s' sending str '%s' to %s@%s",
			Link_err_str(n), buffer,
			RemotePrinter_DYN, RemoteHost_DYN );
		goto error;
	}

	/* now read response */
	DEBUGF(DRECV1)("md5_receive: reading response");
	len = sizeof(input)-1;
	if( (n = Link_line_read(ShortRemote_FQDN,sock,
		Send_query_rw_timeout_DYN,input,&len) )){
		SNPRINTF(errmsg, errlen)
		"md5_receive: error reading challenge - '%s'", Link_err_str(n) );
		goto error;
	} else if( len == 0 ){
		SNPRINTF(errmsg, errlen)
		"md5_receive: zero length response");
		goto error;
	} else if( len >= (int)sizeof( input) -2 ){
		SNPRINTF(errmsg, errlen)
		"md5_receive: response too long");
		goto error;
	}
	DEBUGF(DRECV1)("md5_receive: response '%s'", input );

	dest = input;
	if( (s = strchr(input,' ')) ) *s++ = 0;
	if( s ){
		hash = s;
		if( strpbrk(hash,Whitespace) ){
			SNPRINTF(errmsg, errlen)
				"md5_receive: malformed response" );
			goto error;
		}
		n = safestrlen(hash);
		if( n == 0 || n%2 ){
			SNPRINTF(errmsg, errlen)
			"md5_receive: bad response hash length '%d'", n );
			goto error;
		}
	} else {
		SNPRINTF(errmsg, errlen)
			"md5_receive: no 'hash' in response" );
		goto error;
	}


	DEBUGF(DRECV1)("md5_receive: dest '%s', hash '%s', prefix '%s'",
		dest, hash, buffer );
	if( (destkeylength = md5key( keyfile, dest, keybuffer, KEY_LENGTH, errmsg, errlen ) ) <= 0 ){
		goto error;
	}
	if( (s = strpbrk(keybuffer,Whitespace)) ){
		*s++ = 0;
		while( isspace(cval(s))) ++s;
	} else {
		s = keybuffer;
	}
	destkeylength = safestrlen(s);
	if( destkeylength > KEY_LENGTH ) destkeylength = KEY_LENGTH;
	memcpy(destkey,s,destkeylength);

	
	DEBUGF(DRECV1)("md5_receive: success, sending ACK" );

	if((n = Link_send( RemoteHost_DYN, sock, Send_query_rw_timeout_DYN, "", 1, 0 )) ){
		/* keep the other end dest trying to read */
		SNPRINTF(errmsg,errlen)
			"error '%s' sending ACK to %s@%s",
			Link_err_str(n),
			RemotePrinter_DYN, RemoteHost_DYN );
		goto error;
	}

	DEBUGF(DRECV1)("md5_receive: reading file" );

	/* open a file for the output */
	if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0 ){
		SNPRINTF(errmsg, errlen)
			"md5_receive: reopen of '%s' for write failed",
			tempfile );
	}

	DEBUGF(DRECV1)("md5_receive: starting read dest socket %d", *sock );
	while( (n = read(*sock, buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUGF(DRECV4)("md5_receive: remote read '%d' '%s'", n, buffer );
		if( write( tempfd,buffer,n ) != n ){
			SNPRINTF(errmsg, errlen)
				"md5_receive: bad write to '%s' - '%s'",
				tempfile, Errormsg(errno) );
			goto error;
		}
	}

	if( n < 0 ){
		SNPRINTF(errmsg, errlen)
		"md5_receive: bad read '%d' reading file ", n );
		goto error;
	}
	close(tempfd); tempfd = -1;
	DEBUGF(DRECV4)("md5_receive: end read" );

	DEBUG1("md5_receive: opening tempfile '%s'", tempfile );

	if( (tempfd = Checkread(tempfile,&statb)) < 0){
		SNPRINTF(errmsg, errlen)
			"md5_receive: open '%s' for read failed - %s",
			tempfile, Errormsg(errno) );
		goto error;
	}
	DEBUG1("md5_receive: doing md5 of file");
	MDFile( tempfd, filehash, KEY_LENGTH);
	DEBUG1("md5_receive: filehash '%s'", 
		hexstr( filehash, KEY_LENGTH, buffer, sizeof(buffer) ));
	close(tempfd); tempfd = -1;

	DEBUGF(DRECV1)("md5_receive: challenge '%s'",
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));
	DEBUGF(DRECV1)("md5_receive: destkey '%s'", 
		hexstr( destkey, KEY_LENGTH, buffer, sizeof(buffer) ));
	/* xor the challenge with the dest key */
	n = 0;
	len = destkeylength;
	for(i = 0; i < KEY_LENGTH; i++){
		challenge[i] = (challenge[i]^destkey[n]);
		n=(n+1)%len;
	}
	DEBUGF(DRECV1)("md5_receive: challenge^destkey '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));

	DEBUGF(DRECV1)("md5_receive: challenge^destkey^idkey '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));


	DEBUGF(DRECV1)("md5_receive: filehash '%s'", 
		hexstr( filehash, KEY_LENGTH, buffer, sizeof(buffer) ));

	/* xor the challenge with the file key */
	n = 0;
	len = KEY_LENGTH;
	for(i = 0; i < KEY_LENGTH; i++){
		challenge[i] = (challenge[i]^filehash[n]);
		n=(n+1)%len;
	}
	DEBUGF(DRECV1)("md5_receive: challenge^destkey^idkey^filehash '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));


	/* now we xor the buffer with the key */
	len = safestrlen(input);
	DEBUGF(DRECV1)("md5_receive: idstuff len %d '%s'", len, input );
	n = 0;
	for(i = 0; i < len; i++){
		challenge[n] = (challenge[n]^input[i]);
		n=(n+1)%KEY_LENGTH;
	}
	DEBUGF(DRECV1)("md5_receive: result challenge^destkey^idkey^filehash^deststuff '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));

	/* now, MD5 hash the string */
	MDString(challenge, response, KEY_LENGTH, KEY_LENGTH);

	hexstr( response, KEY_LENGTH, buffer, sizeof(buffer) );

	DEBUGF(DRECV1)("md5_receive: calculated hash '%s'", buffer );
	DEBUGF(DRECV1)("md5_receive: sent hash '%s'", hash );

	if( strcmp( buffer, hash ) ){
		SNPRINTF(errmsg, errlen)
		"md5_receive: bad response value");
		goto error;
	}
	
	DEBUGF(DRECV1)("md5_receive: success" );
	Set_str_value(header_info,FROM,dest);
	status_error = Do_secure_work( jobsize, from_server, tempfile, header_info );
	DEBUGF(DRECV1)("md5_receive: Do_secure_work returned %d", status_error );

	/* we now have the encoded output */
	if( (tempfd = Checkread(tempfile,&statb)) < 0 ){
		SNPRINTF( errmsg, errlen)
			"md5_receive: reopen of '%s' for read failed - %s",
			tempfile, Errormsg(errno) );
		goto error;
	}
	size = statb.st_size;
	DEBUGF(DRECV1)( "md5_receive: return status encoded size %0.0f", size);
	if( size || status_error ){
		buffer[0] = ACK_FAIL;
		write( *sock,buffer,1 );
		while( (n = read(tempfd, buffer,sizeof(buffer)-1)) > 0 ){
			buffer[n] = 0;
			DEBUGF(DRECV4)("md5_receive: sending '%d' '%s'", n, buffer );
			if( write( *sock,buffer,n ) != n ){
				SNPRINTF( errmsg, errlen)
					"md5_receive: bad write to socket - '%s'",
					Errormsg(errno) );
				goto error;
			}
		}
		if( n < 0 ){
			SNPRINTF( errmsg, errlen)
				"md5_receive: read '%s' failed - %s",
				tempfile, Errormsg(errno) );
			goto error;
		}
	} else {
		write( *sock,"",1 );
	}
	return( 0 );


 error:
	return(JFAIL);
}


/***************************************************************************
 * Pgp encode and decode a file
 ***************************************************************************/

int Pgp_get_pgppassfd( struct line_list *info, char *error, int errlen )
{
	char *s;
	int pgppassfd = -1;
	struct stat statb;

	/* get the user authentication */
	error[0] = 0;
	if( !Is_server ){
		char *passphrasefile = Find_str_value(info,"passphrasefile",Value_sep);
		if( (s = getenv( "PGPPASS" )) ){
			DEBUG1("Pgp_get_pgppassfd: PGPPASS '%s'", s );
		} else if( (s = getenv( "PGPPASSFD" )) ){
			pgppassfd = atoi(s);
			if( pgppassfd <= 0 || fstat(pgppassfd, &statb ) ){
				Errorcode = JABORT;
				DIEMSG("PGPASSFD '%s' not file", s);
			}
		} else if( (s = getenv( "PGPPASSFILE" ) ) ){
			if( (pgppassfd = Checkread( s, &statb )) < 0 ){
				Errorcode = JABORT;
				DIEMSG("PGP phrasefile '%s' not opened - %s\n",
					s, Errormsg(errno) );
			}
			DEBUG1("Pgp_get_pgppassfd: PGPPASSFD file '%s', size %0.0f, fd %d",
				s, (double)statb.st_size, pgppassfd );
		} else if( (s = getenv("HOME")) && passphrasefile ){
			char *path;
			s = safestrdup2(s,"/.pgp",__FILE__,__LINE__);
			path = Make_pathname( s, passphrasefile);
			if( s ) free(s); s = 0;
			if( (pgppassfd = Checkread( path, &statb )) < 0 ){
				Errorcode = JABORT;
				DIEMSG("passphrase file %s not readable - %s",
					passphrasefile, Errormsg(errno));
			}
			DEBUG1("Pgp_get_pgppassfd: PGPPASSFD file '%s', size %0.0f, fd %d",
				path, (double)statb.st_size, pgppassfd );
			if( path ) free(path); path = 0;
		}
	} else {
		char *server_passphrasefile = Find_str_value(info,"server_passphrasefile",Value_sep);
		if(DEBUGL1)Dump_line_list("Pgp_get_pgppassfd: info", info);
		if( !server_passphrasefile ){
			SNPRINTF(error,errlen)
				"Pgp_get_pgppassfd: no 'pgp_server_passphrasefile' value\n" );
		} else if( (pgppassfd =
			Checkread(server_passphrasefile,&statb)) < 0 ){
				SNPRINTF(error,errlen)
					"Pgp_get_pgppassfd: cannot open '%s' - '%s'\n",
					server_passphrasefile, Errormsg(errno) );
		}
	}
	return(pgppassfd);
}

int Pgp_decode(struct line_list *info, char *tempfile, char *pgpfile,
	struct line_list *pgp_info, char *buffer, int bufflen,
	char *error, int errlen, char *esc_to_id, struct line_list *from_info,
	int *pgp_exit_code, int *not_a_ciphertext )
{
	struct line_list env, files;
	plp_status_t procstatus;
	int pgppassfd = -1, error_fd[2], status, cnt, n, pid, i;
	char *s, *t;
	*pgp_exit_code = *not_a_ciphertext = 0;

	Init_line_list(&env);
	Init_line_list(&files);

	DEBUG1("Pgp_decode: esc_to_id '%s'", esc_to_id );

	error[0] = 0;
	status = 0;
	if( ISNULL(Pgp_path_DYN) ){
		SNPRINTF( error, errlen)
		"Pgp_decode: missing pgp_path info"); 
		status = JFAIL;
		goto error;
	}

	status = 0;
	error_fd[0] = error_fd[1] = -1;

	error[0] = 0;
	pgppassfd = Pgp_get_pgppassfd( info, error, errlen );
	if( error[0] ){
		status = JFAIL;
		goto error;
	}

	/* run the PGP decoder */
	if( pipe(error_fd) == -1 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO) "Pgp_decode: pipe() failed" );
	}
	Max_open(error_fd[0]); Max_open(error_fd[1]);
	Check_max(&files,10);
	files.list[files.count++] = Cast_int_to_voidstar(0);
	files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);
	files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);
	if( pgppassfd >= 0 ){
		Set_decimal_value(&env,"PGPPASSFD",files.count);
		files.list[files.count++] = Cast_int_to_voidstar(pgppassfd);
	}

	/* now we run the PGP code */

	SNPRINTF(buffer,bufflen)
		"%s +force +batch %s -u '$%%%s' -o '%s'",
		Pgp_path_DYN, pgpfile, esc_to_id, tempfile ); 

	if( (pid = Make_passthrough(buffer, 0, &files, 0, &env )) < 0 ){
		DEBUG1("Pgp_decode: fork failed - %s", Errormsg(errno));
		status = JFAIL;
		goto error;
	}

	files.count = 0;
	Free_line_list(&files);
	Free_line_list(&env);
	close(error_fd[1]); error_fd[1] = -1;
	if( pgppassfd >= 0) close(pgppassfd); pgppassfd = -1;

	/* now we read authentication info from pgp */

	n = 0;
	while( n < bufflen-1
		&& (cnt = read( error_fd[0], buffer+n, bufflen-1-n )) > 0 ){
		buffer[n+cnt] = 0;
		while( (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
			DEBUG1("Pgp_decode: pgp output '%s'", buffer );
			while( cval(buffer) && !isprint(cval(buffer)) ){
				memmove(buffer,buffer+1,safestrlen(buffer+1)+1);
			}
			/* rip out extra spaces */
			for( t = buffer; *t ; ){
				if( isspace(cval(t)) && isspace(cval(t+1)) ){
					memmove(t,t+1,safestrlen(t+1)+1);
				} else {
					++t;
				}
			}
			if( buffer[0] ){
				DEBUG1("Pgp_decode: pgp final output '%s'", buffer );
				Add_line_list( pgp_info, buffer, 0, 0, 0 );
			}
			memmove(buffer,s,safestrlen(s)+1);
		}
	}
	close(error_fd[0]); error_fd[0] = -1;

	/* wait for pgp to exit and check status */
	while( (n = waitpid(pid,&procstatus,0)) != pid ){
		int err = errno;
		DEBUG1("Pgp_decode: waitpid(%d) returned %d, err '%s'",
			pid, n, Errormsg(err) );
		if( err == EINTR ) continue; 
		Errorcode = JABORT;
		LOGERR_DIE(LOG_ERR) "Pgp_decode: waitpid(%d) failed", pid);
	} 
	DEBUG1("Pgp_decode: pgp pid %d exit status '%s'",
		pid, Decode_status(&procstatus) );
	if( WIFEXITED(procstatus) && (n = WEXITSTATUS(procstatus)) ){
		SNPRINTF(error,errlen)"Pgp_decode: exit status %d",n);
		DEBUG1("Pgp_decode: pgp exited with status %d on host %s", n, FQDNHost_FQDN );
		*pgp_exit_code = n;
		for( i = 0; (n = safestrlen(error)) < errlen - 2 && i < pgp_info->count; ++i ){
			s = pgp_info->list[i];
			SNPRINTF(error+n, errlen-n)"\n %s",s);
			if( !*not_a_ciphertext ){
				*not_a_ciphertext = (strstr(s, "not a ciphertext") != 0);
			}
		}
		status = JFAIL;
		goto error;
	} else if( WIFSIGNALED(procstatus) ){
		n = WTERMSIG(procstatus);
		DEBUG1( "Pgp_decode: pgp died with signal %d, '%s'",
			n, Sigstr(n));
		status = JFAIL;
		goto error;
	}

	for( i = 0; i < pgp_info->count; ++i ){
		s = pgp_info->list[i];
		if( !safestrncmp("Good",s,4) ){
			if( (t = safestrchr(s,'"')) ){
				*t++ = 0;
				if( (s = safestrrchr(t,'"')) ) *s = 0;
				DEBUG1( "Pgp_decode: FROM '%s'", t );
				Set_str_value(from_info,FROM,t);
			}
		}
	}

 error:
	DEBUG1( "Pgp_decode: error '%s'", error );
	if( error_fd[0] >= 0 ) close(error_fd[0]); error_fd[0] = -1;
	if( error_fd[1] >= 0 ) close(error_fd[1]); error_fd[1] = -1;
	if( pgppassfd >= 0) close(pgppassfd); pgppassfd = -1;
	Free_line_list(&env);
	files.count = 0;
	Free_line_list(&files);
	return( status );
}

int Pgp_encode(struct line_list *info, char *tempfile, char *pgpfile,
	struct line_list *pgp_info, char *buffer, int bufflen,
	char *error, int errlen, char *esc_from_id, char *esc_to_id,
	int *pgp_exit_code )
{
	struct line_list env, files;
	plp_status_t procstatus;
	int error_fd[2], status, cnt, n, pid, pgppassfd = -1, i;
	char *s, *t;

	Init_line_list(&env);
	Init_line_list(&files);
	*pgp_exit_code = 0;
	status = 0;
	if( ISNULL(Pgp_path_DYN) ){
		SNPRINTF( error, errlen)
		"Pgp_encode: missing pgp_path info"); 
		status = JFAIL;
		goto error;
	}
	DEBUG1("Pgp_encode: esc_from_id '%s', esc_to_id '%s'",
		esc_from_id, esc_to_id );

	status = 0;
	pgppassfd = error_fd[0] = error_fd[1] = -1;

	error[0] = 0;
	pgppassfd = Pgp_get_pgppassfd( info, error, errlen );
	if( error[0] ){
		status = JFAIL;
		goto error;
	}

	pgpfile = safestrdup2(tempfile,".pgp",__FILE__,__LINE__);
	Check_max(&Tempfiles,1);
	if( !Debug ) Tempfiles.list[Tempfiles.count++] = pgpfile;

	if( pipe(error_fd) == -1 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO) "Pgp_encode: pipe() failed" );
	}
	Max_open(error_fd[0]); Max_open(error_fd[1]);
	Check_max(&files,10);
	files.count = 0;
	files.list[files.count++] = Cast_int_to_voidstar(0);
	files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);
	files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);

	if( pgppassfd > 0 ){
		Set_decimal_value(&env,"PGPPASSFD",files.count);
		files.list[files.count++] = Cast_int_to_voidstar(pgppassfd);
	}

	/* now we run the PGP code */

	SNPRINTF(buffer,bufflen)
		"$- %s +armorlines=0 +verbose=0 +force +batch -sea '%s' '$%%%s' -u '$%%%s' -o %s",
		Pgp_path_DYN, tempfile, esc_to_id, esc_from_id, pgpfile );

	if( (pid = Make_passthrough(buffer, 0, &files, 0, &env )) < 0 ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_INFO) "Pgp_encode: fork failed");
	}

	DEBUG1("Pgp_encode: pgp pid %d", pid );
	files.count = 0;
	Free_line_list(&files);
	Free_line_list(&env);
	close(error_fd[1]); error_fd[1] = -1;
	if( pgppassfd >= 0) close(pgppassfd); pgppassfd = -1;

	/* now we read error info from pgp */

	n = 0;
	while( n < bufflen-1
		&& (cnt = read( error_fd[0], buffer+n, bufflen-1-n )) > 0 ){
		buffer[n+cnt] = 0;
		while( (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
			DEBUG1("Pgp_encode: pgp output '%s'", buffer );
			while( cval(buffer) && !isprint(cval(buffer)) ){
				memmove(buffer,buffer+1,safestrlen(buffer+1)+1);
			}
			/* rip out extra spaces */
			for( t = buffer; *t ; ){
				if( isspace(cval(t)) && isspace(cval(t+1)) ){
					memmove(t,t+1,safestrlen(t+1)+1);
				} else {
					++t;
				}
			}
			if( buffer[0] ){
				DEBUG1("Pgp_encode: pgp final output '%s'", buffer );
				Add_line_list( pgp_info, buffer, 0, 0, 0 );
			}
			memmove(buffer,s,safestrlen(s)+1);
		}
	}
	close(error_fd[0]); error_fd[0] = -1;

	/* wait for pgp to exit and check status */
	while( (n = waitpid(pid,&procstatus,0)) != pid ){
		int err = errno;
		DEBUG1("Pgp_encode: waitpid(%d) returned %d, err '%s', status '%s'",
			pid, n, Errormsg(err), Decode_status(&procstatus));
		if( err == EINTR ) continue; 
		Errorcode = JABORT;
		LOGERR_DIE(LOG_ERR) "Pgp_encode: waitpid(%d) failed", pid);
	} 
	DEBUG1("Pgp_encode: pgp pid %d exit status '%s'",
		pid, Decode_status(&procstatus) );
	if(DEBUGL1)Dump_line_list("Pgp_encode: pgp_info", pgp_info);
	if( WIFEXITED(procstatus) && (n = WEXITSTATUS(procstatus)) ){
		SNPRINTF(error,errlen)
			"Pgp_encode: pgp exited with status %d on host %s", n, FQDNHost_FQDN );
		*pgp_exit_code = n;
		for( i = 0; (n = safestrlen(error)) < errlen - 2 && i < pgp_info->count; ++i ){
			s = pgp_info->list[i];
			SNPRINTF(error+n, errlen-n)"\n %s",s);
		}
		status = JFAIL;
		goto error;
	} else if( WIFSIGNALED(procstatus) ){
		n = WTERMSIG(procstatus);
		SNPRINTF(error,errlen)
		"Pgp_encode: pgp died with signal %d, '%s'",
			n, Sigstr(n));
		status = JFAIL;
		goto error;
	}

 error:
	DEBUG1("Pgp_encode: status %d, error '%s'", status, error );
	if( error_fd[0] >= 0 ) close(error_fd[0]); error_fd[0] = -1;
	if( error_fd[1] >= 0 ) close(error_fd[1]); error_fd[1] = -1;
	if( pgppassfd >= 0) close(pgppassfd); pgppassfd = -1;
	Free_line_list(&env);
	files.count = 0;
	Free_line_list(&files);
	return( status );
}


/*
 * 
 * The following routines simply implement the encryption and transfer of
 * the files and/or values
 * 
 * By default, when sending a command,  the file will contain:
 *   key=value lines.
 *   KEY           PURPOSE
 *   client        client or user name
 *   from          originator - server if forwarding, client otherwise
 *   command       command to send
 * 
 */

/*************************************************************
 * PGP Transmission
 * 
 * Configuration:
 *   pgp_id            for client to server
 *   pgp_forward_id    for server to server
 *   pgp_forward_id    for server to server
 *   pgp_path          path to pgp program
 *   pgp_passphrasefile     user passphrase file (relative to $HOME/.pgp)
 *   pgp_server_passphrasefile server passphrase file
 * User ENVIRONMENT Variables
 *   PGPPASS           - passphrase
 *   PGPPASSFD         - passfd if set up
 *   PGPPASSFILE       - passphrase in this file
 *   HOME              - for passphrase relative to thie file
 * 
 *  We encrypt and sign the file,  then send it to the other end.
 *  It will decrypt it, and then send the data back, encrypted with
 *  our public key.
 * 
 *  Keyrings must contain keys for users.
 *************************************************************/

int Pgp_send( int *sock, int transfer_timeout, char *tempfile,
	char *error, int errlen,
	struct security *security, struct line_list *info )
{
	char *pgpfile;
	struct line_list pgp_info;
	char buffer[LARGEBUFFER];
	int status, i, tempfd, len, n, fd;
	struct stat statb;
	char *from, *destination, *s, *t;
	int pgp_exit_code = 0;
	int not_a_ciphertext = 0;

	DEBUG1("Pgp_send: sending on socket %d", *sock );

	len = 0;
	error[0] = 0;
	from = Find_str_value( info, FROM, Value_sep);
	destination = Find_str_value( info, ID, Value_sep );

	tempfd = -1;

	Init_line_list( &pgp_info );
    pgpfile = safestrdup2(tempfile,".pgp",__FILE__,__LINE__); 
    Check_max(&Tempfiles,1);
    Tempfiles.list[Tempfiles.count++] = pgpfile;

	status = Pgp_encode( info, tempfile, pgpfile, &pgp_info,
		buffer, sizeof(buffer), error, errlen, 
        from, destination, &pgp_exit_code );

	if( status ){
		goto error;
	}
	if( !Is_server && Verbose ){
		for( i = 0; i < pgp_info.count; ++i ){
			if( Write_fd_str(1,pgp_info.list[i]) < 0
				|| Write_fd_str(1,"\n") < 0 ) cleanup(0);
		}
	}
	Free_line_list(&pgp_info);

	if( (tempfd = Checkread(pgpfile,&statb)) < 0 ){
		SNPRINTF(error,errlen)
			"Pgp_send: cannot open '%s' - %s", pgpfile, Errormsg(errno) );
		goto error;
	}

	DEBUG1("Pgp_send: encrypted file size '%0.0f'", (double)(statb.st_size) );
	SNPRINTF(buffer,sizeof(buffer))"%0.0f\n",(double)(statb.st_size) );
	Write_fd_str(*sock,buffer);

	while( (len = read( tempfd, buffer, sizeof(buffer)-1 )) > 0 ){
		buffer[len] = 0;
		DEBUG4("Pgp_send: file information '%s'", buffer );
		if( write( *sock, buffer, len) != len ){
			SNPRINTF(error,errlen)
			"Pgp_send: write to socket failed - %s", Errormsg(errno) );
			goto error;
		}
	}

	DEBUG2("Pgp_send: sent file" );
	close(tempfd); tempfd = -1;
	/* we close the writing side */
	shutdown( *sock, 1 );
	if( (tempfd = Checkwrite(pgpfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
		SNPRINTF(error,errlen)
			"Pgp_send: open '%s' for write failed - %s", pgpfile, Errormsg(errno));
		goto error;
	}
	DEBUG2("Pgp_send: starting read");
	len = 0;
	while( (n = read(*sock,buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUG4("Pgp_send: read '%s'", buffer);
		if( write(tempfd,buffer,n) != n ){
			SNPRINTF(error,errlen)
			"Pgp_send: write '%s' failed - %s", tempfile, Errormsg(errno) );
			goto error;
		}
		len += n;
	}
	close( tempfd ); tempfd = -1;

	DEBUG2("Pgp_send: total %d bytes status read", len );

	Free_line_list(&pgp_info);

	/* decode the PGP file into the tempfile */
	if( len ){
		status = Pgp_decode( info, tempfile, pgpfile, &pgp_info,
			buffer, sizeof(buffer), error, errlen, from, info,
			&pgp_exit_code, &not_a_ciphertext );
		if( not_a_ciphertext ){
			DEBUG2("Pgp_send: not a ciphertext" );
			if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
				SNPRINTF(error,errlen)
				"Pgp_send: open '%s' for write failed - %s",
					tempfile, Errormsg(errno));
			}
			if( (fd = Checkread(pgpfile,&statb)) < 0){
				SNPRINTF(error,errlen)
				"Pgp_send: open '%s' for write failed - %s",
					pgpfile, Errormsg(errno));
			}
			if( error[0] ){
				Write_fd_str(tempfd,error);
				Write_fd_str(tempfd,"\n Contents -\n");
			}
			error[0] = 0;
			len = 0;
			while( (n = read(fd, buffer+len, sizeof(buffer)-len-1)) > 0 ){
				DEBUG2("Pgp_send: read '%s'", buffer );
				while( (s = strchr( buffer, '\n')) ){
					*s++ = 0;
					for( t = buffer; *t; ++t ){
						if( !isprint(cval(t)) ) *t = ' ';
					}
					SNPRINTF(error,errlen)"  %s\n", buffer);
					Write_fd_str(tempfd, error );
					DEBUG2("Pgp_send: wrote '%s'", error );
					memmove(buffer,s,safestrlen(s)+1);
				}
				len = safestrlen(buffer);
			}
			DEBUG2("Pgp_send: done" );
			error[0] = 0;
			close(fd); fd = -1;
			close(tempfd); tempfd = -1;
			error[0] = 0;
		}
	}

 error:
	if( error[0] ){
		char *s, *end;
		DEBUG2("Pgp_send: writing error to file '%s'", error );
		if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
			SNPRINTF(error,errlen)
			"Pgp_send: open '%s' for write failed - %s",
				tempfile, Errormsg(errno));
		}
		for( s = error; !ISNULL(s); s = end ){
			if( (end = strchr( error, '\n')) ) *end++ = 0;
			SNPRINTF(buffer,sizeof(buffer)) "%s\n", s);
			Write_fd_str(tempfd, buffer );
			DEBUG2("Pgp_send: wrote '%s'", buffer );
		}
		close( tempfd ); tempfd = -1;
		error[0] = 0;
	}
	Free_line_list(&pgp_info);
	return(status);
}

int Pgp_receive( int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile )
{
	char *pgpfile;
	int tempfd, status, n;
	char buffer[LARGEBUFFER];
	struct stat statb;
	struct line_list pgp_info;
	double len;
	char *id = Find_str_value( info, ID, Value_sep );
	char *from = 0;
	int pgp_exit_code = 0;
	int not_a_ciphertext = 0;

	Init_line_list(&pgp_info);
	tempfd = -1;
	errmsg[0] = 0;

	pgpfile = safestrdup2(tempfile,".pgp",__FILE__,__LINE__);
	Check_max(&Tempfiles,1);
	Tempfiles.list[Tempfiles.count++] = pgpfile;

	if( id == 0 ){
		status = JABORT;
		SNPRINTF( errmsg, errlen) "Pgp_receive: no pgp_id or auth_id value");
		goto error;
	}

	if( Write_fd_len( *sock, "", 1 ) < 0 ){
		status = JABORT;
		SNPRINTF( errmsg, errlen) "Pgp_receive: ACK 0 write error - %s",
			Errormsg(errno) );
		goto error;
	}


	if( (tempfd = Checkwrite(pgpfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0 ){
		status = JFAIL;
		SNPRINTF( errmsg, errlen)
			"Pgp_receive: reopen of '%s' for write failed - %s",
			pgpfile, Errormsg(errno) );
		goto error;
	}
	DEBUGF(DRECV4)("Pgp_receive: starting read from %d", *sock );
	while( (n = read(*sock, buffer,1)) > 0 ){
		/* handle old and new format of file */
		buffer[n] = 0;
		DEBUGF(DRECV4)("Pgp_receive: remote read '%d' '%s'", n, buffer );
		if( isdigit(cval(buffer)) ) continue;
		if( isspace(cval(buffer)) ) break;
		if( write( tempfd,buffer,1 ) != 1 ){
			status = JFAIL;
			SNPRINTF( errmsg, errlen)
				"Pgp_receive: bad write to '%s' - '%s'",
				tempfile, Errormsg(errno) );
			goto error;
		}
		break;
	}
	while( (n = read(*sock, buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUGF(DRECV4)("Pgp_receive: remote read '%d' '%s'", n, buffer );
		if( write( tempfd,buffer,n ) != n ){
			status = JFAIL;
			SNPRINTF( errmsg, errlen)
				"Pgp_receive: bad write to '%s' - '%s'",
				tempfile, Errormsg(errno) );
			goto error;
		}
	}
	if( n < 0 ){
		status = JFAIL;
		SNPRINTF( errmsg, errlen)
			"Pgp_receive: bad read from socket - '%s'",
			Errormsg(errno) );
		goto error;
	}
	close(tempfd); tempfd = -1;
	DEBUGF(DRECV4)("Pgp_receive: end read" );

	status = Pgp_decode(info, tempfile, pgpfile, &pgp_info,
		buffer, sizeof(buffer), errmsg, errlen, id, header_info,
		&pgp_exit_code, &not_a_ciphertext );
	if( status ) goto error;

	DEBUGFC(DRECV1)Dump_line_list("Pgp_receive: header_info", header_info );

	from = Find_str_value(header_info,FROM,Value_sep);
	if( from == 0 ){
		status = JFAIL;
		SNPRINTF( errmsg, errlen)
			"Pgp_receive: no 'from' information" );
		goto error;
	}

	status = Do_secure_work( jobsize, from_server, tempfile, header_info );

	Free_line_list( &pgp_info);
 	status = Pgp_encode(info, tempfile, pgpfile, &pgp_info,
		buffer, sizeof(buffer), errmsg, errlen,
		id, from, &pgp_exit_code );
	if( status ) goto error;

	/* we now have the encoded output */
	if( (tempfd = Checkread(pgpfile,&statb)) < 0 ){
		status = JFAIL;
		SNPRINTF( errmsg, errlen)
			"Pgp_receive: reopen of '%s' for read failed - %s",
			tempfile, Errormsg(errno) );
		goto error;
	}
	len = statb.st_size;
	DEBUGF(DRECV1)( "Pgp_receive: return status encoded size %0.0f",
		len);
	while( (n = read(tempfd, buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUGF(DRECV4)("Pgp_receive: sending '%d' '%s'", n, buffer );
		if( write( *sock,buffer,n ) != n ){
			status = JFAIL;
			SNPRINTF( errmsg, errlen)
				"Pgp_receive: bad write to socket - '%s'",
				Errormsg(errno) );
			goto error;
		}
	}
	if( n < 0 ){
		status = JFAIL;
		SNPRINTF( errmsg, errlen)
			"Pgp_receive: read '%s' failed - %s",
			tempfile, Errormsg(errno) );
		goto error;
	}

 error:
	if( tempfd>=0) close(tempfd); tempfd = -1;
	Free_line_list(&pgp_info);
	return(status);
}



 struct security SecuritySupported[] = {
	/* name, config_name, flags,
        client  connect, send, send_done
		server  accept, receive, receive_done
	*/
#if defined(KERBEROS)
# if defined(MIT_KERBEROS4)
	{ "kerberos4", "kerberos", IP_SOCKET_ONLY, Send_krb4_auth, 0,0,0 },
# endif
	{ "kerberos*", "kerberos", IP_SOCKET_ONLY, 0, Krb5_send, 0, Krb5_receive },
#endif

	/* { "test", "test", 0, Test_connect,0, Test_accept,0 }, */
	{ "test", "test", 0, 0, Test_send, 0, Test_receive },
	{ "md5", "md5",   0, 0, md5_send, 0, md5_receive },
	{ "pgp", "pgp",   0, 0, Pgp_send, 0, Pgp_receive },
#ifdef SSL_ENABLE
	{ "ssl", "ssl",   0, 0, Ssl_send, 0, Ssl_receive },
#endif

	{0,0,0,
		0,0,
		0,0}
};

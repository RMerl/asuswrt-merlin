/***********************************************************************
*
* winbind.c
*
* WINBIND plugin for pppd.  Performs PAP, CHAP, MS-CHAP, MS-CHAPv2
* authentication using WINBIND to contact a NT-style PDC.
* 
* Based on the structure of the radius module.
*
* Copyright (C) 2003 Andrew Bartlet <abartlet@samba.org>
*
* Copyright 1999 Paul Mackerras, Alan Curry. 
* (pipe read code from passpromt.c)
*
* Copyright (C) 2002 Roaring Penguin Software Inc.
*
* Based on a patch for ipppd, which is:
*    Copyright (C) 1996, Matjaz Godec <gody@elgo.si>
*    Copyright (C) 1996, Lars Fenneberg <in5y050@public.uni-hamburg.de>
*    Copyright (C) 1997, Miguel A.L. Paraz <map@iphil.net>
*
* Uses radiusclient library, which is:
*    Copyright (C) 1995,1996,1997,1998 Lars Fenneberg <lf@elemental.net>
*    Copyright (C) 2002 Roaring Penguin Software Inc.
*
* MPPE support is by Ralf Hofmann, <ralf.hofmann@elvido.net>, with
* modification from Frank Cusack, <frank@google.com>.
*
* Updated on 2003-12-12 to support updated PPP plugin API from latest CVS
*    Copyright (C) 2003, Sean E. Millichamp <sean at bruenor dot org>
*
* This plugin may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
***********************************************************************/

#include "pppd.h"
#include "chap-new.h"
#include "chap_ms.h"
#ifdef MPPE
#include "md5.h"
#endif
#include "fsm.h"
#include "ipcp.h"
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#define BUF_LEN 1024

#define NOT_AUTHENTICATED 0
#define AUTHENTICATED 1

static char *ntlm_auth = NULL;

static int set_ntlm_auth(char **argv)
{
	char *p;

	p = argv[0];
	if (p[0] != '/') {
		option_error("ntlm_auth-helper argument must be full path");
		return 0;
	}
	p = strdup(p);
	if (p == NULL) {
		novm("ntlm_auth-helper argument");
		return 0;
	}
	if (ntlm_auth != NULL)
		free(ntlm_auth);
	ntlm_auth = p;
	return 1;
}

static option_t Options[] = {
	{ "ntlm_auth-helper", o_special, (void *) &set_ntlm_auth,
	  "Path to ntlm_auth executable", OPT_PRIV },
	{ NULL }
};

static int
winbind_secret_check(void);

static int winbind_pap_auth(char *user,
			   char *passwd,
			   char **msgp,
			   struct wordlist **paddrs,
			   struct wordlist **popts);
static int winbind_chap_verify(char *user, char *ourname, int id,
			       struct chap_digest_type *digest,
			       unsigned char *challenge,
			       unsigned char *response,
			       char *message, int message_space);
static int winbind_allowed_address(u_int32_t addr); 

char pppd_version[] = VERSION;

/**********************************************************************
* %FUNCTION: plugin_init
* %ARGUMENTS:
*  None
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Initializes WINBIND plugin.
***********************************************************************/
void
plugin_init(void)
{
    pap_check_hook = winbind_secret_check;
    pap_auth_hook = winbind_pap_auth;

    chap_check_hook = winbind_secret_check;
    chap_verify_hook = winbind_chap_verify;

    allowed_address_hook = winbind_allowed_address;

    /* Don't ask the peer for anything other than MS-CHAP or MS-CHAP V2 */
    chap_mdtype_all &= (MDTYPE_MICROSOFT_V2 | MDTYPE_MICROSOFT);
    
    add_options(Options);

    info("WINBIND plugin initialized.");
}

/**
 Routine to get hex characters and turn them into a 16 byte array.
 the array can be variable length, and any non-hex-numeric
 characters are skipped.  "0xnn" or "0Xnn" is specially catered
 for.

 valid examples: "0A5D15"; "0x15, 0x49, 0xa2"; "59\ta9\te3\n"

**/

/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   
   Copyright (C) Andrew Tridgell 1992-2001
   Copyright (C) Simo Sorce      2001-2002
   Copyright (C) Martin Pool     2003
   
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

size_t strhex_to_str(char *p, size_t len, const char *strhex)
{
	size_t i;
	size_t num_chars = 0;
	unsigned char   lonybble, hinybble;
	const char     *hexchars = "0123456789ABCDEF";
	char           *p1 = NULL, *p2 = NULL;

	for (i = 0; i < len && strhex[i] != 0; i++) {
		if (strncmp(hexchars, "0x", 2) == 0) {
			i++; /* skip two chars */
			continue;
		}

		if (!(p1 = strchr(hexchars, toupper(strhex[i]))))
			break;

		i++; /* next hex digit */

		if (!(p2 = strchr(hexchars, toupper(strhex[i]))))
			break;

		/* get the two nybbles */
		hinybble = (p1 - hexchars);
		lonybble = (p2 - hexchars);

		p[num_chars] = (hinybble << 4) | lonybble;
		num_chars++;

		p1 = NULL;
		p2 = NULL;
	}
	return num_chars;
}

static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Encode a base64 string into a malloc()ed string caller to free.
 *
 *From SQUID: adopted from http://ftp.sunet.se/pub2/gnu/vm/base64-encode.c with adjustments
 **/
char * base64_encode(const char *data)
{
	size_t out_cnt = 0;
	size_t len = strlen(data);
	size_t output_len = 4 * ((len + 2) / 3) + 2;
	const unsigned char *ptr = (const unsigned char *) data;
	char *result = malloc(output_len); /* get us plenty of space */
	unsigned int bits;

	for (; len >= 3; len -= 3) {
		bits = (ptr[0] << 16) + (ptr[1] << 8) + ptr[2];
		ptr += 3;
		result[out_cnt++] = b64[bits >> 18];
		result[out_cnt++] = b64[(bits >> 12) & 0x3f];
		result[out_cnt++] = b64[(bits >> 6) & 0x3f];
		result[out_cnt++] = b64[bits & 0x3f];
	}
	if (len != 0) {
		bits = ptr[0] << 16;
		if (len > 1)
			bits |= ptr[1] << 8;
		result[out_cnt++] = b64[bits >> 18];
		result[out_cnt++] = b64[(bits >> 12) & 0x3f];
		result[out_cnt++] = (len > 1)? b64[(bits >> 6) & 0x3f]: '=';
		result[out_cnt++] = '=';
	}

	result[out_cnt] = '\0';	/* terminate */
	return result;
}

unsigned int run_ntlm_auth(const char *username, 
			   const char *domain, 
			   const char *full_username,
			   const char *plaintext_password,
			   const u_char *challenge,
			   size_t challenge_length,
			   const u_char *lm_response, 
			   size_t lm_response_length,
			   const u_char *nt_response, 
			   size_t nt_response_length,
			   u_char nt_key[16], 
			   char **error_string) 
{
	
	pid_t forkret;
        int child_in[2];
        int child_out[2];
	int status;

	int authenticated = NOT_AUTHENTICATED; /* not auth */
	int got_user_session_key = 0; /* not got key */

	char buffer[1024];

	FILE *pipe_in;
	FILE *pipe_out;
	
	int i;
	char *challenge_hex;
	char *lm_hex_hash;
	char *nt_hex_hash;

	/* First see if we have a program to run... */
	if (ntlm_auth == NULL)
		return NOT_AUTHENTICATED;

        /* Make first child */
        if (pipe(child_out) == -1) {
                error("pipe creation failed for child OUT!");
		return NOT_AUTHENTICATED;
        }

        if (pipe(child_in) == -1) {
                error("pipe creation failed for child IN!");
		return NOT_AUTHENTICATED;
        }

        forkret = safe_fork(child_in[0], child_out[1], 2);
        if (forkret == -1) {
		if (error_string) {
			*error_string = strdup("fork failed!");
		}

                return NOT_AUTHENTICATED;
        }

	if (forkret == 0) {
		/* child process */
		uid_t uid;

		close(child_out[0]);
		close(child_in[1]);

		/* run winbind as the user that invoked pppd */
		setgid(getgid());
		uid = getuid();
		if (setuid(uid) == -1 || getuid() != uid)
			fatal("pppd/winbind: could not setuid to %d: %m", uid);
		execl("/bin/sh", "sh", "-c", ntlm_auth, NULL);  
		fatal("pppd/winbind: could not exec /bin/sh: %m");
	}

        /* parent */
	close(child_out[1]);
	close(child_in[0]);

	/* Need to write the User's info onto the pipe */

	pipe_in = fdopen(child_in[1], "w");

	pipe_out = fdopen(child_out[0], "r");

	/* look for session key coming back */

	if (username) {
		char *b64_username = base64_encode(username);
		fprintf(pipe_in, "Username:: %s\n", b64_username);
		free(b64_username);
	}

	if (domain) {
		char *b64_domain = base64_encode(domain);
		fprintf(pipe_in, "NT-Domain:: %s\n", b64_domain);
		free(b64_domain);
	}

	if (full_username) {
		char *b64_full_username = base64_encode(full_username);
		fprintf(pipe_in, "Full-Username:: %s\n", b64_full_username);
		free(b64_full_username);
	}

	if (plaintext_password) {
		char *b64_plaintext_password = base64_encode(plaintext_password);
		fprintf(pipe_in, "Password:: %s\n", b64_plaintext_password);
		free(b64_plaintext_password);
	}

	if (challenge_length) {
		fprintf(pipe_in, "Request-User-Session-Key: yes\n");

		challenge_hex = malloc(challenge_length*2+1);
		
		for (i = 0; i < challenge_length; i++)
			sprintf(challenge_hex + i * 2, "%02X", challenge[i]);
		
		fprintf(pipe_in, "LANMAN-Challenge: %s\n", challenge_hex);
		free(challenge_hex);
	}
	
	if (lm_response_length) {
		lm_hex_hash = malloc(lm_response_length*2+1);
		
		for (i = 0; i < lm_response_length; i++)
			sprintf(lm_hex_hash + i * 2, "%02X", lm_response[i]);
		
		fprintf(pipe_in, "LANMAN-response: %s\n", lm_hex_hash);
		free(lm_hex_hash);
	}
	
	if (nt_response_length) {
		nt_hex_hash = malloc(nt_response_length*2+1);
		
		for (i = 0; i < nt_response_length; i++)
			sprintf(nt_hex_hash + i * 2, "%02X", nt_response[i]);
		
		fprintf(pipe_in, "NT-response: %s\n", nt_hex_hash);
		free(nt_hex_hash);
	}
	
	fprintf(pipe_in, ".\n");
	fflush(pipe_in);
	
	while (fgets(buffer, sizeof(buffer)-1, pipe_out) != NULL) {
		char *message, *parameter;
		if (buffer[strlen(buffer)-1] != '\n') {
			break;
		}
		buffer[strlen(buffer)-1] = '\0';
		message = buffer;

		if (!(parameter = strstr(buffer, ": "))) {
			break;
		}
		
		parameter[0] = '\0';
		parameter++;
		parameter[0] = '\0';
		parameter++;
		
		if (strcmp(message, ".") == 0) {
			/* end of sequence */
			break;
		} else if (strcasecmp(message, "Authenticated") == 0) {
			if (strcasecmp(parameter, "Yes") == 0) {
				authenticated = AUTHENTICATED;
			} else {
				notice("Winbind has declined authentication for user!");
				authenticated = NOT_AUTHENTICATED;
			}
		} else if (strcasecmp(message, "User-session-key") == 0) {
			/* length is the number of characters to parse */
			if (nt_key) { 
				if (strhex_to_str(nt_key, 32, parameter) == 16) {
					got_user_session_key = 1;
				} else {
					notice("NT session key for user was not 16 bytes!");
				}
			}
		} else if (strcasecmp(message, "Error") == 0) {
			authenticated = NOT_AUTHENTICATED;
			if (error_string)
				*error_string = strdup(parameter);
		} else if (strcasecmp(message, "Authentication-Error") == 0) {
			authenticated = NOT_AUTHENTICATED;
			if (error_string)
				*error_string = strdup(parameter);
		} else {
			notice("unrecognised input from ntlm_auth helper - %s: %s", message, parameter); 
		}
	}

        /* parent */
        if (close(child_out[0]) == -1) {
                notice("error closing pipe?!? for child OUT[0]");
                return NOT_AUTHENTICATED;
        }

       /* parent */
        if (close(child_in[1]) == -1) {
                notice("error closing pipe?!? for child IN[1]");
                return NOT_AUTHENTICATED;
        }

	while ((wait(&status) == -1) && errno == EINTR)
                ;

	if ((authenticated == AUTHENTICATED) && nt_key && !got_user_session_key) {
		notice("Did not get user session key, despite being authenticated!");
		return NOT_AUTHENTICATED;
	}
	return authenticated;
}

/**********************************************************************
* %FUNCTION: winbind_secret_check
* %ARGUMENTS:
*  None
* %RETURNS:
*  0 if we don't have an ntlm_auth program to run, otherwise 1.
* %DESCRIPTION:
* Tells pppd that we will try to authenticate the peer, and not to
* worry about looking in /etc/ppp/ *-secrets
***********************************************************************/
static int
winbind_secret_check(void)
{
	return ntlm_auth != NULL;
}

/**********************************************************************
* %FUNCTION: winbind_pap_auth
* %ARGUMENTS:
*  user -- user-name of peer
*  passwd -- password supplied by peer
*  msgp -- Message which will be sent in PAP response
*  paddrs -- set to a list of possible peer IP addresses
*  popts -- set to a list of additional pppd options
* %RETURNS:
*  1 if we can authenticate, -1 if we cannot.
* %DESCRIPTION:
* Performs PAP authentication using WINBIND
***********************************************************************/
static int
winbind_pap_auth(char *user,
		char *password,
		char **msgp,
		struct wordlist **paddrs,
		struct wordlist **popts)
{
	if (run_ntlm_auth(NULL, NULL, user, password, NULL, 0, NULL, 0, NULL, 0, NULL, msgp) == AUTHENTICATED) {
		return 1;
	} 
	return -1;
}

/**********************************************************************
* %FUNCTION: winbind_chap_auth
* %ARGUMENTS:
*  user -- user-name of peer
*  remmd -- hash received from peer
*  remmd_len -- length of remmd
*  cstate -- pppd's chap_state structure
* %RETURNS:
*  AUTHENTICATED (1) if we can authenticate, NOT_AUTHENTICATED (0) if we cannot.
* %DESCRIPTION:
* Performs MS-CHAP and MS-CHAPv2 authentication using WINBIND.
***********************************************************************/

static int 
winbind_chap_verify(char *user, char *ourname, int id,
		    struct chap_digest_type *digest,
		    unsigned char *challenge,
		    unsigned char *response,
		    char *message, int message_space)
{
	int challenge_len, response_len;
	char domainname[256];
	char *domain;
	char *username;
	char *p;
	char saresponse[MS_AUTH_RESPONSE_LENGTH+1];

	/* The first byte of each of these strings contains their length */
	challenge_len = *challenge++;
	response_len = *response++;
	
	/* remove domain from "domain\username" */
	if ((username = strrchr(user, '\\')) != NULL)
		++username;
	else
		username = user;
	
	strlcpy(domainname, user, sizeof(domainname));
	
	/* remove domain from "domain\username" */
	if ((p = strrchr(domainname, '\\')) != NULL) {
		*p = '\0';
		domain = domainname;
	} else {
		domain = NULL;
	}
	
	/*  generate MD based on negotiated type */
	switch (digest->code) {
		
	case CHAP_MICROSOFT:
	{
		char *error_string = NULL;
		u_char *nt_response = NULL;
		u_char *lm_response = NULL;
		int nt_response_size = 0;
		int lm_response_size = 0;
		u_char session_key[16];
		
		if (response_len != MS_CHAP_RESPONSE_LEN)
			break;			/* not even the right length */
		
		/* Determine which part of response to verify against */
		if (response[MS_CHAP_USENT]) {
			nt_response = &response[MS_CHAP_NTRESP];
			nt_response_size = MS_CHAP_NTRESP_LEN;
		} else {
#ifdef MSLANMAN
			lm_response = &response[MS_CHAP_LANMANRESP];
			lm_response_size = MS_CHAP_LANMANRESP_LEN;
#else
			/* Should really propagate this into the error packet. */
			notice("Peer request for LANMAN auth not supported");
			return NOT_AUTHENTICATED;
#endif /* MSLANMAN */
		}
		
		/* ship off to winbind, and check */
		
		if (run_ntlm_auth(username, 
				  domain,
				  NULL,
				  NULL,
				  challenge, challenge_len,
				  lm_response, lm_response_size,
				  nt_response, nt_response_size,
				  session_key,
				  &error_string) == AUTHENTICATED) {
			mppe_set_keys(challenge, session_key);
			slprintf(message, message_space, "Access granted");
			return AUTHENTICATED;
			
		} else {
			if (error_string) {
				notice(error_string);
				free(error_string);
			}
			slprintf(message, message_space, "E=691 R=1 C=%0.*B V=0",
				 challenge_len, challenge);
			return NOT_AUTHENTICATED;
		}
		break;
	}
	
	case CHAP_MICROSOFT_V2:
	{
		u_char Challenge[8];
		u_char session_key[MD4_SIGNATURE_SIZE];
		char *error_string = NULL;
		
		if (response_len != MS_CHAP2_RESPONSE_LEN)
			break;			/* not even the right length */
		
		ChallengeHash(&response[MS_CHAP2_PEER_CHALLENGE], challenge,
			      user, Challenge);
		
		/* ship off to winbind, and check */
		
		if (run_ntlm_auth(username, 
				  domain, 
				  NULL,
				  NULL,
				  Challenge, 8,
				  NULL, 0,
				  &response[MS_CHAP2_NTRESP],
				  MS_CHAP2_NTRESP_LEN,
				  session_key,
				  &error_string) == AUTHENTICATED) {
			
			GenerateAuthenticatorResponse(session_key,
				&response[MS_CHAP2_NTRESP],
				&response[MS_CHAP2_PEER_CHALLENGE],
				challenge, user, saresponse);
			mppe_set_keys2(session_key, &response[MS_CHAP2_NTRESP],
				       MS_CHAP2_AUTHENTICATOR);
			if (response[MS_CHAP2_FLAGS]) {
				slprintf(message, message_space, "S=%s", saresponse);
			} else {
				slprintf(message, message_space, "S=%s M=%s",
					 saresponse, "Access granted");
			}
			return AUTHENTICATED;
			
		} else {
			if (error_string) {
				notice(error_string);
				slprintf(message, message_space, "E=691 R=1 C=%0.*B V=0 M=%s",
					 challenge_len, challenge, error_string);
				free(error_string);
			} else {
				slprintf(message, message_space, "E=691 R=1 C=%0.*B V=0 M=%s",
					 challenge_len, challenge, "Access denied");
			}
			return NOT_AUTHENTICATED;
		}
		break;
	}
	
	default:
		error("WINBIND: Challenge type %u unsupported", digest->code);
	}
	return NOT_AUTHENTICATED;
}

static int 
winbind_allowed_address(u_int32_t addr) 
{
	ipcp_options *wo = &ipcp_wantoptions[0];
	if (wo->hisaddr !=0 && wo->hisaddr == addr) {
		return 1;
	}
	return -1;
}

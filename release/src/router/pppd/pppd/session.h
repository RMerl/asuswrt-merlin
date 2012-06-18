/*
 * session.c - PPP session control.
 *
 * Copyright (c) 2007 Diego Rivera. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 3. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __SESSION_H
#define __SESSION_H

#define SESS_AUTH  1	/* Check User Authentication */
#define SESS_ACCT  2	/* Check Account Validity */

/* Convenience parameter to do the whole enchilada */
#define SESS_ALL   (SESS_AUTH | SESS_ACCT)

/*
 * int session_start(...)
 *
 * Start a session, performing any necessary validations.
 *
 * Parameters:
 * 	const int flags :
 * 		Any combination of the SESS_XXX flags, to indicate what the function
 *		should do as part of its checks
 *
 *	const char* user :
 *		The username to validate.  May safely be null.
 *
 *	const char* passwd :
 *		The password to validate the user with. May safely be null.
 *
 *	const char* tty :
 *		The TTY the user is connected on. May safely be null.
 *
 *	char** msg :
 *		A char* to return an error or success message.  This message will be returned
 *		regardless of the result.  May safely be null.
 *
 * Return Value:
 * 	Zero value for failure, non-zero value for successful session verification.
 */
int
session_start(const int flags, const char* user, const char* passwd, const char* tty, char** msg);

/* Added these macros for convenience... */
#define session_auth(user, pass, tty, msg) \
	session_start(SESS_AUTH, user, pass, tty, msg)

#define session_check(user, pass, tty, msg) \
	session_start(SESS_ACCT, user, pass, tty, msg)

#define session_full(user, pass, tty, msg) \
	session_start(SESS_ALL, user, pass, tty, msg)

/*
 * void session_end(...)
 *
 * End a previously-started session.
 *
 * Parameters:
 *	const char* tty :
 *		The TTY the user is connected on. May safely be null.
 */
void
session_end(const char* tty);

#endif

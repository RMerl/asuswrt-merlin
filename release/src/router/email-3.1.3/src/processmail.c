/**
    eMail is a command line SMTP client.

    Copyright (C) 2001 - 2008 email by Dean Jones
    Software supplied and written by http://www.cleancode.org

    This file is part of eMail.

    eMail is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    eMail is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with eMail; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**/
#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include "email.h"
#include "dnet.h"
#include "utils.h"
#include "smtpcommands.h"
#include "processmail.h"
#include "progress_bar.h"
#include "error.h"

#define CHUNK_BYTES 1500

/**
 * will invoke the path specified to sendmail with any 
 * options specified and it will send the mail via sendmail...
**/
int
processInternal(const char *sm_bin, dstrbuf *msgcon)
{
	int written_bytes=0, bytes = 0;
	struct prbar *bar;
	FILE *open_sendmail;
	char *ptr = msgcon->str;
	dstrbuf *smpath;

	bar = prbarInit(msgcon->len);
	smpath = expandPath(sm_bin);

	open_sendmail = popen(smpath->str, "w");
	dsbDestroy(smpath);
	if (!open_sendmail) {
		fatal("Could not open internal sendmail path: %s", smpath->str);
		return ERROR;
	}

	/* Loop through getting what's out of message and sending it to sendmail */
	while (*ptr != '\0') {
		bytes = strlen(ptr);
		if (bytes > CHUNK_BYTES) {
			bytes = CHUNK_BYTES;
		}
		written_bytes = fwrite(ptr, sizeof(char), bytes, open_sendmail);
		if (Mopts.verbose && bar != NULL) {
			prbarPrint(written_bytes, bar);
		}
		ptr += written_bytes;
	}

	fflush(open_sendmail);
	fclose(open_sendmail);
	prbarDestroy(bar);
	return SUCCESS; 
}

/**
 * Prints a user friendly error message when we encounter and 
 * error with SMTP communications.
**/
static void
printSmtpError()
{
	fprintf(stderr, "\n");
	fatal("Smtp error: %s\n", smtpGetErr());
}

/**
 * Gets the users smtp password from the configuration file.
 * if it does not exist, it will prompt them for it.
**/
static char *
getSmtpPass(void)
{
	char *retval = getConfValue("SMTP_AUTH_PASS");
	if (!retval) {
		retval = getpass("Enter your SMTP Password: ");
	}
	return retval;
}

/**
 * This function will take the FILE and process it via a
 * Remote SMTP server...
**/
int
processRemote(const char *smtp_serv, int smtp_port, dstrbuf *msg)
{
	dsocket *sd;
	int retval=0, bytes;
	char *smtp_auth=NULL;
	char *email_addr=NULL;
	char *use_tls=NULL;
	char *user=NULL, *pass=NULL;
	struct prbar *bar=NULL;
	char nodename[MAXBUF] = { 0 };
	char *ptr = msg->str;
	struct addr *next=NULL;

	email_addr = getConfValue("MY_EMAIL");
	//if (gethostname(nodename, sizeof(nodename) - 1) < 0) {
		snprintf(nodename, sizeof(nodename) - 1, "geek");
	//}

	/* Get other possible configuration values */
	smtp_auth = getConfValue("SMTP_AUTH");
	if (smtp_auth) {
		user = getConfValue("SMTP_AUTH_USER");
		if (!user) {
			fatal("You must set SMTP_AUTH_USER in order to user SMTP_AUTH\n");
			return ERROR;
		}
		pass = getSmtpPass();
		if (!pass) {
			fatal("Failed to get SMTP Password.\n");
			return ERROR;
		}
	}

	bar = prbarInit(msg->len);
	if (Mopts.verbose) {
		printf("Connecting to server %s on port %d\n", smtp_serv, smtp_port);
	}
	sd = dnetConnect(smtp_serv, smtp_port);
	if (sd == NULL) {
		fatal("Could not connect to server: %s on port: %d", 
			smtp_serv, smtp_port);
		return ERROR;
	}

	/* Start SMTP Communications */
	if (smtpInit(sd, nodename) == ERROR) {
		printSmtpError();
		goto end;
	}

	/* Use TLS? */
	use_tls = getConfValue("USE_TLS");
#ifndef HAVE_LIBSSL
	if (use_tls) {
		warning("No SSL support compiled in. Disabling TLS.\n");
		use_tls = NULL;
	}
#endif
	if (use_tls && strcasecmp(use_tls, "true") == 0) {
		if (smtpStartTls(sd) != ERROR) {
			dnetUseTls(sd);
			dnetVerifyCert(sd);
			if (smtpInit(sd, nodename) == ERROR) {
				printSmtpError();
				goto end;
			}
		} else {
			printSmtpError();
			goto end;
		}
	}

	/* See if we're using SMTP_AUTH. */
	if (smtp_auth) {
		retval = smtpInitAuth(sd, smtp_auth, user, pass);
		if (retval == ERROR) {
			printSmtpError();
			goto end;
		}
	}

	retval = smtpSetMailFrom(sd, email_addr);
	if (retval == ERROR) {
		printSmtpError();
		goto end;
	}

	while ((next = (struct addr *)dlGetNext(Mopts.to)) != NULL) {
		retval = smtpSetRcpt(sd, next->email);
		if (retval == ERROR) {
			printSmtpError();
			goto end;
		}
	}
	while ((next = (struct addr *)dlGetNext(Mopts.cc)) != NULL) {
		retval = smtpSetRcpt(sd, next->email);
		if (retval == ERROR) {
			printSmtpError();
			goto end;
		}
	}
	while ((next = (struct addr *)dlGetNext(Mopts.bcc)) != NULL) {
		retval = smtpSetRcpt(sd, next->email);
		if (retval == ERROR) {
			printSmtpError();
			goto end;
		}
	}

	retval = smtpStartData(sd);
	if (retval == ERROR) {
		printSmtpError();
		goto end;
	}
	while (*ptr != '\0') {
		bytes = strlen(ptr);
		if (bytes > CHUNK_BYTES) {
			bytes = CHUNK_BYTES;
		}
		retval = smtpSendData(sd, ptr, bytes);
		if (retval == ERROR) {
			goto end;
		}
		if (Mopts.verbose && bar != NULL) {
			prbarPrint(bytes, bar);
		}
		ptr += bytes;
	}
	retval = smtpEndData(sd);
	if (retval != ERROR) {
		retval = smtpQuit(sd);
	}

end:
	prbarDestroy(bar);
	dnetClose(sd);
	return retval;
}


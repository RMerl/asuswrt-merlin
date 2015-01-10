#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <termios.h>

#include <sys/select.h>
#include <sys/ioctl.h>

#include "dnet.h"
#include "dstrbuf.h"
#include "email.h"
#include "mimeutils.h"

static dstrbuf *errorstr;


/** 
 * Figures out the screen width and prints the message to fit the screen.
 */
void printProgress(const char *msg)
{
	uint i=0;
	char *buf=NULL;
	struct winsize win_size;

	if (!Mopts.verbose) {
		return;
	}
	if (!isatty(STDOUT_FILENO)) {
		return;
	}
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *)&win_size) < 0) {
		return;
	}

	buf = xmalloc(win_size.ws_col+1);
	memset(buf, ' ', win_size.ws_col);
	for (i=0; i < win_size.ws_col; i++) {
		if (*msg == '\0') {
			break;
		}
		buf[i] = *msg++;
	}
	printf("\r%s", buf);
	fflush(stdout);
	free(buf);
}


/**
 * Will generate a string from an error code and return
 * that string as a return value.
 */
char *
smtpGetErr(void)
{
	return errorstr->str;
}

/**
 * Simple interface to copy over buffer into error string
 */
static void
smtpSetErr(const char *buf)
{
	if (!errorstr) {
		errorstr = DSB_NEW;
	}
	dsbClear(errorstr);
	dsbCopy(errorstr, buf);
}

/**
 * Reads a line from the smtp server using sgets().
 * It will continue to read the response until the 4th character
 * in the line is found to be a space.    Per the RFC 821 this means
 * that this will be the last line of response from the SMTP server
 * and the appropirate return value response.
 */
static int
readResponse(dsocket *sd, dstrbuf *buf)
{
	int retval=ERROR;
	int read_size = 0;
	dstrbuf *tmpbuf = DSB_NEW;
	struct timeval tv;
	fd_set rfds;
	char *timeout = getConfValue("TIMEOUT");

	FD_ZERO(&rfds);
	FD_SET(dnetGetSock(sd), &rfds);
	if (timeout) {
		tv.tv_sec = atoi(timeout);
	} else {
		tv.tv_sec = 10;
	}
	tv.tv_usec = 0;
	(void) select(dnetGetSock(sd)+1, &rfds, NULL, NULL, &tv);
	if (FD_ISSET(dnetGetSock(sd), &rfds)) {
		do {
			dsbClear(tmpbuf);
			read_size = dnetReadline(sd, tmpbuf);
			if (dnetErr(sd)) {
				smtpSetErr("Lost connection with SMTP server");
				retval = ERROR;
				break;
			}
			if(read_size == 0) {	//Sam, 2014/11/24
				smtpSetErr("Read socket but no data");
				retval = ERROR;
				break;
			}
			dsbCat(buf, tmpbuf->str);
			retval = SUCCESS;
		/* The last line of a response has a space in the 4th column */
		} while (tmpbuf->str[3] != ' ');
	} else {
		smtpSetErr("Timeout(10) while trying to read from SMTP server");
		retval = ERROR;
	}

	if (retval != ERROR) {
		retval = atoi(tmpbuf->str);
	}

	dsbDestroy(tmpbuf);
	return retval;
}

static int
writeResponse(dsocket *sd, char *line, ...)
{
	va_list vp;
	int sval, size=MAXBUF, bytes=0;
	struct timeval tv;
	fd_set wfds;
	char *buf = xmalloc(size+1);
	char *timeout = getConfValue("TIMEOUT");

	while (true) {
		va_start(vp, line);
		bytes = vsnprintf(buf, size, line, vp);
		va_end(vp);
		if (bytes > -1 && bytes < size) {
			/* String written properly */
			break;
		}
		
		if (bytes > -1) {
			size += 1;
		} else { 
			size *= 2;
		}
		buf = xrealloc(buf, size+1);
	}


	FD_ZERO(&wfds);
	FD_SET(dnetGetSock(sd), &wfds);
	if (timeout) {
		tv.tv_sec = atoi(timeout);
	} else {
		tv.tv_sec = 10;
	}
	tv.tv_usec = 0;
	sval = select(dnetGetSock(sd)+1, NULL, &wfds, NULL, &tv);
	if (sval == -1) {
		smtpSetErr("writeResponse: select error");
		bytes = ERROR;
	} else if (sval) {
		dnetWrite(sd, buf, bytes);
		if (dnetErr(sd)) {
			smtpSetErr(dnetGetErr(sd));
			bytes = ERROR;
		}
	} else {
		smtpSetErr("Timeout(10) trying to write to SMTP server.");
		bytes = ERROR;
	}
	xfree(buf);
	return bytes;
}

static int
helo(dsocket *sd, const char *domain)
{
	int retval;
	dstrbuf *rbuf = DSB_NEW;

	/*
	 * We will be calling this function after ehlo() has already
	 * been called.  Since ehlo() already grabs the header, go
	 * straight into sending the HELO
	 */
	if (writeResponse(sd, "HELO %s\r\n", domain) < 0) {
		smtpSetErr("Lost connection to SMTP server");
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("--> HELO\n");
	fflush(stdout);
#endif

	retval = readResponse(sd, rbuf);
	if (retval != 250) {
		if (retval != ERROR) {
			smtpSetErr(rbuf->str);
		}
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("<-- %s\n", rbuf->str);
	fflush(stdout);
#endif

end:
	dsbDestroy(rbuf);
	return retval;
}


static int
ehlo(dsocket *sd, const char *domain)
{
	int retval;
	dstrbuf *rbuf = DSB_NEW;

	/* This initiates the connection, so let's read the header first */
	retval = readResponse(sd, rbuf);
	if (retval != 220) {
		if (retval != ERROR) {
			smtpSetErr(rbuf->str);
		}
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("\r\n<-- %s", rbuf->str);
	fflush(stdout);
#endif

	if (writeResponse(sd, "EHLO %s\r\n", domain) < 0) {
		smtpSetErr("Lost connection to SMTP server");
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("\r\n--> EHLO %s\r\n", domain);
	fflush(stdout);
#endif

	retval = readResponse(sd, rbuf);
	if (retval != 250) {
		if (retval != ERROR) {
			smtpSetErr(rbuf->str);
		}
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("\r\n<-- %s", rbuf->str);
	fflush(stdout);
#endif

end:
	dsbDestroy(rbuf);
	return retval;
}

/** 
 * Send the MAIL FROM: command to the smtp server 
 */
static int
mailFrom(dsocket *sd, const char *email)
{
	int retval = 0;
	dstrbuf *rbuf = DSB_NEW;

	/* Create the MAIL FROM: command */
	if (writeResponse(sd, "MAIL FROM:<%s>\r\n", email) < 0) {
		smtpSetErr("Lost connection with SMTP server");
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("\r\n--> MAIL FROM:<%s>\r\n", email);
#endif

	/* read return message and let's return it's code */
	retval = readResponse(sd, rbuf);
	if (retval != 250) {
		if (retval != ERROR) {
			smtpSetErr(rbuf->str);
		}
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("\r\n<-- %s", rbuf->str);
#endif

end:
	dsbDestroy(rbuf);
	return retval;
}

/**
 * Send the RCPT TO: command to the smtp server
 */
static int
rcpt(dsocket *sd, const char *email)
{
	int retval = 0;
	dstrbuf *rbuf = DSB_NEW;

	if (writeResponse(sd, "RCPT TO: <%s>\r\n", email) < 0) {
		smtpSetErr("Lost connection with SMTP server");
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("\r\n--> RCPT TO: <%s>\r\n", email);
	fflush(stdout);
#endif

	/* Read return message and let's return it's code */
	retval = readResponse(sd, rbuf);
	if ((retval != 250) && (retval != 251)) {
		if (retval != ERROR) {
			smtpSetErr(rbuf->str);
		}
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("\r\n<-- %s", rbuf->str);
	fflush(stdout);
#endif

end:
	dsbDestroy(rbuf);
	return retval;
}


/**
 * Send the QUIT command
 */
static int
quit(dsocket *sd)
{
	int retval = 0;
	dstrbuf *rbuf = DSB_NEW;

	/* Create QUIT command and send it */
	if (writeResponse(sd, "QUIT\r\n") < 0) {
		smtpSetErr("Lost Connection with SMTP server: Quit()");
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("--> QUIT\r\n");
#endif

	retval = readResponse(sd, rbuf);
	if (retval != 221) {
		if (retval != ERROR) {
			smtpSetErr(rbuf->str);
		}
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("<-- %s", rbuf->str);
#endif

end:
	dsbDestroy(rbuf);
	return retval;
}

int
data(dsocket *sd)
{
	int retval = 0;
	dstrbuf *rbuf = DSB_NEW;

	/* Create the DATA command and send it */
	if (writeResponse(sd, "DATA\r\n") < 0) {
		smtpSetErr("Lost connection with SMTP server");
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("\r\n--> DATA\r\n");
#endif

	/* Read return message and let's return it's code */
	retval = readResponse(sd, rbuf);
	if (retval != 354) {
		if (retval != ERROR) {
			smtpSetErr(rbuf->str);
		}
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("<-- %s", rbuf->str);
#endif

end:
	dsbDestroy(rbuf);
	return retval;
}

/**
 * Send the RSET command. 
 */
static int
rset(dsocket *sd)
{
	int retval = 0;
	dstrbuf *rbuf = DSB_NEW;

	/* Send the RSET command */
	if (writeResponse(sd, "RSET\r\n") < 0) {
		smtpSetErr("Socket write error: rset");
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("--> RSET\n");
	fflush(stdout);
#endif

	retval = readResponse(sd, rbuf);
	if (retval != 250) {
		if (retval != ERROR) {
			smtpSetErr(rbuf->str);
		}
		retval = ERROR;
		goto end;
	}


#ifdef DEBUG_SMTP
	printf("<-- %s\n", rbuf->str);
	fflush(stdout);
#endif

end:
	dsbDestroy(rbuf);
	return retval;
}


/** 
 * SMTP AUTH login.
 */
static int
smtpAuthLogin(dsocket *sd, const char *user, const char *pass)
{
	int retval = 0;
	dstrbuf *data;
	dstrbuf *rbuf = DSB_NEW;

	data = mimeB64EncodeString((u_char *)user, strlen(user), false);
	if (writeResponse(sd, "AUTH LOGIN %s\r\n", data->str) < 0) {
		smtpSetErr("Socket write error: smtp_auth_login");
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("--> AUTH LOGIN\n");
	fflush(stdout);
#endif

	retval = readResponse(sd, rbuf);
	if (retval != 334) {
		if (retval != ERROR) {
			smtpSetErr(rbuf->str);
		}
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("<-- %s\n", rbuf->str);
	fflush(stdout);
#endif

	/* Encode the password */
	dsbDestroy(data);
	data = mimeB64EncodeString((u_char *)pass, strlen(pass), false);
	if (writeResponse(sd, "%s\r\n", data->str) < 0) {
		smtpSetErr("Socket write error: smtp_auth_login");
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("--> %s\n", data->str);
	fflush(stdout);
#endif
	dsbDestroy(data);

	/* Read back "OK" from server */
	retval = readResponse(sd, rbuf);
	if (retval != 235) {
		if (retval != ERROR) {
			smtpSetErr(rbuf->str);
		}
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("<-- %s\n", rbuf->str);
	fflush(stdout);
#endif

end:
	dsbDestroy(rbuf);
	return retval;
}

static int
smtpAuthPlain(dsocket *sd, const char *user, const char *pass)
{
	int retval = 0;
	dstrbuf *data=NULL;
	dstrbuf *up = DSB_NEW;
	dstrbuf *rbuf = DSB_NEW;

	if (writeResponse(sd, "AUTH PLAIN\r\n") < 0) {
		smtpSetErr("Socket write error: smtp_auth_plain");
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("--> AUTH PLAIN\n");
	fflush(stdout);
#endif

	retval = readResponse(sd, rbuf);
	if (retval != 334) {
		if (retval != ERROR) {
			smtpSetErr(rbuf->str);
		}
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("<-- %s\n", rbuf->str);
	fflush(stdout);
#endif

	dsbPrintf(up, "%c%s%c%s", '\0', user, '\0', pass);
	data = mimeB64EncodeString((u_char *)up->str, up->len, false);
	if (writeResponse(sd, "%s\r\n", data->str) < 0) {
		smtpSetErr("Socket write error: smtp_auth_plain");
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("--> %s\n", data->str);
	fflush(stdout);
#endif

	dsbClear(rbuf);
	retval = readResponse(sd, rbuf);
	if (retval != 235) {
		if (retval != ERROR) {
			smtpSetErr(rbuf->str);
		}
		retval = ERROR;
		goto end;
	}

#ifdef DEBUG_SMTP
	printf("<-- %s\n", rbuf->str);
	fflush(stdout);
#endif

end:
	dsbDestroy(up);
	dsbDestroy(data);
	dsbDestroy(rbuf);
	return retval;
}

/**
 * Initializes the SMTP communications with SMTP_AUTH.
 *
 * Params
 * 	sd - Socket descriptor
 * 	auth - LOGIN or PLAIN 
 * 	user - Username of the SMTP server
 * 	pass - Password for the Username
 *
 * Return
 * 	- ERROR
 * 	- SUCCESS
 */
int
smtpInitAuth(dsocket *sd, const char *auth, const char *user, const char *pass)
{
	int retval=ERROR;
	if (strcasecmp(auth, "LOGIN") == 0) {
		retval = smtpAuthLogin(sd, user, pass);
	} else if (strcasecmp(auth, "PLAIN") == 0) {
		retval = smtpAuthPlain(sd, user, pass);
	} 

	return retval;
}


/**
 * Initializes the SMTP communications by sending the EHLO
 * If EHLO errors out, it will try the HELO command.
 *
 * Params
 * 	sd - Socket descriptor
 * 	domain - Your domain name.
 *
 * Return
 * 	- ERROR
 * 	- SUCCESS
 */
int
smtpInit(dsocket *sd, const char *domain)
{
	int retval;

	printProgress("Greeting the SMTP server...");
	retval = ehlo(sd, domain);
	if (retval == ERROR) {
		/*
		 * Per RFC, if ehlo error's out, you can
		 * ignore the error, RSET and try a 
		 * regular helo.
		 */
		rset(sd);
		retval = helo(sd, domain);
	}

	return retval;
}

int
smtpStartTls(dsocket *sd)
{
        int retval=SUCCESS;
#ifdef HAVE_LIBSSL
        dstrbuf *sb=DSB_NEW;

	printProgress("Starting TLS Communications...");
        if (writeResponse(sd, "STARTTLS\r\n") < 0) {
                smtpSetErr("Lost connection to SMTP Server");
                retval = ERROR;
                goto end;
        }

#ifdef DEBUG_SMTP
	printf("--> STARTTLS\n");
	fflush(stdout);
#endif

        retval = readResponse(sd, sb);
        if (retval != 220) {
                smtpSetErr(sb->str);
                retval = ERROR;
        }

#ifdef DEBUG_SMTP
	printf("<-- %s\n", sb->str);
	fflush(stdout);
#endif

end:
        dsbDestroy(sb);
#else
	sd = sd;
#endif
        return retval;
}

/**
 * Sets who the message is from.  Basically runs the 
 * MAIL FROM: SMTP command
 *
 * Params
 * 	sd - Socket descriptor
 * 	email - From Email
 *
 * Return
 * 	- ERROR
 * 	- SUCCESS
 */
int
smtpSetMailFrom(dsocket *sd, const char *email)
{
	return mailFrom(sd, email);
}

/**
 * Sets all the recipients with the RCPT command
 * to the smtp server. Multiple calls to this function
 * are expected if you have multiple To, CC and BCC 
 * recipients.
 *
 * Params
 * 	sd - Socket descriptor
 * 	to - An e-mail address to send the message to
 *
 * Return
 * 	- ERROR
 * 	- SUCCESS
 */
int
smtpSetRcpt(dsocket *sd, const char *to)
{
	return rcpt(sd, to);
}

/** 
 * Send the DATA command to the smtp server (no data, just the command)
 *
 * Params
 * 	sd - Socket descriptor
 *
 * Return
 * 	- ERROR
 * 	- SUCCESS
 */
int
smtpStartData(dsocket *sd)
{
	return data(sd);
}

/**
 * Sends data to the smtp server. You can try and send the
 * whole chunk at once, or it may be a better idea to break
 * up the data into smaller chunks.
 *
 * Params
 * 	sd - Socket descriptor
 * 	data - A chunk of data.
 * 	len - The length of the data to send
 *
 * Return
 * 	- ERROR
 * 	- SUCCESS
 */
int
smtpSendData(dsocket *sd, const char *data, size_t len)
{
	int retval = SUCCESS;

	assert(data != NULL);
	assert(sd != NULL);

	/* Write the data to the socket. */
	retval = dnetWrite(sd, data, len);
	if (dnetErr(sd)) {
		smtpSetErr("Error writing to socket.");
		retval = ERROR;
	}
	if(retval <=0) {	//Sam, 2014/11/24
		smtpSetErr("Write data unsuccessfully.");
		retval = ERROR;
	}
	else {
		retval = SUCCESS;
	}
	return retval;
}

/**
 * Let's the SMTP server know it's the end of the data stream.
 *
 * Params
 * 	sd - Socket descriptor
 *
 * Return
 * 	- ERROR
 * 	- SUCCESS
 */
int 
smtpEndData(dsocket *sd)
{
	int retval=ERROR;
	dstrbuf *rbuf = DSB_NEW;

	printProgress("Ending Data...");
	if (writeResponse(sd, "\r\n.\r\n") != ERROR) {
		retval = readResponse(sd, rbuf);
		if (retval != 250) {
			if (retval != ERROR) {
				smtpSetErr(rbuf->str);
				retval = ERROR;
			}
		}
	} else {
		smtpSetErr("Lost Connection with SMTP server: smtpEndData()");
		retval = ERROR;
	}

	dsbDestroy(rbuf);
	return retval;
}

/**
 * Sends the QUIT\r\n signal to the smtp server.
 *
 * Params
 * 	sd - Socket Descriptor
 *
 * Return
 * 	- ERROR
 * 	- SUCCESS
 */
int
smtpQuit(dsocket *sd)
{
	int retval=0;
	printProgress("Sending QUIT...");
	retval = quit(sd);
	dsbDestroy(errorstr);
	return retval;
}



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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Autoconf manual suggests this. */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "email.h"
#include "execgpg.h"
#include "utils.h"
#include "file_io.h"
#include "addy_book.h"
#include "remotesmtp.h"
#include "addr_parse.h"
#include "message.h"
#include "mimeutils.h"
#include "error.h"
#ifdef RTCONFIG_NOTIFICATION_CENTER
#include <libnt.h>
#endif

/**
 * All functions below are pretty self explanitory.  
 * They basically just preform some simple header printing 
 * for our email 'message'.  I'm not doing to comment every 
 * function from here because you should be able to read the 
 * damn functions and get a great idea
**/
static void
printMimeHeaders(const char *b, dstrbuf *msg, CharSetType charset)
{
	dsbPrintf(msg, "Mime-Version: 1.0\r\n");
	if (Mopts.gpg_opts & GPG_ENC) {
		dsbPrintf(msg, "Content-Type: multipart/encrypted; "
			"protocol=\"application/pgp-encrypted\"; " 
			"boundary=\"%s\"\r\n", b);
	} else if (Mopts.gpg_opts & GPG_SIG) {
		dsbPrintf(msg, "Content-Type: multipart/signed; "
			"micalg=pgp-sha1; protocol=\"application/pgp-signature\"; "
			"boundary=\"%s\"\r\n", b);
	} else if (Mopts.attach) {
		dsbPrintf(msg, "Content-Type: multipart/mixed; boundary=\"%s\"\r\n", b);
	} else {
		if (charset == IS_UTF8 || charset == IS_PARTIAL_UTF8) {
			dsbPrintf(msg, "Content-Type: text/plain; charset=utf-8\r\n");
			if (charset == IS_PARTIAL_UTF8) {
				dsbPrintf(msg, "Content-Transfer-Encoding: quoted-printable\r\n");
			} else {
				dsbPrintf(msg, "Content-Transfer-Encoding: base64\r\n");
			}
			dsbPrintf(msg, "Content-Disposition: inline\r\n");
		} else if (Mopts.html) {
			dsbPrintf(msg, "Content-Type: text/html\r\n");
		} else {
			dsbPrintf(msg, "Content-Type: text/plain\r\n");
		}
	}
}

/**
 * just prints the to headers, and cc headers if available
**/
static void
printToHeaders(dlist to, dlist cc, dstrbuf *msg)
{
	struct addr *a = (struct addr *)dlGetNext(to);
	dsbPrintf(msg, "To: ");
	while (a) {
		dstrbuf *tmp = formatEmailAddr(a->name, a->email);
		dsbPrintf(msg, "%s", tmp->str);
		dsbDestroy(tmp);
		a = (struct addr *)dlGetNext(to);
		if (a != NULL) {
			dsbPrintf(msg, ", ");
		} else {
			dsbPrintf(msg, "\r\n");
		}
	}

	if (cc != NULL) {
		dsbPrintf(msg, "Cc: ");
		a = (struct addr *)dlGetNext(cc);
		while (a) {
			dstrbuf *tmp = formatEmailAddr(a->name, a->email);
			dsbPrintf(msg, "%s", tmp->str);
			dsbDestroy(tmp);
			a = (struct addr *)dlGetNext(cc);
			if (a != NULL) {
				dsbPrintf(msg, ", ");
			} else {
				dsbPrintf(msg, "\r\n");
			}
		}
	}
}

/**
 * Bcc gets a special function because it's not always printed for standard
 * Mail delivery.  Only if sendmail is going to be invoked, shall it be printed
 * reason being is because sendmail needs the Bcc header to know everyone who
 * is going to recieve the message, when it is done with reading the Bcc headers
 * it will remove this from the headers field.
**/
static void
printBccHeaders(dlist bcc, dstrbuf *msg)
{
	struct addr *a=NULL;

	if (bcc != NULL) {
		dsbPrintf(msg, "Bcc: ");
		a = (struct addr *)dlGetNext(bcc);
		while (a != NULL) {
			dstrbuf *tmp = formatEmailAddr(a->name, a->email);
			dsbPrintf(msg, "%s", tmp->str);
			dsbDestroy(tmp);
			a = (struct addr *)dlGetNext(bcc);
			if (a) {
				dsbPrintf(msg, ", ");
			} else {
				dsbPrintf(msg, "\r\n");
			}
		}
        }
}

/** Print From Headers **/
static void
printFromHeaders(char *name, char *address, dstrbuf *msg)
{
	dstrbuf *addr = formatEmailAddr(name, address);
	dsbPrintf(msg, "From: %s\r\n", addr->str);
	dsbDestroy(addr);
}

/** Print Date Headers **/
static void
printDateHeaders(dstrbuf *msg)
{
	time_t set_time;
	struct tm *lt;
	char buf[MAXBUF] = { 0 };

	set_time = time(&set_time);

#ifdef USE_GMT
	lt = gmtime(&set_time);
#else
	lt = localtime(&set_time);
#endif

#ifdef USE_GNU_STRFTIME
	strftime(buf, MAXBUF, "%a, %d %b %Y %H:%M:%S %z", lt);
#else
	strftime(buf, MAXBUF, "%a, %d %b %Y %H:%M:%S %Z", lt);
#endif

	dsbPrintf(msg, "Date: %s\r\n", buf);
}

static void
printExtraHeaders(dlist headers, dstrbuf *msg)
{
	char *hdr=NULL;
	while ((hdr = (char *)dlGetNext(headers)) != NULL) {
		dsbPrintf(msg, "%s\r\n", hdr);
	}
}

/**
 * This function takes the current content that was copied
 * in to us and creates a final message with the email header
 * and the appended content.  It will also attach any files
 * that were specified at the command line.
**/
static void
printHeaders(const char *border, dstrbuf *msg, CharSetType msg_cs)
{
	char *subject=Mopts.subject;
	char *user_name = getConfValue("MY_NAME");
	char *email_addr = getConfValue("MY_EMAIL");
	char *sm_bin = getConfValue("SENDMAIL_BIN");
	char *smtp_serv = getConfValue("SMTP_SERVER");
	char *reply_to = getConfValue("REPLY_TO");
	dstrbuf *dsb=NULL;

	if (subject) {
		if (Mopts.encoding) {
			CharSetType cs = getCharSet((u_char *)subject);
			if (cs == IS_UTF8) {
				dsb = encodeUtf8String((u_char *)subject, false);
				subject = dsb->str;
			} else if (cs == IS_PARTIAL_UTF8) {
				dsb = encodeUtf8String((u_char *)subject, true);
				subject = dsb->str;
			}
		}
		dsbPrintf(msg, "Subject: %s\r\n", subject);
		if (dsb) {
			dsbDestroy(dsb);
		}
	}
	printFromHeaders(user_name, email_addr, msg);
	printToHeaders(Mopts.to, Mopts.cc, msg);

	/**
	 * We want to check here to see if we are sending mail by invoking sendmail
	 * If so, We want to add the BCC line to the headers.  Sendmail checks this
	 * Line and makes sure it sends the mail to the BCC people, and then remove
	 * the BCC addresses...  Keep in mind that sending to an smtp servers takes
	 * presidence over sending to sendmail incase both are mentioned.
	 */
	if (sm_bin && !smtp_serv) {
		printBccHeaders(Mopts.bcc, msg);
	}

	/* The rest of the standard headers */
	printDateHeaders(msg);
	if (reply_to) {
		dsbPrintf(msg, "Reply-To: <%s>\r\n", reply_to);
	}
	printMimeHeaders(border, msg, msg_cs);
	dsbPrintf(msg, "X-Mailer: Cleancode.email v%s \r\n", EMAIL_VERSION);
	if (Mopts.priority) {
		dsbPrintf(msg, "X-Priority: 1\r\n");
	}
	printExtraHeaders(Mopts.headers, msg);
	dsbPrintf(msg, "\r\n");
}


/**
 * set up the appropriate MIME and Base64 headers for 
 * the attachment of file specified in Mopts.attach
**/
static int
attachFiles(const char *boundary, dstrbuf *out)
{
	dstrbuf *file_name = NULL;
	dstrbuf *file_type = NULL;
	char *next_file = NULL;

	/*
	* What we will do here is parse Mopts.attach for comma delimited file
	* names.  If there was only one file specified with no comma, then strtok()
	* will just return that file and the next call to strtok() will be NULL
	* which will allow use to break out of our loop of attaching base64 stuff.
	*/
	while ((next_file = (char *)dlGetNext(Mopts.attach)) != NULL) {
		FILE *current = fopen(next_file, "r");
		if (!current) {
			fatal("Could not open attachment: %s", next_file);
			//return (ERROR);
			continue; //skip nonexistent file, Sam 2014/04/30
		}

		/* If the user specified an absolute path, just get the file name */
		file_type = mimeFiletype(next_file);
		file_name = mimeFilename(next_file);

		/* Set our MIME headers */
		dsbPrintf(out, "\r\n--%s\r\n", boundary);
		dsbPrintf(out, "Content-Transfer-Encoding: base64\r\n");
		dsbPrintf(out, "Content-Type: %s; name=\"%s\"\r\n", 
			file_type->str, file_name->str);
		dsbPrintf(out, "Content-Disposition: attachment; filename=\"%s\"\r\n", 
			file_name->str);
		dsbPrintf(out, "\r\n");

		/* Encode to 'out' */
		mimeB64EncodeFile(current, out);
		dsbDestroy(file_type);
		dsbDestroy(file_name);
	}
	return SUCCESS;
}

/** 
 * Makes a standard plain text message while taking into
 * account the MIME message types and boundary's needed
 * if and when a file is attached.
**/
static int
makeMessage(dstrbuf *in, dstrbuf *out, const char *border, CharSetType charset)
{
	dstrbuf *enc=NULL;
	if (Mopts.attach) {
		dsbPrintf(out, "--%s\r\n", border);
		if (charset == IS_UTF8 || charset == IS_PARTIAL_UTF8) {
			dsbPrintf(out, "Content-Type: text/plain; charset=utf-8\r\n");
			if (IS_PARTIAL_UTF8) {
				dsbPrintf(out, "Content-Transfer-Encoding: quoted-printable\r\n");
				enc = mimeQpEncodeString((u_char *)in->str, true);
			} else {
				dsbPrintf(out, "Content-Transfer-Encoding: base64\r\n");
				enc = mimeB64EncodeString((u_char *)in->str, in->len, true);
			}
			dsbPrintf(out, "Content-Disposition: inline\r\n\r\n");
		} else if (Mopts.html) {
			dsbPrintf(out, "Content-Type: text/html\r\n\r\n");
			enc = DSB_NEW;
			dsbCat(enc, in->str);
		} else {
			dsbPrintf(out, "Content-Type: text/plain\r\n\r\n");
			enc = DSB_NEW;
			dsbCat(enc, in->str);
		}
	} else {
		if (charset == IS_UTF8) {
			enc = mimeB64EncodeString((u_char *)in->str, in->len, true);
		} else if (charset == IS_PARTIAL_UTF8) {
			enc = mimeQpEncodeString((u_char *)in->str, true);
		} else {
			enc = DSB_NEW;
			dsbCat(enc, in->str);
		}
	}
	dsbPrintf(out, "%s\r\n", enc->str);
	if (Mopts.attach) {
		if (attachFiles(border, out) == ERROR) {
			return ERROR;
		}
		dsbPrintf(out, "\r\n\r\n--%s--\r\n", border);
	}
	dsbDestroy(enc);
	return 0;
}

/**
 * Makes a message type specifically for gpg encryption and 
 * signing.  Specific MIME message descriptions are needed
 * when signing/encrypting a file before it is actuall signed
 * or encrypted.  This function takes care of that.
**/
static int
makeGpgMessage(dstrbuf *in, dstrbuf *out, const char *border)
{
	dstrbuf *qp=NULL;

	assert(in != NULL);
	assert(out != NULL);
	assert(border != NULL);

	if (Mopts.attach) {
		dsbPrintf(out, "Content-Type: multipart/mixed; "
			"boundary=\"%s\"\r\n\r\n", border);
		dsbPrintf(out, "\r\n--%s\r\n", border);
	}

	if (Mopts.html) {
		dsbPrintf(out, "Content-Type: text/html\r\n");
	} else {
		dsbPrintf(out, "Content-Type: text/plain\r\n");
	}

	dsbPrintf(out, "Content-Transfer-Encoding: quoted-printable\r\n\r\n");
	qp = mimeQpEncodeString((u_char *)in->str, true);
	dsbnCat(out, qp->str, qp->len);
	dsbDestroy(qp);
	if (Mopts.attach) {
		attachFiles(border, out);
		dsbPrintf(out, "\r\n--%s--\r\n", border);
	}
	return 0;
}

/**
 * Creates a signed message with gpg and takes into 
 * account the correct MIME message types to add to 
 * the message.
**/
static dstrbuf *
createGpgEmail(dstrbuf *msg, GpgCallType gpg_type)
{
	dstrbuf *tmpbuf=DSB_NEW;
	dstrbuf *gpgdata=NULL, *buf=DSB_NEW;
	dstrbuf *border1=NULL, *border2=NULL;

	assert(msg != NULL);

	/* Create two borders if we're attaching files */
	border1 = mimeMakeBoundary();
	if (Mopts.attach) {
		border2 = mimeMakeBoundary();
	} else {
		border2 = DSB_NEW;
	}

	if (makeGpgMessage(msg, tmpbuf, border2->str) < 0) {
		dsbDestroy(buf);
		buf=NULL;
		goto end;
	}

	gpgdata = callGpg(tmpbuf, gpg_type);
	if (!gpgdata) {
		dsbDestroy(buf);
		buf=NULL;
		goto end;
	}
	printHeaders(border1->str, buf, IS_ASCII);

	dsbPrintf(buf, "\r\n--%s\r\n", border1->str);
	if (gpg_type & GPG_ENC) {
		dsbPrintf(buf, "Content-Type: application/pgp-encrypted\r\n\r\n");
		dsbPrintf(buf, "Version: 1\r\n");
		dsbPrintf(buf, "\r\n--%s\r\n", border1->str);
		dsbPrintf(buf, "Content-type: application/octet-stream; "
			       "name=encrypted.asc\r\n\r\n");
	} else if (gpg_type & GPG_SIG) {
		dsbPrintf(buf, "%s\r\n", tmpbuf->str);
		dsbPrintf(buf, "\r\n--%s\r\n", border1->str);
		dsbPrintf(buf, "Content-Type: application/pgp-signature\r\n");
		dsbPrintf(buf, "Content-Description: This is a digitally signed message\r\n\r\n");
	} 
	dsbPrintf(buf, "%s", gpgdata->str);
	dsbPrintf(buf, "\r\n--%s--\r\n", border1->str);

end:
	dsbDestroy(tmpbuf);
	dsbDestroy(gpgdata);
	dsbDestroy(border1);
	dsbDestroy(border2);
	return buf;
}

/**
 * Creates a plain text (or html) email and 
 * specifies the necessary MIME types if needed
 * due to attaching base64 files.
 * when this function is done, it will rewind
 * the file position and return an open file
**/
static dstrbuf *
createPlainEmail(dstrbuf *msg) 
{
	dstrbuf *border=NULL;
	dstrbuf *buf=DSB_NEW;
	CharSetType cs;

	if (Mopts.attach) {
		border = mimeMakeBoundary();
	} else {
		border = DSB_NEW;
	}

	if (Mopts.encoding) {
		cs = getCharSet((u_char *)msg->str);
	} else {
		cs = IS_ASCII;
	}
	printHeaders(border->str, buf, cs);
	if (makeMessage(msg, buf, border->str, cs) < 0) {
		dsbDestroy(buf);
		buf=NULL;
	}
	dsbDestroy(border);
	return buf;
}

/**
 * this is the function that takes over from main().  
 * It will call all functions nessicary to finish off the 
 * rest of the program and then return properly. 
**/
void
createMail(void)
{
	dstrbuf *msg=NULL;
	char subject[MAXBUF]={0};

	/**
	 * first let's check if someone has tried to send stuff in from STDIN 
	 * if they have, let's call a read to stdin
	 */
	if (isatty(STDIN_FILENO) == 0) {
		msg = readInput();
		if (!msg) {
			fatal("Problem reading from STDIN redirect\n");
			properExit(ERROR);
		}
	} else {
		/* If they aren't sending a blank email */
		if (!Mopts.blank) {
			/* let's check if they want to add a subject or not */
			if (Mopts.subject == NULL) {
				fprintf(stderr, "Subject: ");
				fgets(subject, sizeof(subject)-1, stdin);
				chomp(subject);
				Mopts.subject = subject;
			}

			/* Now we need to let them create a file */
			msg = editEmail();
			if (!msg) {
				properExit(ERROR);
			}
		} else {
			/* Create a blank message */
			msg = DSB_NEW;
		}
	}

	/* Create a message according to the type */
	if (Mopts.gpg_opts) {
		global_msg = createGpgEmail(msg, Mopts.gpg_opts);
	} else {
		global_msg = createPlainEmail(msg);
	}

	if (!global_msg) {
		dsbDestroy(msg);
		properExit(ERROR);
	}

	dsbDestroy(msg);
	if(sendmail(global_msg) == ERROR) {
		properExit(ERROR);
	}
#ifdef RTCONFIG_NOTIFICATION_CENTER
	else {
		extern int report_f;
		extern int sendId;
		if(report_f) {
			NOTIFY_EVENT_T *e = initial_nt_event();
			e->event = RESERVATION_MAIL_REPORT_EVENT;
			e->mail_t.MsendId = sendId;
			e->mail_t.MsendStatus = MAIL_SUCCESS;
			send_notify_event(e, NOTIFY_MAIL_SERVICE_SOCKET_PATH);
			nt_event_free(e);
		}
	}
#endif
}


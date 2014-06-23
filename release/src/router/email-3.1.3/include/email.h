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
#ifndef __EMAIL_H
#define __EMAIL_H   1

/* Everyone needs this */
#include <assert.h>

#include <dlib.h>
#include <dstrbuf.h>
#include <dlist.h>
#include <dhash.h>
#include <dutil.h>

#define MAXBUF  600
#define ERROR   -6
#define TRUE    1
#define FALSE   0
#define EASY    100

#define TMPFILE_TEMPLATE "/tmp/.email.msg.XXXXXX"
#define TMPFILE_TEMPLATE_SIZE 23

#define EMAIL_LOG_FILE	"/tmp/email.log"

/* EMAIL_DIR determined at compile time */
#define MAIN_CONFIG  EMAIL_DIR"/email.conf"
#define EMAIL_HELP_FILE   EMAIL_DIR"/email.help"

#define DEBUG_SMTP 1

/* Debugger */
#define DEBUG(str)                             \
(                                              \
  fprintf(stderr, "DEBUG: %s:%s():%d: %s\n",   \
      __FILE__, __FUNCTION__, __LINE__, (str)) \
)

struct addr {
	char *name;
	char *email;
};

typedef enum { GPG_SIG=0x01, GPG_ENC=0x02 } GpgCallType;


/* Globally defined vars */
dhash table;
char *conf_file;
dstrbuf *global_msg;

struct mailer_options {
	bool verbose;
	bool encoding;
	short html;
	short priority;
	short receipt;
	short blank;
	GpgCallType gpg_opts;
	char *subject;
	dlist attach;
	dlist headers;
	dlist to;
	dlist cc;
	dlist bcc;
} Mopts;

void usage(void);

char *getConfValue(const char *tok);
void setConfValue(const char *tok, const char *val);


#endif /* __EMAIL_H */

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

#include <pwd.h>

#include <sys/utsname.h>
#include <sys/types.h>

#include "email.h"
#include "conf.h"
#include "utils.h"
#include "error.h"

#define MAX_CONF_VARS 20

/* There are the variables accepted in the configuration file */
static char conf_vars[MAX_CONF_VARS][MAXBUF] = {
	"SMTP_SERVER",
	"SMTP_PORT",
	"SENDMAIL_BIN",
	"MY_NAME",
	"MY_EMAIL",
	"REPLY_TO",
	"SIGNATURE_FILE",
	"ADDRESS_BOOK",
	"SAVE_SENT_MAIL",
	"TEMP_DIR",
	"GPG_BIN",
	"GPG_PASS",
	"SMTP_AUTH",
	"USE_TLS",
	"SMTP_AUTH_USER",
	"SMTP_AUTH_PASS",
	"VCARD"
};

/**
 * Make sure that var is part of a possible 
 * configuration variable in the configure file
**/
static int
checkVar(dstrbuf *var)
{
	int i;

	for (i = 0; i < MAX_CONF_VARS; i++) {
		if (strcasecmp(var->str, conf_vars[i]) == 0) {
			return 0;
		}
	}
	/*
	for (i=0; i < MAX_DEP_CONF_VARS; i++) {
		if (strcasecmp(var->str, dep_conf_vars[i]) == 0) {
			warning("Deprecated variable: %s\n", var->str);
			return 0;
		}
	}
	*/
	return -1;
}

/**
 * Check the hash values to make sure they aren't 
 * NULL.  IF they aren't, then Hash them, other
 * wise, ERROR 
**/
static int
hashit(const char *var, const char *val)
{
	if ((*var == '\0') && (*val == '\0')) {
		/* Nothing to hash */
		return -1;
	} else if ((*var == '\0') || (*val == '\0')) {
		/* Something went wrong */
		return ERROR;
	}

	/* Don't store if there is something already there. */
	if (!getConfValue(var)) {
		setConfValue(var, xstrdup(val));
	}
	return 0;
}


/**
 * Read the configuration file char by char and make
 * sure each char is taken care of properly.
 * Hash each token we get.  Newlines are considered the
 * end of the expression if a \ is not found.
**/
int
readConfig(FILE *in)
{
	int ch, line=1;
	int retval=0;
	dstrbuf *var, *val;
	dstrbuf *ptr;   /* start with var first */

	var = DSB_NEW;
	val = DSB_NEW;
	ptr = var;

	while ((ch = fgetc(in)) != EOF) {
		switch (ch) {
		case '#':
			while ((ch = fgetc(in)) != '\n')
				; /* Deal with newline below */
			break;
		case '\\':
			ch = fgetc(in);
			if (ch == '\r') {
				ch = fgetc(in);
			}
			if (ch != '\n') {
				dsbCatChar(ptr, ch);
			}
			line++;
			/* If this char is a newline, 
			   we don't want to handle it below */
			ch = 0; 
			break;
		case '\'':
			if (copyUpTo(ptr, ch, in) == '\n') {
				/* If newline was found before end quote */
				ch = line;
				goto exit;
			}
			break;
		case '"':
			if (copyUpTo(ptr, ch, in) == '\n') {
				/* Newline was found before end quote. ERROR */
				ch = line;
				goto exit;
			}
			break;
		case '=':
			if (val->len != 0) {
				ch = line;
				goto exit;
			} else if (checkVar(var) < 0) {
				fatal("Variable: '%s' is not valid\n", var);
				ch = line;
				goto exit;
			}
			ptr = val;
			break;
		case ' ':
			/* Nothing for spaces */
			break;
		case '\t':
			/* Nothing for tabs */
			break;
		case '\r':
			/* Ignore */
			break;
		case '\n':
			/* Handle Newlines below */
			break;
		default:
			dsbCatChar(ptr, ch);
			break;
		}

		/* See about the newline, hash vals if possible */
		if (ch == '\n') {
			line++;
			retval = hashit(var->str, val->str);
			if (retval == -1) {
				/* No error, just keep going */
				continue;
			} else if (retval == ERROR) {
				ch = line;
				goto exit;
			}

			/* Everything went fine. */
			dsbDestroy(var);
			dsbDestroy(val);
			var = DSB_NEW;
			val = DSB_NEW;
			ptr = var;
		}
	}

exit:
	dsbDestroy(var);
	dsbDestroy(val);
	return (ch);
}

/**
 * Get the user name of the person running me
 * Return Value: 
 *    malloced string 
**/
static char *
getSystemName(void)
{
	int uid = getuid();
	char *name = NULL;
	struct passwd *ent;

	/* Try to get the "Real Name" of the user. Else, the user name */
	ent = getpwuid(uid);
	if (!ent) {
		name = xstrdup("Unknown User");
	} else if (ent->pw_gecos) {
		name = xstrdup(ent->pw_gecos);
	} else {
		name = xstrdup(ent->pw_name);
	}
	return name;
}

/**
 * Get the email address of the person running me.
 * this is done by getting the "user name" of the current
 * person, and the host name running me.
 * Return value:
 *    malloced string
**/
static char *
getSystemEmail(void)
{
	int uid = getuid();
	char *email, *name, *host;
	struct utsname hinfo;
	struct passwd *ent;
	dstrbuf *buf = DSB_NEW;

	ent = getpwuid(uid);
	if (!ent) {
		name = "unknown";
	} else {
		name = ent->pw_name;
	}

	if (uname(&hinfo) < 0) {
		host = "localhost";
	} else {
		host = hinfo.nodename;
	}

	/* format it all */
	dsbPrintf(buf, "%s@%s", name, host);
	email = xstrdup(buf->str);
	dsbDestroy(buf);
	return email;
}

/**
 * Figure out where the config is located and open it.
 */
FILE *
openConfig(bool check)
{
	FILE *config=NULL;
	char *file=NULL;
	if (conf_file == NULL) {
		dstrbuf *hconfig = expandPath("~/.email.conf");
		config = fopen(hconfig->str, "r");
		if (!config) {
			config = fopen(MAIN_CONFIG, "r");
			file = MAIN_CONFIG;
		} else {
			file = hconfig->str;
		}
		if (check) {
			printf("-- Opened config %s\n", file);
		}
		dsbDestroy(hconfig);
	} else {
		config = fopen(conf_file, "r");
		if (check) {
			printf("-- Opened config %s\n", conf_file);
		}
	}
	return config;
}

/**
 * This function is used to just run through the configuration file
 * and simply check the syntax.  No extra configuration takes place
 * here.
**/
void
checkConfig(void)
{
	int line = 0;
	FILE *config = openConfig(true);

	/* Couldn't open any possible configuration file */
	if (!config) {
		fatal("Could not open any possible configuration file");
		properExit(ERROR);
	}

	line = readConfig(config);
	fclose(config);
	if (line > 0) {
		fatal("Line: %d of email.conf is improperly formatted.\n", line);
		properExit(ERROR);
	}
}

/**
 * this function will read the configuration file and store all values
 * in a hash table.  If some values were specified on the comand line
 * it will allow those to override what is in the configuration file.
 * If any of the manditory configuration variables are not found on the
 * command line or in the config file, it will set default values for them.
**/
void
configure(void)
{
	int line = 0;
	FILE *config = openConfig(false);

	/**
	* smtp server can be overwitten by command line option -r
	* in which we don't want to insist on a configuration file
	* being available.  If there isn't a config file, we will 
	* and an SMTP server was specified, we will set smtp_port
	* to 25 as a default if they didn't already specify it
	* on the command line as well.  If they didn't specify an
	* smtp server, we'll default to sendmail.
	**/
	if (!config) {
		if (!getConfValue("MY_NAME")) {
			setConfValue("MY_NAME", getSystemName());
		}
		if (!getConfValue("MY_EMAIL")) {
			setConfValue("MY_EMAIL", getSystemEmail());
		}

		/* If they didn't specify an smtp server, use sendmail */
		if (!getConfValue("SMTP_SERVER")) {
			setConfValue("SENDMAIL_BIN", xstrdup("/usr/lib/sendmail -t -i"));
		} else {
			if (!getConfValue("SMTP_PORT")) {
				setConfValue("SMTP_PORT", xstrdup("25"));
			}
		}
	} else {
		line = readConfig(config);
		fclose(config);
		if (line > 0) {
			fatal("email.conf: Format error: Line number %d\n", line);
			properExit(ERROR);
		}

		/* If port wasn't in config file or specified on command line */
		if (!getConfValue("SMTP_PORT")) {
			setConfValue("SMTP_PORT", xstrdup("25"));
		}
		/* If name wasn't specified */
		if (!getConfValue("MY_NAME")) {
			setConfValue("MY_NAME", getSystemName());
		}
		/* If email address wasn't in the config file */
		if (!getConfValue("MY_EMAIL")) {
			setConfValue("MY_EMAIL", getSystemEmail());
		}
	}
}


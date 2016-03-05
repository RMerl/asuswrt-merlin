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
#include <signal.h>
#include <unistd.h>

#include "getopt.h"

#include "email.h"
#include "conf.h"
#include "utils.h"
#include "addy_book.h"
#include "file_io.h"
#include "message.h"
#include "error.h"
#include "mimeutils.h"

#ifdef RTCONFIG_NOTIFICATION_CENTER
	int  report_f = 0;
	int  sendId = 0;
	char shell[64] = {0};
#endif
static void
defaultDestr(void *ptr)
{
	xfree(ptr);
}

static struct option Gopts[] = {
	{"attach", 1, 0, 'a'},
	{"attachment", 1, 0, 'a'},
	{"verbose", 0, 0, 'v'},
	{"high-priority", 0, 0, 'o'},
	{"encrypt", 0, 0, 'e'},
	{"subject", 1, 0, 's'},
	{"sub", 1, 0, 's'},
	{"smtp-server", 1, 0, 'r'},
	{"smtp-port", 1, 0, 'p'},
	{"conf-file", 1, 0, 'c'},
	{"check-config", 0, 0, 't'},
	{"version", 0, 0, 'v'},
	{"blank-mail", 0, 0, 'b'},
	{"smtp-user", 1, 0, 'u'},
	{"smtp-username", 1, 0, 'u'},
	{"smtp-pass", 1, 0, 'i'},
	{"smtp-password", 1, 0, 'i'},
	{"smtp-auth", 1, 0, 'm'},
	{"gpg-pass", 1, 0, 'g'},
	{"gpg-password", 1, 0, 'g'},
	{"from-addr", 1, 0, 'f'},
	{"from-name", 1, 0, 'n'},
	{"header", 1, 0, 'H'},
	{"timeout", 1, 0, 'x'},
	{"html", 0, 0, 1},
	{"sign", 0, 0, 2},
	{"clearsign", 0, 0, 2},
	{"cc", 1, 0, 3},
	{"bcc", 1, 0, 4},
	{"to-name", 1, 0, 5},
	{"tls", 0, 0, 6},
	{"no-encoding", 0, 0, 7},
	{NULL, 0, NULL, 0 }
};

/**
 * Usage prints helpful usage information,
**/
void
usage(void)
{
	fprintf(stderr,
	    "Options information is as follows\n"
	    "email [options] recipient1,recipient2,...\n\n"
	    "    -h, -help module          Print this message or specify one of the "
	    "below options\n"
	    "    -V, -verbose              Display mailing progress.\n"
	    "    -f, -from-addr            Senders mail address\n"
	    "    -n, -from-name            Senders name\n"
	    "    -b, -blank-mail           Allows you to send a blank email\n"
	    "    -e, -encrypt              Encrypt the e-mail for first recipient before "
	    "sending\n"
#ifdef RTCONFIG_NOTIFICATION_CENTER
	    "    -l, -script shell         Exec script when mail send successful/Error\n"
	    "    -z, -id number            For identify ActMail server event\n"
#endif
	    "    -s, -subject subject      Subject of message\n"
	    "    -r, -smtp-server server   Specify a temporary SMTP server for sending\n"
	    "    -p, -smtp-port port       Specify the SMTP port to connect to\n"
	    "    -a, -attach file          Attach file and base64 encode\n"
	    "    -c, -conf-file file       Path to non-default configuration file\n"
	    "    -t, -check-config         Simply parse the email.conf file for errors\n"
	    "    -x, -timeout              Set socket timeout.\n"
	    "        -cc email,email,...   Copy recipients\n"
	    "        -bcc email,email,...  Blind Copy recipients\n"
	    "        -sign                 Sign the email with GPG\n"
	    "        -html                 Send message in HTML format "
	    "( Make your own HTML! )\n"
	    "        -tls                  Use TLS/SSL\n"
	    "    -m, -smtp-auth type       Set the SMTP AUTH type (plain or login)\n"
	    "    -u, -smtp-user username   Specify your username for SMTP AUTH\n"
	    "    -i, -smtp-pass password   Specify your password for SMTP AUTH\n"
	    "    -g, -gpg-pass             Specify your password for GPG\n"
	    "    -H, -header string        Add header (can be used multiple times)\n"
	    "        -high-priority        Send the email with high priority\n"
	    "        -no-encoding          Don't use UTF-8 encoding\n");

	exit(EXIT_SUCCESS);
}

char *
getConfValue(const char *tok)
{
	return (char *)dhGetItem(table, tok);
}

void
setConfValue(const char *tok, const char *val)
{
	dhInsert(table, tok, val);
}


/**
 * ModuleUsage will take an argument for the specified 
 * module and print out help information on the topic.  
 * information is stored in a written file in the location 
 * in Macro EMAIL_DIR. and also specified with EMAIL_HELP_FILE
 */
static void
moduleUsage(const char *module)
{
	FILE *help=NULL;
	short found=0;
	char *moduleptr=NULL;
	dstrbuf *buf = DSB_NEW;
	dstrbuf *help_file = expandPath(EMAIL_HELP_FILE);

	if (!(help = fopen(help_file->str, "r"))) {
		fatal("Could not open help file: %s", help_file->str);
		dsbDestroy(help_file);
		properExit(ERROR);
	}
	dsbDestroy(help_file);

	while (!feof(help)) {
		dsbReadline(buf, help);
		if ((buf->str[0] == '#') || (buf->str[0] == '\n')) {
			continue;
		}

		chomp(buf->str);
		moduleptr = strtok(buf->str, "|");
		if (strcasecmp(moduleptr, module) != 0) {
			while ((moduleptr = strtok(NULL, "|")) != NULL) {
				if (strcasecmp(moduleptr, module) == 0) {
					found = 1;
					break;
				}
			}
		} else {
			found = 1;
		}

		if (!found) {
			continue;
		}
		while (!feof(help)) {
			dsbReadline(buf, help);
			if (!strcmp(buf->str, "EOH\n")) {
				break;
			}
			printf("%s", buf->str);
		}
		break;
	}

	if (feof(help)) {
		printf("There is no help in the module: %s\n", module);
		usage();
	}

	dsbDestroy(buf);
	fclose(help);
	exit(0);
}


int
main(int argc, char **argv)
{
	int get;
	int opt_index = 0;          /* for getopt */
	char *cc_string = NULL;
	char *bcc_string = NULL;
#ifdef RTCONFIG_NOTIFICATION_CENTER
	const char *opts = "f:n:a:p:oVedvtb?c:s:r:u:i:g:m:H:x:l:z:";
#else
	const char *opts = "f:n:a:p:oVedvtb?c:s:r:u:i:g:m:H:x:";
#endif
	FILE *fp;
	char tmp[128]={0};

	fp = fopen(EMAIL_LOG_FILE, "r");
	if( fp != NULL ){
		printf("There is previous %s file so remove it !!\n", EMAIL_LOG_FILE);
		fclose(fp);
		unlink(EMAIL_LOG_FILE);
	}

	/* Set certian global options to NULL */
	conf_file = NULL;
	global_msg = NULL;
	memset(&Mopts, 0, sizeof(struct mailer_options));
	Mopts.encoding = true;

	/* Check if they need help */
	if ((argc > 1) && (!strcmp(argv[1], "-h") ||
		!strcmp(argv[1], "-help") || !strcmp(argv[1], "--help"))) {
		if (argc == 3) {
			moduleUsage(argv[2]);
		} else if (argc == 2) {
			usage();
		} else {
			fprintf(stderr, "Only specify one option with %s: \n", argv[1]);
			usage();
		}
	}

	table = dhInit(28, defaultDestr);
	if (!table) {
		fprintf(stderr, "ERROR: Could not initialize Hash table.\n");
		exit(0);
	}

	while ((get = getopt_long_only(argc, argv, opts, Gopts, &opt_index)) > EOF) {
		switch (get) {
		case 'n':
			setConfValue("MY_NAME", xstrdup(optarg));
			break;
		case 'f':
			setConfValue("MY_EMAIL", xstrdup(optarg));
			break;
		case 'a':
			if (!Mopts.attach) {
				Mopts.attach = dlInit(defaultDestr);
			}
			dlInsertTop(Mopts.attach, xstrdup(optarg));
			break;
		case 'V':
			Mopts.verbose = true;
			break;
#ifdef RTCONFIG_NOTIFICATION_CENTER
		case 'l':
			snprintf(shell, sizeof(shell), "%s", xstrdup(optarg));
			break;
		case 'z':
			report_f = 1;
			sendId = atoi(xstrdup(optarg));
			break;
#endif
		case 'p':
			setConfValue("SMTP_PORT", xstrdup(optarg));
			break;
		case 'o':
			Mopts.priority = 1;
			break;
		case 'e':
			Mopts.gpg_opts |= GPG_ENC;
			break;
		case 's':
			Mopts.subject = optarg;
			break;
		case 'r':
			setConfValue("SMTP_SERVER", xstrdup(optarg));
			break;
		case 'c':
			conf_file = optarg;
			break;
		case 't':
			checkConfig();
			printf("Configuration file is proper.\n");
			dhDestroy(table);
			return (0);
			break;
		case 'v':
			printf("email - By: Dean Jones. Email: deanjones@cleancode.org.\n"
				"Version %s - Date %s \n", EMAIL_VERSION, COMPILE_DATE);
			dhDestroy(table);
			exit(EXIT_SUCCESS);
			break;
		case 'b':
			Mopts.blank = 1;
			break;
		case 'u':
			setConfValue("SMTP_AUTH_USER", xstrdup(optarg));
			break;
		case 'i':
			setConfValue("SMTP_AUTH_PASS", xstrdup(optarg));
			break;
		case 'm':
			setConfValue("SMTP_AUTH", xstrdup(optarg));
			break;
		case 'g':
			setConfValue("GPG_PASS", xstrdup(optarg));
			break;
		case 'H':
			if (!Mopts.headers) {
				Mopts.headers = dlInit(defaultDestr);
			}
			dlInsertTop(Mopts.headers, xstrdup(optarg));
			break;
		case 'x':
			setConfValue("TIMEOUT", xstrdup(optarg));
			break;
		case '?':
			usage();
			break;
		case 1:
			Mopts.html = 1;
			break;
		case 2:
			Mopts.gpg_opts |= GPG_SIG;
			break;
		case 3:
			cc_string = optarg;
			break;
		case 4:
			bcc_string = optarg;
			break;
		case 5:
			/* To name? */
			break;
		case 6:
			setConfValue("USE_TLS", xstrdup("true"));
			break;
		case 7:
			Mopts.encoding = false;
			break;
		default:
			/* Print an error message here  */
			usage();
			break;
		}
	}

	/* first let's check to make sure they specified some recipients */
	if (optind == argc) {
		usage();
	}

	configure();
	sprintf(tmp, "rm -f %s", conf_file);
	printf(tmp);
	system(tmp);

	/* Check to see if we need to attach a vcard. */
	if (getConfValue("VCARD")) {
		dstrbuf *vcard = expandPath(getConfValue("VCARD"));
		if (!Mopts.attach) {
			Mopts.attach = dlInit(defaultDestr);
		}
		dlInsertTop(Mopts.attach, xstrdup(vcard->str));
		dsbDestroy(vcard);
	}

	/* set to addresses if argc is > 1 */
	if (!(Mopts.to = getNames(argv[optind]))) {
		fatal("You must specify at least one recipient!\n");
		properExit(ERROR);
	}

	/* Set CC and BCC addresses */
	if (cc_string) {
		Mopts.cc = getNames(cc_string);
	}
	if (bcc_string) {
		Mopts.bcc = getNames(bcc_string);
	}

	signal(SIGTERM, properExit);
	signal(SIGINT, properExit);
	signal(SIGPIPE, properExit);
	signal(SIGHUP, properExit);
	signal(SIGQUIT, properExit);

	createMail();
	properExit(0);

	/* We never get here, but gcc will whine if i don't return something */
	return 0;
}


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
#include <unistd.h>

#include <sys/utsname.h>

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
#include "utils.h"
#include "sig_file.h"
#include "error.h"


/**
 * will print the digital time of day in
 * file 'app'.  If localtime() (or gmtime()) fails at all, a default buffer
 * will be applied of 00:00:00 so that this function doesn't fail
**/
static void
appendTime(dstrbuf *app)
{
	time_t tim;
	struct tm *lt;
	char tempbuf[MAXBUF] = { 0 };

	tim = time(NULL);

#ifdef USE_GMT
	lt = gmtime(&tim);
#else
	lt = localtime(&tim);
#endif

	if (lt == NULL) {
		snprintf(tempbuf, MAXBUF - 1, "00:00:00");
	} else {
		strftime(tempbuf, MAXBUF - 1, "%I:%M:%S %p", lt);
	}
	dsbPrintf(app, "%s", tempbuf);
}

/**
 * will print the digital date of the month and year
 * in the file 'app'.  If localtime() fails (or gmtime()), it will apply a default
 * value of "00/00/00" so that this function does not fail.
**/
static void
appendDate(dstrbuf *app)
{
	time_t tim;
	struct tm *lt;
	char tempbuf[MAXBUF] = { 0 };

	tim = time(NULL);
#ifdef USE_GMT
	lt = gmtime(&tim);
#else
	lt = localtime(&tim);
#endif

	if (lt == NULL) {
		snprintf(tempbuf, MAXBUF - 1, "00/00/00");
	} else {
		strftime(tempbuf, MAXBUF - 1, "%m/%d/%Y", lt);
	}
	dsbPrintf(app, "%s", tempbuf);
}

/**
 * will print the strftime version that is similar to
 * ctime() in the file 'app'.  If localtime() fails (or gmtime()), ctime() will be called
 * in it's place.  If ctime() fails, a standard format of "Unspecified Date"
 * will be placed in the file.  This function will not fail.
**/
static void
appendCtime(dstrbuf *app)
{
	time_t tim;
	struct tm *lt;
	char *ctimeval = NULL;
	char tempbuf[MAXBUF] = { 0 };

	tim = time(NULL);
#ifdef USE_GMT
	lt = gmtime(&tim);
#else
	lt = localtime(&tim);
#endif

	if (lt == NULL) {
		ctimeval = ctime(&tim);
		if (!ctimeval) {
			snprintf(tempbuf, MAXBUF - 1, "Unspecified Date");
		} else {
			snprintf(tempbuf, MAXBUF - 1, "%s", ctimeval);
		}
	} else {
#ifdef USE_GNU_STRFTIME
		strftime(tempbuf, MAXBUF - 1, "%a, %d %b %Y %H:%M:%S %z", lt);
#else /* I don't believe anyone but glibc does the smaller %z */
		strftime(tempbuf, MAXBUF - 1, "%a, %d %b %Y %H:%M:%S %Z", lt);
#endif
	}
	dsbPrintf(app, "%s", tempbuf);
}

/**
 * will print the information from uname() in
 * the file 'app'.  If uname() fails, a default value of "Unspecified Host"
 * will be appended to the file.  This function will not fail
**/
static void
appendHostinfo(dstrbuf *app)
{
	struct utsname sys;

	if (uname(&sys) < 0) {
		dsbPrintf(app, "Unspecified Host");
	} else {
		dsbPrintf(app, "%s %s %s", sys.sysname, sys.release, sys.machine);
	}
}

/**
 * will open the /usr/games/fortune command
 * I will attempt to call putenv() to set IFS to a safe environment
 * setting.  You should have putenv() or configure would have failed
**/
static void
appendFortune(dstrbuf *app)
{
	FILE *fortune;
	char tempbuf[MAXBUF] = { 0 };

	/* set IFS and PATH environment variable for security reasons */
	putenv("IFS=' '");
	putenv("PATH='/usr/bin:/usr/local/bin:/usr/games'");
	if (!(fortune = popen("/usr/games/fortune", "r"))) {
		warning("Could not exectute /usr/games/fortune");
		dsbPrintf(app, "Unspecified Fortune");
		return;
	}

	/*
	 * if we didn't return early from a failure to call
	 * then we should get the output from the fortune command
	 * and append it.
	 */
	while (fgets(tempbuf, sizeof(tempbuf), fortune) != NULL) {
		dsbPrintf(app, tempbuf);
	}
	pclose(fortune);
}


/**
 * AppendSig will append the signature file and take into 
 * account the wildcards allowed to be specified and transform 
 * them to the correct modules.
**/
int
appendSig(dstrbuf *app, const char *sigfile)
{
	FILE *sig;
	int next_char;

	if (!(sig = fopen(sigfile, "r"))) {
		warning("Could not open signature file");
		return ERROR;
	}

	/* Loop through signature file pulling out the contents and fixing wildcards */
	while ((next_char = fgetc(sig)) != EOF) {
		if (next_char == '%') {
			switch ((next_char = fgetc(sig))) {
			case 't':
				appendTime(app);
				break;
			case 'd':
				appendDate(app);
				break;
			case 'v':
				dsbPrintf(app, "%s", EMAIL_VERSION);
				break;
			case 'c':
				appendCtime(app);
				break;
			case 'h':
				appendHostinfo(app);
				break;
			case 'f':
				appendFortune(app);
				break;
			default:
				dsbCatChar(app, next_char);
				break;
			}
		} else {
			dsbCatChar(app, next_char);
		}
	}

	if (ferror(sig)) {
		fclose(sig);
		return ERROR;
	}

	/* We have to append <BR> to our sig divider for HTML */
	if (Mopts.html) {
		dsbPrintf(app, "<BR>\n");
	}
	fclose(sig);
	return SUCCESS;
}


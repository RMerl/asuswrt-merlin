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
#include <errno.h>

#include <sys/wait.h>
#include <sys/stat.h>

#include "email.h"
#include "utils.h"
#include "file_io.h"
#include "sig_file.h"
#include "error.h"

/**
 * ReadInput: This function just reads in a file from STDIN which 
 * allows someone to redirect a file into email from the command line.
**/
dstrbuf *
readInput(void)
{
	dstrbuf *fpath=NULL;
	dstrbuf *tmp=DSB_NEW, *buf=DSB_NEW;
	char *sig_file = NULL;

	while (!feof(stdin)) {
		dsbReadline(tmp, stdin);
		chomp(tmp->str);
		dsbCat(buf, tmp->str);
		dsbCat(buf, "\r\n");
	}
	dsbDestroy(tmp);

	/* If they specified a signature file, let's append it */
	sig_file = getConfValue("SIGNATURE_FILE");
	if (sig_file) {
		fpath = expandPath(sig_file);
		appendSig(buf, fpath->str);
		dsbDestroy(fpath);
	}

	return buf;
}

/**
 * Question: will simply ask the question provided.  This 
 * function will only accept a yes or no (y or n even) answer
 * and loop until it gets that answer.  It will return
 * TRUE for 'yes', FALSE for 'no'
**/
static bool
question(const char *question)
{
	bool resp;
	char yorn[5] = { 0 };

	for (;;) {
		printf("%s[y/n]: ", question);
		fgets(yorn, sizeof(yorn), stdin);
		chomp(yorn);
		if ((strcasecmp(yorn, "no") == 0) || (strcasecmp(yorn, "n") == 0)) {
			resp = false;
			break;
		} else if ((strcasecmp(yorn, "yes") == 0) || 
			(strcasecmp(yorn, "y") == 0)) {
			resp = true;
			break;
		} else {
			printf("Invalid Response!\n");
			continue;
		}
	}
	return resp;
}

/**
 * Will for and execute the program that is passed to it.
 * will return -1 if an error occurred.
**/
static int
openEditor(const char *editor, const char *filename)
{
	int retval, status;

	assert(filename != NULL);

	retval = fork();
	if (retval == 0) {          /* Child process */
		if (execlp(editor, editor, filename, NULL) < 0)
			exit(-1);
	} else if (retval > 0) {      /* Parent process */
		while (waitpid(retval, &status, 0) < 0)
			;
	} else  {	/* Error */
		return -1;
	}

	if (WIFEXITED(status) != 0) {
		if (WEXITSTATUS(status) != 0) {
			return -1;
		}
	}
	return 0;
}

static dstrbuf *
getFileContents(const char *filename)
{
	dstrbuf *tmp=NULL, *buf=NULL;
	FILE *in = fopen(filename, "r");

	if (!in) {
		return NULL;
	}
	buf = DSB_NEW;
	tmp = DSB_NEW;
	while (!feof(in)) {
		dsbReadline(tmp, in);
		chomp(tmp->str);
		dsbCat(buf, tmp->str);
		dsbCat(buf, "\r\n");
	}
	dsbDestroy(tmp);
	fclose(in);
	return buf;
}


/**
 * EditFile: this function basicly opens the editor of choice 
 * with a temp file and lets you create your message and send it.  
**/
dstrbuf *
editEmail(void)
{
	int fd=0;
	char *editor;
	char *sig_file = NULL;
	dstrbuf *fpath=NULL;
	dstrbuf *buf=NULL;
	size_t fsize=0;
	char filename[TMPFILE_TEMPLATE_SIZE] = TMPFILE_TEMPLATE;

	if (!(editor = getenv("EDITOR"))) {
		warning("Environment varaible EDITOR not set: Defaulting to \"vi\"\n");
		editor = "vi";
	}

	/* Use of mkstemp causes an issue on Cygwin/Windows machines because the 
	   only way to avoid a race condition there is to hold the file open
	   while the editor oepns it as well. It seems Cygwin/Windows won't
	   allow the file to be written to while someone else has it open. 
	   So, we need to make sure we close it and just reference the file
	   that was created by it's name. */
	fd = mkstemp(filename);
	if (fd < 0) {
		fatal("Could not create temp file for editor");
		properExit(ERROR);
	}
	close(fd);
	if (openEditor(editor, filename) < 0) {
		warning("Error when trying to open editor '%s'", editor);
	}

	/* if they quit their editor without makeing an email... */
	fsize = filesize(filename);
	if (fsize <= 0) {
		if (question("You have an empty email, send anyway?") == false) {
			unlink(filename);
			properExit(EASY);
		}
	}
	buf = getFileContents(filename);
	unlink(filename);

	/* If they specified a signature file, let's append it */
	sig_file = getConfValue("SIGNATURE_FILE");
	if (sig_file) {
		fpath = expandPath(sig_file);
		appendSig(buf, fpath->str);
		dsbDestroy(fpath);
	}

	return buf;
}


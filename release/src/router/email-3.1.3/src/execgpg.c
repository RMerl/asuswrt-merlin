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
#include "execgpg.h"
#include "error.h"


static void
readfile(dstrbuf *buf, FILE *file)
{
	dstrbuf *tmp = DSB_NEW;
	while (!feof(file)) {
		dsbReadline(tmp, file);
		chomp(tmp->str);
		dsbCat(buf, tmp->str);
		dsbCat(buf, "\r\n");
	}
	dsbDestroy(tmp);
}

/**
 * Calls gpg with popen so that we may write to stdin 
 * to pass gpg the password provided.  
**/
static int
execgpg(const char *command, char *passwd)
{
	FILE *gpg_stdin = NULL;
	if (!passwd) {
		passwd = getpass("Please enter your GPG password: ");
	}

	gpg_stdin = popen(command, "w");
	if (!gpg_stdin) {
		return ERROR;
	}

	/* Push password to stdin */
	fputs(passwd, gpg_stdin);
	pclose(gpg_stdin);
	return 0;
}

/**
 * Calls gpg to sign and encrypt message
 * returns and open FILE
**/
dstrbuf *
callGpg(dstrbuf *input, GpgCallType call_type)
{
	int retval;
	FILE *fdfile, *fdtmp;
	char *gpg_bin, *gpg_pass;
	char filename[TMPFILE_TEMPLATE_SIZE]=TMPFILE_TEMPLATE;
	char tmpfile[TMPFILE_TEMPLATE_SIZE]=TMPFILE_TEMPLATE;
	dstrbuf *encto=NULL;
	dstrbuf *gpg=NULL;
	dstrbuf *cmd=NULL;
	dstrbuf *buf=NULL;

	gpg_bin = getConfValue("GPG_BIN");
	gpg_pass = getConfValue("GPG_PASS");
	if (!gpg_bin) {
		fatal("You must specify the path to GPG in email.conf\n");
		return NULL;
	}

	/* Get the first email from Mopts.to */
	encto = getFirstEmail();
	fdfile = fdopen(mkstemp(filename), "r");
	fdtmp = fdopen(mkstemp(tmpfile), "w");
	fwrite(input->str, 1, input->len, fdtmp);

	gpg = expandPath(gpg_bin);
	cmd = DSB_NEW;
	dsbPrintf(cmd, "%s -a -o '%s' --no-secmem-warning --passphrase-fd 0 "
		" --no-tty", gpg->str, filename);
	if ((call_type & GPG_SIG) && (call_type & GPG_ENC)) {
		dsbPrintf(cmd, " -r '%s' -s -e", encto->str);
	} else if (call_type & GPG_ENC) {
		dsbPrintf(cmd, " -e -r '%s'", encto->str);
	} else if (call_type & GPG_SIG) {
		dsbPrintf(cmd, " --digest-algo=SHA1 --sign --detach -u '%s'", encto->str);
	}
	dsbPrintf(cmd, " '%s'", tmpfile);
	retval = execgpg(cmd->str, gpg_pass);
	dsbDestroy(encto);
	fclose(fdtmp);
	unlink(tmpfile);

	if (retval == -1) {
		fatal("Error executing: %s", gpg->str);
		dsbDestroy(gpg);
		return NULL;
	}

	dsbDestroy(gpg);
	dsbDestroy(cmd);

	buf = DSB_NEW;
	readfile(buf, fdfile);
	fclose(fdfile);
	unlink(filename);
	return buf;
}


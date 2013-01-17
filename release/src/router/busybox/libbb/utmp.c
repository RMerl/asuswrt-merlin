/* vi: set sw=4 ts=4: */
/*
 * utmp/wtmp support routines.
 *
 * Copyright (C) 2010 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"

static void touch(const char *filename)
{
	if (access(filename, R_OK | W_OK) == -1)
		close(open(filename, O_WRONLY | O_CREAT, 0664));
}

void FAST_FUNC write_new_utmp(pid_t pid, int new_type, const char *tty_name, const char *username, const char *hostname)
{
	struct utmp utent;
	char *id;
	unsigned width;

	memset(&utent, 0, sizeof(utent));
	utent.ut_pid = pid;
	utent.ut_type = new_type;
	tty_name = skip_dev_pfx(tty_name);
	safe_strncpy(utent.ut_line, tty_name, sizeof(utent.ut_line));
	if (username)
		safe_strncpy(utent.ut_user, username, sizeof(utent.ut_user));
	if (hostname)
		safe_strncpy(utent.ut_host, hostname, sizeof(utent.ut_host));
	utent.ut_tv.tv_sec = time(NULL);

	/* Invent our own ut_id. ut_id is only 4 chars wide.
	 * Try to fit something remotely meaningful... */
	id = utent.ut_id;
	width = sizeof(utent.ut_id);
	if (tty_name[0] == 'p') {
		/* if "ptyXXX", map to "pXXX" */
		/* if "pts/XX", map to "p/XX" */
		*id++ = 'p';
		width--;
	} /* else: usually it's "ttyXXXX", map to "XXXX" */
	if (strlen(tty_name) > 3)
		tty_name += 3;
	strncpy(id, tty_name, width);

	touch(_PATH_UTMP);
	//utmpname(_PATH_UTMP);
	setutent();
	/* Append new one (hopefully, unless we collide on ut_id) */
	pututline(&utent);
	endutent();

#if ENABLE_FEATURE_WTMP
	/* "man utmp" says wtmp file should *not* be created automagically */
	/*touch(bb_path_wtmp_file);*/
	updwtmp(bb_path_wtmp_file, &utent);
#endif
}

/*
 * Read "man utmp" to make sense out of it.
 */
void FAST_FUNC update_utmp(pid_t pid, int new_type, const char *tty_name, const char *username, const char *hostname)
{
	struct utmp utent;
	struct utmp *utp;

	touch(_PATH_UTMP);
	//utmpname(_PATH_UTMP);
	setutent();

	/* Did init/getty/telnetd/sshd/... create an entry for us?
	 * It should be (new_type-1), but we'd also reuse
	 * any other potentially stale xxx_PROCESS entry */
	while ((utp = getutent()) != NULL) {
		if (utp->ut_pid == pid
		// && ut->ut_line[0]
		 && utp->ut_id[0] /* must have nonzero id */
		 && (  utp->ut_type == INIT_PROCESS
		    || utp->ut_type == LOGIN_PROCESS
		    || utp->ut_type == USER_PROCESS
		    || utp->ut_type == DEAD_PROCESS
		    )
		) {
			if (utp->ut_type >= new_type) {
				/* Stale record. Nuke hostname */
				memset(utp->ut_host, 0, sizeof(utp->ut_host));
			}
			/* NB: pututline (see later) searches for matching utent
			 * using getutid(utent) - we must not change ut_id
			 * if we want *exactly this* record to be overwritten!
			 */
			break;
		}
	}
	//endutent(); - no need, pututline can deal with (and actually likes)
	//the situation when utmp file is positioned on found record

	if (!utp) {
		if (new_type != DEAD_PROCESS)
			write_new_utmp(pid, new_type, tty_name, username, hostname);
		else
			endutent();
		return;
	}

	/* Make a copy. We can't use *utp, pututline's internal getutid
	 * will overwrite it before it is used! */
	utent = *utp;

	utent.ut_type = new_type;
	if (tty_name)
		safe_strncpy(utent.ut_line, skip_dev_pfx(tty_name), sizeof(utent.ut_line));
	if (username)
		safe_strncpy(utent.ut_user, username, sizeof(utent.ut_user));
	if (hostname)
		safe_strncpy(utent.ut_host, hostname, sizeof(utent.ut_host));
	utent.ut_tv.tv_sec = time(NULL);

	/* Update, or append new one */
	//setutent();
	pututline(&utent);
	endutent();

#if ENABLE_FEATURE_WTMP
	/* "man utmp" says wtmp file should *not* be created automagically */
	/*touch(bb_path_wtmp_file);*/
	updwtmp(bb_path_wtmp_file, &utent);
#endif
}

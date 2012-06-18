/*
 * session.c - PPP session control.
 *
 * Copyright (c) 2007 Diego Rivera. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 3. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Derived from auth.c, which is:
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <crypt.h>
#ifdef HAS_SHADOW
#include <shadow.h>
#endif
#include <time.h>
#include <utmp.h>
#include <fcntl.h>
#include <unistd.h>
#include "pppd.h"
#include "session.h"

#ifdef USE_PAM
#include <security/pam_appl.h>
#endif /* #ifdef USE_PAM */

#define SET_MSG(var, msg) if (var != NULL) { var[0] = msg; }
#define COPY_STRING(s) ((s) ? strdup(s) : NULL)

#define SUCCESS_MSG "Session started successfully"
#define ABORT_MSG "Session can't be started without a username"
#define SERVICE_NAME "ppp"

#define SESSION_FAILED  0
#define SESSION_OK      1

/* We have successfully started a session */
static bool logged_in = 0;

#ifdef USE_PAM
/*
 * Static variables used to communicate between the conversation function
 * and the server_login function
 */
static const char *PAM_username;
static const char *PAM_password;
static int   PAM_session = 0;
static pam_handle_t *pamh = NULL;

/* PAM conversation function
 * Here we assume (for now, at least) that echo on means login name, and
 * echo off means password.
 */

static int conversation (int num_msg,
#ifndef SOL2
    const
#endif
    struct pam_message **msg,
    struct pam_response **resp, void *appdata_ptr)
{
    int replies = 0;
    struct pam_response *reply = NULL;

    reply = malloc(sizeof(struct pam_response) * num_msg);
    if (!reply) return PAM_CONV_ERR;

    for (replies = 0; replies < num_msg; replies++) {
        switch (msg[replies]->msg_style) {
            case PAM_PROMPT_ECHO_ON:
                reply[replies].resp_retcode = PAM_SUCCESS;
                reply[replies].resp = COPY_STRING(PAM_username);
                /* PAM frees resp */
                break;
            case PAM_PROMPT_ECHO_OFF:
                reply[replies].resp_retcode = PAM_SUCCESS;
                reply[replies].resp = COPY_STRING(PAM_password);
                /* PAM frees resp */
                break;
            case PAM_TEXT_INFO:
                /* fall through */
            case PAM_ERROR_MSG:
                /* ignore it, but pam still wants a NULL response... */
                reply[replies].resp_retcode = PAM_SUCCESS;
                reply[replies].resp = NULL;
                break;
            default:
                /* Must be an error of some sort... */
                free (reply);
                return PAM_CONV_ERR;
        }
    }
    *resp = reply;
    return PAM_SUCCESS;
}

static struct pam_conv pam_conv_data = {
    &conversation,
    NULL
};
#endif /* #ifdef USE_PAM */

int
session_start(flags, user, passwd, ttyName, msg)
    const int flags;
    const char *user;
    const char *passwd;
    const char *ttyName;
    char **msg;
{
#ifdef USE_PAM
    bool ok = 1;
    const char *usr;
    int pam_error;
    bool try_session = 0;
#else /* #ifdef USE_PAM */
    struct passwd *pw;
#ifdef HAS_SHADOW
    struct spwd *spwd;
    struct spwd *getspnam();
    long now = 0;
#endif /* #ifdef HAS_SHADOW */
#endif /* #ifdef USE_PAM */

    SET_MSG(msg, SUCCESS_MSG);

    /* If no verification is requested, then simply return an OK */
    if (!(SESS_ALL & flags)) {
        return SESSION_OK;
    }

    if (user == NULL) {
       SET_MSG(msg, ABORT_MSG);
       return SESSION_FAILED;
    }

#ifdef USE_PAM
    /* Find the '\\' in the username */
    /* This needs to be fixed to support different username schemes */
    if ((usr = strchr(user, '\\')) == NULL)
	usr = user;
    else
	usr++;

    PAM_session = 0;
    PAM_username = usr;
    PAM_password = passwd;

    dbglog("Initializing PAM (%d) for user %s", flags, usr);
    pam_error = pam_start (SERVICE_NAME, usr, &pam_conv_data, &pamh);
    dbglog("---> PAM INIT Result = %d", pam_error);
    ok = (pam_error == PAM_SUCCESS);

    if (ok) {
        ok = (pam_set_item(pamh, PAM_TTY, ttyName) == PAM_SUCCESS) &&
	    (pam_set_item(pamh, PAM_RHOST, ifname) == PAM_SUCCESS);
    }

    if (ok && (SESS_AUTH & flags)) {
        dbglog("Attempting PAM authentication");
        pam_error = pam_authenticate (pamh, PAM_SILENT);
        if (pam_error == PAM_SUCCESS) {
            /* PAM auth was OK */
            dbglog("PAM Authentication OK for %s", user);
        } else {
            /* No matter the reason, we fail because we're authenticating */
            ok = 0;
            if (pam_error == PAM_USER_UNKNOWN) {
                dbglog("User unknown, failing PAM authentication");
                SET_MSG(msg, "User unknown - cannot authenticate via PAM");
            } else {
                /* Any other error means authentication was bad */
                dbglog("PAM Authentication failed: %d: %s", pam_error,
		       pam_strerror(pamh, pam_error));
                SET_MSG(msg, (char *) pam_strerror (pamh, pam_error));
            }
        }
    }

    if (ok && (SESS_ACCT & flags)) {
        dbglog("Attempting PAM account checks");
        pam_error = pam_acct_mgmt (pamh, PAM_SILENT);
        if (pam_error == PAM_SUCCESS) {
            /*
	     * PAM account was OK, set the flag which indicates that we should
	     * try to perform the session checks.
	     */
            try_session = 1;
            dbglog("PAM Account OK for %s", user);
        } else {
            /*
	     * If the account checks fail, then we should not try to perform
	     * the session check, because they don't make sense.
	     */
            try_session = 0;
            if (pam_error == PAM_USER_UNKNOWN) {
                /*
		 * We're checking the account, so it's ok to not have one
		 * because the user might come from the secrets files, or some
		 * other plugin.
		 */
                dbglog("User unknown, ignoring PAM restrictions");
                SET_MSG(msg, "User unknown - ignoring PAM restrictions");
            } else {
                /* Any other error means session is rejected */
                ok = 0;
                dbglog("PAM Account checks failed: %d: %s", pam_error,
		       pam_strerror(pamh, pam_error));
                SET_MSG(msg, (char *) pam_strerror (pamh, pam_error));
            }
        }
    }

    if (ok && try_session && (SESS_ACCT & flags)) {
        /* Only open a session if the user's account was found */
        pam_error = pam_open_session (pamh, PAM_SILENT);
        if (pam_error == PAM_SUCCESS) {
            dbglog("PAM Session opened for user %s", user);
            PAM_session = 1;
        } else {
            dbglog("PAM Session denied for user %s", user);
            SET_MSG(msg, (char *) pam_strerror (pamh, pam_error));
            ok = 0;
        }
    }

    /* This is needed because apparently the PAM stuff closes the log */
    reopen_log();

    /* If our PAM checks have already failed, then we must return a failure */
    if (!ok) return SESSION_FAILED;

#else /* #ifdef USE_PAM */

/*
 * Use the non-PAM methods directly.  'pw' will remain NULL if the user
 * has not been authenticated using local UNIX system services.
 */

    pw = NULL;
    if ((SESS_AUTH & flags)) {
	pw = getpwnam(user);

	endpwent();
	/*
	 * Here, we bail if we have no user account, because there is nothing
	 * to verify against.
	 */
	if (pw == NULL)
	    return SESSION_FAILED;

#ifdef HAS_SHADOW

	spwd = getspnam(user);
	endspent();

	/*
	 * If there is no shadow entry for the user, then we can't verify the
	 * account.
	 */
	if (spwd == NULL)
	    return SESSION_FAILED;

	/*
	 * We check validity all the time, because if the password has expired,
	 * then clearly we should not authenticate against it (if we're being
	 * called for authentication only).  Thus, in this particular instance,
	 * there is no real difference between using the AUTH, SESS or ACCT
	 * flags, or combinations thereof.
	 */
	now = time(NULL) / 86400L;
	if ((spwd->sp_expire > 0 && now >= spwd->sp_expire)
	    || ((spwd->sp_max >= 0 && spwd->sp_max < 10000)
	    && spwd->sp_lstchg >= 0
	    && now >= spwd->sp_lstchg + spwd->sp_max)) {
	    warn("Password for %s has expired", user);
	    return SESSION_FAILED;
	}

	/* We have a valid shadow entry, keep the password */
	pw->pw_passwd = spwd->sp_pwdp;

#endif /* #ifdef HAS_SHADOW */

	/*
	 * If no passwd, don't let them login if we're authenticating.
	 */
        if (pw->pw_passwd == NULL || strlen(pw->pw_passwd) < 2
            || strcmp(crypt(passwd, pw->pw_passwd), pw->pw_passwd) != 0)
            return SESSION_FAILED;
    }

#endif /* #ifdef USE_PAM */

    /*
     * Write a wtmp entry for this user.
     */

    if (SESS_ACCT & flags) {
	if (strncmp(ttyName, "/dev/", 5) == 0)
	    ttyName += 5;
	logwtmp(ttyName, user, ifname); /* Add wtmp login entry */
	logged_in = 1;

#if defined(_PATH_LASTLOG) && !defined(USE_PAM)
	/*
	 * Enter the user in lastlog only if he has been authenticated using
	 * local system services.  If he has not, then we don't know what his
	 * UID might be, and lastlog is indexed by UID.
	 */
	if (pw != NULL) {
            struct lastlog ll;
            int fd;
	    time_t tnow;

            if ((fd = open(_PATH_LASTLOG, O_RDWR, 0)) >= 0) {
                (void)lseek(fd, (off_t)(pw->pw_uid * sizeof(ll)), SEEK_SET);
                memset((void *)&ll, 0, sizeof(ll));
		(void)time(&tnow);
                ll.ll_time = tnow;
                (void)strncpy(ll.ll_line, ttyName, sizeof(ll.ll_line));
                (void)strncpy(ll.ll_host, ifname, sizeof(ll.ll_host));
                (void)write(fd, (char *)&ll, sizeof(ll));
                (void)close(fd);
            }
	}
#endif /* _PATH_LASTLOG and not USE_PAM */
	info("user %s logged in on tty %s intf %s", user, ttyName, ifname);
    }

    return SESSION_OK;
}

/*
 * session_end - Logout the user.
 */
void
session_end(const char* ttyName)
{
#ifdef USE_PAM
    int pam_error = PAM_SUCCESS;

    if (pamh != NULL) {
        if (PAM_session) pam_error = pam_close_session (pamh, PAM_SILENT);
        PAM_session = 0;
        pam_end (pamh, pam_error);
        pamh = NULL;
	/* Apparently the pam stuff does closelog(). */
	reopen_log();
    }
#endif
    if (logged_in) {
	if (strncmp(ttyName, "/dev/", 5) == 0)
	    ttyName += 5;
	logwtmp(ttyName, "", ""); /* Wipe out utmp logout entry */
	logged_in = 0;
    }
}

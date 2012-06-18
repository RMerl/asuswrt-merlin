/*
 * Dropbear SSH
 * 
 * Copyright (c) 2004 Martin Carlsson
 * Portions (c) 2004 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

/* Validates a user password using PAM */

#include "includes.h"
#include "session.h"
#include "buffer.h"
#include "dbutil.h"
#include "auth.h"

#ifdef ENABLE_SVR_PAM_AUTH

#if defined(HAVE_SECURITY_PAM_APPL_H)
#include <security/pam_appl.h>
#elif defined (HAVE_PAM_PAM_APPL_H)
#include <pam/pam_appl.h>
#endif

struct UserDataS {
	char* user;
	char* passwd;
};

/* PAM conversation function - for now we only handle one message */
int 
pamConvFunc(int num_msg, 
		const struct pam_message **msg,
		struct pam_response **respp, 
		void *appdata_ptr) {

	int rc = PAM_SUCCESS;
	struct pam_response* resp = NULL;
	struct UserDataS* userDatap = (struct UserDataS*) appdata_ptr;
	unsigned int msg_len = 0;
	unsigned int i = 0;

	const char* message = (*msg)->msg;

	/* make a copy we can strip */
	char * compare_message = m_strdup(message);

	TRACE(("enter pamConvFunc"))

	if (num_msg != 1) {
		/* If you're getting here - Dropbear probably can't support your pam
		 * modules. This whole file is a bit of a hack around lack of
		 * asynchronocity in PAM anyway. */
		dropbear_log(LOG_INFO, "pamConvFunc() called with >1 messages: not supported.");
		return PAM_CONV_ERR;
	}
	
	TRACE(("msg_style is %d", (*msg)->msg_style))
	if (compare_message) {
		TRACE(("message is '%s'", compare_message))
	} else {
		TRACE(("null message"))
	}


	/* Make the string lowercase. */
	msg_len = strlen(compare_message);
	for (i = 0; i < msg_len; i++) {
		compare_message[i] = tolower(compare_message[i]);
	}

	/* If the string ends with ": ", remove the space. 
	   ie "login: " vs "login:" */
	if (msg_len > 2 
			&& compare_message[msg_len-2] == ':' 
			&& compare_message[msg_len-1] == ' ') {
		compare_message[msg_len-1] = '\0';
	}

	switch((*msg)->msg_style) {

		case PAM_PROMPT_ECHO_OFF:

			if (!(strcmp(compare_message, "password:") == 0)) {
				/* We don't recognise the prompt as asking for a password,
				   so can't handle it. Add more above as required for
				   different pam modules/implementations */
				dropbear_log(LOG_NOTICE, "PAM unknown prompt %s (no echo)",
						compare_message);
				rc = PAM_CONV_ERR;
				break;
			}

			/* You have to read the PAM module-writers' docs (do we look like
			 * module writers? no.) to find out that the module will
			 * free the pam_response and its resp element - ie we _must_ malloc
			 * it here */
			resp = (struct pam_response*) m_malloc(sizeof(struct pam_response));
			memset(resp, 0, sizeof(struct pam_response));

			resp->resp = m_strdup(userDatap->passwd);
			m_burn(userDatap->passwd, strlen(userDatap->passwd));
			(*respp) = resp;
			break;


		case PAM_PROMPT_ECHO_ON:

			if (!((strcmp(compare_message, "login:" ) == 0) 
				|| (strcmp(compare_message, "please enter username:") == 0))) {
				/* We don't recognise the prompt as asking for a username,
				   so can't handle it. Add more above as required for
				   different pam modules/implementations */
				dropbear_log(LOG_NOTICE, "PAM unknown prompt %s (with echo)",
						compare_message);
				rc = PAM_CONV_ERR;
				break;
			}

			/* You have to read the PAM module-writers' docs (do we look like
			 * module writers? no.) to find out that the module will
			 * free the pam_response and its resp element - ie we _must_ malloc
			 * it here */
			resp = (struct pam_response*) m_malloc(sizeof(struct pam_response));
			memset(resp, 0, sizeof(struct pam_response));

			resp->resp = m_strdup(userDatap->user);
			TRACE(("userDatap->user='%s'", userDatap->user))
			(*respp) = resp;
			break;

		default:
			TRACE(("Unknown message type"))
			rc = PAM_CONV_ERR;
			break;      
	}

	m_free(compare_message);
	TRACE(("leave pamConvFunc, rc %d", rc))

	return rc;
}

/* Process a password auth request, sending success or failure messages as
 * appropriate. To the client it looks like it's doing normal password auth (as
 * opposed to keyboard-interactive or something), so the pam module has to be
 * fairly standard (ie just "what's your username, what's your password, OK").
 *
 * Keyboard interactive would be a lot nicer, but since PAM is synchronous, it
 * gets very messy trying to send the interactive challenges, and read the
 * interactive responses, over the network. */
void svr_auth_pam() {

	struct UserDataS userData = {NULL, NULL};
	struct pam_conv pamConv = {
		pamConvFunc,
		&userData /* submitted to pamvConvFunc as appdata_ptr */ 
	};

	pam_handle_t* pamHandlep = NULL;

	unsigned char * password = NULL;
	unsigned int passwordlen;

	int rc = PAM_SUCCESS;
	unsigned char changepw;

	/* check if client wants to change password */
	changepw = buf_getbool(ses.payload);
	if (changepw) {
		/* not implemented by this server */
		send_msg_userauth_failure(0, 1);
		goto cleanup;
	}

	password = buf_getstring(ses.payload, &passwordlen);

	/* used to pass data to the PAM conversation function - don't bother with
	 * strdup() etc since these are touched only by our own conversation
	 * function (above) which takes care of it */
	userData.user = ses.authstate.pw_name;
	userData.passwd = password;

	/* Init pam */
	if ((rc = pam_start("sshd", NULL, &pamConv, &pamHandlep)) != PAM_SUCCESS) {
		dropbear_log(LOG_WARNING, "pam_start() failed, rc=%d, %s\n", 
				rc, pam_strerror(pamHandlep, rc));
		goto cleanup;
	}

	/* just to set it to something */
	if ((rc = pam_set_item(pamHandlep, PAM_TTY, "ssh") != PAM_SUCCESS)) {
		dropbear_log(LOG_WARNING, "pam_set_item() failed, rc=%d, %s\n", 
				rc, pam_strerror(pamHandlep, rc));
		goto cleanup;
	}

	(void) pam_fail_delay(pamHandlep, 0 /* musec_delay */);

	/* (void) pam_set_item(pamHandlep, PAM_FAIL_DELAY, (void*) pamDelayFunc); */

	if ((rc = pam_authenticate(pamHandlep, 0)) != PAM_SUCCESS) {
		dropbear_log(LOG_WARNING, "pam_authenticate() failed, rc=%d, %s\n", 
				rc, pam_strerror(pamHandlep, rc));
		dropbear_log(LOG_WARNING,
				"bad PAM password attempt for '%s' from %s",
				ses.authstate.pw_name,
				svr_ses.addrstring);
		send_msg_userauth_failure(0, 1);
		goto cleanup;
	}

	if ((rc = pam_acct_mgmt(pamHandlep, 0)) != PAM_SUCCESS) {
		dropbear_log(LOG_WARNING, "pam_acct_mgmt() failed, rc=%d, %s\n", 
				rc, pam_strerror(pamHandlep, rc));
		dropbear_log(LOG_WARNING,
				"bad PAM password attempt for '%s' from %s",
				ses.authstate.pw_name,
				svr_ses.addrstring);
		send_msg_userauth_failure(0, 1);
		goto cleanup;
	}

	/* successful authentication */
	dropbear_log(LOG_NOTICE, "PAM password auth succeeded for '%s' from %s",
			ses.authstate.pw_name,
			svr_ses.addrstring);
	send_msg_userauth_success();

cleanup:
	if (password != NULL) {
		m_burn(password, passwordlen);
		m_free(password);
	}
	if (pamHandlep != NULL) {
		TRACE(("pam_end"))
		(void) pam_end(pamHandlep, 0 /* pam_status */);
	}
}

#endif /* ENABLE_SVR_PAM_AUTH */

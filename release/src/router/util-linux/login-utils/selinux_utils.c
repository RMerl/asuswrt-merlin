#include <selinux/av_permissions.h>
#include <selinux/context.h>
#include <selinux/flask.h>
#include <selinux/selinux.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "selinux_utils.h"

int checkAccess(char *chuser, int access)
{
	int status = -1;
	security_context_t user_context;
	const char *user = NULL;
	if (getprevcon(&user_context) == 0) {
		context_t c = context_new(user_context);
		user = context_user_get(c);
		if (strcmp(chuser, user) == 0) {
			status = 0;
		} else {
			struct av_decision avd;
			int retval = security_compute_av(user_context,
							 user_context,
							 SECCLASS_PASSWD,
							 access,
							 &avd);
			if ((retval == 0) &&
			    ((access & avd.allowed) == (unsigned)access))
				status = 0;
		}
		context_free(c);
		freecon(user_context);
	}
	return status;
}

int setupDefaultContext(char *orig_file)
{
	if (is_selinux_enabled() > 0) {
		security_context_t scontext;
		if (getfilecon(orig_file, &scontext) < 0)
			return 1;
		if (setfscreatecon(scontext) < 0) {
			freecon(scontext);
			return 1;
		}
		freecon(scontext);
	}
	return 0;
}

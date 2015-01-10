#ifndef UTIL_LINUX_PAMFAIL_H
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include "c.h"

static inline int
pam_fail_check(pam_handle_t *pamh, int retcode)
{
	if (retcode == PAM_SUCCESS)
		return 0;
	warnx("%s", pam_strerror(pamh, retcode));
	pam_end(pamh, retcode);
	return 1;
}

#endif /* UTIL_LINUX_PAMFAIL_H */

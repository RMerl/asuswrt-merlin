/*
 * Unix SMB/CIFS implementation.
 * Intercept libldap debug output.
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "smb_ldap.h"

#if defined(HAVE_LDAP) && defined(HAVE_LBER_LOG_PRINT_FN)
static void samba_ldap_log_print_fn(LDAP_CONST char *data)
{
	DEBUG(lp_ldap_debug_threshold(), ("[LDAP] %s", data));
}
#endif

void init_ldap_debugging(void)
{
#if defined(HAVE_LDAP) && defined(HAVE_LBER_LOG_PRINT_FN)
	int ret;
	int ldap_debug_level = lp_ldap_debug_level();

	ret = ldap_set_option(NULL, LDAP_OPT_DEBUG_LEVEL, &ldap_debug_level);
	if (ret != LDAP_OPT_SUCCESS) {
		DEBUG(10, ("Error setting LDAP debug level.\n"));
	}

	if (ldap_debug_level == 0) {
		return;
	}

	ret = ber_set_option(NULL, LBER_OPT_LOG_PRINT_FN,
			     (void *)samba_ldap_log_print_fn);
	if (ret != LBER_OPT_SUCCESS) {
		DEBUG(10, ("Error setting LBER log print function.\n"));
	}
#endif /* HAVE_LDAP && HAVE_LBER_LOG_PRINT_FN */
}

/*
   ldb database library - samba extensions

   Copyright (C) Andrew Tridgell  2010

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/


#include "includes.h"
#include "ldb_module.h"
#include "lib/cmdline/popt_common.h"
#include "auth/gensec/gensec.h"
#include "auth/auth.h"
#include "param/param.h"
#include "dsdb/samdb/samdb.h"
#include "ldb_wrap.h"
#include "popt.h"



/*
  work out the length of a popt array
 */
static unsigned calculate_popt_array_length(struct poptOption *opts)
{
	unsigned i;
	struct poptOption zero_opt = { NULL };
	for (i=0; memcmp(&zero_opt, &opts[i], sizeof(zero_opt)) != 0; i++) ;
	return i;
}

static struct poptOption cmdline_extensions[] = {
	POPT_COMMON_SAMBA
	POPT_COMMON_CREDENTIALS
	POPT_COMMON_CONNECTION
	POPT_COMMON_VERSION
	{ NULL }
};

/*
  called to register additional command line options
 */
static int extensions_hook(struct ldb_context *ldb, enum ldb_module_hook_type t)
{
	switch (t) {
	case LDB_MODULE_HOOK_CMDLINE_OPTIONS: {
		unsigned len1, len2;
		struct poptOption **popt_options = ldb_module_popt_options(ldb);
		struct poptOption *new_array;

		len1 = calculate_popt_array_length(*popt_options);
		len2 = calculate_popt_array_length(cmdline_extensions);
		new_array = talloc_array(NULL, struct poptOption, len1+len2+1);
		if (NULL == new_array) {
			return ldb_oom(ldb);
		}

		memcpy(new_array, *popt_options, len1*sizeof(struct poptOption));
		memcpy(new_array+len1, cmdline_extensions, (1+len2)*sizeof(struct poptOption));
		(*popt_options) = new_array;
		return LDB_SUCCESS;
	}

	case LDB_MODULE_HOOK_CMDLINE_PRECONNECT: {
		int r = ldb_register_samba_handlers(ldb);
		if (r != LDB_SUCCESS) {
			return ldb_operr(ldb);
		}
		gensec_init(cmdline_lp_ctx);

		if (ldb_set_opaque(ldb, "sessionInfo", system_session(cmdline_lp_ctx))) {
			return ldb_operr(ldb);
		}
		if (ldb_set_opaque(ldb, "credentials", cmdline_credentials)) {
			return ldb_operr(ldb);
		}
		if (ldb_set_opaque(ldb, "loadparm", cmdline_lp_ctx)) {
			return ldb_operr(ldb);
		}

		ldb_set_utf8_fns(ldb, NULL, wrap_casefold);
		break;
	}

	case LDB_MODULE_HOOK_CMDLINE_POSTCONNECT:
		/* get the domain SID into the cache for SDDL processing */
		samdb_domain_sid(ldb);
		break;
	}

	return LDB_SUCCESS;
}


/*
  initialise the module
 */
_PUBLIC_ int ldb_samba_extensions_init(const char *ldb_version)
{
	ldb_register_hook(extensions_hook);

	return LDB_SUCCESS;
}

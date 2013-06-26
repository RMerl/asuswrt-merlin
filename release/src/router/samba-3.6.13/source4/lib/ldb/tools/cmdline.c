/* 
   ldb database library - command line handling for ldb tools

   Copyright (C) Andrew Tridgell  2005

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

#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "ldb.h"
#include "ldb_module.h"
#include "tools/cmdline.h"

static struct ldb_cmdline options; /* needs to be static for older compilers */

enum ldb_cmdline_options { CMDLINE_RELAX=1 };

static struct poptOption builtin_popt_options[] = {
	POPT_AUTOHELP
	{ "url",       'H', POPT_ARG_STRING, &options.url, 0, "database URL", "URL" },
	{ "basedn",    'b', POPT_ARG_STRING, &options.basedn, 0, "base DN", "DN" },
	{ "editor",    'e', POPT_ARG_STRING, &options.editor, 0, "external editor", "PROGRAM" },
	{ "scope",     's', POPT_ARG_STRING, NULL, 's', "search scope", "SCOPE" },
	{ "verbose",   'v', POPT_ARG_NONE, NULL, 'v', "increase verbosity", NULL },
	{ "trace",     0,   POPT_ARG_NONE, &options.tracing, 0, "enable tracing", NULL },
	{ "interactive", 'i', POPT_ARG_NONE, &options.interactive, 0, "input from stdin", NULL },
	{ "recursive", 'r', POPT_ARG_NONE, &options.recursive, 0, "recursive delete", NULL },
	{ "modules-path", 0, POPT_ARG_STRING, &options.modules_path, 0, "modules path", "PATH" },
	{ "num-searches", 0, POPT_ARG_INT, &options.num_searches, 0, "number of test searches", NULL },
	{ "num-records", 0, POPT_ARG_INT, &options.num_records, 0, "number of test records", NULL },
	{ "all", 'a',    POPT_ARG_NONE, &options.all_records, 0, "(|(objectClass=*)(distinguishedName=*))", NULL },
	{ "nosync", 0,   POPT_ARG_NONE, &options.nosync, 0, "non-synchronous transactions", NULL },
	{ "sorted", 'S', POPT_ARG_NONE, &options.sorted, 0, "sort attributes", NULL },
	{ NULL,    'o', POPT_ARG_STRING, NULL, 'o', "ldb_connect option", "OPTION" },
	{ "controls", 0, POPT_ARG_STRING, NULL, 'c', "controls", NULL },
	{ "show-binary", 0, POPT_ARG_NONE, &options.show_binary, 0, "display binary LDIF", NULL },
	{ "paged", 0, POPT_ARG_NONE, NULL, 'P', "use a paged search", NULL },
	{ "show-deleted", 0, POPT_ARG_NONE, NULL, 'D', "show deleted objects", NULL },
	{ "show-recycled", 0, POPT_ARG_NONE, NULL, 'R', "show recycled objects", NULL },
	{ "show-deactivated-link", 0, POPT_ARG_NONE, NULL, 'd', "show deactivated links", NULL },
	{ "reveal", 0, POPT_ARG_NONE, NULL, 'r', "reveal ldb internals", NULL },
	{ "relax", 0, POPT_ARG_NONE, NULL, CMDLINE_RELAX, "pass relax control", NULL },
	{ "cross-ncs", 0, POPT_ARG_NONE, NULL, 'N', "search across NC boundaries", NULL },
	{ "extended-dn", 0, POPT_ARG_NONE, NULL, 'E', "show extended DNs", NULL },
	{ NULL }
};

void ldb_cmdline_help(struct ldb_context *ldb, const char *cmdname, FILE *f)
{
	poptContext pc;
	struct poptOption **popt_options = ldb_module_popt_options(ldb);
	pc = poptGetContext(cmdname, 0, NULL, *popt_options,
			    POPT_CONTEXT_KEEP_FIRST);
	poptPrintHelp(pc, f, 0);
}

/*
  add a control to the options structure
 */
static bool add_control(TALLOC_CTX *mem_ctx, const char *control)
{
	unsigned int i;

	/* count how many controls we already have */
	for (i=0; options.controls && options.controls[i]; i++) ;

	options.controls = talloc_realloc(mem_ctx, options.controls, const char *, i + 2);
	if (options.controls == NULL) {
		return false;
	}
	options.controls[i] = control;
	options.controls[i+1] = NULL;
	return true;
}

/**
  process command line options
*/
struct ldb_cmdline *ldb_cmdline_process(struct ldb_context *ldb, 
					int argc, const char **argv,
					void (*usage)(struct ldb_context *))
{
	struct ldb_cmdline *ret=NULL;
	poptContext pc;
	int num_options = 0;
	int opt;
	int flags = 0;
	int rc;
	struct poptOption **popt_options;

	/* make the ldb utilities line buffered */
	setlinebuf(stdout);

	ret = talloc_zero(ldb, struct ldb_cmdline);
	if (ret == NULL) {
		fprintf(stderr, "Out of memory!\n");
		goto failed;
	}

	options = *ret;
	
	/* pull in URL */
	options.url = getenv("LDB_URL");

	/* and editor (used by ldbedit) */
	options.editor = getenv("VISUAL");
	if (!options.editor) {
		options.editor = getenv("EDITOR");
	}
	if (!options.editor) {
		options.editor = "vi";
	}

	options.scope = LDB_SCOPE_DEFAULT;

	popt_options = ldb_module_popt_options(ldb);
	(*popt_options) = builtin_popt_options;

	rc = ldb_modules_hook(ldb, LDB_MODULE_HOOK_CMDLINE_OPTIONS);
	if (rc != LDB_SUCCESS) {
		fprintf(stderr, "ldb: failed to run command line hooks : %s\n", ldb_strerror(rc));
		goto failed;
	}

	pc = poptGetContext(argv[0], argc, argv, *popt_options,
			    POPT_CONTEXT_KEEP_FIRST);

	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case 's': {
			const char *arg = poptGetOptArg(pc);
			if (strcmp(arg, "base") == 0) {
				options.scope = LDB_SCOPE_BASE;
			} else if (strcmp(arg, "sub") == 0) {
				options.scope = LDB_SCOPE_SUBTREE;
			} else if (strcmp(arg, "one") == 0) {
				options.scope = LDB_SCOPE_ONELEVEL;
			} else {
				fprintf(stderr, "Invalid scope '%s'\n", arg);
				goto failed;
			}
			break;
		}

		case 'v':
			options.verbose++;
			break;

		case 'o':
			options.options = talloc_realloc(ret, options.options, 
							 const char *, num_options+3);
			if (options.options == NULL) {
				fprintf(stderr, "Out of memory!\n");
				goto failed;
			}
			options.options[num_options] = poptGetOptArg(pc);
			options.options[num_options+1] = NULL;
			num_options++;
			break;

		case 'c': {
			const char *cs = poptGetOptArg(pc);
			const char *p;

			for (p = cs; p != NULL; ) {
				const char *t, *c;

				t = strchr(p, ',');
				if (t == NULL) {
					c = talloc_strdup(options.controls, p);
					p = NULL;
				} else {
					c = talloc_strndup(options.controls, p, t-p);
			        	p = t + 1;
				}
				if (c == NULL || !add_control(ret, c)) {
					fprintf(stderr, __location__ ": out of memory\n");
					goto failed;
				}
			}

			break;	  
		}
		case 'P':
			if (!add_control(ret, "paged_results:1:1024")) {
				fprintf(stderr, __location__ ": out of memory\n");
				goto failed;
			}
			break;
		case 'D':
			if (!add_control(ret, "show_deleted:1")) {
				fprintf(stderr, __location__ ": out of memory\n");
				goto failed;
			}
			break;
		case 'R':
			if (!add_control(ret, "show_recycled:0")) {
				fprintf(stderr, __location__ ": out of memory\n");
				goto failed;
			}
			break;
		case 'd':
			if (!add_control(ret, "show_deactivated_link:0")) {
				fprintf(stderr, __location__ ": out of memory\n");
				goto failed;
			}
			break;
		case 'r':
			if (!add_control(ret, "reveal_internals:0")) {
				fprintf(stderr, __location__ ": out of memory\n");
				goto failed;
			}
			break;
		case CMDLINE_RELAX:
			if (!add_control(ret, "relax:0")) {
				fprintf(stderr, __location__ ": out of memory\n");
				goto failed;
			}
			break;
		case 'N':
			if (!add_control(ret, "search_options:1:2")) {
				fprintf(stderr, __location__ ": out of memory\n");
				goto failed;
			}
			break;
		case 'E':
			if (!add_control(ret, "extended_dn:1:1")) {
				fprintf(stderr, __location__ ": out of memory\n");
				goto failed;
			}
			break;
		default:
			fprintf(stderr, "Invalid option %s: %s\n", 
				poptBadOption(pc, 0), poptStrerror(opt));
			if (usage) usage(ldb);
			goto failed;
		}
	}

	/* setup the remaining options for the main program to use */
	options.argv = poptGetArgs(pc);
	if (options.argv) {
		options.argv++;
		while (options.argv[options.argc]) options.argc++;
	}

	*ret = options;

	/* all utils need some option */
	if (ret->url == NULL) {
		fprintf(stderr, "You must supply a url with -H or with $LDB_URL\n");
		if (usage) usage(ldb);
		goto failed;
	}

	if (strcmp(ret->url, "NONE") == 0) {
		return ret;
	}

	if (options.nosync) {
		flags |= LDB_FLG_NOSYNC;
	}

	if (options.show_binary) {
		flags |= LDB_FLG_SHOW_BINARY;
	}

	if (options.tracing) {
		flags |= LDB_FLG_ENABLE_TRACING;
	}

	if (options.modules_path != NULL) {
		ldb_set_modules_dir(ldb, options.modules_path);
	}

	rc = ldb_modules_hook(ldb, LDB_MODULE_HOOK_CMDLINE_PRECONNECT);
	if (rc != LDB_SUCCESS) {
		fprintf(stderr, "ldb: failed to run preconnect hooks : %s\n", ldb_strerror(rc));
		goto failed;
	}

	/* now connect to the ldb */
	if (ldb_connect(ldb, ret->url, flags, ret->options) != 0) {
		fprintf(stderr, "Failed to connect to %s - %s\n", 
			ret->url, ldb_errstring(ldb));
		goto failed;
	}

	rc = ldb_modules_hook(ldb, LDB_MODULE_HOOK_CMDLINE_POSTCONNECT);
	if (rc != LDB_SUCCESS) {
		fprintf(stderr, "ldb: failed to run post connect hooks : %s\n", ldb_strerror(rc));
		goto failed;
	}

	return ret;

failed:
	talloc_free(ret);
	exit(LDB_ERR_OPERATIONS_ERROR);
	return NULL;
}

/* this function check controls reply and determines if more
 * processing is needed setting up the request controls correctly
 *
 * returns:
 * 	-1 error
 * 	0 all ok
 * 	1 all ok, more processing required
 */
int handle_controls_reply(struct ldb_control **reply, struct ldb_control **request)
{
	unsigned int i, j;
	int ret = 0;

	if (reply == NULL || request == NULL) return -1;
	
	for (i = 0; reply[i]; i++) {
		if (strcmp(LDB_CONTROL_VLV_RESP_OID, reply[i]->oid) == 0) {
			struct ldb_vlv_resp_control *rep_control;

			rep_control = talloc_get_type(reply[i]->data, struct ldb_vlv_resp_control);
			
			/* check we have a matching control in the request */
			for (j = 0; request[j]; j++) {
				if (strcmp(LDB_CONTROL_VLV_REQ_OID, request[j]->oid) == 0)
					break;
			}
			if (! request[j]) {
				fprintf(stderr, "Warning VLV reply received but no request have been made\n");
				continue;
			}

			/* check the result */
			if (rep_control->vlv_result != 0) {
				fprintf(stderr, "Warning: VLV not performed with error: %d\n", rep_control->vlv_result);
			} else {
				fprintf(stderr, "VLV Info: target position = %d, content count = %d\n", rep_control->targetPosition, rep_control->contentCount);
			}

			continue;
		}

		if (strcmp(LDB_CONTROL_ASQ_OID, reply[i]->oid) == 0) {
			struct ldb_asq_control *rep_control;

			rep_control = talloc_get_type(reply[i]->data, struct ldb_asq_control);

			/* check the result */
			if (rep_control->result != 0) {
				fprintf(stderr, "Warning: ASQ not performed with error: %d\n", rep_control->result);
			}

			continue;
		}

		if (strcmp(LDB_CONTROL_PAGED_RESULTS_OID, reply[i]->oid) == 0) {
			struct ldb_paged_control *rep_control, *req_control;

			rep_control = talloc_get_type(reply[i]->data, struct ldb_paged_control);
			if (rep_control->cookie_len == 0) /* we are done */
				break;

			/* more processing required */
			/* let's fill in the request control with the new cookie */

			for (j = 0; request[j]; j++) {
				if (strcmp(LDB_CONTROL_PAGED_RESULTS_OID, request[j]->oid) == 0)
					break;
			}
			/* if there's a reply control we must find a request
			 * control matching it */
			if (! request[j]) return -1;

			req_control = talloc_get_type(request[j]->data, struct ldb_paged_control);

			if (req_control->cookie)
				talloc_free(req_control->cookie);
			req_control->cookie = (char *)talloc_memdup(
				req_control, rep_control->cookie,
				rep_control->cookie_len);
			req_control->cookie_len = rep_control->cookie_len;

			ret = 1;

			continue;
		}

		if (strcmp(LDB_CONTROL_SORT_RESP_OID, reply[i]->oid) == 0) {
			struct ldb_sort_resp_control *rep_control;

			rep_control = talloc_get_type(reply[i]->data, struct ldb_sort_resp_control);

			/* check we have a matching control in the request */
			for (j = 0; request[j]; j++) {
				if (strcmp(LDB_CONTROL_SERVER_SORT_OID, request[j]->oid) == 0)
					break;
			}
			if (! request[j]) {
				fprintf(stderr, "Warning Server Sort reply received but no request found\n");
				continue;
			}

			/* check the result */
			if (rep_control->result != 0) {
				fprintf(stderr, "Warning: Sorting not performed with error: %d\n", rep_control->result);
			}

			continue;
		}

		if (strcmp(LDB_CONTROL_DIRSYNC_OID, reply[i]->oid) == 0) {
			struct ldb_dirsync_control *rep_control, *req_control;
			char *cookie;

			rep_control = talloc_get_type(reply[i]->data, struct ldb_dirsync_control);
			if (rep_control->cookie_len == 0) /* we are done */
				break;

			/* more processing required */
			/* let's fill in the request control with the new cookie */

			for (j = 0; request[j]; j++) {
				if (strcmp(LDB_CONTROL_DIRSYNC_OID, request[j]->oid) == 0)
					break;
			}
			/* if there's a reply control we must find a request
			 * control matching it */
			if (! request[j]) return -1;

			req_control = talloc_get_type(request[j]->data, struct ldb_dirsync_control);

			if (req_control->cookie)
				talloc_free(req_control->cookie);
			req_control->cookie = (char *)talloc_memdup(
				req_control, rep_control->cookie,
				rep_control->cookie_len);
			req_control->cookie_len = rep_control->cookie_len;

			cookie = ldb_base64_encode(req_control, rep_control->cookie, rep_control->cookie_len);
			printf("# DIRSYNC cookie returned was:\n# %s\n", cookie);

			continue;
		}

		/* no controls matched, throw a warning */
		fprintf(stderr, "Unknown reply control oid: %s\n", reply[i]->oid);
	}

	return ret;
}


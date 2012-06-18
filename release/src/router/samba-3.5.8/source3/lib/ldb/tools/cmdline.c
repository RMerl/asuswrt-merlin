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

#include "includes.h"
#include "ldb/include/includes.h"
#include "ldb/tools/cmdline.h"

#if (_SAMBA_BUILD_ >= 4)
#include "lib/cmdline/popt_common.h"
#include "lib/ldb/samba/ldif_handlers.h"
#include "auth/gensec/gensec.h"
#include "auth/auth.h"
#include "db_wrap.h"
#endif



/*
  process command line options
*/
struct ldb_cmdline *ldb_cmdline_process(struct ldb_context *ldb, int argc, const char **argv,
					void (*usage)(void))
{
	static struct ldb_cmdline options; /* needs to be static for older compilers */
	struct ldb_cmdline *ret=NULL;
	poptContext pc;
#if (_SAMBA_BUILD_ >= 4)
	int r;
#endif
	int num_options = 0;
	int opt;
	int flags = 0;

	struct poptOption popt_options[] = {
		POPT_AUTOHELP
		{ "url",       'H', POPT_ARG_STRING, &options.url, 0, "database URL", "URL" },
		{ "basedn",    'b', POPT_ARG_STRING, &options.basedn, 0, "base DN", "DN" },
		{ "editor",    'e', POPT_ARG_STRING, &options.editor, 0, "external editor", "PROGRAM" },
		{ "scope",     's', POPT_ARG_STRING, NULL, 's', "search scope", "SCOPE" },
		{ "verbose",   'v', POPT_ARG_NONE, NULL, 'v', "increase verbosity", NULL },
		{ "interactive", 'i', POPT_ARG_NONE, &options.interactive, 0, "input from stdin", NULL },
		{ "recursive", 'r', POPT_ARG_NONE, &options.recursive, 0, "recursive delete", NULL },
		{ "num-searches", 0, POPT_ARG_INT, &options.num_searches, 0, "number of test searches", NULL },
		{ "num-records", 0, POPT_ARG_INT, &options.num_records, 0, "number of test records", NULL },
		{ "all", 'a',    POPT_ARG_NONE, &options.all_records, 0, "(|(objectClass=*)(distinguishedName=*))", NULL },
		{ "nosync", 0,   POPT_ARG_NONE, &options.nosync, 0, "non-synchronous transactions", NULL },
		{ "sorted", 'S', POPT_ARG_NONE, &options.sorted, 0, "sort attributes", NULL },
		{ "sasl-mechanism", 0, POPT_ARG_STRING, &options.sasl_mechanism, 0, "choose SASL mechanism", "MECHANISM" },
		{ "input", 'I', POPT_ARG_STRING, &options.input, 0, "Input File", "Input" },
		{ "output", 'O', POPT_ARG_STRING, &options.output, 0, "Output File", "Output" },
		{ NULL,    'o', POPT_ARG_STRING, NULL, 'o', "ldb_connect option", "OPTION" },
		{ "controls", 0, POPT_ARG_STRING, NULL, 'c', "controls", NULL },
#if (_SAMBA_BUILD_ >= 4)
		POPT_COMMON_SAMBA
		POPT_COMMON_CREDENTIALS
		POPT_COMMON_VERSION
#endif
		{ NULL }
	};

	ldb_global_init();

#if (_SAMBA_BUILD_ >= 4)
	r = ldb_register_samba_handlers(ldb);
	if (r != 0) {
		goto failed;
	}

#endif

	ret = talloc_zero(ldb, struct ldb_cmdline);
	if (ret == NULL) {
		ldb_oom(ldb);
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

	pc = poptGetContext(argv[0], argc, argv, popt_options, 
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
				ldb_oom(ldb);
				goto failed;
			}
			options.options[num_options] = poptGetOptArg(pc);
			options.options[num_options+1] = NULL;
			num_options++;
			break;

		case 'c': {
			const char *cs = poptGetOptArg(pc);
			const char *p, *q;
			int cc;

			for (p = cs, cc = 1; (q = strchr(p, ',')); cc++, p = q + 1) ;

			options.controls = talloc_array(ret, char *, cc + 1);
			if (options.controls == NULL) {
				ldb_oom(ldb);
				goto failed;
			}
			for (p = cs, cc = 0; p != NULL; cc++) {
				const char *t;

				t = strchr(p, ',');
				if (t == NULL) {
					options.controls[cc] = talloc_strdup(options.controls, p);
					p = NULL;
				} else {
					options.controls[cc] = talloc_strndup(options.controls, p, t-p);
			        	p = t + 1;
				}
			}
			options.controls[cc] = NULL;

			break;	  
		}
		default:
			fprintf(stderr, "Invalid option %s: %s\n", 
				poptBadOption(pc, 0), poptStrerror(opt));
			if (usage) usage();
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
		if (usage) usage();
		goto failed;
	}

	if (strcmp(ret->url, "NONE") == 0) {
		return ret;
	}

	if (options.nosync) {
		flags |= LDB_FLG_NOSYNC;
	}

#if (_SAMBA_BUILD_ >= 4)
	/* Must be after we have processed command line options */
	gensec_init(); 
	
	if (ldb_set_opaque(ldb, "sessionInfo", system_session(ldb))) {
		goto failed;
	}
	if (ldb_set_opaque(ldb, "credentials", cmdline_credentials)) {
		goto failed;
	}
	ldb_set_utf8_fns(ldb, NULL, wrap_casefold);
#endif

	/* now connect to the ldb */
	if (ldb_connect(ldb, ret->url, flags, ret->options) != 0) {
		fprintf(stderr, "Failed to connect to %s - %s\n", 
			ret->url, ldb_errstring(ldb));
		goto failed;
	}

	return ret;

failed:
	talloc_free(ret);
	exit(1);
	return NULL;
}

struct ldb_control **parse_controls(void *mem_ctx, char **control_strings)
{
	int i;
	struct ldb_control **ctrl;

	if (control_strings == NULL || control_strings[0] == NULL)
		return NULL;

	for (i = 0; control_strings[i]; i++);

	ctrl = talloc_array(mem_ctx, struct ldb_control *, i + 1);

	for (i = 0; control_strings[i]; i++) {
		if (strncmp(control_strings[i], "vlv:", 4) == 0) {
			struct ldb_vlv_req_control *control;
			const char *p;
			char attr[1024];
			char ctxid[1024];
			int crit, bc, ac, os, cc, ret;

			attr[0] = '\0';
			ctxid[0] = '\0';
			p = &(control_strings[i][4]);
			ret = sscanf(p, "%d:%d:%d:%d:%d:%1023[^$]", &crit, &bc, &ac, &os, &cc, ctxid);
			if (ret < 5) {
				ret = sscanf(p, "%d:%d:%d:%1023[^:]:%1023[^$]", &crit, &bc, &ac, attr, ctxid);
			}
			       
			if ((ret < 4) || (crit < 0) || (crit > 1)) {
				fprintf(stderr, "invalid server_sort control syntax\n");
				fprintf(stderr, " syntax: crit(b):bc(n):ac(n):<os(n):cc(n)|attr(s)>[:ctxid(o)]\n");
				fprintf(stderr, "   note: b = boolean, n = number, s = string, o = b64 binary blob\n");
				return NULL;
			}
			if (!(ctrl[i] = talloc(ctrl, struct ldb_control))) {
				fprintf(stderr, "talloc failed\n");
				return NULL;
			}
			ctrl[i]->oid = LDB_CONTROL_VLV_REQ_OID;
			ctrl[i]->critical = crit;
			if (!(control = talloc(ctrl[i],
					       struct ldb_vlv_req_control))) {
				fprintf(stderr, "talloc failed\n");
				return NULL;
			}
			control->beforeCount = bc;
			control->afterCount = ac;
			if (attr[0]) {
				control->type = 1;
				control->match.gtOrEq.value = talloc_strdup(control, attr);
				control->match.gtOrEq.value_len = strlen(attr);
			} else {
				control->type = 0;
				control->match.byOffset.offset = os;
				control->match.byOffset.contentCount = cc;
			}
			if (ctxid[0]) {
				control->ctxid_len = ldb_base64_decode(ctxid);
				control->contextId = (char *)talloc_memdup(control, ctxid, control->ctxid_len);
			} else {
				control->ctxid_len = 0;
				control->contextId = NULL;
			}
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "dirsync:", 8) == 0) {
			struct ldb_dirsync_control *control;
			const char *p;
			char cookie[1024];
			int crit, flags, max_attrs, ret;
		       
			cookie[0] = '\0';
			p = &(control_strings[i][8]);
			ret = sscanf(p, "%d:%d:%d:%1023[^$]", &crit, &flags, &max_attrs, cookie);

			if ((ret < 3) || (crit < 0) || (crit > 1) || (flags < 0) || (max_attrs < 0)) {
				fprintf(stderr, "invalid dirsync control syntax\n");
				fprintf(stderr, " syntax: crit(b):flags(n):max_attrs(n)[:cookie(o)]\n");
				fprintf(stderr, "   note: b = boolean, n = number, o = b64 binary blob\n");
				return NULL;
			}

			/* w2k3 seems to ignore the parameter,
			 * but w2k sends a wrong cookie when this value is to small
			 * this would cause looping forever, while getting
			 * the same data and same cookie forever
			 */
			if (max_attrs == 0) max_attrs = 0x0FFFFFFF;

			ctrl[i] = talloc(ctrl, struct ldb_control);
			ctrl[i]->oid = LDB_CONTROL_DIRSYNC_OID;
			ctrl[i]->critical = crit;
			control = talloc(ctrl[i], struct ldb_dirsync_control);
			control->flags = flags;
			control->max_attributes = max_attrs;
			if (*cookie) {
				control->cookie_len = ldb_base64_decode(cookie);
				control->cookie = (char *)talloc_memdup(control, cookie, control->cookie_len);
			} else {
				control->cookie = NULL;
				control->cookie_len = 0;
			}
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "asq:", 4) == 0) {
			struct ldb_asq_control *control;
			const char *p;
			char attr[256];
			int crit, ret;

			attr[0] = '\0';
			p = &(control_strings[i][4]);
			ret = sscanf(p, "%d:%255[^$]", &crit, attr);
			if ((ret != 2) || (crit < 0) || (crit > 1) || (attr[0] == '\0')) {
				fprintf(stderr, "invalid asq control syntax\n");
				fprintf(stderr, " syntax: crit(b):attr(s)\n");
				fprintf(stderr, "   note: b = boolean, s = string\n");
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			ctrl[i]->oid = LDB_CONTROL_ASQ_OID;
			ctrl[i]->critical = crit;
			control = talloc(ctrl[i], struct ldb_asq_control);
			control->request = 1;
			control->source_attribute = talloc_strdup(control, attr);
			control->src_attr_len = strlen(attr);
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "extended_dn:", 12) == 0) {
			struct ldb_extended_dn_control *control;
			const char *p;
			int crit, type, ret;

			p = &(control_strings[i][12]);
			ret = sscanf(p, "%d:%d", &crit, &type);
			if ((ret != 2) || (crit < 0) || (crit > 1) || (type < 0) || (type > 1)) {
				fprintf(stderr, "invalid extended_dn control syntax\n");
				fprintf(stderr, " syntax: crit(b):type(b)\n");
				fprintf(stderr, "   note: b = boolean\n");
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			ctrl[i]->oid = LDB_CONTROL_EXTENDED_DN_OID;
			ctrl[i]->critical = crit;
			control = talloc(ctrl[i], struct ldb_extended_dn_control);
			control->type = type;
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "sd_flags:", 9) == 0) {
			struct ldb_sd_flags_control *control;
			const char *p;
			int crit, ret;
			unsigned secinfo_flags;

			p = &(control_strings[i][9]);
			ret = sscanf(p, "%d:%u", &crit, &secinfo_flags);
			if ((ret != 2) || (crit < 0) || (crit > 1) || (secinfo_flags < 0) || (secinfo_flags > 0xF)) {
				fprintf(stderr, "invalid sd_flags control syntax\n");
				fprintf(stderr, " syntax: crit(b):secinfo_flags(n)\n");
				fprintf(stderr, "   note: b = boolean, n = number\n");
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			ctrl[i]->oid = LDB_CONTROL_SD_FLAGS_OID;
			ctrl[i]->critical = crit;
			control = talloc(ctrl[i], struct ldb_sd_flags_control);
			control->secinfo_flags = secinfo_flags;
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "search_options:", 15) == 0) {
			struct ldb_search_options_control *control;
			const char *p;
			int crit, ret;
			unsigned search_options;

			p = &(control_strings[i][15]);
			ret = sscanf(p, "%d:%u", &crit, &search_options);
			if ((ret != 2) || (crit < 0) || (crit > 1) || (search_options < 0) || (search_options > 0xF)) {
				fprintf(stderr, "invalid search_options control syntax\n");
				fprintf(stderr, " syntax: crit(b):search_options(n)\n");
				fprintf(stderr, "   note: b = boolean, n = number\n");
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			ctrl[i]->oid = LDB_CONTROL_SEARCH_OPTIONS_OID;
			ctrl[i]->critical = crit;
			control = talloc(ctrl[i], struct ldb_search_options_control);
			control->search_options = search_options;
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "domain_scope:", 13) == 0) {
			const char *p;
			int crit, ret;

			p = &(control_strings[i][13]);
			ret = sscanf(p, "%d", &crit);
			if ((ret != 1) || (crit < 0) || (crit > 1)) {
				fprintf(stderr, "invalid domain_scope control syntax\n");
				fprintf(stderr, " syntax: crit(b)\n");
				fprintf(stderr, "   note: b = boolean\n");
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			ctrl[i]->oid = LDB_CONTROL_DOMAIN_SCOPE_OID;
			ctrl[i]->critical = crit;
			ctrl[i]->data = NULL;

			continue;
		}

		if (strncmp(control_strings[i], "paged_results:", 14) == 0) {
			struct ldb_paged_control *control;
			const char *p;
			int crit, size, ret;
		       
			p = &(control_strings[i][14]);
			ret = sscanf(p, "%d:%d", &crit, &size);

			if ((ret != 2) || (crit < 0) || (crit > 1) || (size < 0)) {
				fprintf(stderr, "invalid paged_results control syntax\n");
				fprintf(stderr, " syntax: crit(b):size(n)\n");
				fprintf(stderr, "   note: b = boolean, n = number\n");
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			ctrl[i]->oid = LDB_CONTROL_PAGED_RESULTS_OID;
			ctrl[i]->critical = crit;
			control = talloc(ctrl[i], struct ldb_paged_control);
			control->size = size;
			control->cookie = NULL;
			control->cookie_len = 0;
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "server_sort:", 12) == 0) {
			struct ldb_server_sort_control **control;
			const char *p;
			char attr[256];
			char rule[128];
			int crit, rev, ret;

			attr[0] = '\0';
			rule[0] = '\0';
			p = &(control_strings[i][12]);
			ret = sscanf(p, "%d:%d:%255[^:]:%127[^:]", &crit, &rev, attr, rule);
			if ((ret < 3) || (crit < 0) || (crit > 1) || (rev < 0 ) || (rev > 1) ||attr[0] == '\0') {
				fprintf(stderr, "invalid server_sort control syntax\n");
				fprintf(stderr, " syntax: crit(b):rev(b):attr(s)[:rule(s)]\n");
				fprintf(stderr, "   note: b = boolean, s = string\n");
				return NULL;
			}
			ctrl[i] = talloc(ctrl, struct ldb_control);
			ctrl[i]->oid = LDB_CONTROL_SERVER_SORT_OID;
			ctrl[i]->critical = crit;
			control = talloc_array(ctrl[i], struct ldb_server_sort_control *, 2);
			control[0] = talloc(control, struct ldb_server_sort_control);
			control[0]->attributeName = talloc_strdup(control, attr);
			if (rule[0])
				control[0]->orderingRule = talloc_strdup(control, rule);
			else
				control[0]->orderingRule = NULL;
			control[0]->reverse = rev;
			control[1] = NULL;
			ctrl[i]->data = control;

			continue;
		}

		if (strncmp(control_strings[i], "notification:", 13) == 0) {
			const char *p;
			int crit, ret;

			p = &(control_strings[i][13]);
			ret = sscanf(p, "%d", &crit);
			if ((ret != 1) || (crit < 0) || (crit > 1)) {
				fprintf(stderr, "invalid notification control syntax\n");
				fprintf(stderr, " syntax: crit(b)\n");
				fprintf(stderr, "   note: b = boolean\n");
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			ctrl[i]->oid = LDB_CONTROL_NOTIFICATION_OID;
			ctrl[i]->critical = crit;
			ctrl[i]->data = NULL;

			continue;
		}

		if (strncmp(control_strings[i], "show_deleted:", 13) == 0) {
			const char *p;
			int crit, ret;

			p = &(control_strings[i][13]);
			ret = sscanf(p, "%d", &crit);
			if ((ret != 1) || (crit < 0) || (crit > 1)) {
				fprintf(stderr, "invalid show_deleted control syntax\n");
				fprintf(stderr, " syntax: crit(b)\n");
				fprintf(stderr, "   note: b = boolean\n");
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			ctrl[i]->oid = LDB_CONTROL_SHOW_DELETED_OID;
			ctrl[i]->critical = crit;
			ctrl[i]->data = NULL;

			continue;
		}

		if (strncmp(control_strings[i], "permissive_modify:", 18) == 0) {
			const char *p;
			int crit, ret;

			p = &(control_strings[i][18]);
			ret = sscanf(p, "%d", &crit);
			if ((ret != 1) || (crit < 0) || (crit > 1)) {
				fprintf(stderr, "invalid permissive_modify control syntax\n");
				fprintf(stderr, " syntax: crit(b)\n");
				fprintf(stderr, "   note: b = boolean\n");
				return NULL;
			}

			ctrl[i] = talloc(ctrl, struct ldb_control);
			ctrl[i]->oid = LDB_CONTROL_PERMISSIVE_MODIFY_OID;
			ctrl[i]->critical = crit;
			ctrl[i]->data = NULL;

			continue;
		}

		/* no controls matched, throw an error */
		fprintf(stderr, "Invalid control name: '%s'\n", control_strings[i]);
		return NULL;
	}

	ctrl[i] = NULL;

	return ctrl;
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
	int i, j;
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


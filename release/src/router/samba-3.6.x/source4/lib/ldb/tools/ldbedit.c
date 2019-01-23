/*
   ldb database library

   Copyright (C) Andrew Tridgell  2004

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

/*
 *  Name: ldb
 *
 *  Component: ldbedit
 *
 *  Description: utility for ldb database editing
 *
 *  Author: Andrew Tridgell
 */

#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "system/filesys.h"
#include "ldb.h"
#include "tools/cmdline.h"
#include "tools/ldbutil.h"

static struct ldb_cmdline *options;

/*
  debug routine
*/
static void ldif_write_msg(struct ldb_context *ldb,
			   FILE *f,
			   enum ldb_changetype changetype,
			   struct ldb_message *msg)
{
	struct ldb_ldif ldif;
	ldif.changetype = changetype;
	ldif.msg = msg;
	ldb_ldif_write_file(ldb, f, &ldif);
}

/*
  modify a database record so msg1 becomes msg2
  returns the number of modified elements
*/
static int modify_record(struct ldb_context *ldb,
			 struct ldb_message *msg1,
			 struct ldb_message *msg2,
			 struct ldb_control **req_ctrls)
{
	int ret;
	struct ldb_message *mod;

	if (ldb_msg_difference(ldb, ldb, msg1, msg2, &mod) != LDB_SUCCESS) {
		fprintf(stderr, "Failed to calculate message differences\n");
		return -1;
	}

	ret = mod->num_elements;
	if (ret == 0) {
		goto done;
	}

	if (options->verbose > 0) {
		ldif_write_msg(ldb, stdout, LDB_CHANGETYPE_MODIFY, mod);
	}

	if (ldb_modify_ctrl(ldb, mod, req_ctrls) != LDB_SUCCESS) {
		fprintf(stderr, "failed to modify %s - %s\n",
			ldb_dn_get_linearized(msg1->dn), ldb_errstring(ldb));
		ret = -1;
		goto done;
	}

done:
	talloc_free(mod);
	return ret;
}

/*
  find dn in msgs[]
*/
static struct ldb_message *msg_find(struct ldb_context *ldb,
				    struct ldb_message **msgs,
				    unsigned int count,
				    struct ldb_dn *dn)
{
	unsigned int i;
	for (i=0;i<count;i++) {
		if (ldb_dn_compare(dn, msgs[i]->dn) == 0) {
			return msgs[i];
		}
	}
	return NULL;
}

/*
  merge the changes in msgs2 into the messages from msgs1
*/
static int merge_edits(struct ldb_context *ldb,
		       struct ldb_message **msgs1, unsigned int count1,
		       struct ldb_message **msgs2, unsigned int count2)
{
	unsigned int i;
	struct ldb_message *msg;
	int ret;
	unsigned int adds=0, modifies=0, deletes=0;
	struct ldb_control **req_ctrls = ldb_parse_control_strings(ldb, ldb, (const char **)options->controls);
	if (options->controls != NULL && req_ctrls == NULL) {
		fprintf(stderr, "parsing controls failed: %s\n", ldb_errstring(ldb));
		return -1;
	}

	if (ldb_transaction_start(ldb) != LDB_SUCCESS) {
		fprintf(stderr, "Failed to start transaction: %s\n", ldb_errstring(ldb));
		return -1;
	}

	/* do the adds and modifies */
	for (i=0;i<count2;i++) {
		msg = msg_find(ldb, msgs1, count1, msgs2[i]->dn);
		if (!msg) {
			if (options->verbose > 0) {
				ldif_write_msg(ldb, stdout, LDB_CHANGETYPE_ADD, msgs2[i]);
			}
			if (ldb_add_ctrl(ldb, msgs2[i], req_ctrls) != LDB_SUCCESS) {
				fprintf(stderr, "failed to add %s - %s\n",
					ldb_dn_get_linearized(msgs2[i]->dn),
					ldb_errstring(ldb));
				ldb_transaction_cancel(ldb);
				return -1;
			}
			adds++;
		} else {
			ret = modify_record(ldb, msg, msgs2[i], req_ctrls);
			if (ret != -1) {
				modifies += (unsigned int) ret;
			} else {
				return -1;
			}
		}
	}

	/* do the deletes */
	for (i=0;i<count1;i++) {
		msg = msg_find(ldb, msgs2, count2, msgs1[i]->dn);
		if (!msg) {
			if (options->verbose > 0) {
				ldif_write_msg(ldb, stdout, LDB_CHANGETYPE_DELETE, msgs1[i]);
			}
			if (ldb_delete_ctrl(ldb, msgs1[i]->dn, req_ctrls) != LDB_SUCCESS) {
				fprintf(stderr, "failed to delete %s - %s\n",
					ldb_dn_get_linearized(msgs1[i]->dn),
					ldb_errstring(ldb));
				ldb_transaction_cancel(ldb);
				return -1;
			}
			deletes++;
		}
	}

	if (ldb_transaction_commit(ldb) != LDB_SUCCESS) {
		fprintf(stderr, "Failed to commit transaction: %s\n", ldb_errstring(ldb));
		return -1;
	}

	printf("# %u adds  %u modifies  %u deletes\n", adds, modifies, deletes);

	return 0;
}

/*
  save a set of messages as ldif to a file
*/
static int save_ldif(struct ldb_context *ldb,
		     FILE *f, struct ldb_message **msgs, unsigned int count)
{
	unsigned int i;

	fprintf(f, "# editing %u records\n", count);

	for (i=0;i<count;i++) {
		struct ldb_ldif ldif;
		fprintf(f, "# record %u\n", i+1);

		ldif.changetype = LDB_CHANGETYPE_NONE;
		ldif.msg = msgs[i];

		ldb_ldif_write_file(ldb, f, &ldif);
	}

	return 0;
}


/*
  edit the ldb search results in msgs using the user selected editor
*/
static int do_edit(struct ldb_context *ldb, struct ldb_message **msgs1,
		   unsigned int count1, const char *editor)
{
	int fd, ret;
	FILE *f;
	char file_template[] = "/tmp/ldbedit.XXXXXX";
	char *cmd;
	struct ldb_ldif *ldif;
	struct ldb_message **msgs2 = NULL;
	unsigned int count2 = 0;

	/* write out the original set of messages to a temporary
	   file */
	fd = mkstemp(file_template);

	if (fd == -1) {
		perror(file_template);
		return -1;
	}

	f = fdopen(fd, "r+");

	if (!f) {
		perror("fopen");
		close(fd);
		unlink(file_template);
		return -1;
	}

	if (save_ldif(ldb, f, msgs1, count1) != 0) {
		return -1;
	}

	fclose(f);

	cmd = talloc_asprintf(ldb, "%s %s", editor, file_template);

	if (!cmd) {
		unlink(file_template);
		fprintf(stderr, "out of memory\n");
		return -1;
	}

	/* run the editor */
	ret = system(cmd);
	talloc_free(cmd);

	if (ret != 0) {
		unlink(file_template);
		fprintf(stderr, "edit with %s failed\n", editor);
		return -1;
	}

	/* read the resulting ldif into msgs2 */
	f = fopen(file_template, "r");
	if (!f) {
		perror(file_template);
		return -1;
	}

	while ((ldif = ldb_ldif_read_file(ldb, f))) {
		msgs2 = talloc_realloc(ldb, msgs2, struct ldb_message *, count2+1);
		if (!msgs2) {
			fprintf(stderr, "out of memory");
			return -1;
		}
		msgs2[count2++] = ldif->msg;
	}

	fclose(f);
	unlink(file_template);

	return merge_edits(ldb, msgs1, count1, msgs2, count2);
}

static void usage(struct ldb_context *ldb)
{
	printf("Usage: ldbedit <options> <expression> <attributes ...>\n");
	ldb_cmdline_help(ldb, "ldbedit", stdout);
	exit(LDB_ERR_OPERATIONS_ERROR);
}

int main(int argc, const char **argv)
{
	struct ldb_context *ldb;
	struct ldb_result *result = NULL;
	struct ldb_dn *basedn = NULL;
	int ret;
	const char *expression = "(|(objectClass=*)(distinguishedName=*))";
	const char * const * attrs = NULL;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	struct ldb_control **req_ctrls;

	ldb = ldb_init(mem_ctx, NULL);
	if (ldb == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	options = ldb_cmdline_process(ldb, argc, argv, usage);

	/* the check for '=' is for compatibility with ldapsearch */
	if (options->argc > 0 &&
	    strchr(options->argv[0], '=')) {
		expression = options->argv[0];
		options->argv++;
		options->argc--;
	}

	if (options->argc > 0) {
		attrs = (const char * const *)(options->argv);
	}

	if (options->basedn != NULL) {
		basedn = ldb_dn_new(ldb, ldb, options->basedn);
		if (basedn == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	req_ctrls = ldb_parse_control_strings(ldb, ldb, (const char **)options->controls);
	if (options->controls != NULL &&  req_ctrls== NULL) {
		printf("parsing controls failed: %s\n", ldb_errstring(ldb));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_search_ctrl(ldb, ldb, &result, basedn, options->scope, attrs, req_ctrls, "%s", expression);
	if (ret != LDB_SUCCESS) {
		printf("search failed - %s\n", ldb_errstring(ldb));
		return ret;
	}

	if (result->count == 0) {
		printf("no matching records - cannot edit\n");
		talloc_free(mem_ctx);
		return LDB_SUCCESS;
	}

	ret = do_edit(ldb, result->msgs, result->count, options->editor);

	talloc_free(mem_ctx);

	return ret == 0 ? LDB_SUCCESS : LDB_ERR_OPERATIONS_ERROR;
}

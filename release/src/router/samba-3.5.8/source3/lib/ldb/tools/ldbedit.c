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

#include "includes.h"
#include "ldb/include/includes.h"
#include "ldb/tools/cmdline.h"

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
			 struct ldb_message *msg2)
{
	struct ldb_message *mod;

	mod = ldb_msg_diff(ldb, msg1, msg2);
	if (mod == NULL) {
		fprintf(stderr, "Failed to calculate message differences\n");
		return -1;
	}

	if (mod->num_elements == 0) {
		return 0;
	}

	if (options->verbose > 0) {
		ldif_write_msg(ldb, stdout, LDB_CHANGETYPE_MODIFY, mod);
	}

	if (ldb_modify(ldb, mod) != 0) {
		fprintf(stderr, "failed to modify %s - %s\n", 
			ldb_dn_linearize(ldb, msg1->dn), ldb_errstring(ldb));
		return -1;
	}

	return mod->num_elements;
}

/*
  find dn in msgs[]
*/
static struct ldb_message *msg_find(struct ldb_context *ldb,
				    struct ldb_message **msgs,
				    int count,
				    const struct ldb_dn *dn)
{
	int i;
	for (i=0;i<count;i++) {
		if (ldb_dn_compare(ldb, dn, msgs[i]->dn) == 0) {
			return msgs[i];
		}
	}
	return NULL;
}

/*
  merge the changes in msgs2 into the messages from msgs1
*/
static int merge_edits(struct ldb_context *ldb,
		       struct ldb_message **msgs1, int count1,
		       struct ldb_message **msgs2, int count2)
{
	int i;
	struct ldb_message *msg;
	int ret = 0;
	int adds=0, modifies=0, deletes=0;

	/* do the adds and modifies */
	for (i=0;i<count2;i++) {
		msg = msg_find(ldb, msgs1, count1, msgs2[i]->dn);
		if (!msg) {
			if (options->verbose > 0) {
				ldif_write_msg(ldb, stdout, LDB_CHANGETYPE_ADD, msgs2[i]);
			}
			if (ldb_add(ldb, msgs2[i]) != 0) {
				fprintf(stderr, "failed to add %s - %s\n",
					ldb_dn_linearize(ldb, msgs2[i]->dn),
					ldb_errstring(ldb));
				return -1;
			}
			adds++;
		} else {
			if (modify_record(ldb, msg, msgs2[i]) > 0) {
				modifies++;
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
			if (ldb_delete(ldb, msgs1[i]->dn) != 0) {
				fprintf(stderr, "failed to delete %s - %s\n",
					ldb_dn_linearize(ldb, msgs1[i]->dn),
					ldb_errstring(ldb));
				return -1;
			}
			deletes++;
		}
	}

	printf("# %d adds  %d modifies  %d deletes\n", adds, modifies, deletes);

	return ret;
}

/*
  save a set of messages as ldif to a file
*/
static int save_ldif(struct ldb_context *ldb, 
		     FILE *f, struct ldb_message **msgs, int count)
{
	int i;

	fprintf(f, "# editing %d records\n", count);

	for (i=0;i<count;i++) {
		struct ldb_ldif ldif;
		fprintf(f, "# record %d\n", i+1);

		ldif.changetype = LDB_CHANGETYPE_NONE;
		ldif.msg = msgs[i];

		ldb_ldif_write_file(ldb, f, &ldif);
	}

	return 0;
}


/*
  edit the ldb search results in msgs using the user selected editor
*/
static int do_edit(struct ldb_context *ldb, struct ldb_message **msgs1, int count1,
		   const char *editor)
{
	int fd, ret;
	FILE *f;
	char file_template[] = "/tmp/ldbedit.XXXXXX";
	char *cmd;
	struct ldb_ldif *ldif;
	struct ldb_message **msgs2 = NULL;
	int count2 = 0;

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

static void usage(void)
{
	printf("Usage: ldbedit <options> <expression> <attributes ...>\n");
	printf("Options:\n");
	printf("  -H ldb_url       choose the database (or $LDB_URL)\n");
	printf("  -s base|sub|one  choose search scope\n");
	printf("  -b basedn        choose baseDN\n");
	printf("  -a               edit all records (expression 'objectclass=*')\n");
	printf("  -e editor        choose editor (or $VISUAL or $EDITOR)\n");
	printf("  -v               verbose mode\n");
	exit(1);
}

int main(int argc, const char **argv)
{
	struct ldb_context *ldb;
	struct ldb_result *result = NULL;
	struct ldb_dn *basedn = NULL;
	int ret;
	const char *expression = "(|(objectClass=*)(distinguishedName=*))";
	const char * const * attrs = NULL;

	ldb_global_init();

	ldb = ldb_init(NULL, NULL);

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
		basedn = ldb_dn_explode(ldb, options->basedn);
		if (basedn == NULL) {
			printf("Invalid Base DN format\n");
			exit(1);
		}
	}

	ret = ldb_search(ldb, ldb, &result, basedn, options->scope, attrs, "%s", expression);
	if (ret != LDB_SUCCESS) {
		printf("search failed - %s\n", ldb_errstring(ldb));
		exit(1);
	}

	if (result->count == 0) {
		printf("no matching records - cannot edit\n");
		return 0;
	}

	do_edit(ldb, result->msgs, result->count, options->editor);

	ret = talloc_free(result);
	if (ret == -1) {
		fprintf(stderr, "talloc_free failed\n");
		exit(1);
	}

	talloc_free(ldb);
	return 0;
}

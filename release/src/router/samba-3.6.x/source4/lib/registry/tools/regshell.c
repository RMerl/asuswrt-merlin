/*
   Unix SMB/CIFS implementation.
   simple registry frontend

   Copyright (C) Jelmer Vernooij 2004-2007
   Copyright (C) Wilco Baan Hofman 2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "lib/registry/registry.h"
#include "lib/cmdline/popt_common.h"
#include "lib/events/events.h"
#include "system/time.h"
#include "../libcli/smbreadline/smbreadline.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "lib/registry/tools/common.h"
#include "param/param.h"

struct regshell_context {
	struct registry_context *registry;
	char *path;
	char *predef;
	struct registry_key *current;
	struct registry_key *root;
};

static WERROR get_full_path(struct regshell_context *ctx, char *path, char **ret_path) 
{
	char *dir;
	char *tmp;
	char *new_path;

	if (path[0] == '\\') {
		new_path = talloc_strdup(ctx, "");
	} else {
 		new_path = talloc_strdup(ctx, ctx->path);
	}		

	dir = strtok(path, "\\");
	if (dir == NULL) {
		*ret_path = new_path;
		return WERR_OK;
	}
	do {
		if (strcmp(dir, "..") == 0) {
			if (strchr(new_path, '\\')) {
				new_path[strrchr(new_path, '\\') - new_path] = '\0';
			} else {
				tmp = new_path;
				new_path = talloc_strdup(ctx, "");
				talloc_free(tmp);
			}
			continue;
		} 
		if (strcmp(dir, ".") == 0) {
			continue;
		}

		tmp = new_path;
		/* No prepending a backslash */
		if (strcmp(new_path, "") == 0) {
			new_path = talloc_strdup(ctx, dir);
		} else {
			new_path = talloc_asprintf(ctx, "%s\\%s", new_path, dir);
		}
		talloc_free(tmp);
		
	} while ((dir = strtok(NULL, "\\")));

	*ret_path = new_path;
	return WERR_OK;
}

/* *
 * ck/cd - change key
 * ls - list values/keys
 * rmval/rm - remove value
 * rmkey/rmdir - remove key
 * mkkey/mkdir - make key
 * ch - change hive
 * info - show key info
 * save - save hive
 * print - print value
 * help
 * exit
 */

static WERROR cmd_info(struct regshell_context *ctx, int argc, char **argv)
{
	struct security_descriptor *sec_desc = NULL;
	time_t last_mod;
	WERROR error;
	const char *classname = NULL;
	NTTIME last_change;
	uint32_t max_subkeynamelen;
	uint32_t max_valnamelen;
	uint32_t max_valbufsize;
	uint32_t num_subkeys;
	uint32_t num_values;

	error = reg_key_get_info(ctx, ctx->current, &classname, &num_subkeys, &num_values,
				 &last_change, &max_subkeynamelen, &max_valnamelen, &max_valbufsize);
	if (!W_ERROR_IS_OK(error)) {
		printf("Error getting key info: %s\n", win_errstr(error));
		return error;
	}


	printf("Name: %s\n", strchr(ctx->path, '\\')?strrchr(ctx->path, '\\')+1:
		   ctx->path);
	printf("Full path: %s\n", ctx->path);
	if (classname != NULL)
		printf("Key Class: %s\n", classname);
	last_mod = nt_time_to_unix(last_change);
	printf("Time Last Modified: %s", ctime(&last_mod));
	printf("Number of subkeys: %d\n", num_subkeys);
	printf("Number of values: %d\n", num_values);

	if (max_valnamelen > 0)
		printf("Maximum value name length: %d\n", max_valnamelen);

	if (max_valbufsize > 0)
		printf("Maximum value data length: %d\n", max_valbufsize);

	if (max_subkeynamelen > 0)
		printf("Maximum sub key name length: %d\n", max_subkeynamelen);

	error = reg_get_sec_desc(ctx, ctx->current, &sec_desc);
	if (!W_ERROR_IS_OK(error)) {
		printf("Error getting security descriptor: %s\n", win_errstr(error));
		return WERR_OK;
	}
	ndr_print_debug((ndr_print_fn_t)ndr_print_security_descriptor,
			"Security", sec_desc);
	talloc_free(sec_desc);

	return WERR_OK;
}

static WERROR cmd_predef(struct regshell_context *ctx, int argc, char **argv)
{
	struct registry_key *ret = NULL;
	if (argc < 2) {
		fprintf(stderr, "Usage: predef predefined-key-name\n");
	} else if (!ctx) {
		fprintf(stderr, "No full registry loaded, no predefined keys defined\n");
	} else {
		WERROR error = reg_get_predefined_key_by_name(ctx->registry,
							      argv[1], &ret);

		if (!W_ERROR_IS_OK(error)) {
			fprintf(stderr, "Error opening predefined key %s: %s\n",
				argv[1], win_errstr(error));
			return error;
		}

		ctx->predef = strupper_talloc(ctx, argv[1]);
		ctx->current = ret;
		ctx->root = ret;
	}

	return WERR_OK;
}

static WERROR cmd_pwd(struct regshell_context *ctx,
		      int argc, char **argv)
{
	if (ctx->predef) {
		printf("%s\\", ctx->predef);
	}
	printf("%s\n", ctx->path);
	return WERR_OK;
}

static WERROR cmd_set(struct regshell_context *ctx, int argc, char **argv)
{
	struct registry_value val;
	WERROR error;

	if (argc < 4) {
		fprintf(stderr, "Usage: set value-name type value\n");
		return WERR_INVALID_PARAM;
	}

	if (!reg_string_to_val(ctx, argv[2], argv[3], &val.data_type, &val.data)) {
		fprintf(stderr, "Unable to interpret data\n");
		return WERR_INVALID_PARAM;
	}

	error = reg_val_set(ctx->current, argv[1], val.data_type, val.data);
	if (!W_ERROR_IS_OK(error)) {
		fprintf(stderr, "Error setting value: %s\n", win_errstr(error));
		return error;
	}

	return WERR_OK;
}

static WERROR cmd_ck(struct regshell_context *ctx, int argc, char **argv)
{
	struct registry_key *nkey = NULL;
	char *full_path;
	WERROR error;

	if(argc == 2) {
		if (!W_ERROR_IS_OK(get_full_path(ctx, argv[1], &full_path))) {
			fprintf(stderr, "Unable to parse the supplied path\n");
			return WERR_INVALID_PARAM;
		}
		error = reg_open_key(ctx->registry, ctx->root, full_path,
				     &nkey);
		if(!W_ERROR_IS_OK(error)) {
			DEBUG(0, ("Error opening specified key: %s\n",
				win_errstr(error)));
			return error;
		}

		talloc_free(ctx->path);
		ctx->path = full_path;

		ctx->current = nkey;
	}
	printf("New path is: %s\\%s\n", ctx->predef?ctx->predef:"", ctx->path);

	return WERR_OK;
}

static WERROR cmd_print(struct regshell_context *ctx, int argc, char **argv)
{
	uint32_t value_type;
	DATA_BLOB value_data;
	WERROR error;

	if (argc != 2) {
		fprintf(stderr, "Usage: print <valuename>\n");
		return WERR_INVALID_PARAM;
	}

	error = reg_key_get_value_by_name(ctx, ctx->current, argv[1],
					  &value_type, &value_data);
	if (!W_ERROR_IS_OK(error)) {
		fprintf(stderr, "No such value '%s'\n", argv[1]);
		return error;
	}

	printf("%s\n%s\n", str_regtype(value_type),
		   reg_val_data_string(ctx, value_type, value_data));

	return WERR_OK;
}

static WERROR cmd_ls(struct regshell_context *ctx, int argc, char **argv)
{
	unsigned int i;
	WERROR error;
	uint32_t valuetype;
	DATA_BLOB valuedata;
	const char *name = NULL;

	for (i = 0; W_ERROR_IS_OK(error = reg_key_get_subkey_by_index(ctx,
								      ctx->current,
								      i,
								      &name,
								      NULL,
								      NULL)); i++) {
		printf("K %s\n", name);
	}

	if (!W_ERROR_EQUAL(error, WERR_NO_MORE_ITEMS)) {
		fprintf(stderr, "Error occurred while browsing through keys: %s\n",
			win_errstr(error));
		return error;
	}

	for (i = 0; W_ERROR_IS_OK(error = reg_key_get_value_by_index(ctx,
		ctx->current, i, &name, &valuetype, &valuedata)); i++)
		printf("V \"%s\" %s %s\n", name, str_regtype(valuetype),
			   reg_val_data_string(ctx, valuetype, valuedata));

	return WERR_OK;
}
static WERROR cmd_mkkey(struct regshell_context *ctx, int argc, char **argv)
{
	struct registry_key *tmp;
	WERROR error;

	if(argc < 2) {
		fprintf(stderr, "Usage: mkkey <keyname>\n");
		return WERR_INVALID_PARAM;
	}

	error = reg_key_add_name(ctx, ctx->current, argv[1], 0, NULL, &tmp);

	if (!W_ERROR_IS_OK(error)) {
		fprintf(stderr, "Error adding new subkey '%s': %s\n", argv[1],
			win_errstr(error));
		return error;
	}

	return WERR_OK;
}

static WERROR cmd_rmkey(struct regshell_context *ctx,
			int argc, char **argv)
{
	WERROR error;

	if(argc < 2) {
		fprintf(stderr, "Usage: rmkey <name>\n");
		return WERR_INVALID_PARAM;
	}

	error = reg_key_del(ctx, ctx->current, argv[1]);
	if(!W_ERROR_IS_OK(error)) {
		fprintf(stderr, "Error deleting '%s'\n", argv[1]);
		return error;
	} else {
		fprintf(stderr, "Successfully deleted '%s'\n", argv[1]);
	}

	return WERR_OK;
}

static WERROR cmd_rmval(struct regshell_context *ctx, int argc, char **argv)
{
	WERROR error;

	if(argc < 2) {
		fprintf(stderr, "Usage: rmval <valuename>\n");
		return WERR_INVALID_PARAM;
	}

	error = reg_del_value(ctx, ctx->current, argv[1]);
	if(!W_ERROR_IS_OK(error)) {
		fprintf(stderr, "Error deleting value '%s'\n", argv[1]);
		return error;
	} else {
		fprintf(stderr, "Successfully deleted value '%s'\n", argv[1]);
	}

	return WERR_OK;
}

_NORETURN_ static WERROR cmd_exit(struct regshell_context *ctx,
				  int argc, char **argv)
{
	exit(0);
}

static WERROR cmd_help(struct regshell_context *ctx, int, char **);

static struct {
	const char *name;
	const char *alias;
	const char *help;
	WERROR (*handle)(struct regshell_context *ctx, int argc, char **argv);
} regshell_cmds[] = {
	{"ck", "cd", "Change current key", cmd_ck },
	{"info", "i", "Show detailed information of a key", cmd_info },
	{"list", "ls", "List values/keys in current key", cmd_ls },
	{"print", "p", "Print value", cmd_print },
	{"mkkey", "mkdir", "Make new key", cmd_mkkey },
	{"rmval", "rm", "Remove value", cmd_rmval },
	{"rmkey", "rmdir", "Remove key", cmd_rmkey },
	{"pwd", "pwk", "Printing current key", cmd_pwd },
	{"set", "update", "Update value", cmd_set },
	{"help", "?", "Help", cmd_help },
	{"exit", "quit", "Exit", cmd_exit },
	{"predef", "predefined", "Go to predefined key", cmd_predef },
	{NULL }
};

static WERROR cmd_help(struct regshell_context *ctx,
		       int argc, char **argv)
{
	unsigned int i;
	printf("Available commands:\n");
	for(i = 0; regshell_cmds[i].name; i++) {
		printf("%s - %s\n", regshell_cmds[i].name,
			regshell_cmds[i].help);
	}
	return WERR_OK;
}

static WERROR process_cmd(struct regshell_context *ctx,
			  char *line)
{
	int argc;
	char **argv = NULL;
	int ret, i;

	if ((ret = poptParseArgvString(line, &argc, (const char ***) &argv)) != 0) {
		fprintf(stderr, "regshell: %s\n", poptStrerror(ret));
		return WERR_INVALID_PARAM;
	}

	for(i = 0; regshell_cmds[i].name; i++) {
		if(!strcmp(regshell_cmds[i].name, argv[0]) ||
		   (regshell_cmds[i].alias && !strcmp(regshell_cmds[i].alias, argv[0]))) {
			return regshell_cmds[i].handle(ctx, argc, argv);
		}
	}

	fprintf(stderr, "No such command '%s'\n", argv[0]);

	return WERR_INVALID_PARAM;
}

#define MAX_COMPLETIONS 100

static struct registry_key *current_key = NULL;

static char **reg_complete_command(const char *text, int start, int end)
{
	/* Complete command */
	char **matches;
	size_t len, samelen=0;
	unsigned int i, count=1;

	matches = malloc_array_p(char *, MAX_COMPLETIONS);
	if (!matches) return NULL;
	matches[0] = NULL;

	len = strlen(text);
	for (i=0;regshell_cmds[i].handle && count < MAX_COMPLETIONS-1;i++) {
		if (strncmp(text, regshell_cmds[i].name, len) == 0) {
			matches[count] = strdup(regshell_cmds[i].name);
			if (!matches[count])
				goto cleanup;
			if (count == 1)
				samelen = strlen(matches[count]);
			else
				while (strncmp(matches[count], matches[count-1], samelen) != 0)
					samelen--;
			count++;
		}
	}

	switch (count) {
	case 0:	/* should never happen */
	case 1:
		goto cleanup;
	case 2:
		matches[0] = strdup(matches[1]);
		break;
	default:
		matches[0] = strndup(matches[1], samelen);
	}
	matches[count] = NULL;
	return matches;

cleanup:
	count--;
	while (count >= 0) {
		free(matches[count]);
		count--;
	}
	free(matches);
	return NULL;
}

static char **reg_complete_key(const char *text, int start, int end)
{
	struct registry_key *base;
	const char *subkeyname;
	unsigned int i, j = 1;
	size_t len, samelen = 0;
	char **matches;
	const char *base_n = "";
	TALLOC_CTX *mem_ctx;
	WERROR status;

	matches = malloc_array_p(char *, MAX_COMPLETIONS);
	if (!matches) return NULL;
	matches[0] = NULL;
	mem_ctx = talloc_init("completion");

	base = current_key;

	len = strlen(text);
	for(i = 0; j < MAX_COMPLETIONS-1; i++) {
		status = reg_key_get_subkey_by_index(mem_ctx, base, i,
					     &subkeyname, NULL, NULL);
		if(W_ERROR_IS_OK(status)) {
			if(!strncmp(text, subkeyname, len)) {
				matches[j] = strdup(subkeyname);
				j++;

				if (j == 1)
					samelen = strlen(matches[j]);
				else
					while (strncmp(matches[j], matches[j-1], samelen) != 0)
						samelen--;
			}
		} else if(W_ERROR_EQUAL(status, WERR_NO_MORE_ITEMS)) {
			break;
		} else {
			printf("Error creating completion list: %s\n",
				win_errstr(status));
			talloc_free(mem_ctx);
			return NULL;
		}
	}

	if (j == 1) { /* No matches at all */
		SAFE_FREE(matches);
		talloc_free(mem_ctx);
		return NULL;
	}

	if (j == 2) { /* Exact match */
		asprintf(&matches[0], "%s%s", base_n, matches[1]);
	} else {
		asprintf(&matches[0], "%s%s", base_n,
				talloc_strndup(mem_ctx, matches[1], samelen));
	}
	talloc_free(mem_ctx);

	matches[j] = NULL;
	return matches;
}

static char **reg_completion(const char *text, int start, int end)
{
	smb_readline_ca_char(' ');

	if (start == 0) {
		return reg_complete_command(text, start, end);
	} else {
		return reg_complete_key(text, start, end);
	}
}

int main(int argc, char **argv)
{
	int opt;
	const char *file = NULL;
	poptContext pc;
	const char *remote = NULL;
	struct regshell_context *ctx;
	struct tevent_context *ev_ctx;
	bool ret = true;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"file", 'F', POPT_ARG_STRING, &file, 0, "open hive file", NULL },
		{"remote", 'R', POPT_ARG_STRING, &remote, 0, "connect to specified remote server", NULL},
		POPT_COMMON_SAMBA
		POPT_COMMON_CREDENTIALS
		POPT_COMMON_VERSION
		{ NULL }
	};

	pc = poptGetContext(argv[0], argc, (const char **) argv, long_options,0);

	while((opt = poptGetNextOpt(pc)) != -1) {
	}

	ctx = talloc_zero(NULL, struct regshell_context);

	ev_ctx = s4_event_context_init(ctx);

	if (remote != NULL) {
		ctx->registry = reg_common_open_remote(remote, ev_ctx,
					 cmdline_lp_ctx, cmdline_credentials);
	} else if (file != NULL) {
		ctx->current = reg_common_open_file(file, ev_ctx, cmdline_lp_ctx, cmdline_credentials);
		if (ctx->current == NULL)
			return 1;
		ctx->registry = ctx->current->context;
		ctx->path = talloc_strdup(ctx, "");
		ctx->predef = NULL;
		ctx->root = ctx->current;
	} else {
		ctx->registry = reg_common_open_local(cmdline_credentials, ev_ctx, cmdline_lp_ctx);
	}

	if (ctx->registry == NULL)
		return 1;

	if (ctx->current == NULL) {
		unsigned int i;

		for (i = 0; (reg_predefined_keys[i].handle != 0) &&
			(ctx->current == NULL); i++) {
			WERROR err;
			err = reg_get_predefined_key(ctx->registry,
						     reg_predefined_keys[i].handle,
						     &ctx->current);
			if (W_ERROR_IS_OK(err)) {
				ctx->predef = talloc_strdup(ctx,
							  reg_predefined_keys[i].name);
				ctx->path = talloc_strdup(ctx, "");
				ctx->root = ctx->current;
				break;
			} else {
				ctx->current = NULL;
				ctx->root = NULL;
			}
		}
	}

	if (ctx->current == NULL) {
		fprintf(stderr, "Unable to access any of the predefined keys\n");
		return 1;
	}

	poptFreeContext(pc);

	while (true) {
		char *line, *prompt;

		if (asprintf(&prompt, "%s\\%s> ", ctx->predef?ctx->predef:"",
			     ctx->path) < 0) {
			ret = false;
			break;
		}

		current_key = ctx->current; 		/* No way to pass a void * pointer
							   via readline :-( */
		line = smb_readline(prompt, NULL, reg_completion);

		if (line == NULL) {
			free(prompt);
			break;
		}

		if (line[0] != '\n') {
			ret = W_ERROR_IS_OK(process_cmd(ctx, line));
		}
		free(line);
		free(prompt);
	}
	talloc_free(ctx);

	return (ret?0:1);
}

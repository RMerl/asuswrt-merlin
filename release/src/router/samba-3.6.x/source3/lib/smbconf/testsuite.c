/*
 *  Unix SMB/CIFS implementation.
 *  libsmbconf - Samba configuration library: testsuite
 *  Copyright (C) Michael Adam 2008
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "popt_common.h"
#include "lib/smbconf/smbconf.h"
#include "lib/smbconf/smbconf_init.h"
#include "lib/smbconf/smbconf_reg.h"
#include "lib/smbconf/smbconf_txt.h"

static void print_strings(const char *prefix,
			  uint32_t num_strings, const char **strings)
{
	uint32_t count;

	if (prefix == NULL) {
		prefix = "";
	}

	for (count = 0; count < num_strings; count++) {
		printf("%s%s\n", prefix, strings[count]);
	}
}

static bool test_get_includes(struct smbconf_ctx *ctx)
{
	sbcErr err;
	bool ret = false;
	uint32_t num_includes = 0;
	char **includes = NULL;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	printf("TEST: get_includes\n");
	err = smbconf_get_global_includes(ctx, mem_ctx,
					  &num_includes, &includes);
	if (!SBC_ERROR_IS_OK(err)) {
		printf("FAIL: get_includes - %s\n", sbcErrorString(err));
		goto done;
	}

	printf("got %u includes%s\n", num_includes,
	       (num_includes > 0) ? ":" : ".");
	print_strings("", num_includes, (const char **)includes);

	printf("OK: get_includes\n");
	ret = true;

done:
	talloc_free(mem_ctx);
	return ret;
}

static bool test_set_get_includes(struct smbconf_ctx *ctx)
{
	sbcErr err;
	uint32_t count;
	bool ret = false;
	const char *set_includes[] = {
		"/path/to/include1",
		"/path/to/include2"
	};
	uint32_t set_num_includes = 2;
	char **get_includes = NULL;
	uint32_t get_num_includes = 0;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	printf("TEST: set_get_includes\n");

	err = smbconf_set_global_includes(ctx, set_num_includes, set_includes);
	if (!SBC_ERROR_IS_OK(err)) {
		printf("FAIL: get_set_includes (setting includes) - %s\n",
		       sbcErrorString(err));
		goto done;
	}

	err = smbconf_get_global_includes(ctx, mem_ctx, &get_num_includes,
					  &get_includes);
	if (!SBC_ERROR_IS_OK(err)) {
		printf("FAIL: get_set_includes (getting includes) - %s\n",
		       sbcErrorString(err));
		goto done;
	}

	if (get_num_includes != set_num_includes) {
		printf("FAIL: get_set_includes - set %d includes, got %d\n",
		       set_num_includes, get_num_includes);
		goto done;
	}

	for (count = 0; count < get_num_includes; count++) {
		if (!strequal(set_includes[count], get_includes[count])) {
			printf("expected: \n");
			print_strings("* ", set_num_includes, set_includes);
			printf("got: \n");
			print_strings("* ", get_num_includes,
				      (const char **)get_includes);
			printf("FAIL: get_set_includes - data mismatch:\n");
			goto done;
		}
	}

	printf("OK: set_includes\n");
	ret = true;

done:
	talloc_free(mem_ctx);
	return ret;
}

static bool test_delete_includes(struct smbconf_ctx *ctx)
{
	sbcErr err;
	bool ret = false;
	const char *set_includes[] = {
		"/path/to/include",
	};
	uint32_t set_num_includes = 1;
	char **get_includes = NULL;
	uint32_t get_num_includes = 0;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	printf("TEST: delete_includes\n");

	err = smbconf_set_global_includes(ctx, set_num_includes, set_includes);
	if (!SBC_ERROR_IS_OK(err)) {
		printf("FAIL: delete_includes (setting includes) - %s\n",
		       sbcErrorString(err));
		goto done;
	}

	err = smbconf_delete_global_includes(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		printf("FAIL: delete_includes (deleting includes) - %s\n",
		       sbcErrorString(err));
		goto done;
	}

	err = smbconf_get_global_includes(ctx, mem_ctx, &get_num_includes,
					  &get_includes);
	if (!SBC_ERROR_IS_OK(err)) {
		printf("FAIL: delete_includes (getting includes) - %s\n",
		       sbcErrorString(err));
		goto done;
	}

	if (get_num_includes != 0) {
		printf("FAIL: delete_includes (not empty after delete)\n");
		goto done;
	}

	err = smbconf_delete_global_includes(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		printf("FAIL: delete_includes (delete empty includes) - "
		       "%s\n", sbcErrorString(err));
		goto done;
	}

	printf("OK: delete_includes\n");
	ret = true;

done:
	return ret;
}

static bool create_conf_file(const char *filename)
{
	FILE *f;

	printf("TEST: creating file\n");
	f = sys_fopen(filename, "w");
	if (!f) {
		printf("failure: failed to open %s for writing: %s\n",
		       filename, strerror(errno));
		return false;
	}

	fprintf(f, "[global]\n");
	fprintf(f, "\tserver string = smbconf testsuite\n");
	fprintf(f, "\tworkgroup = SAMBA\n");
	fprintf(f, "\tsecurity = user\n");

	fclose(f);

	printf("OK: create file\n");
	return true;
}

static bool torture_smbconf_txt(void)
{
	sbcErr err;
	bool ret = true;
	const char *filename = "/tmp/smb.conf.smbconf_testsuite";
	struct smbconf_ctx *conf_ctx = NULL;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	printf("test: text backend\n");

	if (!create_conf_file(filename)) {
		ret = false;
		goto done;
	}

	printf("TEST: init\n");
	err = smbconf_init_txt(mem_ctx, &conf_ctx, filename);
	if (!SBC_ERROR_IS_OK(err)) {
		printf("FAIL: text backend failed: %s\n", sbcErrorString(err));
		ret = false;
		goto done;
	}
	printf("OK: init\n");

	ret &= test_get_includes(conf_ctx);

	smbconf_shutdown(conf_ctx);

	printf("TEST: unlink file\n");
	if (unlink(filename) != 0) {
		printf("OK: unlink failed: %s\n", strerror(errno));
		ret = false;
		goto done;
	}
	printf("OK: unlink file\n");

done:
	printf("%s: text backend\n", ret ? "success" : "failure");
	talloc_free(mem_ctx);
	return ret;
}

static bool torture_smbconf_reg(void)
{
	sbcErr err;
	bool ret = true;
	struct smbconf_ctx *conf_ctx = NULL;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	printf("test: registry backend\n");

	printf("TEST: init\n");
	err = smbconf_init_reg(mem_ctx, &conf_ctx, NULL);
	if (!SBC_ERROR_IS_OK(err)) {
		printf("FAIL: init failed: %s\n", sbcErrorString(err));
		ret = false;
		goto done;
	}
	printf("OK: init\n");

	ret &= test_get_includes(conf_ctx);
	ret &= test_set_get_includes(conf_ctx);
	ret &= test_delete_includes(conf_ctx);

	smbconf_shutdown(conf_ctx);

done:
	printf("%s: registry backend\n", ret ? "success" : "failure");
	talloc_free(mem_ctx);
	return ret;
}

static bool torture_smbconf(void)
{
	bool ret = true;
	ret &= torture_smbconf_txt();
	printf("\n");
	ret &= torture_smbconf_reg();
	return ret;
}

int main(int argc, const char **argv)
{
	bool ret;
	poptContext pc;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	struct poptOption long_options[] = {
		POPT_COMMON_SAMBA
		{0, 0, 0, 0}
	};

	load_case_tables();
	setup_logging(argv[0], DEBUG_STDERR);

	/* parse options */
	pc = poptGetContext("smbconftort", argc, (const char **)argv,
			    long_options, 0);

	while(poptGetNextOpt(pc) != -1) { }

	poptFreeContext(pc);

	ret = lp_load(get_dyn_CONFIGFILE(),
		      true,  /* globals_only */
		      false, /* save_defaults */
		      false, /* add_ipc */
		      true   /* initialize globals */);

	if (!ret) {
		printf("failure: error loading the configuration\n");
		goto done;
	}

	ret = torture_smbconf();

done:
	talloc_free(mem_ctx);
	return ret ? 0 : -1;
}

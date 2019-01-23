/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) 2001 Steve French  (sfrench@us.ibm.com)
   Copyright (C) 2001 Jim McDonough (jmcd@us.ibm.com)
   Copyright (C) 2001 Andrew Tridgell (tridge@samba.org)
   Copyright (C) 2001 Andrew Bartlett (abartlet@samba.org)
   Copyright (C) 2004 Stefan Metzmacher (metze@samba.org)
   Copyright (C) 2009 Jelmer Vernooij (jelmer@samba.org)

   Largely rewritten by metze in August 2004

   Originally written by Steve and Jim. Largely rewritten by tridge in
   November 2001.

   Reworked again by abartlet in December 2001

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

/*****************************************************/
/*                                                   */
/*   Distributed SMB/CIFS Server Management Utility  */
/*                                                   */
/*   The intent was to make the syntax similar       */
/*   to the NET utility (first developed in DOS      */
/*   with additional interesting & useful functions  */
/*   added in later SMB server network operating     */
/*   systems).                                       */
/*                                                   */
/*****************************************************/

#include <Python.h>
#include "includes.h"
#include "samba_tool/samba_tool.h"
#include "lib/cmdline/popt_common.h"
#include <ldb.h>
#include "librpc/rpc/dcerpc.h"
#include "param/param.h"
#include "lib/events/events.h"
#include "auth/credentials/credentials.h"
#include "scripting/python/modules.h"

/* There's no Py_ssize_t in 2.4, apparently */
#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 5
typedef int Py_ssize_t;
#endif

static PyObject *py_tuple_from_argv(int argc, const char *argv[])
{
	PyObject *l;
	Py_ssize_t i;

	l = PyTuple_New(argc);
	if (l == NULL) {
		return NULL;
	}

	for (i = 0; i < argc; i++) {
		PyTuple_SetItem(l, i, PyString_FromString(argv[i]));
	}

	return l;
}

static int py_call_with_string_args(PyObject *self, const char *method, int argc, const char *argv[])
{
	PyObject *ret, *args, *py_method;

	args = py_tuple_from_argv(argc, argv);
	if (args == NULL) {
		PyErr_Print();
		return 1;
	}

	py_method = PyObject_GetAttrString(self, method);
	if (py_method == NULL) {
		PyErr_Print();
		return 1;
	}

	ret = PyObject_CallObject(py_method, args);

	Py_DECREF(args);

	if (ret == NULL) {
		PyErr_Print();
		return 1;
	}

	if (ret == Py_None) {
		return 0;
	} else if (PyInt_Check(ret)) {
		return PyInt_AsLong(ret);
	} else {
		fprintf(stderr, "Function return value type unexpected.\n");
		return -1;
	}
}

static PyObject *py_commands(void)
{
	PyObject *netcmd_module, *py_cmds;
	netcmd_module = PyImport_ImportModule("samba.netcmd");
	if (netcmd_module == NULL) {
		PyErr_Print();
		return NULL;
	}

	py_cmds = PyObject_GetAttrString(netcmd_module, "commands");
	if (py_cmds == NULL) {
		PyErr_Print();
		return NULL;
	}

	if (!PyDict_Check(py_cmds)) {
		d_printf("Python net commands is not a dictionary\n");
		return NULL;
	}

	return py_cmds;
}

/*
  run a function from a function table. If not found then
  call the specified usage function
*/
int net_run_function(struct net_context *ctx,
			int argc, const char **argv,
			const struct net_functable *functable,
			int (*usage_fn)(struct net_context *ctx, int argc, const char **argv))
{
	int i;

	if (argc == 0) {
		return usage_fn(ctx, argc, argv);

	} else if (argc == 1 && strequal(argv[0], "help")) {
		return net_help(ctx, functable);
	}

	for (i=0; functable[i].name; i++) {
		if (strcasecmp_m(argv[0], functable[i].name) == 0)
			return functable[i].fn(ctx, argc-1, argv+1);
	}

	d_printf("No command: '%s'\n", argv[0]);
	return usage_fn(ctx, argc, argv);
}

/*
  run a usage function from a function table. If not found then fail
*/
int net_run_usage(struct net_context *ctx,
			int argc, const char **argv,
			const struct net_functable *functable)
{
	int i;
	PyObject *py_cmds, *py_cmd;

	for (i=0; functable[i].name; i++) {
		if (strcasecmp_m(argv[0], functable[i].name) == 0)
			if (functable[i].usage) {
				return functable[i].usage(ctx, argc-1, argv+1);
			}
	}

	py_cmds = py_commands();
	if (py_cmds == NULL) {
		return 1;
	}

	py_cmd = PyDict_GetItemString(py_cmds, argv[0]);
	if (py_cmd != NULL) {
		return py_call_with_string_args(py_cmd, "usage", argc-1,
                                                argv+1);
	}

	d_printf("No usage information for command: %s\n", argv[0]);

	return 1;
}


/* main function table */
static const struct net_functable net_functable[] = {
	{"password", "Changes/Sets the password on a user account [server connection needed]\n", net_password, net_password_usage},
	{"samdump", "dump the sam of a domain\n", net_samdump, net_samdump_usage},
	{"samsync", "synchronise into the local ldb the sam of an NT4 domain\n", net_samsync_ldb, net_samsync_ldb_usage},
	{"gpo", "Administer group policies\n", net_gpo, net_gpo_usage},
	{NULL, NULL, NULL, NULL}
};

static int net_help_builtin(const struct net_functable *ftable)
{
	int i = 0;
	const char *name = ftable[i].name;
	const char *desc = ftable[i].desc;

	while (name && desc) {
		if (strlen(name) > 7) {
			d_printf("\t%s\t%s", name, desc);
		} else {
			d_printf("\t%s\t\t%s", name, desc);
		}
		name = ftable[++i].name;
		desc = ftable[i].desc;
	}
	return 0;
}

static int net_help_python(void)
{
	PyObject *py_cmds;
	PyObject *key, *value;
	Py_ssize_t pos = 0;

	py_cmds = py_commands();
	if (py_cmds == NULL) {
		return 1;
	}

	while (PyDict_Next(py_cmds, &pos, &key, &value)) {
		char *name, *desc;
		PyObject *py_desc;
		if (!PyString_Check(key)) {
			d_printf("Command name not a string\n");
			return 1;
		}
		name = PyString_AsString(key);
		py_desc = PyObject_GetAttrString(value, "description");
		if (py_desc == NULL) {
			PyErr_Print();
			return 1;
		}
		if (!PyString_Check(py_desc)) {
			d_printf("Command description for %s not a string\n",
				name);
			return 1;
		}
		desc = PyString_AsString(py_desc);
		if (strlen(name) > 7) {
			d_printf("\t%s\t%s\n", name, desc);
		} else {
			d_printf("\t%s\t\t%s\n", name, desc);
		}
	}
	return 0;
}

int net_help(struct net_context *ctx, const struct net_functable *ftable)
{
	d_printf("Available commands:\n");
	net_help_builtin(ftable);
	net_help_python();
	return 0;
}

static int net_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("Usage:\n");
	d_printf("samba-tool <command> [options]\n");
	net_help(ctx, net_functable);
	return -1;
}

/****************************************************************************
  main program
****************************************************************************/
static int binary_net(int argc, const char **argv)
{
	int opt,i;
	int rc;
	int argc_new;
	PyObject *py_cmds, *py_cmd;
	const char **argv_new;
	struct tevent_context *ev;
	struct net_context *ctx = NULL;
	poptContext pc;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		POPT_COMMON_SAMBA
		POPT_COMMON_CONNECTION
		POPT_COMMON_CREDENTIALS
		POPT_COMMON_VERSION
		{ NULL }
	};

	setlinebuf(stdout);

	dcerpc_init(cmdline_lp_ctx);

	ev = s4_event_context_init(NULL);
	if (!ev) {
		d_printf("Failed to create an event context\n");
		exit(1);
	}
	Py_Initialize();
	PySys_SetArgv(argc, discard_const_p(char *, argv));
	py_update_path(); /* Put the Samba path at the start of sys.path */

	py_cmds = py_commands();
	if (py_cmds == NULL) {
		return 1;
	}

	for (i=1; i<argc; i++) {
		if (argv[i][0] != '-') {
			py_cmd = PyDict_GetItemString(py_cmds, argv[i]);
			if (py_cmd != NULL) {
				rc = py_call_with_string_args(py_cmd, "_run",
							      argc-1, argv+1);
				talloc_free(ev);
				return rc;
			}
		}
	}

	pc = poptGetContext("net", argc, (const char **) argv, long_options,
			    POPT_CONTEXT_KEEP_FIRST);

	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		default:
			d_printf("Invalid option %s: %s\n",
				 poptBadOption(pc, 0), poptStrerror(opt));
			net_usage(ctx, argc, argv);
			exit(1);
		}
	}

	argv_new = (const char **)poptGetArgs(pc);

	argc_new = argc;
	for (i=0; i<argc; i++) {
		if (argv_new[i] == NULL) {
			argc_new = i;
			break;
		}
	}

	if (argc_new < 2) {
		return net_usage(ctx, argc, argv);
	}


	ctx = talloc(ev, struct net_context);
	if (!ctx) {
		d_printf("Failed to talloc a net_context\n");
		exit(1);
	}

	ZERO_STRUCTP(ctx);
	ctx->lp_ctx = cmdline_lp_ctx;
	ctx->credentials = cmdline_credentials;
	ctx->event_ctx = ev;



	rc = net_run_function(ctx, argc_new-1, argv_new+1, net_functable,
			      net_usage);

	if (rc != 0) {
		DEBUG(0,("return code = %d\n", rc));
	}

	talloc_free(ev);
	return rc;
}

 int main(int argc, const char **argv)
{
	return binary_net(argc, argv);
}

/* 
   Unix SMB/CIFS implementation.
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007
   Copyright (C) Matthias Dieter Wallnöfer          2009
   
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

#include <Python.h>
#include "includes.h"
#include "version.h"
#include "param/pyparam.h"
#include "lib/socket/netif.h"

void init_glue(void);

static PyObject *py_generate_random_str(PyObject *self, PyObject *args)
{
	int len;
	PyObject *ret;
	char *retstr;
	if (!PyArg_ParseTuple(args, "i", &len))
		return NULL;

	retstr = generate_random_str(NULL, len);
	ret = PyString_FromString(retstr);
	talloc_free(retstr);
	return ret;
}

static PyObject *py_generate_random_password(PyObject *self, PyObject *args)
{
	int min, max;
	PyObject *ret;
	char *retstr;
	if (!PyArg_ParseTuple(args, "ii", &min, &max))
		return NULL;

	retstr = generate_random_password(NULL, min, max);
	if (retstr == NULL) {
		return NULL;
	}
	ret = PyString_FromString(retstr);
	talloc_free(retstr);
	return ret;
}

static PyObject *py_unix2nttime(PyObject *self, PyObject *args)
{
	time_t t;
	NTTIME nt;
	if (!PyArg_ParseTuple(args, "I", &t))
		return NULL;

	unix_to_nt_time(&nt, t);

	return PyLong_FromLongLong((uint64_t)nt);
}

static PyObject *py_nttime2unix(PyObject *self, PyObject *args)
{
	time_t t;
	NTTIME nt;
	if (!PyArg_ParseTuple(args, "K", &nt))
		return NULL;

	t = nt_time_to_unix(nt);

	return PyInt_FromLong((uint64_t)t);
}

static PyObject *py_nttime2string(PyObject *self, PyObject *args)
{
	PyObject *ret;
	NTTIME nt;
	TALLOC_CTX *tmp_ctx;
	const char *string;
	if (!PyArg_ParseTuple(args, "K", &nt))
		return NULL;

	tmp_ctx = talloc_new(NULL);
	if (tmp_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	string = nt_time_string(tmp_ctx, nt);
	ret =  PyString_FromString(string);

	talloc_free(tmp_ctx);

	return ret;
}

static PyObject *py_set_debug_level(PyObject *self, PyObject *args)
{
	unsigned level;
	if (!PyArg_ParseTuple(args, "I", &level))
		return NULL;
	(DEBUGLEVEL) = level;
	Py_RETURN_NONE;
}

static PyObject *py_get_debug_level(PyObject *self)
{
	return PyInt_FromLong(DEBUGLEVEL);
}

/*
  return the list of interface IPs we have configured
  takes an loadparm context, returns a list of IPs in string form

  Does not return addresses on 127.0.0.0/8
 */
static PyObject *py_interface_ips(PyObject *self, PyObject *args)
{
	PyObject *pylist;
	int count;
	TALLOC_CTX *tmp_ctx;
	PyObject *py_lp_ctx;
	struct loadparm_context *lp_ctx;
	struct interface *ifaces;
	int i, ifcount;
	int all_interfaces;

	if (!PyArg_ParseTuple(args, "Oi", &py_lp_ctx, &all_interfaces))
		return NULL;

	tmp_ctx = talloc_new(NULL);
	if (tmp_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	lp_ctx = lpcfg_from_py_object(tmp_ctx, py_lp_ctx);
	if (lp_ctx == NULL) {
		talloc_free(tmp_ctx);
		return NULL;
	}

	load_interfaces(tmp_ctx, lpcfg_interfaces(lp_ctx), &ifaces);

	count = iface_count(ifaces);

	/* first count how many are not loopback addresses */
	for (ifcount = i = 0; i<count; i++) {
		const char *ip = iface_n_ip(ifaces, i);
		if (!(!all_interfaces && iface_same_net(ip, "127.0.0.1", "255.0.0.0"))) {
			ifcount++;
		}
	}

	pylist = PyList_New(ifcount);
	for (ifcount = i = 0; i<count; i++) {
		const char *ip = iface_n_ip(ifaces, i);
		if (!(!all_interfaces && iface_same_net(ip, "127.0.0.1", "255.0.0.0"))) {
			PyList_SetItem(pylist, ifcount, PyString_FromString(ip));
			ifcount++;
		}
	}
	talloc_free(tmp_ctx);
	return pylist;
}

static PyMethodDef py_misc_methods[] = {
	{ "generate_random_str", (PyCFunction)py_generate_random_str, METH_VARARGS,
		"generate_random_str(len) -> string\n"
		"Generate random string with specified length." },
	{ "generate_random_password", (PyCFunction)py_generate_random_password,
		METH_VARARGS, "generate_random_password(min, max) -> string\n"
		"Generate random password with a length >= min and <= max." },
	{ "unix2nttime", (PyCFunction)py_unix2nttime, METH_VARARGS,
		"unix2nttime(timestamp) -> nttime" },
	{ "nttime2unix", (PyCFunction)py_nttime2unix, METH_VARARGS,
		"nttime2unix(nttime) -> timestamp" },
	{ "nttime2string", (PyCFunction)py_nttime2string, METH_VARARGS,
		"nttime2string(nttime) -> string" },
	{ "set_debug_level", (PyCFunction)py_set_debug_level, METH_VARARGS,
		"set debug level" },
	{ "get_debug_level", (PyCFunction)py_get_debug_level, METH_NOARGS,
		"get debug level" },
	{ "interface_ips", (PyCFunction)py_interface_ips, METH_VARARGS,
		"get interface IP address list"},
	{ NULL }
};

void init_glue(void)
{
	PyObject *m;

	debug_setup_talloc_log();

	m = Py_InitModule3("_glue", py_misc_methods, 
			   "Python bindings for miscellaneous Samba functions.");
	if (m == NULL)
		return;

	PyModule_AddObject(m, "version",
					   PyString_FromString(SAMBA_VERSION_STRING));

	/* one of the most annoying things about python scripts is
 	   that they don't die when you hit control-C. This fixes that
 	   sillyness. As we do all database operations using
 	   transactions, this is also safe. 
	*/
	signal(SIGINT, SIG_DFL);
}


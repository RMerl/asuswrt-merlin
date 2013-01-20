#include <Python.h>
#include <time.h>
#include <uuid/uuid.h>

static PyObject * _uuid_generate(PyObject *self, PyObject *args)
{
  uuid_t u;
  char uuid[37];
  if (!PyArg_ParseTuple(args, "")) return NULL;
  uuid_generate(u);
  uuid_unparse(u, uuid);
  return Py_BuildValue("s", uuid);
}

static PyMethodDef _uuid_methods[] = {
  {"generate", _uuid_generate, METH_VARARGS, "Generate UUID"},
  {NULL, NULL, 0, NULL}
};

void inite2fsprogs_uuid(void)
{
  (void) Py_InitModule("e2fsprogs_uuid", _uuid_methods);
}

/*
   WMI Implementation
   Copyright (C) 2006 Andrzej Hajda <andrzej.hajda@wp.pl>
   Copyright (C) 2008 Jelmer Vernooij <jelmer@samba.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

%module wmi

%include "typemaps.i"
%include "libcli/util/errors.i"
%import "stdint.i"
%import "lib/talloc/talloc.i"

%runtime %{
void push_object(PyObject **stack, PyObject *o)
{
	if ((!*stack) || (*stack == Py_None)) {
    		*stack = o;
	} else {
		PyObject *o2, *o3;
    		if (!PyTuple_Check(*stack)) {
        		o2 = *stack;
        		*stack = PyTuple_New(1);
        		PyTuple_SetItem(*stack,0,o2);
    		}
    		o3 = PyTuple_New(1);
	        PyTuple_SetItem(o3,0,o);
	        o2 = *stack;
    		*stack = PySequence_Concat(o2,o3);
	        Py_DECREF(o2);
    		Py_DECREF(o3);
	}
}
%}

%{
#include "includes.h"
#include "librpc/gen_ndr/misc.h"
#include "librpc/rpc/dcerpc.h"
#include "lib/com/dcom/dcom.h"
#include "librpc/gen_ndr/com_dcom.h"
#include "lib/wmi/wmi.h"


WERROR WBEM_ConnectServer(struct com_context *ctx, const char *server, const char *nspace, const char *user, const char *password, 
	const char *locale, uint32_t flags, const char *authority, struct IWbemContext* wbem_ctx, struct IWbemServices** services);
WERROR IEnumWbemClassObject_SmartNext(struct IEnumWbemClassObject *d, TALLOC_CTX *mem_ctx, int32_t lTimeout,uint32_t uCount, 
	struct WbemClassObject **apObjects, uint32_t *puReturned);

static PyObject *PyObject_FromCVAR(uint32_t cimtype, union CIMVAR *cvar);
static PyObject *PySWbemObject_FromWbemClassObject(struct WbemClassObject *wco);

static struct com_context *com_ctx;
static PyObject *ComError;
static PyObject *mod_win32_client;
static PyObject *mod_pywintypes;

typedef struct IUnknown IUnknown;
typedef struct IWbemServices IWbemServices;
typedef struct IWbemClassObject IWbemClassObject;
typedef struct IEnumWbemClassObject IEnumWbemClassObject;
%}

%wrapper %{

#define RETURN_CVAR_ARRAY(fmt, arr) {\
	PyObject *l, *o;\
	uint32_t i;\
\
	if (!arr) {\
		Py_INCREF(Py_None);\
		return Py_None;\
	}\
	l = PyList_New(arr->count);\
	if (!l) return NULL;\
	for (i = 0; i < arr->count; ++i) {\
		o = _Py_BuildValue(fmt, arr->item[i]);\
		if (!o) {\
			Py_DECREF(l);\
			return NULL;\
		}\
		PyList_SET_ITEM(l, i, o);\
	}\
	return l;\
}

static PyObject *_Py_BuildValue(char *str, ...)
{
   PyObject * result = NULL;
   va_list lst;
   va_start(lst, str);
   if (str && *str == 'I') {
        uint32_t value = va_arg(lst, uint32_t);
	if (value & 0x80000000) {
           result = Py_BuildValue("L", (long)value);
        } else {
           result = Py_BuildValue("i", value);
	}
   } else {
       result = Py_VaBuildValue(str, lst);
   }
   va_end(lst);
   return result;
}


static PyObject *PyObject_FromCVAR(uint32_t cimtype, union CIMVAR *cvar)
{
	switch (cimtype) {
        case CIM_SINT8: return Py_BuildValue("b", cvar->v_sint8);
        case CIM_UINT8: return Py_BuildValue("B", cvar->v_uint8);
        case CIM_SINT16: return Py_BuildValue("h", cvar->v_sint16);
        case CIM_UINT16: return Py_BuildValue("H", cvar->v_uint16);
        case CIM_SINT32: return Py_BuildValue("i", cvar->v_sint32);
        case CIM_UINT32: return _Py_BuildValue("I", cvar->v_uint32);
        case CIM_SINT64: return Py_BuildValue("L", cvar->v_sint64);
        case CIM_UINT64: return Py_BuildValue("K", cvar->v_uint64);
        case CIM_REAL32: return Py_BuildValue("f", cvar->v_real32);
        case CIM_REAL64: return Py_BuildValue("d", cvar->v_real64);
        case CIM_BOOLEAN: return Py_BuildValue("h", cvar->v_boolean);
        case CIM_STRING: return Py_BuildValue("s", cvar->v_string);
        case CIM_DATETIME: return Py_BuildValue("s", cvar->v_datetime);
        case CIM_REFERENCE: return Py_BuildValue("s", cvar->v_reference);
        case CIM_OBJECT: return PySWbemObject_FromWbemClassObject(cvar->v_object);
        case CIM_ARR_SINT8: RETURN_CVAR_ARRAY("b", cvar->a_sint8);
        case CIM_ARR_UINT8: RETURN_CVAR_ARRAY("B", cvar->a_uint8);
        case CIM_ARR_SINT16: RETURN_CVAR_ARRAY("h", cvar->a_sint16);
        case CIM_ARR_UINT16: RETURN_CVAR_ARRAY("H", cvar->a_uint16);
        case CIM_ARR_SINT32: RETURN_CVAR_ARRAY("i", cvar->a_sint32);
        case CIM_ARR_UINT32: RETURN_CVAR_ARRAY("I", cvar->a_uint32);
        case CIM_ARR_SINT64: RETURN_CVAR_ARRAY("L", cvar->a_sint64);
        case CIM_ARR_UINT64: RETURN_CVAR_ARRAY("K", cvar->a_uint64);
        case CIM_ARR_REAL32: RETURN_CVAR_ARRAY("f", cvar->a_real32);
        case CIM_ARR_REAL64: RETURN_CVAR_ARRAY("d", cvar->a_real64);
        case CIM_ARR_BOOLEAN: RETURN_CVAR_ARRAY("h", cvar->a_boolean);
        case CIM_ARR_STRING: RETURN_CVAR_ARRAY("s", cvar->a_string);
        case CIM_ARR_DATETIME: RETURN_CVAR_ARRAY("s", cvar->a_datetime);
        case CIM_ARR_REFERENCE: RETURN_CVAR_ARRAY("s", cvar->a_reference);
	default:
		{
		char *str;
		str = talloc_asprintf(NULL, "Unsupported CIMTYPE(0x%04X)", cimtype);
		PyErr_SetString(PyExc_RuntimeError, str);
		talloc_free(str);
		return NULL;
		}
	}
}

#undef RETURN_CVAR_ARRAY

PyObject *PySWbemObject_InitProperites(PyObject *o, struct WbemClassObject *wco)
{
	PyObject *properties;
	PyObject *addProp;
	uint32_t i;
	int32_t r;
	PyObject *result;

	result = NULL;
	properties = PyObject_GetAttrString(o, "Properties_");
	if (!properties) return NULL;
	addProp = PyObject_GetAttrString(properties, "Add");
	if (!addProp) {
		Py_DECREF(properties);
		return NULL;
	}

	for (i = 0; i < wco->obj_class->__PROPERTY_COUNT; ++i) {
		PyObject *args, *property;

		args = Py_BuildValue("(si)", wco->obj_class->properties[i].property.name, wco->obj_class->properties[i].property.desc->cimtype & CIM_TYPEMASK);
		if (!args) goto finish;
		property = PyObject_CallObject(addProp, args);
		Py_DECREF(args);
		if (!property) goto finish;
		if (wco->flags & WCF_INSTANCE) {
			PyObject *value;

			if (wco->instance->default_flags[i] & 1) {
				value = Py_None;
				Py_INCREF(Py_None);
			} else
				value = PyObject_FromCVAR(wco->obj_class->properties[i].property.desc->cimtype & CIM_TYPEMASK, &wco->instance->data[i]);
			if (!value) {
				Py_DECREF(property);
				goto finish;
			}
			r = PyObject_SetAttrString(property, "Value", value);
			Py_DECREF(value);
			if (r == -1) {
				PyErr_SetString(PyExc_RuntimeError, "Error setting value of property");
				goto finish;
			}
		}
		Py_DECREF(property);
	}

	Py_INCREF(Py_None);
	result = Py_None;
finish:
	Py_DECREF(addProp);
	Py_DECREF(properties);
	return result;
}

static PyObject *PySWbemObject_FromWbemClassObject(struct WbemClassObject *wco)
{
	PyObject *swo_class, *swo, *args, *result;

	swo_class = PyObject_GetAttrString(mod_win32_client, "SWbemObject");
	if (!swo_class) return NULL;
	args = PyTuple_New(0);
	if (!args) {
		Py_DECREF(swo_class);
		return NULL;
	}
	swo = PyObject_CallObject(swo_class, args);
	Py_DECREF(args);
	Py_DECREF(swo_class);
	if (!swo) return NULL;

	result = PySWbemObject_InitProperites(swo, wco);
	if (!result) {
		Py_DECREF(swo);
		return NULL;
	}
	Py_DECREF(result);

	return swo;
}

%}

%typemap(in, numinputs=0) struct com_context *ctx {
	$1 = com_ctx;
}

%typemap(in, numinputs=0) struct IWbemServices **services (struct IWbemServices *temp) {
	$1 = &temp;
}

%typemap(argout) struct IWbemServices **services {
	PyObject *o;
	o = SWIG_NewPointerObj(*$1, SWIGTYPE_p_IWbemServices, 0);
	push_object(&$result, o);
}

WERROR WBEM_ConnectServer(struct com_context *ctx, const char *server, const char *nspace, const char *user, const char *password,
        const char *locale, uint32_t flags, const char *authority, struct IWbemContext* wbem_ctx, struct IWbemServices** services);

%typemap(in, numinputs=0) struct IEnumWbemClassObject **ppEnum (struct IEnumWbemClassObject *temp) {
	$1 = &temp;
}

%typemap(argout) struct IEnumWbemClassObject **ppEnum {
	PyObject *o;
	o = SWIG_NewPointerObj(*$1, SWIGTYPE_p_IEnumWbemClassObject, 0);
	push_object(&$result, o);
}

typedef struct IUnknown {
    %extend {
    uint32_t Release(TALLOC_CTX *mem_ctx);
    }
} IUnknown;

%typemap(in) struct BSTR {
    $1.data = PyString_AsString($input);
}


typedef struct IWbemServices {
    %extend {
    WERROR ExecQuery(TALLOC_CTX *mem_ctx, struct BSTR strQueryLanguage, struct BSTR strQuery, int32_t lFlags, struct IWbemContext *pCtx, struct IEnumWbemClassObject **ppEnum);
    WERROR ExecNotificationQuery(TALLOC_CTX *mem_ctx, struct BSTR strQueryLanguage, struct BSTR strQuery, int32_t lFlags, struct IWbemContext *pCtx, struct IEnumWbemClassObject **ppEnum);
    WERROR CreateInstanceEnum(TALLOC_CTX *mem_ctx, struct BSTR strClass, 
	int32_t lFlags, struct IWbemContext *pCtx, struct IEnumWbemClassObject **ppEnum);
    }
} IWbemServices;

typedef struct IEnumWbemClassObject {
    %extend {
    WERROR Reset(TALLOC_CTX *mem_ctx);
    }
} IEnumWbemClassObject;

%typemap(in, numinputs=1) (uint32_t uCount, struct WbemClassObject **apObjects, uint32_t *puReturned) (uint32_t uReturned) {
        if (PyLong_Check($input))
    		$1 = PyLong_AsUnsignedLong($input);
        else if (PyInt_Check($input))
    		$1 = PyInt_AsLong($input);
        else {
            PyErr_SetString(PyExc_TypeError,"Expected a long or an int");
            return NULL;
        }
	$2 = talloc_array(NULL, struct WbemClassObject *, $1);
	$3 = &uReturned;
}

%typemap(argout) (struct WbemClassObject **apObjects, uint32_t *puReturned) {
	uint32_t i;
	PyObject *o;
	int32_t error;

	error = 0;

	$result = PyTuple_New(*$2);
	for (i = 0; i < *$2; ++i) {
		if (!error) {
			o = PySWbemObject_FromWbemClassObject($1[i]);
			if (!o)
				--error;
			else
			    error = PyTuple_SetItem($result, i, o);
		}
		talloc_free($1[i]);
	}
	talloc_free($1);
	if (error) return NULL;
}

WERROR IEnumWbemClassObject_SmartNext(struct IEnumWbemClassObject *d, TALLOC_CTX *mem_ctx, int32_t lTimeout, uint32_t uCount, 
	struct WbemClassObject **apObjects, uint32_t *puReturned);

%init %{

	mod_win32_client = PyImport_ImportModule("win32com.client");
	mod_pywintypes = PyImport_ImportModule("pywintypes");
	ComError = PyObject_GetAttrString(mod_pywintypes, "com_error");

    wmi_init(&com_ctx, NULL);
    {
	PyObject *pModule;

	pModule = PyImport_ImportModule( "win32com.client" );
    }
%}

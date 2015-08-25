/* $Id: py_pjsua.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PY_PJSUA_H__
#define __PY_PJSUA_H__

#define _CRT_SECURE_NO_DEPRECATE

#include <Python.h>
#include <structmember.h>
#include <pjsua-lib/pjsua.h>


PJ_INLINE(pj_str_t) PyString_to_pj_str(const PyObject *obj)
{
    pj_str_t str;

    if (obj) {
	str.ptr = PyString_AS_STRING(obj);
	str.slen = PyString_GET_SIZE(obj);
    } else {
	str.ptr = NULL;
	str.slen = 0;
    }

    return str;
}


//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pj_pool
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pj_pool_t * pool;
} PyObj_pj_pool;


/*
 * PyTyp_pj_pool_t
 */
static PyTypeObject PyTyp_pj_pool_t =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.Pj_Pool",        /*tp_name*/
    sizeof(PyObj_pj_pool),    /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "pj_pool_t objects",       /* tp_doc */

};


//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsip_endpoint
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjsip_endpoint * endpt;
} PyObj_pjsip_endpoint;


/*
 * PyTyp_pjsip_endpoint
 */
static PyTypeObject PyTyp_pjsip_endpoint =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.Pjsip_Endpoint", /*tp_name*/
    sizeof(PyObj_pjsip_endpoint),/*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "pjsip_endpoint objects",  /* tp_doc */
};


/*
 * PyObj_pjmedia_endpt
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjmedia_endpt * endpt;
} PyObj_pjmedia_endpt;


//////////////////////////////////////////////////////////////////////////////
/*
 * PyTyp_pjmedia_endpt
 */
static PyTypeObject PyTyp_pjmedia_endpt =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.Pjmedia_Endpt",  /*tp_name*/
    sizeof(PyObj_pjmedia_endpt), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "pjmedia_endpt objects",   /* tp_doc */

};


//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pj_pool_factory
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pj_pool_factory * pool_fact;
} PyObj_pj_pool_factory;



/*
 * PyTyp_pj_pool_factory
 */
static PyTypeObject PyTyp_pj_pool_factory =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.Pj_Pool_Factory",/*tp_name*/
    sizeof(PyObj_pj_pool_factory), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "pj_pool_factory objects", /* tp_doc */

};


//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsip_cred_info
 */
typedef struct
{
    PyObject_HEAD

    /* Type-specific fields go here. */
    PyObject *realm;
    PyObject *scheme;
    PyObject *username;
    int	      data_type;
    PyObject *data;
    
} PyObj_pjsip_cred_info;

/*
 * cred_info_dealloc
 * deletes a cred info from memory
 */
static void PyObj_pjsip_cred_info_delete(PyObj_pjsip_cred_info* self)
{
    Py_XDECREF(self->realm);
    Py_XDECREF(self->scheme);
    Py_XDECREF(self->username);
    Py_XDECREF(self->data);
    self->ob_type->tp_free((PyObject*)self);
}


static void PyObj_pjsip_cred_info_import(PyObj_pjsip_cred_info *obj,
					 const pjsip_cred_info *cfg)
{
    Py_XDECREF(obj->realm);
    obj->realm = PyString_FromStringAndSize(cfg->realm.ptr, cfg->realm.slen);
    Py_XDECREF(obj->scheme);
    obj->scheme = PyString_FromStringAndSize(cfg->scheme.ptr, cfg->scheme.slen);
    Py_XDECREF(obj->username);
    obj->username = PyString_FromStringAndSize(cfg->username.ptr, cfg->username.slen);
    obj->data_type = cfg->data_type;
    Py_XDECREF(obj->data);
    obj->data = PyString_FromStringAndSize(cfg->data.ptr, cfg->data.slen);
}

static void PyObj_pjsip_cred_info_export(pjsip_cred_info *cfg,
					 PyObj_pjsip_cred_info *obj)
{
    cfg->realm	= PyString_to_pj_str(obj->realm);
    cfg->scheme	= PyString_to_pj_str(obj->scheme);
    cfg->username = PyString_to_pj_str(obj->username);
    cfg->data_type = obj->data_type;
    cfg->data	= PyString_to_pj_str(obj->data);
}


/*
 * cred_info_new
 * constructor for cred_info object
 */
static PyObject * PyObj_pjsip_cred_info_new(PyTypeObject *type, 
					    PyObject *args,
					    PyObject *kwds)
{
    PyObj_pjsip_cred_info *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsip_cred_info *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->realm = PyString_FromString("");
        if (self->realm == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        self->scheme = PyString_FromString("");
        if (self->scheme == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        self->username = PyString_FromString("");
        if (self->username == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
        self->data = PyString_FromString("");
        if (self->data == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
    }

    return (PyObject *)self;
}


/*
 * PyObj_pjsip_cred_info_members
 */
static PyMemberDef PyObj_pjsip_cred_info_members[] =
{
    {
        "realm", T_OBJECT_EX,
        offsetof(PyObj_pjsip_cred_info, realm), 0,
        "Realm"
    },
    {
        "scheme", T_OBJECT_EX,
        offsetof(PyObj_pjsip_cred_info, scheme), 0,
        "Scheme"
    },
    {
        "username", T_OBJECT_EX,
        offsetof(PyObj_pjsip_cred_info, username), 0,
        "User name"
    },
    {
        "data", T_OBJECT_EX,
        offsetof(PyObj_pjsip_cred_info, data), 0,
        "The data, which can be a plaintext password or a hashed digest, "
	"depending on the value of data_type"
    },
    {
        "data_type", T_INT, 
	offsetof(PyObj_pjsip_cred_info, data_type), 0,
        "Type of data"
    },
    
    {NULL}  /* Sentinel */
};

/*
 * PyTyp_pjsip_cred_info
 */
static PyTypeObject PyTyp_pjsip_cred_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Pjsip_Cred_Info",      /*tp_name*/
    sizeof(PyObj_pjsip_cred_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)PyObj_pjsip_cred_info_delete,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "PJSIP credential information", /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    PyObj_pjsip_cred_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyObj_pjsip_cred_info_new,             /* tp_new */

};


//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsip_event
 * C/python typewrapper for event struct
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjsip_event * event;
} PyObj_pjsip_event;



/*
 * PyTyp_pjsip_event
 * event struct signatures
 */
static PyTypeObject PyTyp_pjsip_event =
{
    PyObject_HEAD_INIT(NULL)
    0,                          /*ob_size*/
    "py_pjsua.Pjsip_Event",     /*tp_name*/
    sizeof(PyObj_pjsip_event),  /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    0,                          /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash */
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,         /*tp_flags*/
    "pjsip_event object",       /*tp_doc */
};


//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsip_rx_data
 * C/python typewrapper for pjsip_rx_data struct
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjsip_rx_data * rdata;
} PyObj_pjsip_rx_data;


/*
 * PyTyp_pjsip_rx_data
 */
static PyTypeObject PyTyp_pjsip_rx_data =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Pjsip_Rx_Data",       /*tp_name*/
    sizeof(PyObj_pjsip_rx_data),    /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    0,                              /*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "pjsip_rx_data object",         /*tp_doc*/
};



//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsua_callback
 * C/python typewrapper for callback struct
 */
typedef struct PyObj_pjsua_callback
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    PyObject * on_call_state;
    PyObject * on_incoming_call;
    PyObject * on_call_media_state;
    PyObject * on_dtmf_digit;
    PyObject * on_call_transfer_request;
    PyObject * on_call_transfer_status;
    PyObject * on_call_replace_request;
    PyObject * on_call_replaced;
    PyObject * on_reg_state;
    PyObject * on_buddy_state;
    PyObject * on_pager;
    PyObject * on_pager_status;
    PyObject * on_typing;
} PyObj_pjsua_callback;


/*
 * PyObj_pjsua_callback_delete
 * destructor function for callback struct
 */
static void PyObj_pjsua_callback_delete(PyObj_pjsua_callback* self)
{
    Py_XDECREF(self->on_call_state);
    Py_XDECREF(self->on_incoming_call);
    Py_XDECREF(self->on_call_media_state);
    Py_XDECREF(self->on_dtmf_digit);
    Py_XDECREF(self->on_call_transfer_request);
    Py_XDECREF(self->on_call_transfer_status);
    Py_XDECREF(self->on_call_replace_request);
    Py_XDECREF(self->on_call_replaced);
    Py_XDECREF(self->on_reg_state);
    Py_XDECREF(self->on_buddy_state);
    Py_XDECREF(self->on_pager);
    Py_XDECREF(self->on_pager_status);
    Py_XDECREF(self->on_typing);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * PyObj_pjsua_callback_new
 * * declares constructor for callback struct
 */
static PyObject * PyObj_pjsua_callback_new(PyTypeObject *type, 
					   PyObject *args,
					   PyObject *kwds)
{
    PyObj_pjsua_callback *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_callback *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        Py_INCREF(Py_None);
        self->on_call_state = Py_None;
        if (self->on_call_state == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_incoming_call = Py_None;
        if (self->on_incoming_call == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_call_media_state = Py_None;
        if (self->on_call_media_state == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_dtmf_digit = Py_None;
        if (self->on_dtmf_digit == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_call_transfer_request = Py_None;
        if (self->on_call_transfer_request == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_call_transfer_status = Py_None;
        if (self->on_call_transfer_status == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_call_replace_request = Py_None;
        if (self->on_call_replace_request == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_call_replaced = Py_None;
        if (self->on_call_replaced == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_reg_state = Py_None;
        if (self->on_reg_state == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_buddy_state = Py_None;
        if (self->on_buddy_state == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_pager = Py_None;
        if (self->on_pager == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_pager_status = Py_None;
        if (self->on_pager_status == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->on_typing = Py_None;
        if (self->on_typing == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
    }

    return (PyObject *)self;
}


/*
 * PyObj_pjsua_callback_members
 * declares available functions for callback object
 */
static PyMemberDef PyObj_pjsua_callback_members[] =
{
    {
        "on_call_state", T_OBJECT_EX, 
	offsetof(PyObj_pjsua_callback, on_call_state), 0, 
	"Notify application when invite state has changed. Application may "
        "then query the call info to get the detail call states."
    },
    {
        "on_incoming_call", T_OBJECT_EX,
        offsetof(PyObj_pjsua_callback, on_incoming_call), 0,
        "Notify application on incoming call."
    },
    {
        "on_call_media_state", T_OBJECT_EX,
        offsetof(PyObj_pjsua_callback, on_call_media_state), 0,
        "Notify application when media state in the call has changed. Normal "
        "application would need to implement this callback, e.g. to connect "
        "the call's media to sound device."
    },
    {
	"on_dtmf_digit", T_OBJECT_EX,
	offsetof(PyObj_pjsua_callback, on_dtmf_digit), 0,
	"Notify application upon receiving incoming DTMF digit."
    },
    {
        "on_call_transfer_request", T_OBJECT_EX,
        offsetof(PyObj_pjsua_callback, on_call_transfer_request), 0,
        "Notify application on call being transfered. "
	"Application can decide to accept/reject transfer request "
	"by setting the code (default is 200). When this callback "
	"is not defined, the default behavior is to accept the "
	"transfer."
    },
    {
        "on_call_transfer_status", T_OBJECT_EX,
        offsetof(PyObj_pjsua_callback, on_call_transfer_status), 0,
        "Notify application of the status of previously sent call "
        "transfer request. Application can monitor the status of the "
        "call transfer request, for example to decide whether to "
        "terminate existing call."
    },
    {
        "on_call_replace_request", T_OBJECT_EX,
        offsetof(PyObj_pjsua_callback, on_call_replace_request), 0,
        "Notify application about incoming INVITE with Replaces header. "
        "Application may reject the request by setting non-2xx code."
    },
    {
        "on_call_replaced", T_OBJECT_EX,
        offsetof(PyObj_pjsua_callback, on_call_replaced), 0,
	"Notify application that an existing call has been replaced with "
	"a new call. This happens when PJSUA-API receives incoming INVITE "
	"request with Replaces header."
	" "
	"After this callback is called, normally PJSUA-API will disconnect "
	"old_call_id and establish new_call_id."
    },
    {
        "on_reg_state", T_OBJECT_EX,
        offsetof(PyObj_pjsua_callback, on_reg_state), 0,
        "Notify application when registration status has changed. Application "
        "may then query the account info to get the registration details."
    },
    {
        "on_buddy_state", T_OBJECT_EX,
        offsetof(PyObj_pjsua_callback, on_buddy_state), 0,
        "Notify application when the buddy state has changed. Application may "
        "then query the buddy into to get the details."
    },
    {
        "on_pager", T_OBJECT_EX, 
	offsetof(PyObj_pjsua_callback, on_pager), 0,
        "Notify application on incoming pager (i.e. MESSAGE request). "
        "Argument call_id will be -1 if MESSAGE request is not related to an "
        "existing call."
    },
    {
        "on_pager_status", T_OBJECT_EX,
        offsetof(PyObj_pjsua_callback, on_pager_status), 0,
        "Notify application about the delivery status of outgoing pager "
        "request."
    },
    {
        "on_typing", T_OBJECT_EX, 
	offsetof(PyObj_pjsua_callback, on_typing), 0,
        "Notify application about typing indication."
    },
    {NULL}  /* Sentinel */
};


/*
 * PyTyp_pjsua_callback
 * callback class definition
 */
static PyTypeObject PyTyp_pjsua_callback =
{
    PyObject_HEAD_INIT(NULL)
    0,					/*ob_size*/
    "py_pjsua.Callback",		/*tp_name*/
    sizeof(PyObj_pjsua_callback),	/*tp_basicsize*/
    0,					/*tp_itemsize*/
    (destructor)PyObj_pjsua_callback_delete,   /*tp_dealloc*/
    0,                             	/*tp_print*/
    0,                             	/*tp_getattr*/
    0,                             	/*tp_setattr*/
    0,                             	/*tp_compare*/
    0,                             	/*tp_repr*/
    0,                             	/*tp_as_number*/
    0,                             	/*tp_as_sequence*/
    0,                             	/*tp_as_mapping*/
    0,                             	/*tp_hash */
    0,                             	/*tp_call*/
    0,                             	/*tp_str*/
    0,                             	/*tp_getattro*/
    0,                             	/*tp_setattro*/
    0,                             	/*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,            	/*tp_flags*/
    "This structure describes application callback "
    "to receive various event notifications from "
    "PJSUA-API",			/* tp_doc */
    0,                           	/* tp_traverse */
    0,                           	/* tp_clear */
    0,                           	/* tp_richcompare */
    0,                           	/* tp_weaklistoffset */
    0,                           	/* tp_iter */
    0,                           	/* tp_iternext */
    0,                 			/* tp_methods */
    PyObj_pjsua_callback_members,       /* tp_members */
    0,                             	/* tp_getset */
    0,                             	/* tp_base */
    0,                             	/* tp_dict */
    0,                             	/* tp_descr_get */
    0,                             	/* tp_descr_set */
    0,                             	/* tp_dictoffset */
    0,          			/* tp_init */
    0,                             	/* tp_alloc */
    PyObj_pjsua_callback_new,           /* tp_new */

};


//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsua_media_config
 * C/Python wrapper for pjsua_media_config object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    unsigned clock_rate;
    unsigned max_media_ports;
    int	     has_ioqueue;
    unsigned thread_cnt;
    unsigned quality;
    unsigned ptime;
    int	     no_vad;
    unsigned ilbc_mode;
    unsigned tx_drop_pct;
    unsigned rx_drop_pct;
    unsigned ec_options;
    unsigned ec_tail_len;
} PyObj_pjsua_media_config;


/*
 * PyObj_pjsua_media_config_members
 * declares attributes accessible from both C and Python for media_config file
 */
static PyMemberDef PyObj_pjsua_media_config_members[] =
{
    {
        "clock_rate", T_INT, 
	offsetof(PyObj_pjsua_media_config, clock_rate), 0,
        "Clock rate to be applied to the conference bridge. If value is zero, "
        "default clock rate will be used (16KHz)."
    },
    {
        "max_media_ports", T_INT,
        offsetof(PyObj_pjsua_media_config, max_media_ports), 0,
        "Specify maximum number of media ports to be created in the "
        "conference bridge. Since all media terminate in the bridge (calls, "
        "file player, file recorder, etc), the value must be large enough to "
        "support all of them. However, the larger the value, the more "
        "computations are performed."
    },
    {
        "has_ioqueue", T_INT, 
	offsetof(PyObj_pjsua_media_config, has_ioqueue), 0,
        "Specify whether the media manager should manage its own ioqueue for "
        "the RTP/RTCP sockets. If yes, ioqueue will be created and at least "
        "one worker thread will be created too. If no, the RTP/RTCP sockets "
        "will share the same ioqueue as SIP sockets, and no worker thread is "
        "needed."
    },
    {
        "thread_cnt", T_INT, 
	offsetof(PyObj_pjsua_media_config, thread_cnt), 0,
        "Specify the number of worker threads to handle incoming RTP packets. "
        "A value of one is recommended for most applications."
    },
    {
        "quality", T_INT, 
	offsetof(PyObj_pjsua_media_config, quality), 0,
        "The media quality also sets speex codec quality/complexity to the "
        "number."
    },
    {
        "ptime", T_INT, 
	offsetof(PyObj_pjsua_media_config, ptime), 0,
        "Specify default ptime."
    },
    {
        "no_vad", T_INT, 
	offsetof(PyObj_pjsua_media_config, no_vad), 0,
        "Disable VAD?"
    },
    {
        "ilbc_mode", T_INT, 
	offsetof(PyObj_pjsua_media_config, ilbc_mode), 0,
        "iLBC mode (20 or 30)."
    },
    {
        "tx_drop_pct", T_INT, 
	offsetof(PyObj_pjsua_media_config, tx_drop_pct), 0,
        "Percentage of RTP packet to drop in TX direction (to simulate packet "
        "lost)."
    },
    {
        "rx_drop_pct", T_INT, 
	offsetof(PyObj_pjsua_media_config, rx_drop_pct), 0,
        "Percentage of RTP packet to drop in RX direction (to simulate packet "
        "lost)."},
    {
        "ec_options", T_INT, 
	offsetof(PyObj_pjsua_media_config, ec_options), 0,
        "Echo canceller options (see pjmedia_echo_create())"
    },
    {
        "ec_tail_len", T_INT, 
	offsetof(PyObj_pjsua_media_config, ec_tail_len), 0,
        "Echo canceller tail length, in miliseconds."
    },
    {NULL}  /* Sentinel */
};


/*
 * PyTyp_pjsua_media_config
 */
static PyTypeObject PyTyp_pjsua_media_config =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Media_Config",        /*tp_name*/
    sizeof(PyObj_pjsua_media_config),/*tp_basicsize*/
    0,                              /*tp_itemsize*/
    0,                              /*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Media Config objects",         /*tp_doc*/
    0,                              /*tp_traverse*/
    0,                              /*tp_clear*/
    0,                              /*tp_richcompare*/
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    PyObj_pjsua_media_config_members, /* tp_members */

};


static void PyObj_pjsua_media_config_import(PyObj_pjsua_media_config *obj,
					    const pjsua_media_config *cfg)
{
    obj->clock_rate	    = cfg->clock_rate;
    obj->max_media_ports    = cfg->max_media_ports;
    obj->has_ioqueue	    = cfg->has_ioqueue;
    obj->thread_cnt	    = cfg->thread_cnt;
    obj->quality	    = cfg->quality;
    obj->ptime		    = cfg->ptime;
    obj->no_vad		    = cfg->no_vad;
    obj->ilbc_mode	    = cfg->ilbc_mode;
    obj->tx_drop_pct	    = cfg->tx_drop_pct;
    obj->rx_drop_pct	    = cfg->rx_drop_pct;
    obj->ec_options	    = cfg->ec_options;
    obj->ec_tail_len	    = cfg->ec_tail_len;
}

static void PyObj_pjsua_media_config_export(pjsua_media_config *cfg,
					    const PyObj_pjsua_media_config *obj)
{
    cfg->clock_rate	    = obj->clock_rate;
    cfg->max_media_ports    = obj->max_media_ports;
    cfg->has_ioqueue	    = obj->has_ioqueue;
    cfg->thread_cnt	    = obj->thread_cnt;
    cfg->quality	    = obj->quality;
    cfg->ptime		    = obj->ptime;
    cfg->no_vad		    = obj->no_vad;
    cfg->ilbc_mode	    = obj->ilbc_mode;
    cfg->tx_drop_pct	    = obj->tx_drop_pct;
    cfg->rx_drop_pct	    = obj->rx_drop_pct;
    cfg->ec_options	    = obj->ec_options;
    cfg->ec_tail_len	    = obj->ec_tail_len;
}

//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsua_config
 * attribute list for config object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    unsigned		  max_calls;
    unsigned		  thread_cnt;
    PyObject		 *outbound_proxy;
    PyObject	         *stun_domain;
    PyObject		 *stun_host;
    PyObject		 *stun_relay_host;
    PyObj_pjsua_callback *cb;
    PyObject		 *user_agent;
} PyObj_pjsua_config;


static void PyObj_pjsua_config_delete(PyObj_pjsua_config* self)
{
    Py_XDECREF(self->outbound_proxy);
    Py_XDECREF(self->stun_domain);
    Py_XDECREF(self->stun_host);
    Py_XDECREF(self->stun_relay_host);
    Py_XDECREF(self->cb);
    Py_XDECREF(self->user_agent);
    self->ob_type->tp_free((PyObject*)self);
}


static void PyObj_pjsua_config_import(PyObj_pjsua_config *obj,
				      const pjsua_config *cfg)
{
    obj->max_calls	= cfg->max_calls;
    obj->thread_cnt	= cfg->thread_cnt;
    Py_XDECREF(obj->outbound_proxy);
    obj->outbound_proxy = PyString_FromStringAndSize(cfg->outbound_proxy[0].ptr,
						     cfg->outbound_proxy[0].slen);
    Py_XDECREF(obj->stun_domain);
    obj->stun_domain	= PyString_FromStringAndSize(cfg->stun_domain.ptr,
						     cfg->stun_domain.slen);
    Py_XDECREF(obj->stun_host);
    obj->stun_host	= PyString_FromStringAndSize(cfg->stun_host.ptr,
						     cfg->stun_host.slen);
    Py_XDECREF(obj->stun_relay_host);
    obj->stun_relay_host= PyString_FromStringAndSize(cfg->stun_host.ptr,
						     cfg->stun_host.slen);
    Py_XDECREF(obj->user_agent);
    obj->user_agent	= PyString_FromStringAndSize(cfg->user_agent.ptr,
						     cfg->user_agent.slen);
}


static void PyObj_pjsua_config_export(pjsua_config *cfg,
				      PyObj_pjsua_config *obj)
{
    cfg->max_calls	= obj->max_calls;
    cfg->thread_cnt	= obj->thread_cnt;
    if (PyString_Size(obj->outbound_proxy) > 0) {
	cfg->outbound_proxy_cnt = 1;
	cfg->outbound_proxy[0] = PyString_to_pj_str(obj->outbound_proxy);
    } else {
	cfg->outbound_proxy_cnt = 0;
    }
    cfg->stun_domain	= PyString_to_pj_str(obj->stun_domain);
    cfg->stun_host	= PyString_to_pj_str(obj->stun_host);
    //cfg->stun_relay_host= PyString_to_pj_str(obj->stun_host);
    cfg->user_agent	= PyString_to_pj_str(obj->user_agent);

}


static PyObject *PyObj_pjsua_config_new(PyTypeObject *type, 
					PyObject *args, 
					PyObject *kwds)
{
    PyObj_pjsua_config *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_config *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->user_agent = PyString_FromString("");
        if (self->user_agent == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        self->outbound_proxy = PyString_FromString("");
        if (self->outbound_proxy == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        self->cb = (PyObj_pjsua_callback *)
		   PyType_GenericNew(&PyTyp_pjsua_callback, NULL, NULL);
        if (self->cb == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
    }
    return (PyObject *)self;
}


/*
 * PyObj_pjsua_config_members
 * attribute list accessible from Python/C
 */
static PyMemberDef PyObj_pjsua_config_members[] =
{
    {
    	"max_calls", T_INT, 
	offsetof(PyObj_pjsua_config, max_calls), 0,
    	"Maximum calls to support (default: 4) "
    },
    {
    	"thread_cnt", T_INT, 
	offsetof(PyObj_pjsua_config, thread_cnt), 0,
    	"Number of worker threads. Normally application will want to have at "
    	"least one worker thread, unless when it wants to poll the library "
    	"periodically, which in this case the worker thread can be set to "
    	"zero."
    },
    {
    	"outbound_proxy", T_OBJECT_EX,
    	offsetof(PyObj_pjsua_config, outbound_proxy), 0,
    	"SIP URL of the outbound proxy (optional)"
    },
    {
    	"stun_domain", T_OBJECT_EX,
    	offsetof(PyObj_pjsua_config, stun_domain), 0,
    	"Domain of the STUN server (optional). STUN server will be resolved "
	"using DNS SRV resolution only when nameserver is configured. "
	"Alternatively, if DNS SRV resolution for STUN is not desired, "
	"application can specify the STUN server hostname or IP address "
	"in stun_host attribute."
    },
    {
    	"stun_host", T_OBJECT_EX,
    	offsetof(PyObj_pjsua_config, stun_host), 0,
    	"Hostname or IP address of the STUN server (optional)."
    },
    {
    	"stun_relay_host", T_OBJECT_EX,
    	offsetof(PyObj_pjsua_config, stun_relay_host), 0,
    	"Hostname or IP address of the TURN server (optional)."
    },
    {
    	"cb", T_OBJECT_EX, offsetof(PyObj_pjsua_config, cb), 0,
    	"Application callback."
    },
    {
    	"user_agent", T_OBJECT_EX, offsetof(PyObj_pjsua_config, user_agent), 0,
    	"User agent string (default empty)"
    },
    {NULL}  /* Sentinel */
};


/*
 * PyTyp_pjsua_config
 * type wrapper for config class
 */
static PyTypeObject PyTyp_pjsua_config =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.Config",         /*tp_name*/
    sizeof(PyObj_pjsua_config),/*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyObj_pjsua_config_delete,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "Config object",           /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
    PyObj_pjsua_config_members,/* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    PyObj_pjsua_config_new,    /* tp_new */

};

//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsua_logging_config
 * configuration class for logging_config object
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    int		 msg_logging;
    unsigned	 level;
    unsigned	 console_level;
    unsigned	 decor;
    PyObject	*log_filename;
    PyObject	*cb;
} PyObj_pjsua_logging_config;


/*
 * PyObj_pjsua_logging_config_delete
 * deletes a logging config from memory
 */
static void PyObj_pjsua_logging_config_delete(PyObj_pjsua_logging_config* self)
{
    Py_XDECREF(self->log_filename);
    Py_XDECREF(self->cb);
    self->ob_type->tp_free((PyObject*)self);
}


static void PyObj_pjsua_logging_config_import(PyObj_pjsua_logging_config *obj,
					      const pjsua_logging_config *cfg)
{
    obj->msg_logging	= cfg->msg_logging;
    obj->level		= cfg->level;
    obj->console_level	= cfg->console_level;
    obj->decor		= cfg->decor;
}

static void PyObj_pjsua_logging_config_export(pjsua_logging_config *cfg,
					      PyObj_pjsua_logging_config *obj)
{
    cfg->msg_logging	= obj->msg_logging;
    cfg->level		= obj->level;
    cfg->console_level	= obj->console_level;
    cfg->decor		= obj->decor;
    cfg->log_filename	= PyString_to_pj_str(obj->log_filename);
}


/*
 * PyObj_pjsua_logging_config_new
 * constructor for logging_config object
 */
static PyObject * PyObj_pjsua_logging_config_new(PyTypeObject *type, 
						 PyObject *args,
					         PyObject *kwds)
{
    PyObj_pjsua_logging_config *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_logging_config *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->log_filename = PyString_FromString("");
        if (self->log_filename == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->cb = Py_None;
        if (self->cb == NULL)
    	{
            Py_DECREF(Py_None);
            return NULL;
        }
    }

    return (PyObject *)self;
}


/*
 * PyObj_pjsua_logging_config_members
 */
static PyMemberDef PyObj_pjsua_logging_config_members[] =
{
    {
    	"msg_logging", T_INT, 
	offsetof(PyObj_pjsua_logging_config, msg_logging), 0,
    	"Log incoming and outgoing SIP message? Yes!"
    },
    {
    	"level", T_INT, 
	offsetof(PyObj_pjsua_logging_config, level), 0,
    	"Input verbosity level. Value 5 is reasonable."
    },
    {
    	"console_level", T_INT, 
	offsetof(PyObj_pjsua_logging_config, console_level),
    	0, "Verbosity level for console. Value 4 is reasonable."
    },
    {
    	"decor", T_INT, 
	 offsetof(PyObj_pjsua_logging_config, decor), 0,
    	"Log decoration"
    },
    {
    	"log_filename", T_OBJECT_EX,
    	offsetof(PyObj_pjsua_logging_config, log_filename), 0,
    	"Optional log filename"
    },
    {
    	"cb", T_OBJECT_EX, 
	offsetof(PyObj_pjsua_logging_config, cb), 0,
    	"Optional callback function to be called to write log to application "
    	"specific device. This function will be called forlog messages on "
    	"input verbosity level."
    },
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjsua_logging_config
 */
static PyTypeObject PyTyp_pjsua_logging_config =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Logging_Config",      /*tp_name*/
    sizeof(PyObj_pjsua_logging_config),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)PyObj_pjsua_logging_config_delete,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Logging Config objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    PyObj_pjsua_logging_config_members,/* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyObj_pjsua_logging_config_new, /* tp_new */

};


//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsua_msg_data
 * typewrapper for MessageData class
 * !modified @ 061206
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    PyObject * hdr_list;
    PyObject * content_type;
    PyObject * msg_body;
} PyObj_pjsua_msg_data;


/*
 * PyObj_pjsua_msg_data_delete
 * deletes a msg_data
 * !modified @ 061206
 */
static void PyObj_pjsua_msg_data_delete(PyObj_pjsua_msg_data* self)
{
    Py_XDECREF(self->hdr_list);
    Py_XDECREF(self->content_type);
    Py_XDECREF(self->msg_body);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * PyObj_pjsua_msg_data_new
 * constructor for msg_data object
 * !modified @ 061206
 */
static PyObject * PyObj_pjsua_msg_data_new(PyTypeObject *type, 
					   PyObject *args,
					   PyObject *kwds)
{
    PyObj_pjsua_msg_data *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_msg_data *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        Py_INCREF(Py_None);
        self->hdr_list = Py_None;
        if (self->hdr_list == NULL) {
            Py_DECREF(self);
            return NULL;
        }
        self->content_type = PyString_FromString("");
        if (self->content_type == NULL) {
            Py_DECREF(self);
            return NULL;
        }
        self->msg_body = PyString_FromString("");
        if (self->msg_body == NULL) {
            Py_DECREF(self);
            return NULL;
        }
    }

    return (PyObject *)self;
}


/*
 * PyObj_pjsua_msg_data_members
 * !modified @ 061206
 */
static PyMemberDef PyObj_pjsua_msg_data_members[] =
{
    {
        "hdr_list", T_OBJECT_EX, 
	offsetof(PyObj_pjsua_msg_data, hdr_list), 0, 
	"Additional message headers as linked list of strings."
    }, 
    {
	"content_type", T_OBJECT_EX, 
	offsetof(PyObj_pjsua_msg_data, content_type), 0, 
	"MIME type of optional message body."
    },
    {
    	"msg_body", T_OBJECT_EX, 
	offsetof(PyObj_pjsua_msg_data, msg_body), 0,
    	"Optional message body."
    },
    {NULL}  /* Sentinel */
};


/*
 * PyTyp_pjsua_msg_data
 */
static PyTypeObject PyTyp_pjsua_msg_data =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.Msg_Data",       /*tp_name*/
    sizeof(PyObj_pjsua_msg_data),   /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyObj_pjsua_msg_data_delete,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "msg_data objects",        /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
    PyObj_pjsua_msg_data_members,          /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    PyObj_pjsua_msg_data_new,                 /* tp_new */

};


//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsua_transport_config
 * Transport configuration for creating UDP transports for both SIP
 * and media.
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    unsigned	port;
    PyObject   *public_addr;
    PyObject   *bound_addr;
} PyObj_pjsua_transport_config;


/*
 * PyObj_pjsua_transport_config_delete
 * deletes a transport config from memory
 */
static void PyObj_pjsua_transport_config_delete(PyObj_pjsua_transport_config* self)
{
    Py_XDECREF(self->public_addr);    
    Py_XDECREF(self->bound_addr);    
    self->ob_type->tp_free((PyObject*)self);
}


static void PyObj_pjsua_transport_config_export(pjsua_transport_config *cfg,
						PyObj_pjsua_transport_config *obj)
{
    cfg->public_addr	= PyString_to_pj_str(obj->public_addr);
    cfg->bound_addr	= PyString_to_pj_str(obj->bound_addr);
    cfg->port		= obj->port;

}

static void PyObj_pjsua_transport_config_import(PyObj_pjsua_transport_config *obj,
						const pjsua_transport_config *cfg)
{
    Py_XDECREF(obj->public_addr);    
    obj->public_addr = PyString_FromStringAndSize(cfg->public_addr.ptr, 
						  cfg->public_addr.slen);

    Py_XDECREF(obj->bound_addr);    
    obj->bound_addr = PyString_FromStringAndSize(cfg->bound_addr.ptr, 
					         cfg->bound_addr.slen);

    obj->port = cfg->port;
}


/*
 * PyObj_pjsua_transport_config_new
 * constructor for transport_config object
 */
static PyObject * PyObj_pjsua_transport_config_new(PyTypeObject *type, 
						   PyObject *args,
						   PyObject *kwds)
{
    PyObj_pjsua_transport_config *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_transport_config *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->public_addr = PyString_FromString("");
        if (self->public_addr == NULL) {
            Py_DECREF(self);
            return NULL;
        }
	self->bound_addr = PyString_FromString("");
        if (self->bound_addr == NULL) {
            Py_DECREF(self);
            return NULL;
        }
    }

    return (PyObject *)self;
}


/*
 * PyObj_pjsua_transport_config_members
 */
static PyMemberDef PyObj_pjsua_transport_config_members[] =
{
    {
        "port", T_INT, 
	offsetof(PyObj_pjsua_transport_config, port), 0,
        "UDP port number to bind locally. This setting MUST be specified "
        "even when default port is desired. If the value is zero, the "
        "transport will be bound to any available port, and application "
        "can query the port by querying the transport info."
    },
    {
        "public_addr", T_OBJECT_EX, 
	offsetof(PyObj_pjsua_transport_config, public_addr), 0,
        "Optional address to advertise as the address of this transport. "
        "Application can specify any address or hostname for this field, "
        "for example it can point to one of the interface address in the "
        "system, or it can point to the public address of a NAT router "
        "where port mappings have been configured for the application."		
    },    
    {
        "bound_addr", T_OBJECT_EX, 
        offsetof(PyObj_pjsua_transport_config, bound_addr), 0,
        "Optional address where the socket should be bound to. This option "
        "SHOULD only be used to selectively bind the socket to particular "
        "interface (instead of 0.0.0.0), and SHOULD NOT be used to set the "
        "published address of a transport (the public_addr field should be "
        "used for that purpose)."		
    },    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjsua_transport_config
 */
static PyTypeObject PyTyp_pjsua_transport_config =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Transport_Config",    /*tp_name*/
    sizeof(PyObj_pjsua_transport_config),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)PyObj_pjsua_transport_config_delete,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Transport setting",	    /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    PyObj_pjsua_transport_config_members,/* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyObj_pjsua_transport_config_new,/* tp_new */
};


//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsua_transport_info
 * Transport info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    int		 id;
    int		 type;
    PyObject	*type_name;
    PyObject	*info;
    unsigned	 flag;
    PyObject	*addr;
    unsigned	 port;
    unsigned	 usage_count;
} PyObj_pjsua_transport_info;


/*
 * PyObj_pjsua_transport_info_delete
 * deletes a transport info from memory
 */
static void PyObj_pjsua_transport_info_delete(PyObj_pjsua_transport_info* self)
{
    Py_XDECREF(self->type_name); 
    Py_XDECREF(self->info);
    Py_XDECREF(self->addr);
    self->ob_type->tp_free((PyObject*)self);
}


static void PyObj_pjsua_transport_info_import(PyObj_pjsua_transport_info *obj,
					      const pjsua_transport_info *info)
{
    obj->id	    = info->id;
    obj->type	    = info->type;
    obj->type_name  = PyString_FromStringAndSize(info->type_name.ptr,
						 info->type_name.slen);
    obj->info	    = PyString_FromStringAndSize(info->info.ptr,
						 info->info.slen);
    obj->flag	    = info->flag;
    obj->addr	    = PyString_FromStringAndSize(info->local_name.host.ptr,
						 info->local_name.host.slen);
    obj->port	    = info->local_name.port;
    obj->usage_count= info->usage_count;
}

/*
 * PyObj_pjsua_transport_info_new
 * constructor for transport_info object
 */
static PyObject * PyObj_pjsua_transport_info_new(PyTypeObject *type, 
						 PyObject *args,
						 PyObject *kwds)
{
    PyObj_pjsua_transport_info *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_transport_info *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->type_name = PyString_FromString("");
        if (self->type_name == NULL) {
            Py_DECREF(self);
            return NULL;
        }
        self->info = PyString_FromString(""); 
        if (self->info == NULL) {
            Py_DECREF(self);
            return NULL;
        }
        self->addr = PyString_FromString("");
        if (self->addr == NULL) {
            Py_DECREF(self);
            return NULL;
        }
    }

    return (PyObject *)self;
}


/*
 * PyObj_pjsua_transport_info_members
 */
static PyMemberDef PyObj_pjsua_transport_info_members[] =
{
    {
        "id", T_INT, 
	offsetof(PyObj_pjsua_transport_info, id), 0,
        "PJSUA transport identification."
    },
    {
        "type", T_INT, 
	offsetof(PyObj_pjsua_transport_info, id), 0,
        "Transport type."
    },
    {
        "type_name", T_OBJECT_EX,
        offsetof(PyObj_pjsua_transport_info, type_name), 0,
        "Transport type name."
    },
    {
        "info", T_OBJECT_EX,
        offsetof(PyObj_pjsua_transport_info, info), 0,
        "Transport string info/description."
    },
    {
        "flag", T_INT, 
	offsetof(PyObj_pjsua_transport_info, flag), 0,
        "Transport flag (see ##pjsip_transport_flags_e)."
    },
    {
        "addr", T_OBJECT_EX,
        offsetof(PyObj_pjsua_transport_info, addr), 0,
        "Published address (or transport address name)."
    },
    {
        "port", T_INT,
        offsetof(PyObj_pjsua_transport_info, port), 0,
        "Published port number."
    },
    {
        "usage_count", T_INT, 
	offsetof(PyObj_pjsua_transport_info, usage_count), 0,
        "Current number of objects currently referencing this transport."
    },    
    {NULL}  /* Sentinel */
};


/*
 * PyTyp_pjsua_transport_info
 */
static PyTypeObject PyTyp_pjsua_transport_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Transport_Info",      /*tp_name*/
    sizeof(PyObj_pjsua_transport_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)PyObj_pjsua_transport_info_delete,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Transport Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    PyObj_pjsua_transport_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyObj_pjsua_transport_info_new, /* tp_new */

};


//////////////////////////////////////////////////////////////////////////////

/*
 * PyObj_pjsua_acc_config
 * Acc Config
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    int		     priority;	
    PyObject	    *id;
    PyObject	    *reg_uri;
    int		     publish_enabled;
    PyObject	    *force_contact;
    /*pj_str_t proxy[8];*/
    PyListObject    *proxy;
    unsigned	     reg_timeout;
    /*pjsip_cred_info cred_info[8];*/
    PyListObject    *cred_info;
    int		     transport_id;
} PyObj_pjsua_acc_config;


/*
 * PyObj_pjsua_acc_config_delete
 * deletes a acc_config from memory
 */
static void PyObj_pjsua_acc_config_delete(PyObj_pjsua_acc_config* self)
{
    Py_XDECREF(self->id); 
    Py_XDECREF(self->reg_uri);
    Py_XDECREF(self->force_contact);	
    Py_XDECREF(self->proxy);
    Py_XDECREF(self->cred_info);
    self->ob_type->tp_free((PyObject*)self);
}


static void PyObj_pjsua_acc_config_import(PyObj_pjsua_acc_config *obj,
					  const pjsua_acc_config *cfg)
{
    unsigned i;

    obj->priority   = cfg->priority;
    Py_XDECREF(obj->id);
    obj->id	    = PyString_FromStringAndSize(cfg->id.ptr, cfg->id.slen);
    Py_XDECREF(obj->reg_uri);
    obj->reg_uri    = PyString_FromStringAndSize(cfg->reg_uri.ptr, 
						 cfg->reg_uri.slen);
    obj->publish_enabled = cfg->publish_enabled;
    Py_XDECREF(obj->force_contact);
    obj->force_contact = PyString_FromStringAndSize(cfg->force_contact.ptr,
						    cfg->force_contact.slen);
    Py_XDECREF(obj->proxy);
    obj->proxy = (PyListObject *)PyList_New(0);
    for (i=0; i<cfg->proxy_cnt; ++i) {
	PyObject * str;
	str = PyString_FromStringAndSize(cfg->proxy[i].ptr, 
					 cfg->proxy[i].slen);
	PyList_Append((PyObject *)obj->proxy, str);
    }

    obj->reg_timeout = cfg->reg_timeout;

    Py_XDECREF(obj->cred_info);
    obj->cred_info = (PyListObject *)PyList_New(0);
    for (i=0; i<cfg->cred_count; ++i) {
	PyObj_pjsip_cred_info * ci;
	
	ci = (PyObj_pjsip_cred_info *)
	     PyObj_pjsip_cred_info_new(&PyTyp_pjsip_cred_info,NULL,NULL);
	PyObj_pjsip_cred_info_import(ci, &cfg->cred_info[i]);
	PyList_Append((PyObject *)obj->cred_info, (PyObject *)ci);
    }

    obj->transport_id = cfg->transport_id;
}

static void PyObj_pjsua_acc_config_export(pjsua_acc_config *cfg,
					  PyObj_pjsua_acc_config *obj)
{
    unsigned i;

    cfg->priority   = obj->priority;
    cfg->id	    = PyString_to_pj_str(obj->id);
    cfg->reg_uri    = PyString_to_pj_str(obj->reg_uri);
    cfg->publish_enabled = obj->publish_enabled;
    cfg->force_contact = PyString_to_pj_str(obj->force_contact);

    cfg->proxy_cnt = PyList_Size((PyObject*)obj->proxy);
    for (i = 0; i < cfg->proxy_cnt; i++) {
        /*cfg.proxy[i] = ac->proxy[i];*/
        cfg->proxy[i] = PyString_to_pj_str(PyList_GetItem((PyObject *)obj->proxy,i));
    }

    cfg->reg_timeout = obj->reg_timeout;

    cfg->cred_count = PyList_Size((PyObject*)obj->cred_info);
    for (i = 0; i < cfg->cred_count; i++) {
        /*cfg.cred_info[i] = ac->cred_info[i];*/
        PyObj_pjsip_cred_info *ci;
	ci = (PyObj_pjsip_cred_info*) 
	     PyList_GetItem((PyObject *)obj->cred_info,i);
	PyObj_pjsip_cred_info_export(&cfg->cred_info[i], ci);
    }

    cfg->transport_id = obj->transport_id;
}


/*
 * PyObj_pjsua_acc_config_new
 * constructor for acc_config object
 */
static PyObject * PyObj_pjsua_acc_config_new(PyTypeObject *type, 
					     PyObject *args,
					     PyObject *kwds)
{
    PyObj_pjsua_acc_config *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_acc_config *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->id = PyString_FromString("");
        if (self->id == NULL) {
            Py_DECREF(self);
            return NULL;
        }
        self->reg_uri = PyString_FromString("");
        if (self->reg_uri == NULL) {
            Py_DECREF(self);
            return NULL;
        }
        self->force_contact = PyString_FromString("");
        if (self->force_contact == NULL) {
            Py_DECREF(self);
            return NULL;
        }
	self->proxy = (PyListObject *)PyList_New(0);
	if (self->proxy == NULL) {
	    Py_DECREF(self);
	    return NULL;
	}
	self->cred_info = (PyListObject *)PyList_New(0);
	if (self->cred_info == NULL) {
	    Py_DECREF(self);
	    return NULL;
	}
    }

    return (PyObject *)self;
}



/*
 * PyObj_pjsua_acc_config_members
 */
static PyMemberDef PyObj_pjsua_acc_config_members[] =
{
    {
        "priority", T_INT, offsetof(PyObj_pjsua_acc_config, priority), 0,
        "Account priority, which is used to control the order of matching "
        "incoming/outgoing requests. The higher the number means the higher "
        "the priority is, and the account will be matched first. "
    },
    {
        "id", T_OBJECT_EX,
        offsetof(PyObj_pjsua_acc_config, id), 0,
        "The full SIP URL for the account. "
        "The value can take name address or URL format, "
        "and will look something like 'sip:account@serviceprovider'. "
        "This field is mandatory."
    },
    {
        "reg_uri", T_OBJECT_EX,
        offsetof(PyObj_pjsua_acc_config, reg_uri), 0,
        "This is the URL to be put in the request URI for the registration, "
        "and will look something like 'sip:serviceprovider'. "
        "This field should be specified if registration is desired. "
        "If the value is empty, no account registration will be performed. "
    },
    {
        "publish_enabled", T_INT, 
        offsetof(PyObj_pjsua_acc_config, publish_enabled), 0,
        "Publish presence? "
    },
    {
        "force_contact", T_OBJECT_EX,
        offsetof(PyObj_pjsua_acc_config, force_contact), 0,
        "Optional URI to be put as Contact for this account. "
        "It is recommended that this field is left empty, "
        "so that the value will be calculated automatically "
        "based on the transport address. "
    },
    {
        "proxy", T_OBJECT_EX,
        offsetof(PyObj_pjsua_acc_config, proxy), 0,
        "Optional URI of the proxies to be visited for all outgoing requests "
	"that are using this account (REGISTER, INVITE, etc). Application need "
	"to specify these proxies if the service provider requires "
	"that requests destined towards its network should go through certain "
	"proxies first (for example, border controllers)."
    },
    {
        "reg_timeout", T_INT, offsetof(PyObj_pjsua_acc_config, reg_timeout), 0,
        "Optional interval for registration, in seconds. "
        "If the value is zero, default interval will be used "
        "(PJSUA_REG_INTERVAL, 55 seconds). "
    },
    {
        "cred_info", T_OBJECT_EX,
        offsetof(PyObj_pjsua_acc_config, cred_info), 0,
        "Array of credentials. If registration is desired, normally there "
	"should be at least one credential specified, to successfully "
	"authenticate against the service provider. More credentials can "
	"be specified, for example when the requests are expected to be "
	"challenged by the proxies in the route set."
    },
    {
	"transport_id", T_INT,
	offsetof(PyObj_pjsua_acc_config, transport_id), 0,
	"Optionally bind this account to specific transport. This normally is"
	" not a good idea, as account should be able to send requests using"
	" any available transports according to the destination. But some"
	" application may want to have explicit control over the transport to"
	" use, so in that case it can set this field."
    },
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjsua_acc_config
 */
static PyTypeObject PyTyp_pjsua_acc_config =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Acc_Config",      /*tp_name*/
    sizeof(PyObj_pjsua_acc_config),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)PyObj_pjsua_acc_config_delete,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Acc Config objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0/*acc_config_methods*/,                              /* tp_methods */
    PyObj_pjsua_acc_config_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyObj_pjsua_acc_config_new,             /* tp_new */

};


//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsua_acc_info
 * Acc Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    int		 id;	
    int		 is_default;
    PyObject	*acc_uri;
    int		 has_registration;
    int		 expires;
    int		 status;
    PyObject	*status_text;
    int		 online_status;	
    PyObject	*online_status_text;
} PyObj_pjsua_acc_info;


/*
 * PyObj_pjsua_acc_info_delete
 * deletes a acc_info from memory
 */
static void PyObj_pjsua_acc_info_delete(PyObj_pjsua_acc_info* self)
{
    Py_XDECREF(self->acc_uri);
    Py_XDECREF(self->status_text);
    Py_XDECREF(self->online_status_text);
    self->ob_type->tp_free((PyObject*)self);
}


static void PyObj_pjsua_acc_info_import(PyObj_pjsua_acc_info *obj,
					const pjsua_acc_info *info)
{
    obj->id	    = info->id;
    obj->is_default = info->is_default;
    obj->acc_uri    = PyString_FromStringAndSize(info->acc_uri.ptr, 
						 info->acc_uri.slen);
    obj->has_registration = info->has_registration;
    obj->expires    = info->expires;
    obj->status	    = info->status;
    obj->status_text= PyString_FromStringAndSize(info->status_text.ptr,
						 info->status_text.slen);
    obj->online_status = info->online_status;
    obj->online_status_text = PyString_FromStringAndSize(info->online_status_text.ptr,
							 info->online_status_text.slen);
}


/*
 * PyObj_pjsua_acc_info_new
 * constructor for acc_info object
 */
static PyObject * PyObj_pjsua_acc_info_new(PyTypeObject *type, 
					   PyObject *args,
					   PyObject *kwds)
{
    PyObj_pjsua_acc_info *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_acc_info *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->acc_uri = PyString_FromString("");
        if (self->acc_uri == NULL) {
            Py_DECREF(self);
            return NULL;
        }
	self->status_text = PyString_FromString("");
        if (self->status_text == NULL) {
            Py_DECREF(self);
            return NULL;
        }
	self->online_status_text = PyString_FromString("");
        if (self->online_status_text == NULL) {
            Py_DECREF(self);
            return NULL;
        }        
    }

    return (PyObject *)self;
}

/*
 * acc_info_members
 */
static PyMemberDef acc_info_members[] =
{
    {
        "id", T_INT, 
	offsetof(PyObj_pjsua_acc_info, id), 0,
        "The account ID."
    },
    {
        "is_default", T_INT, 
	offsetof(PyObj_pjsua_acc_info, is_default), 0,
        "Flag to indicate whether this is the default account. "
    },
    {
        "acc_uri", T_OBJECT_EX,
        offsetof(PyObj_pjsua_acc_info, acc_uri), 0,
        "Account URI"
    },
    {
        "has_registration", T_INT, 
	offsetof(PyObj_pjsua_acc_info, has_registration), 0,
        "Flag to tell whether this account has registration setting "
        "(reg_uri is not empty)."
    },
    {
        "expires", T_INT, 
	offsetof(PyObj_pjsua_acc_info, expires), 0,
        "An up to date expiration interval for account registration session."
    },
    {
        "status", T_INT, 
	offsetof(PyObj_pjsua_acc_info, status), 0,
        "Last registration status code. If status code is zero, "
        "the account is currently not registered. Any other value indicates "
        "the SIP status code of the registration. "
    },
    {
        "status_text", T_OBJECT_EX,
        offsetof(PyObj_pjsua_acc_info, status_text), 0,
        "String describing the registration status."
    },
    {
        "online_status", T_INT, 
	offsetof(PyObj_pjsua_acc_info, online_status), 0,
        "Presence online status for this account. "
    },
    {
        "online_status_text", T_OBJECT_EX, 
	offsetof(PyObj_pjsua_acc_info, online_status_text), 0,
        "Presence online status text."
    },
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjsua_acc_info
 */
static PyTypeObject PyTyp_pjsua_acc_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Acc_Info",      /*tp_name*/
    sizeof(PyObj_pjsua_acc_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)PyObj_pjsua_acc_info_delete,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Acc Info objects",             /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    NULL,                           /* tp_methods */
    acc_info_members,		    /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyObj_pjsua_acc_info_new,       /* tp_new */

};


//////////////////////////////////////////////////////////////////////////////

/*
 * PyObj_pjsua_buddy_config
 * Buddy Config
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    PyObject	*uri;
    int		 subscribe;
} PyObj_pjsua_buddy_config;


/*
 * PyObj_pjsua_buddy_config_delete
 * deletes a buddy_config from memory
 */
static void PyObj_pjsua_buddy_config_delete(PyObj_pjsua_buddy_config* self)
{
    Py_XDECREF(self->uri);
    self->ob_type->tp_free((PyObject*)self);
}


static void PyObj_pjsua_buddy_config_import(PyObj_pjsua_buddy_config *obj,
					    const pjsua_buddy_config *cfg)
{
    Py_XDECREF(obj->uri);
    obj->uri = PyString_FromStringAndSize(cfg->uri.ptr, cfg->uri.slen);
    obj->subscribe = cfg->subscribe;
}


static void PyObj_pjsua_buddy_config_export(pjsua_buddy_config *cfg,
					    PyObj_pjsua_buddy_config *obj)
{
    cfg->uri = PyString_to_pj_str(obj->uri);
    cfg->subscribe = obj->subscribe;
}



/*
 * PyObj_pjsua_buddy_config_new
 * constructor for buddy_config object
 */
static PyObject *PyObj_pjsua_buddy_config_new(PyTypeObject *type, 
					      PyObject *args,
					      PyObject *kwds)
{
    PyObj_pjsua_buddy_config *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_buddy_config *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->uri = PyString_FromString("");
        if (self->uri == NULL) {
            Py_DECREF(self);
            return NULL;
        }        
    }
    return (PyObject *)self;
}

/*
 * PyObj_pjsua_buddy_config_members
 */
static PyMemberDef PyObj_pjsua_buddy_config_members[] =
{
    
    {
        "uri", T_OBJECT_EX,
        offsetof(PyObj_pjsua_buddy_config, uri), 0,
        "TBuddy URL or name address."        
    },
    
    {
        "subscribe", T_INT, 
        offsetof(PyObj_pjsua_buddy_config, subscribe), 0,
        "Specify whether presence subscription should start immediately. "
    },
    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjsua_buddy_config
 */
static PyTypeObject PyTyp_pjsua_buddy_config =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Buddy_Config",        /*tp_name*/
    sizeof(PyObj_pjsua_buddy_config),/*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)PyObj_pjsua_buddy_config_delete,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Buddy Config objects",         /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    PyObj_pjsua_buddy_config_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyObj_pjsua_buddy_config_new,   /* tp_new */

};

//////////////////////////////////////////////////////////////////////////////
/*
 * PyObj_pjsua_buddy_info
 * Buddy Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    int		 id;
    PyObject	*uri;
    PyObject	*contact;
    int		 status;
    PyObject	*status_text;
    int		 monitor_pres;
} PyObj_pjsua_buddy_info;


/*
 * PyObj_pjsua_buddy_info_delete
 * deletes a buddy_info from memory
 * !modified @ 071206
 */
static void PyObj_pjsua_buddy_info_delete(PyObj_pjsua_buddy_info* self)
{
    Py_XDECREF(self->uri);
    Py_XDECREF(self->contact);
    Py_XDECREF(self->status_text);
    
    self->ob_type->tp_free((PyObject*)self);
}


static void PyObj_pjsua_buddy_info_import(PyObj_pjsua_buddy_info *obj,
					  const pjsua_buddy_info *info)
{
    obj->id = info->id;
    Py_XDECREF(obj->uri);
    obj->uri = PyString_FromStringAndSize(info->uri.ptr, info->uri.slen);
    Py_XDECREF(obj->contact);
    obj->contact = PyString_FromStringAndSize(info->contact.ptr, info->contact.slen);
    obj->status = info->status;
    Py_XDECREF(obj->status_text);
    obj->status_text = PyString_FromStringAndSize(info->status_text.ptr, 
						  info->status_text.slen);
    obj->monitor_pres = info->monitor_pres;
}


/*
 * PyObj_pjsua_buddy_info_new
 * constructor for buddy_info object
 * !modified @ 071206
 */
static PyObject * PyObj_pjsua_buddy_info_new(PyTypeObject *type, 
					     PyObject *args,
					     PyObject *kwds)
{
    PyObj_pjsua_buddy_info *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_buddy_info *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->uri = PyString_FromString("");
        if (self->uri == NULL) {
            Py_DECREF(self);
            return NULL;
        }        
	self->contact = PyString_FromString("");
        if (self->contact == NULL) {
            Py_DECREF(self);
            return NULL;
        }
	self->status_text = PyString_FromString("");
        if (self->status_text == NULL) {
            Py_DECREF(self);
            return NULL;
        }
	
    }
    return (PyObject *)self;
}

/*
 * PyObj_pjsua_buddy_info_members
 * !modified @ 071206
 */
static PyMemberDef PyObj_pjsua_buddy_info_members[] =
{
    {
        "id", T_INT, 
        offsetof(PyObj_pjsua_buddy_info, id), 0,
        "The buddy ID."
    },
    {
        "uri", T_OBJECT_EX,
        offsetof(PyObj_pjsua_buddy_info, uri), 0,
        "The full URI of the buddy, as specified in the configuration. "        
    },
    {
        "contact", T_OBJECT_EX,
        offsetof(PyObj_pjsua_buddy_info, contact), 0,
        "Buddy's Contact, only available when presence subscription "
        "has been established to the buddy."        
    },
    {
        "status", T_INT, 
        offsetof(PyObj_pjsua_buddy_info, status), 0,
        "Buddy's online status. "
    },
    {
        "status_text", T_OBJECT_EX,
        offsetof(PyObj_pjsua_buddy_info, status_text), 0,
        "Text to describe buddy's online status."        
    },
    {
        "monitor_pres", T_INT, 
        offsetof(PyObj_pjsua_buddy_info, monitor_pres), 0,
        "Flag to indicate that we should monitor the presence information "
        "for this buddy (normally yes, unless explicitly disabled). "
    },
    
    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjsua_buddy_info
 */
static PyTypeObject PyTyp_pjsua_buddy_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Buddy_Info",      /*tp_name*/
    sizeof(PyObj_pjsua_buddy_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)PyObj_pjsua_buddy_info_delete,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Buddy Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    PyObj_pjsua_buddy_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyObj_pjsua_buddy_info_new,             /* tp_new */

};





#endif	/* __PY_PJSUA_H__ */


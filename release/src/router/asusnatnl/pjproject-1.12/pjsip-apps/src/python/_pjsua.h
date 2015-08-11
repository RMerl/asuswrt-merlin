/* $Id: _pjsua.h 3553 2011-05-05 06:14:19Z nanang $ */
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


PJ_INLINE(pj_str_t) PyString_ToPJ(const PyObject *obj)
{
    pj_str_t str;

    if (obj && PyString_Check(obj)) {
	str.ptr = PyString_AS_STRING(obj);
	str.slen = PyString_GET_SIZE(obj);
    } else {
	str.ptr = NULL;
	str.slen = 0;
    }

    return str;
}

PJ_INLINE(PyObject*) PyString_FromPJ(const pj_str_t *str)
{
    return PyString_FromStringAndSize(str->ptr, str->slen);
}

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
    obj->realm = PyString_FromPJ(&cfg->realm);
    Py_XDECREF(obj->scheme);
    obj->scheme = PyString_FromPJ(&cfg->scheme);
    Py_XDECREF(obj->username);
    obj->username = PyString_FromPJ(&cfg->username);
    obj->data_type = cfg->data_type;
    Py_XDECREF(obj->data);
    obj->data = PyString_FromPJ(&cfg->data);
}

static void PyObj_pjsip_cred_info_export(pjsip_cred_info *cfg,
					 PyObj_pjsip_cred_info *obj)
{
    cfg->realm	= PyString_ToPJ(obj->realm);
    cfg->scheme	= PyString_ToPJ(obj->scheme);
    cfg->username = PyString_ToPJ(obj->username);
    cfg->data_type = obj->data_type;
    cfg->data	= PyString_ToPJ(obj->data);
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
    if (self != NULL) {
        self->realm = PyString_FromString("");
        self->scheme = PyString_FromString("");
        self->username = PyString_FromString("");
	self->data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
        self->data = PyString_FromString("");
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
    "_pjsua.Pjsip_Cred_Info",      /*tp_name*/
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
    PyObj_pjsip_cred_info_members,  /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyObj_pjsip_cred_info_new,      /* tp_new */

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
    PyObject * on_incoming_subscribe;
    PyObject * on_buddy_state;
    PyObject * on_pager;
    PyObject * on_pager_status;
    PyObject * on_typing;
    PyObject * on_mwi_info;
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
    Py_XDECREF(self->on_incoming_subscribe);
    Py_XDECREF(self->on_buddy_state);
    Py_XDECREF(self->on_pager);
    Py_XDECREF(self->on_pager_status);
    Py_XDECREF(self->on_typing);
    Py_XDECREF(self->on_mwi_info);
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
    if (self != NULL) {
        self->on_call_state = Py_BuildValue("");
        self->on_incoming_call = Py_BuildValue("");
        self->on_call_media_state = Py_BuildValue("");
        self->on_dtmf_digit = Py_BuildValue("");
        self->on_call_transfer_request = Py_BuildValue("");
        self->on_call_transfer_status = Py_BuildValue("");
        self->on_call_replace_request = Py_BuildValue("");
        self->on_call_replaced = Py_BuildValue("");
        self->on_reg_state = Py_BuildValue("");
        self->on_incoming_subscribe = Py_BuildValue("");
        self->on_buddy_state = Py_BuildValue("");
        self->on_pager = Py_BuildValue("");
        self->on_pager_status = Py_BuildValue("");
        self->on_typing = Py_BuildValue("");
	self->on_mwi_info = Py_BuildValue("");
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
        "on_incoming_subscribe", T_OBJECT_EX,
        offsetof(PyObj_pjsua_callback, on_incoming_subscribe), 0,
        "Notification when incoming SUBSCRIBE request is received."
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
    {
        "on_mwi_info", T_OBJECT_EX, 
	offsetof(PyObj_pjsua_callback, on_mwi_info), 0,
        "Notify application about MWI indication."
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
    "_pjsua.Callback",		/*tp_name*/
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
    unsigned snd_clock_rate;
    unsigned channel_count;
    unsigned audio_frame_ptime;
    int	     snd_auto_close_time;
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
    int	     jb_min;
    int	     jb_max;
    int	     enable_ice;
    int	     enable_turn;
    PyObject *turn_server;
    int	     turn_conn_type;
    PyObject *turn_realm;
    PyObject *turn_username;
    int	     turn_passwd_type;
    PyObject *turn_passwd;
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
        "snd_clock_rate", T_INT, 
	offsetof(PyObj_pjsua_media_config, snd_clock_rate), 0,
        "Specify different clock rate of sound device, otherwise 0."
    },
    {
        "channel_count", T_INT, 
	offsetof(PyObj_pjsua_media_config, channel_count), 0,
        "Specify channel count (default 1)."
    },
    {
        "audio_frame_ptime", T_INT, 
	offsetof(PyObj_pjsua_media_config, audio_frame_ptime), 0,
        "Audio frame length in milliseconds."
    },
    {
        "snd_auto_close_time", T_INT, 
	offsetof(PyObj_pjsua_media_config, snd_auto_close_time), 0,
        "Sound idle time before it's closed."
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
    {
        "jb_min", T_INT, 
	offsetof(PyObj_pjsua_media_config, jb_min), 0,
        "Jitter buffer minimum size in milliseconds."
    },
    {
        "jb_max", T_INT, 
	offsetof(PyObj_pjsua_media_config, jb_max), 0,
        "Jitter buffer maximum size in milliseconds."
    },
    {
	"enable_ice", T_INT,
	offsetof(PyObj_pjsua_media_config, enable_ice), 0,
        "Enable ICE."
    },
    {
	"enable_turn", T_INT,
	offsetof(PyObj_pjsua_media_config, enable_turn), 0,
        "Enable TURN."
    },
    {
    	"turn_server", T_OBJECT_EX,
    	offsetof(PyObj_pjsua_media_config, turn_server), 0,
    	"Specify the TURN server."
    },
    {
	"turn_conn_type", T_INT,
	offsetof(PyObj_pjsua_media_config, turn_conn_type), 0,
        "Specify TURN connection type."
    },
    {
    	"turn_realm", T_OBJECT_EX,
    	offsetof(PyObj_pjsua_media_config, turn_realm), 0,
    	"Specify the TURN realm."
    },
    {
    	"turn_username", T_OBJECT_EX,
    	offsetof(PyObj_pjsua_media_config, turn_username), 0,
    	"Specify the TURN username."
    },
    {
	"turn_passwd_type", T_INT,
	offsetof(PyObj_pjsua_media_config, turn_passwd_type), 0,
        "Specify TURN password type."
    },
    {
    	"turn_passwd", T_OBJECT_EX,
    	offsetof(PyObj_pjsua_media_config, turn_passwd), 0,
    	"Specify the TURN password."
    },

    {NULL}  /* Sentinel */
};


static PyObject *PyObj_pjsua_media_config_new(PyTypeObject *type, 
					      PyObject *args, 
					      PyObject *kwds)
{
    PyObj_pjsua_media_config *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_media_config*)type->tp_alloc(type, 0);
    if (self != NULL) {
	self->turn_server = PyString_FromString("");
	self->turn_realm = PyString_FromString("");
	self->turn_username = PyString_FromString("");
	self->turn_passwd = PyString_FromString("");
    }

    return (PyObject *)self;
}

static void PyObj_pjsua_media_config_delete(PyObj_pjsua_media_config * self)
{
    Py_XDECREF(self->turn_server);
    Py_XDECREF(self->turn_realm);
    Py_XDECREF(self->turn_username);
    Py_XDECREF(self->turn_passwd);
    self->ob_type->tp_free((PyObject*)self);
}

static void PyObj_pjsua_media_config_import(PyObj_pjsua_media_config *obj,
					    const pjsua_media_config *cfg)
{
    obj->clock_rate	    = cfg->clock_rate;
    obj->snd_clock_rate	    = cfg->snd_clock_rate;
    obj->channel_count	    = cfg->channel_count;
    obj->audio_frame_ptime  = cfg->audio_frame_ptime;
    obj->snd_auto_close_time= cfg->snd_auto_close_time;
    obj->max_media_ports    = cfg->max_media_ports;
    obj->has_ioqueue	    = cfg->has_ioqueue;
    obj->thread_cnt	    = cfg->thread_cnt;
    obj->quality	    = cfg->quality;
    obj->ptime		    = cfg->ptime;
    obj->no_vad		    = cfg->no_vad;
    obj->ilbc_mode	    = cfg->ilbc_mode;
    obj->jb_min		    = cfg->jb_min_pre;
    obj->jb_max		    = cfg->jb_max;
    obj->tx_drop_pct	    = cfg->tx_drop_pct;
    obj->rx_drop_pct	    = cfg->rx_drop_pct;
    obj->ec_options	    = cfg->ec_options;
    obj->ec_tail_len	    = cfg->ec_tail_len;
    obj->enable_ice	    = cfg->enable_ice;
    obj->enable_turn	    = cfg->enable_turn;
    Py_XDECREF(obj->turn_server);
    obj->turn_server	    = PyString_FromPJ(&cfg->turn_server);
    obj->turn_conn_type	    = cfg->turn_conn_type;
    if (cfg->turn_auth_cred.type == PJ_STUN_AUTH_CRED_STATIC) {
	const pj_stun_auth_cred *cred = &cfg->turn_auth_cred;

	Py_XDECREF(obj->turn_realm);
	obj->turn_realm	= PyString_FromPJ(&cred->data.static_cred.realm);
	Py_XDECREF(obj->turn_username);
	obj->turn_username = PyString_FromPJ(&cred->data.static_cred.username);
	obj->turn_passwd_type = cred->data.static_cred.data_type;
	Py_XDECREF(obj->turn_passwd);
	obj->turn_passwd = PyString_FromPJ(&cred->data.static_cred.data);
    } else {
	Py_XDECREF(obj->turn_realm);
	obj->turn_realm	= PyString_FromString("");
	Py_XDECREF(obj->turn_username);
	obj->turn_username = PyString_FromString("");
	obj->turn_passwd_type = 0;
	Py_XDECREF(obj->turn_passwd);
	obj->turn_passwd = PyString_FromString("");
    }
}

static void PyObj_pjsua_media_config_export(pjsua_media_config *cfg,
					    const PyObj_pjsua_media_config *obj)
{
    cfg->clock_rate	    = obj->clock_rate;
    cfg->snd_clock_rate	    = obj->snd_clock_rate;
    cfg->channel_count	    = obj->channel_count;
    cfg->audio_frame_ptime  = obj->audio_frame_ptime;
    cfg->snd_auto_close_time=obj->snd_auto_close_time;
    cfg->max_media_ports    = obj->max_media_ports;
    cfg->has_ioqueue	    = obj->has_ioqueue;
    cfg->thread_cnt	    = obj->thread_cnt;
    cfg->quality	    = obj->quality;
    cfg->ptime		    = obj->ptime;
    cfg->no_vad		    = obj->no_vad;
    cfg->jb_min_pre	    = obj->jb_min;
    cfg->jb_max		    = obj->jb_max;
    cfg->ilbc_mode	    = obj->ilbc_mode;
    cfg->tx_drop_pct	    = obj->tx_drop_pct;
    cfg->rx_drop_pct	    = obj->rx_drop_pct;
    cfg->ec_options	    = obj->ec_options;
    cfg->ec_tail_len	    = obj->ec_tail_len;
    cfg->enable_ice	    = obj->enable_ice;
    cfg->enable_turn	    = obj->enable_turn;

    if (cfg->enable_turn) {
	cfg->turn_server = PyString_ToPJ(obj->turn_server);
	cfg->turn_conn_type = obj->turn_conn_type;
	cfg->turn_auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
	cfg->turn_auth_cred.data.static_cred.realm = PyString_ToPJ(obj->turn_realm);
	cfg->turn_auth_cred.data.static_cred.username = PyString_ToPJ(obj->turn_username);
	cfg->turn_auth_cred.data.static_cred.data_type= obj->turn_passwd_type;
	cfg->turn_auth_cred.data.static_cred.data = PyString_ToPJ(obj->turn_passwd);
    }
}


/*
 * PyTyp_pjsua_media_config
 */
static PyTypeObject PyTyp_pjsua_media_config =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "_pjsua.Media_Config",        /*tp_name*/
    sizeof(PyObj_pjsua_media_config),/*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)PyObj_pjsua_media_config_delete,/*tp_dealloc*/
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
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyObj_pjsua_media_config_new,   /* tp_new */
};


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
    PyListObject	 *nameserver;
    PyObj_pjsua_callback *cb;
    PyObject		 *user_agent;
} PyObj_pjsua_config;


static void PyObj_pjsua_config_delete(PyObj_pjsua_config* self)
{
    Py_XDECREF(self->outbound_proxy);
    Py_XDECREF(self->stun_domain);
    Py_XDECREF(self->stun_host);
    Py_XDECREF(self->nameserver);
    Py_XDECREF(self->cb);
    Py_XDECREF(self->user_agent);
    self->ob_type->tp_free((PyObject*)self);
}


static void PyObj_pjsua_config_import(PyObj_pjsua_config *obj,
				      const pjsua_config *cfg)
{
    unsigned i;

    obj->max_calls	= cfg->max_calls;
    obj->thread_cnt	= cfg->thread_cnt;
    Py_XDECREF(obj->outbound_proxy);
    if (cfg->outbound_proxy_cnt)
	obj->outbound_proxy = PyString_FromPJ(&cfg->outbound_proxy[0]);
    else
	obj->outbound_proxy = PyString_FromString("");

    Py_XDECREF(obj->stun_domain);
    obj->stun_domain	= PyString_FromPJ(&cfg->stun_domain);
    Py_XDECREF(obj->stun_host);
    obj->stun_host	= PyString_FromPJ(&cfg->stun_host);
    Py_XDECREF(obj->nameserver);
    obj->nameserver = (PyListObject *)PyList_New(0);
    for (i=0; i<cfg->nameserver_count; ++i) {
	PyObject * str;
	str = PyString_FromPJ(&cfg->nameserver[i]);
	PyList_Append((PyObject *)obj->nameserver, str);
    }
    Py_XDECREF(obj->user_agent);
    obj->user_agent	= PyString_FromPJ(&cfg->user_agent);
}


static void PyObj_pjsua_config_export(pjsua_config *cfg,
				      PyObj_pjsua_config *obj)
{
    unsigned i;

    cfg->max_calls	= obj->max_calls;
    cfg->thread_cnt	= obj->thread_cnt;
    if (PyString_Size(obj->outbound_proxy) > 0) {
	cfg->outbound_proxy_cnt = 1;
	cfg->outbound_proxy[0] = PyString_ToPJ(obj->outbound_proxy);
    } else {
	cfg->outbound_proxy_cnt = 0;
    }
    cfg->nameserver_count = PyList_Size((PyObject*)obj->nameserver);
    if (cfg->nameserver_count > PJ_ARRAY_SIZE(cfg->nameserver))
	cfg->nameserver_count = PJ_ARRAY_SIZE(cfg->nameserver);
    for (i = 0; i < cfg->nameserver_count; i++) {
	PyObject *item = PyList_GetItem((PyObject *)obj->nameserver,i);
        cfg->nameserver[i] = PyString_ToPJ(item);
    }
    cfg->stun_domain	= PyString_ToPJ(obj->stun_domain);
    cfg->stun_host	= PyString_ToPJ(obj->stun_host);
    cfg->user_agent	= PyString_ToPJ(obj->user_agent);

}


static PyObject *PyObj_pjsua_config_new(PyTypeObject *type, 
					PyObject *args, 
					PyObject *kwds)
{
    PyObj_pjsua_config *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_config *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->user_agent = PyString_FromString("");
        self->outbound_proxy = PyString_FromString("");
	self->stun_domain = PyString_FromString("");
	self->stun_host = PyString_FromString("");
        self->cb = (PyObj_pjsua_callback *)
		   PyType_GenericNew(&PyTyp_pjsua_callback, NULL, NULL);
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
    	"nameserver", T_OBJECT_EX,
    	offsetof(PyObj_pjsua_config, nameserver), 0,
    	"IP address of the nameserver."
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
    "_pjsua.Config",         /*tp_name*/
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
    Py_XDECREF(obj->log_filename);
    obj->log_filename = PyString_FromPJ(&cfg->log_filename);
}

static void PyObj_pjsua_logging_config_export(pjsua_logging_config *cfg,
					      PyObj_pjsua_logging_config *obj)
{
    cfg->msg_logging	= obj->msg_logging;
    cfg->level		= obj->level;
    cfg->console_level	= obj->console_level;
    cfg->decor		= obj->decor;
    cfg->log_filename	= PyString_ToPJ(obj->log_filename);
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
    if (self != NULL) {
        self->log_filename = PyString_FromString("");
        self->cb = Py_BuildValue("");
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
    "_pjsua.Logging_Config",      /*tp_name*/
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
    if (self != NULL) {
        self->hdr_list = PyList_New(0);
        self->content_type = PyString_FromString("");
        self->msg_body = PyString_FromString("");
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
    "_pjsua.Msg_Data",       /*tp_name*/
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
    pjsua_transport_config_default(cfg);
    cfg->public_addr	= PyString_ToPJ(obj->public_addr);
    cfg->bound_addr	= PyString_ToPJ(obj->bound_addr);
    cfg->port		= obj->port;

}

static void PyObj_pjsua_transport_config_import(PyObj_pjsua_transport_config *obj,
						const pjsua_transport_config *cfg)
{
    Py_XDECREF(obj->public_addr);    
    obj->public_addr = PyString_FromPJ(&cfg->public_addr);

    Py_XDECREF(obj->bound_addr);    
    obj->bound_addr = PyString_FromPJ(&cfg->bound_addr);

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
	self->bound_addr = PyString_FromString("");
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
    "_pjsua.Transport_Config",    /*tp_name*/
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
    obj->type_name  = PyString_FromPJ(&info->type_name);
    obj->info	    = PyString_FromPJ(&info->info);
    obj->flag	    = info->flag;
    obj->addr	    = PyString_FromPJ(&info->local_name.host);
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
        self->info = PyString_FromString(""); 
        self->addr = PyString_FromString("");
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
	offsetof(PyObj_pjsua_transport_info, type), 0,
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
    "_pjsua.Transport_Info",      /*tp_name*/
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
    int		     mwi_enabled;
    PyObject	    *force_contact;
    PyListObject    *proxy;
    unsigned	     reg_timeout;
    unsigned	     reg_delay_before_refresh;
    PyListObject    *cred_info;
    int		     transport_id;
    int		     auth_initial_send;
    PyObject	    *auth_initial_algorithm;
    PyObject	    *pidf_tuple_id;
    PyObject	    *contact_params;
    PyObject	    *contact_uri_params;
    int		     require_100rel;
    int		     use_timer;
    unsigned	     timer_se;
    unsigned	     timer_min_se;
    int		     allow_contact_rewrite;
    int		     ka_interval;
    PyObject	    *ka_data;
    unsigned	     use_srtp;
    unsigned	     srtp_secure_signaling;
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
    Py_XDECREF(self->auth_initial_algorithm);
    Py_XDECREF(self->pidf_tuple_id);
    Py_XDECREF(self->contact_params);
    Py_XDECREF(self->contact_uri_params);
    Py_XDECREF(self->ka_data);
    self->ob_type->tp_free((PyObject*)self);
}


static void PyObj_pjsua_acc_config_import(PyObj_pjsua_acc_config *obj,
					  const pjsua_acc_config *cfg)
{
    unsigned i;

    obj->priority   = cfg->priority;
    Py_XDECREF(obj->id);
    obj->id	    = PyString_FromPJ(&cfg->id);
    Py_XDECREF(obj->reg_uri);
    obj->reg_uri    = PyString_FromPJ(&cfg->reg_uri);
    obj->publish_enabled = cfg->publish_enabled;
    obj->mwi_enabled = cfg->mwi_enabled;
    Py_XDECREF(obj->force_contact);
    obj->force_contact = PyString_FromPJ(&cfg->force_contact);
    Py_XDECREF(obj->proxy);
    obj->proxy = (PyListObject *)PyList_New(0);
    for (i=0; i<cfg->proxy_cnt; ++i) {
	PyObject * str;
	str = PyString_FromPJ(&cfg->proxy[i]);
	PyList_Append((PyObject *)obj->proxy, str);
    }

    obj->reg_timeout = cfg->reg_timeout;
    obj->reg_delay_before_refresh = cfg->reg_delay_before_refresh;

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

    obj->auth_initial_send = cfg->auth_pref.initial_auth;
    Py_XDECREF(obj->auth_initial_algorithm);
    obj->auth_initial_algorithm = PyString_FromPJ(&cfg->auth_pref.algorithm);
    Py_XDECREF(obj->pidf_tuple_id);
    obj->pidf_tuple_id = PyString_FromPJ(&cfg->pidf_tuple_id);
    Py_XDECREF(obj->contact_params);
    obj->contact_params = PyString_FromPJ(&cfg->contact_params);
    Py_XDECREF(obj->contact_uri_params);
    obj->contact_uri_params = PyString_FromPJ(&cfg->contact_uri_params);
    obj->require_100rel = cfg->require_100rel;
    obj->use_timer = cfg->use_timer;
    obj->timer_se = cfg->timer_setting.sess_expires;
    obj->timer_min_se = cfg->timer_setting.min_se;
    obj->allow_contact_rewrite = cfg->allow_contact_rewrite;
    obj->ka_interval = cfg->ka_interval;
    Py_XDECREF(obj->ka_data);
    obj->ka_data = PyString_FromPJ(&cfg->ka_data);
    obj->use_srtp = cfg->use_srtp;
    obj->srtp_secure_signaling = cfg->srtp_secure_signaling;
}

static void PyObj_pjsua_acc_config_export(pjsua_acc_config *cfg,
					  PyObj_pjsua_acc_config *obj)
{
    unsigned i;

    cfg->priority   = obj->priority;
    cfg->id	    = PyString_ToPJ(obj->id);
    cfg->reg_uri    = PyString_ToPJ(obj->reg_uri);
    cfg->publish_enabled = obj->publish_enabled;
    cfg->mwi_enabled = obj->mwi_enabled;
    cfg->force_contact = PyString_ToPJ(obj->force_contact);

    cfg->proxy_cnt = PyList_Size((PyObject*)obj->proxy);
    if (cfg->proxy_cnt > PJ_ARRAY_SIZE(cfg->proxy))
	cfg->proxy_cnt = PJ_ARRAY_SIZE(cfg->proxy);
    for (i = 0; i < cfg->proxy_cnt; i++) {
	PyObject *item = PyList_GetItem((PyObject *)obj->proxy, i);
        cfg->proxy[i] = PyString_ToPJ(item);
    }

    cfg->reg_timeout = obj->reg_timeout;
    cfg->reg_delay_before_refresh = obj->reg_delay_before_refresh;

    cfg->cred_count = PyList_Size((PyObject*)obj->cred_info);
    if (cfg->cred_count > PJ_ARRAY_SIZE(cfg->cred_info))
	cfg->cred_count = PJ_ARRAY_SIZE(cfg->cred_info);
    for (i = 0; i < cfg->cred_count; i++) {
        PyObj_pjsip_cred_info *ci;
	ci = (PyObj_pjsip_cred_info*) 
	     PyList_GetItem((PyObject *)obj->cred_info, i);
	PyObj_pjsip_cred_info_export(&cfg->cred_info[i], ci);
    }

    cfg->transport_id = obj->transport_id;
    cfg->auth_pref.initial_auth = obj->auth_initial_send;
    cfg->auth_pref.algorithm = PyString_ToPJ(obj->auth_initial_algorithm);
    cfg->pidf_tuple_id = PyString_ToPJ(obj->pidf_tuple_id);
    cfg->contact_params = PyString_ToPJ(obj->contact_params);
    cfg->contact_uri_params = PyString_ToPJ(obj->contact_uri_params);
    cfg->require_100rel = obj->require_100rel;
    cfg->use_timer = obj->use_timer;
    cfg->timer_setting.sess_expires = obj->timer_se;
    cfg->timer_setting.min_se = obj->timer_min_se;
    cfg->allow_contact_rewrite = obj->allow_contact_rewrite;
    cfg->ka_interval = obj->ka_interval;
    cfg->ka_data = PyString_ToPJ(obj->ka_data);
    cfg->use_srtp = obj->use_srtp;
    cfg->srtp_secure_signaling = obj->srtp_secure_signaling;
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
        self->reg_uri = PyString_FromString("");
        self->force_contact = PyString_FromString("");
	self->proxy = (PyListObject *)PyList_New(0);
	self->cred_info = (PyListObject *)PyList_New(0);
	self->auth_initial_algorithm = PyString_FromString("");
	self->pidf_tuple_id = PyString_FromString("");
	self->contact_params = PyString_FromString("");
	self->contact_uri_params = PyString_FromString("");
	self->ka_data = PyString_FromString("");
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
        "mwi_enabled", T_INT, 
        offsetof(PyObj_pjsua_acc_config, mwi_enabled), 0,
        "Enable MWI subscription "
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
        "reg_timeout", T_INT, 
	offsetof(PyObj_pjsua_acc_config, reg_timeout), 0,
        "Optional interval for registration, in seconds. "
        "If the value is zero, default interval will be used "
        "(PJSUA_REG_INTERVAL, 55 seconds). "
    },
    {
        "reg_delay_before_refresh", T_INT, 
	offsetof(PyObj_pjsua_acc_config, reg_delay_before_refresh), 0,
        "Specify the number of seconds to refresh the client registration"
        "before the registration expires."
        "(PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH, 5 seconds). "
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
    {
	"auth_initial_send", T_INT,
	offsetof(PyObj_pjsua_acc_config, auth_initial_send), 0,
	"Send empty initial authorization header."
    },
    {
	"auth_initial_algorithm", T_OBJECT_EX,
	offsetof(PyObj_pjsua_acc_config, auth_initial_algorithm), 0,
	"Specify algorithm in empty initial authorization header."
    },
    {
	"pidf_tuple_id", T_OBJECT_EX,
	offsetof(PyObj_pjsua_acc_config, pidf_tuple_id), 0,
	"PIDF tuple id."
    },
    {
	"contact_params", T_OBJECT_EX,
	offsetof(PyObj_pjsua_acc_config, contact_params), 0,
	"Additional parameters for Contact header."
    },
    {
	"contact_uri_params", T_OBJECT_EX,
	offsetof(PyObj_pjsua_acc_config, contact_uri_params), 0,
	"Additional parameters for Contact URI."
    },
    {
	"require_100rel", T_INT,
	offsetof(PyObj_pjsua_acc_config, require_100rel), 0,
	"Require reliable provisional response."
    },
    {
	"use_timer", T_INT,
	offsetof(PyObj_pjsua_acc_config, use_timer), 0,
	"Use SIP session timers? (default=1)"
	"0:inactive, 1:optional, 2:mandatory, 3:always"
    },
    {
	"timer_se", T_INT,
	offsetof(PyObj_pjsua_acc_config, timer_se), 0,
	"Session timer expiration period, in seconds."
    },
    {
	"timer_min_se", T_INT,
	offsetof(PyObj_pjsua_acc_config, timer_min_se), 0,
	"Session timer minimum expiration period, in seconds."
    },
    {
	"allow_contact_rewrite", T_INT,
	offsetof(PyObj_pjsua_acc_config, allow_contact_rewrite), 0,
	"Re-REGISTER if behind symmetric NAT."
    },
    {
	"ka_interval", T_INT,
	offsetof(PyObj_pjsua_acc_config, ka_interval), 0,
	"Keep-alive interval."
    },
    {
	"ka_data", T_OBJECT_EX,
	offsetof(PyObj_pjsua_acc_config, ka_data), 0,
	"Keep-alive data."
    },
    {
	"use_srtp", T_INT,
	offsetof(PyObj_pjsua_acc_config, use_srtp), 0,
	"Specify SRTP usage."
    },
    {
	"srtp_secure_signaling", T_INT,
	offsetof(PyObj_pjsua_acc_config, srtp_secure_signaling), 0,
	"Specify if SRTP requires secure signaling to be used."
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
    "_pjsua.Acc_Config",	    /*tp_name*/
    sizeof(PyObj_pjsua_acc_config), /*tp_basicsize*/
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
    "Account settings",		    /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    PyObj_pjsua_acc_config_members, /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyObj_pjsua_acc_config_new,     /* tp_new */

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
    Py_XDECREF(obj->acc_uri);
    obj->acc_uri    = PyString_FromPJ(&info->acc_uri);
    obj->has_registration = info->has_registration;
    obj->expires    = info->expires;
    obj->status	    = info->status;
    Py_XDECREF(obj->status_text);
    obj->status_text= PyString_FromPJ(&info->status_text);
    obj->online_status = info->online_status;
    Py_XDECREF(obj->online_status_text);
    obj->online_status_text = PyString_FromPJ(&info->online_status_text);
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
	self->status_text = PyString_FromString("");
	self->online_status_text = PyString_FromString("");
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
    "_pjsua.Acc_Info",		    /*tp_name*/
    sizeof(PyObj_pjsua_acc_info),   /*tp_basicsize*/
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
    "Account info",		    /* tp_doc */
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
    obj->uri = PyString_FromPJ(&cfg->uri);
    obj->subscribe = cfg->subscribe;
}


static void PyObj_pjsua_buddy_config_export(pjsua_buddy_config *cfg,
					    PyObj_pjsua_buddy_config *obj)
{
    cfg->uri = PyString_ToPJ(obj->uri);
    cfg->subscribe = obj->subscribe;
    cfg->user_data = NULL;
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
    "_pjsua.Buddy_Config",        /*tp_name*/
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
    "Buddy config",		    /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    PyObj_pjsua_buddy_config_members,/* tp_members */
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
    int		 activity;
    int		 sub_state;
    PyObject	*sub_term_reason;
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
    Py_XDECREF(self->sub_term_reason);
    
    self->ob_type->tp_free((PyObject*)self);
}


static void PyObj_pjsua_buddy_info_import(PyObj_pjsua_buddy_info *obj,
					  const pjsua_buddy_info *info)
{
    obj->id = info->id;
    Py_XDECREF(obj->uri);
    obj->uri = PyString_FromPJ(&info->uri);
    Py_XDECREF(obj->contact);
    obj->contact = PyString_FromPJ(&info->contact);
    obj->status = info->status;
    Py_XDECREF(obj->status_text);
    obj->status_text = PyString_FromPJ(&info->status_text);
    obj->monitor_pres = info->monitor_pres;
    obj->activity = info->rpid.activity;
    obj->sub_state = info->sub_state;
    Py_XDECREF(obj->sub_term_reason);
    obj->sub_term_reason = PyString_FromPJ(&info->sub_term_reason);
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
	self->contact = PyString_FromString("");
	self->status_text = PyString_FromString("");
	self->sub_term_reason = PyString_FromString("");
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
    {
        "activity", T_INT, 
        offsetof(PyObj_pjsua_buddy_info, activity), 0,
        "Activity type. "
    },
    {
        "sub_state", T_INT, 
        offsetof(PyObj_pjsua_buddy_info, sub_state), 0,
        "Subscription state."
    },
    {
        "sub_term_reason", T_INT, 
        offsetof(PyObj_pjsua_buddy_info, sub_term_reason), 0,
        "Subscription termination reason."
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
    "_pjsua.Buddy_Info",	    /*tp_name*/
    sizeof(PyObj_pjsua_buddy_info), /*tp_basicsize*/
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
    "Buddy Info object",	    /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    PyObj_pjsua_buddy_info_members, /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    PyObj_pjsua_buddy_info_new,     /* tp_new */

};


//////////////////////////////////////////////////////////////////////////////

/*
 * PyObj_pjsua_codec_info
 * Codec Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    PyObject * codec_id;
    pj_uint8_t priority;    
} PyObj_pjsua_codec_info;


/*
 * codec_info_dealloc
 * deletes a codec_info from memory
 */
static void codec_info_dealloc(PyObj_pjsua_codec_info* self)
{
    Py_XDECREF(self->codec_id);    
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * codec_info_new
 * constructor for codec_info object
 */
static PyObject * codec_info_new(PyTypeObject *type, PyObject *args,
                                 PyObject *kwds)
{
    PyObj_pjsua_codec_info *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_codec_info *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->codec_id = PyString_FromString("");
    }
    return (PyObject *)self;
}

/*
 * codec_info_members
 * !modified @ 071206
 */
static PyMemberDef codec_info_members[] =
{    
    {
        "codec_id", T_OBJECT_EX,
        offsetof(PyObj_pjsua_codec_info, codec_id), 0,
        "Codec unique identification."        
    },
    {
        "priority", T_INT, 
        offsetof(PyObj_pjsua_codec_info, priority), 0,
        "Codec priority (integer 0-255)."
    },

    {NULL}  /* Sentinel */
};

/*
 * PyTyp_pjsua_codec_info
 */
static PyTypeObject PyTyp_pjsua_codec_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "_pjsua.Codec_Info",	    /*tp_name*/
    sizeof(PyObj_pjsua_codec_info), /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)codec_info_dealloc, /*tp_dealloc*/
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
    "Codec Info",		    /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    codec_info_members,		    /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    codec_info_new,		    /* tp_new */

};


//////////////////////////////////////////////////////////////////////////////

/*
 * PyObj_pjsua_conf_port_info
 * Conf Port Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    int		 slot_id;
    PyObject	*name;
    unsigned	 clock_rate;
    unsigned	 channel_count;
    unsigned	 samples_per_frame;
    unsigned	 bits_per_sample;
    PyObject	*listeners;

} PyObj_pjsua_conf_port_info;


/*
 * conf_port_info_dealloc
 * deletes a conf_port_info from memory
 */
static void conf_port_info_dealloc(PyObj_pjsua_conf_port_info* self)
{
    Py_XDECREF(self->name);    
    Py_XDECREF(self->listeners);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * conf_port_info_new
 * constructor for conf_port_info object
 */
static PyObject * conf_port_info_new(PyTypeObject *type, PyObject *args,
                                     PyObject *kwds)
{
    PyObj_pjsua_conf_port_info *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_conf_port_info *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->name = PyString_FromString("");
	self->listeners = PyList_New(0);
    }
    return (PyObject *)self;
}

/*
 * conf_port_info_members
 */
static PyMemberDef conf_port_info_members[] =
{   
    {
        "slot_id", T_INT, 
        offsetof(PyObj_pjsua_conf_port_info, slot_id), 0,
        "Conference port number."
    },
    {
        "name", T_OBJECT_EX,
        offsetof(PyObj_pjsua_conf_port_info, name), 0,
        "Port name"        
    },
    {
        "clock_rate", T_INT, 
        offsetof(PyObj_pjsua_conf_port_info, clock_rate), 0,
        "Clock rate"
    },
    {
        "channel_count", T_INT, 
        offsetof(PyObj_pjsua_conf_port_info, channel_count), 0,
        "Number of channels."
    },
    {
        "samples_per_frame", T_INT, 
        offsetof(PyObj_pjsua_conf_port_info, samples_per_frame), 0,
        "Samples per frame "
    },
    {
        "bits_per_sample", T_INT, 
        offsetof(PyObj_pjsua_conf_port_info, bits_per_sample), 0,
        "Bits per sample"
    },
    {
        "listeners", T_OBJECT_EX,
        offsetof(PyObj_pjsua_conf_port_info, listeners), 0,
        "Array of listeners (in other words, ports where this port "
	"is transmitting to"
    },
    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjsua_conf_port_info
 */
static PyTypeObject PyTyp_pjsua_conf_port_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "_pjsua.Conf_Port_Info",	    /*tp_name*/
    sizeof(PyObj_pjsua_conf_port_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)conf_port_info_dealloc,/*tp_dealloc*/
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
    "Conf Port Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    conf_port_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    conf_port_info_new,             /* tp_new */

};

//////////////////////////////////////////////////////////////////////////////

/*
 * PyObj_pjmedia_snd_dev_info
 * PJMedia Snd Dev Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
        
    unsigned  input_count;
    unsigned  output_count;
    unsigned  default_samples_per_sec;    
    PyObject *name;

} PyObj_pjmedia_snd_dev_info;

/*
 * pjmedia_snd_dev_info_dealloc
 * deletes a pjmedia_snd_dev_info from memory
 */
static void pjmedia_snd_dev_info_dealloc(PyObj_pjmedia_snd_dev_info* self)
{
    Py_XDECREF(self->name);        
    self->ob_type->tp_free((PyObject*)self);
}

/*
 * pjmedia_snd_dev_info_new
 * constructor for pjmedia_snd_dev_info object
 */
static PyObject * pjmedia_snd_dev_info_new(PyTypeObject *type, 
					   PyObject *args,
					   PyObject *kwds)
{
    PyObj_pjmedia_snd_dev_info *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjmedia_snd_dev_info *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->name = PyString_FromString("");	
    }
    return (PyObject *)self;
}

/*
 * pjmedia_snd_dev_info_members
 */
static PyMemberDef pjmedia_snd_dev_info_members[] =
{
    {
        "input_count", T_INT, 
        offsetof(PyObj_pjmedia_snd_dev_info, input_count), 0,
        "Max number of input channels"
    },
    {
        "output_count", T_INT, 
        offsetof(PyObj_pjmedia_snd_dev_info, output_count), 0,
        "Max number of output channels"
    },
    {
        "default_samples_per_sec", T_INT, 
        offsetof(PyObj_pjmedia_snd_dev_info, default_samples_per_sec), 0,
        "Default sampling rate."
    },
    {
        "name", T_OBJECT_EX,
        offsetof(PyObj_pjmedia_snd_dev_info, name), 0,
        "Device name"        
    },
        
    {NULL}  /* Sentinel */
};


/*
 * PyTyp_pjmedia_snd_dev_info
 */
static PyTypeObject PyTyp_pjmedia_snd_dev_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "_pjsua.PJMedia_Snd_Dev_Info",  /*tp_name*/
    sizeof(PyObj_pjmedia_snd_dev_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)pjmedia_snd_dev_info_dealloc,/*tp_dealloc*/
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
    "PJMedia Snd Dev Info object",  /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjmedia_snd_dev_info_members,   /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    pjmedia_snd_dev_info_new,       /* tp_new */

};

//////////////////////////////////////////////////////////////////////////////

/*
 * PyObj_pjmedia_codec_param_info
 * PJMedia Codec Param Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    unsigned	clock_rate;
    unsigned	channel_cnt;
    pj_uint32_t avg_bps;
    pj_uint16_t frm_ptime;
    pj_uint8_t  pcm_bits_per_sample;
    pj_uint8_t  pt;	

} PyObj_pjmedia_codec_param_info;



/*
 * pjmedia_codec_param_info_members
 */
static PyMemberDef pjmedia_codec_param_info_members[] =
{
    {
        "clock_rate", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_info, clock_rate), 0,
        "Sampling rate in Hz"
    },
    {
        "channel_cnt", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_info, channel_cnt), 0,
        "Channel count"
    },
    {
        "avg_bps", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_info, avg_bps), 0,
        "Average bandwidth in bits/sec"
    },
    {
        "frm_ptime", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_info, frm_ptime), 0,
        "Base frame ptime in msec."
    },
    {
        "pcm_bits_per_sample", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_info, pcm_bits_per_sample), 0,
        "Bits/sample in the PCM side"
    },
    {
        "pt", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_info, pt), 0,
        "Payload type"
    },
    
    {NULL}  /* Sentinel */
};


/*
 * PyTyp_pjmedia_codec_param_info
 */
static PyTypeObject PyTyp_pjmedia_codec_param_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "_pjsua.PJMedia_Codec_Param_Info",      /*tp_name*/
    sizeof(PyObj_pjmedia_codec_param_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    0,				    /*tp_dealloc*/
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
    "PJMedia Codec Param Info objects",/* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjmedia_codec_param_info_members,/* tp_members */
};


//////////////////////////////////////////////////////////////////////////////

/*
 * PyObj_pjmedia_codec_param_setting
 * PJMedia Codec Param Setting
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    pj_uint8_t  frm_per_pkt; 
    unsigned    vad;
    unsigned    cng;
    unsigned    penh;
    unsigned    plc;
#if 0
    pj_uint8_t  enc_fmtp_mode;
    pj_uint8_t  dec_fmtp_mode; 
#endif

} PyObj_pjmedia_codec_param_setting;



/*
 * pjmedia_codec_param_setting_members
 */
static PyMemberDef pjmedia_codec_param_setting_members[] =
{
    {
        "frm_per_pkt", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, frm_per_pkt), 0,
        "Number of frames per packet"
    },
    {
        "vad", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, vad), 0,
        "Voice Activity Detector"
    },
    {
        "cng", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, cng), 0,
        "Comfort Noise Generator"
    },
    {
        "penh", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, penh), 0,
        "Perceptual Enhancement"
    },
    {
        "plc", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, plc), 0,
        "Packet loss concealment"
    },
#if 0	// no longer valid with latest modification in codec
    {
        "enc_fmtp_mode", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, enc_fmtp_mode), 0,
        "Mode param in fmtp (def:0)"
    },
    {
        "dec_fmtp_mode", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, dec_fmtp_mode), 0,
        "Mode param in fmtp (def:0)"
    },
#endif
    
    {NULL}  /* Sentinel */
};


/*
 * PyTyp_pjmedia_codec_param_setting
 */
static PyTypeObject PyTyp_pjmedia_codec_param_setting =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "_pjsua.PJMedia_Codec_Param_Setting",/*tp_name*/
    sizeof(PyObj_pjmedia_codec_param_setting),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    0,				    /*tp_dealloc*/
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
    "PJMedia Codec Param Setting",  /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjmedia_codec_param_setting_members,/* tp_members */
};

//////////////////////////////////////////////////////////////////////////////


/*
 * PyObj_pjmedia_codec_param
 * PJMedia Codec Param
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    PyObj_pjmedia_codec_param_info * info;
    PyObj_pjmedia_codec_param_setting * setting;

} PyObj_pjmedia_codec_param;

/*
 * pjmedia_codec_param_dealloc
 * deletes a pjmedia_codec_param from memory
 */
static void pjmedia_codec_param_dealloc(PyObj_pjmedia_codec_param* self)
{
    Py_XDECREF(self->info);        
    Py_XDECREF(self->setting);        
    self->ob_type->tp_free((PyObject*)self);
}

/*
 * pjmedia_codec_param_new
 * constructor for pjmedia_codec_param object
 */
static PyObject * pjmedia_codec_param_new(PyTypeObject *type, 
					  PyObject *args,
					  PyObject *kwds)
{
    PyObj_pjmedia_codec_param *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjmedia_codec_param *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->info = (PyObj_pjmedia_codec_param_info *)
		     PyType_GenericNew(&PyTyp_pjmedia_codec_param_info, 
				       NULL, NULL);
	self->setting = (PyObj_pjmedia_codec_param_setting *)
			PyType_GenericNew(&PyTyp_pjmedia_codec_param_setting,
					  NULL, NULL);
    }
    return (PyObject *)self;
}

/*
 * pjmedia_codec_param_members
 */
static PyMemberDef pjmedia_codec_param_members[] =
{   
    
    {
        "info", T_OBJECT_EX,
        offsetof(PyObj_pjmedia_codec_param, info), 0,
        "The 'info' part of codec param describes the capability of the codec,"
        " and the value should NOT be changed by application."        
    },
    {
        "setting", T_OBJECT_EX,
        offsetof(PyObj_pjmedia_codec_param, setting), 0, 
        "The 'setting' part of codec param describes various settings to be "
        "applied to the codec. When the codec param is retrieved from the "
        "codec or codec factory, the values of these will be filled by "
        "the capability of the codec. Any features that are supported by "
        "the codec (e.g. vad or plc) will be turned on, so that application "
        "can query which capabilities are supported by the codec. "
        "Application may change the settings here before instantiating "
        "the codec/stream."        
    },
    
    {NULL}  /* Sentinel */
};

/*
 * PyTyp_pjmedia_codec_param
 */
static PyTypeObject PyTyp_pjmedia_codec_param =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "_pjsua.PJMedia_Codec_Param",   /*tp_name*/
    sizeof(PyObj_pjmedia_codec_param),/*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)pjmedia_codec_param_dealloc,/*tp_dealloc*/
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
    "PJMedia Codec Param",	    /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjmedia_codec_param_members,    /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    pjmedia_codec_param_new,        /* tp_new */

};

//////////////////////////////////////////////////////////////////////////////

/*
 * PyObj_pjsua_call_info
 * Call Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    int		 id;
    int		 role;
    int		 acc_id;
    PyObject	*local_info;
    PyObject	*local_contact;
    PyObject	*remote_info;
    PyObject	*remote_contact;
    PyObject	*call_id;
    int		 state;
    PyObject	*state_text;
    int		 last_status;
    PyObject	*last_status_text;
    int		 media_status;
    int		 media_dir;
    int		 conf_slot;
    int		 connect_duration;
    int		 total_duration;

} PyObj_pjsua_call_info;


/*
 * call_info_dealloc
 * deletes a call_info from memory
 */
static void call_info_dealloc(PyObj_pjsua_call_info* self)
{
    Py_XDECREF(self->local_info);
    Py_XDECREF(self->local_contact);
    Py_XDECREF(self->remote_info);
    Py_XDECREF(self->remote_contact);
    Py_XDECREF(self->call_id);
    Py_XDECREF(self->state_text);
    Py_XDECREF(self->last_status_text);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * call_info_new
 * constructor for call_info object
 */
static PyObject * call_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    PyObj_pjsua_call_info *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_call_info *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->local_info = PyString_FromString("");
	self->local_contact = PyString_FromString("");
	self->remote_info = PyString_FromString("");
	self->remote_contact = PyString_FromString("");
	self->call_id = PyString_FromString("");
	self->state_text = PyString_FromString("");
	self->last_status_text = PyString_FromString("");
    }
    return (PyObject *)self;
}

/*
 * call_info_members
 */
static PyMemberDef call_info_members[] =
{   
    {
        "id", T_INT, 
        offsetof(PyObj_pjsua_call_info, id), 0,
        "Call identification"
    },
    {
        "role", T_INT, 
        offsetof(PyObj_pjsua_call_info, role), 0,
        "Initial call role (UAC == caller)"
    },
    {
        "acc_id", T_INT, 
        offsetof(PyObj_pjsua_call_info, acc_id), 0,
        "The account ID where this call belongs."
    },
    {
        "local_info", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, local_info), 0,
        "Local URI"        
    },
    {
        "local_contact", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, local_contact), 0,
        "Local Contact"        
    },
    {
        "remote_info", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, remote_info), 0,
        "Remote URI"        
    },
    {
        "remote_contact", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, remote_contact), 0,
        "Remote Contact"        
    },
    {
        "call_id", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, call_id), 0,
        "Dialog Call-ID string"        
    },
    {
        "state", T_INT, 
        offsetof(PyObj_pjsua_call_info, state), 0,
        "Call state"
    },
    {
        "state_text", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, state_text), 0,
        "Text describing the state "        
    },
    {
        "last_status", T_INT, 
        offsetof(PyObj_pjsua_call_info, last_status), 0,
        "Last status code heard, which can be used as cause code"
    },
    {
        "last_status_text", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, last_status_text), 0,
        "The reason phrase describing the status."        
    },
    {
        "media_status", T_INT, 
        offsetof(PyObj_pjsua_call_info, media_status), 0,
        "Call media status."
    },
    {
        "media_dir", T_INT, 
        offsetof(PyObj_pjsua_call_info, media_dir), 0,
        "Media direction"
    },
    {
        "conf_slot", T_INT, 
        offsetof(PyObj_pjsua_call_info, conf_slot), 0,
        "The conference port number for the call"
    },
    {
        "connect_duration", T_INT,
        offsetof(PyObj_pjsua_call_info, connect_duration), 0,
        "Up-to-date call connected duration(zero when call is not established)"
    },
    {
        "total_duration", T_INT,
        offsetof(PyObj_pjsua_call_info, total_duration), 0,
        "Total call duration, including set-up time"        
    },
    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjsua_call_info
 */
static PyTypeObject PyTyp_pjsua_call_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "_pjsua.Call_Info",		    /*tp_name*/
    sizeof(PyObj_pjsua_call_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)call_info_dealloc,  /*tp_dealloc*/
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
    "Call Info",		    /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    call_info_members,		    /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    call_info_new,		    /* tp_new */

};



//////////////////////////////////////////////////////////////////////////////

#endif	/* __PY_PJSUA_H__ */

